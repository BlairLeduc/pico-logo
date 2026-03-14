# Project

- This project is a **Logo interpreter written in C**, targeting Raspberry Pi Pico boards (RP2040 / RP2350) using the **Pico C/C++ SDK**.
- The interpreter aims to be strictly compatible with the semantics described in [Pico_Language_reference](../reference/Pico_Logo_Reference.md).

# Development
- The code should be written in standard C (C11 or later) to ensure compatibility with the Pico SDK and ease of cross-compilation.
- The project must use **CMake** with presets for building and managing dependencies.
- The interpreter should be efficient and lightweight, suitable for running on resource-constrained hardware like the Raspberry Pi Pico.
  - Only use single-precision floating point (32-bit) for numerical calculations. The RP2350 supports single-precision natively in hardware and the RP2040 supports single-precision in software.
- Only use the test framework when developing. Do not use the host device for developing new functionality or fixing bugs.
- Avoid introducing unnecessary dependencies; keep the project lightweight and focused on C and the Pico SDK.

## Code Structure

- Core interpreter files live in `core/`. Primatives are defined in `core/primitives_<topic>.c`.
- Device-specific code lives in `devices/`. Each device has its own subdirectory (e.g. `devices/host/`, `devices/picocalc/`). The host device uses standard input/output for the REPL and does not implement graphics or sound.

# Testing

- Unit tests live in `tests/` and use **Unity**.
- Tests will use CMake for building and running, **ctest**.
- Test files are named `test_*.c` and should match the source files they test (e.g. `test_eval.c` tests `core/eval.c`).
- Each test file should include setup and teardown functions as needed with a `main()` function to run the tests in that file.
- If we find a bug while working on a feature, we should write a test that reproduces the bug first, then fix the bug.
- Never change a test just to make it pass without understanding why it failed. If a test fails, investigate the failure and fix the underlying issue.
- Tests should cover both typical use cases and edge cases.
- Tests should be easy to read and understand, with clear assertions and descriptive names.
- Tests should use the mock device and the mock device should be updated as needed to support testing.
- We aim for high test coverage of core interpreter logic and primitives.
- After you complete a feature or fix a bug, **run all tests** to ensure nothing is broken.
- **Tests run natively on the host system**; you cannot test memory safety or hardware-specific features on the target device.
- Only use the test framework when testing. Do not use the host device for testing new functionality or fixing bugs.

# How I’d like you (the assistant) to behave

- When adding new features or fixing bugs:
  - Propose or update **unit tests first or alongside** the code changes.
- Work incrementally toward the larger goal of Logo semantics, focusing on small, testable changes.
- Prioritize code **clarity**, **maintainability**, **simplicity of implementation**, and adherence to Logo semantics. Refactor code as needed to improve these aspects, even if it means changing existing code.
