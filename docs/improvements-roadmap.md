# Improvements Roadmap

Tracks candidate improvements to Pico Logo, comparing the current implementation
(everything in `reference/Pico_Logo_Reference.md`, 296 primitive/section
headings as of 2026-07-03) against the wider Logo family (Apple Logo II — the
model dialect — plus UCB/Berkeley Logo, Atari Logo, Terrapin, FMSLogo).

Companion documents:
- [`memory-reclamation-design.md`](memory-reclamation-design.md) — atom GC /
  `erall` soft reset designs (deferred).
- [`code-review-2026-07-02.md`](code-review-2026-07-02.md) — the review that
  produced PR #86; a few small refinements from it are tracked below.

**Status legend:** `todo` · `in progress` · `done` · `deferred`

---

## Tracking table

### Language: cheap wins (small primitives, high classroom value)

| Item | Dialect precedent | Status | Notes |
|---|---|---|---|
| `pick` (random element of word/list) | UCB | todo | Planned: [P2](#p2--list-utilities-pick-reverse-shuffle) |
| `reverse` (word or list) | UCB | todo | Planned: [P2](#p2--list-utilities-pick-reverse-shuffle) |
| `shuffle` | UCB (library) | todo | Planned: [P2](#p2--list-utilities-pick-reverse-shuffle) |
| `rerandom` / seedable RNG | UCB, Apple | todo | Planned: [P3](#p3--rerandom-and-a-core-prng) |
| `arc` | UCB | todo | Planned: [P4](#p4--arc-and-setpensize) |
| `setpensize` / `pensize` | UCB, FMSLogo | todo | Planned: [P4](#p4--arc-and-setpensize) |
| `remove`, `remdup` | UCB | todo | Backlog; trivial with `mem_list_append` |
| `localmake` | UCB | todo | Backlog; convenience wrapper |
| `tan`, two-input `(arctan x y)` | UCB | todo | Backlog; `atan2f` already used internally by `towards` |
| `modulo` (floor-division sign) | UCB | todo | Backlog; distinct from `remainder` |
| `runresult` | UCB | todo | Backlog; outputs `[value]` or `[]` |
| Graphics-screen text at turtle | UCB `label` | todo | Backlog; needs a different name (`label` is the `go` target here) |

### Language: medium

| Item | Status | Notes |
|---|---|---|
| Long words via blobs on PSRAM boards | todo | `word` errors >255 chars even on Pico Plus 2 W; `mem_word` already blobs HTTP responses — unify |
| `play [notes]` background melody | todo | Classic Atari/Apple territory; builds on `toot` |

### Language: big bets

| Item | Status | Notes |
|---|---|---|
| Multiple turtles/sprites (`tell`/`ask`/`each`), `touching?`, `when` events | todo | Planned (design first): [P5](#p5--multi-sprite-turtles-with-collision-design-first) |
| Arrays (`array`/`setitem`) | deferred | O(1) indexing; needs a new object kind (likely blob-backed). Wait for demonstrated need |
| Atom reclamation / `erall` soft reset | deferred | See `memory-reclamation-design.md` |

### Documentation

| Item | Status | Notes |
|---|---|---|
| "Differences from other Logos" appendix | todo | Named lambdas vs `?` templates, case-insensitive `equal?`, single-precision floats, `n`-notation, no mutators, no arrays |
| Document the TCO guarantee | todo | Tail recursion runs in constant space — a differentiator classic Logos lacked; currently undocumented |

### Implementation refinements (code-review leftovers)

| Item | Status | Notes |
|---|---|---|
| TCO lookahead double-parse on last lines | todo | Measure after PR #86's lookup speedups; may no longer matter |
| `name_buf[64]` identifier truncation aliasing | todo | Two >63-char names can alias at lookup |
| `parse_list` silently drops unknown tokens | todo | Consider erroring inside `[...]` literals |
| Procedure-body token re-classification per run | todo | Profile before optimizing (per-atom numeric cache is the candidate) |

### Tooling and process

| Item | Status | Notes |
|---|---|---|
| Host REPL under piped stdin | todo | Planned: [P1](#p1--host-repl-stdin--ci) — currently hangs non-interactively |
| CI: tests + firmware builds per PR | todo | Planned: [P1](#p1--host-repl-stdin--ci) |
| Reference link/anchor checker | todo | Planned: [P1](#p1--host-repl-stdin--ci) — the 4 hand-fixed PDF hyperref warnings were this class of bug |
| Dependabot triage (16 alerts on default branch) | todo | Separate small task |
| Lexer/parser fuzzing on host | todo | Backlog |

---

## The pick of five: plans

Suggested order: P1 first (it protects everything after it), then P2–P4 in any
order (each is an independent small PR), P5 last (design gate before code).

Recurring checklist for any new primitive:
1. Implementation in the matching `core/primitives_<topic>.c` + registration.
2. A `## name` section in `reference/Pico_Logo_Reference.md` — this **is** the
   help text (`scripts/generate_help.awk` generates `help_data.c` from it), and
   the help-coverage test fails without it.
3. Unity tests in the matching `tests/test_*.c` (typical + edge + OOM cases).
4. `ctest --preset=tests` all green; `cmake --build --preset=pico2` links.

### P1 — Host REPL stdin + CI

**Goal:** `echo 'print 1' | ./build-host/logo` works; every PR runs tests,
firmware builds, and a reference link check automatically.

- **Host stdin fix** (`devices/host/`): diagnose the non-tty hang — likely the
  console stream assumes a raw-mode tty for `read_char`/line editing, or output
  is unflushed when not a terminal. Fix: on `!isatty(stdin)`, use cooked
  line-buffered reads, flush after each line, treat EOF as `bye`. Keep
  interactive behaviour unchanged.
- **Golden-file e2e tests**: add a ctest that pipes a small Logo script through
  the host binary and diffs expected output (2–3 scripts: arithmetic/lists,
  procedure definition + TCO loop, error message shape). This is the cheapest
  regression net for whole-REPL behaviour.
- **CI workflow** (`.github/workflows/ci.yml`), three jobs:
  1. Host tests: configure `tests` preset, build, `ctest`.
  2. Firmware: arm-none-eabi toolchain + Pico SDK (repo uses
     `pico_sdk_import.cmake`, so `PICO_SDK_FETCH_FROM_GIT=1` or a pinned
     checkout action), build `pico2`, `pico2w`, `pico+2w`; fail on link errors
     (this catches SRAM overflows too).
  3. Reference check: a small script validating that every `](#anchor)` in
     `Pico_Logo_Reference.md` matches a real heading under pandoc's slug rules
     (lowercase, punctuation stripped, spaces→dashes). Cheap, no TeX in CI;
     the full PDF build stays local in `dist.sh`.
- **Acceptance:** piped REPL session produces expected output and exits 0 at
  EOF; CI green on a test PR; link checker fails when given a known-bad anchor.

### P2 — List utilities: `pick`, `reverse`, `shuffle`

**Goal:** the three classic list/word utilities, in one small PR.

- **Where:** `core/primitives_words_lists.c`, following the house pattern —
  `normalize_to_word` for numeric inputs, word *and* list support, OOM
  propagation via `mem_list_append` / `ERR_OUT_OF_SPACE`.
- **`pick obj`**: random element of a list (random char of a word). Empty →
  `ERR_TOO_FEW_ITEMS`. Uses the same randomness source as `random` (after P3,
  the core PRNG — which makes `pick` testable with `rerandom`).
- **`reverse obj`**: word → reversed atom (≤255 chars by construction);
  list → prepend-loop copy (naturally reversing, cheapest possible), checking
  each cons.
- **`shuffle obj`**: Fisher–Yates. Lists: collect element Nodes into a
  `malloc`'d array (the `reduce`/`crossmap` precedent), shuffle, rebuild,
  free. Words: shuffle in a 256-byte stack buffer.
- **Tests:** typical/edge (empty, single element, word forms, numbers), OOM
  (pool exhaustion via the existing `exhaust_node_pool` recipe), and — once P3
  lands — deterministic seeded tests for `pick`/`shuffle` distribution shape.
- **Reference:** three new sections under Words and Lists (feeds `help`).

### P3 — `rerandom` and a core PRNG

**Goal:** reproducible randomness, identical behaviour across boards and host.

- **Design:** move randomness into core — a small PRNG (xorshift32 or PCG32,
  ~10 lines, single-precision friendly) owned by the interpreter, seeded once
  at startup from the device entropy source (existing hardware op). `random`
  and `pick`/`shuffle` draw from it. Devices stop being consulted per call.
- **`rerandom`**: reseeds with a fixed constant; `(rerandom n)` seeds with
  `n`. Matches UCB semantics.
- **Why core-side:** deterministic tests (seed, then assert exact sequences),
  identical program behaviour on every board, one fewer per-call device hop.
- **Watch for:** the mock device's current random stub — tests that relied on
  it need migrating to `rerandom`-seeded expectations.
- **Tests:** same seed → same sequence; different seeds differ; `random n`
  bounds respected across the range; `rerandom` arity forms.
- **Reference:** `rerandom` section + a reproducibility note under `random`.

### P4 — `arc` and `setpensize`

**Goal:** two small graphics primitives with outsized drawing value.

- **`arc angle radius`** (`core/primitives_turtle.c`): draws an arc of
  `angle` degrees, radius `radius`, centred on the turtle, starting at the
  turtle's heading, clockwise (UCB semantics); the turtle does not move.
  Implemented as short line segments (fixed ~4° steps, or adaptive to radius)
  through the existing device line-drawing path, so pen colour/mode and
  wrap/fence clipping behave exactly like `forward`.
- **`setpensize n` / `pensize`**: pen width for all line drawing. This one
  touches the device layer: extend the line op (or add a width parameter) in
  `devices/`, implement thickness in the PicoCalc LCD driver (perpendicular
  offset strokes or a brush stamp per step), record-only in the mock, no-op on
  host. Width state lives with the other pen state in core.
- **Tests:** mock-device assertions on segment counts/endpoints for `arc`
  (quarter/full circles, negative angle), pen-size state round-trip,
  drawing-op width recorded; OOM not applicable.
- **Reference:** `arc`, `setpensize`, `pensize` sections; mention in the pen
  overview.

### P5 — Multi-sprite turtles with collision (design first)

**Goal:** turn the PicoCalc into a game machine: `tell`/`ask`/`each`,
`touching?`, and Atari-style `when` demons. **Gate: a design doc PR before any
implementation.**

The design doc must settle:
- **Turtle model:** N turtles (SRAM budget suggests 8–16), per-turtle state
  (pos, heading, pen, shape, visibility, colour) — sized against the ~10%
  SRAM headroom; `tell [0 1]` / `ask 2 [...]` / `each [...]` / `who`
  semantics; how existing single-turtle primitives implicitly address the
  active set.
- **Rendering:** the existing shape system (`getsh`/`putsh`/`setsh`) as sprite
  bitmaps; investigate the PicoCalc LCD driver's capability for dirty-rect
  compositing vs full-line redraw; worst-case frame cost with all sprites
  moving.
- **Collision:** `touching? t1 t2` — bounding-box first, optional pixel
  overlap; cost per check on a 150 MHz M33.
- **Events:** `when [condition] [action]` demons polled at the top of
  `eval_instruction` (the interrupt/pause checks already establish this hook
  point); re-entrancy rules (actions run like mini `run` lists; no nesting of
  demon firing); how demons interact with TCO and `catch`/`throw`.
- **Phasing:** likely M1 multiple turtles + tell/ask/each, M2 `touching?`,
  M3 `when` demons — each independently shippable.

**Risks to answer in the design:** SRAM for sprite backing store, evaluator
hook complexity (the trampoline's re-entrancy contract), and whether the host
device (no graphics) degrades cleanly.

---

## Progress log

| Date | Item | Change |
|---|---|---|
| 2026-07-03 | (all) | Roadmap created; P1–P5 planned, backlog triaged |
