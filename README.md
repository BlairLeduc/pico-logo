# Pico Logo

Pico Logo is a lightweight, modular Logo interpreter written in C, designed for the Raspberry Pi Pico (RP2040/RP2350). The project aims for compatibility with classic LCSI Logo semantics, _although is influenced by classic MIT Logo semantics_, focusing on clarity, maintainability, and resource efficiency.

![sqiral](assets/sqiral.png)

```
repeat 220 [ fd repcount rt 88 ]
```
## Goals

- Enable learning and experimentation with Logo using classic resources.
- Provide a clear, maintainable codebase for future enhancements (graphics, sound, hardware integration).
- Ensure efficient operation on resource-constrained hardware.

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



# Learn Logo
Pico Logo follows the LCSI dialect, however, basic logo and turtle graphics from the follow books should work without or minor modifications. 

The best place to start is the [Pico Logo Reference](/reference/Pico_Logo_Reference.md). Continue your learning journey with the following books.

## Beginning Logo
- [Primarily Logo](https://archive.org/details/primarilylogo/page/n37/mode/2up) by Donna Bearden, Kathleen Martin, Brad Foster
- [Logo for Kids: An Introduction](https://www.snee.com/logo/logo4kids.pdf) by Bob DuCharme
- [The Great Logo Adventure](https://softronix.com/download/tgla.zip) by Jim Muller
- [Introducing Logo](https://archive.org/details/tibook_introducing-logo/) by Peter Ross

## Science with Logo

- [Exploring Language with Logo](https://archive.org/details/exploringlanguag00gold) by E. Paul Goldenberg and Wallace Feurzeig
- [Logo Physics](https://archive.org/details/logo-physics-1985) by James P. Hurley

## Advanced Logo
- [LogoWorks: Challenging Programs in Logo](https://logothings.github.io/logothings/logoworks/Home.html) by Cynthia Solomon, Margaret Minsky, Brian Harvey
- [Turtle Geometry: The Computer as a Medium for Exploring Mathematics](https://direct.mit.edu/books/oa-monograph/4663/Turtle-GeometryThe-Computer-as-a-Medium-for) by Harold Abelson, Andrea diSessa
- <ins>Computer Science Logo Style</ins> by Brian Harvey
  - [Volume 1: Intermediate programming](https://archive.org/details/computersciencel0000harv)
  - [Volume 2: Projects, styles, and techniques](https://archive.org/details/computersciencel02harv)
  - [Volume 3: Advanced Topics](https://archive.org/details/computersciencel03harv)

Many more books are freely available on the internet.



# Contributing

The following information will allow you to configure your development environment.

## Visual Studio Code

Visual Studio Code is recommended to build Pico Logo. You will need the following extensions:

- [Raspberry Pi Pico](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico)
- [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
- [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)

The [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension should be installed and I recommend learning how to use it and how it integrates with the test explorer.

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

# About Logo
- [Mindstorms](http://worrydream.com/refs/Papert%20-%20Mindstorms%201st%20ed.pdf) by Seymour Papert
- [Logo's Lineage](https://www.atarimagazines.com/v2n12/logoslineage.php) by Ian Chadwick
- [History of Logo](https://escholarship.org/uc/item/1623m1p3) by Cynthia Solomon, Brian Harvey, Ken Kahn, Henry Lieberman, Mark L. Miller, Margaret Minsky, Artemis Papert, Brian Silverman
- [Logo Philosophy and Implementation](http://www.microworlds.com/support/logo-philosophy-implementation.html) by Seymour Papert, Clotilde Fonseca, Geraldine Kozberg and Michael Tempel, Sergei Soprunov and Elena Yakovleva, Horacio C. Reggini, Jeff Richardson, Maria Elizabeth B. Almeida, David Cavallo
- [Logo Tree project](https://web.archive.org/web/20180820132053/http://elica.net/download/papers/LogoTreeProject.pdf) by Pavel Boytchev

## License

Copyright 2025 Blair Leduc  
See [LICENSE](/LICENSE) for details.
