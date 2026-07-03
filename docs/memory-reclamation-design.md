# Memory Reclamation: Design Notes (deferred)

**Status: deferred, 2026-07-03.** Pico Logo runs on handheld, battery-powered
devices. The expected usage pattern is short sessions: pick up the device, work,
save, power off. Long-running programs that would exhaust the atom table within
a session are not a realistic workload, so none of the designs below are
scheduled. This note preserves the analysis so it can be picked up if the need
arises.

## Background: the "atoms are never freed" simplification

The memory system (`core/memory.c`) was deliberately kept simple: interned atoms
are permanent. Every distinct word the interpreter ever creates — every quoted
word, every character sliced out by `first`/`item`, every number formatted into
a word, every intermediate of a `word`-concatenation loop — consumes atom space
until power-off. Consequences:

- The atom table is capped at **32 KB** of the 128 KB `LOGO_MEMORY_SIZE` block
  (offsets must fit the 15-bit field of a 16-bit cons-cell half).
- `recycle` reclaims cons cells and blobs, but `nodes` output declines
  monotonically as new words are interned; word space never comes back.
- The node region's floor (`node_bottom`) also never rises: after a transient
  list spike, freed cells are reusable *as cells*, but the space is permanently
  unavailable to atoms (a one-way ratchet).
- `erall` does **not** help today: it only deactivates entries in the
  procedure/variable/property tables. Atoms, garbage cells, and blobs are
  untouched.

On atom exhaustion the interpreter now fails cleanly with `ERR_OUT_OF_SPACE`
(see PR #86, which removed the silent-truncation and `strlen(NULL)` failure
modes), so the cost of the simplification is a hard stop, not corruption.

## What PR #86 changed that makes reclamation feasible

Two pieces of groundwork now exist that earlier designs lacked:

1. **A complete, enumerable GC root set.** `prim_recycle` marks globals,
   procedure bodies, property lists, the frame stack (`frame_gc_mark_all`),
   all in-flight evaluator state (`op_stack_gc_mark`: loop bodies, saved
   token-source positions, staged arguments, pending operands, result values),
   the active token source, and pending tail-call arguments. Any atom-GC design
   reuses this infrastructure directly.
2. **Atom entries are self-describing and chained.** The layout is
   `[next:2][len:1][chars][nul][pad-to-4]` with a 256-bucket hash index.
   The `next` field and the walkable, self-sizing entries are exactly what a
   sweep needs.

Also note: the `mem_word_ptr` contract in `memory.h` already says the returned
pointer is "valid until the next GC" — the API anticipated atom collection.

---

## Design 1: `erall` soft reset (recommended first step, small)

**Idea.** `erall` is the one moment when a *total* memory reset is legitimate:
if the workspace is truly empty, nothing can be holding an atom, cell, or blob.
This sidesteps the entire hard part of atom GC — no root tracing at all —
because the invariant is "there are no roots" rather than "find all the roots".

**Behaviour.** Inside `prim_erall`, after the erasures, if all safety
conditions hold, perform a soft reset of the memory system. Fully transparent:
no new user-facing surface. Restores full atom space, releases the
`node_bottom` ratchet, and empties the blob heap — a genuinely fresh machine
without a reboot.

**Safety conditions (all must hold):**

1. **Workspace fully empty, including buried items.** `erall` spares buried
   procedures/variables, and buried bodies/values reference atoms and cells.
   If anything survives the erasure, skip the reset. (Most users bury nothing,
   so the common case qualifies.)
2. **Nothing in flight.** `erall` inside a `repeat` body or procedure would
   destroy the very list being executed — the same corruption class as the
   recycle GC bug fixed in PR #86. Guard: `eval->proc_depth == 0`, frame stack
   empty, op stack empty, no pending tail call. At the top-level REPL these all
   hold; the current line is lexer text, not nodes.

**Soft reset ≠ `logo_mem_init`.** This is the one real subtlety:
`logo_mem_init` calls `blob_reset`, which rebuilds the free list over the
*whole* aux region — clobbering permanent `mem_region_alloc` blocks (the PSRAM
HTTP transfer buffer and editor buffer that devices hold raw pointers to).
The soft reset must instead:

- reset `atom_next`, the hash buckets, `node_bottom`, `node_count`, the cell
  free list, and `gc_marks`;
- free descriptor-tracked blobs individually (`blob_free` each), leaving the
  region's free list and permanent allocations intact;
- re-intern the bootstrap atoms (newline marker, `true`, `false`) and refresh
  `mem_newline_marker` / `mem_true_node` / `mem_false_node`. (They land at
  identical offsets anyway — first atoms, deterministic order — but reassign
  explicitly.)

**Effort:** roughly 60–80 lines plus tests. **Risk:** low; the guard conditions
are cheap and already queryable.

**Tests to write:** reset happens at top level with empty workspace (verify
`nodes` returns to the boot value); no reset when a buried procedure survives;
no reset inside a procedure or `repeat` body (and no corruption — reuse the
PR #86 repro shapes); permanent aux allocations still valid after reset
(HTTP/editor buffer contents survive); bootstrap atoms and cached boolean
nodes work after reset.

**Reference update:** the `nodes` section's "word space is permanent" paragraph
gains "…until `erall` empties the workspace".

## Design 2: atom mark-sweep with in-place reuse (larger, helps running programs)

**Idea.** Extend the existing mark-sweep to atoms, freeing dead entries onto
size-class free lists for reuse **in place**. No compaction, so offsets never
move and no pointer ever needs rewriting — which neutralizes the two hard
blockers (interior `char*` name pointers everywhere, and atom offsets baked
into cons-cell halves).

**Mark phase additions:**

- `gc_mark_node` marks the atom for word Nodes (it currently skips words).
- `gc_mark_index` marks the atom offset in word-tagged cell halves
  (`car_idx & CELL_WORD_MASK` when `CELL_WORD_MARKER` is set) — currently
  skipped.
- New `mem_atom_mark_cstr(const char *p)`: range-check `p` against the atom
  region; if it points into it, mark the owning entry (`p` is always a
  `mem_word_ptr` result, i.e. entry offset + 3). This one helper covers every
  raw-string holder:
  - `Variable.name`, `UserProcedure.name` + `params[]`, `Binding.name`
  - `ForState.varname`, `CatchState.tag`, `PrimCallState.user_name`
  - `TailCall.proc_name`, the pause-prompt stack (`current_proc_stack`)
  - `Result` error strings (`error_proc`/`error_arg`/`error_caller`) held in
    op-stack results — these may also be string literals or static buffers,
    which the range check safely ignores.
- Bootstrap atoms marked unconditionally (as `mem_gc` already does for the
  newline marker).
- Mark-bit storage: bit 15 of each entry's `next` field is free (offsets are
  ≤ 0x7FF8; change the chain-end sentinel from 0xFFFF to 0x7FFF). Alternative:
  a separate 1 KB bitmap (offset/4 → 8192 bits) if touching the encoding is
  unwanted.

**Sweep phase:** walk the table (entries are contiguous and self-sizing via
`len`), unlink dead entries from their hash buckets, push them onto
**segregated free lists by entry size**. Entry sizes are `ALIGN4(len + 4)` =
8, 12, …, 260 bytes → 64 exact-fit size classes → a 64 × 2-byte head array
(128 B SRAM), no search, no fragmentation *within* a class. Rebuild the 256
hash buckets during the same walk. The `next` field doubles as the free-list
link (hash-chain when live, free-chain when dead — disjoint states). Keep the
`len` byte valid in free entries so the table stays walkable; rewrite it on
reuse (lengths within a class share the aligned size; padding absorbs the
difference).

**Allocation:** `mem_atom` on a miss pops the matching size class before
growing `atom_next`.

**Properties preserved:** interning/dedup survives (dead atoms leave the hash
chains, so a lookup never finds one); the `mem_words_equal` same-offset fast
path stays valid; blobs are unaffected (already collected).

**Known limitations:**

- No coalescing of adjacent free entries in v1 (the `len` byte caps a merged
  entry's representable size at 260 B). Acceptable: allocation falls back to
  growing `atom_next` when a class is empty.
- The 32 KB cap still bounds *live* atoms. Raising it means referencing atoms
  by handle (index into a side table) instead of byte offset — a much larger
  redesign, and far less urgent once space recycles.
- Trigger remains explicit `recycle` (plus, potentially, automatic
  collect-and-retry when `mem_atom`/`mem_cons` fail — deliberately not
  designed here; it requires the full root set to be marked at any allocation
  point, which the current recycle-time-only walk does not guarantee).

**Risk profile:** same class as the PR #86 recycle fix — one missed root frees
a live atom, and its bytes get rewritten on reuse. Mitigations are the same:
the root walk is centralized in `prim_recycle`, and the established test
recipes transfer (exhaust-then-verify; allocate junk after `recycle` and check
survivors; the `repeat … [recycle …]` and procedure-local shapes from
`test_primitives_workspace.c`).

**Effort:** ~200 lines in `memory.c`, ~80 for the holder walks, plus a
substantial test batch. Noticeably bigger than the PR #86 recycle fix.

## Companion: let the node region shrink back

Independent, small: during sweep, find the contiguous run of free cells at the
region floor (highest indices), remove them from the free list, and raise
`node_bottom`, returning the space to the atom side. Fixes the one-way ratchet
where a transient list spike permanently eats atom headroom. Needs a way to
distinguish free cells during the scan (the sweep already knows — do it in the
same pass, before rebuilding the free list).

## Options considered and rejected

- **Mark-compact atoms:** moving atoms requires rewriting 15-bit offsets in
  every referencing cons cell *and* every raw `char*` name pointer across the
  interpreter (variables, procedures, bindings, error strings, …). Invasive
  and risky; in-place reuse gets the benefit without any of it.
- **Reference counting:** `Value`/`Node` copies are pervasive and unhooked;
  no way to maintain counts in C without touching every copy site.
- **Handle-table indirection (to exceed 32 KB):** valid future direction, big
  redesign; unnecessary while live-atom footprints stay small.

## Suggested order, if ever needed

1. Design 1 (`erall` soft reset) — small, low-risk, delivers most practical
   value for the session-based usage pattern.
2. Companion node-region shrink-back — small, pairs with either design.
3. Design 2 (atom mark-sweep) — only if a real workload demonstrates atom
   exhaustion *within* a session that `erall` can't address.
