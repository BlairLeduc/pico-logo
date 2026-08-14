# GitHub Copilot Instructions

A **Logo interpreter in C (C11)** for three RP2350 boards on the Pico SDK. Behaviour
must match [Pico_Logo_Reference](../reference/Pico_Logo_Reference.md).

Review the diff as given: concrete problems only, a few high-confidence findings over
many speculative ones. Never ask the author to run or add tests first.

## PR Review Checklist (CRITICAL)
<!-- Copilot not longer has a ~4000 char limit -->

### 1. Floating point — single precision only
- The RP2350 is hardware **single-precision** only. Flag `double`, `long double`, `%lf`.
- Flag double-precision libm calls having an `f` variant (`sqrt`→`sqrtf`, `pow`→`powf`).
- Flag float literals without an `f` suffix (`1.0` → `1.0f`) — they promote to double.

### 2. Static memory footprint
- SRAM (~520 KB) is nearly full; oversized static/global buffers crash `repl_init` with
  an OOM panic. Flag large new static/global arrays, and stack buffers on recursive or
  evaluation paths.
- Capacities live in `core/limits.h`. Flag new bare `#define` size limits in `.c` files,
  and fixed arrays indexed without bounds checks.

### 3. Error handling conventions
- Primitives return `Result`; never `exit()`, `abort()`, or print errors directly. Flag
  paths bypassing `Result` / the `ERR_*` codes.
- Flag missing arg validation in new primitives (`REQUIRE_*`) and unchecked allocations.

### 4. Logo semantics
- Flag behaviour contradicting the reference: operator precedence, truthiness,
  list-vs-word handling, 1-based indexing, error message wording.

### 5. Project conventions
- New primitives belong in `core/primitives_<topic>.c` and must be registered.
- `core/foo.c` needs tests in `tests/test_foo.c`. Flag new primitives or behaviour
  changes arriving with no test coverage.
- Tests use **Unity** + the **mock device** (`tests/mock_device.*`); no real hardware,
  network, or filesystem.
- Device code lives under `devices/<device>/`; `host` has no graphics or sound. Flag
  hardware assumptions leaking into `core/`.
- Networking is board-gated: `LOGO_HAS_WIFI` (radio → WiFi/HTTP), `LOGO_HAS_TLS`
  (PSRAM → HTTPS). Flag code assuming WiFi/TLS is always present, or gating TLS-only
  pieces on `LOGO_HAS_WIFI`. A missing device op returns an `ERR_*`, never crashes.
- Standard C11+ only. Flag new dependencies and code that won't cross-compile under the
  Pico SDK.

## What NOT to comment on
- Length-aware name lookups (`primitive_name_compare`, `find_procedure_index_n`) index
  `pname[len]` only after `strncasecmp` over `len` bytes returned 0, so
  `strlen(pname) >= len`. In bounds, not UB (ASan/UBSan verified).
- `error_format` never passes NULL to `snprintf`: a `%s` template with no `error_proc`
  or `error_arg` strips the placeholder, so `result_error(CODE)` is safe for any code
  — not UB (ASan/UBSan verified).
- `repl_proc_def_append` never sees an empty line — the REPL and `load` skip them, the
  editor appends the newline itself. Not a blank-line bug.
- A lone `end` closes a `to` definition even inside an unclosed `[`/`(` (#127), so an
  unbalanced bracket can't swallow later procedures. `repl_find_end_token`'s per-line
  depth is intentional.
- `docs/roadmap.md` and `docs/bugs.md` are **append-only dated logs**: a row records what
  shipped on that date, and a later row amends an earlier one rather than rewriting it.
  Don't flag an older entry as inaccurate because a later change superseded it — including
  when both entries land in the same PR.
- `graphify-out/` is machine-generated, committed as-is; never flag it. Dated snapshots
  archive the *prior* state, so a header trailing its directory date is by design.
- A blob word can never be a list element: `mem_cons` returns NODE_NIL for a blob
  operand (the 16-bit cell encoding cannot hold one). Code walking list elements —
  `token_source.c`'s node iterator above all — therefore only ever sees interned atoms,
  whose length is capped at 255 by the 1-byte prefix. Don't flag blob-length overflow
  on those paths.
- Pure style or formatting that already matches the surrounding code.
- Pre-existing issues outside the diff.
- The wide pen stamps a disc centred on an integer pixel, so drawn diameter is always
  odd; even pen sizes render one pixel wider by design.
- `Result.value` sits outside the union and constructors zero-fill, so
  `result.value.type` is a valid tag (`VALUE_NONE`) for any status.
- `parse_voice_set` fills `MAX_VOICES` from range-checked, deduped values; its sorted
  insert cannot overflow.
- LittleFS restore (`logo_lfs_restore`) is intentionally **sparse**: littlefs rebuilds
  from the superblock and erases free blocks on demand.
- `sb_available()` guards **IRQ-context** southbridge callers against a thread-context
  transfer already in flight — that is the only direction that can race. Everything
  runs on core 0 (nothing calls `multicore_launch_core1`) and an IRQ handler holds the
  core for its whole blocking I²C transfer, so no southbridge transfer can be in flight
  while thread code runs. Thread-context callers (`keyboard_get_key`'s idle poll,
  `sb_read_battery` via the battery primitive) therefore need no `sb_available()` check
  and cannot collide with each other. Don't flag a thread-context `sb_*` call for
  skipping it.
