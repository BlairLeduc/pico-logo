# Pico Logo

Pico Logo is a lightweight, modular Logo interpreter written in C, designed for the Raspberry Pi Pico (RP2040/RP2350). The project aims for compatibility with classic LCSI Logo semantics, _although is influenced by classic MIT Logo semantics_, focusing on clarity, maintainability, and resource efficiency.

![screenshot](assets/spiral.bmp)

## Features

- **Classic Logo Semantics:** Compatible with Logo books and manuals from the 1980s and 1990s.
- **Modular Core:** Primitives and interpreter logic are organized for clarity and extensibility.
- **REPL Interface:** Interactive terminal-based read-eval-print loop with friendly error messages.
- **Multi-line Input:** Supports procedure definitions spanning multiple lines.
- **Basic Constructs:** Variables, procedures, control structures (`if`, `repeat`), lists, arithmetic, and logical operations.
- **Device Abstraction:** Core logic separated from device-specific code for portability.
- **Host & Pico Support:** Runs on desktop for development; designed for easy porting to Raspberry Pi Pico hardware.
- **Single-Precision Math:** Uses 32-bit floats for numerical calculations, with fallback for integer-only hardware.
- **File I/O:** Manage files (`catalog`, `setprefix`, `erasefile`) and `load` and `save` Logo programs.
- **Unit Testing:** Uses Unity and CMake for isolated, maintainable tests.

## Features for the PicoCalc

- 320×320 resolution turtle graphics with 256 colours (from a palette of 65K colours)
- Three simultaneous display modes: full screen text for programs without graphics, full screen graphics for running graphical programs, and split screen for interactive use
- Full line editing and history
- Saving and loading of programs to and from a SD card formatted as FAT32

## Recommended Requirements

- Raspberry Pi Pico 2 or compatible board (RP2350) for hardware supported floating-point
- PicoCalc device with the latest firmware

# Contributing

The following informationwill allow to to configure your development environment.

## Visual Studio Code

Visual Studio Code is recommended to build Pico Logo. You will need the following extensions:

- [Raspberry Pi Pico](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico)
- [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
- [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)


## Directory Structure

- `core/` — Interpreter logic and primitives
- `devices/` — Device-specific code (`host/`, `picocalc/`)
- `reference/` — Language reference, error messages, and documentation
- `tests/` — Unit tests (Unity framework)

## Building and Running

This project uses CMake presets for all common configurations.

### Host (desktop) REPL

**Release build:**

```sh
cmake --preset=host
cmake --build --preset=host
./build-host/logo
```

**Debug build:**

```sh
cmake --preset=host-debug
cmake --build --preset=host-debug
./build-host-debug/logo
```

### Unit tests

```sh
cmake --preset=tests
cmake --build --preset=tests
ctest --preset=tests
```

For coverage builds, use the `tests-coverage` preset instead of `tests`.

### Pico firmware (RP2040 / RP2350)

**Pico 2 / RP2350 (recommended):**

```sh
cmake --preset=pico2
cmake --build --preset=pico2
```

This produces `build/pico-logo.uf2`, which you can flash with `picotool` or by copying to the Pico’s USB mass‑storage device.

**Original Pico / RP2040:**

```sh
cmake --preset=pico
cmake --build --preset=pico
```

This produces `build-pico/pico-logo.uf2`.

## Goals

- Enable learning and experimentation with Logo using classic resources.
- Provide a clear, maintainable codebase for future enhancements (graphics, sound, hardware integration).
- Ensure efficient operation on resource-constrained hardware.

## References

- See the [Pico Logo Reference](/reference/Pico_Logo_Reference.md) for language details.

## License

Copyright 2025 Blair Leduc  
See [LICENSE](/LICENSE) for details.
