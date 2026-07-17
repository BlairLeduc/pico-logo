# Improvements Roadmap

Tracks candidate improvements to Pico Logo, comparing the current implementation
(everything in `reference/Pico_Logo_Reference.md`, 296 primitive/section
headings as of 2026-07-03) against the wider Logo family (Apple Logo II — the
model dialect — plus UCB/Berkeley Logo, Atari Logo, Terrapin, FMSLogo).

Companion documents (everything in `docs/`):
- [`multi-sprite-design.md`](multi-sprite-design.md) — P5 sprites/collision/demons
  (implemented, M0–M3).
- [`launch-design.md`](launch-design.md) — P6 `launch` background processes
  (design complete, implementation not started).
- [`http-server-design.md`](http-server-design.md) — P7 HTTP server + mDNS
  (design complete, implementation not started).
- [`sound-design.md`](sound-design.md) — P8 stereo PSG synthesizer (design
  complete, implementation may begin).
- [`littlefs-filesystem-design.md`](littlefs-filesystem-design.md) — internal
  LittleFS root + `/sd` FAT32 mount (implemented, PR #83, 2026-06-29 —
  predates this roadmap).
- [`memory-reclamation-design.md`](memory-reclamation-design.md) — atom GC /
  `erall` soft reset designs (deferred).
- [`space-invaders-design.md`](space-invaders-design.md) /
  [`galaxian-design.md`](galaxian-design.md) — shipped games (#101, #106) that
  validate the sprite stack.
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
| `remove`, `remdup` | UCB | done | Landed 2026-07-11; word + list, `equal?` semantics, `remdup` keeps last |
| `localmake` | UCB | done | Landed 2026-07-11; `local` + `make` in one step |
| `tan`, two-input `(arctan x y)` | UCB | done | Landed 2026-07-11; two-input `arctan` is `atan2` |
| `modulo` (floor-division sign) | UCB | done | Landed 2026-07-11; sign of divisor, distinct from `remainder` |
| `runresult` | UCB | done | Landed 2026-07-11; new `OP_RUNRESULT` op; outputs `[value]` or `[]` |
| Graphics-screen text at turtle | UCB `label` | done | Landed as `write` (`label` is the `go` target here); new `draw_text` device op, upright horizontal text at the turtle in pen colour, `print` formatting, fans out over the active set |

### Language: medium

| Item | Status | Notes |
|---|---|---|
| Long words via blobs on PSRAM boards | done | Landed 2026-07-12; `word` builds >255-char results via `mem_word` (blob on PSRAM boards), refuses without PSRAM |
| `play [notes]` background melody | in progress | Absorbed into the P8 sound design — a background sequencer over real voices, not a melody of `toot`s: [P8](#p8--sound-stereo-psg-synthesizer-design-first) |
| Help discoverability | done | [P4](#p4--arc-and-help-discoverability): keyword search fallback in `help`, `(help)` topic listing, "did you mean" on unknown names |

### Language: big bets

| Item | Status | Notes |
|---|---|---|
| Multiple turtles/sprites (`tell`/`ask`/`each`), `touching?`, `when` events | done | All milestones landed (M0–M3); validated end-to-end by the Space Invaders game (#101/#102). `launch` processes are the P6 design below |
| `launch [instrs]` background processes | todo | Design done (gate closed 2026-07-12), implementation not started: [P6](#p6--launch-background-processes-design-first). `broadcast` deferred per Q3 |
| HTTP server (`http.listen`, `when [http.request?]`, `http.respond`, file transfer) | in progress | M0–M5 implemented on `p7-http-server` (2026-07-16): mDNS + `wifi.hostname`/`wifi.sethostname`, TCP server ops, demon-driven pump/parser, handler surface + `http.element`, `webturtle` example, file transfer. Browser + mDNS hardware-validated; `curl -T` upload validation pending. Design: [P7](#p7--http-server-design-complete-implementation-not-started) |
| Arrays (`array`/`setitem`) | deferred | O(1) indexing; needs a new object kind (likely blob-backed). Wait for demonstrated need |
| Atom reclamation / `erall` soft reset | deferred | See `memory-reclamation-design.md` |

### Platform

| Item | Status | Notes |
|---|---|---|
| LittleFS internal filesystem (root `/`) + `/sd` FAT32 mount | done | Landed 2026-06-29 (PR #83), before this roadmap existed; listed for completeness. Design: [`littlefs-filesystem-design.md`](littlefs-filesystem-design.md) |

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

### P6 — `launch` background processes (design first)

**Goal:** MicroWorlds-style `launch [instrs]` — run an instruction list as a
background process, cooperatively scheduled beside the foreground program,
composing with `tell`/demons/sound. The concurrency deferral from the P5 doc
(§8). **Gate closed:** [`launch-design.md`](launch-design.md) (v1; Q1–Q6 all
resolved with the user 2026-07-12 — every recommendation accepted).
Implementation may begin at M0.

- Model: a process is a paused evaluation — own `Evaluator`, own (shallow,
  runtime-sized) op stack, own frame arena slice; suspended processes hold
  zero C stack. Yields at op boundaries of the outermost trampoline
  (`RESULT_YIELD` + step budget); nested sub-trampolines (`map` lambdas, arg
  collection) run whole within a turn.
- Scheduler: round-robin at the existing demon poll sites; `wait` in a
  process sleeps that process only; shared re-entrancy guard with demons.
- Surface: `launch`, `halt`, `(launch)` print form; `broadcast`/`message?`
  designed but deferred (Q3). Lifetime: `cs`/error-unwind/BREAK halt all
  processes; `freeze`/`thaw` suspend/resume.
- The crux is SRAM (§5 of the doc): ~8 KB/process estimated on target
  against ~34 KB free on pico2 — Q1 decided a uniform SRAM pool on all
  boards (`MAX_PROCESSES` 2–4, count fixed by M0 target measurements).
- Milestones: M0 evaluator groundwork (runtime-sized `OpStack`, yield) →
  M1 table + scheduler + lifetime → M2 polish/broadcast → M3 hardware
  validation + game retrofit.

### P7 — HTTP server (design complete, implementation not started)

**Goal:** serve HTTP from Logo — browser-driven turtle control and cable-free
file transfer (`curl -T`) on both WiFi boards. **Gate closed:**
[`http-server-design.md`](http-server-design.md) (v3; all open
questions resolved with the user 2026-07-10). Numbered P7 because P6 was
reserved for the `launch` process design flagged in the P5 doc (now drafted
above).

- Demon-driven surface: `http.listen 80` · `when [http.request?] [...]` ·
  `http.respond`, with accessors (`http.method`/`path`/`query`/`body`/
  `reqheader`/`remote`). Plain HTTP only (`LOGO_HAS_WIFI`); HTTPS serving
  rejected (self-signed-cert pain, no trust gained on a LAN).
- Non-blocking pump on the demon poll sites: one connection at a time, always
  `Connection: close`, auto-responses for malformed/stalled/unanswered
  requests; listener follows the demon lifetime (`cs`/error-unwind).
- Three new device ops (`network_tcp_listen`/`unlisten`/`accept`) beside the
  existing TCP client ops; accepted connections reuse the client
  read/write/close path; mock gets scripted connections.
- Milestones: M1 device ops → M2 pump/parser → M3 Logo surface →
  M4 hardware validation + `webturtle.logo` → M5 file transfer
  (`http.respondfile`/`http.savebody`; oversized bodies fire unread and
  stream straight to storage — binary-safe, works on the 4 KB-buffer
  Pico 2 W).
- **Added scope (2026-07-12), folded into the design doc (v3, §7,
  milestone M0):** mDNS responder so other machines on the LAN can reach
  the server by name (`http://picologo.local` instead of an IP address) —
  lwIP ships an mDNS responder app the Pico SDK already builds
  (`pico_lwip_mdns`). Comes with `wifi.sethostname`/`wifi.hostname` primitives; the
  hostname excludes the `.local` suffix (mDNS appends it). User decisions
  (2026-07-12): the responder starts with `wifi.connect`, not
  `http.listen` — the device is findable on the LAN as soon as WiFi is
  up; default hostname `picologo`; no persistence across reboots — a
  custom name goes in the user's startup file.
- **Not started by user decision (2026-07-10):** another design is being
  worked first (the P8 sound design).

### P8 — Sound: stereo PSG synthesizer (design first)

**Goal:** replace the one-square-wave-per-ear PIO driver with a software
synthesizer — per ear, 3 tone voices + 1 noise voice (the SN76489 layout,
doubled) with ADSR envelopes, selectable waveforms, and a background music
sequencer. **Gate closed:** [`sound-design.md`](sound-design.md) (v1; all
open questions resolved with the user 2026-07-10 — voice map by ear with
`tell`-style voice-list fan-out, note words only with `#`/`s` accidentals,
`play` waits on a full queue, `stopsound` stop-only, music survives the
prompt while BREAK/error silence, `sound` range 20 Hz–10 kHz).

- Decided up front by the user: ADSR envelopes; rendering on core 0 in the
  DMA IRQ (no core 1 — the engine module keeps that door open); 4 voices per
  stereo channel, 3 tones + 1 noise.
- Engine: samples mixed in software, output via hardware PWM slice 5
  (GPIO 26/27 share it) + DMA ring at a 73.2 kHz carrier / 24.4 kHz mix
  rate; ~4–5 % of core 0; frees both `pio0` state machines; ≈4.8 KB SRAM
  (limits in `core/limits.h`).
- Surface: `toot` unchanged; `sound` (immediate, Atari Logo `TOOT`
  lineage), `setenv`/`env` + `setwave`/`wave` (timbre), `play` note-word
  sequencer with append semantics (TI Logo II music buffer + Terrapin/MML
  notation), `playing?`, `stopsound`.
- Prior art surveyed from primary sources: Atari Logo (Antic v2n8), TI
  Logo II manual ch. 9, LogoWriter, modern Terrapin, BBC/MSX/Atari BASIC,
  Scratch.
- Milestones: M1 engine swap (`toot`-compatible) → M2 immediate + timbre
  surface → M3 `play` sequencer → M4 game validation (Space Invaders
  retrofit, Galaxian needs).

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
| 2026-07-05 | P5 | M2 sensing: `touching?` (pixel-true mask AND, wrap-fold, both-visible), `over?`/`colourunder` (canvas beneath the mask, sprites excluded), `distance` (Euclidean); core-side geometry over new device ops `get_raster`/`canvas_point`/`sense_metrics`, mock rasters+canvas fixtures, 18 tests; host degrades to false/0 |
| 2026-07-05 | P5 | M3 autonomy + events (`core/demons.c`): edge-triggered `when` demons (arm/`[]`-disarm/`(when)`-print, `MAX_DEMONS` 8), budgeted poll (`DEMON_POLL_MS` 20) at the instruction point and the picocalc prompt idle loop, actions in a fresh nested evaluator with re-entrancy suppression; `freeze`/`thaw`; `setspeed`/`speed`/`setanim` over new device ops `set_speed`/`get_speed`/`set_anim`/`turtle_tick`; new monotonic `ticks_ms` hardware op (host/picocalc/mock); lifetime — `cs` and toplevel error-unwind clear demons and stop motion/animation; 18 tests + `logo/m3accept`. Decisions: speed = steps/second, `setanim first last interval_ms`, `freeze` suspends demons *and* motion |
| 2026-07-10 | P5 | Done: Space Invaders game shipped (#101, 2026-07-06; migration tool #102, 2026-07-10) as the end-to-end exercise of the sprite stack — M0–M3 all validated in a real game. `launch` processes stay behind the P6 design gate |
| 2026-07-10 | P7 | HTTP server design gate closed: `http-server-design.md` v2 — demon-driven server on the demon poll sites, three TCP server device ops, milestones M1–M5 incl. file transfer (`http.respondfile`/`http.savebody`, oversized bodies fire unread); HTTPS serving rejected; five open questions resolved with user (demon-rule lifetime, raw `http.query`, explicit port, links-only `webturtle.logo`, handler-side write auth). Implementation deferred — another design first |
| 2026-07-10 | P8 | Design draft: `sound-design.md` v1 — 2×(3 tone + 1 noise) software PSG with ADSR over PWM slice 5 + DMA on core 0 (user decisions: ADSR, core-0 IRQ, 8 fixed-ear voices); prior-art survey from primary sources (Atari Logo `TOOT`/`SETENV` via Antic v2n8, TI Logo II music system ch. 9, LogoWriter `tone`, Terrapin `PLAY` notation, BBC `ENVELOPE`, MSX MML, Scratch); surface: `toot` unchanged, `sound`, `setenv`/`setwave`, `play` append-sequencer, `playing?`; `play [notes]` backlog item absorbed; open questions Q1–Q6 pending user |
| 2026-07-11 | Cheap wins | Done: `remove`/`remdup` (word + list, `equal?` semantics, `remdup` keeps the last of equals), `localmake`, `tan` + two-input `(arctan x y)` (`atan2`), `modulo` (sign-of-divisor floor division), `runresult` (new `OP_RUNRESULT` trampoline op mirroring `catch`). Reference sections + Unity tests for each; 56/56 ctest green, pico2 links (RAM 93.5%). Graphics-screen text deferred by user — it needs a device text op, not just a core primitive |
| 2026-07-12 | Language: medium | Done: long words via blobs — `prim_word` now concatenates through a stack/heap buffer and finishes with `mem_word` instead of `mem_atom_cstr`, so a >255-char result blobs into PSRAM on the Pico Plus 2 W and still errors (rather than truncates) on non-PSRAM boards; unifies with the `mem_word` path already used for HTTP bodies and `reverse`/`shuffle`. Reference `word` + board sections updated, PSRAM success test added; 57/57 ctest, pico2 (RAM 93.5%) and pico+2w link |
| 2026-07-10 | P8 | Sound design gate closed: Q1–Q6 all resolved with user — voices by ear (0–3 L / 4–7 R, noise 3 & 7) with `tell`-style voice-list fan-out; note words only, `#` and `s` both accepted for sharp; `play` waits (BREAK-able) on a full queue; `stopsound` stops without resetting timbre; music keeps playing at the prompt, BREAK/toplevel-error silence, `cs` untouched; `sound` range 20 Hz–10 kHz. Implementation may begin at M1 |
| 2026-07-12 | P7 | Scope added and folded into `http-server-design.md` (v3, §7, milestone M0): mDNS responder so LAN machines reach the device by name (`http://picologo.local`) via lwIP's `pico_lwip_mdns`; `sethostname`/`hostname` primitives (label-validated, `.local` excluded, `HOSTNAME_MAX` in `core/limits.h`); new `network_set_hostname` device op also feeding the DHCP hostname. User decisions: responder starts with `wifi.connect`, not `http.listen`; default hostname `picologo`; no reboot persistence (custom names go in the startup file) |
| 2026-07-12 | Roadmap | Synced with `docs/`: companion list now indexes every design doc; LittleFS filesystem (PR #83, 2026-06-29 — pre-roadmap) recorded under a new Platform table |
| 2026-07-12 | P6 | Design draft: `launch-design.md` v1 — a process is a paused evaluation (own evaluator/op-stack/arena, zero C stack when suspended); yield-at-outermost-trampoline scheduling at the demon poll sites; evaluator audit shows proc bodies/loops are op-driven (suspendable) while `map`-style lambdas run whole per turn; measured budgets (host: `OpStack` 58 KB, `EvalOp` 224 B; pico2 ~34 KB free) drive the runtime-sized-OpStack + per-process-arena plan (~8 KB/process); `launch`/`halt`/`(launch)`, `wait` sleeps the process, lifetime = demons' rules; broadcast designed, recommended deferred; open questions Q1–Q6 pending user |
| 2026-07-12 | P6 | Design gate closed: Q1–Q6 all resolved with user (every recommendation accepted) — uniform SRAM process pool on all boards (count fixed at M0), error in a process unwinds everything (demon rule), broadcast deferred, keyboard readers error in a process while `play` yields, `halt` stops processes only, `launch` inherits the launcher's active turtle set. Implementation may begin at M0 |
| 2026-07-13 | P7 | M0 done: mDNS naming — new `network_set_hostname` device op; core-owned name (`sethostname`/`hostname` in `primitives_wifi.c`, `HOSTNAME_MAX` 32, label validation, default `picologo`, no reboot persistence); picocalc sets the netif hostname (DHCP) and starts/renames/stops the lwIP mDNS responder on `wifi.connect`/`sethostname`/`wifi.disconnect`; `lwipopts.h` enables `LWIP_MDNS_RESPONDER`/`LWIP_IGMP` (+1 UDP PCB, +4 sys-timeouts, 1 netif-client-data slot), CMake links `pico_lwip_mdns`; mock records the name. 11 new wifi tests (default, rename before/after, label validation), 57/57 ctest green, all three firmware presets link. Hardware validated 2026-07-16: `ping picologo.local` resolves and `sethostname` re-announces under the new name on a real board |
| 2026-07-16 | P7 | Large-upload robustness, round 2: small files transferred fine but ~150 KB+ still failed (RST/`503`). Two more fixes. (1) Replaced the `ERR_MEM` receive backpressure — whose refused segments only retried on lwIP's 250 ms timer, crawling/stalling big transfers — with **deferred `altcp_recved`**: the window reopens from the read path as bytes are drained, so it tracks consumption continuously (no drops, no stalls, ring stays ≤ `TCP_WND`). (2) `lfs_stream_write_bytes` seeked before every write, and `lfs_file_seek` flushes LittleFS's cache, so a chunk-streamed upload forced a flash flush per 512 B → heavy copy-on-write churn (slow, and transient block exhaustion → write fails → `savebody` errors → RST). Now seeks only when the position actually differs, letting LittleFS batch to block boundaries. 59/59 ctest, all presets link. Hardware re-validation pending |
| 2026-07-16 | P7 | Hardware bug fixes (large uploads): small files transferred fine, but a 20 MB upload corrupted/stalled to a `503` after ~97 s and then bricked the server (next connect refused; re-`fileserver` → "Can't open http server"). Two picocalc TCP fixes: (1) the receive callback dropped-and-ACKed overflow when the 2 KB ring filled faster than `http.savebody` drained — replaced with real **backpressure** (`ERR_MEM` refuse-when-full), ring bumped to 4 KB (> `TCP_WND`) so a refusal can't deadlock; (2) a server connection abandoned without a response is now **RST-closed** (not graceful FIN) so its port frees immediately with no TIME_WAIT, fixing the "Can't open" on re-run. 59/59 ctest green, all presets link. Note: 20 MB still exceeds the internal flash, so such an upload now fails cleanly at disk-full and the server stays restartable. Hardware re-validation of `curl -T` pending |
| 2026-07-16 | P7 | Example added: `logo/fileserver` — browse/download files in a browser (a `map`-built HTML index at `/`), download with `curl -o`, upload with `curl -T` (`http.savebody`); GET/PUT routing, 404 for missing files, and a documented shared-secret guard for uploads. Verified loading + HTML building on the host |
| 2026-07-16 | P7 | M5 done: file transfer — `http.respondfile status path` streams a file to the connection as the body (default `application/octet-stream`, overridable; missing file is an ordinary error left pending so a handler can `404`), `http.savebody path` streams the request body to a file (buffered head written from `g_buf`, then, for an oversized body, the tail drained from the socket). Oversized bodies now **fire unread** instead of auto-`413`: `http.body` errors and points at `http.savebody`; the pump keeps the buffered head and leaves the rest in the socket. Both route through the storage router and reject `..`; both binary-safe via a small chunk buffer (`HTTPD_CHUNK_MAX`). Mock filesystem made binary-safe (`write_bytes` + `mock_fs_create_file_bytes`); 7 new tests incl. a >buffer-cap binary round-trip with NUL bytes (`test_httpd` now 45), 59/59 ctest green, all presets link. Hardware `curl -T` round-trip on a Pico 2 W pending |
| 2026-07-16 | P7 | Hardware bug fix: `http://picologo.local` returned `408` on a real board (ping worked). Cause — the picocalc accept path parked the new PCB and wired its receive callback ~20 ms later in `network_tcp_accept`, but lwIP frees inbound data delivered to a PCB whose recv callback is still NULL, so a browser's request (sent right after the handshake) was dropped, stalling to the `408`. Fix: the listener pre-allocates one connection slot in `listen` (never `malloc` in the lwIP background callback) and the accept callback attaches the PCB and wires recv immediately; `close` on a server connection detaches but keeps the slot for reuse, `unlisten` frees it. Design §5 updated; 59/59 ctest green, all presets link. The mock can't reproduce this (no real lwIP timing) |
| 2026-07-16 | P7 | Rename (user): `sethostname`/`hostname` → `wifi.sethostname`/`wifi.hostname`, moving them under the `wifi.` namespace with the rest of the WiFi/naming surface. Registrations, handler names (`prim_wifi_*`), reference sections/headings, `webturtle`, and design §3/§7 updated; 59/59 green, all presets link |
| 2026-07-16 | P7 | Lifetime revisited (user): kept `cs`/error-unwind closing the listener — a `cs` clears the handler demon anyway, so a surviving listener would only sit headless and `503`; the clarifying point is that `cs` in a handler disarms the handler (and drops the connection), so in-handler screen clears use `clean`/`clean home`, unrelated to the listener. Instead removed the re-run footgun by making `http.listen` **idempotent** (same port = no-op, new port = move); dropped the "already listening" error. Design §3/§10 + reference updated; `test_httpd` now 38 tests, 59/59 green, all presets link |
| 2026-07-16 | P7 | M4 partial (software): `logo/webturtle` example — a phone-browser turtle controller (heading + forward/left/right/clean links, each a GET the handler maps to a turtle command; `/clean` uses `clean` not `clearscreen` so the reset lifetime doesn't close the server). Added `http.element tag content` / `(http.element tag content name value ...)` HTML-builder primitive (user request during M4, folded into design §3) — content word-or-list formatted like `print`, nests by composition, attribute pairs in the parenthesised form; removes the `char 60`/`char 62` gymnastics the lexer would otherwise force. 7 new tests (`test_httpd` now 37), reference section, 59/59 ctest green, all three presets link. Browser hardware validated 2026-07-16 on a Pico 2 W: `http://picologo.local` loads the page and the link taps drive the turtle (after the accept-path recv-callback fix below) |
| 2026-07-16 | P7 | M3 done: Logo handler surface — request accessors `http.method`/`path` (percent-decoded)/`query` (raw)/`body`/`reqheader` (case-insensitive, empty list if absent)/`remote`, each erroring when no request is pending; `http.respond status body` with the parenthesised `(... name value ...)` extra-header form, `text/plain` default overridable by a `Content-Type` pair, framing headers (`Content-Length`/`Connection: close`) always set by the pump and skipped if the handler passes copies, CR/LF-injection rejected, body streamed straight from the Logo value via `format_value`. End-to-end `when [http.request?] [http.respond ...]` demon test and a client-`http.get`-mid-handler test proving the server/client buffers don't collide; 13 new tests (`test_httpd` now 30), 59/59 ctest green, all three presets link. Reference "HTTP Server" chapter completed (accessors + `http.respond`); HTML-body examples use backslash-escaped angle brackets |
| 2026-07-16 | P7 | M2 done: request pump + parser (`core/httpd.c`) on the demon poll sites (pumped before demons in `eval.c` and the picocalc prompt idle so a just-completed request is visible to a `when [http.request?]` handler same-tick); `http.listen`/`http.unlisten`/`http.request?` primitives; incremental parse of request line + headers + Content-Length body with percent-decoded path; own lazily-allocated tiered request buffer (PSRAM 64 KB / SRAM 4 KB caps in `core/limits.h`); auto-responses 400/408/411/413/414/431 and the 503 unanswered-deadline; stall/response timers accumulate only across active polls so they pause under `freeze`; listener follows the demon lifetime (`cs`, repl error-unwind, prompt demon-error all `httpd_reset`). Mock gains a stall mode + response getter; new `test_httpd.c` (17 tests), 59/59 ctest green, all three firmware presets link. Reference gains an "HTTP Server" chapter (listen/unlisten/request?) |
| 2026-07-16 | P7 | M1 done: TCP server device ops — `network_tcp_listen`/`unlisten`/`accept` in `hardware.h`; picocalc altcp listener (bind + listen + accept callback parks one PCB per `TCP_LISTEN_BACKLOG`, claimed PCB wrapped in the existing `TcpClientState` so read/write/close are unchanged); mock scripted connections (`mock_httpd_queue_connection[_ex]`, dribble reads, recorded responses, remote-IP reporting) with read/write/close/can-read dispatching client-vs-server by handle; host ops NULL. New `test_httpd_device.c` (9 tests: listen/accept/read/write/close, accept-when-empty, dribbled reads, unlisten-drops-pending, serial connections), 58/58 ctest green, all three firmware presets link. Hardware raw-TCP smoke test pending on real boards |
