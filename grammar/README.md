# Pico Logo

Syntax highlighting for the Pico Logo programming language.

## Features

- **Comments** — `; [comment text]` (including nested brackets)
- **Procedure definitions** — `to`/`end` blocks with procedure name and parameter highlighting
- **Lists** — `[` and `]` bracket highlighting
- **Quoted words** — `"word` string highlighting
- **Variable references** — `:variable` highlighting
- **Numbers** — integers, decimals, and scientific notation (`1e4`, `1n4`)
- **Operators** — `+ - * / = < >`
- **Code folding** — fold `to`/`end` blocks
- **Auto-closing** brackets and parentheses

## File Extensions

`.logo`, `.lgo`, `.lg`

## Building

```sh
cd grammar
npm install
npm run package
```

This produces `pico-logo-0.1.0.vsix`.

## Installation

```sh
code --install-extension pico-logo-0.1.0.vsix
```

Or in VS Code: **Extensions** → **⋯** → **Install from VSIX…**
