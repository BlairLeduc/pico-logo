### Project

- This project is a **Logo interpreter written in C**, targeting Raspberry Pi Pico boards (RP2040 / RP2350) using the **Pico C/C++ SDK**.
- **Simplicity and clarity** of the implementation are prioritized over performance and advanced features. The codebase should be modular and well-documented to facilitate future enhancements and maintenance.
- The code should be written in standard C (C11 or later) to ensure compatibility with the Pico SDK and ease of cross-compilation.
- The interpreter aims to be strictly compatible with the semantics described in [Pico_Language_reference](../reference/Pico_Logo_Reference.md).
- This interpreter is inspired by and based on the **LCSI Logo** implementation written in C by LCSI in the 1980s and 1990s.
- All information about LCSI Logo is available in public domain books and manuals.
  - “Apple Logo: The Language and Its Implementation (Harvey & Wright, 1985)” does not exist as a real, published book. It looks like a Franken-citation that mashed a few real things together.
- The main goal of this project is to allow me to use this interpreter to learn Logo using the library of Logo books that were written in the 1980s and 1990s.
- Interactions happen via a simple **REPL** (read-eval-print loop) in the terminal.
  - Error messages should be friendly and informative, similar to classic Logo implementation, see [Error Messages](../reference/Error_Messages.md).
- The interpreter should be efficient and lightweight, suitable for running on resource-constrained hardware like the Raspberry Pi Pico.
  - The RP2350 has 520KiB of RAM available (100KiB for video RAM and 420KiB for the interpreter and Logo programs to run).
  - Only use single-precision floating point (32-bit) for numerical calculations. The RP2350 supports single-precision natively in hardware.
  - We need to also provide support for the RP2040 (only has integer math hardware) and limited to 264KiB of RAM (100KiB for video RAM and 164KiB for the interpreter and Logo programs to run).
  - The interpreter should be designed with these constraints in mind.
- The project should use **CMake** with presets for building and managing dependencies.
- The interpreter needs target different devices with well-defined abstraction layers to separate platform-specific code from core interpreter logic:
  - A host system (Linux, macOS, Windows) for development and testing, using standard input/output for the REPL and no graphics or sound.
  - Raspberry Pi Pico hardware using the Pico SDK and the USB serial port for input/output (no graphics or sound).
  - A PicoCalc device with a small LCD display (320x320 with RGB565 color), keyboard for input/output and a SD Card for file storage.


### Core

- Core interpreter files live in `core/`.
- Primatives are defined in `core/primitives_<topic>.c` and declared in `core/primitives.h`.
  - Primitives are registered in `core/primitives.c`.
  - Primitives must be easy to implement. Common patterns should be abstracted away.
- The core includes implemations for:
  - Tokenization
    - Produces a simple linear stream.
  - Instruction evaluation
    - Takes the first token; calls procedures.
  - Expression evaluation
    - Called whenever a primitive requires an evaluated argument.
  - A small Pratt parser for expressions
    - Handles primary expressions (numbers, words, parenthesized expr, prefix ops)
    - Infix operators (+ - * / < = >)
    - Binding power table
  - No AST nodes; everything is evaluated directly.

### Devices

- Device-specific code lives in `devices/`.
- Each device has its own subdirectory (e.g. `devices/host/`, `devices/picocalc/`).
- Device-specific code should implement a common interfaces defined in:
  - `devices/console.h`
  - `devices/hardware.h`
  - `devices/io.h`
  - `devices/storage.h`
  - `devices/stream.h`

### Testing

- Unit tests live in `tests/` and use **Unity**.
- Tests will use CMake for building and running, **ctest**.
- Test files are named `test_*.c` and should match the source files they test (e.g. `test_eval.c` tests `core/eval.c`).
- Each test file should include setup and teardown functions as needed.
- Each test file has a `main()` function to run the tests in that file.
- Each test file is built into its own executable for isolated testing.
- Shared code for tests (e.g. test utilities) can go in `tests/scaffold.c` and `tests/scaffold.h`.
- If we find a bug while working on a feature, we should write a test that reproduces the bug first, then fix the bug.
- Tests should cover both typical use cases and edge cases.
- Tests should be easy to read and understand, with clear assertions and descriptive names.
- Tests should run quickly to facilitate rapid development.
- Tests should use the mock device and the mock device should be updated as needed to support testing
- We aim for high test coverage of core interpreter logic and primitives.

## Development and testing
- Use cmake presets to build and run tests.
- Only use the test framework when working. Do not use the host device for developing or testing new functionality or fixing bugs.

### How I’d like you (the assistant) to behave

- Assume this Logo interpreter project context by default when I’m in this workspace.
- When suggesting changes:
  - Show the **exact code edits** (or patches) needed, not just high-level descriptions.
- When adding new features or fixing bugs:
  - Propose or update **unit tests first or alongside** the code changes.
- Work incrementally toward the larger goal of LCSI semantics, focusing on small, testable changes.
- When I ask for new features or bug fixes, clarify any ambiguities by asking questions before proceeding.
- Prioritize code **clarity**, **maintainability**, **simplicity of implementation**, and adherence to classic Logo semantics.
- When I ask for explanations, provide concise and clear answers focused on the Logo interpreter context.
- Remember that the target platform is the Raspberry Pi Pico.
- When I ask for help with specific files, functions, or modules, focus your responses on those areas.
- Avoid introducing unnecessary dependencies; keep the project lightweight and focused on C and the Pico SDK.
