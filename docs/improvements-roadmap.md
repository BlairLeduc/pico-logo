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
| `pick` (random element of word/list) | UCB | done | [P2](#p2--list-utilities-pick-reverse-shuffle) |
| `reverse` (word or list) | UCB | done | [P2](#p2--list-utilities-pick-reverse-shuffle) |
| `shuffle` | UCB (library) | done | [P2](#p2--list-utilities-pick-reverse-shuffle) |
| `rerandom` / seedable RNG | UCB, Apple | done | [P3](#p3--rerandom-and-a-core-prng) (hybrid: TRNG default, PCG32 when seeded) |
| `arc` | UCB | done | [P4](#p4--arc-and-help-discoverability) |
| `setpensize` / `pensize` | UCB, FMSLogo | on hold | Deferred 2026-07-04; stamped-disc design kept: [On hold](#on-hold--setpensize--pensize) |
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
| Help discoverability | done | [P4](#p4--arc-and-help-discoverability): keyword search fallback in `help`, `(help)` topic listing, "did you mean" on unknown names |

### Language: big bets

| Item | Status | Notes |
|---|---|---|
| Multiple turtles/sprites (`tell`/`ask`/`each`), `touching?`, `when` events | in progress | Design done; M0 pipeline + M1 sprite model landed: [P5](#p5--multi-sprite-turtles-with-collision-design-first) |
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
| Host REPL under piped stdin | done | [P1](#p1--host-repl-stdin--ci): EOF now returns `RESULT_EOF`; prompts/banner suppressed when stdin is not a tty |
| CI: tests + firmware builds per PR | done | [P1](#p1--host-repl-stdin--ci): `.github/workflows/ci.yml` — unit tests, e2e, 3 firmware presets, link check |
| Reference link/anchor checker | done | [P1](#p1--host-repl-stdin--ci): `scripts/check_reference_anchors.py`; verified it catches the historical `#processor-limits` breaks |
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

**Goal:** reproducible randomness on request, without giving up the RP2350's
hardware TRNG for normal use.

- **Design (hybrid, determinism opt-in):** by default, every draw comes from
  the device source exactly as before — the hardware TRNG on real boards.
  Running `rerandom` switches a small core-side PCG32 into the path, and
  `random`/`pick`/`shuffle` draw the reproducible stream until the next boot
  (UCB semantics). A single `logo_random_next(io)` helper in `core/random.c`
  routes `seeded ? pcg32() : logo_io_random(io)`; the device layer is
  untouched. Rationale: a TRNG's virtue is unpredictability, not statistical
  uniformity — the SDK's own `pico_rand` conditions TRNG entropy through a
  xoroshiro generator for the same reason. Nothing is lost by default, and
  determinism engages only when it is wanted (replays, classrooms, tests).
- **`rerandom`**: fixed sequence; `(rerandom n)` selects among sequences.
- **Tests:** same seed → same sequence (also through `pick`/`shuffle`);
  different seeds differ; bounds respected; default mode still hits the
  device op (mock's fixed value observed).
- **Reference:** `rerandom` section + a reproducibility note under `random`.

### P4 — `arc` and help discoverability

**Goal:** one small graphics primitive with outsized drawing value, plus make
the help system useful when you *don't* already know a primitive's name.

- **`arc angle radius`** (`core/primitives_turtle.c`): draws an arc of
  `angle` degrees, radius `radius`, centred on the turtle, starting at the
  turtle's heading, clockwise (UCB semantics); the turtle does not move.
  Implemented as short line segments (fixed ~4° steps, or adaptive to radius)
  through the existing device line-drawing path, so pen colour/mode and
  wrap/fence clipping behave exactly like `forward`.
- **Help keyword search**: when `help "name` finds no exact entry, fall back
  to a case-insensitive search over `help_entries[]` (names first, then
  section text) and list the matching primitive names instead of erroring
  with "I don't know about". Exact lookups are untouched.
- **Topic listing**: `(help)` with no inputs prints a categorised list of
  primitives. Categories come from the reference's chapter headings — extend
  `scripts/generate_help.awk` to record the enclosing `#` chapter for each
  `##` section (a small `category` field per entry; string data lives in
  flash, not SRAM).
- **"Did you mean"**: when evaluation hits an unknown name
  (`ERR_DONT_KNOW_HOW`), append the closest match (small edit distance or
  shared prefix) from primitives + defined procedures to the error message.
  Must not break existing error-format expectations in tests/reference —
  check those first; if the format is load-bearing, print the suggestion as a
  separate line.
- **Tests:** mock-device assertions on segment counts/endpoints for `arc`
  (quarter/full circles, negative angle); help search hit/miss/multiple-hit
  cases; `(help)` category output; did-you-mean suggestion and its absence
  when nothing is close.
- **Reference:** `arc` section; update the `help` section to document search
  and the no-input form.

### On hold — `setpensize` / `pensize`

Deferred by user decision (2026-07-04). Design notes preserved for when it
resumes:

- Pen width for all line drawing; touches the device layer (extend the line
  op or add a width parameter in `devices/`, record-only in the mock, no-op
  on host). Width state lives with the other pen state in core.
- **Algorithm: stamped disc.** At each Bresenham step, plot a filled disc of
  diameter `pensize` instead of a single pixel. Solid by construction
  (consecutive stamp centres are 1 px apart, so stamps always overlap — no
  angle can open a gap), uniform apparent width at every angle, and round
  caps/joins for free, which hides the seams between segmented curves like
  `arc`. Rejected: parallel offset strokes (up to ~29 % thinner at 45°,
  notched corners at every turtle turn).
- **Caveat — reverse mode:** `penreverse` toggles pixels, so overlapping
  stamps would toggle twice and speckle. Thick reverse lines must touch each
  pixel exactly once (delta-stamp only the pixels not covered by the
  previous stamp, or fill the thick segment as per-scanline spans).

### P5 — Multi-sprite turtles with collision (design first)

**Goal:** turn the PicoCalc into a game machine: `tell`/`ask`/`each`,
`touching?`, and Atari-style `when` demons. **Gate: a design doc PR before any
implementation.**

**Design doc:** [`multi-sprite-design.md`](multi-sprite-design.md) (draft v2
for review). Scope grew twice:

- The LCD update pipeline: the blocking CPU-paced blit and full-width
  row-band dirty tracking cannot afford eight moving sprites, so M0 of the
  design reworks the display path (tile dirty tracking, DMA-pipelined blit,
  scanline sprite compositor, and a `setrefresh "auto | "manual` + `refresh`
  policy) before any sprite work.
- A survey of period/modern multi-turtle Logos (TI Logo, Atari Logo, TRS-80
  Color Logo, LogoWriter/MicroWorlds, StarLogo/NetLogo, Scratch) from primary
  sources, which reshaped the sprite model: full-colour variable-size
  costumes (pool-backed, PSRAM tier), rotation styles + scaling, `setspeed`
  autonomous motion with `freeze`/`thaw`, costume-list animation,
  `stamp`/`snapsh`, `over?`/`colourunder`/`distance` sensing, and
  expression-based `when` demons. Turtle-as-process (`launch`) is
  explicitly designed-for but deferred to a P6 gate.

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
| 2026-07-03 | P1 | Done: host REPL EOF/prompt fixes, e2e golden tests (`tests/e2e/`), CI workflow, anchor checker |
| 2026-07-03 | P2 | Done: `pick`, `reverse`, `shuffle` primitives with reference sections and tests |
| 2026-07-04 | P3 | Done: `rerandom` + hybrid core PRNG (TRNG remains the default source; PCG32 only when seeded) |
| 2026-07-04 | P4 | Pen-size algorithm pinned: stamped disc (solid at all angles, round joins); reverse-mode dedup caveat noted |
| 2026-07-04 | Backlog | Added help discoverability (keyword search, topic listing, "did you mean") |
| 2026-07-04 | P4 | Rescoped: `setpensize`/`pensize` on hold (design notes preserved); help discoverability promoted into P4 alongside `arc` |
| 2026-07-04 | P4 | Done: `arc` (segments via the device setpos path, no device changes); help keyword search + `(help)` category listing (chapter data from the generator); REPL "Did you mean" via bounded edit distance |
| 2026-07-04 | P5 | Design draft: `multi-sprite-design.md` — scanline-composited sprites (8 turtles), tile dirty rects + DMA blit pipeline, refresh policy primitives, `touching?` masks in core, budgeted edge-triggered `when` demons; display-pipeline rework (M0) added as a prerequisite milestone |
| 2026-07-04 | P5 | Design v2: prior-art survey from primary sources (TI Logo, Atari Logo, TRS-80 Color Logo manuals; LogoWriter/MicroWorlds, StarLogo, Scratch); modernized sprite model — colour costumes with rotation/scale, `setspeed`+`freeze`/`thaw`, `setanim`, `stamp`/`snapsh`, `over?`/`distance`, expression `when` demons; `launch` processes deferred to a P6 design gate |
| 2026-07-04 | P5 | Open questions resolved with user: `tell` out-of-range errors; `over?`/`colourunder` first-active-only; demons stay armed at the REPL prompt (Atari-style); all four behaviour changes signed off (BMPs/`dot?` sprite-free, lowest-turtle queries, verbatim colour costumes) |
| 2026-07-04 | P5 | M0 display pipeline: tile dirty tracking (`dirty_tiles.c` + tests), DMA-pipelined blit (`lcd_blit_begin/row/end`), scanline sprite compositor (turtle out of the canvas, save-under deleted), `setrefresh`/`refresh`/`refreshmode` with auto restored on `cs`/error/`throw "toplevel` |
| 2026-07-04 | P5 | M1 sprite model + addressing: 8 turtles (per-turtle device state behind a `select` op, sprite id = turtle number, lower on top), `tell`/`ask`/`each`/`who` with command fan-out and lowest-active queries, colour costume pool (`costumes.c` + tests, 8 KB compact-on-free), indexed compositor sprites, `snapsh`/`stamp`, `setrot`/`setmag`; single-turtle programs unchanged (`cs` re-hides 1-7) |
