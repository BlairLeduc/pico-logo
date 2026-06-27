# Networking: Memory and Storage Design Notes

Status: design discussion captured 2026-06-27. Scope: HTTP/HTTPS work on Pico
Logo, with particular attention to the Pimoroni Pico Plus 2 W (RP2350, 8 MB
PSRAM, 16 MB flash). This is a notes document, not a specification — it records
*why* we chose the v1 shape and what we deferred, so we can revisit it later.

See also: the `# HTTP Operations` section of `Pico_Logo_Reference.md` for the
language surface, and `core/memory.h` for the authoritative memory model.

---

## 1. The current memory model (what constrains us)

Pico Logo manages Logo objects in a **single contiguous arena** of
`LOGO_MEMORY_SIZE` bytes (default 128 KB, overridable from CMake). Layout
(`core/memory.h`):

```
+------------------+ offset 0
|   Atom table     |  interned words (strings); grows up
+------------------+ atom_next
|   Free space     |
+------------------+ node_bottom
|   Node pool      |  cons cells (lists); grows down
+------------------+ LOGO_MEMORY_SIZE
```

Two encoding facts dominate every storage decision:

- **Words** are referenced by a 30-bit atom-table **offset** at the *bare `Node`*
  level (`NODE_MAKE_WORD`), which in isolation could address ~1 GB. **But the
  moment a word is stored inside a cons cell**, `node_to_index()` re-encodes it
  as a 16-bit cell reference: `CELL_WORD_MARKER (0x8000) | (offset & 0x7FFF)`. So
  any word reachable through a list is capped at a **15-bit offset → 32 KB**
  (`LOGO_ATOM_LIMIT`, `core/memory.c`). Since virtually every word ends up in a
  list, **32 KB is the effective atom-arena ceiling**, not 1 GB.
- **Atoms are never garbage-collected.** `mem_gc_mark` skips words ("atoms are
  never freed", `core/memory.c`); the arena is append-only with dedup via
  `find_atom`. Anything interned lives until reset.
- **Cons cells** pack car and cdr as **16-bit indices**, capping the node pool
  at **65,535 nodes** regardless of how much RAM exists. This bounds *list*
  size independently of total memory.

Consequence for networking: even a payload returned as **one word** is bounded
by the 32 KB in-cell offset ceiling (and leaks, since atoms are never freed) the
instant it can be consed into a list; a payload parsed into a **list** (lines,
tokens) is additionally limited to ~64 K cells no matter how much memory we add.

---

## 2. PSRAM on the Pico Plus 2 W (8 MB)

There are two distinct roles PSRAM could play. They are not the same project.

### Role A — transient transfer / scratch buffer (recommended for v1)

Receiving the raw HTTP response off the socket, accumulating it, and parsing it
(status line → headers → body, including de-chunking) needs a working buffer.
PSRAM is a clean fit:

- Decouples "largest response we can receive" from the ~520 KB SRAM budget.
- It is write-once / read-once streaming memory; XIP/QMI PSRAM latency is
  irrelevant for a byte pipe gated by the network.
- Lives entirely in the **device hardware layer + HTTP client**. `core/` is
  untouched.

Recommendation: use PSRAM here, and only here, for the first HTTP cut.

### Role B — backing store for Logo *values* (deferred)

The body that `http.get` outputs becomes a Logo **word**. The naive idea —
"relocate the atom arena into PSRAM" — does **not** by itself help, for two
reasons established in §1:

- **The 15-bit in-cell offset is the real wall.** A word referenced from any
  cons cell is capped at a 32 KB offset (`LOGO_ATOM_LIMIT`). Moving the arena to
  PSRAM without changing the cell encoding just relocates the same 32 KB cap into
  slower memory. The blocker is the **encoding**, not only the layout. (An
  earlier draft of this note had this backwards.)
- **Atoms are never freed.** Pouring multi-MB bodies into the interned atom table
  is a monotonic leak, and bloats every linear `find_atom` scan. Interning is
  correct for *symbols* (procedure/keyword names) — not for large transient
  values.

So Role B is really two problems: *where the bytes live* (easy: PSRAM) and *how
large byte vectors are referenced and reclaimed* (hard). The recommended shape
is **not** "big atoms" but a **separate PSRAM-backed blob heap**:

- Keep the interned atom table small, permanent, and in SRAM (symbols, keywords,
  short words — the hot dedup path).
- Add a PSRAM blob heap for large byte vectors, referenced by a **handle** into
  a small SRAM descriptor table, with the blob bit carved out of the
  `NODE_TYPE_WORD` payload (`bit 29`). Handles are constrained to 15 bits so they
  still survive the existing 4-byte cell encoding — no pool-wide 8-byte-cell cost.
- Give blobs their **own mark/sweep GC** (unlike atoms, blobs must be
  reclaimable). The public `mem_word_ptr`/`mem_word_len` API stays unchanged, so
  callers still just see "a word"; only the `mem_word_ptr`-across-GC lifetime
  contract needs care if the blob heap compacts.

This is a real, contained, but non-trivial change. Deferred out of v1.

### Role C — relocating cold static buffers (orthogonal SRAM relief)

Independent of networking: the SRAM crunch can be eased by moving **cold,
human-speed** static buffers to PSRAM. Triage by access pattern, not size:

| Buffer | Size | Access pattern | Verdict |
|--------|------|----------------|---------|
| `editor_buffer` + `editor_proc_buffer` (`core/primitives_editor.c`) | 8 KB × 2 | only while a human edits | **move to PSRAM** |
| `proc_buffer` / `expr_buffer` (`core/repl.c`, already `malloc`'d) | 4 KB × 2 | REPL line accumulation, typing speed | good candidate |
| `gfx_buffer` (`devices/picocalc/screen.c`) | ~100 KB | rewritten every draw, blitted to LCD | **keep in SRAM** |
| `global_op_stack` (`core/eval.c`) | per `LOGO_OP_STACK_DEPTH` | every eval step | **keep in SRAM** |
| `frame_stack_memory` (`core/procedures.c`) | 32 KB | every procedure call | **keep in SRAM** |
| atom/node `memory_block` (`core/memory.c`) | `LOGO_MEMORY_SIZE` | hottest interpreter path | **keep in SRAM** |

The editor buffers (16 KB) are the clear win — touched only at human speed, so
PSRAM latency is invisible. The REPL buffers (8 KB) are secondary. The hot
interpreter stacks and the framebuffer must stay in SRAM.

The catch: `editor_buffer`/`editor_proc_buffer` live in `core/`, which must not
know about board PSRAM. So this needs the **same plumbing as Role B** — a
device-supplied region passed into `core/` at init, with the buffers converted
from `static` arrays to pointers that fall back to SRAM (or `malloc`) when no
PSRAM is present, keeping host/test builds unchanged. Do Role C alongside Role B
rather than inventing a second mechanism.

### PSRAM caveats (independent of role)

- The Pico SDK does not transparently route `malloc` to PSRAM; the PSRAM region
  is allocated explicitly and is board/cache-config specific. All of this is
  device-layer plumbing and must never leak into `core/`.

---

## 3. v1 storage decision

- Body is returned as **one word**, not a parsed list, to avoid the 64 K-node
  cap and to stay PSRAM-friendly later.
- Body size is bounded by a documented constant (an `HTTP_MAX_BODY` in
  `core/limits.h`); an over-size response is an **error**, never a silent
  truncation. This invariant is now enforced at the source: `mem_atom`/
  `mem_atom_unescape` return `NODE_NIL` instead of truncating past 255 bytes, and
  `prim_http_request` maps a non-empty body that fails to intern to
  `ERR_FILE_TOO_BIG`.
- PSRAM, when present, hosts the **receive/scratch buffer** (Role A) and lets us
  raise `HTTP_MAX_BODY`. It does **not** back Logo values in v1 (Role B).

### Raising `HTTP_MAX_BODY` (the 512 KB target)

512 KB is a reasonable post-rework target (1/16th of PSRAM; far under both the
31-bit blob-descriptor length and the ~8 MB byte ceiling). But it depends on
**two** things being PSRAM-backed first — raising the constant alone is a trap:

1. **Value storage (Role B).** The body must become a PSRAM blob, not an atom.
   Atoms cap at 255 bytes and are never freed; a 512 KB atom is impossible and
   would leak regardless.
2. **Receive buffer (Role A) — easy to miss.** The body is received into a
   *static SRAM* buffer: `static char g_io[HTTP_MAX_HEADERS + HTTP_MAX_BODY + 64]`
   (`core/primitives_http.c`). Bumping `HTTP_MAX_BODY` to 512 KB turns that into a
   ~513 KB static array — the exact OOM-at-`repl_init` failure mode (it cannot fit
   in 520 KB SRAM). `g_io` must move to PSRAM too.

**Order of operations:** do *not* raise `HTTP_MAX_BODY` until both `g_io` and the
body value are PSRAM-backed. Until then the honest cap is 255 bytes, now enforced
as an error rather than a truncation.

**Peak-memory note:** the naive path double-buffers transiently — raw bytes in
`g_io` (≤512 KB) plus the interned blob copy (≤512 KB) ≈ 1 MB peak PSRAM per
request. Fine within 8 MB, but the copy can be avoided by **receiving directly
into a freshly allocated blob** and dropping the separate scratch buffer; decide
this when building Role A/B.

---

## 4. Flash and CA roots (16 MB) — for the later HTTPS step

Flash is **not** the constraint:

- A full Mozilla CA bundle is ~200 KB of PEM (less as trimmed DER). 16 MB
  swallows it trivially, alongside the mbedTLS code itself.

The real HTTPS limits are **RAM and code**, not flash:

- mbedTLS handshake and cert-chain verification want tens of KB of SRAM
  transiently (the TLS record buffer alone is up to ~16 KB each way).
- You rarely want the whole bundle parsed into RAM. Bundling only the handful of
  roots for the hosts you target (or a single root) is the usual embedded
  approach and is far more RAM-friendly.

So plan HTTPS around the SRAM handshake cost and a **trimmed** root set, not
around flash capacity. Cert policy (bundled roots vs. user-supplied vs.
insecure-no-verify) is still an open decision for the HTTPS phase.

---

## 5. Open items to revisit

- Role B (PSRAM-backed value storage): add a PSRAM blob heap with its own GC and
  a 15-bit handle/descriptor indirection — *not* a relocated atom arena, which
  hits the 32 KB in-cell offset wall and the never-freed-atoms leak.
- Role C (cold static buffers): move the editor (and possibly REPL) buffers to
  PSRAM via the same device-supplied region as Role B.
- The 64 K-node cap will bound any future "parse response into a list" feature;
  widening cons-cell indices is a separate, larger change.
- HTTPS: mbedTLS/`altcp_tls` enablement, SRAM budget during handshake, and CA
  trust policy.
- Final value of `HTTP_MAX_BODY`, and whether it scales with detected PSRAM.
