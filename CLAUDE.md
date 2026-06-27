# Project

- This project is a **Logo interpreter written in C**, targeting Pimoroni Pico Plus 2 W board (RP2350, 16MB Flash, 8MB PSRAM) using the **Pico C/C++ SDK**.
- The interpreter aims to be strictly compatible with the semantics described in [Pico_Language_reference](../reference/Pico_Logo_Reference.md).

# Expected Behaviour

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

# Development
- The code must be written in standard C (C11 or later) to ensure compatibility with the Pico SDK and ease of cross-compilation.
- The project must use **CMake** with presets for building and managing dependencies.
- The interpreter must be efficient and lightweight, suitable for running on resource-constrained hardware like the Raspberry Pi Pico 2.
  - Only use single-precision floating point (32-bit) for numerical calculations. The RP2350 supports single-precision natively in hardware.
- Only use the test framework when developing. Do not use the host device for developing new functionality or fixing bugs.
- Avoid introducing unnecessary dependencies; keep the project lightweight and focused on C and the Pico SDK.

## Code Structure

- Core interpreter files live in `core/`. Primatives are defined in `core/primitives_<topic>.c`.
- Device-specific code lives in `devices/`. Each device has its own subdirectory (e.g. `devices/host/`, `devices/picocalc/`). The host device uses standard input/output for the REPL and does not implement graphics or sound.

# Unit Testing

- Match the current implementation of unit tests.
- Write a test that reproduces the bug first, then fix the bug.
- Never change a test just to make it pass without understanding why it failed. If a test fails, investigate the failure and fix the underlying issue.
- Tests should cover both typical use cases and edge cases.
- Tests should be easy to read and understand, with clear assertions and descriptive names.
- Tests should use the mock device and the mock device should be updated as needed to support testing.
- We aim for high test coverage of core interpreter logic and primitives.
- After you complete a feature or fix a bug, **run all tests** to ensure nothing is broken.
- **Tests run natively on the host system**; you cannot test memory safety or hardware-specific features for the target device.
- Never use the host device for testing. Write a test instead.
