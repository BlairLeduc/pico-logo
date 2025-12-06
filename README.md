# Pico Logo

Pico Logo is a lightweight, modular Logo interpreter written in C, designed for the Raspberry Pi Pico (RP2040/RP2350) and host-based development. The project aims for strict compatibility with classic LCSI Logo semantics, focusing on clarity, maintainability, and resource efficiency.

## Features

- **Classic Logo Semantics:** Compatible with Logo books and manuals from the 1980s and 1990s.
- **Modular Core:** Primitives and interpreter logic are organized for clarity and extensibility.
- **REPL Interface:** Interactive terminal-based read-eval-print loop with friendly error messages.
- **Multi-line Input:** Supports procedure definitions spanning multiple lines.
- **Basic Constructs:** Variables, procedures, control structures (`if`, `repeat`), lists, arithmetic, and logical operations.
- **Device Abstraction:** Core logic separated from device-specific code for portability.
- **Host & Pico Support:** Runs on desktop for development; designed for easy porting to Raspberry Pi Pico hardware.
- **Single-Precision Math:** Uses 32-bit floats for numerical calculations, with fallback for integer-only hardware.
- **File I/O:** Load and save Logo programs on host; future support for SD card/flash on Pico.
- **Unit Testing:** Uses Unity and CMake for isolated, maintainable tests.

## Directory Structure

- `core/` — Interpreter logic and primitives
- `devices/` — Device-specific code (`host/`, `pico/`)
- `reference/` — Language reference, error messages, and documentation
- `tests/` — Unit tests (Unity framework)
- `main.c` — Entry point for the interpreter

## Building & Running

1. **Configure:**  
	```sh
	cmake -S . -B build
	```
2. **Build:**  
	```sh
	cmake --build build
	```
3. **Run:**  
	Execute the built binary (e.g., `build/logo`) in your terminal.

## Goals

- Enable learning and experimentation with Logo using classic resources.
- Provide a clear, maintainable codebase for future enhancements (graphics, sound, hardware integration).
- Ensure efficient operation on resource-constrained hardware.

## References

- See `reference/Pico_Logo_Reference.md` for language details.
- Error messages: `reference/Error_Messages.md`
- Parsing and semantics: `reference/Parsing.md`

## License

Copyright 2025 Blair Leduc  
See `LICENSE` for details.

