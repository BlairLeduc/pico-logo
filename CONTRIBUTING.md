# Contributing to Pico Logo

Pico Logo is a Logo interpreter written in C11 for the RP2350, targeting the
PicoCalc. This document covers setting up a development environment, building,
and running the tests.

## Where things are

- `core/` — the interpreter: lexer, evaluator, memory, and the primitives
- `devices/` — device-specific code (`host/` for the desktop build,
  `picocalc/` for the real thing)
- `logo/` — the Logo programs that ship on the device (games, demos, samples,
  tools, themes, the `startup` file)
- `reference/` — the language reference, error messages and documentation
- `tests/` — unit tests (Unity), end-to-end scripts, and Logo test programs
- `tools/` — host-side utilities, such as `mklfsimg`, the LittleFS image builder
- `docs/` — design documents, the [roadmap](docs/roadmap.md) and the
  [bug tracker](docs/bugs.md)

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
> You may need to restart your terminal after installing BasicTeX for the TeX
> commands to be available, or run `eval "$(/usr/libexec/path_helper)"`.

Pandoc, librsvg and BasicTeX are only needed to produce the PDF reference
manual in `dist.sh`; you can build and test the interpreter without them.

## Visual Studio Code

Visual Studio Code is recommended to build Pico Logo. You will need the
following extensions:

- [Raspberry Pi Pico](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico)
- [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
- [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)

The [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
extension should be installed; it is worth learning how it integrates with the
test explorer.

## Building

This project uses CMake presets for all common configurations. Each preset
builds into its own `build-<preset>` directory.

### Unit tests

Tests run natively on the host, against the mock device in
`tests/mock_device.*`. This is how new functionality should be developed — not
by poking at the interactive host REPL.

```sh
cmake --preset=tests
cmake --build --preset=tests
ctest --preset=tests
```

Use the `tests-coverage` preset instead of `tests` for a coverage build.

### Host (desktop) REPL

Useful for a quick look at the language, but it has no graphics, sound or
keyboard — those live only on the PicoCalc.

```sh
cmake --preset=host
cmake --build --preset=host
./build-host/logo
```

The `host-debug` preset builds the same thing with debug symbols into
`build-host-debug/`.

### Pico firmware (RP2350)

Choose the preset for the board in your PicoCalc:

- `pico2` — Raspberry Pi Pico 2 (offline)
- `pico2w` — Raspberry Pi Pico 2 W (WiFi, `http://` only)
- `pico+2w` — Pimoroni Pico Plus 2 W (WiFi + PSRAM, `https://`)

```sh
cmake --preset=pico2w
cmake --build --preset=pico2w
```

This produces `build-pico2w/pico-logo.uf2`, which you can flash with
`picotool` or by copying it onto the Pico's USB mass-storage device.

Each preset also sets the two capability flags the networking code is tiered
on: `LOGO_HAS_WIFI` (a radio — WiFi, DNS, NTP, ping, plain `http://`) and
`LOGO_HAS_TLS` (PSRAM — `https://`, because mbedTLS's handshake heap lives
there).

## Scripts

### flash.sh

Flashes the Pico firmware over SWD, using `openocd` under the hood.

```sh
./flash.sh
```

### dist.sh

Builds the release files — a UF2 per board, the `logo.img` filesystem image,
and the PDF reference manual — into a `dist` directory.

```sh
./dist.sh
```

## House rules

These are the conventions the codebase is held to; `CLAUDE.md` carries the
same list for agent sessions.

- **Standard C11+ only**, for Pico SDK compatibility and cross-compilation.
  Avoid new dependencies.
- **Single-precision floats** (32-bit) for all maths; the RP2350 does those in
  hardware.
- **SRAM (~520 KB) is nearly full.** Large static or global buffers can crash
  `repl_init` with an out-of-memory panic. New fixed capacities belong in
  `core/limits.h`, not in scattered `#define`s.
- **Simplicity first.** The minimum code that solves the problem: no
  speculative features, no abstractions for single-use code, no error handling
  for impossible scenarios.
- **Surgical changes.** Touch only what the change requires and match the
  surrounding style.
- **Found a bug?** Write the test that reproduces it first, then fix it, and
  record it in [`docs/bugs.md`](docs/bugs.md) — the Fixed table if you are
  fixing it now, the open list if you are not. Never change a test just to make
  it pass.
- **Run all tests** after any feature or fix.
- Features go in [`docs/roadmap.md`](docs/roadmap.md); defects go in
  [`docs/bugs.md`](docs/bugs.md). Anything substantial gets a design document
  in `docs/` first.
- The interpreter aims to be strictly compatible with the semantics in
  [`reference/Pico_Logo_Reference.md`](reference/Pico_Logo_Reference.md); a
  change to behaviour is a change to that manual too.

CI runs the unit tests, the end-to-end scripts, all three firmware builds, and
a reference link check on every pull request.
