# Roadmap

Tracks **features** — past, present and future. Defects live in
[`bugs.md`](bugs.md); if an item adds capability it belongs here, if it means
"this doesn't work as documented" it belongs there.

Compares the current implementation
(everything in `reference/Pico_Logo_Reference.md`, 296 primitive/section
headings as of 2026-07-03) against the wider Logo family (Apple Logo II — the
model dialect — plus UCB/Berkeley Logo, Atari Logo, Terrapin, FMSLogo).

Companion documents (everything in `docs/`):
- [`multi-sprite-design.md`](multi-sprite-design.md) — P5 sprites/collision/demons
  (implemented, M0–M3).
- [`launch-design.md`](launch-design.md) — P6 `launch` background processes
  (design complete, implementation not started).
- [`http-server-design.md`](http-server-design.md) — P7 HTTP server + mDNS
  (implemented, M0–M5, merged to `main` via #108; `curl -T` hardware
  validation pending).
- [`sound-design.md`](sound-design.md) — P8 stereo PSG synthesizer (design
  complete, implementation may begin).
- [`littlefs-filesystem-design.md`](littlefs-filesystem-design.md) — internal
  LittleFS root + `/sd` FAT32 mount (implemented, PR #83, 2026-06-29 —
  predates this roadmap).
- [`memory-reclamation-design.md`](memory-reclamation-design.md) — atom GC
  (implemented); `erall` soft reset remains deferred.
- [`space-invaders-design.md`](space-invaders-design.md) /
  [`galaxian-design.md`](galaxian-design.md) — shipped games (#101, #106) that
  validate the sprite stack.
- [`checkpoint-run-design.md`](checkpoint-run-design.md) /
  [`turtle-trails-design.md`](turtle-trails-design.md) — shipped games (#124)
  that validate the tile-map, deterministic `sync`-mode simulation and
  mutate-in-place memory patterns. Turtle Trails also draws its whole maze
  from its own map, so it ships without a picture asset.
- [`code-review-2026-07-02.md`](code-review-2026-07-02.md) — the review that
  produced PR #86; a few small refinements from it are tracked below, its
  defects in [`bugs.md`](bugs.md).
- [`bugs.md`](bugs.md) — the bug tracker (open and fixed defects).

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
| `setpensize` / `pensize` | UCB, FMSLogo | done | Landed 2026-07-18; stamped-disc, single-integer diameter (1–32), `penreverse` stays 1px |
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
| `play [notes]` background melody | done | Landed 2026-07-18 as the P8 music sequencer: a real background sequencer over the 8-voice PSG (note-word notation, append semantics), not a melody of `toot`s. See [P8](#p8--sound-stereo-psg-synthesizer-design-first) |
| Help discoverability | done | [P4](#p4--arc-and-help-discoverability): keyword search fallback in `help`, `(help)` topic listing, "did you mean" on unknown names |
| `.reset` — clear the workspace for the next program | todo | One command instead of the `erall` + `cleardemons` + `cs` + `stopsound` + `recycle` incantation; buried procedures survive. See [`.reset`](#reset--clear-the-workspace-for-the-next-program) |

### Language: big bets

| Item | Status | Notes |
|---|---|---|
| Multiple turtles/sprites (`tell`/`ask`/`each`), `touching?`, `when` events | done | All milestones landed (M0–M3); validated end-to-end by the Space Invaders game (#101/#102). `launch` processes are the P6 design below |
| `launch [instrs]` background processes | todo | Design done (gate closed 2026-07-12), implementation not started: [P6](#p6--launch-background-processes-design-first). `broadcast` deferred per Q3 |
| Non-blocking `wifi.start` + `wifi.status` | done | Landed 2026-07-21; a startup file reaches the prompt immediately and a `when [wifi?] [network.ntp ...]` demon does the follow-up. Independent of P6 — `launch-design.md` never cited WiFi as a motivating case |
| HTTP server (`http.listen`, `when [http.request?]`, `http.respond`, file transfer) | done | M0–M5 implemented, merged to `main` (#108, 2026-07-16): mDNS + `wifi.hostname`/`wifi.sethostname`, TCP server ops, demon-driven pump/parser, handler surface + `http.element`, `webturtle` example, file transfer. Browser + mDNS hardware-validated; `curl -T` upload validation pending. Design: [P7](#p7--http-server-implemented) |
| Arrays (`array`/`setitem`) | deferred | O(1) indexing; needs a new object kind (likely blob-backed). Wait for demonstrated need |
| Atom reclamation / `erall` soft reset | done / deferred | Atom reclamation landed 2026-07-23; `erall` soft reset remains deferred. See `memory-reclamation-design.md` |
| Tile maps + smooth scrolling (accelerated tile games) | in progress | M1+M2 done and **hardware-accepted** 2026-08-02 — Trails' board bakes in **7.45 ms** against the 5,916 ms `draw.board` it replaces (794×) (bank, map, bake path: `newtiles`/`snaptile`/`newmap`/`settile`/`tile`/`stampmap`/`stamptile`). **M3 done and hardware-accepted 2026-08-02**: Turtle Trails revamped in place — the board is the C map and `draw.board` is a `stampmap`. The level build is **56.7 → 3.1 ms on the host (18×)** and 303 ms on a Plus 2 W against a 5,916 ms Pico 2 figure. Two things the run caught: a blank maze, which was **B11** (`dot` ignored the pen size on the PicoCalc) and not the tile system; and that **the C map does not move the frame** (host 0.616 → 0.597 ms). So §3.4's and P10 §7's expectation that it closes Trails is contradicted. **Caveat: the hardware runs are on a Plus 2 W and every baseline is a Pico 2** — design §13.5 has the same-board re-run that settles it. Design drafted 2026-07-29 ([`tilemap-scrolling-design.md`](tilemap-scrolling-design.md)); M0 measured 2026-08-01 and the gate **failed** — the interpreter, not the wire, is the bottleneck, which opened [P10](#p10--interpreter-throughput). Split (design §3.4): the bake half (`stampmap`/`stamptile` + C map, deleting the 5,916 ms `draw.board` and 1,346 ms `draw.sector`) proceeds now; the scrolling half waits on P10's M3 re-measure. All boards, tiered capacity. See [P9](#p9--tile-maps-and-smooth-scrolling-design-first) |
| Interpreter throughput (games hit their frame budgets) | done | Opened 2026-08-01 by P9's failed M0 gate, design drafted ([`interpreter-throughput-design.md`](interpreter-throughput-design.md)): the display was never the bottleneck — both shipped games run at ~9 fps and ~4 fps against a designed 25, and ~48 % of interpreter runtime is spent re-deriving facts that cannot change (word class re-lexed every evaluation, names resolved by `strncasecmp` every call). Memoise them on the interned atom. Target: Turtle Trails' `play.frame` under 40 ms, from 87.3 ms. M0–M3 done 2026-08-01, M4 declined. M1 (word class) delivered all of it on hardware — Trails 87.3 → **73.4 ms**, Checkpoint Run 258.6 → **232.6 ms**. M2 (name binding) flattened the workspace-scan cliff (**128.3 → 24.0 µs** per call) and returned 9 KB of SRAM, but moved neither game and regressed the profiled loop 1.64× on the board. **§1's 40 ms is not met**, and P9's C map — named here as what would close Trails — landed on 2026-08-02 and moved the frame by 0.2 ms (73.4 → 73.6). That expectation is **disproved** (P9 design §13.4): it misread P9 M0, which measured `step.bugs` at 59 % of a frame rather than the `tile.at` walk inside it. **M5 profiled the frame on 2026-08-02 (design §11.1) and found one.** There is no hot spot — 791 operations on the board against 787 predicted from the host, every slot proportional to its statement count — but a `make "x (:x + 1)` costs **102.5 µs against a procedure call's 24 µs, 4.3×, where the host ratio is 2.5×**. Calls scale host→board at 75×, a `make` with arithmetic at 129×. M2 made calls cheap; the statement itself is what is left, and the hot slots are almost nothing but `make` statements. The uncached piece inside it is **variable resolution**, which §3.2/§7 set aside as dynamically scoped — a reason it cannot use M2's mechanism, not a reason it must stay slow. Before M5, the target had no named lever, and M4 and the bytecode body — the only candidates then left — had both been rejected partly on the strength of the disproved claim. **M5 (design §11) is therefore to re-profile before choosing**: `logo/tests/p10prof` splits a frame into its thirteen parts on a board and reports each in *operations* as well as milliseconds, so "no hot spot exists" is a result the profile can actually return. **It returned exactly that, and the answer was the flash.** The board:host ratios were 60× for a bare loop and 67× for a call against 132× for an arithmetic statement and 212× for the parenthesised-call path -- the RP2350 executes the interpreter from flash through a 16 KB XIP cache, and the code entered once per statement pays for it. Four tiers of `__not_in_flash_func` (design §11.2–§11.6) took the frame **81.0 → 47.0 ms, 1.72×, for 13.6 KB of SRAM**, `sync` flat at 1.6-1.8 ms throughout as the control. Returns halved every tier — 1.24×, 1.23×, 1.105×, 1.024× — so the tiering is done. **§1's 40 ms is still not met**, by 1.17×, but it is now a game-side number: `step.bugs` and `place.all` are 65 % of the frame and are nothing but statements. Enabled on the `pico2w` and `pico+2w` presets. See [P10](#p10--interpreter-throughput) |

### Platform

| Item | Status | Notes |
|---|---|---|
| LittleFS internal filesystem (root `/`) + `/sd` FAT32 mount | done | Landed 2026-06-29 (PR #83), before this roadmap existed; listed for completeness. Design: [`littlefs-filesystem-design.md`](littlefs-filesystem-design.md) |

### Documentation

| Item | Status | Notes |
|---|---|---|
| "Differences from other Logos" appendix | done | Landed 2026-07-18; expanded the existing "Difference from other Logo interpreters" chapter (named lambdas vs `?` templates, case-insensitive comparisons, single-precision floats + `n`-notation, no arrays despite `.setfirst`/`.setbf`/`.setitem`) |
| Document the TCO guarantee | done | Landed 2026-07-18; new "Tail Call Optimization" note — self tail-recursive calls reuse the frame (constant space) and don't count against the 128/192-level recursion limits; cross-linked from Supported Pico Boards |

### Implementation refinements (code-review leftovers)

Performance and cleanup only — defects moved to [`bugs.md`](bugs.md).

| Item | Status | Notes |
|---|---|---|
| TCO lookahead double-parse on last lines | todo | Measure after PR #86's lookup speedups; may no longer matter |
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

### Done — `setpensize` / `pensize`

Landed 2026-07-18. `setpensize` takes a single integer diameter (rounded,
clamped to `MAX_PEN_SIZE` = 32 in `core/limits.h`); `pensize` outputs it. Pen
size is per-turtle state beside the pen colour/state, reached through new
`set_pen_size`/`get_pen_size` device ops (host leaves them NULL).

- **Algorithm: stamped disc.** `screen_gfx_line` gained a `width` parameter and
  stamps a filled disc of diameter `pensize` at each Bresenham step instead of
  a single pixel. Solid by construction (consecutive stamp centres are 1 px
  apart, so stamps always overlap), uniform apparent width at every angle, and
  round caps/joins for free — which hides the seams between segmented curves
  like `arc`. Rejected: parallel offset strokes (up to ~29 % thinner at 45°,
  notched corners at every turtle turn).
- **Reverse mode still draws 1 px wide.** `penreverse` toggles pixels, so
  overlapping thick stamps would toggle shared pixels twice and speckle. Left
  as a documented limitation; a future thick-reverse pass would delta-stamp
  only the pixels the previous stamp did not cover (or fill per-scanline
  spans).
- **Hardware validation:** `load "penaccept` (`logo/tests/penaccept`) and run
  `penaccept`. The steps check pen-size state (round/clamp/reject), solidity at
  every angle (sunburst), round joins, a thick `arc`, thick erase, and the
  1 px reverse pen; `pencap` writes `pentest.bmp` via `savepic` for a
  pixel-exact check on a computer.

### P5 — Multi-sprite turtles with collision (implemented)

**Goal:** turn the PicoCalc into a game machine: `tell`/`ask`/`each`,
`touching?`, and Atari-style `when` demons. **Design doc:**
[`multi-sprite-design.md`](multi-sprite-design.md) (v2, gate closed
2026-07-04).

- **M0 — display pipeline rework**, a prerequisite the design surfaced:
  tile dirty tracking (`dirty_tiles.c`), DMA-pipelined blit
  (`lcd_blit_begin/row/end`), scanline sprite compositor, and
  `setrefresh`/`refresh`/`refreshmode` (auto restored on `cs`/error).
- **M1 — sprite model + addressing:** 8 turtles addressed via `select`
  (sprite id = turtle number, lower on top), `tell`/`ask`/`each`/`who`
  with command fan-out and lowest-active queries, a colour costume pool
  (`costumes.c`, 8 KB compact-on-free), `snapsh`/`stamp`,
  `setrot`/`setmag`. Single-turtle programs are unaffected.
- **M2 — sensing:** `touching?` (pixel-true mask AND, wrap-fold,
  both-visible), `over?`/`colourunder` (canvas beneath the mask, sprites
  excluded), `distance` (Euclidean) — over new device ops
  (`get_raster`/`canvas_point`/`sense_metrics`); host degrades to
  `false`/`0` (no graphics device).
- **M3 — autonomy + events** (`core/demons.c`): edge-triggered `when`
  demons (`MAX_DEMONS` 8, budgeted poll), actions run in a fresh nested
  evaluator with re-entrancy suppression; `freeze`/`thaw`;
  `setspeed`/`speed`/`setanim`. `cs` and toplevel error-unwind clear
  demons and stop motion/animation. `launch`-as-process was designed for
  but deferred to the P6 gate.

**Validated end-to-end** by the Space Invaders (#101/#102) and Galaxian
game ports — real games exercising tell/ask/each, collision, and demons
together, not just unit tests.

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

### P7 — HTTP server (implemented)

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
- **Implemented and merged to `main` via #108 (2026-07-16).** Browser +
  mDNS hardware-validated; `curl -T` large-upload validation on real
  hardware still pending.

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
- **M1–M3 implemented 2026-07-18** (core + device engine + tests, all three
  firmware presets link). M4 game retrofit and hardware A/B listen pending.

### P9 — Tile maps and smooth scrolling (design first)

Status: **design drafted 2026-07-29; M0 measured 2026-08-01 — gate FAILED**
([`tilemap-scrolling-design.md`](tilemap-scrolling-design.md) §3.3–§3.5): a
present is 21–26 ms but the game frame bodies are 87.3 ms and 258.6 ms, so
the interpreter, not the display, is the bottleneck — which opened
[P10](#p10--interpreter-throughput). The item is split (design §3.4): the
**bake half** — `stampmap`/`stamptile`, the C map, and the render-only
Turtle Trails revamp — proceeds on its own schedule and needs no frame
budget; the **scrolling half** (live view, `setscroll`, the Checkpoint Run
camera) is blocked until P10's M3 re-measure shows a frame body under
40 ms. Scoping decided with the user: available on
**all three boards** with tiered capacity (SRAM tier everywhere, PSRAM tier on
the Plus 2 W); the Turtle Trails revamp is render-only (board and gameplay
unchanged); both game revamps replace the shipped games in place. The prose
below is the original roadmap sketch; where they differ, the design doc wins
(notably: per-bank tile size 8 or 16 rather than one compile-time size, a
viewport rather than full-screen-only, and a `stampmap`/`stamptile` bake path
for non-scrolling tile boards).

**Goal:** make scrolling tile games — the Rally-X / Super Mario Bros. shape —
practical on the PicoCalc: a map of tiles much larger than the screen, a view
into it at an arbitrary **pixel** offset so scrolling is smooth rather than
tile-stepped, and tiles that are *drawn* with the turtle and then snapped out
of the canvas into a reusable bank. Nothing here is a new kind of graphics; it
is the existing `snapsh`-draw-then-pick-it-up idiom applied to a background
layer instead of a sprite.

**Why now.** Both shipped tile games hit the same wall from opposite sides.
Checkpoint Run rebuilds a sector by *redrawing* it — ~400 stamps per crossing —
and its design (§4.2) closes with "do not add a framebuffer-scroll or tilemap
primitive for this game without revising this design… the case should be made
with numbers from **both** this game and Turtle Trails." Turtle Trails avoids
the problem by fitting its whole 28×36 board on screen at once, which is
exactly the constraint that rules the genre out. This item *is* that revision,
and its gate is those numbers.

- **The insight that makes it cheap:** the display already composites. Since
  P5/M0, `gfx_buffer` holds only the canvas and each outgoing row is expanded
  into a line buffer at blit time, with sprites overlaid per scanline
  (`multi-sprite-design.md` §2.4). A scrolling background is the *same*
  operation one layer lower: when a map view is active, the row's source
  becomes the tile bank sampled at `(scroll_x + x, scroll_y + y)` instead of a
  `memcpy` from `gfx_buffer`. Scrolling then costs **nothing per pixel of
  offset** — no canvas move, no redraw, no re-stamping — because the picture is
  never stored, only generated. Changing `scroll_x` by one pixel is one integer
  write.
- **The number to measure first (M0):** a scroll dirties every tile, so each
  frame is a full-screen blit — 320×320 8-bit expanded to RGB565 over the
  existing DMA pipeline. Whether the sample-from-map row builder holds 30 fps
  on a Pico 2 is the whole feasibility question, and it decides the shape of
  everything below. Measure with `ticks` before writing the surface, and
  measure the two shipped games' redraw costs beside it so the comparison the
  Checkpoint Run design asked for is on the record.
- **Tile bank:** fixed-size square tiles (8×8 or 16×16 — one size, chosen at
  M0, not per-map), 8-bit indexed like the canvas, in a pool separate from the
  15 shape slots so a game can have a full tile set *and* its sprites. Filled
  by capture: draw the tile with the pen anywhere on the canvas, snap it into
  the bank, clear, repeat — so a game ships without a picture asset, the way
  Turtle Trails already draws its own maze.
- **Map storage:** one byte per cell, so a 64×64 map is 4 KB and a 128×128 map
  is 16 KB. Too big for Logo lists (a cons per cell is out of the question) and
  awkward as a blob, so this most likely wants its own fixed region with a
  tiered cap — small in SRAM, large in PSRAM — following the HTTP body and
  costume-pool precedent, sizes in `core/limits.h`. **The memory plan is the
  open question**, not the rendering: SRAM is at ~95.6 % on pico2.
- **Surface sketch** (names are provisional — `window` and `map` are both taken
  by existing primitives, so the view is `setscroll`/`scroll`): `snaptile n`
  captures the tile under the turtle into bank slot _n_; `newmap cols rows`
  allocates and clears; `settile col row n` / `tile col row` write and read
  cells; `setscroll x y` / `scroll` move the view in pixels;
  `showmap`/`hidemap` switch the background source between the map and the
  ordinary canvas. Pen drawing, `dot?`, `fill` and `savepic` keep seeing the
  canvas alone, exactly as they keep seeing it free of sprites today.
- **Levers if M0 misses budget:** the LCD's hardware vertical scroll
  (`lcd_define_scrolling`/`lcd_scroll_up`, already driving text scrolling)
  makes vertical-only scrolling nearly free by shifting the panel's start line,
  at the cost of horizontal motion; or restrict smooth offset to one axis; or
  blit only the moving band. Each of these narrows the genre, so take them only
  with numbers in hand.
- **Open questions for the gate:** tile size; where the map lives and how big
  it may be on each board tier; whether a map is bigger than one screen only,
  or wraps (Rally-X's radar-mapped world wraps, Mario's does not); whether tile
  cells carry a "solid" bit for collision or games keep that in their own list;
  how `touching?`/`over?`/`colourunder` read a map-sourced background; what
  `cs` and the error unwind do to a bank and a map.
- **Milestones** (resequenced 2026-08-01 after the M0 verdict; design §13):
  M0 measure + gate (done, failed) → M1 tile bank and capture (done) → M2 map
  storage and the bake path (done) → M3 Turtle Trails render revamp (done) →
  M4 live view and pixel scrolling (**gated on P10 M3**) → M5 Checkpoint Run
  camera retrofit as the before/after.

**M1+M2 landed 2026-08-02** (design §13.1), taken together because a bank
nothing can draw is not reviewable on its own: `newtiles`, `snaptile`,
`newmap`, `settile`, `tile`, `stampmap`, `stamptile` over a new
`core/tilemap.c`, with a `# Tile Maps` reference chapter. The bake path is
what M3 needs; the live view (`showmap`, `setscroll`) is still M4 and still
gated.

**M3 landed 2026-08-02** (design §13.3): Turtle Trails' board is the C map
now — a cell holds a bank slot ordered so `walk?`, `nest.open?` and
"paintable" are each one comparison — and `draw.board` is a `stampmap` over
50 tiles the game draws once with the same pen-8 that used to carve the maze.
The nested Logo map, `decode.map`, `fill.board`, `carve.paths` and
`draw.specks` are gone; the level build is **56.7 → 3.3 ms** on the host
against the 5,916 ms `draw.board` it replaces. **Hardware-accepted the same
day** (design §13.4), over two runs: the level build is **303 ms** and
`draw.board` alone 20 ms. Those are **Pico Plus 2 W** numbers against a
**Pico 2** baseline of 5,916 ms, so "about 19×" is the most that pair can
say; the host, where before and after are one machine, shows 18×
independently and is the figure to lean on (design §13.5). The first run caught two
things no native test could. The maze came up **blank**, which was
[B11](bugs.md) — `dot` ignored the pen size on the PicoCalc, so every
captured tile was background with one hedge pixel in it; already shipping
(Trails' specks and blossoms had been 1-px dots all along) and invisible to
the mock, which recorded no pen size for a dot. And the frame was 8 % the
wrong way, which turned out to be two reads of the dynamically scoped
`:sl.dc`/`:sl.dr` inside `tile.at`, now written out as literals.

With both fixed, the frame is **73.6 ms** (simulation 48.6, drawing 24.8,
present 0.25) against P10 M1's **73.4 ms** — but that baseline is a Pico 2
and this run is a Plus 2 W, so the two landing 0.2 ms apart proves nothing
by itself. The host, one machine before and after, says the same thing
properly: 0.616 → 0.597 ms. So **the C map does not move the frame, and
design §3.4's and [P10 §7's](interpreter-throughput-design.md) expectation
that it is what closes Turtle Trails is contradicted.** Both misread P9 M0: it
measured `step.bugs` at 59 % of a frame, not `tile.at`'s walk inside it.
**P10's 40 ms target is unmet with no named lever left** — what remains is
ordinary interpreter overhead spread thin across `step.bugs`, which is what
P10's declined M4 and its rejected bytecode body would attack. One correction found while building that profiler: the whole
87.3 → 73.4 → 73.6 series **never presented** — `p9m0.trails` runs in text
mode, where the blit returns immediately — so those are Logo-body figures and
a real fullscreen frame is 73.6 ms plus the dirty sprite tiles.

### `.reset` — clear the workspace for the next program

**Goal:** one command that clears out the program you just ran so the next one
can be loaded onto a clean slate. Today that means remembering an incantation —
`cleardemons`, `erall`, `cs`, `stopsound`, `recycle` — and even then a previous
program's leftovers (open files, an HTTP listener, pen and sound settings) are
still there to surprise the next one.

- **Name:** `.reset`, following the dot-prefix convention already used for the
  primitives you have to mean (`.setfirst`, `.setbf`, `.setitem`) — it throws
  the workspace away without asking, so the dot is the warning.
- **What it clears** — each of these is existing teardown code, so the
  primitive is mostly a call list, not new machinery: procedures and global
  names via `erall` (which already skips buried ones, so a buried library
  survives — that's the point of burying it); demons (`cleardemons`) and, once
  P6 lands, processes (`halt`); graphics via `cs` plus the settings `cs`
  doesn't touch (`refreshmode` auto, pen size/colour/mode, turtles 1–7 hidden,
  costumes freed, `setspeed`/`setanim` defaults); sound via `stopsound` plus
  envelope/waveform defaults; an open HTTP listener (`http.unlisten`) and any
  open file handles; finally `recycle`.
- **What it leaves alone:** buried procedures and names; the filesystem (no
  file is erased) and the current prefix; the WiFi link and hostname — staying
  online across a reset is the useful behaviour, and rejoining is slow and
  unreliable on some networks; the RNG seed set by `rerandom`; and the startup
  file, which is *not* re-run — the workspace is left empty, so `.reset` is
  also the way out of a startup file that misbehaves.
- **Where:** `core/primitives_workspace.c` beside `erall`/`recycle`, delegating
  to the per-subsystem reset helpers the toplevel error-unwind path already
  calls (`demons_reset`, `httpd_reset`, …). If a subsystem lacks one, add it
  there rather than reaching into its internals from the primitive.
- **Tests:** define procedures + globals, bury one of each, arm a demon, listen
  on a port, open a file, set pen/sound state, then `.reset` and assert the
  unburied things are gone, the buried ones remain, and the device state is
  back to defaults; plus `.reset` inside a procedure body unwinding to toplevel
  rather than returning into a body that no longer exists.
- **Reference:** a `## .reset` section under Workspace Management, cross-linked
  from `erall` and `recycle`, spelling out exactly what survives.

### P10 — Interpreter throughput

Status: **M0–M3 done 2026-08-01; M4 declined; §1 target not met** —
[`interpreter-throughput-design.md`](interpreter-throughput-design.md).
Opened by P9's M0 measurement, which failed its gate and found the display was
never the bottleneck. The benchmark harness and both baselines are in
(`tests/test_bench_throughput.c` in ctest as a relative-ratio regression
guard, `logo/tests/p10m0` for the hardware side; design §6.1), with the
Pico 2 columns measured before and after M1.

**The finding.** Both shipped games miss their frame budgets badly and always
have: Turtle Trails' `play.frame` is 87.3 ms and Checkpoint Run's 258.6 ms
against the 40 ms their `(setrefresh "sync 25)` asks for — about 9 fps and
4 fps. Neither had ever been timed on hardware. `sync` does not wait when a
frame overruns, so they degraded quietly rather than failing.

**It is not the screen.** A present is 21–26 ms. The host build runs the same
interpreter against the mock device, where drawing is a recorded command
rather than a rasterised one, and the host:Pico ratio barely moves between a
frame that draws almost nothing (91×) and a board build that draws everything
(107×). Rasterisation is a minor term.

**The cause.** Sampling a pure `repeat [make "x (:x + 1)]` loop: 34 % re-lexing
and classifying words on every evaluation (`classify_word` is the largest
single leaf in the interpreter), 14 % resolving names by case-insensitive
string compare on every call, 20 % cons-cell walk and index→pointer
indirection, 8 % `memmove`/`memset`, 24 % actual evaluation.

**The design.** Words are interned, immutable atoms, so word class, parsed
numeric value and resolved binding are all pure functions of the atom —
derive them once and store them with it. Roughly half of runtime is spent
rediscovering facts that cannot change, which makes this a memoisation
problem rather than the bytecode rewrite it might look like. The one
context-dependent case (a leading `-`, which is unary or binary depending on
the previous token) keeps today's logic behind a distinguished cached class.

**Target:** Turtle Trails' `play.frame` under 40 ms — the smallest goal that
turns a shipped game back into the 25 fps it was written for. M1+M2 target
48 % of runtime, an upper bound of ~1.9×, which likely gets Turtle Trails
close but does **not** on its own rescue Checkpoint Run's 6.5× shortfall;
that one needs P9's tile map and game-side work as well. M0 is a benchmark
harness and baseline, and nothing lands without a before-and-after from it.

**M1 is done and measured on hardware** (design §6.1–§6.2). Word class is now
memoised on the interned atom, and on a Pico 2 the profiled loop went 92.4 →
**65.9 µs/iter (1.40×)**, Turtle Trails' frame 87.3 → **73.2 ms (1.19×)**, and
Checkpoint Run's 258.6 → **232.7 ms (1.11×)**. §2.2's 34 % held: 28.7 % of the
loop disappeared against a 34 % ceiling. It held *only on the board*, though —
on the host classification is a few per cent, and a perfect cache of it
(3.08 M hits, 7 misses) bought 1.5 % until the lookup itself was collapsed
into a single `mem_word_view` call. **Host shares cannot size a board
milestone in either direction.**

**M2 landed and, on hardware, moved neither game.** A name's binding is
memoised in the same 16-bit word as its class, and it did what it was designed
to do: a call no longer scales with workspace size — a full-table call went
**128.3 → 24.0 µs (5.35×)** on a Pico 2 and the scan spread flattened from
3.94× to 0.98×. It also *returned* 9,208 bytes of SRAM (`pico2` 95.62 % →
93.86 %) by deleting a one-token lookahead buffer nothing ever filled. But
Turtle Trails' frame went 73.2 → 73.4 ms and Checkpoint Run's 232.7 → 232.6 —
flat — and the profiled loop **regressed 1.64×** (65.9 → 108.1 µs/iter),
which the host does not reproduce and the algorithm does not explain
(design §6.4).

**So P10's whole gain is M1's**: Trails 87.3 → 73.4 ms (1.19×), Checkpoint Run
258.6 → 232.6 ms (1.11×). The §1 target of 40 ms is not met, and M3's decision
is to stop rather than attempt M4 — M4 attacks the same cons-cell milliseconds
P9's C map removes outright, for far more risk. What closes Trails is that
map: `tile.at` inside `step.bugs` is 59 % of the frame, 43 of the 73.4 ms, and
removing it lands the frame near 30 ms.

**One live lead.** The M2 loop regression is board-only (host:board on that
loop went 77× → 135×), which points at instruction fetch rather than
algorithm — and `pico2` reports **XIP_RAM 0 B of 16 KB, entirely unused**. The
Pico SDK can place chosen functions there (`__not_in_flash_func`); the
interpreter's token and resolution path is the obvious candidate, and it would
apply to M1's gains as well. Unmeasured, and needs a board.

**Relationship to P9:** P9's scrolling half is blocked on this item — its
M3 re-measure is the checkpoint that reopens it. P9's bake half
(`stampmap`/`stamptile`) is not blocked and should proceed independently —
it replaces the 5,916 ms `draw.board` and 1,346 ms `draw.sector` with a C
loop, the two largest stalls measured anywhere in the project. And P9's C
map (`tile`/`settile`) is complementary rather than redundant: it removes
the 36+28 cons-cell walk per `tile.at` inside `step.bugs` — 59 % of a
Turtle Trails frame — attacking the same milliseconds from the data side
that this item attacks from the interpreter side.

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
| 2026-07-18 | Documentation | Done: both Documentation todo items — expanded "Difference from other Logo interpreters" (named lambdas vs `?` templates, case-insensitive comparisons, single-precision floats + `n`-notation exponents, no arrays) and a new "Tail Call Optimization" note (self tail-recursive calls reuse the frame and run in constant space, don't count against the 128/192-level recursion limits). `reference/Pico_Logo_Reference.md` only; anchor checker + 59/59 ctest green (no code changes) |
| 2026-07-18 | P4 | Done: `setpensize`/`pensize` off hold — single-integer pen diameter (rounded, clamped to `MAX_PEN_SIZE` 32 in `core/limits.h`), state per-turtle beside pen colour/state; new `set_pen_size`/`get_pen_size` device ops (host NULL). Stamped-disc drawing: `screen_gfx_line` gains a `width` param and stamps a filled disc at each Bresenham step (solid at all angles, round caps/joins). `penreverse` stays 1px for now (avoids the double-toggle speckle). Reference sections + 7 turtle tests, 59/59 ctest green, all three firmware presets link. Hardware acceptance script `logo/tests/penaccept` (sunburst solidity, round joins, thick `arc`, thick erase, 1px reverse, `savepic` pixel check); board validation pending |
| 2026-07-19 | Refinements | Logged loader limitation: single-line `to … end` definitions aren't supported — the REPL/loader only terminate a definition on a standalone `end` line (`repl_line_is_end`), so a one-liner swallows the procedures after it. Surfaced when `sndaccept` failed with "I don't know how to sndtune" (`sndtune` and the tuneblocks above it got absorbed into `f1`); worked around by expanding `logo/tests/sndaccept`'s tuneblocks to multi-line, fix tracked in the refinements table |
| 2026-07-18 | P8 | M1–M3 done (implementation): six sound device ops added to `hardware.h` (`sound_gate`/`queue`/`status`/`stop`/`env`/`wave`) with shared `SoundEvent`/`SoundStatus`/`SoundWave` types; new `devices/picocalc/sound.c` software PSG engine — PWM slice 5 + two chained DMA channels ping-ponging a 2×192 ring paced by the wrap DREQ, mixer + linear-ADSR + 16-bit noise LFSR + per-voice sequencer in the DMA IRQ (`__not_in_flash_func`), replacing `audio.c`/`audio.pio` (deleted; `hardware_pwm` linked). `toot` rerouted through `sound_gate` on voices 0/4 (100–2000 Hz rest, wait-for-previous via `sound_status`). New core: `notation.c` streaming note-word parser + `primitives_sound.c` (`sound`, `setenv`/`env`, `setwave`/`wave`, `play`, `playing?`, `stopsound`) with voice-list fan-out, core-side timbre shadow for the getters, BREAK-able queue-full wait. `MAX_VOICES`/`SOUND_QUEUE_LEN 64`/`SOUND_RING_HALF 192` in `core/limits.h`. Mock records all six ops with a scriptable `sound_status` (drains on poll) driving the wait/`playing?` paths. 8 reference sections + `toot` cross-ref; new `test_notation` (18) + `test_sound` (29); 61/61 ctest green, all three firmware presets link (pico2 RAM 95.2%). Hardware acceptance script `logo/tests/sndaccept` (`load "sndaccept` → `sndaccept`): 12 steps — toot compat, immediate `sound`/rest, L/R stereo routing, the 2 dB volume ladder, the four waveforms, pulse duty, ADSR pluck-vs-pad, white/periodic noise + stereo explosion, a 3-voice chord, the `play` sequencer (Frère Jacques tuneblocks + a tempo/octave/length scale), a two-voice round, and `playing?`/`stopsound`. M4 (Space Invaders retrofit + hardware A/B listen) pending |
| 2026-07-19 | P8 | M4 (retrofit) done: `logo/games/invaders` moved off `toot` to the stereo PSG — four centred voice-pairs, one per sound (`[0 4]` triangle march bass, `[1 5]` sawtooth laser, `[2 6]` **sustained** square UFO siren, `[3 7]` white-noise explosions), timbres set once in `setup.sound` (from `init.game`). New in-game sounds the old `toot` couldn't carry: a laser on `fire`, a noise splat on `kill.alien`, and a two-tone UFO warble held across frames by `ufo.warble` (main loop) *under* the march/laser on other voices; `silence.ufo` gates the siren off on UFO exit/hit/death, `stopsound` on game over. Host smoke-test loads the file and runs every sound path (host sound ops NULL → core arg-validation only) clean; `space-invaders-design.md` §8 rewritten. **Hardware A/B listen on real boards still pending** — the only remaining P8 item |
| 2026-07-21 | Platform | Non-blocking WiFi connect: new `wifi.start ssid password` (kicks off the attempt and returns at once) and `wifi.status` (outputs `off`/`connecting`/`connected`/`failed`), so a startup file reaches the prompt instead of stalling up to 30 s in `wifi.connect`, and a `when [wifi?] [network.ntp -4]` demon does the follow-up once the link is really up. Two new device ops (`wifi_start`/`wifi_status`, host NULL); picocalc uses `cyw43_arch_wifi_connect_async` and reports state from **`cyw43_tcpip_link_status`** rather than `cyw43_wifi_link_status` — the latter goes `LINK_JOIN` at 802.11 association, before DHCP, which would have fired the demon while name resolution still had no route (so `wifi?` now means "connected *with an address*"). Post-connect work (DNS fallback + mDNS) factored into `wifi_configure_link()`, run by the blocking path on success and by `wifi_status` on the link-up edge. `wifi.connect` is unchanged and still blocking. Mock gains `wifi_start`/`wifi_status` with a scriptable state; 10 new tests incl. two end-to-end demon tests (fires on connect, observes a failed attempt); 61/61 ctest green, all three presets link, pico2 RAM unchanged at 95.6 %. **Hardware validation on a Pico 2 W pending.** Considered and rejected: implementing P6 `launch` for this — `launch-design.md` never cites WiFi, and a demon covers the case at a fraction of the cost |
| 2026-07-21 | Platform | Root cause, from a user trace: **joining is simply unreliable on some networks** — individual joins fail with `LINK_FAIL` and only succeed after several attempts. The user's startup file showed the retry machinery working exactly as designed (`connecting`→`failed`→`connecting`, 8 s apart, 3 times, stop at 30 s) while every attempt failed; the same credentials connected from the REPL after several manual tries. So the REPL/startup difference was never timing — it was *how many attempts each was willing to make*. This also explains the original complaint that started P-WiFi: `cyw43_arch_wifi_connect_timeout_ms` returns immediately on `LINK_FAIL` (it only retries `NONET`), so blocking `wifi.connect` failed on this network too. Fix: `wifi.start` retries indefinitely (settled failure → 3 s, DHCP stall → 8 s, never faster than 2 s), stopping only for `badpassword` or `wifi.disconnect`; the 30 s deadline is gone. Also subsumes the remaining "radio needs to settle after `cyw43_arch_init`" hypothesis, since a later attempt succeeds either way. User chose true-state reporting over smoothing to `connecting`, keeping the diagnostic trace; reference documents counting attempts rather than printing on each |
| 2026-07-21 | Platform | Second bug fix (incomplete — see above): the NONET retry below re-issued the join *once per poll*. `cyw43_wifi_join` resets `wifi_join_state`, discarding the AUTH/LINK/KEYED flags a join accumulates from async events over 1–2 s, so a demon polling `wifi.status` every 20 ms restarted the association ~50×/s — faster than it could ever complete. The firmware then rejects `SET_SSID` (→ `WIFI_JOIN_STATE_FAIL`, user saw `failed`) or the link comes half-up and DHCP never answers (user saw `noaddress`), which is exactly the mix reported. The SDK's blocking loop is immune because `cyw43_arch_wait_for_work_until` paces it; a getter polled at 20 ms has no such brake — the "self-throttling" reasoning in the previous entry was simply wrong. Replaced with one rule: the status mapping is pure, and a re-join is issued only when the state has *stalled* (`WIFI_STALL_MS` 8 s, no more often than `WIFI_RETRY_INTERVAL_MS` 2 s, until the 30 s deadline). This also covers a stall the user hit at the REPL — associated but stuck at `noaddress`, which recovers on a re-join — so an unattended startup file now recovers by itself instead of hanging. `badpassword` stays terminal. Diagnosed from a user-supplied state trace, which is why `wifi.status` reports every driver state |
| 2026-07-21 | Platform | Diagnostics: `wifi.status` now maps one-to-one onto the cyw43 driver states (`off`/`connecting`/`noaddress`/`connected`/`notfound`/`badpassword`/`failed`) instead of collapsing every failure to `failed`, at the user's suggestion, so a stalled connection says which stage it stalled at. Reference documents a state-trace recipe (`when [not equal? wifi.status :last] [...]`) — that trace is what identified the bug above |
| 2026-07-21 | Platform | First bug fix (incomplete — see above), found by the user: connecting from a startup file failed most of the time while the identical commands worked from the REPL. `CYW43_LINK_NONET` (-2) is not a failure — it is the ordinary state while the scan has yet to find the AP, and the driver only leaves it if the join is *issued again* (the SDK's blocking connect does exactly this: `cyw43_arch.c`, "If there was no network, keep trying"). `wifi_status` treated any negative link status as terminal, latching `failed` and clearing `wifi_connect_pending` so it could never recover. Only the startup file was fast enough to see it: `when [wifi?]` armed there polls every 20 ms *during the load*, landing inside the blip, whereas at the REPL the seconds spent typing the `when` line let the join reach `LINK_UP` first. Fix mirrors the SDK — re-issue the join on NONET (self-throttling, since re-issuing returns the driver to `WIFI_JOIN_STATE_ACTIVE`) until a 30 s deadline matching the blocking path; `FAIL`/`BADAUTH` stay terminal. Needs the password kept alongside the SSID (+64 B, WiFi boards only). Not reproducible on the mock (no cyw43 join state machine), so this one rests on hardware validation |
| 2026-07-19 | P8 | `logo/games/galaxian` given the same PSG retrofit for consistency: same four centred voice-pairs (`[0 4]` two-note convoy hum, `[1 5]` sawtooth laser, `[2 6]` **sustained** square dive shriek, `[3 7]` white-noise explosions), timbres in `setup.sound` from `init.game`. The signature `dive.shriek` glissando now overlaps notes on a held voice under the hum/laser (the old `toot 15` couldn't sustain); added a laser on `fire`, noise splats on `kill.alien`/`diver.shot`, a low boom on `handle.death`, `stopsound` on game over. `test_galaxian` (loads the file, exercises `kill.alien`/`diver.shot` on the mock which records sound ops) still green — 61/61 ctest; host smoke-test clean; `galaxian-design.md` §8 rewritten, one stale `toot` test comment fixed. Same hardware A/B listen pending |
| 2026-07-28 | Roadmap | Split defect tracking out into [`bugs.md`](bugs.md): this roadmap is now features-only (past, present, future). The five open defects in the refinements table moved to `bugs.md` as B1–B5 (multi-line `(…)` in proc bodies, single-line `to … end`, demons firing during `load`, `parse_list` dropping unknown tokens, `name_buf[64]` aliasing), plus the 1 px `penreverse` limitation as B6; the refinements table keeps only the two performance items. Past fixes recorded from this log and git history. Renamed `improvements-roadmap.md` → `roadmap.md` (all references updated) |
| 2026-07-29 | Language: medium | Added `.reset` (todo, user request): a single command that clears the program you just ran so the next one loads onto a clean slate — unburied procedures and names, demons (later processes), graphics, sound, HTTP listener, open files, then `recycle` — where today it takes a remembered `cleardemons`/`erall`/`cs`/`stopsound`/`recycle` sequence that still leaves state behind. Deliberately survives: buried procedures (`erall` already skips them), the filesystem and prefix, the WiFi link and hostname, the `rerandom` seed; the startup file is not re-run |
| 2026-07-29 | P9 | Added tile maps + smooth scrolling (todo, design first, user request): acceleration for scrolling tile games — a bank of tiles drawn with the pen and snapped off the canvas, a map larger than the screen at one byte per cell, and a view into it at a pixel offset. Rendering falls out of the existing scanline compositor (the row source becomes the map instead of `gfx_buffer`, so an offset change costs one integer write and the picture is never stored); the real questions are the per-frame full-screen blit cost and where the map lives on a pico2 at 95.6 % SRAM. This is the revision `checkpoint-run-design.md` §4.2 demanded before any tilemap primitive, so M0 is measurement — the scroll builder's frame time beside both shipped games' redraw costs |
| 2026-07-29 | P9 | Design drafted: [`tilemap-scrolling-design.md`](tilemap-scrolling-design.md). Key decisions (three scoping calls made with the user): **all boards** with tiered capacity (`TILE_BANK_SIZE`/`TILE_MAP_SIZE` 4 KB each SRAM, 64/256 KB PSRAM; lazy `mem_region_alloc`-else-heap like the HTTP buffer — zero static SRAM), not PSRAM-gated; Turtle Trails revamp is **render-only** (same board/gameplay; C map replaces its ~1,050-cell Logo list, board baked via `stampmap` instead of pen-carved); both revamps **replace the shipped games in place**. Design deviations from this sketch: per-bank tile size (8 or 16, `newtiles`) since the two flagship games need different sizes; a **viewport** (`(showmap x y w h)`) so a HUD stays canvas beside the scroll view (Checkpoint Run's 256×320 road view drops present cost 20 %); a bake path (`stampmap`/`stamptile`) for non-scrolling tile boards; sampling always wraps (bounded worlds clamp their own `setscroll`). Storage + row sampler live in `core/tilemap.c` (natively testable); one new console op `map_changed`. Feasibility framing: the cost is the **wire, not the CPU** — full-screen present ≈ 22 ms at 75 MHz SPI, so 25 fps is the cadence and M0's headline number is measurable today with manual refresh + `ticks`; interrupts stay enabled during blits so P8 audio survives. Checkpoint Run retrofit deletes sector paging (~400-stamp rebuilds → none; worst frame = ordinary frame). M0 gate: present + game frame body ≤ 40 ms with ≥20 % headroom on a Pico 2, else the design's §15 levers |
| 2026-07-22 | Navigation | Done: easier directory navigation. Split the old one-per-line `catalog` into two commands: `cat` is a terse `ls`-style multi-column listing (alphabetical, dirs get a trailing `/`, column-major packing against a new `CATALOG_DISPLAY_WIDTH` 40 in `core/limits.h` = the PicoCalc's `SCREEN_COLUMNS`; over-wide names fall back to one per line), and `catalog` becomes the `ls -l` long form — one per line with a right-aligned 7-char size column (blank for directories, and when a file size can't be read). Added `sp` as an alias for `setprefix`. Shared collect/sort/resolve helpers; per-entry size via `io->storage->ops->file_size` on the joined `dir`/name path (buffer sized to avoid a truncated ancestor stat). Reference gains a `## cat` section, rewrites `## catalog`, aliases `setprefix (sp)`, and the startup `ls` example now calls `cat`; 6 new tests (`test_primitives_files_directory` covers cat columns, catalog size + `<DIR>`, and the `sp` alias). 61/61 ctest green, anchors resolve, pico2 links (RAM 95.6%). Verified on the host REPL: `cat`/`catalog` render correct columns and byte sizes |
| 2026-08-01 | P9 | M0 harness built (design §3.2), numbers still to be taken on a Pico 2. Three throwaway scripts on today's firmware, no tile code: `logo/tests/p9m0` measures the present wall time by **difference** — a present only sends dirty tiles, so each pass repaints first, the repaint is timed again alone and subtracted, and twenty passes are averaged because `ticks` is whole milliseconds against ~22 ms. It dirties an exact band (the leftmost *n* of the 20 tile columns, full height), so Checkpoint Run's 256×320 road view is *measured* rather than extrapolated, and the per-column slope plus the fixed per-present cost fall out of the band differences; it runs in `window` boundary mode because under the default `wrap` a 16-px pen's round cap spills past the left edge onto the right one and dirties the whole tile row, which would have made every band read as a full screen. `p9m0` also does the audio check, presenting flat out for exactly as long as the voice has something to say (`until [not playing?]`) so `play` never blocks on a full queue and stops the very presenting under test. `p9m0.checkrun` and `p9m0.trails` set a round up from cold and time `draw.sector`/`draw.board` and ten `play.frame`s in **manual** refresh, so the `sync` ending a frame presents and returns instead of waiting for the 25 fps boundary — the frame figures include today's small dirty-rect present and are therefore an upper bound on the body. Trails gained `time.board`/`time.frame` mirroring Checkpoint Run's existing pair; both games' tests execute the new procedures end to end, since a script that fails half way through wastes a hardware session. 66/66 ctest green |
| 2026-08-01 | P9 | M0 items 1 and 4 measured on a Pico 2 (design §3.3): full-screen present **25.6 ms**, road view 256×320 **21.1 ms**, per tile column 1.259 ms, fixed cost per present 0.41 ms; audio clean through twelve seconds of flat-out presenting, as predicted. **A present costs 17–21 % more than §3's wire math** — the band figures are internally consistent, so the effective rate is 4.0 Mpx/s, not the 4.69 Mpx/s the raw 75 MHz / 16 bpp figure gives. The gap is what §3 treated as free or overlapped: `compose_row`'s canvas `memcpy` and palette expansion per row, and the twenty *separate* `lcd_blit_begin`/`end` windows a full-height present costs, since `dirty_tiles` keeps one span per 16-px tile row. The near-zero intercept confirms the cost is per pixel, so §15's narrow-the-viewport lever still buys back its area share exactly. Consequence: at the gate's 32 ms the Logo body must fit in **6.4 ms** full-screen or **10.9 ms** in the road view, against the ~18/~22 ms §3 advertised — the road-view layout is now most of the remaining headroom rather than a 20 % nicety. Verdict still pending on the two game frame bodies (items 2 and 3) |
| 2026-08-01 | P9 | M0 items 2 and 3 measured, and **the gate fails** (design §3.3): Turtle Trails frame body **87.3 ms**, Checkpoint Run **258.6 ms**, against a gate allowing 32 ms for present *and* body. The design's premise is inverted — the cost is the CPU, not the wire. A present is 21–26 ms; the Logo frame body is three to ten times that, so setting the present to zero still misses the gate by 2.7× and 8.1×, and every §15 lever narrows only the present. **Neither game had ever been timed on hardware**: `checkpoint-run-design.md` §4.2 calls the rebuild "the one number in the design that is not yet measured" and §13 lists profiling as outstanding; `turtle-trails-design.md` §11 likewise. Both ship at `(setrefresh "sync 25)` and actually run at ~9 fps and ~4 fps — `sync` does not wait when a frame overruns, so they degrade quietly instead of failing, which is why it went unnoticed. Before-numbers: `draw.sector` **1,346 ms** against its 120 ms budget (11× over; §4.2's three levers were sized for a 120 ms problem), `draw.board` **5,916 ms** — a six-second pause at every level start. The finding splits P9 in two: the **scrolling** half rests on a per-frame budget that does not exist, while the **bake** half (`stampmap`/`stamptile`) replaces the two largest stalls measured anywhere in the project with a C loop, needs no frame budget, and is *more* justified by the measurement. Interpreter throughput now looks like the prerequisite item. One decomposition run still wanted (design §3.5): `p9.frame` in both games splits a frame into simulation / drawing / present with min-max and a sector-crossing count, since one crossing carries a 1,346 ms rebuild into a single frame and the 258.6 ms mean may hide it |
| 2026-08-01 | P9 | Root cause of the M0 failure, found on the host rather than by another hardware run (design §3.5): **the interpreter is the bottleneck, not the display**. The host build runs the same core against the mock device, where drawing is a recorded command instead of a rasterised one, so host timings isolate interpretation from plotting — and the host:Pico ratio barely moves between a frame that draws almost nothing (91×) and a board build that draws everything (107×). Rasterisation is a minor term. Also settled: Checkpoint Run's 258.6 ms is an **ordinary** frame, not a hidden sector rebuild — 400 host frames produced zero crossings, since an unattended car stops at the first wall. Within a Trails frame the cost is simulation the tile system never touches (`step.bugs` 59 %, `place.all` 28 %). Sampling a pure `repeat [make "x (:x + 1)]` loop: **34 % re-lexing/classifying words on every evaluation** (`classify_word` is the largest single leaf), **14 % primitive lookup by `strncasecmp`** against the registry, 20 % cons-cell walk and index→pointer indirection, 8 % `memmove`/`memset`, 24 % actual evaluation. Words are interned atoms, so word class, parsed numeric value and primitive index are all pure functions of the atom and could be computed once at intern time — roughly **half of runtime is spent rediscovering facts that do not change**, which is a memoisation problem rather than an interpreter rewrite. Proposed as a new roadmap item ahead of P9's scrolling half; not opened unilaterally — scoping is the user's call |
| 2026-08-01 | P10 | M0 done: benchmark harness and host baseline. `tests/test_bench_throughput.c` runs in ctest with five scenarios — the profile's pure `repeat` loop, a user-procedure call at workspace sizes 1/64/128, both games' `play.frame` on the mock, and the hardware script executed end to end so it cannot waste a Pico session — printing `BENCH` lines for the record and asserting only **relative** ratios (each scenario against an in-process calibration loop, plus the 128:1 workspace-scan ratio), so the guard does not flap on a loaded machine. `logo/tests/p10m0` takes the same scenarios on a Pico 2; frame bodies stay with p9m0's scripts. Host baseline (design §6.1): repeat loop 1.07 µs/iter; proc call 0.43 µs with 1 defined → 2.09 µs with the table full — a **4.8× spread**, steeper than the ad-hoc 0.57 → 0.96 µs the design first recorded, whose 200-procedure workspace no stock build can hold (`MAX_PROCEDURES` is 128); Trails `play.frame` 0.84 ms, Checkpoint Run 2.73 ms. The corrected scan number strengthens M2's case: both games sit near a hundred procedures, so real calls pay most of the worst case |
| 2026-08-01 | P9 | Post-M0 split ratified across the docs and sized against P10's design. Milestones resequenced (design §13) so the unblocked bake half runs first: M1 bank → M2 map storage + `stampmap`/`stamptile` → M3 Turtle Trails render revamp (deletes the 5,916 ms `draw.board` and the ~1,050-cell Logo map; needs no frame budget) — then M4 live view/scrolling **gated on P10's M3 re-measure**, and M5 the Checkpoint Run camera. How P10 helps, honestly (its §7, recorded in design §3.4): M1+M2's ~1.9× upper bound takes a Trails frame 87.3 → ~46 ms against 40 — close, with the C map's `tile.at` removal (inside `step.bugs`, 59 % of the frame) converging on the rest from the data side; Checkpoint Run needs P10 *and* the camera retrofit *and* game-side work. Design §3.5's 14 % row relabelled name resolution, noting P10's review found part of it is variable lookup, which cannot be atom-cached |
| 2026-08-01 | P10 | Added interpreter throughput (todo, design first), opened by P9's failed M0 gate: [`interpreter-throughput-design.md`](interpreter-throughput-design.md). The display was never the bottleneck — both shipped games miss their frame budgets and always have (Turtle Trails 87.3 ms, Checkpoint Run 258.6 ms against 40 ms; ~9 fps and ~4 fps), and neither had ever been timed on hardware. Diagnosis from a sampled profile of a pure `repeat [make "x (:x + 1)]` loop: **34 %** re-lexing and classifying words on every evaluation (`classify_word` is the interpreter's largest single leaf), **14 %** resolving names by `strncasecmp` on every call (`primitive_find_n` binary-searches ~390 primitives, then `find_procedure_index_n` *linearly scans* the ~100-procedure table; measured, a user-procedure call goes 0.57 µs → 0.96 µs as the workspace grows from 1 to 200 procedures), 20 % cons-cell walk and index→pointer indirection, 8 % `memmove`/`memset`. Design: words are interned immutable atoms, so class, parsed numeric value and resolved binding are pure functions of the atom — derive once, store in the atom header (which already carries `ATOM_LINK_FREE`/`ATOM_LINK_MARK` flag bits), invalidate bindings via a generation counter bumped on procedure-table mutation. Costs **no new `bss`**: atoms live inside the existing 128 KB block, so a wider header trades atoms against nodes rather than enlarging a static array (~+1 byte/atom for M1). The single context-dependent case — a leading `-`, unary or binary by preceding token — keeps today's logic behind a distinguished cached class. Milestones: M0 benchmark + baseline (nothing lands without a before/after, and it becomes the CI regression guard), M1 word class, M2 name binding, M3 re-measure and decide, M4 representation only if needed. Honest expectation recorded: M1+M2 target 48 % → upper bound ~1.9×, which likely gets Trails close to 40 ms but does **not** rescue Checkpoint Run's 6.5× shortfall alone. Rejected: bytecode/AST compilation (the real fix, far too large for the evidence — touches GC, `format.c` procedure printing, the editor, and "a program is a list"; revisit only if M3 falls short), case-insensitive interning (changes what `print "Hello` prints), sorting the procedure table (still string compares, and does nothing for the bigger half), overclocking, and lowering the games to 15 fps |
| 2026-08-01 | P10 | M1 done: **cached word class**, measured on a Pico 2. Word class is now derived once and memoised on the interned atom — the entry grows a `[memo:1]` byte (`ALIGN4(len + 5)`), `mem_atom` clears it on both allocation paths so collected storage cannot resurrect a stale class, and the leading-`;` comment peek folds into the same lookup. The one context-dependent shape (a leading `-`) keeps today's logic behind a distinguished class; `-5` is cached as a number, since the grammar accepts the sign in every position. **Hardware result** (design §6.1): repeat loop 92.4 → **65.9 µs/iter (1.40×)**, Turtle Trails `play.frame` 87.3 → **73.2 ms (1.19×)** and `draw.board` 5,916 → 5,054 ms, Checkpoint Run `play.frame` 258.6 → **232.7 ms (1.11×)** and `draw.sector` 1,346 → 1,173 ms. The full-workspace procedure call is unchanged at 127.6 → 128.3 µs — M1 does not touch name resolution, so that row is a control, and reproducing M0 within 0.5 % says the baseline and the method are both sound. §2.2's **34 % held**: 28.7 % of the loop disappeared against a 34 % ceiling. **The lesson is about where the number came from** (design §6.2). It held only on the board: a host leaf profile puts classification at a few per cent, with cons-cell indirection 16.6 %, `memmove`/`memset` 9.8 %, name resolution 7.6 %, re-interning the quoted word `\"x` 5.9 %, `is_delimiter_token` 4.2 % and number parsing 2.2 %. The first cut cached perfectly (3,079,993 hits, 7 misses) and bought 1.5 %, because the read was a separate out-of-line call re-walking the entry on top of `mem_word_ptr`/`mem_word_len`; collapsing all three into one `mem_word_view` is what produced the win. So: host shares cannot size a board milestone in either direction — this design's central number was right about the hardware and would have been abandoned on host evidence alone — and **a memo must be cheaper than what it replaces**. Numeric-value caching **dropped** from M1: 2.2 % does not justify a variable-width entry, a new GC-visible flag bit and a wider `Token`. Cost: 560 / 740 atom bytes after loading each game, 0.6–0.8 % of free nodes, all three presets link, `pico2` RAM unchanged at 95.62 % (no new `bss`, as designed). Six tests added to `test_token_source.c` (29-shape cold/warm corpus, the `-` atoms in both contexts, `-5` context-free, comments over two passes, atom-offset reuse — mutation-checked); 67/67 ctest green with no behaviour test edited. One test *helper* fixed: `exhaust_atom_table` stopped at its first refusal, leaving a layout-dependent remainder, so a wider entry left room for one more atom and silently stopped exercising the NULL-deref path its test guards. **Neither game is home** — Trails needs another 1.83×, Checkpoint Run 5.82× — and M2 (a smaller share, part of it un-cacheable variable lookup) will not close either. For Trails the decisive lever is P9's C map: `tile.at` in `step.bugs` is 43 of the 73.2 ms, and removing it lands the frame near 30 ms. This also argues against M4, which attacks the same milliseconds P9 removes outright |
| 2026-08-01 | P10 | M2 done: **cached name binding**, and the milestone that gave memory back. A word's primitive/procedure binding is resolved once and memoised in the same 16-bit atom word as M1's class (`core/atom_memo.h`: 5 bits class, 2 bits kind, 9 bits table index), so the entry grew only `ALIGN4(len + 5)` → `ALIGN4(len + 6)`. `Token` gained the atom it came from, which is what lets a resolution site reach the memo — and which also deleted the `mem_atom` re-intern that every primitive call was doing for its error-message name, and turned the inline `strncasecmp(t.start, "output", 6)` into `primitive_is_output`, an identity check. **Host result**: workspace-scan spread **4.97× → 1.00×** — a call no longer cares how many procedures are defined — full-table call 2.03 → **0.32 µs**, repeat loop 0.86 → 0.80 µs, Trails `play.frame` 0.75 → **0.64 ms**, Checkpoint Run 2.40 → **2.06 ms**. Not yet run on hardware; §6.2's lesson is that the host does not predict the board. **Three departures from the design.** No generation counter (§4.3): every mutator in `procedures.c` sweeps the atom region instead, which is simpler, covers `define`/`copydef`/`erase`/`erall`/`load` by construction, costs only load-time work, and freed the bits that let the binding sit beside the class. `copydef` sweeps too, since an alias can turn a name that already resolved to "neither" into a primitive mid-run. Numeric caching stays dropped. And **`Token` had to shrink to grow**: it is embedded twice per `TokenSource`, once per `EvalOp`, in a 768-deep static op stack, so four bytes on it cost **6,144 B of `bss`** on a board at 95.6 %. Narrowing `type`/`length` to absorb it was measured and rejected — 16 % on the loop, 8 % on a frame, leaving the loop slower than M1. What paid instead was deleting `NodeIterator`'s `has_peeked`/`peeked_token`, a lookahead buffer nothing ever set to true: **`pico2` RAM 95.62 % → 93.86 %, 9,208 bytes returned**, free nodes down only 194/258 per game, all three presets link. 6 tests added to `test_primitives_procedures.c`, mostly running the call from inside a list because that is the only path the cache serves: a slot freed by `erase` and refilled by the next definition, a name defined after resolving to "neither", `erall`, a `copydef` alias, the plain repeated call, and redefinition through both the lexer and the cached path. 67/67 ctest green |
| 2026-08-01 | P10 | M2 measured on hardware, and **M3 decided: P10 stops, M4 declined**. M2 did what it was designed to do and nothing the games could collect. On a Pico 2 a user-procedure call stopped scaling with workspace size — full table **128.3 → 24.0 µs (5.35×)**, small workspace 32.6 → 24.4, spread 3.94× → **0.98×** — which removes a cliff every growing Logo program was walking towards. But Turtle Trails' `play.frame` went 73.2 → **73.4 ms** and Checkpoint Run's 232.7 → **232.6 ms**, both flat against a host that predicted 1.17× for each, and the profiled loop **regressed 1.64×** (65.9 → 108.1 µs/iter), with `draw.board` and `draw.sector` slipping 3–6 %. **So all of P10's game-visible gain is M1's**: Trails 87.3 → 73.4 ms (1.19×), Checkpoint Run 258.6 → 232.6 ms (1.11×), against a 40 ms target that is **not met**. M3 therefore declines M4: it targets the cons-cell walk, which is the same cost P9's C map removes outright from `tile.at` (59 % of a Trails frame, 43 of the 73.4 ms — a frame near 30 ms without it), for far more risk and a GC-touching change. **The loop regression is unexplained and is the one live lead.** M2 added no work to that loop — it resolves one name per iteration, and a nine-step binary search plus a re-intern became a single memo read — so nothing algorithmic accounts for 42 µs an iteration. The host does not reproduce it: host:board on that loop went 77× at M1 to **135× at M2**, a 1.76× board-only penalty. That shape points at instruction fetch (the RP2350 runs from external flash through the XIP cache, and M2 moved and grew the hot path) rather than at the algorithm. Concrete lead on the record: `pico2` reports **XIP_RAM 0 B of 16 KB, 0 % used**, and the SDK can place chosen functions there with `__not_in_flash_func` — the token and resolution path is the obvious candidate, and it would lift M1's gains too. Hypothesis only; measuring it needs a board. Whether M2 stays is finely balanced and recorded in design §6.4: for it, the scan cliff is gone, 9 KB of SRAM came back, and the games are neutral rather than worse; against it, token-heavy code that calls few procedures is 1.64× slower on the board and the games gained nothing. Recommendation: keep, and chase the XIP_RAM lead |
| 2026-08-02 | P9 | M1+M2 done: **the bank, the map, and the bake path** (design §13.1), taken as one milestone because a tile bank nothing can draw is not reviewable on its own. Seven primitives — `newtiles size` (8 or 16, clears the bank), `snaptile slot` (capture the tile-sized canvas region centred on the turtle, **verbatim**: unlike `snapsh` no pixel becomes transparent, because tiles are background), `newmap cols rows`, `settile col row slot` / `tile col row` (1-based, like `item`), `stampmap` and `stamptile col row` — in `core/primitives_tilemap.c` over a new `core/tilemap.c` that owns the pools, the view state and the row sampler and unit-tests with no device at all. Capacity tiers exactly as designed and costs **0 B of static pool**: on the first `newtiles`/`newmap`, `mem_region_alloc` (PSRAM) else one process-lifetime `malloc` of the SRAM tier — 4 KB bank + 4 KB map on a Pico 2 (63 tiles at 8×8, 15 at 16×16, a 64×64 world), 64 KB + 256 KB on a Plus 2 W (255 tiles either size, 512×512). **Six departures from the design**, all recorded in §13.1: two console ops rather than `map_changed` (`canvas_snap` verbatim capture and `canvas_write_row`, the latter a new `screen_gfx_write_row` that memcpys into `gfx_buffer` and marks one dirty rect per row; `screen_gfx_snap` gained an `opaque` flag rather than acquiring a near-duplicate); the sampler takes the background colour (cell 0 and empty slots are a `memset` core cannot colour by itself) and leaves viewport clipping to its caller, keeping it a pure function; `setscroll`/`showmap` stay in M4, so scroll and viewport exist as core state the sampler is defined over but a bake runs at (0,0) over the whole graphics area — which is what a non-scrolling board wants; "before `newtiles`/`newmap`" needs no new message, since a bank with no size has no slots and a map with no dimensions has no cells, so `ERR_DOESNT_LIKE_INPUT` names the offending input, while input-less `stampmap` is a no-op (every single-`%s` template renders the *primitive's* name, so "map not found" would have printed "stampmap not found"); and `stamptile` re-bakes **every** on-screen copy of a cell, because sampling wraps and a world smaller than the viewport shows a cell more than once. Cost: `pico2` RAM 93.86 % → **93.94 %**, +~380 B `bss` — a 32-byte slot-filled bitmap and the 320-byte row buffer `stampmap` bakes through (`TILEMAP_ROW_MAX`); all three presets link, the host REPL runs the storage half with the graphics primitives as silent no-ops. Tests: `test_tilemap.c` (20, native) covers tier caps, slot arithmetic and bounds, and the sampler against hand-built rows — offsets, wrap seams in x, y and at the corner, partial spans, cell 0, empty slots, both tile sizes, viewport origin; `test_primitives_tilemap.c` (19, mock) covers the surface errors and the full loop of paint → `snaptile` → `settile` → `stampmap` → assert pixels, with each tile marked at two corners so a capture or bake that is flipped, transposed or off by a pixel cannot pass. Mutation-checked against three breakages (transposed tile indexing, no wrap, size-blind slot capacity); each fails both files. 69/69 ctest green. Hardware acceptance script `logo/tests/p9m2` written and dry-run through the host REPL (which caught a `;` inside a printed list swallowing the rest of a procedure): it probes the large tier's real capacity by the pairs either side of it (`snaptile` 255 vs 256, `newmap 512 512` vs 513), times `stampmap` against the 5,916 ms `draw.board`, and puts an asymmetric "L" on screen for the two things the mock cannot check — that the device's own capture path is not flipped or transposed (the mock *reimplements* the Logo-to-screen conversion rather than sharing it), and that `snapsh` still makes background transparent now that both captures share an `opaque` flag. The one risk it cannot cover on a Plus 2 W: that board takes the PSRAM tier, so the SRAM tier's 8 KB **heap** allocation on a Pico 2 at 93.94 % RAM is still unproven. **Next: M3**, the Turtle Trails render revamp — the map replaces `tile.at`'s cons walk inside `step.bugs` (43 of the frame's 73.4 ms) and `stampmap` replaces the 5,916 ms `draw.board` |
| 2026-08-02 | P9 | M1+M2 **accepted on hardware** (design §13.2), all four checks of `logo/tests/p9m2` passing on a Pico Plus 2 W. **The bake is 7.45 ms for Turtle Trails' 28×36 board, against the 5,916 ms `draw.board` it replaces — 794×**, which settles why M2 exists and means M3 can lay down a board at level start for nothing. The large tier is real (slot 255 accepted, 256 refused; a 512×512 map accepted, 513×512 refused), so §4's capacity table holds and the PSRAM path has now run at least once. The two checks no unit test can make also pass: the asymmetric "L" comes back upright and in the right cells through the device's own `turtle_canvas_snap` (the mock *reimplements* the Logo-to-screen conversion rather than sharing it, so a flipped or transposed capture would have passed every test), and `snapsh` still shows the drawing through a worn costume, so the new `opaque` flag did not leak into the costume capture. **But 7.45 ms is slower than it looks** and the number is on the record for M4: 224×288 is 64,512 px, so the bake runs at ~8.7 Mpx/s — 115 ns a pixel, only ~2× faster than the SPI wire M0 clocked at 4.0 Mpx/s, where a memcpy-bound loop on a 150 MHz core should be an order of magnitude quicker. Three suspects, in attack order: the sampler copies a *tile run* at a time (28 runs of 8 bytes a row, 8,064 `memcpy` calls for this board — the call is the cost at that size); `screen_gfx_write_row` marks a dirty rect per row (288 marks spanning 14 tile columns each, where a bake could mark its rectangle once); and none of it is RAM-resident — design §7 asks for `__not_in_flash_func` on the sampler and M1+M2 did **not** deliver it, because the macro is Pico-SDK-only while `core/tilemap.c` compiles for the host too, so it needs a shim. That is the same instruction-fetch story as P10's open XIP_RAM lead, on the same silicon. None of this blocks M3 (`stampmap` runs once per level start); it blocks *M4*, since the same rate extrapolated to a full screen is ~11.8 ms of CPU on top of a 25.6 ms present, which leaves a 40 ms frame nowhere to stand. Still unproven anywhere: the **SRAM tier's 8 KB heap allocation**, which a PSRAM board cannot exercise — it needs a Pico 2 with a game loaded, then `newtiles 8` and `newmap 28 36` |
| 2026-08-02 | P9 | M3 done: **Turtle Trails revamped in place** (design §13.3) — render-only, gameplay byte-identical, and the game's own storage of the board replaced. A map cell now holds a **bank slot**, which has to answer both "what does this look like" and "what is this", so the slots are ordered to make every rule one comparison: 0 off the board, 1 hedge, 2 nest floor *and* door, 3..18 open path (one variant per walkable-neighbour mask), 19..34 the same carrying a speck, 35..50 the same carrying a blossom — `walk?` is `> 2`, `nest.open?` is `> 1`, paintable is `>= 19`, and painting subtracts 16 or 32 to reach the plain variant of the same shape. Two pairs of §5.2 codes therefore share a picture (tunnel with empty corridor, nest floor with its door); nothing in the game ever told them apart. **The mask is the rounded corner**, so the 50 tiles are drawn with the pen the maze used to be carved with: a fat hedge dot, then a pen-8 stroke run into each walkable neighbour — a stroke that stops at the cell centre leaves a round cap, one that runs on leaves a square edge. `setup.tiles` draws them once a session and `draw.board` becomes `stampmap`. One pixel changes and it is not a bug: a pen-8 disc is *nine* pixels across, so the carved board had nine-pixel corridors eating a pixel off the hedge either side, and tiles make corridors eight and one-cell walls eight rather than six — visibly the same board, not pixel-identical, and no tile scheme could be, since the bleed makes a cell depend on its neighbours' neighbours. The map is **40 × 77**: a bake starts at the top left of the graphics area, so it must be the whole screen with the 28×36 board at cell offset (6, 2) — which also lets `tile.at` drop its four bounds tests, because a step off the board lands on margin and reads 0 — and rows 41–76 hold the finished board off the bottom of the 608-pixel world where the sampler never reaches, so `reset.board` copies them down at a level start (two primitives a cell against the twenty deriving one costs). Deleted: `decode.map`, `tile.code`, `count.paint`, `fill.board`, `carve.paths`, `sweep.row`/`sweep.col`, `mark.tile`, `draw.specks`, and the ~1,050-cell nested Logo map. **Numbers** (new `trails.board` line in `test_bench_throughput`, host, same machine): level build **56.7 → 3.3 ms**, 17×, of which the bake is 0.07 ms; `play.frame` unmoved at ~0.61 ms, as design §3.4 predicted — the tile system does not touch the simulation. Tests: map invariants (symmetry, connectivity, sealed nest, no dead ends, tunnels) now read the **encoded words** rather than the runtime map, since that is the source data; three new tests pin what the map became — the built map's slot against the letter *and* the neighbour mask for all 1,008 cells, the kept copy against the live board across a level change, a bank tile's strokes against every one of the 16 masks, the bake's 224×288 landing on a 320×320 screen with nothing outside it, and a one-cell `stamptile` repair hitting exactly its 8×8. 69/69 ctest green. **Open: the hardware run** — the Pico 2's 5,916 ms against the new level build, and whether the frame moves once `tile.at`'s 36+28 cons walk inside `step.bugs` is an index; `p9m0.trails` now prints `setup.level` beside `draw.board` for it |
| 2026-08-02 | P9 | M3 **hardware-accepted** (design §13.4), over two runs, and they answered two questions the native suite could not. **The maze rendered as blank space** — cause was not the tile system but **[B11](bugs.md)**: `dot` ignores the pen size on the PicoCalc (`turtle_dot` plotted a single pixel with `screen_gfx_set_point` where every other drawing path stamps the pen's filled disc through `screen_gfx_line`), so `make.tile`'s pensize-16 hedge patch was one pixel and every captured tile came back as background with a speck of hedge in it. The bake was doing exactly what it was told. The defect was **already shipping and unnoticed**: Trails' specks and blossoms have been 1-px dots rather than the 2-px speck and 6-px disc `draw.specks` asks for, and the blossom erase blacked out a single pixel; no native test could see it because the mock recorded a dot with no pen size. Fixed by drawing a dot as a zero-length line at the current pen size — identical rounding and clipping at pen size 1, so narrow dots are unchanged — plus `MockDot.pen_size` and `test_setpensize_records_on_a_dot`; the device code itself is still only verifiable on a board. **The board build is 303 ms against 5,916 ms — 19.5×**, with `draw.board` alone (bake + HUD) at 20 ms, and the factor matches the host exactly, which is what a purely interpreter-bound build should give. **But on the first run the frame did not improve: 79.2 ms mean** (min 64, max 115; simulation 53.5, drawing 24.4, present 1.4), against P10 M1's 73.4 ms — 8 % the wrong way, where the C map was supposed to attack `step.bugs` from the data side. Two reads of the dynamically scoped `:sl.dc`/`:sl.dr` inside `tile.at` were 3.5 % of a host frame and are now written out as literals (the tests pin the two spellings against each other), and that was the whole of it. **With both fixed the maze renders and M3 is accepted**: the frame is **73.6 ms** (min 68, max 79; simulation 48.6, drawing 24.8, present 0.25) against P10 M1's 73.4. So **the C map is frame-neutral** — not a regression, and not the win either half of this project predicted. Design §3.4 ("the C map attacks the remainder from the data side") and [P10 §7](interpreter-throughput-design.md) ("what closes the gap is P9, not P10", projecting a ~30 ms frame) are both **disproved**, and both by the same scope error: P9 M0 measured `step.bugs` at 59 % of a frame and both read that as `tile.at`'s cons walk, when `step.bugs` is dozens of statements and procedure calls of targeting arithmetic around a handful of lookups — and swapping that walk for a `tile` primitive is close to a wash regardless, since the primitive pays argument validation and a fresh number value where the walk paid pointer chasing. Two consequences worth carrying forward. **Turtle Trails' 40 ms target has no named lever left**: what is in the frame is ordinary interpreter overhead spread thin, the target of P10's declined M4 and its rejected bytecode body, both of which were rejected partly *because* P9's map was believed decisive — reopening either is a fresh decision. **Board caveat, found 2026-08-02:** these runs are on a **Pico Plus 2 W** while every baseline they are set against (5,916 ms, 87.3 ms, 73.4 ms) is a **Pico 2** — same RP2350 and clock, different flash part, and instruction fetch is precisely the open lead in both designs. The two M3 runs are comparable with each other, so the B11 and `tile.at` findings hold; the cross-board pairs do not establish what they were written to. Design §13.5 has the same-board re-run (the pre-M3 game is still in git; load it as `trailsold` and run `p9m0.trails` on the same board for a real *before*), and a **Pico 2** run is worth more than another Plus 2 W one, since its SRAM tier has still never allocated. One further correction while writing P10's profiler: **the whole 87.3 → 73.4 → 73.6 series never presented.** `p9m0.trails` runs from the prompt in text mode and `screen_gfx_blit_dirty` returns immediately there, so its present column has always been ~0 and its frame figure is the Logo body, not a whole frame — a real fullscreen frame is 73.6 ms *plus* the dirty sprite tiles. The three figures stay comparable with each other, and the bake numbers are untouched (`sense_metrics` reports 320×320 in every mode), but `logo/tests/p10prof` switches to fullscreen itself so its `sync` slot measures a present that actually happens |
| 2026-08-02 | P10 | M5 done: **the frame profiled on hardware** (design §11.1), and the question it was built to answer came back closed. `logo/tests/p10prof` splits `play.frame` into thirteen slots on a board, reporting each in *operations* as well as milliseconds so that "no hot spot exists" is a result the profile can return — and it did. On a Plus 2 W over 200 frames: `step.bugs` 30.7 ms, `place.all` 24.2, `step.player` 7.1, `dress.bugs` 6.6, `collisions` 4.4, everything else under 2, **FRAME 81.0 ms / 791 operations**. The two large slots are the two that iterate five and four actors through ~30 statements each; every slot is proportional to its statement count. The confirmation is the total: a host frame is 0.615 ms against a 781 ns benchmark iteration, so **787 operations predicted against 791 measured** — identical composition on both machines, the board uniformly ~131× slower per operation, and nothing board-specific hiding in it. **The lever is the calibration pair.** A `make "x (:x + 1)` costs **102.5 µs against a procedure call's 24 µs — 4.3×**, where the host's own two numbers give 2.5×; both ratios are within one machine, so unlike the frame figures the comparison is sound. Calls scale host→board at 75×, a `make` with arithmetic at 129×. M2 already made calls cheap, so what is left is the statement, and the hot slots are almost nothing but `make` statements (`place.all` is fourteen per actor, five times a frame). The piece inside a `make` that is not inside a call is **variable resolution** — the one name lookup §3.2/§7 left uncached as "dynamically scoped", which is a reason it cannot use M2's atom memo, not a reason it must stay slow. Five runs reproduce every slot within 2 % (FRAME 80.99-81.15 ms). They priced the pieces, net of a 4 µs bare loop: **user procedure call 17.5 µs, variable read 37, variable write 48, `(1 + 1)` 67.5, the whole statement 97.5**. So **a variable write costs 2.7× a procedure call** — M2 spent itself on calls and got them cheap; variable access, the one lookup §3.2/§7 left uncached as dynamically scoped, is now the dearest elementary operation and the hot slots are made of it. **A game-side saving fell out on the way**: a call's last argument absorbs the whole expression, so the outermost parens of `make "v (…)`, `output (…)` and `.setitem i :l (…)` are redundant — and cost 18 % of such a statement on the host. Sixty came out of `logo/games/trails` (expression-form only; `(name …)` may be a varargs call whose bare arity differs), measured at **2.8 % off `place.all` and 2.6 % off `step.bugs`**, full suite green including the bit-exact simulation and the soak. The game's header now says not to add them by habit. **Number literals were suspected and cleared**: `ignore (1 + 1)` 73.5 µs against `ignore (:x + :x)` 90.5 — the variable form is dearer, so §4.1's dropped numeric cache stays dropped. That pair also prices an operand twice over, 8.5 µs for a variable reference over a literal from two independent measurements agreeing to a tenth of a microsecond. I read the remaining ~32 µs as the grouping paren plus the infix operator; **the host disproved that** (new `test_bench_expr_shapes`, same interpreter, net of a 69 ns bare loop): `ignore (1 + 1)` 606 ns is 55 ns *cheaper* than `ignore sum 1 1` 587+, so infix beats a prefix primitive, as it should. The only removable overhead is the **grouping paren**, ~11 % of an expression and **~15-20 % of a `make "v (expr)`** — a game-side saving available today, since the outermost parens are redundant when the last argument absorbs the expression anyway. **The real lead is a ratio**: board:host is ~65× for a bare loop and ~68× for a procedure call, but **135× for an arithmetic statement** — the board is twice as bad at arithmetic statements as at everything else, and the shape data says it is not the operator, the operands or the literals. Prime suspect is instruction fetch on a code path entered once per statement, the same story as §2.2's 34 %-on-board word classification and P9 §13.2. The presets' idle `XIP_RAM: 0 B of 16 KB` is **not** the way in — that region is the XIP cache reconfigured as RAM, offered only for binaries not executing from flash, and Pico Logo does. **The experiment was built and it worked (design §11.2-§11.3)**: new `core/hot.h` gives `__not_in_flash_func` a host-safe spelling (`LOGO_HOT`, the shim P9 §13.2 also wanted for the tilemap sampler), carried by `eval_primary`, `token_source_next`, `eval_expr_bp` and `step_expr_eval` — 6.4 KB of the interpreter's hottest code. Off by default; `-DLOGO_HOT_IN_RAM=ON` costs **+6,144 B on `pico2`, 93.94 → 95.11 %** (25.6 KB still free), all three presets link, symbols verified moving from flash to SRAM and back. **Measured 2026-08-04 on a Plus 2 W: the frame went 81.0 → 65.5 ms, 1.24×, and the 212× paren-call path collapsed to 38× — below the 60× a bare `repeat` iteration costs.** `sync` was unmoved at 1.74 ms, the control that rules out drift. Instruction fetch is no longer a hypothesis. Two further findings: the code *left* in flash got worse (a procedure call 17 → 22 µs, ratio 67 → 87×) because moving 6 KB reshuffles the layout, so the boundary is arbitrary and whatever stays behind pays; and a second tier followed — the call path, `var_get`/`var_set` (the dearest elementary operation) and the `mem_car`/`mem_cdr` accessors §3.3 named at 20 % — for 3.6 KB more, built and unmeasured. RAM: `pico2` 93.94 → 95.11 % (tier 1) → **95.90 %** (both), 21 KB free — which turned out to be a 108 KB op stack left at 768 from the single-board era, not the board, so the option is on for `pico2w` and `pico+2w` and off for `pico2` only for want of hardware to boot it on. §1's 40 ms now needs 1.64× more, and the question has changed shape: how much of the interpreter fits in RAM, rather than which rejected rewrite to reopen. Both runs reproduce every slot within 2 %. 40 ms from 81 is 2.03×. Two smaller results: **`sync` is 1.64 ms**, the first honest in-frame present this project has measured, so the display is not the problem for a game that dirties only its sprites; and the first run lost the tail of its own report to a `;` inside a bracketed list — the hazard `logo/tests/p9m2` records — which now fails at load in `test_trails.c`, mutation-checked, for every Logo file it reads |
| 2026-08-04 | P10 | **M5 closed: 81.0 → 47.0 ms, 1.72×, for 13.6 KB of SRAM** (design §11.6). Tier 4 — `frame.c`'s `frame_push`/`frame_reuse`/`frame_add_local`/`frame_at_depth` and the binding lookups that `var_get`/`var_set` were reaching back into flash for — returned **48.095 → 46.985 ms**, 1.024×, and recovered part of tier 3's call regression (24.0 → **21.0 µs**) with `set` and `op` 5 % better and **no new regression anywhere**, the first tier of which that is true. `sync` moved for the first time, 1.74–1.775 → 1.635 ms; at 0.125 ms that is an order of magnitude under tier 4's 1.11 ms so the result stands, but a 2 % tier is now near the floor. **The tiering stops here, on the curve rather than the budget**: 1.24×, 1.23×, 1.105×, 1.024× — returns halve every tier, and taking the rest of the evaluator costs 10 KB more for what the curve puts at a few percent, on boards where SRAM is what panics `repl_init`. **§1's 40 ms is 1.17× away**, from 2.03× when M5 opened and 87.3 ms when P10 opened, and it is no longer an interpreter number: `step.bugs` (17.5 ms) and `place.all` (13.1) are **65 % of the frame**, are almost nothing but `make` statements, and a statement costs 48 µs — closing 7 ms means removing ~145 statements from those two procedures or finding a cheaper shape for them. Enabled on the `pico2w` and `pico+2w` presets; 69/69 green |
| 2026-08-04 | P10 | **M5's answer: the interpreter was running from flash.** Five reproductions of the frame profile (§11.1) established there is no hot spot — 800 operations, every slot proportional to its statement count — but the board:host ratios were not uniform: 60× for a bare `repeat` iteration and 67× for a procedure call against 132× for an arithmetic statement and **212× for Logo's parenthesised-call path**. That spread is not "the board is slower"; it is one code path costing far more there than its share of the work. Cause: the RP2350 executes the interpreter from flash through a 16 KB XIP cache, and `eval_primary` — the largest function in the build, holding both paren branches — is entered once per statement against the tight loops calls and `repeat` run in. New `core/hot.h` gives `__not_in_flash_func` a host-safe spelling (`LOGO_HOT`; core/ compiles for both, the shim P9 §13.2 also wanted for the tilemap sampler), behind `-DLOGO_HOT_IN_RAM=ON`. Six KB — `eval_primary`, `token_source_next`, `eval_expr_bp`, `step_expr_eval` — **took the frame from 81.0 to 65.5 ms, 1.24×, and the 212× path to 38×**, below the 60× of a bare loop. `sync` unmoved at 1.74 ms is the control that rules out drift. Two more findings. The code *left behind* got worse — a procedure call 17 → 22 µs, ratio 67 → 87× — because moving 6 KB reshuffles the flash layout, so the boundary is arbitrary and whatever stays on the flash side pays; a second tier therefore followed the same evidence (`step_proc_call`, `step_prim_call`, `eval_trampoline`, `eval_instruction`, `eval_call_primitive`, `var_get`/`var_set`, `mem_car`/`mem_cdr`/`mem_word_view`) for 3.6 KB, built and unmeasured. And number literals were cleared along the way: `ignore (1 + 1)` is cheaper than `ignore (:x + :x)`, so §4.1's dropped numeric cache stays dropped. RAM on `pico2`: 93.94 → 95.11 → **95.90 %**, 21 KB free; all fourteen symbols verified moving flash↔SRAM with the option, host and test builds untouched, 69/69 green. **The default was then settled without the Pico 2 boot test it seemed to need**, because that 21 KB turned out not to be the board: `pico2` carries `LOGO_OP_STACK_DEPTH: 768` from the single-board era while the other two presets were given 256 when multi-board landed, and `EvalOp` is 144 bytes — so the stack alone is **108 KB**, five times the headroom it appears short of, and the same firmware links at **81.83 %, 93 KB free**, at 256. It is also why `pico2` reads as tighter than `pico2w`, which carries a WiFi stack and still links smaller. So the option is **on for `pico2w` (89.36 %, 55 KB free) and `pico+2w` (91.05 %, 46 KB)** — the measured board, and one strictly less pressured than it — and off for `pico2`, for want of hardware to boot rather than evidence against. **Tier 2 then measured at 53.165 ms** (design §11.4) — another 1.23×, **81.0 → 53.2 cumulative, 1.52×**, `sync` 1.775 ms across all three runs as the control. The calibration confirms it function by function: the moved set is exactly what improved — variable read **−43 %**, variable write **−34 %**, procedure call **−30 %** (17.0 flash → 22.0 tier 1 → **15.5**, so tier 1's regression is undone) — while the two lines with no variable in them fell only 11–14 %. **Variable access, §11.1's "dearest elementary operation", is no longer**: a read is now cheaper than a literal expression. The one line that got *worse* is the bare loop, +67 %, not moved — the same signature `step_proc_call` showed after tier 1, so **tier 3 is the loop and run-list path** (`step_run_list`, `step_repeat`, `step_forever`, `eval_run_list`/`_expr`, `eval_push_proc_call`, `op_stack_push`/`pop`): 1,664 B that **cost no reported RAM at all**, fitting inside alignment padding tiers 1–2 already paid for (`pico2w` 89.36 % either way). Built, unmeasured. The frame is also now improving more slowly than the calibration statement (×0.811 against ×0.734), which says what is left is loop overhead and primitive bodies rather than statement evaluation. **Tier 3 then measured at 48.095 ms** (design §11.5) — 1.105×, **81.0 → 48.1 cumulative, 1.68×**, `sync` 1.76 ms over four runs. It hit its target exactly: the bare loop went **7.5 → 4.5 µs**, back to its tier-1 value, with every expression line 6–10 % better on top. **But the procedure call regressed again, 15.5 → 24.0 µs, with both `step_proc_call` and `eval_push_proc_call` already in RAM** — the third instance of one effect: tier 1 moved the expression evaluator and the call got worse, tier 2 moved the call path and the loop got worse, tier 3 moved the loop and the call got worse again. **Every tier moves the boundary and the flash residue adjacent to it pays**, because it is re-laid-out against a 16 KB cache each time; that is the finding, and it is self-limiting only because the regressing line is always smaller than the set that improved. **Tier 4** follows the same evidence to what is left on the call path — `frame.c`'s `frame_push`, `frame_reuse`, `frame_add_local`, `frame_at_depth` and the binding lookups, which matter twice over because `var_get`/`var_set` sit in RAM and call straight back out to flash to find a binding — for **+2,048 B** (`pico2w` 89.36 → 89.75 %), built and unmeasured. Sizing the endgame: all of `core/` is 160 KB and out of reach, but **the evaluator proper is 23 KB** with ~13 KB already resident, so ending the boundary game inside the evaluator costs about **10 KB more** against 54 KB free on `pico2w` and 44 KB on `pico+2w`, leaving a natural seam at the primitive bodies rather than a cut through the middle of a call. §1's 40 ms needs **1.20×** more, from 2.03× when M5 opened, and the question has changed from "which rejected rewrite" to "how much of the interpreter fits in RAM" |
