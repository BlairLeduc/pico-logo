# P14 — The vector direction: removing tiles and the sprite games (design)

Status: **Plan recorded 2026-08-26, not started.** Two scoping decisions were
taken with the user the same day: **Asteroids stays** (§2.2) and the
replacement throughput guard measures **Battlezone and Asteroids `play.frame`**
(§7). No implementation has begun; M0 is the first step.

Target: all three boards. Nothing here is board-conditional — the tile engine
was already tiered rather than gated, so its removal reads the same everywhere.

## 1. Why

The tile map and the sprite arcade games do not fit the Turtle graphics
aesthetic. A tile board is a picture assembled from a bank of pre-drawn
squares; a sprite game is a set of bitmaps moved over it. Neither is drawn by a
turtle holding a pen, which is the thing this interpreter is for. The direction
from here is **vector** games — Asteroids and Battlezone are what that looks
like, and both were built without a single tile.

This is a scope reduction, not a rescue. The tile system works and was
hardware-accepted (P9 M1–M3). It is being removed because it is not wanted, and
the roadmap should say so rather than leave 3,000 lines of working code with no
consumer.

## 2. Scope

### 2.1 Removed

- **The tile engine** — `newtiles`, `snaptile`, `newmap`, `settile`, `tile`,
  `stampmap`, `stamptile`, over `core/tilemap.c` and
  `core/primitives_tilemap.c` (P9's bake half).
- **Three games** — Turtle Trails, Galaxian, Space Invaders.
- **Checkpoint Run's remains** — the game file is already gone; its test and
  build wiring are not (§6.2).

### 2.2 Kept, and why

**Asteroids stays.** It was on the first removal list and came off it. It is a
vector game: its only sprite use is `putsh 1` of a two-pixel dot assigned to
four turtles used as shot carriers (`logo/games/asteroids:1483-1484`). It uses
no tile primitive. It also gives §7's guard a second subject, which matters
because Battlezone alone is a fragile one (§7.2).

**Sprites stay.** Only tiles are being removed. The multi-turtle/costume layer
(`setsh`, `putsh`, `snapsh`, `setanim`, `setmag`, `stamp`, `touching?`,
`over?`, `tell`/`ask`) is P5, is what Asteroids' shot carriers use, and is what
`logo/demos/graphics` demonstrates. Removing it is a separate question that is
not being asked.

**The design documents stay** (§8.2).

### 2.3 What is *not* affected, despite appearances

Two things look like tile dependencies and are not. Both were checked:

- **The dirty-tile tracker** (P5 M0, `tests/test_dirty_tiles.c`) is a display
  pipeline mechanism with no code relationship to `core/tilemap.c` — it shares
  the word "tile" and nothing else. `test_dirty_tiles.c` contains no tilemap
  reference at all. Battlezone and Asteroids depend on it. It stays.
- **`logo/demos/graphics` has no tilemap.** Its "STAMPED TILEMAPS" scene
  (`:200-217`) fakes one with repeated `stamp` on a hidden turtle. Only its
  labels and one CMake comment need rewording (§6.6).

## 3. What makes this cut clean

The tile engine is a leaf. Verified:

- **No evaluator integration.** `grep tilemap core/*.c` outside the two tile
  files returns exactly one line: the `primitives_tilemap_init()` call.
- **No checkpoint or save/load integration.** The tile bank and map are not
  serialised anywhere. `tilemap_reset()` has one caller —
  `primitives_tilemap_init()` itself.
- **No static SRAM.** Both pools are lazily allocated on first
  `newtiles`/`newmap`, so boards that never touched tiles already paid nothing.
  What is freed is headroom in the shared `mem_region_alloc` PSRAM region (up
  to 320 KB on the Plus 2 W) that the editor, `httpd` and the HTTP transfer
  buffer also draw from. **There is no SRAM win to claim.**
- **One consumer.** Turtle Trails is the only Logo file in the tree that calls a
  tile primitive. Asteroids, Battlezone, Temple, TTT, Galaxian and Invaders call
  none.

## 4. The two gates that fix the ordering

**Gate A — help coverage.** `tests/test_primitive_help_coverage.c` asserts every
registered primitive has a `reference/Pico_Logo_Reference.md` entry, and `help`
text is generated from that markdown at build time
(`scripts/generate_help.sh`). So the reference chapter deletion and the
primitive deregistration **must land in the same commit**. This is a feature: it
is what stops a half-removal shipping.

**Gate B — the throughput guard.** `tests/test_bench_throughput.c` currently
measures four game subjects; three of them are being deleted. The guard is
therefore **rebuilt before its subjects are removed** (M1 before M3/M4), so
there is one commit where old and new bounds are both green. Removing first
would mean baselining the new subjects with nothing to compare them against.

## 5. Milestones

Each milestone leaves `ctest --preset=tests` green.

| M | What | Gate |
|---|---|---|
| M0 | Baseline: full suite, keep `bench-throughput.txt` | numbers recorded in the P10 roadmap entry |
| M1 | Add Battlezone + Asteroids bench scenarios | seven scenarios green, new bounds set |
| M2 | Checkpoint Run sweep | suite green |
| M3 | Remove Galaxian + Space Invaders | suite green |
| M4 | Remove Turtle Trails | suite green |
| M5 | Remove the tile engine + reference chapter + device orphans | Gate A; anchor checker; all four presets link |
| M6 | Prose, roadmap, `graphify update .` | anchor checker |

### M0 — Baseline

Run the suite and keep `build-tests/bench-throughput.txt`. Those BENCH lines
are the only record of what Galaxian, Invaders and Trails cost, and they are
about to become unreproducible. Paste them into the P10 roadmap entry before
anything is deleted. This is the metrics-go-to-a-file rule: a number you can
only read off a screen is a number you cannot paste anywhere.

### M1 — The replacement guard

See §7. Commit with seven scenarios green.

### M2 — Checkpoint Run sweep

- Delete `tests/test_checkrun.c` (957 lines). It is **not registered** in
  `tests/CMakeLists.txt` — it has been orphaned since the game file was
  deleted, and has been neither built nor run.
- Delete `CHECKRUN_SOURCE` (`tests/CMakeLists.txt:145`), which points at
  `logo/games/checkrun`, a path that does not exist. Delete its `#ifndef` guard
  in `test_bench_throughput.c:31-33` and the unused
  `BOUND_CHECKRUN_FRAME_X_CAL`.
- Reword the two `pr` lines in `tests/logo/p10m0:73-74` that tell the operator
  to `load "checkrun`.

### M3 — Galaxian and Space Invaders

Delete `logo/games/galaxian` (944), `logo/games/invaders` (804),
`tests/test_galaxian.c` (712), `tests/test_invaders.c` (548),
`tests/test_game_huds.c` (158 — both its subjects are these two), and
`tests/logo/p10games` (109 — the hardware frame-cost script, used only by those
two tests).

In `test_bench_throughput.c`: delete `test_bench_galaxian_play_frame`,
`test_bench_invaders_play_frame`, their two bounds, their `#ifndef` guards and
their `RUN_TEST` lines. In `tests/CMakeLists.txt`: the `add_logo_test` blocks at
`:97-112` and `:168-176`.

### M4 — Turtle Trails

Delete `logo/games/trails` (1,712), `tests/test_trails.c` (2,163),
`tests/logo/p9trails` (141), the `add_logo_test` block at
`tests/CMakeLists.txt:157-160`, and `test_bench_trails_play_frame` with **both**
its bounds (`TRAILS_FRAME` and `TRAILS_BOARD` — the level build is guarded
separately from the frame).

`P10PROF_SOURCE` (`tests/logo/p10prof`) is a `test_trails` compile definition
but contains no trails or tile code — it is the board expression profiler,
P10 M5's other half. Check for another consumer before deleting it; if there is
none it goes too.

### M5 — The tile engine (one commit, Gate A)

**Engine.** Delete `core/tilemap.c` (300), `core/tilemap.h` (111),
`core/primitives_tilemap.c` (307). Remove `primitives_tilemap_init()` from
`core/primitives.c:110` and `core/primitives.h:188`. Remove the
`TILE_BANK_SIZE` / `TILE_MAP_SIZE` / `_PSRAM` / `TILEMAP_ROW_MAX` block at
`core/limits.h:226-246`. Remove the three source-list entries in
`CMakeLists.txt` (`:105`, `:456`, `:644` — host, tests and device lists).

**Device orphans.** Two console vtable slots exist solely to back tile
primitives and have no other caller:

- `canvas_snap` and `canvas_write_row` — `devices/console.h:210-226`
- `turtle_canvas_snap`, `turtle_canvas_write_row` and their two vtable entries
  — `devices/picocalc/picocalc_console.c:1250-1261, 1493-1494`
- `screen_gfx_write_row` — `devices/picocalc/screen.c:537`,
  `devices/picocalc/screen.h:167`
- the mock implementations and vtable entries — `tests/mock_device.c:576-593,
  724-725`

**`screen_gfx_snap` stays.** `snapsh` shares it; only its `opaque == true`
branch dies. Dropping the now-constant parameter is optional and can be left.

**Tests.** Delete `tests/test_tilemap.c` (441),
`tests/test_primitives_tilemap.c` (388), `tests/logo/p9m0` (115 — the tile
feasibility measurement script), `tests/logo/p9m2` (224), and the two
`add_logo_test` lines at `tests/CMakeLists.txt:66-67`.

**Reference.** Delete the `# Tile Maps` chapter,
`reference/Pico_Logo_Reference.md:2213-2360`. It sits cleanly between `## thaw`
and `# Text and Screen Commands`. Checked: every anchor into it (`#tile`,
`#newtiles`, `#settile`, `#stampmap`, `#stamptile`) is internal to the chapter,
and **no tile mention exists anywhere else in the 8,781-line file**. There is no
separate help edit — `help` regenerates from this markdown. Run
`scripts/check_reference_anchors.py`.

Also `reference:7887`: `curl -T invaders http://picologo.local/invaders` —
repoint at a surviving game.

Link all four presets after this commit: the device builds are the ones that
carry the deleted device ops.

### M6 — Prose and regeneration

Not load-bearing, but it is what keeps the tree honest.

- `README.md:112-116` — the Tile maps paragraph.
- `docs/roadmap.md` — the P9 entry (`:91`, `:402-416`), the P10 entry (with
  M0's numbers), and this document added to the companion list.
- `docs/bugs.md:179` — a Fixed-table row about a Trails tile bug. **Leave it.**
  The bug tracker is a record of what happened, not of what still exists.
- Passing prose mentions, none load-bearing: `hardware-notes.md`,
  `battlezone-design.md`, `concurrent-present-design.md`,
  `interpreter-throughput-design.md`, `http-server-design.md`,
  `launch-design.md`, `multi-sprite-design.md`, `sound-design.md`,
  `asteroids-design.md`; and comments in `tests/test_pfs.c`,
  `tests/test_primitives_words_lists.c`, `tests/test_temple.c`,
  `tests/test_asteroids.c`.
- `logo/games/asteroids` and `logo/games/battlezone` both cite "Galaxian's
  dive-siren idiom". The idiom outlives the game; these can stay.
- `tests/CMakeLists.txt:88` — the comment claiming `test_graphics_demo` renders
  "tilemap scenes". It does not (§2.3). Reword it and the demo's two labels.
- `graphify update .`

## 6. Notes on individual removals

### 6.1 Nothing needs a packaging change

`dist.sh` runs `mklfsimg logo dist/logo.img` over the whole `logo/` tree, and
there is no launcher manifest or game menu anywhere in `logo/startup`. Deleting
the game files is sufficient.

### 6.2 Checkpoint Run was already half-removed

`logo/games/checkrun` does not exist, but `tests/test_checkrun.c` (957 lines)
and the `CHECKRUN_SOURCE` define both survive it. The test is unregistered so
nothing caught the drift. This is unrelated to the direction change and would
have been worth sweeping anyway; it rides along in M2.

### 6.3 `tests/logo/p10m0` survives

It measures the two synthetic scenarios only, and its
`test_p10m0_script_runs` assertions (`"repeat loop"`, `"full ws"`) are
unaffected. Only its two closing `pr` lines mention removed games.

## 7. The replacement throughput guard (M1)

`tests/test_bench_throughput.c` is P10 M0's regression guard. Its method — a
scenario timed against an in-process calibration loop, so a loaded CI box
inflates numerator and denominator together and the guard does not flap — is
unchanged. Only the subjects change.

### 7.1 What survives untouched

`test_bench_repeat_loop`, `test_bench_proc_call_workspace_scaling`,
`test_bench_expr_shapes` and `test_p10m0_script_runs` are synthetic and have no
game dependency. Four of the seven scenarios need no work at all.

### 7.2 The two new scenarios

Both mirror `test_bench_galaxian_play_frame`:

- **Asteroids** — `load_game(ASTEROIDS_SOURCE)`, setup
  `"init.game setup.level setrefresh \"manual"`, 100 frames.
- **Battlezone** — `load_game(BATTLEZONE_SOURCE)`, setup
  `"init.game setrefresh \"manual"`, 30 frames.

Add `ASTEROIDS_SOURCE` and `BATTLEZONE_SOURCE` to the `test_bench_throughput`
compile definitions (`tests/CMakeLists.txt:141-152`). Read the BENCH lines and
set `BOUND_ASTEROIDS_FRAME_X_CAL` and `BOUND_BATTLEZONE_FRAME_X_CAL` to ~3× the
measured ratios, the convention the file's header documents.

Three findings that shaped this:

**Battlezone's `sync` is not a problem.** `init.game` arms `setrefresh "sync`,
and on the host a sync-mode frame waits the real frame period — which is why
`tests/test_battlezone.c:285` calls `setrefresh "auto` straight after
`init.game`. But `prim_sync` (`core/primitives_text.c:269-274`) degrades to a
plain present when `frame_sync_active()` is false, so `setrefresh "manual"`
alone is enough. No workaround needed. It does mean a mock `refresh_now()` per
frame sits inside Battlezone's measurement and not Asteroids'; say so in the
bench comment rather than trying to subtract it.

**The 128-procedure ceiling is already handled, and must stay handled.**
Battlezone defines exactly 128 procedures — the whole of `MAX_PROCEDURES`, as
P13's polish pass found. Asteroids defines 88. They cannot share a workspace,
and the failure mode is silent: the last `to` in the file goes missing. This is
safe today only because `test_scaffold_setUp` calls `procedures_init()` before
every `RUN_TEST`, so each scenario gets a clean table. **Keep them as separate
test functions and never merge them.**

**Both games are drivable headless.** Both define `play.frame` and `init.game`;
Asteroids also defines `setup.level`. `frame_storage_cells` needs only
`play.frame` plus `nodes`/`recycle`, so the cells-per-frame column can be kept
for both.

### 7.3 Bounds ledger

| Bound | Fate |
|---|---|
| `BOUND_REPEAT_ITER_X_CAL` | keep |
| `BOUND_PROC1_ITER_X_CAL` | keep |
| `BOUND_PROC_SCAN_RATIO` | keep |
| `BOUND_TRAILS_FRAME_X_CAL` | delete (M4) |
| `BOUND_TRAILS_BOARD_X_CAL` | delete (M4) |
| `BOUND_CHECKRUN_FRAME_X_CAL` | delete (M2 — already unused) |
| `BOUND_GALAXIAN_FRAME_X_CAL` | delete (M3) |
| `BOUND_INVADERS_FRAME_X_CAL` | delete (M3) |
| `BOUND_ASTEROIDS_FRAME_X_CAL` | **new (M1)** |
| `BOUND_BATTLEZONE_FRAME_X_CAL` | **new (M1)** |

## 8. Decisions

### 8.1 Asteroids stays (resolved with the user, 2026-08-26)

The first framing of this work listed Asteroids for removal alongside Galaxian
and Trails. It came off the list: it is a vector game, its sprite use is one
two-pixel dot shape on four shot-carrier turtles, and it is the second subject
§7.2 needs.

### 8.2 The design documents are kept as history (recommended)

`tilemap-scrolling-design.md` (1,112), `turtle-trails-design.md` (960),
`galaxian-design.md` (409), `space-invaders-design.md` (614) and
`checkpoint-run-design.md` (959) are **kept**, each with a one-line
`Removed 2026-08-26 — see roadmap P14` banner at the top.

The roadmap links to them and cites their measurements — P9's failed M0 gate and
its scrolling-half re-measure are the analysis that justifies this whole
direction change, and deleting the documents would orphan those links and throw
away the reasoning. The roadmap is explicitly a record of the past as well as
the future.

There is a live forward reference too. P13's L4 lever — the `newmodels` /
`setmodel` / `setview` / `drawmodel` family that is the only Battlezone
optimisation large enough to change what the game can *be* — is specified as
being "in the shape of the P9 tilemap primitives", and cites `stampmap`'s
5,916 ms → 7.6 ms result as its precedent. The tile *code* is not needed for
that; the tile *design* is the thing it was going to copy. Deleting
`tilemap-scrolling-design.md` would cost a still-open plan its model.

Deleting them instead would add ~4,050 lines to the total. That is the
alternative if the user prefers a clean tree over a documented one.

## 9. Totals

| | Lines |
|---|---|
| Tile engine (`core/`) | 718 |
| Tile tests + fixtures | 1,168 |
| Turtle Trails (game + test + fixture) | 4,016 |
| Galaxian (game + test) | 1,656 |
| Space Invaders (game + test) | 1,352 |
| Shared game tests (`test_game_huds`, `p10games`) | 267 |
| Checkpoint Run remains | 957 |
| Reference chapter | ~148 |
| **Deleted** | **~10,280** |
| Design docs retained as history (§8.2) | ~4,050 |

Seven primitives, two console vtable slots, three games, six test binaries.

## 10. Risks

1. **The guard loses two thirds of its subjects.** Four synthetic scenarios and
   two games is thinner than four games and three synthetics. Mitigated by M1's
   ordering (§4 Gate B), but if Battlezone or Asteroids is ever removed the
   guard should get a synthetic vector fixture rather than another game.
2. **Battlezone at exactly 128 procedures.** It cannot gain a procedure and it
   cannot share a workspace. Making it a benchmark subject does not change that,
   but it does add a second place that has to know it.
3. **The device-op removal is only checked by linking.** `canvas_snap` and
   `canvas_write_row` are optional vtable slots — a missed call site degrades
   silently rather than failing to build. Grep before deleting, and link all
   four presets at M5.
