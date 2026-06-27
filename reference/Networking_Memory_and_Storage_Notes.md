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

- **Words** are referenced by a 30-bit atom-table **offset** (`NODE_TYPE_WORD`),
  so the *encoding* can address up to ~1 GB of string data. A word can therefore
  be very large in principle.
- **Cons cells** pack car and cdr as **16-bit indices**, capping the node pool
  at **65,535 nodes** regardless of how much RAM exists. This bounds *list*
  size independently of total memory.

Consequence for networking: a payload returned as **one word** is limited only
by available arena/atom space; a payload parsed into a **list** (lines, tokens)
is limited to ~64 K cells no matter how much memory we add.

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

The body that `http.get` outputs becomes a Logo **word** in the atom table.
Today the atom table and node pool share one contiguous block. To put value
bytes in PSRAM you would need to either:

- relocate the whole arena into PSRAM (slow XIP access on every word
  compare/copy and every GC sweep that touches strings), **or**
- split the atom arena from the node pool, so bulk strings live in PSRAM while
  the hot cons cells stay in fast SRAM.

The 30-bit word offset already allows addressing such large atoms, so the
*encoding* is not the blocker — the **layout** is. This is a real, contained,
but non-trivial change. Deferred out of v1.

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
  truncation.
- PSRAM, when present, hosts the **receive/scratch buffer** (Role A) and lets us
  raise `HTTP_MAX_BODY`. It does **not** back Logo values in v1 (Role B).

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

- Role B (PSRAM-backed value storage): split atom arena from node pool if we
  ever need multi-MB bodies as first-class Logo words.
- The 64 K-node cap will bound any future "parse response into a list" feature;
  widening cons-cell indices is a separate, larger change.
- HTTPS: mbedTLS/`altcp_tls` enablement, SRAM budget during handshake, and CA
  trust policy.
- Final value of `HTTP_MAX_BODY`, and whether it scales with detected PSRAM.
