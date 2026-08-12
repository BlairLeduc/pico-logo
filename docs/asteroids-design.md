# Asteroids in Pico Logo (design)

Status: **M0, M1 and M2 measured on a Plus 2 W, 2026-08-11.** Each measurement
has rewritten this document, and that is the point of the milestone structure
rather than a failure of it.

- **M0** — the frame clears and redraws rather than erasing in place (§3.3),
  the rate is 15 fps rather than 20 (§12), the outlines are three rather than
  nine (§13).
- **M1** — rocks only, and it fits: 60.5 ms at twelve rocks against a 66.7 ms
  budget, `frame = 30.5 + 2.507 n`, and it plays smoothly.
- **M2** — the ship and the shots, measured at **86.4 ms**, nineteen over. The
  cause was not any of the levers §12 had priced: it was `item`. Fusing the
  three passes over the rocks into one (§15) and taking the rate to **14 fps**
  brings twelve rocks to **65.4 ms against a 71.4 ms budget, with the worst
  observed frame inside it** — the first time in this design that has been true
  (§12a). Twelve rocks, three shots and six-segment larges all survive, with
  roughly 9 ms for M3 and M4. **Played and accepted 2026-08-12**: 14 fps reads
  smooth and the ship feels right; the hit boxes were too generous and the ship
  slightly too big, both now fixed.

The three games in the tree so far — [Space Invaders](space-invaders-design.md),
[Galaxian](galaxian-design.md), [Turtle Trails](turtle-trails-design.md) —
are all *sprite* games: bitmap costumes worn by turtles, a crowd stamped onto
the canvas, and a present that touches only the dirty sprite tiles. Asteroids
is the first one that is **not**. The 1979 machine had no raster at all: an
XY monitor traced a display list of line segments, which is why its rocks are
hollow jagged outlines that rotate freely and its ship is three strokes.

That is a turtle-graphics program. `fd`/`rt` with the pen down *is* a vector
display list, `seth` before the walk rotates the whole shape for free, and
`wrap` splits a segment across the screen edge the way the original playfield
wraps. Nothing in this design needs a new primitive. What it does need is a
careful answer to one question the sprite games never had to ask: **how do
you un-draw last frame's lines?** §3 is that question, and it is the whole of
the risk.

The user brief: turtle graphics for the vector look, `sync`-paced managed
refresh so no one sees a frame being built, and `write` for the score and
game info at the top of the display. All three are taken as given below.

---

## 1. Deliverables and source of truth

| | |
|---|---|
| Game | `logo/games/asteroids` — one Logo file, no extension (flash fs files are extensionless), no `-` or `/` in the name so `load "asteroids` parses |
| Tests | `tests/test_asteroids.c` (Unity + mock device, 49 tests), mirroring `tests/test_galaxian.c`; the M0 harness has its own binary, `tests/test_p11rocks.c` |
| Design | this document |
| Measurement | `logo/tests/p11m2` times a real frame at 6, 9 and 12 rocks with the rock pass read apart from the rest — it writes its numbers to a file, because numbers on a display cannot be copied off it. `logo/tests/p11rocks` is M0's standalone erase-strategy harness and survives because nothing else reproduces that question; `logo/tests/p11m1` is **gone**, since the fusion removed the procedures it called by name |
| Outline generator | [`scripts/gen_rocks.py`](../scripts/gen_rocks.py), host-side, output pasted in (§6.3) |

Play: `load "asteroids` then `asteroids`.

## 2. What the game is, mechanically

The arcade rules, kept:

- Rocks drift at constant velocity, rotating, wrapping at every edge.
- Shooting a **large** rock yields two **medium**; a medium yields two
  **small**; a small vanishes. The level ends when no rock is alive.
- The ship rotates, thrusts along its facing, and keeps its momentum. It does
  not stop when you stop thrusting; it has no brake.
- A flying saucer crosses at intervals and shoots back.
- Hyperspace teleports the ship somewhere random, which may be worse.

Removed or reduced in §13, but the list above is the game.

## 3. The central decision: how a frame gets erased

Both candidates present the finished frame with `sync`, so neither shows a
frame being drawn. They differ in what the *previous* frame costs.

### 3.1 Why "clear and redraw" is not free here

The obvious vector-machine translation is to wipe the screen at the top of the
frame, re-trace the display list, and present once at the bottom. Managed
refresh looks like it should make that safe, and on a double-buffered machine
it would be. **On this one it is not, because the clear is not buffered.**

`clean` and `cs` both reach `screen_gfx_clear()`
([screen.c](../devices/picocalc/screen.c)), which has no refresh-mode check:

```c
memset(gfx_buffer, GFX_DEFAULT_BACKGROUND, sizeof(gfx_buffer));
if (screen_mode == SCREEN_MODE_GFX)
    lcd_clear_screen(GFX_DEFAULT_BACKGROUND);   // synchronous panel write
dirty_tiles_clear(&gfx_tiles);                  // "buffer and LCD are in sync"
```

The panel write happens in manual and `sync` mode exactly as it does in auto,
and `lcd_clear_screen` walks the whole **320 × 480 frame memory** one
`lcd_blit` per row ([lcd.c](../devices/picocalc/lcd.c)) — 153,600 px, half
again more than a visible screen, so hardware-notes §9.1's ~25 ms is a floor
for it rather than a ceiling. Two consequences:

- **~25 ms of a 40 ms frame** is gone before a rock is drawn.
- **The screen visibly blanks every frame.** Black is on the panel
  synchronously; the redraw only appears at the `refresh`. That is a
  full-screen flash at the frame rate — the exact artefact managed refresh is
  being used to avoid, and worse than simply drawing in auto mode.

The argument this section originally made from there — that re-tracing every
rock is cheap next to a 25 ms wipe, because a vector frame is thin outlines
over a tiny fraction of 102,400 pixels — is **wrong, and §3.3 has the
measurement that says so.** It fails on its second clause: the outlines are
thin, but a dozen of them are *scattered*, and what the present costs is not
pixels but tile rows.

### 3.1.1 The clear is now buffered (B16, fixed)

Skipping the panel fill in manual mode contradicts the reference's own
contract for it — *"drawing accumulates off-screen and nothing appears until
you say `refresh`"*. The write-through is an auto-mode optimisation (fill the
panel directly and you own a clean dirty state, with no compose-and-blit); it
simply was not revisited for manual mode. Logged as **B16** and **fixed on
2026-08-11**: `screen_gfx_clear` keeps the panel fill for auto mode and calls
`dirty_tiles_mark_all()` in manual mode instead, so the wipe is deferred to
the present like everything else. See [bugs.md](bugs.md).

With B16 fixed, clear-and-redraw becomes *viable* — a ~0.3 ms memset, with the
cost moved into a full-screen present. That is why §3.3 measured it rather
than dropping it, and on the board it is the strategy that won. Without the
B16 fix this design would have been written around the slower of the two on
the strength of an artefact.

### 3.1.2 `sync`, not `refresh`

Worth stating because the two questions look like one: the refresh *mode* is
orthogonal to the erase *strategy*. `sync` mode is manual mode plus pacing —
drawing accumulates off-screen either way — so nothing about clear-and-redraw
would require plain `refresh`, and `refresh` alone would leave the game
free-running at a variable frame rate. This design uses `sync` under every
strategy in §3.

### 3.2 Erase in place — measured, and rejected

> **This section is kept for its reasoning and its verdict. M0 measured it on
> a board on 2026-08-11 and it loses at every rock count the game plays at
> (§3.3). The game is written around clear-and-redraw.** What survives from
> here is the pen-colour finding, which clear-and-redraw does not need, and
> the pen-size rule, which it does.

The alternative is the one the tree already uses for HUD text: draw the shape
again where it was, in a pen that removes it, then draw it at its new place.

```logo
setpc :bg.colour                        ; the eraser, once for the whole pass
repeat :n [place repcount  draw.rock repcount]
step.all                                ; only now mutate x, y, angle
setpc :fg.colour
repeat :n [place repcount  draw.rock repcount]
```

**The eraser is a pen colour, not `pe`.** The obvious spelling —
`pe  place :i  draw.rock :i` — cannot work, and the reason is worth stating
because it is invisible until you run it. Pen up, down, erase and reverse are
**one enum** (`LogoPen`, [devices/console.h](../devices/console.h)), not a
mode orthogonal to up/down; and every outline procedure contains a `pd` in its
prologue, because §6.1's walk reaches the first vertex with the pen up. So the
shape's own `pd` would cancel the `pe` before a single segment was drawn, and
the erase pass would draw a second rock on top of the first. Drawing in the
background colour has none of that coupling, it is what §10's HUD erase
already does, and it is **cheaper**: two statements a frame rather than two
per rock per pass. It needs the canvas to be otherwise empty in exactly the
same way `pe` would, which §3.2 already requires below.

It also makes the pass *visible to the host tests*: the mock device records a
segment only when the pen is `LOGO_PEN_DOWN`, so an erase pass drawn with `pe`
would leave no trace for §17's retrace test to check.

This doubles the line drawing, but the present then touches only the tiles
those strokes covered — the regime the dirty tracker was built for, measured
at **1.6–2.7 ms** for the shipped games. §12's arithmetic said the doubled
drawing would cost far less than the 25 ms it saves. **It does not**: at
twelve rocks the second drawing pass costs 24.6 ms and the present it saves is
4.4 ms (§3.3). The 1.6–2.7 ms figure was for compact sprites on a static
background and never transferred; §18 flagged that risk and it is the one that
bit.

Three properties would have made it safe:

- **The erase retraces exactly.** The shape walk is deterministic and the
  state it starts from is byte-identical, so `pe` removes precisely the pixels
  `pd` wrote. The rule that enforces this: *erase from the old state before
  anything mutates it* — one ordering bug here leaves permanent litter on the
  canvas, which is this design's signature failure mode.
- **Pen size stays 1.** A wide pen's round caps spill outside the stroke and,
  in wrap mode, across the screen edge — the exact effect that made an early
  present-cost harness read every frame as a full screen
  ([hardware-notes §9.1](hardware-notes.md)).
- **Nothing else lives on the canvas.** The playfield is otherwise empty, so
  an erase can only take back its own strokes. The one exception is the HUD,
  handled by §10.

The known artefact: where two rocks' outlines cross, erasing the first punches
a one-pixel hole in the second for one frame. At 20 fps on thin white lines
this is invisible in motion; it is why the original vector machines did not
care either. If it does show, `px` (`penreverse`) XORs instead and recovers
the overlap — at the cost of a pen whose result the reference itself calls
complex on a non-black background. **Not** in v1.

### 3.3 M0's answer: clear and redraw

**Measured on a Pimoroni Pico Plus 2 W, 2026-08-11, 60 frames a point, two
runs reproducing every figure within 1 %** (`logo/tests/p11rocks`; the scene
is 4 large, 4 medium and 4 small at twelve, an equal mix at every count).
Milliseconds:

| rocks | | body | present | **frame** |
|---:|---|---:|---:|---:|
| 6 | erase in place | 25.3 | 14.1 | **39.4** |
| 6 | clear + redraw | 13.3 | 26.2 | **39.5** |
| 9 | erase in place | 37.6 | 17.4 | **55.0** |
| 9 | clear + redraw | 19.5 | 26.3 | **45.8** |
| 12 | erase in place | 50.5 | 21.9 | **72.4** |
| 12 | clear + redraw | 25.9 | 26.3 | **52.1** |

**The strategy question is settled, and §3.2 comes out.** The two are level at
six rocks and clear-and-redraw wins by 9 ms at nine and by 20 ms at twelve —
and six rocks is the *bottom* of what this game plays at, so erase-in-place
never wins during play. It is also the simpler code, so nothing is being
traded for the speed.

The decisive number is the one §3.3 said it would be. **A 12-rock dirty
region is 21.9 ms of a 26.3 ms full screen — 84 % of it.** Erase-in-place
pays a second full drawing pass, 24.6 ms, and buys 4.4 ms of present with it.
The mechanism is §4's, exactly as written but far bigger than it was costed:
the tracker keeps one inclusive span per tile row, so a rock at each end of a
row dirties the whole row, and twelve scattered rocks reach almost every row.
Sparsity in *pixels* does not survive contact with a row-span tracker.

Two structural readings worth carrying forward:

- **The present is a floor, not a variable.** Clear-and-redraw's present is
  26.3 ms at every rock count, because `clean` marks the whole canvas. That is
  half the 50 ms budget gone before a rock is drawn, and no amount of
  game-side tuning touches it. The only lever on it is *area*: the cost is
  1.26 ms per 16-pixel tile row, so a shorter playfield is a linear discount.
- **A vector game is a different animal from a sprite game on this display.**
  Galaxian's whole frame is 11.22 ms because it dirties only its sprites.
  Asteroids' present alone is 2.3× Galaxian's entire frame. Nothing is wrong
  with either number; they are what the dirty tracker does with compact and
  with scattered drawing.

### 3.4 What M0 also priced

| Unit | Assumed (§12) | **Measured** |
|---|---:|---:|
| drawing statement (`fd 17` with the pen down) | 35 µs | **59.5–60.3 µs** |
| arithmetic statement (`make "x :x + 1`) | 48 µs | 42–44.5 µs |
| bare `repeat` iteration | 4.5 µs | 4.5–7 µs |
| 9-way shape dispatch, per rock per pass | not counted | **360–398 µs** |

The last two lines cross-check the harness: the arithmetic statement and the
bare loop land on P10 M5's numbers for the same board, so the drawing figure
is not a broken clock.

**§12's central assumption is wrong, and wrong in an instructive way.** It
bracketed a literal-argument primitive call between a procedure call (21 µs)
and an arithmetic statement (48 µs) and took the midpoint. But a pen-down
`fd 17` is not only interpretation — it *rasterises a line*, clips it and
folds it through `wrap` — so it lands **above** the arithmetic statement, at
1.4× it. The bracket was sound for a primitive that computes; it never applied
to one that draws. Every drawing line in §12 is therefore **72 % low**.

**The dispatch is the other surprise**, and it is nobody's back-of-envelope:
370 µs is about six statements, because reaching one of nine outlines is two
procedure calls and up to four `if`s, each of which evaluates an `item` on a
twelve-element list. At twelve rocks that is 4.4 ms of the 25.9 ms body — 17 %
of it, spent deciding what to draw rather than drawing it. §13 has the levers.

## 4. The playfield

- 320 × 320 turtle steps, origin at centre, `fullscreen`, `wrap`.
- **`wrap` is load-bearing, not decoration.** A rock straddling an edge has
  its segments split and continued on the far side by the engine, which is
  both the arcade behaviour and free. Object *centres* are wrapped by our own
  arithmetic (§5) and always stay in bounds, so `setx`/`sety` never asks the
  turtle to leave the field.
- The price is a present cost the row-span dirty tracker cannot avoid: a
  straddling rock dirties tiles at both ends of its tile rows, and a row's
  span is inclusive, so **each straddling rock costs its full tile rows** —
  320 × 16 px ≈ 1.26 ms each. Typically zero to two rocks straddle at once.
  This is inherent to the tracker's representation and is budgeted for in
  §12, not engineered around.
- The HUD occupies the top ~14 px and rocks pass under it (§10).

## 5. Object representation

The split is the inverse of the sprite games'. There, turtles were the actors
and the canvas held the crowd. Here **the canvas holds everything with a
shape**, and turtles are used only for the things that are a single moving
dot.

| Game object | Count | Representation | Why |
|---|---|---|---|
| Rocks | ≤12 | **canvas**, pen-drawn, one flat-list slot each | more of them than there are turtles; and a pen-drawn polygon rotates for free |
| Ship | 1 | **canvas**, pen-drawn | three strokes, rotates to any heading; a 16×16 costume under `setrot "full` would be a visibly coarser ship |
| Saucer | ≤1 | **canvas**, pen-drawn | the arcade saucer is a distinctive outline |
| Explosions | ≤2 | **canvas**, `arc 360 r` | one primitive call per ring (§9) |
| Player shots | ≤3 | **turtles 1–3** | `setspeed` flies and wraps them with no Logo per frame |
| Saucer shot | ≤1 | **turtle 4** | same |
| The pen | — | **turtle 0**, hidden | draws every canvas object and the HUD |
| — | — | turtles 5–7 spare | |

Two consequences worth stating:

- **No `when` demons at all.** Every collision in this game is between two
  things whose positions Logo already holds, so §8 is arithmetic. The demon
  table stays empty — the opposite of Galaxian, which fits in
  `MAX_DEMONS 8` with zero slack.
- **No `over?`/`colourunder` sensing either.** Canvas colour sensing is how
  Invaders finds an alien under a shot, but it cannot work here: vector rocks
  are *hollow*, and a shot moving ~8 steps a frame samples the canvas at eight
  well-spaced points, so it would fly straight through a 2-px outline most of
  the time. The tunnelling is not a tuning problem, it is the geometry.

### Rock state: eight parallel flat lists

```logo
make "rx    [0 0 0 0 0 0 0 0 0 0 0 0]    ; centre x            (float)
make "ry    [...]                        ; centre y
make "rdx   [...]                        ; velocity, steps/frame
make "rdy   [...]
make "rang  [...]                        ; current rotation, degrees
make "rspin [...]                        ; degrees/frame, may be negative
make "rsize [...]                        ; 0 free, 1 small, 2 medium, 3 large
make "rrad  [...]                        ; collision half-width, from size
```

`rrad` is the **collision half-width, not the drawn radius**: it is the drawn
radius plus `shot.reach`, for the sampling reason in §7.3. One procedure,
`rad.for`, decides it, because M2 needs the size-to-radius map in two places —
once when a level spawns a large and once when a large splits.

Fixed length `MAX.ROCKS` = 12, mutated in place with `.setitem`, never rebuilt:
a frame that moves twelve rocks must allocate nothing (§14). `rsize` = 0 is
the free-slot marker, so allocation is a scan for the first zero.

**There is no `rshape` list.** The original design gave each rock one of three
outlines per size, and M0 priced what that costs to look up: 370 µs a rock, a
fifth of everything a rock costs (§3.4). One outline per size takes the
dispatch down to a single three-way test (§13), and `rsize` already carries
it — so the field that chose an outline is not stored because there is
nothing left to choose.

Parallel flat lists rather than a list of records because `item` and
`.setitem` are the only indexing this Logo has, and a record would cost a
second level of both on every field access.

## 6. Drawing the vectors

### 6.1 Shape procedures are straight-line literals

Each rock outline is a Logo procedure of nothing but `fd`/`rt` with **literal
arguments**. No variables, no arithmetic, no `item`, no loop:

```logo
to rock.l                        ; the large outline
  pu fd 21.1  rt 112.8  pd       ; centre -> first vertex, then face the walk
  fd 16.1  rt 47.5  fd 15.8  rt 48.8  fd 14.8  rt 32.1
  fd 14.8  rt 56.1  fd 14.9  rt 47.3  fd 13.8
end
```

This shape is the performance argument, and M0 half confirmed it. Computing a
vertex would still cost more than drawing it — an arithmetic statement is
43 µs — but the margin is nothing like the one this section claimed: a
pen-down `fd 17` measures **60 µs**, *above* the arithmetic statement rather
than a fraction of it, because it rasterises a line as well as evaluating one
(§3.4). Literals are still right; they are not free.

So there are **three procedures, one per size, with the scale baked into the
literals**, rather than one that multiplies by a radius. It was nine — three
outlines at each size — until M0 priced the lookup at 370 µs a rock (§13).
(`setmag` is not an alternative either way: it scales a turtle's *appearance*
and `stamp`, and explicitly does not change how far the turtle moves.)

The `pu fd 22 rt 126 pd` prologue is what lets the rock's stored position be
its **centre** while the walk starts at a vertex — three statements, and it
keeps `rx`/`ry` meaning the same thing for physics, wrapping and collision.

### 6.2 Rotation is free

```logo
pu setx :x sety :y seth :a
```

Because the walk is entirely turtle-relative, `seth` before it rotates the
whole polygon about the stored centre. A rotating rock costs exactly what a
still one costs. This is the single largest thing turtle graphics buys over a
stamped costume, which would need one bitmap per angle.

`setx`/`sety` and not `setpos`, following Invaders and Galaxian: `setpos`
takes a list, and building one per object per frame allocates.

This was a `place :i` procedure that re-read `rx`, `ry` and `rang` out of the
lists. It is four bare statements inside the rock pass now, using the values
that pass has already computed — see §15.

### 6.3 The outlines are generated, not hand-typed

An outline is authored as eight (or six, or five) radii at equal angular
spacing around the centre; [`scripts/gen_rocks.py`](../scripts/gen_rocks.py)
converts that to the turtle walk — segment lengths and exterior turns, which
close the polygon to within a pixel. Hand-written turns do not close, and an
unclosed rock leaves a gap that looks broken. The script's output is pasted
into the game file; it is not run on the board, and `test_asteroids.c` walks
every outline and checks each one arrives back at the vertex it started from,
which is the one property of a pasted-in block of literals that a bad paste
would break.

The numbers carry one decimal place. That is still a single literal token, so
it evaluates at exactly the cost of an integer, and it is what holds the
closure error under half a pixel — integer turns alone drift by two or three
across eight segments. There is **no turn after the final segment**: `place`
sets the heading before every pass, so it would never be read, and dropping it
is what makes the statement counts below 15/13/11 rather than 16/14/12.

Segment counts fall with size, which is both authentic and exactly the right
place to spend the saving, since small rocks are the numerous ones. **M0 took
them down a notch** (§13): at 60 µs a drawing statement rather than the
assumed 35, the counts M0 measured — 8/6/5, at 19/15/13 statements — cost
1.9 ms more a frame at twelve rocks than the game can afford.

| Size | Segments | Radius | Statements per draw |
|---|---:|---:|---:|
| Large | 6 | 22 | 15 |
| Medium | 5 | 14 | 13 |
| Small | 4 | 8 | 11 |

Four segments is the floor: three would be a triangle and read as a shard
rather than a rock. If M1 finds room, the large rock goes back to eight
before anything else does.

### 6.4 The ship

A notched triangle — nose, right rear, notch, left rear — and it comes off the
same generator as the rocks, because it is the same problem: a closed polygon
whose first vertex sits straight ahead of the turtle, so `walk` and the closure
check apply unchanged. The walk this section originally carried was hand
written and did **not** close; it landed two steps from the nose and then drew
a fifth stroke off into space.

**The flame is folded into the same closed walk**, not drawn as a shape of its
own. Drawing it separately would need a second `pu setx sety seth` to get back
to the ship's centre — four statements, which is most of what the flame costs
to draw at all — so there are two generated outlines and a thrusting ship costs
exactly one dispatch and one placement, like a still one.

| Outline | Segments | Statements |
|---|---:|---:|
| `ship` | 4 | 11 |
| `ship.flame` | 6 | 15 |

**Scaled to 0.85 after the first play report, then put back** — and the reason
is the useful part. A smaller ship reads tidier, which is what the report asked
for; seen next to the rocks it also **makes the game easier**, because the ship
is the one thing on the playfield the rocks have to hit. Full size is the harder
game and the one that ships. The ship is about the size of a **medium rock**,
and `test_the_ship_is_smaller_than_a_large_rock` holds the size class rather
than the walk: bigger than a small, under a medium's radius.

```logo
to draw.ship
  pu setx :shipx sety :shipy seth :sh
  setpc :ship.colour
  if not :thrusting [ship stop]
  if 0 = remainder :frame.count 2 [ship stop]
  ship.flame
end
```

The flame alternates on and off every other frame, as the arcade one does: a
held thrust key would otherwise draw a steady cone, which reads as a nozzle
rather than a burn. The common path is three statements.

## 7. Motion and input

### 7.1 Rocks

Constant velocity, wrapped:

```logo
make "x wrapc ((item repcount :rx) + (item repcount :rdx))
make "y wrapc ((item repcount :ry) + (item repcount :rdy))
make "a (item repcount :rang) + (item repcount :rspin)
.setitem repcount :rx :x
.setitem repcount :ry :y
.setitem repcount :rang :a

to wrapc :v
  if :v > 160 [output (:v - 320)]
  if :v < -160 [output (:v + 320)]
  output :v
end
```

The new values go into locals first and the list second, because the collision
test and the placement both want them and neither should walk the list again
(§15). This was a `step.rock :i` procedure in its own loop.

`wrapc` is two comparisons rather than `modulo` arithmetic because a rock
crosses an edge on perhaps one frame in fifty, and the common path should be
two failed tests. Note the parentheses: this Logo has no `>=`/`<=`, and a
call's last argument greedily absorbs trailing infix, so every argument
expression here is parenthesised deliberately.

### 7.2 The ship keeps its momentum

Velocity is state, thrust is an impulse, and there is no friction term (the
arcade has a very slight drag; leaving it out is one fewer statement a frame
and plays nearly the same):

```logo
to thrust
  local "spd
  make "thrusting true
  make "svx :svx + (:thrust.imp * sin :sh)
  make "svy :svy + (:thrust.imp * cos :sh)
  make "spd sqrt ((:svx * :svx) + (:svy * :svy))
  if :spd > :speed.max [
    make "svx :svx * :speed.max / :spd
    make "svy :svy * :speed.max / :spd
  ]
end
```

The speed clamp is what stops the ship becoming unplayable, and it is the
first constant to tune on hardware. `sin`/`cos` take degrees and Logo's
heading is clockwise-from-north, which is what `seth` wants — so the ship's
heading variable and its drawing heading are the same number, with no
conversion anywhere.

As built: `thrust.imp` 0.4 and `speed.max` 4.5 steps a frame — about 68 steps
a second, against a shot's 160. That **ratio is the constraint the numbers were
picked to**, not the absolute speeds: a ship that closes on its own shots makes
firing forward useless, and the arcade keeps the shot at three or four times
the ship's top speed. The clamp is on the magnitude, not per component, which
is why the test checks it on a diagonal as well as on an axis.

### 7.3 Input

PicoCalc key codes, following the two shipped shooters:

| Key | Code | Action |
|---|---:|---|
| ← | 180 | rotate left 16° |
| → | 183 | rotate right 16° |
| ↑ | 181 | thrust |
| space | 32 | fire |
| ↓ | 182 | hyperspace |
| p | 112 | pause |
| q | 113 | quit to the attract screen |

**Every per-frame constant in this design was cut for 20 fps and has been
re-cut by a third for 15** — the rotation step here, the thrust impulse and
speed clamp in §7.2, shot speed and life, the saucer's jink. 16° a frame is
240°/s, which is what 12° at 20 fps was. They are all first-tune-on-hardware
constants (§18) and none of them is knowable from the host; the point of
re-cutting them now is that iteration should start from the right order of
magnitude rather than from a game that steers a third too slowly.

One `readchar` per frame, as in Invaders — the keyboard ring buffers, so a
held key repeats and rotation is smooth. `poll.input` runs *outside* the
paused guard so `p` can be read while paused.

Firing takes the lowest idle shot turtle: `seth :sh`, place it at the ship's
nose, `pu`, `st`, `setspeed`. From then on the engine flies and wraps it
— `setspeed` "obeys `wrap`, `window` and `fence` exactly as `forward` would"
— and Logo's only per-frame duty is counting the shot's life down. `pu` is
mandatory: a shot turtle with its pen down would draw a permanent trail
across the canvas. The turtle wears a two-pixel dot from `putsh`, because the
default line-drawn turtle reads as a second spaceship.

#### A shot must not outrun a rock's collision box

`setspeed` is the one quantity in this game that is **per second**, because the
engine flies the shot on wall-clock time while Logo does something else. That
is the hazard P10's log flagged for Galaxian arriving from the other direction,
and here it is not a matter of feel — it has a bound.

Collisions are sampled once a frame (§8), so a shot travelling further between
samples than the full *width* of a rock's box can pass through it and be seen
on neither side. Two things eat into that width and neither is a constant: the
rock is moving too — a small rock that came from a split of a split drifts over
twice as fast as a spawned large — and a frame that overruns its budget moves
the shot **further**, not less far. So the bound is asserted at half the true
one, which absorbs both with room:

> **(shot travel + fastest rock) × overrun ≤ 2 × the smallest collision
> half-width** — (160/14 + 2.46) × 1.3 = 18.1 against 20.

Every term is a constant in the game file. The fastest rock is a small one from
a split of a split: a child leaves at `sqrt(split.boost² + kick²)` times its
parent and the kick reaches 1, so two splits multiply `speed.l` by
`split.boost² + 1`. The overrun allowance covers a frame that misses its budget,
which moves the shot **further**, not less far.

`shot.speed` is the one constant the rate does **not** re-cut, because it is
already per second — but `fps` is in the denominator, so a rate change still has
to be checked against this.

**This bound was first written at half its true value**, as `travel ≤ rrad`.
That is safe, and it is blunt, and bluntness has a price a player pays: it
demanded `shot.reach` = 4, which made the game award visible misses as hits —
reported from the board, and **worst on the smallest rocks**, because a flat
number added to radii of 22, 14 and 8 is proportionally largest on the 8. Sized
properly the smallest box is **10 rather than 12** and no shot speed had to
change. A conservative bound is not free; it spends somewhere else.

`shot.reach` is what buys it: **2 steps** added to every rock's radius, so the
smallest box is 10 rather than 8. The generosity that costs is a couple of steps
around an outline that already jags in and out by a quarter of its own radius —
the same "close enough, and stated" reasoning as the square hit test itself, and
`test_the_collision_boxes_are_not_far_wider_than_the_rocks_drawn_in_them` now
holds the excess under 30 % at every size so the next change to this constant
has to answer for how it looks as well as whether it is safe.

`test_a_shot_cannot_outrun_the_smallest_collision_box` pins the arithmetic, and
it caught one constant set already: at `shot.speed` 200 with `shot.reach` 5, a
small rock was 0.7 steps a frame away from being shot-proof. Nothing else in
the build would have shown that — it is not a crash, it is a shot that
occasionally passes through a rock, which reads as bad aim.

## 8. Collisions and scoring

Every test is a box overlap on numbers Logo already holds, written as nested
`if`s with a cheap reject first, so the common case is one comparison. `:r` goes
on the **left** of every test, following this file's rule about trailing infix:
written the other way round each `abs` needs a parenthesis around it, and
parentheses are not free here.

A square test, not a circle: it is one statement instead of a squared-
distance expression, and against a jagged rock whose outline is nowhere near
its bounding circle anyway, the extra reach in the corners is not perceptible.
This is the same "close enough, and stated" reasoning as Invaders' ±10 bitmap
anchor tolerance.

The pair counts are the frame's second-largest line item (§12):

| Pair | Where | Worst-case tests/frame |
|---|---|---:|
| shot × rock | inside the rock pass, three shots unrolled | 36 |
| ship × rock | same loop, one more test | 12 |
| shot × saucer | once per shot | 3 |
| ship × saucer, ship × saucer shot | once each | 2 |

Shot positions are read once a frame with `ask :n [xcor]` / `[ycor]` into a
flat list, never re-read inside the rock loop.

**As built, there is no collision loop at all.** M2's board run measured a
shots-outside-rocks-inside pass at 20.95 ms against 7.5 estimated, and the three
`item` walks per pair were most of it (§12). The test now lives inside the rock
pass that already holds each rock's position (§15), the three shots are unrolled
against six plain variables, and `hit?` is inlined:

```logo
to shot.on :x :y :r
  if :r > abs (:x - :s1x) [if :r > abs (:y - :s1y) [output 1]]
  if :r > abs (:x - :s2x) [if :r > abs (:y - :s2y) [output 2]]
  if :r > abs (:x - :s3x) [if :r > abs (:y - :s3y) [output 3]]
  output 0
end
```

A pair is one comparison in the common case rather than a procedure call and
three list walks. It outputs *which* shot hit, because the caller has to kill
it.

**An idle shot is parked at x = 9999 rather than guarded by an `if`.** The x
test then turns it away as part of a comparison that was going to run anyway —
one statement a pair instead of two — and, more importantly, it is what stops a
shot that kills a rock mid-pass from going on to kill a second one in the same
frame. `kill.shot` parks it, which is why that procedure names each shot.

**Scoring** is the arcade table, which is already small:

| Target | Points |
|---|---:|
| Large rock | 20 |
| Medium rock | 50 |
| Small rock | 100 |
| Large saucer | 200 |
| Small saucer | 1000 |

An extra ship every 10,000 points.

### Splitting, and the slot cap

A large rock yields two medium, a medium two small, a small nothing. Children
inherit the parent's velocity plus a random perpendicular kick, and get a
faster drift and a new spin.

Three starting rocks that are all shot down as larges before any medium is
touched would need 3 → 6 → 12 slots; add a level's extra rocks and the tree
overflows. Rather than tracking it, the rule is blunt and stated:

> **A split fills as many free slots as there are.** Two if two are free, one
> if one is, none if the board is full — and a child that cannot be placed is
> simply not created.

With `MAX.ROCKS` = 12 the cap can only bind if a player deliberately clears
every large before touching a medium, and even then it costs them nothing but
targets. It is also the hard ceiling that makes §12's worst case a real
bound rather than an estimate.

## 9. Saucer, explosions, hyperspace

- **Saucer.** Appears on a countdown that shortens with the level, enters
  from a random edge at a random height, and crosses horizontally with an
  occasional vertical jink. Pen-drawn (two trapezoids, 8 segments), erased
  and redrawn like a rock. Large saucer fires in a random direction; small
  saucer fires at the ship's current position, which is what makes it
  frightening. It leaves when it reaches the far edge.
- **Explosions.** The arcade shatters a rock into drifting line fragments. A
  fragment system would cost more per frame than the rocks do, so instead an
  explosion is an **expanding ring**: `arc 360 :r` at the death point, radius
  growing over four frames, erased and redrawn each frame. One primitive call
  per frame per explosion, two explosions live at once, and it reads correctly
  at speed. Ship death gets a bigger, slower ring and a two-second freeze.
- **Hyperspace.** Erase the ship, set position to a random point, zero the
  velocity, redraw. The arcade's chance of materialising inside a rock is
  kept — it is the mechanic's entire point — but a 1-in-8 flat chance rather
  than the original's velocity-dependent formula.

## 10. HUD via `write`

Per the brief, the score and game info are `write`n on the graphics screen at
the top, not stamped or sprited.

Invaders repaints its HUD only when a value changes, and erases the stale line
by writing it again in the background colour. **This game does neither, and
M0's verdict is why.** `clean` wipes the HUD along with the rocks every frame,
so there is never a stale line to erase and never a frame where the repaint
can be skipped:

```logo
to draw.hud
  pu setx -155 sety 148 setpc :hud.colour  write :hud.text
end
```

Five statements, every frame, and **no allocation at all** — the `sentence`
is built only where a displayed value changes (a kill, a death, a level), in
the code that changes it. Lives are drawn as that many `^` characters in the
same string rather than as ship glyphs, which keeps the whole HUD one `write`.

What this replaces is worth recording, because it was the more intricate half
of the original design: under erase-in-place the HUD had to be redrawn *last*
in every frame, after every erase, since rocks wrap through the top band and a
rock's erase pass would otherwise punch holes through the glyphs. That
ordering constraint is gone with the erase pass.

## 11. Sound

The P8 stereo PSG, one centred voice-pair per sound, timbres set once in
`setup.sound` because `stopsound` preserves them — the Invaders/Galaxian
arrangement:

| Sound | Voices | Timbre | Trigger |
|---|---|---|---|
| Heartbeat | `[0 4]` | triangle, percussive | the two alternating low notes, whose interval shortens as the rock count falls — the game's signature |
| Fire | `[1 5]` | sawtooth zap | a shot launches |
| Thrust | `[2 6]` | white noise, **sustained** | held while thrust is held, gated off when released |
| Explosions / saucer | `[3 7]` | noise, and square for the saucer's warble | rock death, ship death, saucer present |

The heartbeat is the retrofit's payoff, the same way the dive shriek was
Galaxian's: it is a *tempo*, not a note, so it needs a voice that keeps
sounding while the frame loop gets on with its work. The blocking `toot` this
project started with could not have produced it.

## 12. Frame budget

Rebuilt on M0's measurements (§3.3, §3.4). Plus 2 W with P10's `LOGO_HOT`;
**bold rows are measured on the board by `p11rocks`**, the rest are P10 M5's
and hardware-notes'.

| Unit | Cost |
|---|---:|
| bare `repeat` iteration | **4.5 µs** |
| user procedure call | 21 µs |
| arithmetic statement (`make "x :x + 1`) | **43 µs** |
| **drawing statement (`fd 17`, pen down)** | **60 µs** |
| **9-way shape dispatch, per rock** | **370 µs** |
| **`place` — `pu setx sety seth`, three `item` walks** | **~770 µs** |
| **full-screen present (`clean` + `refresh`)** | **26.3 ms** |
| one 16 × 320 tile row presented | 1.26 ms |

Everything below is clear-and-redraw, one drawing pass. The per-rock cost
decomposes from M0's `body(n) = 2.10n + 0.7 ms`:

| Per rock, one pass | ms |
|---|---:|
| the outline itself, 15.7 statements average | 0.94 |
| `place` | 0.77 |
| shape dispatch | 0.37 |
| loop and call overhead | 0.02 |
| **total** | **2.10** |

Two thirds of a rock is spent getting to the drawing rather than drawing.
That is where the levers are, and it is not where §12 originally looked.

### The measured frame (M1, 2026-08-11)

**A real `play.frame` on a Plus 2 W**, 300 frames a point, measured by
`logo/tests/p11m1` — **which no longer exists.** M2's fusion (§15) removed the
procedures it called by name, so these numbers are archival: they are the last
record of a rocks-only frame and cannot be reproduced against the game as it
stands. `logo/tests/p11m2` measures the frame the game actually runs.

| rocks | body | present | **frame** | min | max |
|---:|---:|---:|---:|---:|---:|
| 6 | 19.13 | 26.41 | **45.54** | 42 | 51 |
| 9 | 26.58 | 26.46 | **53.04** | 48 | 62 |
| 12 | 34.17 | 26.37 | **60.53** | 56 | 67 |

Almost perfectly linear, which is what a frame made of one loop over one list
should be:

> **body = 4.09 + 2.507 n ms**, and so **frame = 30.5 + 2.507 n ms.**

The fit predicts the nine-rock body at 26.65 against 26.58 measured. **A rock
costs 2.51 ms all-in** — drawing, `place`, dispatch, physics and its share of
the loop — against the 2.10 ms M0 measured for drawing alone at nine
outlines and 8/6/5 segments, so physics and the slot scan are about 0.9 ms a
rock and §13's two savings took back about 0.5.

**M1 fits, with room.** Twelve rocks is 60.5 ms against 66.7, and the budget
would carry 14 rocks. The worst frame observed is **67 ms** — 0.3 ms over, one
frame in 300, and it is a recycle frame (§14's 1.3 ms) landing on an already
heavy one. That is close enough to the line to confirm 15 fps was the right
call for a rocks-only frame and to rule out 20. **M2 took it to 14** once the
ship and the shots were in it.

The present held at **26.4 ms at every rock count**, reproducing M0's 26.3 on
a different day through a different code path. It is a floor, not a variable.

### What this says about M2, which is the problem

M1 carries rocks and nothing else. §12's estimates for what M2 adds —
collisions at 6.5 ms and ship, shots and HUD at 4.0 — were built on the same
model that under-predicted the rocks-only body by 16 % (29.4 estimated against
34.17 measured). Scaling them by that error and adding:

| | ms |
|---|---:|
| measured 12-rock frame (M1) | 60.5 |
| collisions, 36 + 12 tests | ~7.5 |
| ship, shots, HUD | ~4.6 |
| **projected M2 frame** | **~72.6** |

**That is 6 ms over budget**, and M3's saucer and M4's sound are still to
come. So M2 opened with a decision rather than a keyboard, and the levers were
priced against a measured slope of 2.507 ms a rock:

| Lever | Saving |
|---|---:|
| `MAX.ROCKS` 12 → 10 | **5.0 ms** |
| segregate rocks into three per-size lists (kills the dispatch) | ~2.2 ms |
| cap shots at 2 rather than 3 | ~2.5 ms |
| large rock 6 → 5 segments | ~1.4 ms |

Any two of the first three clear it. **The first is the one that costs the
game something** — ten slots makes the split cap bind more often — so it is
the last to reach for, not the first.

### The decision, taken 2026-08-11: measure first, spend after

**No lever was spent.** The 72.6 ms above is an estimate scaled from a model
that under-predicted M1's own body by 16 %, and every estimate this design has
made has been wrong by more than its own margin: the drawing statement (35 µs
assumed, 60 measured), the present (9–12 ms guessed, 26.3 measured), the reclaim
interval (250 frames copied, 25 needed). Spending two levers — and the cheapest
of them takes away the board filling up, which is what Asteroids *is* — against
a third such number would have traded real gameplay for arithmetic nobody had
checked.

So M2 was built whole, and `logo/tests/p11m2` measured it.

### The measured M2 frame, and it is not 6 ms over

**300 frames a point on a Plus 2 W, three shots live on every frame** — the
worst case, held there by parking the rocks' collision boxes at zero so nothing
splits and no shot is consumed:

| rocks | body | present | **frame** | min | max | of which collisions |
|---:|---:|---:|---:|---:|---:|---:|
| 6 | 36.63 | 26.53 | **63.15** | 59 | 72 | 12.95 |
| 9 | 48.42 | 26.51 | **74.92** | 70 | 84 | 16.94 |
| 12 | 59.89 | 26.54 | **86.43** | 80 | 93 | 20.95 |

> **frame = 39.9 + 3.877 n ms.** 15 fps held to **six rocks**, not twelve.

**19.7 ms over, not 6** — and the miss is not spread out. The ship, shots and
HUD came in at **4.8 ms against 4.6 estimated**, which is the part this document
guessed correctly. The entire overrun is the collision pass: **20.95 ms against
7.5**, nearly 3×.

### The cause was `item`, which nothing had costed

The two measured coefficients pin it. Collisions fit `4.95 + 1.333 n`, and both
terms fall out of one number:

| | |
|---|---:|
| an `item` walk into a 12-element flat list | **~115 µs** |
| an arithmetic statement (M0) | 43 µs |

- the **4.95 ms intercept** is 3 shots × 12 slot scans × 137 µs — the inner
  loop ran all twelve slots whatever `n` was
- the **1.333 ms slope** is 456 µs per pair: three `item` walks (`rx`, `ry`,
  `rrad`) plus the `hit?` call plus a comparison

At twelve rocks the three-pass frame did roughly **290 `item` walks — a third
of the whole frame** — and about 130 of them re-read a value another pass had
already read that same frame. §12 counted statements from the beginning; it
never counted list indexing, and list indexing is what this interpreter charges
most for outside of drawing.

### The levers, re-priced — and they are not enough

| Lever | designed | **measured** |
|---|---:|---:|
| `MAX.ROCKS` 12 → 10 | 5.0 | **~9.2** |
| cap shots at 2 | 2.5 | **~7.0** |
| large rock 6 → 5 segments | 1.4 | ~1.4 |

**All three together are ~17.6 ms against a 19.7 ms deficit.** Spending every
priced lever — cutting the game to ten rocks and two shots, which is most of
what makes it Asteroids — still would not have reached 15 fps. That is the
finding that decided M2: the problem was never which levers to spend.

### What was done instead: one pass over the rocks

The fix matches the cause. `step.all`, `check.hits` and `draw.all` are now a
single `step.draw.all` that visits each rock once (§15), so every field is read
once — **eight `item` walks a rock instead of twenty-three** — and the shot
positions live in six plain variables rather than a list, since the rock pass
reads them 36 times a frame.

### Measured again, and the fusion beat its estimate

| rocks | body | present | **frame** | min | max | rock pass |
|---:|---:|---:|---:|---:|---:|---:|
| 6 | 24.59 | 26.54 | **51.14** | 48 | 56 | 18.87 |
| 9 | 33.21 | 26.49 | **59.70** | 56 | 67 | 27.51 |
| 12 | 41.88 | 26.38 | **68.26** | 63 | 74 | 36.05 |

> **frame = 34.0 + 2.853 n ms**, down from `39.9 + 3.877 n`.

**18.2 ms saved at twelve rocks** against the ~15 estimated, and the slope fell
by 1.02 ms a rock. The collision cost went from 20.95 ms to about 7.8 — which
finally lands near the 7.5 this section guessed before anyone had costed `item`.

The rock pass fits `1.69 + 2.863 n`, so essentially the whole per-rock slope is
inside it. Everything else — ship, shot ageing, HUD, `clean`, amortised
recycle — is a flat **5.75 ms** at all three points. `shot.on` is **0.53 ms a
rock**, 18 % of the pass; thrust **0.9 ms**; a recycle **2.0 ms**. The present
held at 26.4 for the **fourth** independent time.

### The harness over-measures, and by how much

Its board is all **large** rocks, and that board cannot occur in play: a level
spawns `level.rocks` larges, and twelve rocks is only reachable by splitting
down. In M2, with three starting larges, a twelve-rock board is twelve *smalls*
— 11 drawing statements each against a large's 15, so **2.9 ms cheaper**. Once
levels advance to five starting larges the worst reachable twelve-rock board is
8 mediums + 4 smalls, **1.9 ms cheaper**.

The pessimism is kept deliberately: it is reproducible, it compares with M0 and
M1, and it errs on the side that does not flatter the budget. But the real
worst frame is **1.9–2.9 ms below the table**, and that is the number the rate
was chosen against.

### The rate: 15 → 14 fps

Twelve rocks at 68.3 ms against a 66.7 ms budget is 1.6 over on the harness's
board and about level on a reachable one — a fit with **nothing left for M3's
explosions or M4's saucer and sound**. Closing it at 15 fps meant capping the
shots at two (~2.1 ms), the one remaining lever that changes how the game
*plays* rather than how it looks.

**14 fps costs 7 % of the frame rate; two shots costs a third of them.** A
71.4 ms budget keeps twelve rocks, three shots and six-segment larges, puts the
worst observed frame inside the budget rather than at it, and leaves about 5 ms
for the milestones still to come.

It is settled **now** rather than after M3 and M4, because every per-frame
constant is the rate's arithmetic on a per-second quantity the player actually
feels — and deciding later would invalidate their tuning as well as this
section's (§18). `fps` is a single constant at the top of the game file;
`play.level` asks `sync` for it, and
`test_the_per_frame_constants_are_cut_from_the_frame_rate` checks the
per-second quantities rather than the per-frame ones, so the constant that gets
missed on the next move fails a test instead of shipping.

The constants that moved with it: drift 0.9 → 0.96, spin 2.5 → 2.7, turn 16 →
17, thrust 0.4 → 0.43, clamp 4.5 → 4.8, shot life 18 → 17. `shot.speed` did
**not** move, because it is per second — and the tunnelling bound it lives
under (§7.3) had to be re-checked against the new rate: 160/14 = 11.4 against a
12-step half-width, still inside.

One of them was a trap. **`spin.max` was declared and never read** — the
spawner used a literal `/ 8` — so it would have been re-cut with the rest and
changed nothing. It is wired now.

### One saving taken, one not

**`wrapc` is spelled out inside the rock pass** rather than called: a user
procedure call plus an `output` on top of two comparisons, 24 times a frame.
That is the one duplication in the game file — the ship still calls the
procedure — and the rock test drives the inlined copy against `wrapc` at all
four edges so the two cannot drift apart unnoticed.

**Not taken, and still priced:** `MAX.ROCKS` 12 → 11 (~2.7 ms), cap shots at 2
(~1.7 ms), large rock 6 → 5 segments (~1.4 ms). All three remain available if
M3 or M4 need them, and all three are now priced against a measured slope
rather than a model.

## 12a. M2 accepted: the frame at 14 fps

**300 frames a point on a Plus 2 W**, the same worst case — twelve rocks with
three shots live on every frame:

| rocks | body | present | **frame** | min | max | rock pass |
|---:|---:|---:|---:|---:|---:|---:|
| 6 | 23.13 | 26.55 | **49.68** | 46 | 55 | 17.45 |
| 9 | 31.13 | 26.46 | **57.58** | 53 | 62 | 25.32 |
| 12 | 39.01 | 26.43 | **65.44** | 61 | 70 | 33.17 |

> **frame = 33.9 + 2.627 n ms**, against a **71.4 ms** budget.

**Twelve rocks fit with 6.0 ms to spare, and — for the first time in this
design — the worst observed frame meets the budget with room**: 70 ms against
71.4. M1's worst frame was 67 against 66.7, *over*; M2's first run was 93
against 66.7. A rate the worst frame meets is the only rate that can be trusted
without a per-frame profiler (§18), and this is the first time that has been
true.

Correcting for the harness's unreachable all-large board (−2.9 ms), a real
twelve-rock frame is about **62.5 ms — roughly 9 ms of headroom** for M3 and M4.

**It also confirms 14 was the right call rather than a cautious one.** At a
66.7 ms budget the mean would now fit by 1.3 ms, but the worst frame is 70 and
would not — so 15 fps was never available at twelve rocks and three shots, and
buying it would still have cost the shot cap.

### The `wrapc` inline paid double

Estimated 1.4 ms; measured **2.9**. The rock-pass slope fell from 2.863 to
2.620 ms a rock, and the frame slope from 2.853 to 2.627.

The reason is a number worth keeping: **a user procedure call that `output`s
costs about as much as an `item` walk — roughly 110 µs**, not the ~60 assumed.
Two of them a rock is 0.23 ms, and it is the same lesson as §12's, one level
down — the interpreter charges most for the things that are not statements.

One caveat on the attribution: `shot.on` reported 0.42 ms against 0.53 in the
previous run, and its code did not change. That figure is scene-dependent —
whether the sampled shots reject on the first comparison or run the second
depends on where they happen to be — so some of the 2.9 ms may be that variance
rather than `wrapc`. The frame total is what was measured; the split between
those two is not exact.

**And the storage question from the previous run is closed.** Free storage went
25,753 → 25,749 → 25,749: flat across the whole run, and the recycle recovered
nothing because there was nothing to recover. The 808-cell drop the run before
did not reproduce, which fits it having been one-off workspace state rather than
anything the frame does.

A recycle is **2.2 ms** and thrust **0.86 ms**. The present held at **26.4 ms**
at every rock count for the **fifth** independent time — 37 % of the frame, and
still the number no game-side lever reaches.



## 13. Reduced-resource choices

| Arcade Asteroids | This port | Saving |
|---|---|---|
| up to 27 rocks on screen | `MAX.ROCKS` 12, splits fill free slots only | bounds the two largest frame costs |
| 4 shots, generous range | 3 shots, ~1.2 s life | a third off the collision pair count |
| 12-vertex rocks at every size | **6 / 5 / 4** by size | 1.9 ms at twelve rocks; the small ones are the numerous ones |
| three rock outlines at every size | **one outline per size** | 2.2 ms at twelve rocks — the dispatch collapses to a single three-way test in one procedure, with no second call. The nine generated outlines stay in the file; rotation already varies how a rock reads in motion, and the variety comes back the moment M1's measurements say it can be afforded |
| 60 fps | **14 fps** | the present is 26.4 ms whatever the scene holds; 15 after M0, 14 after M2 (§12) |
| rocks shatter into drifting line fragments | 4-frame expanding `arc` ring | one primitive per frame instead of a particle system |
| both saucer sizes can coexist | one saucer at a time | one object, one collision pair |
| velocity-scaled hyperspace risk | flat 1-in-8 | no formula |
| ship has slight drag | none, plus a hard speed clamp | one statement a frame |
| 4 starting rocks, +2 a level to 11 | 3 starting, +1 a level to 5 | keeps the mid-level rock count near the typical-frame budget |

None of these touch the interpreter. Like the other three games, this is pure
Logo against primitives that already exist.

## 14. Memory

**The Galaxian rule does not apply, and M1 found out why.** This design opened
by asserting it — *a frame must allocate nothing*, as the three shipped games
measure — and that is not a rule, it is a property of what those games happen
to store.

`.setitem` of a **number** interns it as a word atom
(`member_value_to_node`, [core/primitives_words_lists.c](../core/primitives_words_lists.c)).
The shipped games measure zero a frame because the values they write back come
out of other lists already interned, or are drawn from a handful of distinct
constants. Continuous physics has neither property: every rock's new x, y and
angle is a value nothing has held before, so **a twelve-rock frame mints 36
atoms**, ~9,000 between reclaims.

Measured on the mock over 2,000 frames at twelve rocks: the working set
settles at **~2,950 cells and stays there**, varying by ±60 between 250-frame
blocks with no trend. So the contract is a steady state, not a zero, and the
collector keeps up with it.

**Flatness was the wrong property to test, and a board proved it.** With
`reclaim` every 250 frames — Galaxian's interval, copied — the game ran out of
storage within a minute of play on a Plus 2 W: `Out of space in step.rock`.
The host soak had passed, because the host is not where the margin is thin.

The property that matters is a **deadline**, not a rate, because **nothing in
this interpreter collects on demand**: `alloc_cell` and `mem_atom`
([core/memory.c](../core/memory.c)) report out of space rather than collecting
and retrying. That is the right call for a frame loop — no unpredictable
pause — but it makes the reclaim interval the game's entire safety margin
rather than a tidiness measure. Measured with `reclaim` disabled, the frame
loop runs **649 frames** before it dies. So 250 was a 2.6× margin, and 2.6×
is not a margin: the atom region is capped at 32 KB *or* wherever the node
pool's floor has reached, whichever is lower, so a board carrying a fuller
workspace has less atom room than the host measuring it.

**`reclaim.every` is 25 frames** — a 26× margin against the same measurement,
and 1.8 s at 14 fps. `test_the_reclaim_interval_stays_inside_the_atom_budget`
measures the deadline rather than assuming it and fails if the interval creeps
back towards it.

**A recycle costs 2.0 ms** at M2, measured on a Plus 2 W (it was 1.3 at M1 —
the frame allocates more now), so recycling ten times more often than the design
first said costs 0.08 ms a frame amortised and puts a 2 ms bump inside a 71.4 ms
budget every 1.8 s. It is invisible, and the tighter interval is free. The frame
loop holds flat: **4 cells over 1,000 frames** on the host.

The rule to write to is **"a frame must not allocate anything it does not hand
back, and must hand it back long before the deadline"** — no `sentence`, no
`list`, no `fput` on the frame path. The HUD text is rebuilt only where a
displayed value changes, which at M2 is level setup and a kill; `wrapc` outputs
a number rather than a cell. `test_asteroids.c` pins both the deadline and the
flatness of the working set over 1,250 frames.

**M2 leaves the per-frame count where M1 found it, and the reason is worth
knowing.** The ship and the sampled shot positions are plain `make` on globals,
and it is `.setitem` into a *list* that interns a number as a word atom — so
the same physics costs nothing when it is held in a variable and two atoms a
frame when it is held in a list. Only the twelve rocks are in lists, so the
worst frame still mints 36.

That is the same fact from two directions: holding the shot positions in a list
would have cost atoms *and* 4 ms a frame to index (§15). A list is the right
shape for twelve rocks that need a free-slot scan and the wrong shape for three
shots that are only ever read. It will decide how the saucer stores its position
at M4.

## 15. Main loop

**One pass over the rocks, not three.** This is the largest change any
measurement has forced on this document, and §12's re-priced table is why.

```logo
to play.frame
  poll.input
  if not :paused [
    make "frame.count :frame.count + 1
    step.ship              ; momentum; thrust is an impulse from poll.input
    step.shots             ; life countdown, and read the turtles back
    clean                  ; buffered in sync mode since B16 (§3.1.1)
    step.draw.all          ; step, test and draw each rock, in one visit
    draw.ship
    draw.hud               ; the clean took it too, so it goes back every frame
    reclaim
  ]
  sync
end

to step.draw.all
  local "s local "x local "y local "a local "h
  setpc :rock.colour
  repeat :max.rocks [
    make "s item repcount :rsize
    if 0 < :s [
      make "x wrapc ((item repcount :rx) + (item repcount :rdx))
      make "y wrapc ((item repcount :ry) + (item repcount :rdy))
      make "a (item repcount :rang) + (item repcount :rspin)
      .setitem repcount :rx :x
      .setitem repcount :ry :y
      .setitem repcount :rang :a
      make "h shot.on :x :y (item repcount :rrad)
      ifelse 0 < :h [kill.shot :h  split.rock repcount]
                    [pu setx :x sety :y seth :a  draw.rock :s]
    ]
  ]
end
```

### Why it is one loop

The design specified three passes — `step.all`, `check.hits`, `draw.all` —
because that is how the game reads. M2's board run measured what it costs.
**An `item` walk into a twelve-element flat list is about 115 µs**, two and a
half times an arithmetic statement, and the three-pass frame did roughly **290
of them: a third of the whole frame**. About 130 re-read a value another pass
had already read that same frame — `check.hits` re-walking the `rsize` that
`step.all` and `draw.all` had both walked, and the `rx`/`ry` that `step.all`
had in hand when it wrote them.

Fused, every field is read once: **eight `item` walks a rock instead of
twenty-three.** The step computes the new x, y and angle into locals, the
collision test uses them, and the placement uses them again.

Two ordering consequences, both one frame long and neither visible at 14 fps:

- A rock that dies is **skipped rather than drawn**, which is exactly what a
  separate collision pass running before the drawing pass bought.
- A child landing in a slot **below** the one being processed appears next
  frame; one landing **above** is stepped and drawn in this one.

And one ordering that is load-bearing: **`step.shots` before the rock pass**,
because the pass tests against the sampled shot positions and they have to be
this frame's. `clean` moves to *before* the pass rather than between passes,
since the pass draws as well as steps.

### What this cost, and what it did not

It cost the readability the three-pass shape had: `step.draw.all` is the
biggest procedure in the file and it does three things. That is a real loss and
it is worth being honest that it is a loss.

It cost **no gameplay at all** — twelve rocks, three shots and six-segment
larges all survive, which none of §12's four priced levers would have allowed.
That is the trade, and it is only available because the levers were left unspent
until something measured them.

`play.frame` is a procedure and not the body of the `until` below, so a test or
a timing harness can call exactly what the game runs — the correction P9 M0
forced on Galaxian.

```logo
to play.level
  setup.level
  (setrefresh "sync 15)
  make "over false
  until [:over] [
    play.frame
    if 0 = :rocks.alive [make "over true]
  ]
  clear.shots
  setrefresh "auto
end
```

`clear.shots` at the end is not tidiness: a shot is a turtle the **engine** is
moving, so one still in flight when the player quits keeps gliding and keeps the
demon poll working at the prompt.

M3 adds the death handling to the loop and turns the cleared board into a level
advance; M4 adds `heartbeat` to the frame.

## 16. Milestones

**M0 — measure, before any game code. DONE, 2026-08-11**
(`logo/tests/p11rocks`, `tests/test_asteroids.c`). Two runs on a Plus 2 W,
every figure reproducing within 1 %. It rewrote §3, §10, §12, §13 and §15:
clear-and-redraw wins at every rock count the game plays at, the design's
assumed 35 µs drawing statement is 60, the dispatch nobody costed is 370 µs a
rock, and the frame rate comes down from 20 fps to 15. §3.3 and §3.4 hold the
numbers.

Four things it deliberately did not do, each of which shapes how much its
numbers are worth: it held the scene still (a rock steps a couple of pixels
and rotates between passes, but both stay inside the same 16-pixel tiles, so
the dirty region is the same to within a tile); it presented with `refresh`
rather than `sync`, since the number wanted is the work and not the cadence;
it cleared with `clean` rather than `cs`, which would restore automatic
refresh and silently end the measurement; and it ran `fullscreen`, because
`screen_gfx_blit_dirty` returns immediately in text mode and a present
measured at the prompt is zero — the correction P9 had to make to its entire
first series of numbers.

**M1 — rocks only. DONE and accepted on hardware, 2026-08-11.** Twelve rocks
is **60.5 ms against the 66.7 ms budget**, body 34.2 and present 26.4, with
`frame = 30.5 + 2.507 n` fitting all three points (§12). It fits with room,
and it leaves **M2 about 6 ms short** — which is M2's opening decision, with
the levers priced in §12.

The build:
`logo/games/asteroids` (12 slots, three outlines, wrap, spin,
clear-and-redraw, `sync` at 15 fps, nothing to shoot with),
`tests/test_asteroids.c` (23 tests), and `logo/tests/p11m1`, which timed a
real frame at 6, 9 and 12 rocks with the body and the present read apart —
the split P9 M5 wished it had. **That harness was removed at M2** and is
described here in the past tense for that reason; `logo/tests/p11m2` replaces
it.

Two things M1 settled on the host before the board saw it. It **disproved this
document's memory rule** — an Asteroids frame does allocate, ~36 atoms a
frame at twelve rocks, and the contract is a flat working set rather than a
zero (§14). And the harness has to spell `play.frame` out again minus its
`sync`, because the present must be timed on its own and `sync` is the last
thing the frame does; `test_the_harness_frame_matches_the_game_frame` drives
both from one state and requires the same drawing and the same physics, since
a harness frame that drifts from the game measures a game nobody plays.

**M2 — ship and shots. MEASURED on hardware 2026-08-11, and it forced the
biggest structural change in this document.** Rotation, thrust, momentum, firing, shot×rock collisions, splitting,
scoring, the HUD.

It opened projected 6 ms over and **no lever was spent** on that projection; it
was built whole and measured instead. The board said **86.4 ms at twelve rocks,
19.7 over**, with the whole miss in the collision pass and the cause in `item`
rather than in anything §12 had priced — and **all three priced levers together
would not have closed it**. Fusing the three passes over the rocks into one
took it to **68.3 ms**, 18.2 saved against ~15 estimated; the rate went **15 →
14 fps**, because 7 % of the rate is cheaper than a third of the shots; and
inlining `wrapc` took another **2.9 ms**, double its estimate. **Accepted at
65.4 ms against 71.4, worst frame 70** (§12a). Twelve rocks, three shots and
six-segment larges all survive.

The build: `logo/games/asteroids` (36 procedures), `tests/test_asteroids.c`
(49 tests) and `logo/tests/p11m2`, which reads the rock pass apart from the rest
of the body and times one `shot.on` on its own — between them those price every
remaining lever. `logo/tests/p11m1` is **gone**: the fusion removed `step.all`,
`draw.all` and `place`, which it called by name, so it could no longer run
against this game at all. Its numbers live in §12's table. `p11rocks` survives
because it defines its own drawing and measures a question nothing else
reproduces.

Three things M2 settled on the host. The ship walk this document carried in
§6.4 **did not close** — hand written, it landed two steps from the nose and
then drew a fifth stroke off into space; the ship now comes off the rock
generator and gets the rocks' closure test, with the thrust flame folded into
the same closed walk so a thrusting ship costs one dispatch and one placement.
`shot.speed` has a **hard bound rather than a feel** (§7.3), and the test that
states it caught the first constant set: at 200 steps a second a shot could
pass clean through a small rock between samples. And `.setitem` of a number
still interns it, so M1's memory contract carries over unchanged — the frame
loop is soaked for flat storage and the reclaim deadline is re-measured rather
than assumed.

**M3 — lives, levels, deaths.** Ship explosion, respawn, level advance,
attract screen, game over, hyperspace. It inherits a frame that **fits with its
worst case inside the budget** (§12a) — roughly 9 ms of headroom to share with
M4 on a reachable board — and three levers still priced against a measured
slope if that runs out. M2's play report closed the last of
this document's original risks (§18): the rate reads smooth, the ship feels
right, and the two constants that played badly — the hit-box generosity and the
ship's size — are fixed and held by tests.

**M4 — saucer and sound.** Both saucer sizes; the full PSG arrangement with
the heartbeat.

**M5 — hardware pass.** 300 frames timed on a Plus 2 W in the shape of
`logo/tests/p10games`, alongside Invaders' 11.55 ms and Galaxian's 11.22 ms,
with the body/present split M5 of P9 wished it had. Then tune: rotation rate,
thrust impulse, speed clamp, shot speed, saucer accuracy.

## 17. Tests

`tests/test_asteroids.c`, mock device, mirroring `test_galaxian.c`'s split
between pure logic and driven paths.

**In, for M1** (23 tests): the eight rock lists are all `MAX.ROCKS` long;
`wrapc` at both edges, exactly on the boundary, and its one-correction
contract; all three outlines close on themselves and draw the segment count
§6.3 claims; the walk out to the first vertex does not draw, or every rock
would wear a spoke; the dispatch reaches the outline its size names; a
rotation past 360° still places, since the angle is never normalised; slot
allocation, including that a full board yields no slot and creates no rock;
wrap on a rock leaving the field, and that a free slot is never stepped; a
level never exceeds the slot count and does not inherit the last one's rocks;
a frame draws the world and nothing else; the one-pixel pen; the frame loop
holds free storage flat over 1,250 frames (§14); a paused frame neither steps
nor recycles; `P` is the only key a paused game answers; a level ends on `Q`
and hands the screen back out of `sync` mode; and the M1 harness runs end to
end, writes its file, and steps and draws exactly what `play.frame` does.

The M0 harness has its own binary, `tests/test_p11rocks.c` — both Logo files
define `place` and `draw.rock`, and the harness has to keep the shape it was
measured in.

**Added for M2** (26 more, 49 in all): both ship outlines close and draw their
segment counts; the flame appears only when thrusting and only every other
frame; the ship keeps its momentum and wraps like a rock; thrust pushes along
the heading; the speed clamp holds on an axis *and* on a diagonal, which is
what separates a clamp on the magnitude from one written per component; a ship
heading outside 0–360 still thrusts and draws, in both directions; the arrows
turn both ways; a frame with no key at all puts the flame out, which is the
thing that fails if `thrusting` is cleared after the input guard rather than
before it; firing takes the lowest idle turtle and a fourth shot is simply not
fired; a shot expires, hides its turtle and stops it; a shot flies on its own
and its position is read back, driven off the mock clock because `setspeed` is
wall-clock; `shot.on` inside and outside the square, and a parked shot hitting
nothing anywhere on the field; one shot killing exactly one rock a frame, with
the rock it killed not drawn; the split
table at all three sizes, with the children separating rather than travelling
together; a split fills the slots it finds and never writes past `MAX.ROCKS`;
three larges split all the way down into exactly twelve slots, with the score
that implies; a shot on a rock splits it, scores it and is consumed; an idle
shot hits nothing, which is what fails if the outer loop loses its guard and
starts testing a stale position; a level ends when the board is clear; the
shot-speed bound of §7.3; and the M2 harness — that it runs end to end, that it
measures the rock counts it reports, that it holds three shots live and the
board still, and that its frame matches the game's in drawing, physics, ship
and shot bookkeeping.

**Still to come with M3 and M4:**

- The 10,000-point extra ship, and level advance rather than level end.
- Ship × rock, and the death and respawn it implies.
- The saucer's pairs, and its firing.
- Allocation: the frame loop is soaked for flat storage over 1,250 frames and
  the reclaim deadline is re-measured; both carry over from M1 unchanged and
  need re-checking once the saucer allocates too.

## 18. Risks

Three of the five this document opened with are closed by M0. They are kept
with their outcomes, because two of them were closed *against* the design and
that is the useful part.

- ~~**The literal-call cost is unmeasured.**~~ **Closed, and it was wrong.**
  60 µs, not the assumed 35 — and *above* the arithmetic statement rather than
  below it, because a pen-down `fd` rasterises (§3.4). Every drawing line in
  the original §12 was 72 % low.
- ~~**Present cost in a wrap-mode vector game is unmeasured too.**~~
  **Closed, and it was wrong by more.** The guess was 9–12 ms; a 12-rock
  dirty region is 21.9 ms, and the full-screen present the shipping strategy
  actually pays is 26.3 ms every frame regardless of the scene. This is the
  single largest line item in the game and the one no game-side lever reaches.
- ~~**Erase-in-place is a new technique in this tree.**~~ **Closed by not
  doing it** (§3.3). The risk is retired along with the technique, and with it
  the ordering rule, the overlap artefact and the HUD-last constraint.
- ~~**15 fps might read as sluggish.**~~ **Closed by playing it**, 2026-08-11:
  twelve rocks drifting and spinning at 15 fps looks *smooth*, not slow, and
  the six/five/four-segment outlines read as rocks. This was the one risk no
  host test could reach and the one that could have invalidated §12's whole
  trade, since every lever that buys frame rate costs rocks. It is worth
  stating why the rate survives what a 15 fps *sprite* game would not: nothing
  here accelerates, the motion is constant-velocity drift, and a rotating
  polygon has no animation phase to stutter.
- **Feel is the rest of the game, and it is all constants** — and the frame
  rate changes all of them, which is why it is settled before M3 and M4 tune
  anything. It has moved twice: 20 → 15 at M0, 15 → 14 at M2. Rotation rate, thrust impulse, speed clamp, shot speed and life,
  small-saucer accuracy are all per-*frame* quantities against a frame that is
  now 66.7 ms rather than 50, so every one of them needs re-cutting by a third
  before it is even worth iterating on. This is the hazard flagged for
  Galaxian in P10's log (per-frame constants against per-second `setspeed`
  motion) arriving from the other direction. Isolated at the top of the file,
  expected to need on-hardware iteration, and not knowable from the host.
- ~~**The physics third of the frame is still estimated.**~~ **Closed by M1**,
  which measured a whole rocks-only frame at 60.5 ms and fitted all three
  points to 2.507 ms a rock.
- ~~**What M2 adds to the frame is unmeasured.**~~ **Closed, and it was wrong
  by three times the margin it allowed itself**: 86.4 ms at twelve rocks,
  19.7 over rather than 6, with the whole miss in the collision pass (§12).
- **`item` is the frame's hidden line item, and only one pass has been fixed.**
  A walk into a twelve-element list is ~115 µs, two and a half times an
  arithmetic statement, and §12 counted statements from the beginning. The
  rock pass is now fused so each field is read once, but nothing has audited
  the ship, the shots or anything M3 and M4 will add against the same yardstick.
  The habit to keep: **count list walks, not statements.**
- ~~**The fusion is unmeasured.**~~ **Closed, and it beat its estimate**: 18.2
  ms against ~15, taking twelve rocks to 68.3 ms (§12). The rate went to 14 fps
  rather than spend the last gameplay lever.
- ~~**The fused frame has not been run at 14 fps.**~~ **Closed: 65.4 ms at
  twelve rocks against 71.4, worst frame 70** (§12a) — the first frame in this
  design whose worst case meets its budget. Roughly 9 ms of headroom on a
  reachable board for M3's explosion rings and M4's saucer and sound, with
  three levers still priced if that runs out.
- ~~**Nobody has played it.**~~ **Closed, 2026-08-12, and it found two
  things.** 14 fps reads **smooth with no jitter**, so the rate that M2's
  budget bought is not one the game pays for. Ship movement — momentum, no
  drag, the magnitude clamp — **feels natural**, which is the one part of §7.2
  that was pure arithmetic against a document. What was wrong was the two
  numbers a player can see rather than feel: **shots landed on visible misses,
  worst on the smallest rocks** (`shot.reach`, §7.3), and the **ship read
  slightly too big** (§6.4). The hit boxes were the real defect and are fixed.
  The ship was **tried smaller and put back**: it reads tidier, and it also
  makes the game easier, since the ship is the one thing the rocks have to hit
  — a legibility change that turned out to be a difficulty change. The general
  shape is worth keeping: the constants that came from arithmetic were right,
  and the one that came from a *safety margin* was the one that played badly.
- **A board of all large rocks is not reachable in play, and the harness builds
  one anyway** (§12). It over-measures by 1.9–2.9 ms depending on how far levels
  have advanced. Kept deliberately, because it is reproducible and errs against
  the budget rather than for it — but the real worst frame sits below the table,
  and anything cut fine enough for that difference to matter should be decided
  on a reachable board instead.
- **Per-second and per-frame constants are mixed, and only one of them is
  bounded.** `setspeed` is the only wall-clock quantity in the game, and §7.3
  turns that into an inequality with a test behind it. Everything else the
  saucer and the sound bring in should be checked the same way before it is
  tuned by feel: the failure is not a crash, it is a shot that occasionally
  passes through a rock.
- **A pico2 is unmeasured**, and M0 makes the gap worse rather than better.
  Scaling by P10's 1.72× puts a 12-rock frame near 114 ms — about 9 fps — and
  the present alone, which does not scale with the interpreter, is 26 ms of
  it. If the pico2 matters, `MAX.ROCKS` becomes a per-board constant. Nobody
  has measured either shipped game there either.
- **The dirty tracker's row spans are why the present is what it is** (§3.3).
  One inclusive span per tile row means a dozen scattered rocks dirty 84 % of
  the screen when their outlines cover a few per cent of it. Tile-granular
  tracking would cut the dirty area to roughly a quarter — but it trades 20
  row blits for ~108 small ones, so whether it is faster at all is unmeasured,
  and it is device work rather than game work. Out of scope for this design;
  worth an experiment of its own if a second vector game ever follows.
