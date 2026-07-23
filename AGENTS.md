# Project

- This project is a **Logo interpreter written in C**, targeting three RP2350 boards using the **Pico C/C++ SDK**: the Raspberry Pi **Pico 2** (4MB Flash, no radio), the Raspberry Pi **Pico 2 W** (4MB Flash, WiFi, no PSRAM), and the Pimoroni **Pico Plus 2 W** (16MB Flash, 8MB PSRAM, WiFi).
- Networking is tiered by hardware via two build flags: `LOGO_HAS_WIFI` (radio → WiFi, DNS/NTP/ping, plain `http://`) and `LOGO_HAS_TLS` (PSRAM → `https://`, since mbedTLS's handshake heap lives in PSRAM). So Pico 2 is offline, Pico 2 W does plain HTTP only, and Pico Plus 2 W does HTTPS. Set these per board in `CMakePresets.json`.
- The interpreter aims to be strictly compatible with the semantics described in [Pico_Logo_Reference](reference/Pico_Logo_Reference.md).
- The roadmap is in [Improvements Roadmap](docs/improvements-roadmap.md).

# Build and test

Tests run natively on the host. Do **not** rely on the interactive host REPL to develop or debug new functionality; write a test instead.

```bash
cmake --preset=tests
cmake --build --preset=tests
ctest --preset=tests
```

Use the `tests-coverage` preset for coverage. For the host REPL, use `--preset=host` and run `./build-host/logo`. Target firmware presets are `pico+2w`, `pico2`, and `pico2w`.

# Working guidelines

- **Think before coding.** State assumptions. When multiple interpretations exist, present them rather than choosing silently. Call out unclear requirements or a simpler approach before implementing.
- **Simplicity first.** Use the minimum code that solves the problem. Avoid speculative features, single-use abstractions, and error handling for impossible scenarios. Prefer a 50-line solution over an equivalent 200-line solution.
- **Surgical changes.** Touch only what the request requires and match existing style. Do not refactor working unrelated code. Remove orphans created by your change, but leave pre-existing dead code and mention it instead. Every changed line should trace to the request.
- **Goal-driven.** Turn work into a verifiable check, then iterate until it passes. For example: reproduce a bug in a test, then make it pass. After every feature or fix, run the full test suite.

# Constraints

- Use standard **C11+** only, for Pico SDK compatibility and cross-compilation. Avoid new dependencies.
- Use single-precision (32-bit) `float` for all math; RP2350 supports it natively in hardware.
- SRAM (about 520 KB) is nearly full: large static/global buffers can cause `repl_init` to panic from out-of-memory. Define new fixed capacities in `core/limits.h`, not as scattered `#define`s.

# Code structure

- Core interpreter code lives in `core/`. Primitives are defined in `core/primitives_<topic>.c`.
- Device code lives in `devices/<device>/`, such as `host/` and `picocalc/`. The `host` device uses stdin/stdout for the REPL and has no graphics or sound.

# Unit testing

- Tests use Unity and CTest. A test file `test_foo.c` mirrors source file `core/foo.c`; cover ordinary and edge cases with clear, descriptive assertions.
- Tests use the mock device (`tests/mock_device.*`); extend that mock when needed rather than touching real hardware.
- When finding a bug, write a reproducing test first, then fix it.
- Never change a test merely to make it pass without understanding the failure; investigate and fix the underlying cause.

# Graphify

When the task concerns the codebase and `graphify-out/graph.json` exists, start with `graphify query "<question>"`. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts; these produce smaller, scoped results than broad reports or raw search.

- If `graphify-out/wiki/index.md` exists, use it for broad navigation instead of raw source browsing.
- Read `graphify-out/GRAPH_REPORT.md` only for broad architecture review or if query/path/explain do not provide enough context.
- Dirty `graphify-out/` files may be expected after hooks or incremental updates; they are not a reason to skip Graphify. Skip it only when the task is about stale/incorrect graph output or the user explicitly asks not to use it.
- After modifying code, run `graphify update .` to keep the graph current (AST-only; no API cost).
- When the user types `/graphify`, use the installed Graphify skill or instructions before doing anything else.
