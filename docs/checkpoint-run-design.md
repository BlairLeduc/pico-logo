# Checkpoint Run — a maze-driving game (design)

**Removed 2026-08-28 — see roadmap P14.** Kept as history.

Status: **design revised 2026-07-27; implementation in progress.**
The first implementation was withdrawn (see §16). Target files are
[`logo/games/checkrun`](../logo/games/checkrun) and
[`tests/test_checkrun.c`](../tests/test_checkrun.c).

This document specifies a maze-driving game inspired by the 1980 arcade game
*Rally-X*, developed by Namco and distributed in North America by Midway. The
implementation is pure Pico Logo and targets the existing interpreter, plus one
test-only extension to the mock device (§11.2). Its working release name is
**Checkpoint Run**. It should live at `logo/games/checkrun` and start with:

```logo
load "checkrun
checkrun
```

“Inspired by” means preserving the play pattern that makes the genre
recognisable:

- a car that always moves and turns at road junctions;
- a road network larger than the current view;
- ten checkpoints, including an early-collection score multiplier;
- pursuing cars, rocks, smoke screens, fuel, and a strategic radar;
- increasing pressure and periodic challenge rounds.

It does **not** mean shipping Namco's name, maze layouts, sprites, cabinet art,
flag symbol, text, or music. Checkpoint Run needs original maps, art, sounds,
title treatment, and terminology. The working name also needs an ordinary
release-name/trademark check before publication.

## 1. Product goals and boundaries

### Goals

- Fill the PicoCalc's entire **320×320 LCD** during play.
- Feel immediate with only four directions and one action button.
- Make the radar essential rather than decorative.
- Run acceptably on the Pico 2, the smallest and tightest target.
- Use the existing eight turtles, canvas, keyboard input, 25 fps synchronised
  refresh, and eight-voice PSG.
- Keep all simulation deterministic and testable on the native mock device.
- **Make maps data.** Adding a world must mean adding rows of text to the Logo
  file, with no asset pipeline and no binary to regenerate.

### Deliberate boundaries

- The world changes in screen-sized sectors instead of smoothly scrolling.
  Pico Logo has no canvas-shift or source-rectangle blit primitive; redrawing a
  scrolling 256×320 maze in Logo every frame is the wrong cost profile.
- Enemy driving is readable pursuit, not emulation of the arcade Z80 logic.
- High score is session-local in v1.
- One player only; no alternating two-player mode.
- No new interpreter primitive, larger device buffer, dependency, or
  interpreter limit is required. The only C change is to `tests/mock_device.*`
  (§11.2), which is test-only and never compiled into firmware.

Sector paging retains the important reason for the radar: most checkpoints and
enemies remain outside the current view. The swap happens only at a road exit,
where the player is already crossing a strong visual boundary.

## 2. Full-screen composition

Gameplay divides the 320×320 raster into two regions with no unused border:

```text
pixel x 0                                                    319
        ┌────────────────────────────────┬────────┐
        │                                │ SCORE  │  y 0
        │                                │ 012300 │
        │                                ├────────┤
        │                                │ FUEL   │
        │      256 × 320 ROAD VIEW       │ █████░ │
        │                                ├────────┤
        │                                │ RADAR  │
        │      one 16 × 20 sector        │ ·  □ · │
        │                                │  ▲  ●  │
        │                                ├────────┤
        │                                │ CP 4/10│
        │                                │ R03 L02│
        └────────────────────────────────┴────────┘  y 319
                                      x 256
```

In turtle coordinates:

| Region | Pixel bounds | Turtle-coordinate bounds |
|---|---|---|
| road view | x 0..255, y 0..319 | x −160..95, y −159..160 |
| instrument column | x 256..319, y 0..319 | x 96..159, y −159..160 |

Turtle coordinates relate to pixels by `turtle.x = pixel.x − 160` and
`turtle.y = 160 − pixel.y`.

The road view is exactly **16 columns × 20 rows of 16-pixel tiles**. Tile
centres for local column `c` and row `r`, both zero-based, are:

```text
screen.x(c) = -156 + 16 × c        ; c = 0..15
screen.y(r) =  156 - 16 × r        ; r = 0..19
```

These place a tile's 16×16 block flush with the view edges: tile `c` covers
pixel x `16c .. 16c+15`, and tile `r` covers pixel y `16r .. 16r+15`, so
columns 0..15 and rows 0..19 tile the 256×320 view exactly with no margin.
The turtle sits at the block's centre, offset (+4, +4) from its top-left
pixel.

> The withdrawn implementation used `screen.x(c) = -151 + 16c`, which left a
> 5-pixel margin on one side and a 4-pixel overhang on the other. The formula
> above is the one the tests assert.

### 2.1 Instrument column bands

The instrument column uses fixed bands so old text can be erased and rewritten
without disturbing neighbours. Every band is drawn by turtle 7; nothing in the
panel comes from an asset.

| Pixel y | Turtle y | Content |
|---:|---:|---|
| 0..31 | 160..129 | score and session high score |
| 32..63 | 128..97 | fuel label and horizontal gauge |
| 64..151 | 96..9 | 64×80 radar plus a 4-pixel vertical inset |
| 152..191 | 8..−31 | checkpoints collected and multiplier |
| 192..231 | −32..−71 | round and challenge-round status |
| 232..271 | −72..−111 | lives |
| 272..319 | −112..−159 | `READY`, `PAUSE`, smoke-empty, and warnings |

Band separators are single horizontal lines at pixel y 32, 64, 152, 192, 232
and 272, drawn in HUD white across x 256..319.

**Text baselines must sit inside their own band.** `write` begins at the
turtle's x and is centred vertically on its y, so a field's turtle y must be at
least 4 pixels inside its band. The withdrawn implementation wrote the
checkpoint field at turtle y 24 (pixel 136), placing it on top of the radar.

The attract, ready, crash, round-clear, and game-over screens may use all
320×320 pixels without the gameplay split.

## 3. World and map model

### 3.1 A 32×40-tile world

Each round uses one **32×40** road graph, divided into four sectors:

```text
          world columns
          0..15        16..31
        ┌────────────┬────────────┐
rows    │ sector 0   │ sector 1   │
0..19   │ northwest  │ northeast  │
        ├────────────┼────────────┤
rows    │ sector 2   │ sector 3   │
20..39  │ southwest  │ southeast  │
        └────────────┴────────────┘
```

Roads crossing a sector boundary must meet at the same row or column on both
sides. The outer world boundary is closed. A sector switch occurs when the
centre of the player's car enters the adjacent world tile:

1. hide all turtles;
2. redraw the road view for the new sector from the map (§4);
3. redraw the instrument panel and its static labels;
4. stamp every uncollected checkpoint, rock, and live smoke cloud in the sector;
5. draw dynamic HUD fields and all radar markers;
6. place visible cars using their unchanged world coordinates;
7. present once with `refresh`.

The crossing has already committed before the swap, and the simulation does not
add catch-up movement for the wall time spent redrawing. Hardware acceptance
should target a sector rebuild below **120 ms**. A late transition frame is
acceptable; ordinary frames are not.

### 3.2 The map is the only source of truth

Collision, AI, **and rendering** all read one Logo map. Nothing reads canvas
colour, and there is no second copy of the world in any other form.

The map is 40 words of 32 characters. Each character is a hexadecimal
four-way connection mask:

| Bit | Value | Road exit |
|---:|---:|---|
| 0 | 1 | north |
| 1 | 2 | east |
| 2 | 4 | south |
| 3 | 8 | west |

`A` is east+west, `5` is north+south, `F` is a four-way junction, and `0` is
not road. `road.at` converts a character to its mask without allocating a list.

**Each world is one list literal spanning forty lines.** A bracketed list may
span lines, unlike a parenthesised expression (§12.3), and two globals are
required rather than eighty because `MAX_GLOBAL_VARIABLES` is 128 for the whole
session (§12.7):

```logo
make "cr.map.1 [
  6AAEAAEAAEEEEAAEAAEEEEAAEAAEAAEC
  5005005007FFD005007FFD005005007D
  ...
]
```

A row consisting only of digits would be read as a *number* and lose its
leading zeros, so the test suite asserts every row is still 32 characters
long. That check is what makes this storage form safe for future maps.

Two worlds ship, alternating by round, with checkpoint and rock placements
changing between rounds.

This replaces the withdrawn design's four 320×320 BMPs per world. Removing
them removes ~412 KB of flash per world, the asset-regeneration step, and the
entire class of defect where the picture and the logic disagree — which is what
went wrong the first time (§16).

### 3.3 Map authoring rules

Validated by the test suite (§11), not by review alone:

- every exit has the opposite exit in its neighbouring tile (reciprocity);
- no exit crosses the outer world boundary (closed edge);
- every road tile belongs to one connected component;
- each sector has at least two exits to other sectors;
- at least three sector-boundary routes connect each half of the world;
- every checkpoint candidate and garage is reachable from the player start;
- the player start has at least two immediate route choices;
- enemy garages are distributed across at least two sectors;
- dead ends (tiles with exactly one exit) number no more than four per world.

These rules prevent the local enemy heuristic from becoming trapped and stop a
single enemy from permanently sealing a sector boundary.

The recommended shape is a full road grid with **rectangular building blocks**
carved out of it: obstacles are regions of `0` tiles, two or more tiles on a
side, never touching each other or the outer border. This yields the loop-rich,
dead-end-free network the pursuit heuristic needs, and reads as a city.

## 4. Rendering the road view from the map

The road view is drawn, never loaded. A road tile is a solid 16×16 block of
road slate; a `0` tile is left as off-road background. Corridors are therefore
one tile wide, and connections are implied by adjacency — two orthogonally
adjacent road tiles are joined because their blocks touch.

Because tiles tile the view exactly (§2), the rendered picture is a direct
function of the map, and a test can assert that correspondence tile by tile.

### 4.1 Drawing a block

`putsh` shapes are **8 columns × 16 rows, mono** — the `getsh` list format is
sixteen row bitmasks. A 16×16 block is therefore **two stamps of one solid
8×16 shape**, painted in the current pen colour:

```logo
putsh 1 [255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255]
```

Left half at `screen.x(c) − 4`, right half at `screen.x(c) + 4`, both at
`screen.y(r)`.

A 16×16 *colour* tile is only possible via `snapsh`, which costs one of the
fifteen shape slots per tile type. A four-bit connection mask would need
fifteen distinct tiles, consuming the entire slot budget and leaving nothing
for cars, checkpoints, rocks, smoke, or the crash animation. The mono
two-stamp scheme uses **one** slot for all road drawing.

### 4.2 Cost and the tuning levers

A sector is 320 tiles, of which roughly 200 are road, so a rebuild is roughly
**400 stamps** plus the panel. This is the one number in the design that is not
yet measured. Milestone 1 must instrument `draw.sector` with `ticks` on a
Pico 2 and report it before any further work.

If the 120 ms budget is missed, apply in this order:

1. `setmag 2` on the road shape gives a 16×32 stamp, covering a road tile and
   the road tile below it in one operation — roughly halving stamps in vertical
   runs.
2. Draw the off-road blocks instead of the road blocks when a sector is more
   than half road.
3. Build the four sector canvases once at round start and `savepic` them, then
   `loadpic` on crossings. This reintroduces an asset, but a generated,
   per-session one that still cannot disagree with the map. Note the flash-wear
   cost of writing ~412 KB per round to the internal filesystem.

Do not add a framebuffer-scroll or tilemap primitive for this game without
revising this design. If measurement shows a primitive is genuinely needed, the
case should be made with numbers from **both** this game and Turtle Trails,
since they have the same shape of problem.

### 4.3 Page reconstruction

Redrawing restores only the immutable road view and panel. `restore.sector`
then stamps each alive checkpoint, rock, and live smoke cloud whose tile
belongs to the sector, writes the current HUD fields, draws all radar marks,
and shows and positions cars in the sector.

Collected checkpoints never return when revisiting a sector because
`flag.alive`, not the picture, owns their state.

When a smoke cloud expires, turtle 7 redraws that one tile as a plain road
block, then redraws a checkpoint or rock if one belongs there. Smoke may be
emitted only on a road tile, so reconstruction is bounded and needs no
road-patch shape set.

## 5. Runtime representation

### 5.1 Turtle allocation

| Turtle | Use |
|---:|---|
| 0 | player car |
| 1..6 | up to six pursuing cars |
| 7 | canvas helper for road blocks, checkpoints, smoke, radar, and HUD |

All cars exist in global world coordinates even when not visible. A pursuing
car's turtle is shown only when it occupies the current sector. Road blocks,
smoke, rocks, checkpoints, radar marks, and HUD are canvas pixels, not turtles.

**Turtle 7 is the only turtle that draws.** Every drawing sequence must be
wrapped in a single `ask 7 [...]`; `ask` restores the previous addressee when
the block ends, so commands written after the closing bracket act on whichever
turtle was current. The withdrawn implementation closed the `ask` too early in
`draw.radar` and dragged the player's car across the instrument panel every
frame.

No `when` demons are needed. Player/enemy, player/rock, and enemy/smoke
collisions are resolved from logical coordinates in one deterministic
post-movement pass.

### 5.2 Fixed state

Small, fixed lists mutated with `.setitem`:

- `car.col`, `car.row`, `car.dir`, `car.offset`, `car.state`, `car.timer`,
  `car.speed`, `car.home` — seven entries each;
- `flag.tile` and `flag.alive` for ten checkpoints;
- `rock.tile` for the round's fixed rocks;
- `smoke.tile` and `smoke.until` for four smoke clouds.

Index 1 in each car list is the player; indices 2..7 correspond to turtles
1..6. Keep tile column and row as separate numbers rather than allocating
`[col row]` pairs in the frame loop.

Checkpoint, rock, smoke, and garage tiles are single integers:

```text
tile.index = row × 32 + col + 1       ; 1..1280
col = remainder (tile.index - 1) 32
row = intquotient (tile.index - 1) 32
```

`car.timer` is a general-purpose countdown (garage release, spin, recovery) and
must not be conflated with `car.release`; the withdrawn implementation reused
one field for both, so a smoked enemy could never recover.

### 5.3 Round versus game state

`init.round` sets up **only** per-round state: car positions, flags, rocks,
smoke, fuel, frame counter, collected count, multiplier. Score, lives, round
number, and session high score belong to `init.game` and must survive a round
boundary. The withdrawn implementation reset score and lives inside the
per-round setup, so no round bonus, progression, or extra life could ever
persist.

## 6. Driving and controls

### 6.1 Controls

| Key | Byte from `ascii readchar` | Action |
|---|---:|---|
| up | 181 | request north |
| down | 182 | request south |
| left | 180 | request west |
| right | 183 | request east |
| space | 32 | release smoke |
| `p` | 112 | pause/resume |
| `q` | 113 | leave the current game |

`poll.input` drains the keyboard queue each frame. The newest arrow becomes the
buffered direction; space sets a one-frame smoke request. This works well with
the keyboard's repeat behaviour because the car moves continuously — the player
steers rather than holds an accelerator.

### 6.2 Deterministic movement

Run at `(setrefresh "sync 25)` and advance every car explicitly. Do not use
`setspeed`: tile-centre turns, sector changes, smoke hits, and the native tests
must not depend on when autonomous motion happens to be polled.

Each car stores signed pixel progress from its current tile centre in
`car.offset`, measured along `car.dir`. A normal starting speed of 60
pixels/second advances 2.4 pixels per frame. When the offset reaches **8**, the
car enters the connected neighbouring tile and the offset carries forward as
`offset − 16`, which places it 8 pixels *before* the new tile's centre.

Player turning rules:

- a reverse is accepted immediately: `car.dir` flips and `car.offset` negates;
- a perpendicular direction remains buffered until the relevant exit opens;
- within four pixels of a tile centre, a legal perpendicular turn snaps to the
  centre. **The unused movement is carried into the new direction**: the new
  offset is the absolute value of the old one, not zero. Discarding it, as the
  withdrawn implementation did, loses up to four pixels of travel per corner;
- at a road end, the car stops at the centre but the engine and fuel continue;
- the costume uses `setrot "full`, so one north-facing bitmap serves all four
  directions.

The four-pixel cornering window makes steering forgiving without allowing a
turn through a wall. `road.open?` must consult `road.at`; a bounds-only check
lets the car drive through buildings.

## 7. Checkpoints, scoring, and rounds

Each round chooses ten distinct tiles from a reviewed candidate table. The
selection is deterministic for a given round number (`rerandom` plus a fixed
round seed during tests). Distribution rules require:

- at least two checkpoints per sector;
- no checkpoint on a garage, rock, sector-boundary tile, or player start;
- one checkpoint chosen as the **Turbo checkpoint**.

The Turbo checkpoint is Checkpoint Run's original presentation of the
early-special-item decision. It has a chequered cyan/magenta icon and blinks on
the radar. Collecting it turns on `×2` for checkpoints collected **afterwards**;
it neither doubles its own value nor retroactively doubles earlier points.

Checkpoint values rise with collection order:

```text
first = 100, second = 200, ... tenth = 1000
```

Apply `×2` only to checkpoints collected after the Turbo one, then award a
round-clear bonus of `10 × fuel`. Scores are integers. Award one extra life the
first time the score reaches 20,000; v1 awards no further extra lives, and the
award must be latched so it cannot fire twice.

A normal round ends only when all ten checkpoints are collected. Running out of
fuel does not directly kill the player (§8).

### 7.1 Challenge rounds

Rounds 3, 7, 11, ... are **challenge rounds**:

- all enemies begin parked in their garages;
- they remain parked while fuel is nonzero;
- at zero fuel they release together;
- there is no round-clear fuel bonus;
- a crash ends the challenge round but does not cost a life;
- collecting all checkpoints awards 5,000 additional points.

The same maps and simulation are used, so this mode costs little code while
changing the route-planning pressure substantially.

## 8. Fuel and smoke

Fuel starts at 1,500 units:

- subtract 1 per frame (60 seconds at 25 fps);
- subtract 60 per smoke release;
- **clamp at zero after every change**, not only after the per-frame drain;
- redraw the gauge only when its pixel width changes.

At zero fuel the player slows over one second to 30 pixels/second and cannot
release smoke. The round continues, leaving a narrow chance to reach the last
checkpoint. A new round or normal-life respawn restores full fuel.

The withdrawn implementation subtracted the smoke cost without clamping, so
fuel went negative; the per-frame drain then stopped (it guarded on `> 0`) and
the slowdown never triggered (it tested `= 0`), permanently disabling the fuel
mechanic. Tests must cover a smoke release with fuel below 60.

Space releases one smoke cloud on the tile behind the car if:

- fuel is at least 60;
- 250 ms (7 frames) have elapsed since the previous release;
- **one of four cloud records is free** — the release must search for a free
  slot, not always write slot 1.

A cloud lasts 1.6 seconds (40 frames) and animates on the canvas by restamping
one of two small dithered shapes every 200 ms. An enemy entering the cloud's
tile changes to `spin` for 900 ms, stops translating, and rotates 45 degrees per
frame. It then spends 300 ms in `recover`, moves at half speed, and returns to
normal pursuit. A cloud can spin multiple enemies but charges fuel only once.

**Spin and recovery must both expire.** An enemy in `spin` transitions to
`recover` when its timer reaches zero, and from `recover` back to `normal`
likewise. The withdrawn implementation had no transition out of `spin`, so
smoke deleted enemies permanently.

Smoke is drawn every frame it is live, not only when a sector is rebuilt.

Canvas smoke is sensed from its logical records, not with `over?`: it works for
off-screen enemies and remains correct while a sector is being redrawn.

## 9. Pursuing cars

### 9.1 Population and release

Normal rounds use:

| Round | Active enemies | Base speed |
|---:|---:|---:|
| 1 | 3 | 50 px/s |
| 2 | 4 | 52 px/s |
| 3 challenge | 4 parked | 52 px/s |
| 4–5 | 5 | 54–56 px/s |
| 6+ | 6 | 58 px/s, capped |

`init.round` must apply this table; a fixed six-car, fixed-speed population
ignores the difficulty curve entirely. Cars release from their garages at
staggered one-second intervals. A garage is a normal road tile after release;
it has no special collision rule.

### 9.2 Junction decisions

At each tile centre, an enemy gathers connected exits, excluding an immediate
reverse unless at a dead end. It selects the exit with the smallest Manhattan
distance to its target:

| Enemy type | Target |
|---|---|
| hunter (cars 1, 4) | player's current tile |
| interceptor (cars 2, 5) | three connected steps ahead of the player |
| collector (cars 3, 6) | nearest alive checkpoint to the player |

The collector targets the checkpoint the player is most likely to approach, not
the checkpoint nearest the enemy. This creates cut-offs without a global
pathfinder.

Equal distances use a per-car rotated priority order so the pack separates. On
20% of eligible junction decisions, choose the second-best exit. This prevents
permanent local loops and makes repeated rounds less mechanical while remaining
deterministic after `rerandom`.

Enemies update in every sector. Off-screen cars skip all turtle commands and
perform only map/state arithmetic.

**Target helpers must not write caller state.** Use `repcount` and `local`
rather than shared `cr.*` globals: the withdrawn `nearest.flag` clobbered the
loop counter and best-so-far of the `choose.enemy.dir` loop that called it.

### 9.3 Collisions

After all movement:

1. collect a checkpoint at the player's centre;
2. test player against rocks;
3. test each enemy against live smoke clouds;
4. test each non-spinning enemy against the player;
5. commit at most one crash or round-clear transition.

Use squared global pixel distance, not `touching?` and not tile equality. A car
collision occurs at distance ≤10 pixels; a rock collision at ≤9 pixels. With a
maximum movement below 3 pixels/frame, these radii cannot be skipped in one
update.

An enemy hitting a rock enters the same `spin` state as a smoke hit. The player
hitting a rock or active enemy crashes. During the crash sequence simulation is
frozen, the player cycles through four explosion shapes, all PSG voices are
stopped, and one life is removed. Checkpoints already collected remain gone. If
lives remain, **cars return to their garages** (`car.home`), fuel refills, and
the player respawns at the start tile; otherwise the game moves to game over.

## 10. Radar, HUD, art, and sound

### 10.1 Radar

The radar occupies a 64×80 area and maps one world tile to a 2×2 pixel cell:

```text
radar pixel x = 256 + 2 × world.col        ; turtle x =  96 + 2 × world.col
radar pixel y =  68 + 2 × world.row        ; turtle y =  92 - 2 × world.row
```

Columns 0..31 therefore occupy pixel x 256..319 exactly. The withdrawn
implementation used turtle x `101 + 2c`, which ran off the right edge of the
screen for the last two world columns.

It deliberately omits the road network; the player must learn routes while the
radar answers only “where should I go, and what is coming?” Markers are:

| Object | Marker |
|---|---|
| player | blinking cyan/black 2×2 square |
| enemy | red 2×2 square |
| checkpoint | yellow 2×2 square |
| Turbo checkpoint | alternating cyan/magenta 2×2 square |

Turtle 7 **erases each old marker before stamping its new position**. Radar car
markers update only when a car enters a new tile, not for sub-tile motion.
Checkpoint marks change only on collection; player/Turbo blinking changes every
eight frames. This avoids clearing and repainting 5,120 radar pixels each frame,
and avoids the permanent smear left by stamping without erasing.

### 10.2 HUD

Score, fuel, checkpoints, round, lives, and message fields are change-tracked:
each keeps its last drawn value, and a field is redrawn only when its value
changes. Erase the previous text or gauge rectangle, then draw its replacement
in the same frame before `sync`. Writing a shorter number over a longer one
without erasing leaves stale digits.

### 10.3 Palette

Game logic compares palette *slots*, never RGB values:

The game defines its own palette with `setpalette` in `setup.palette`, so no
asset carries one and the slots cannot drift:

| Slot | Use | RGB |
|---:|---|---|
| 255 | reserved transparent — never a colour | — |
| 254 | HUD white | 232, 237, 242 |
| 253 | Turbo magenta | 224, 70, 190 |
| 252 | checkpoint yellow | 246, 210, 70 |
| 251 | player cyan | 75, 220, 230 |
| 250 | road slate | 86, 97, 109 |
| 249 | enemy coral | 238, 105, 90 |
| 248 | smoke grey | 190, 196, 202 |
| 247 | instrument panel black | 8, 11, 16 |
| 246 | shoulder sand | 198, 161, 100 |
| 245 | rock orange | 214, 122, 40 |
| 244 | off-road dark teal (screen background) | 18, 59, 66 |

The withdrawn design's table had no panel slot, assigned 247 to rock orange,
and made 255 both "transparent" and the off-road colour. The shipped art used
247 for panel black, so the implementation aliased rocks onto 246 — drawing
them in shoulder sand, indistinguishable from the verge — and 255 cannot be a
background at all (§12.8). Slots 245 and 244 resolve both collisions.

### 10.4 Shape slots

Fifteen slots are available. All road drawing uses one:

| Slot | Use |
|---:|---|
| 1 | solid 8×16 road block (mono) |
| 2 | player car |
| 3 | enemy car |
| 4 | checkpoint |
| 5 | Turbo checkpoint |
| 6 | rock |
| 7–8 | smoke animation |
| 9–12 | crash animation |
| 13 | radar marker (2×2 within a mono 8×16) |
| 14–15 | spare |

Cars use mono shapes, pen tint, `setrot "full`, and a manual two-frame toggle
every four simulation frames. Do not use `setanim`; pausing, sector hiding, and
crash-state animation are simpler when one clock owns every visual change.

### 10.5 Sound

Use original procedural effects and an original short ostinato.

| Voices | Purpose | Timbre |
|---|---|---|
| `[0 4]` | engine drone | sustained triangle, pitch follows speed state |
| `[1 5]` | original driving ostinato | quiet pulse sequence via `play` |
| `[2 6]` | checkpoint, Turbo, round-clear cues | short saw/triangle phrases |
| `[3 7]` | smoke hiss, skid, crash | periodic/white noise |

Set envelopes and waveforms once in `setup.sound`. Update the engine pitch only
when the speed state changes. Restart the ostinato with `playing?` rather than
queueing notes every frame. Smoke noise is brief; crash noise takes priority on
its pair. All sounds are centred by addressing the matching left/right pair.

Pause gates the engine and music and resumes them from a new phrase. `stopsound`
runs on crash, game over, and normal exit; BREAK/error handling remains the
interpreter's final safety net.

## 11. State machine and frame order

States are flat and iterative:

```text
attract -> ready -> playing
                 -> crash -> ready | game over
                 -> round clear -> next round
         challenge playing -> challenge result -> next round
game over -> attract
```

No state calls the next state recursively. The top-level procedure loops over
one game, and one game loops over rounds, keeping evaluator stack depth bounded.

One playing frame is:

```logo
to play.frame
  poll.input
  if not :paused [
    step.fuel
    maybe.smoke
    step.player
    step.enemies
    step.smoke
    collect.checkpoint
    check.rocks
    check.smoke.hits
    check.car.hits
    update.engine.sound
  ]
  update.visible.turtles
  draw.changed.radar
  draw.changed.hud
  sync
end
```

Ordering guarantees that a checkpoint reached on the same frame as the final
fuel unit is spent still counts, while a collision on the final checkpoint frame
takes precedence over round clear. Commit only one transition after `sync`.

## 12. Logo coding constraints

These are properties of the interpreter, each reproduced against
`./build-host/logo`. Ignoring the first two is what made the first
implementation unrunnable; the rest were found while rewriting it.

1. **There is no `>=` or `<=`.** The parser splits them and leaves a dangling
   `=`. Write `not (a < b)`.
2. **A call's final argument greedily absorbs trailing infix.**
   `item :i :car.dir * item :i :car.offset` parses as
   `item :i (:car.dir * item :i :car.offset)`, applying `*` to the list. Always
   parenthesise: `(item :i :car.dir) * (item :i :car.offset)`.
3. **Multi-line parenthesised expressions are dropped inside procedure
   bodies.** Keep each parenthesised expression on one line.
4. **`load` joins lines only for a `to`…`end` procedure body**; every other
   line is executed on its own, and `LOAD_MAX_LINE` is 256 bytes. A bracketed
   list literal therefore cannot span lines in a loadable file — a multi-line
   `make "m [` … `]` fails with `I don't know how to <first row>`, and a list
   literal inside a procedure body is mangled too. Bulk data needs one
   complete statement per line (§3.2).

   **The test harness must load the file the same way `load` does.** An
   earlier `tests/test_checkrun.c` joined bracketed literals across lines, so
   the whole suite passed against a file the device could not load at all.
5. **`~` line continuation is not supported.** It raises
   `I don't know how to ~`.
6. **A parenthesised call to a user-defined procedure must not appear as an
   operand inside an arithmetic expression.** Two observed forms:
   - as the left operand of a `make` value or command argument,
     `make "x ((screen.x :c) - 4)` fails outright with `) without (`;
   - nested on the right, `output (item 1 :car.col) + (3 * (dir.dx :d))`
     **silently returns the wrong number** — 27 where 11 was correct.

   Bind the call first: `make "u (dir.dx :d)`, then
   `output (item 1 :car.col) + (3 * :u)`. Primitives are unaffected
   (`make "x ((item 1 :L) + 5)` is fine). The silent-wrong-answer form is the
   dangerous one, because nothing errors and a plausible value comes back.
7. **Three call levels deep with `repcount` at the bottom evaluates one
   iteration stale** as the value of a `make`, and can surface as a spurious
   `Out of space`. `make "nm (word :p (pad2 (repcount - 1)))` inside a `repeat`
   returns the previous iteration's value. Give each step its own `make`.
8. **`MAX_GLOBAL_VARIABLES` is 128 for the whole session** (`core/limits.h`).
   Eighty map-row globals plus game state exhausts it and reports
   `Out of space`. Store bulk data in lists, not in many named variables.
9. **`setbg` takes 0..254; 255 is the reserved transparent index** and cannot
   be a background colour. Off-road therefore needs its own slot (§10.3).
10. `repcount` gives the 1-based loop index, and `output`/`stop` inside a
   `repeat` body return from the enclosing procedure. Loops need no counter
   globals; helpers must use `local`, not shared `cr.*` names.
11. `remainder` truncates toward zero, so `remainder -3 4` is `-3`. Keep
    operands non-negative when using it to wrap.
12. `ask` restores the previous addressee at its closing bracket. Every drawing
    sequence belongs inside one `ask 7 [...]`.

Constraints 6 and 7 have the same practical remedy, which is also the house
style for this file: **bind every intermediate result to a `local` rather than
nesting calls inside expressions.** Two mechanical checks are worth running
over any change to the game:

```bash
grep -nE '>=|<=' logo/games/checkrun            # constraint 1
grep -nE '\(\([a-z]' logo/games/checkrun        # candidates for constraint 5
```

**Every procedure must be executed at least once by the test suite.** These are
runtime errors that reading does not reliably catch, and a suite that exercises
only the arithmetic helpers will pass over a game that cannot run a frame.

## 13. Memory and performance

The hot path must allocate no lists:

- use `setx`/`sety`, not `setpos (list ...)`;
- mutate fixed state with `.setitem`;
- return target column and row through separate calls or locals, not a
  two-item list;
- never use `sentence`, `fput`, or `lput` per frame — including for HUD text,
  which must be change-tracked and therefore built only when a value changes;
- do not rebuild radar or checkpoint lists during play.

Persistent map storage is 40 row words per world plus small candidate tables.
Four smoke records and seven car records are fixed. Run `recycle` after
constructing a round and before entering its frame loop, and at attract and
game-over boundaries — not every frame.

At 25 fps, an ordinary frame has 40 ms. Profile on Pico 2 with `ticks`:

- ordinary same-sector frames must stay within 40 ms;
- a frame with six enemy junction decisions and radar moves should stay within
  40 ms;
- a sector rebuild should stay below 120 ms (§4.2);
- free nodes must remain stable through 30 minutes of smoke use, crashes, and
  sector crossings.

## 14. Tests

`tests/test_checkrun.c` follows the pure-Logo loading pattern in
`tests/test_galaxian.c`.

### 14.1 Required coverage

1. **Map invariants**
   - 40 rows × 32 characters, per world;
   - reciprocal road exits and closed world edge;
   - one connected road component;
   - sector-crossing, garage, start, checkpoint-candidate and rock validity;
   - dead-end count within budget.
2. **Coordinates and sectors**
   - all four world corners and sector-local corners;
   - world tile ↔ screen coordinate conversion, including that columns 0..15
     and rows 0..19 tile the view exactly;
   - exact sector changes in all four directions.
3. **Rendering agrees with the map**
   - `draw.sector` stamps a block at every road tile of the sector and at no
     other tile;
   - stamps land on the coordinates §2 specifies;
   - the panel is redrawn and no road stamp lands in the instrument column.
4. **Player driving**
   - continuous travel, fractional carry, wall stop against a building;
   - buffered perpendicular turn and the four-pixel window, including that the
     unused movement carries into the new direction;
   - immediate reverse;
   - no turn through a missing map connection.
5. **Checkpoint setup and scoring**
   - ten distinct legal placements with required sector distribution;
   - rising values;
   - Turbo multiplier applies only afterward and not to itself;
   - final checkpoint and fuel bonus;
   - one extra life at 20,000, latched so it fires once.
6. **Fuel and smoke**
   - frame drain, smoke cost, zero clamp including a release below 60 fuel,
     cooldown, and the zero-fuel slowdown;
   - four-cloud capacity, free-slot search, and expiry;
   - smoke spin **and recovery back to normal**, including multiple enemies in
     one cloud.
7. **Enemy AI**
   - hunter/interceptor/collector targets;
   - reverse exclusion, dead-end reversal, rotated ties;
   - deterministic second-best choice;
   - off-screen enemies continue to update;
   - a target helper does not disturb its caller's loop.
8. **Collisions and state**
   - player/enemy, player/rock, enemy/rock, and harmless spinning enemy;
   - collected checkpoints survive death and sector redraw;
   - score, lives and round survive a round boundary;
   - normal death versus challenge-round crash;
   - iterative replay/round soak does not grow procedure depth.
9. **Smoke test of every procedure**
   - each procedure in the file is executed at least once, so the parse
     hazards in §12 cannot hide in an untested branch.

### 14.2 Mock device extension

`MOCK_CMD_STAMP` currently records no parameters, so nothing can assert *where*
a stamp landed, and `MOCK_COMMAND_HISTORY_SIZE` (256) is smaller than one
sector rebuild. Group 3 requires:

- recording turtle number, x, y, shape, magnification, and pen colour with each
  stamp;
- a history large enough for a full sector rebuild;
- an accessor in the shape of the existing `mock_device_get_*` helpers.

This is test-only code in `tests/mock_device.*`, never compiled into firmware,
so it carries no SRAM cost on any board. Turtle Trails needs the same facility
to validate its board against its 36 encoded words.

### 14.3 Running

```bash
cmake --preset=tests
cmake --build --preset=tests
ctest --preset=tests
```

Then build all three firmware presets and play-test on Pico 2 hardware first.
Acceptance requires a complete normal round, a challenge round, a crash in each
sector, fuel exhaustion, rapid smoke use, and a 30-minute soak.

## 15. Implementation milestones

1. **Map, road view, and measurement:** author and validate one world; sector
   rendering, coordinate conversion, player driving, and the full-screen panel.
   **Report `ticks` for a sector rebuild and an ordinary frame on a Pico 2
   before proceeding** (§4.2).
2. **Round objective:** checkpoint selection, Turbo multiplier, score, radar,
   fuel, round clear, and one complete replayable round.
3. **Threats:** rocks, one hunter, crashes, lives, respawn, and game over.
4. **Full pursuit:** six-car state tables, three target styles, off-screen
   updates, smoke, spin/recovery, and difficulty profiles.
5. **Game structure:** second world, challenge rounds, attract/ready/results,
   extra life, session high score, and iterative soak.
6. **Presentation and release gate:** final original art, original PSG music and
   effects, hardware tuning, all firmware builds, name clearance, and an
   explicit review that no Rally-X protected asset remains.

Only milestone 1 may change C, and only `tests/mock_device.*` (§14.2). If
implementation finds a missing interpreter primitive, stop and revise this
design before changing `core/` or `devices/`.

## 16. Why the first implementation was withdrawn

Recorded so the same failure is not repeated.

The 463-line implementation dated 2026-07-27 could not run a single frame.
Seven procedures errored immediately on the two parse hazards in §12 —
`step.enemies`, `check.rocks`, `check.car.hits`, `car.global.x`, `car.global.y`,
`update.visible.turtles`, `nearest.flag`, and `choose.enemy.dir`. Its test suite
passed because it exercised only the fourteen procedures that happened to avoid
those traps, and never called the frame loop.

Independently, the encoded map was dead code: `road.at` was referenced only by
the test, `road.open?` was a bounds check that ignored the map, and
`make.world` produced a fully dense grid in which every interior tile was a
four-way junction. The four shipped BMPs meanwhile contained a genuine maze, so
the picture and the logic described different worlds and the car would have
driven through every wall. The design specified the test that would have caught
this — “road tile centres agree with the encoded map” — and it was never
written.

The lessons are in §3.2 (one source of truth, and rendering derives from it),
§12 (the parse hazards and the requirement that every procedure be executed by
a test), and §14.2 (the mock must be able to observe what was drawn).

## References

- [Pico Logo Reference](../reference/Pico_Logo_Reference.md), especially
  `getsh`/`putsh`, `snapsh`, `stamp`, `setmag`, `setrot`, `ask`/`tell`, refresh
  modes, keyboard input, sound, `ticks`, and `recycle`.
- [Multi-sprite design](multi-sprite-design.md) for the compositor, turtle,
  costume, demon, and memory budgets.
- [Turtle Trails design](turtle-trails-design.md), which has the same
  map-versus-picture problem and the same need for §14.2.
- [Space Invaders design](space-invaders-design.md) and
  [Galaxian design](galaxian-design.md) for proven pure-Logo state loops,
  canvas/sprite separation, mutable fixed lists, sound, and native tests.
- [Midway Rally-X Parts and Operating Manual](https://arcarc.xmission.com/PDF_Arcade_Bally_Midway/Rally-X_Parts%20and%20Operating_Manual_%28Jan_1981%29_%28Bad_Scan%29.pdf)
  for the contemporary control, radar, checkpoint, multiplier, fuel, smoke, and
  operator context.
- [Museum of the Game: Rally-X](https://www.arcade-museum.com/Videogame/rally-x)
  for a concise gameplay and cabinet summary.
