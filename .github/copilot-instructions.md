# GitHub Copilot Instructions

A **Logo interpreter in C (C11)** for three RP2350 boards (Pico 2, Pico 2 W, Pico Plus
2 W) on the Pico SDK. Behaviour must match
[Pico_Logo_Reference](../reference/Pico_Logo_Reference.md).

Review the diff as given: only concrete problems visible in it, a few high-confidence
findings over many speculative ones. Never ask the author to run or add tests first.

## PR Review Checklist (CRITICAL)
<!-- KEEP UNDER 4000 CHARS - Copilot only reads the first ~4000 -->

### 1. Floating point — single precision only
- The RP2350 is hardware **single-precision** only. Flag `double`, `long double`, `%lf`.
- Flag double-precision libm calls having an `f` variant (`sqrt`→`sqrtf`, `pow`→`powf`).
- Flag float literals without an `f` suffix (`1.0` → `1.0f`) — they promote the whole
  expression to double.

### 2. Static memory footprint
- SRAM (~520 KB) is nearly full; oversized static/global buffers crash `repl_init` with
  an OOM panic. Flag new large static/global arrays and large stack buffers on
  recursive or evaluation paths.
- Capacities live in `core/limits.h`. Flag new bare `#define` size limits in `.c` files,
  and fixed-capacity arrays indexed without bounds-checking.

### 3. Error handling conventions
- Primitives return `Result`; never `exit()`, `abort()`, or print error text directly.
  Flag error paths bypassing `Result` / the `ERR_*` codes.
- Flag missing argument validation in new primitives (`REQUIRE_*` macros) and unchecked
  allocation results.

### 4. Logo semantics
- Flag behaviour contradicting the reference: operator precedence, truthiness,
  list-vs-word handling, 1-based indexing, error message wording.

### 5. Project conventions
- New primitives belong in `core/primitives_<topic>.c` and must be registered.
- `core/foo.c` needs matching tests in `tests/test_foo.c`. Flag new primitives or
  behaviour changes arriving with no test coverage.
- Tests use **Unity** and the **mock device** (`tests/mock_device.*`); no real hardware,
  network, or filesystem outside the mocks.
- Device code lives under `devices/<device>/`; the `host` device has no graphics or
  sound. Flag hardware assumptions leaking into `core/`.
- Networking is board-gated: `LOGO_HAS_WIFI` (radio → WiFi/HTTP), `LOGO_HAS_TLS`
  (PSRAM → HTTPS). Flag code assuming WiFi/TLS is always present, or gating TLS-only
  pieces on `LOGO_HAS_WIFI`. A missing device op must return an `ERR_*`, not crash.
- Standard C11+ only. Flag new dependencies and constructs that would not cross-compile
  under the Pico SDK.

## What NOT to comment on
- `error_format` never passes NULL to `snprintf`: a `%s` template with no `error_proc`
  or `error_arg` falls back to stripping the placeholder, so `result_error(CODE)` is
  safe for any code — not UB (ASan/UBSan verified).
- `repl_proc_def_append` never sees an empty line — the REPL and `load` skip them, the
  editor appends the newline itself. Not a blank-line bug.
- A lone `end` closes a `to` definition even inside an unclosed `[` or `(` (#127), so an
  unbalanced bracket can't swallow later procedures. `repl_find_end_token`'s per-line
  depth is intentional.
- `graphify-out/` is machine-generated, committed as-is; never flag its contents. Dated
  snapshots archive the *prior* state, so a header trailing its directory date is by
  design.
- Pure style or formatting that already matches the surrounding code.
- Pre-existing issues outside the diff.
- The wide pen stamps a disc centred on an integer pixel, so its drawn diameter is
  always odd; even pen sizes render one pixel wider by design.
- `Result.value` sits outside the union and constructors zero-fill, so
  `result.value.type` is a valid tag (`VALUE_NONE`) for any status.
- `parse_voice_set` fills `MAX_VOICES` from range-checked, deduped values; its sorted
  insert cannot overflow.
- LittleFS restore (`logo_lfs_restore`) is intentionally **sparse**: littlefs rebuilds
  from the superblock and erases free blocks on demand.
