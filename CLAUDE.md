# Project

- This project is a **Logo interpreter written in C**, targeting three RP2350 boards using the **Pico C/C++ SDK**: the Raspberry Pi **Pico 2** (4MB Flash, no radio), the Raspberry Pi **Pico 2 W** (4MB Flash, WiFi, no PSRAM), and the Pimoroni **Pico Plus 2 W** (16MB Flash, 8MB PSRAM, WiFi).
- Networking is tiered by hardware via two build flags: `LOGO_HAS_WIFI` (radio → WiFi, DNS/NTP/ping, plain `http://`) and `LOGO_HAS_TLS` (PSRAM → `https://`, since mbedTLS's handshake heap lives in PSRAM). So Pico 2 is offline, Pico 2 W does plain HTTP only, Pico Plus 2 W does HTTPS. Set per board in `CMakePresets.json`.
- The interpreter aims to be strictly compatible with the semantics described in [Pico_Logo_Reference](reference/Pico_Logo_Reference.md).

# Build & Test

Tests run natively on the host; do **not** rely on the interactive host REPL to develop or debug new functionality — write a test instead.

```bash
cmake --preset=tests        # configure
cmake --build --preset=tests
ctest --preset=tests        # run all tests
```

Use the `tests-coverage` preset for coverage. Host REPL: `--preset=host` → `./build-host/logo`. Target firmware: `--preset=pico+2w` (also `pico2`, `pico2w`).

# How to work

- **Think before coding.** State assumptions; if multiple interpretations exist, present them rather than picking silently. If something is unclear or a simpler approach exists, say so before implementing.
- **Simplicity first.** Minimum code that solves the problem — no speculative features, abstractions for single-use code, or error handling for impossible scenarios. If 200 lines could be 50, rewrite it.
- **Surgical changes.** Touch only what the request requires; match existing style; don't refactor what isn't broken. Remove orphans *your* change creates, but leave pre-existing dead code (mention it instead). Every changed line should trace to the request.
- **Goal-driven.** Turn the task into a verifiable check ("fix the bug" → "write a test that reproduces it, then make it pass") and loop until it passes. After any feature or fix, **run all tests**.

# Constraints

- Standard **C11+** only, for Pico SDK compatibility and cross-compilation. Avoid new dependencies.
- Use **single-precision** float (32-bit) for all math; the RP2350 supports it natively in hardware.
- SRAM (~520 KB) is nearly full: large static/global buffers can crash `repl_init` with an out-of-memory panic. Keep new fixed-capacity sizes in `core/limits.h`, not scattered `#define`s.

# Code Structure

- Core interpreter lives in `core/`. Primitives are defined in `core/primitives_<topic>.c`.
- Device code lives in `devices/<device>/` (e.g. `host/`, `picocalc/`). The `host` device uses stdin/stdout for the REPL and has no graphics or sound.

# Unit Testing

- Tests use **Unity** + **ctest**. A test file `test_foo.c` mirrors the source `core/foo.c`; cover typical and edge cases with clear, descriptive assertions.
- Tests use the **mock device** (`tests/mock_device.*`); extend the mock as needed rather than touching real hardware.
- Found a bug while working? Write a test that reproduces it **first**, then fix it.
- Never change a test just to make it pass without understanding the failure — investigate and fix the underlying cause.
