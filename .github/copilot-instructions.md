### Project

- This project is a **Logo interpreter written in C**, targeting Raspberry Pi Pico boards (RP2040 / RP2350) using the **Pico C/C++ SDK**.
- **Simplicity and clarity** of the implementation are prioritized over performance and advanced features. The codebase should be modular and well-documented to facilitate future enhancements and maintenance.
- For now, everything runs on the **host** (desktop) for development and testing; we’ll worry about Pico-specific integration later.
- The interpreter aims to be strickly compatible with the semantics described in [Pico_Language_reference](../reference/Pico_Logo_Reference.md).
- This interpreter is inspired by and based on the **LCSI Logo** implementation written in C by LCSI in the 1980s and 1990s.
- All information about LCSI Logo is available in public domain books and manuals.
  - “Apple Logo: The Language and Its Implementation (Harvey & Wright, 1985)” does not exist as a real, published book. It looks like a Franken-citation that mashed a few real things together.
- The main goal of this project is to allow me to use this interpreter to learn Logo using the library of Logo books that were written in the 1980s and 1990s.
- We will eventually add turtle graphics and sound/music support, but for now we are focusing on core language semantics and primitives.
- Interactions happen via a simple **REPL** (read-eval-print loop) in the terminal.
  - Error messages should be friendly and informative, similar to classic Logo implementation, see [Error Messages](../reference/Error_Messages.md).
  - Support multi-line input for procedure definitions.
- The interpreter should handle basic Logo constructs: variables, procedures, control structures (if, repeat), lists, and arithmetic operations.
- The intepreter should be efficient and lightweight, suitable for running on resource-constrained hardware like the Raspberry Pi Pico.
  - The RP2350 has 520KiB of RAM available (200KiB for video RAM and 320KiB for the intepreter and Logo programs to run).
  - Only use single-precision floating point (32-bit) for numerical calculations. The RP2350 supports single-precision natively in hardware.
  - However, I would like to provide basic support the RP2040 (only has integer math hardware) and limited to 264KiB of RAM (200KiB for video RAM and 64KiB for the intepreter and Logo programs to run). The interpreter should run correctly on both devices, though not optimally on the RP2040.
  - The interpreter should be designed with these constraints in mind.
- The project should use **CMake** for building and managing dependencies.
- The interpreter needs target different devices:
  - Initially, focus on host-based development for ease of testing and debugging.
  - Later, Raspberry Pi Pico hardware using the Pico SDK and the USB serial port for input/output.
  - Later, a device with a small LCD screen and keyboard for input/output.
  - We need well-defined abstraction layers to separate platform-specific code from core interpreter logic.
- The code should be written in standard C (C99 or later) to ensure compatibility with the Pico SDK and ease of cross-compilation.
- We need to support basic file I/O operations to load and save Logo programs from/to files on the host system.
  - On the Pico, we will be using an SD card or the onboard flash memory for file storage in the future.


### Core

- Core interpreter files live in `core/`.
- Primatives are defined in `core/primitives_<topic>.c` and declared in `core/primitives.h`.
  - The topics include:
    - `arithmetic`
    - `conditionals_control_flow`
    - `logical`
    - `manage_workspace`
    - `ourside_world`
    - `procedures`
    - `variables`
    - `words_lists`
    - `turtle` (later)
    - etc.
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
- Each device has its own subdirectory (e.g. `devices/host/`, `devices/pico/`).
- Device-specific code should implement a common interface defined in `devices/device.h`.

### Reference

- Reference documentation lives in `reference/`.
- This includes information on:
  - Error messages (`reference/Error_Messages.md`)
  - Apple Logo II documentation (`reference/Apple_Logo_II_Reference_Manual_HiRes_djvu.txt`) as a backup for LCSI Logo semantics.
  - And more...
- `reference/Pico_Logo_Reference.md` is the main language reference document.

### Testing

- Unit tests live in `tests/` and use **Unity**.
- Tests will use CMake for building and running, **ctest**.
- Test files are named `test_*.c` and should match the source files they test (e.g. `test_eval.c` tests `core/eval.c`).
- Each test file should include setup and teardown functions as needed.
- Each test file has a `main()` function to run the tests in that file.
- Each test file is built into its own executable for isolated testing.
- Shared code for tests (e.g. test utilities) can go in `tests/scaffold.c`.
- If we find a bug while working on a feature, we should write a test that reproduces the bug first, then fix the bug.


### How I’d like you (the assistant) to behave

- Assume this Logo interpreter project context by default when I’m in this workspace.
- When suggesting changes:
  - Show the **exact code edits** (or patches) needed, not just high-level descriptions.
- When adding new features or fixing bugs:
  - Propose or update **unit tests first or alongside** the code changes.
- Work incrementally toward the larger goal of LCSI semantics, focusing on small, testable changes.
- When I ask for new features or bug fixes, clarify any ambiguities by asking questions before proceeding.
- Prioritize code clarity, maintainability, and adherence to classic Logo semantics.
- When I ask for explanations, provide concise and clear answers focused on the Logo interpreter context.
- Remember that the target platform is Raspberry Pi Pico, but for now we are focusing on host-based development.
- When I ask for help with specific files, functions, or modules, focus your responses on those areas.
- Avoid introducing unnecessary dependencies; keep the project lightweight and focused on C and the Pico SDK.
