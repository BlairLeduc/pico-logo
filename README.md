# Pico Logo

Pico Logo is a lightweight, modular Logo interpreter written in C, designed for RP2350-based boards: the Raspberry Pi Pico 2, the Raspberry Pi Pico 2 W, and the Pimoroni Pico Plus 2 W. The project aims for compatibility with classic LCSI Logo semantics, _although it is influenced by classic MIT Logo semantics_, focusing on clarity, maintainability, and resource efficiency.

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
- **Built-in Help:** `help name` explains a primitive's inputs and purpose, falls back to a keyword search over names and descriptions when it isn't one, and `(help)` lists every primitive by manual chapter.
- **Multi-line Input:** Supports procedure definitions spanning multiple lines.
- **Basic Constructs:** Variables, procedures, control structures (`if`, `repeat`), lists, arithmetic, and logical operations.
- **Tail Call Optimization:** A self tail-recursive call reuses the current procedure frame instead of growing the call stack, so recursive loops run in constant space and don't count against the recursion limit.
- **Functional List Processing:** `apply`, `foreach`, `map`, `map.se`, `filter`, `find`, `reduce`, and `crossmap` for working over words and lists.
- **Bitwise Operations:** `bitand`, `bitor`, `bitxor`, `bitnot`, `ashift`, and `lshift` for low-level integer manipulation.
- **Property Lists:** `pprop`/`gprop`/`plist`/`remprop` attach and query key-value data on any name.
- **Workspace Management:** `bury`/`unbury` protect procedures and variables from `erase`; `nodes`/`recycle` reclaim list space by hand.
- **Device Abstraction:** Core logic separated from device-specific code for portability.
- **Host & Pico Support:** Runs on desktop for development; targets three RP2350 boards (Pico 2, Pico 2 W, Pico Plus 2 W).
- **Single-Precision Math:** Uses 32-bit floats for numerical calculations, hardware-accelerated on the RP2350.
- **File I/O:** Manage files (`catalog`/`cat`, `setprefix`, `erasefile`) and `load` and `save` Logo programs, backed by an internal LittleFS filesystem at `/` and a FAT32 SD card mounted at `/sd`. Words longer than 255 characters are built on PSRAM-backed blobs on boards with PSRAM. `dribble`/`nodribble` record a session transcript to file.
- **Networking (WiFi boards):** Non-blocking WiFi join (`wifi.start`/`wifi.status`) alongside `wifi.connect`/`wifi.scan`, DNS resolution, NTP time, `ping`, an HTTP client (`http.get`, `http.post`, `http.put`, `http.patch`, `http.delete`) with JSON read/build primitives, and an HTTP server (`http.listen`) with mDNS so the device answers at `picologo.local`. Plain `http://` works on all WiFi boards; `https://` requires a TLS-capable (PSRAM) board.
- **Time Management:** `date`/`time` and `setdate`/`settime` for a real-time clock (kept in sync over WiFi via `network.ntp`), plus a monotonic `ticks` millisecond counter for timing intervals.
- **Event-Driven Programs:** `when` arms a demon that fires an action the moment a condition turns true — up to eight at once, running alongside your program or at the prompt; `cleardemons` clears them.
- **Unit Testing:** Uses Unity and CMake for isolated, maintainable tests.


## Additional Features for the PicoCalc

- 320×320 resolution turtle graphics with 256 colours (from a palette of 65K colours); remap any palette slot with `setpalette`/`palette`/`restorepalette`
- Turtle drawing tools: `arc`, `dot`, `fill`, `clean`, `stamp`, and custom turtle shapes (`shape`/`getsh`/`putsh`)
- Screen behaviour control: `window`/`wrap`/`fence` edge modes, and manual `refresh`/`refreshmode`/`sync` control over redraws
- Eight independent turtles (`tell`, `ask`, `each`, `who`) with costumes, autonomous motion and animation (`setspeed`, `setanim`), and pixel-accurate collision detection (`touching?`, `over?`, `colourunder`, `distance`)
- Eight-voice stereo PSG sound synthesizer: immediate notes (`sound`) or background music queues (`play`), with per-voice ADSR envelopes and waveforms (`setenv`, `setwave`)
- Two included example games, Space Invaders and Galaxian, exercising the sprite and sound stack
- Three simultaneous display modes: full screen text for programs without graphics, full screen graphics for running graphical programs, and split screen for interactive use
- Full line editing and history
- Full display text editor (edit procedures, variables and files) with Logo syntax highlighting — keywords, procedure names, variables, strings, numbers, comments, and rainbow bracket-depth colouring
- Access to PicoCalc hardware (battery status, power control, reboot to USB bootloader with `.bootsel`)


## Recommended Requirements

- One of the supported RP2350 boards, for hardware-supported floating-point:
  - **Raspberry Pi Pico 2** (4 MB flash, no radio) — offline
  - **Raspberry Pi Pico 2 W** (4 MB flash, WiFi) — adds networking and plain `http://`
  - **Pimoroni Pico Plus 2 W** (16 MB flash, 8 MB PSRAM, WiFi) — adds `https://` and larger responses
- PicoCalc device with the latest firmware


# Learn Logo
Pico Logo follows the LCSI dialect, however, basic logo and turtle graphics from the follow books should work without or minor modifications. 

The best place to start is the included [Pico Logo Reference](/reference/Pico_Logo_Reference.md). Continue your learning journey with the following books.

## Beginning Logo
- [Primarily Logo](https://archive.org/details/primarilylogo/page/n37/mode/2up) by Donna Bearden, Kathleen Martin, Brad Foster
- [Logo for Kids: An Introduction](https://www.snee.com/logo/logo4kids.pdf) by Bob DuCharme
- [The Great Logo Adventure](https://softronix.com/download/tgla.zip) by Jim Muller
- [Introducing Logo](https://archive.org/details/tibook_introducing-logo/) by Peter Ross

## Science with Logo

- [Exploring Language with Logo](https://archive.org/details/exploringlanguag00gold) by E. Paul Goldenberg and Wallace Feurzeig
- [Logo Physics](https://archive.org/details/logo-physics-1985) by James P. Hurley

## Advanced Logo
- [Advanced Logo: A Language for Learning](https://www.routledge.com/Advanced-Logo-A-Language-for-Learning/Friendly/p/book/9780805800746) by Michael Friendly
- [LogoWorks: Challenging Programs in Logo](https://logothings.github.io/logothings/logoworks/Home.html) by Cynthia Solomon, Margaret Minsky, Brian Harvey
- [Turtle Geometry: The Computer as a Medium for Exploring Mathematics](https://direct.mit.edu/books/oa-monograph/4663/Turtle-GeometryThe-Computer-as-a-Medium-for) by Harold Abelson, Andrea diSessa
- <ins>Computer Science Logo Style</ins> by Brian Harvey
  - [Volume 1: Intermediate programming](https://archive.org/details/computersciencel0000harv)
  - [Volume 2: Projects, styles, and techniques](https://archive.org/details/computersciencel02harv)
  - [Volume 3: Advanced Topics](https://archive.org/details/computersciencel03harv)

Many more books are freely available on the internet.



# Contributing

The following information will allow you to configure your development environment.

## Dependencies

Install the following with [Homebrew](https://brew.sh/):

```sh
brew install cmake pandoc librsvg
brew install --cask font-iosevka
brew install --cask basictex
```

After installing BasicTeX, add the required LaTeX packages:

```sh
sudo tlmgr update --self
sudo tlmgr install latexmk framed
```

> [!NOTE]
> You may need to restart your terminal after installing BasicTeX for the TeX commands to be available, or run `eval "$(/usr/libexec/path_helper)"`.


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
- `tools/` — Host-side utilities (e.g. `mklfsimg`, the LittleFS image builder)

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

### Pico firmware (RP2350)

Choose the preset for your board:

- `pico2` — Raspberry Pi Pico 2 (offline)
- `pico2w` — Raspberry Pi Pico 2 W (WiFi, `http://` only)
- `pico+2w` — Pimoroni Pico Plus 2 W (WiFi + PSRAM, `https://`)

```sh
cmake --preset=pico2w
cmake --build --preset=pico2w
```

This produces `build-pico2w/pico-logo.uf2` (each preset builds into its own `build-<preset>` directory), which you can flash with `picotool` or by copying to the Pico’s USB mass‑storage device.

## Scripts

### flash.sh

A helper script to flash the Pico firmware. It uses `openocd` under the hood to program the device via SWD.

```sh
./flash.sh
```

### dist.sh

A helper script to create a release files of the project, including documentation, and build artifacts. The `dist` directory is created to hold the release files.

```sh
./dist.sh
```

# About Logo
- [Mindstorms](http://worrydream.com/refs/Papert%20-%20Mindstorms%201st%20ed.pdf) by Seymour Papert
- [Logo's Lineage](https://www.atarimagazines.com/v2n12/logoslineage.php) by Ian Chadwick
- [History of Logo](https://escholarship.org/uc/item/1623m1p3) by Cynthia Solomon, Brian Harvey, Ken Kahn, Henry Lieberman, Mark L. Miller, Margaret Minsky, Artemis Papert, Brian Silverman
- [Logo Philosophy and Implementation](http://www.microworlds.com/support/logo-philosophy-implementation.html) by Seymour Papert, Clotilde Fonseca, Geraldine Kozberg and Michael Tempel, Sergei Soprunov and Elena Yakovleva, Horacio C. Reggini, Jeff Richardson, Maria Elizabeth B. Almeida, David Cavallo
- [Logo Tree project](https://web.archive.org/web/20180820132053/http://elica.net/download/papers/LogoTreeProject.pdf) by Pavel Boytchev

## Credits

Pico Logo was created by an experienced software engineer collaborating with [GitHub Copilot](https://github.com/features/copilot), [Claude Code](https://claude.com/claude-code), and [Codex](https://openai.com/codex/).

This is my safe, side open-source project where I experiment with agentic engineering. I am not making a point on what can be or not be done in this new world where we find ourselves. This is not an example of vibe coding. I am applying engineering principles vigoroursly.

## License

Copyright 2026 Blair Leduc  
See [LICENSE](/LICENSE) for details.
