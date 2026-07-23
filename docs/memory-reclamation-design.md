# Atom Garbage Collection: Implementation Plan

**Status: implemented, 2026-07-23.** Explicit `recycle` reclaims
unreachable atoms without moving live entries. Harden transient GC roots first
so nested evaluation is safe for both the existing node collector and the new
atom collector. Automatic collection and instruction retry are out of scope for
the first implementation.

Success means an exhausted atom table can run `recycle`, reclaim dead words,
and continue without damaging variables, procedures, active evaluation, or
partially constructed results.

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

## Existing groundwork and prerequisite

Atom entries are self-describing and chained. Their
`[next:2][len:1][chars][nul][pad-to-4]` layout and 256-bucket hash index make
the table walkable during a sweep. The `mem_word_ptr` contract also already
says that returned pointers are valid only until the next GC.

The existing root walk covers workspace values, procedure bodies, property
lists, frames, evaluator operations, token-source positions, demons, and
pending tail-call arguments. It is not yet complete for re-entrant evaluation:
primitives such as `map`, `filter`, `reduce`, and `crossmap` retain cursors,
partially built results, callback specifications, and accumulators in C locals
or heap scratch arrays while invoking Logo code. If that callback runs
`recycle`, those values are invisible to the current collector.

A scoped transient-root mechanism is therefore a prerequisite, not optional
hardening. It fixes the existing node-GC hazard and makes atom collection safe.

## Implementation

### Atom allocator and collector

- Preserve the live atom layout and existing offsets; live atoms never move.
- Reserve aligned link value `0x7FFC` as the chain terminator and limit new
  entry starts to `0x7FF8`.
- Use bit 15 of the link as the GC mark and bit 0 as the free-entry flag.
  Four-byte alignment leaves those bits available.
- Represent free entries as `[free-link|flag:2][block-size:2]`.
- Define 65 segregated free-list heads in `core/limits.h`, covering four-byte
  units through the maximum 260-byte live atom entry. The final bin accepts
  larger coalesced blocks.
- On an interning miss, take the first suitable free block, split any remainder
  of at least four bytes, and grow `atom_next` only when no reusable block fits.
- During sweep:
  1. Convert unmarked atoms to free blocks.
  2. Coalesce adjacent free blocks.
  3. Lower `atom_next` past a trailing free run.
  4. Rebuild hash buckets and free-list bins in a second table walk.
- Make `mem_word_ptr` and `mem_word_len` reject offsets currently marked free.
- Change `mem_free_atoms` to include reusable holes plus the allocatable
  contiguous region up to `min(node_bottom, LOGO_ATOM_LIMIT)`.

### Marking persistent roots

- Make `mem_gc_mark(Node)` mark ordinary atom Nodes as well as blobs.
- While traversing cons cells, mark word-tagged `car` and `cdr` references.
- Add `mem_gc_mark_atom_ptr(const char *)`. It accepts exact pointers returned
  by `mem_word_ptr` and ignores literals, static buffers, and pointers outside
  the atom arena.
- Extend the existing root walkers to cover:
  - global variable names and values;
  - procedure names, parameters, bodies, the current-procedure stack, and
    pending tail-call state;
  - frame binding names and values;
  - loop variable names, catch tags, primitive display names, cached `Result`
    metadata, and token-cache pointers;
  - caught-error procedure and caller pointers.
- Always mark the newline, `true`, and `false` bootstrap atoms.

### Scoped transient roots

Add a LIFO root-scope API to `memory.h`:

```c
typedef struct MemGcRootScope
{
    const Node *roots;
    size_t count;
    struct MemGcRootScope *previous;
} MemGcRootScope;

void mem_gc_roots_push(MemGcRootScope *scope, const Node *roots, size_t count);
void mem_gc_roots_pop(MemGcRootScope *scope);
void mem_gc_mark_transient_roots(void);
```

The scope and Node array live on the C stack; the memory subsystem retains only
the current-scope pointer, so there is no new fixed-capacity root table.
`mem_gc_roots_pop` asserts LIFO ordering in debug builds.

- Centralize primitive invocation through a wrapper that registers every word
  and list argument for the duration of the call. Route direct primitive
  calls, including list-processing callbacks, through the wrapper.
- Register additional roots around every re-entrant evaluator call for state
  not reachable from the original arguments: partially built result lists,
  cursors, converted-number atoms, lambda saved values, reducer accumulators,
  load/startup state, and comparable local state.
- Pop each scope immediately after the nested evaluator or callback returns so
  early-result handling cannot leak registrations.

### Shared arena recovery

- During node sweep, find the highest live node index.
- Rebuild the free list only through that index, raise `node_bottom` over the
  trailing free run, and update `node_count`.
- Continue to reuse interior free cells normally.

This returns shared arena space in both directions: trailing dead atoms lower
`atom_next`, while trailing dead cells raise `node_bottom`.

## Collection behaviour

- `recycle` remains the only collection trigger.
- A failed instruction is not retried automatically.
- Allocation routines do not invoke GC because arbitrary allocation points do
  not guarantee that all C-local state and source bytes have registered roots.
- Preserve raw-token primitive dispatch so top-level `recycle` remains
  callable even when its display-name atom cannot be allocated.
- The 32 KB atom-offset limit remains unchanged.
- Reusing an unreachable atom's offset is valid because atom identity is not
  exposed by Logo. The same-offset equality fast path remains valid for live
  values.

## Test plan

### Memory unit tests

- A directly rooted atom survives; an unrooted atom is collected and its block
  can be reused.
- Atoms reachable through word-tagged `car` and `cdr` fields survive.
- Newline, `true`, and `false` survive an otherwise rootless collection.
- Empty-word four-byte entries, all size classes, splitting, coalescing,
  hash-chain rebuilding, and trailing-run trimming work.
- Repeated collect/reuse cycles do not corrupt hash lookup or word equality.
- Node sweep raises `node_bottom` without losing live interior nodes.

### Root and integration tests

- Preserve global names and values, procedure names/parameters/bodies,
  properties, frame bindings, loop/catch state, tail calls, token caches,
  caught errors, demons, and blobs.
- Add nested-collection regressions for `map`, `map.se`, `filter`, `find`,
  `reduce`, `crossmap`, lambda saved variables, `load`, `ask`, `each`, pause,
  and nested evaluator paths.
- Allocate new atoms and cells immediately after each collection so a missed
  root is overwritten and exposed rather than passing accidentally.
- Exhaust atom space with unreachable words, execute top-level `recycle`, and
  verify that a previously failing allocation succeeds.
- Verify that live atoms filling the 32 KB offset space still produce
  `ERR_OUT_OF_SPACE` after `recycle`.

### Verification

Run:

```bash
cmake --preset=tests
cmake --build --preset=tests
ctest --preset=tests

cmake --preset=tests-coverage
cmake --build --preset=tests-coverage
ctest --preset=tests-coverage

cmake --build --preset=pico2
cmake --build --preset=pico2w
cmake --build --preset=pico+2w

graphify update .
```

Compare linker SRAM usage for all three firmware targets. The collector adds
65 two-byte free-list heads and one transient-root pointer; any larger static
increase requires explicit review.

## Documentation updates during implementation

- Update the `nodes` and `recycle` reference sections: unreachable word storage
  is reclaimed by explicit `recycle`, while live atoms remain subject to the
  32 KB offset limit.
- Mark atom reclamation complete in `docs/improvements-roadmap.md`.
- Update comments in `memory.h`, `memory.c`, and root-walker headers that still
  state atoms are permanent.

## Alternatives not selected

- **`erall` soft reset:** useful as an independent recovery feature, but it
  discards the workspace and does not help a long-running program.
- **Mark-compact atoms:** requires rewriting every 15-bit cell offset and every
  cached `char *`; in-place reuse provides reclamation without that risk.
- **Reference counting:** pervasive unhooked `Value` and `Node` copies make
  correct counts impractical.
- **Handle-table indirection:** could exceed the 32 KB offset limit, but is a
  substantially larger redesign and is unnecessary for reclamation.
- **Automatic collect-and-retry:** requires safe roots and rollback semantics at
  every allocation site; defer until explicit collection has proven reliable.
