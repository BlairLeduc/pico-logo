# Asteroids in Pico Logo (design)

Status: **M0–M4 measured on a Plus 2 W and played. The game is done bar the M5
tuning pass.** Each measurement has rewritten this document, and
that is the point of the milestone structure rather than a failure of it.

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
- **M3** — lives, levels and deaths, measured at **69.2 ms** against 71.4, and
  played the same day. The ship test cost 0.245 ms a rock, the first estimate
  here to miss by less than its own margin; the worst frame is a recycle frame
  and the hitch is undetectable (§12b).
- **M4** — the saucer and the sound. First measured at **73.1 ms quiet and 77.9
  with a saucer** against a 71.4 ms budget, and **the reason it missed was not
  the saucer**: the slope moved from M3's 2.860 to 3.020 ms a rock on a loop
  whose source had not changed, because the interpreter found a global by
  scanning the table in creation order and M4 had added 35 constants ahead of
  the rock state. Reordering the file bought 3.72 ms and took the frame to
  **69.35 quiet — 0.14 ms above M3's, with the entire saucer and sound in it**.
  Then **the fix moved into the interpreter**, where it belongs: `find_global`
  is a hash lookup now, the position effect measures zero, and every Logo
  program in the tree collects the win (§12c). **Third run, with the arcade's
  saucer pacing and two more collision pairs in the hot loop: 71.78 quiet and
  75.58 with a saucer** — `frame = 35.36 + 3.035 n`, which fits in play except on
  frames carrying twelve rocks and a saucer at once (§12f).

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
| Tests | `tests/test_asteroids.c` (Unity + mock device, 97 tests), mirroring `tests/test_galaxian.c`; the M0 harness has its own binary, `tests/test_p11rocks.c` |
| Design | this document |
| Measurement | `logo/tests/p11m4` times a real frame at 6, 9 and 12 rocks and then at 12 rocks **with a saucer held on screen**, with the rock pass read apart from the rest — it writes its numbers to a file, because numbers on a display cannot be copied off it. `logo/tests/p11rocks` is M0's standalone erase-strategy harness and survives because nothing else reproduces that question; `p11m1` and `p11m2` are **gone** and `p11m3` was renamed, because a harness that does not run the frame the game runs measures a game nobody plays |
| Outline generator | [`scripts/gen_rocks.py`](../scripts/gen_rocks.py), host-side, output pasted in (§6.3) |

Play: `load "asteroids` then `asteroids`.

## 2. What the game is, mechanically

The arcade rules, kept:

- Rocks drift at constant velocity, rotating, wrapping at every edge.
- Shooting a **large** rock yields two **medium**; a medium yields two
  **small**; a small vanishes. The level ends when no rock and no saucer is
  alive.
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
| Explosion | 1, the ship's | **canvas**, four pen-drawn fragments | the ship's own four segments, floating apart from where it died (§9) |
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
| ship × rock | same loop, one more test (M3) | 12 |
| **saucer shot × rock** | same call, a fourth "shooter" (M4) | 12 |
| **saucer × rock** | same call, a fifth (M4) | 12 |
| shot × saucer | once per shot | 3 |
| ship × saucer, ship × saucer shot | once each | 2 |

**The last two rows were ruled out on paper and put back after measuring where
they actually go** (§9.3). Priced as a pass of their own they are twelve `item`
walks a frame and unaffordable; folded into the call that is already visiting
every rock they are one comparison each, because the position and the radius are
in hand and the parked-at-9999 idiom makes the absent case a single failed test.
The rule this keeps proving: **count the walks and the branches, not the
features.**

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

**M3's ship test is the same shape and one addition more.** The rock's radius
is already in hand from the shot test, but the box a *ship* is tested against
is a different one — `rrad` carries the rock plus `shot.reach`, and a ship is
ten steps wide — so the pass adds `ship.rad` to it:

```logo
make "r :r + :ship.rad
if :r > abs (:x - :shipcx) [if :r > abs (:y - :shipcy) [ship.hit]]
```

A second radius list would have been an `item` walk (115 µs) where the sum is
an arithmetic statement (43): the same "count list walks, not statements"
lesson as everything else in this loop.

#### A box is built from the part of the shape the other object meets

Three of this design's collision constants have come back from a board wrong,
and stating them together is worth more than any one of them:

| | was | is | the complaint |
|---|---:|---:|---|
| `shot.reach` (M2) | 4 | 2 | shots landed on visible misses, worst on small rocks |
| `ship.rad` (M3) | 10 | 6 | rocks killed before they reached the ship |
| `sau.shot.rad` (M4, B18) | 9 | **12.8** | **saucer shots passed straight through the ship** |

The first two were boxes taken from a shape's **widest** measurement and they
awarded hits the player could see were misses. The third was taken from the
ship's **narrowest** — its 9-step beam, where the hull reaches 12 to the nose
and 12.73 to the rear corners — and it refused hits the player could see land.
Both readings are the same error: *a box stands for the shape a player is
looking at, so it is built from the part of that shape the other object actually
meets.* A rock is 16 to 44 steps wide and meets the ship broadside, so the beam
is right for it and a few steps either way are invisible. A shot is a single
dot, and a dot inside the drawn hull that does nothing is the most visible
failure a collision test has.

**None of the three was catchable by the bound tests**, because none of them was
a tunnelling problem — `sau.shot.rad` = 9 passed its anti-tunnelling inequality
comfortably and went on passing. What catches this class is a test that walks
the **drawn outline** and asks the collision test about the points the generator
actually put on the screen, which is what
`test_a_saucer_shot_through_the_ships_nose_kills_it` does.

#### `ship.rad` is 6, and the play report is why

It was 10, and M3's board report was that **the ship died before rocks reached
it, worst on the medium and small ones** — which is the *same finding*
`shot.reach` produced at M2 (§7.3), from the same cause: a flat number added to
radii of 22, 14 and 8 is proportionally largest on the 8, so the excess a player
sees grows as the rock shrinks. Twice now, this design's collision constants
have been too generous, and both times the report came from the small end.

Two things set the number:

- **The ship is a thin triangle, not a disc.** Its rear corners are 12.68 steps
  from the centre and its **beam is 8.95**, and most bearings meet the beam.
  Ten was the corner radius rounded down; it should never have been the corner.
- **`rrad` already carries `shot.reach`**, which exists so a 160-step-a-second
  shot cannot tunnel between samples. A ship closes at 7.3 steps a frame at
  worst, so that 2 has no business in a ship test — and since it cannot be taken
  out of `rrad` without an arithmetic statement a rock, it comes out of
  `ship.rad` instead, for free.

So the addend over the rock's *drawn* radius is 6 + 2 = 8, which puts the box
exactly where the rock's longest spike meets the ship's beam:

| size | box | spike + beam | was |
|---|---:|---:|---:|
| large | 30 | 21.68 + 8.95 = 30.63 | 34 |
| medium | 22 | 13.24 + 8.95 = 22.19 | 26 |
| small | 16 | 7.49 + 8.95 = 16.44 | 20 |

Just inside contact at every size. **It errs towards a graze that should have
killed and away from a kill the player can see past**, which is the right side
of the error: the second reads as the game cheating and the first reads as luck.
`test_the_ship_box_is_not_wider_than_the_shapes_it_is_drawn_from` holds it
between those two bounds — above the ship's own beam, below spike-plus-beam —
and fails at 10, so the next change to this constant has to answer for how it
plays as well as whether it is safe.

**And the ship is parked at 9999 exactly as a spent shot is** — `ship.hit` does
it before it returns. That is not tidiness, it is what stops one frame taking
every life the player has: without it, a board of twelve rocks sitting on the
ship ends a full game in a single pass. It also removes the `if` per rock that
an "is the ship alive" guard would have cost, so an exploding or newly
respawned ship costs the same first comparison and no more.

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

An extra ship every 10,000 points. Every point in the game arrives through one
procedure, `add.score`, so that is tested in one place — and against a **moving
threshold** (`next.extra`) rather than a `remainder`, because a score steps over
a boundary rather than landing on it.

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

- **Saucer.** Appears on a countdown that shortens **with every saucer spawned,
  across the whole game** (§9.2), enters from a random edge at a random height
  clear of the HUD band, and crosses horizontally with a vertical jink about
  once a second. Pen-drawn, **8 segments in one closed walk** — dome,
  a short vertical rim at the widest point, then the hull sloping in to a flat
  bottom. Large saucer fires in a random direction; small saucer fires at the
  ship's current position, which is what makes it frightening. It leaves when it
  reaches the far edge, and that is the one object in the game that does not
  wrap in x. Two sizes: large only until level 3, one in two after that.

### 9.1 What M4 had to change, and both changes came from arithmetic

**The saucer is 1.8:1, not the arcade's 2.5:1.** §7.3's tunnelling bound applies
to it as much as to a rock, and against a target this shape it has to be read
**per axis**, because that is how the box is tested:

> **(shot travel + saucer speed) × overrun ≤ 2 × the box half-size**, on each
> axis separately — across: (160/14 + 1.8) × 1.3 = 17.2 against 24; down:
> (160/14 + 1.2) × 1.3 = 16.4 against 18.

The vertical one is the binding constraint, and it is the reason the shape is
what it is. A saucer drawn at the arcade's proportions is about 7 steps tall at
the small size; a shot covers 11.4 steps between two collision samples, so a
shot coming down on it passes through and is seen on neither side. The two ways
out are a **box half again as tall as the thing drawn in it** — which is exactly
the failure this design has already had twice, from `shot.reach` at M2 and
`ship.rad` at M3 — or a **taller saucer**. It is a taller saucer: 32 × 18 and
20 × 14, with the same `reach` of 2 the rocks use, which keeps every box under
30 % wider than the outline drawn inside it and keeps the test that says so
(`test_the_saucer_boxes_are_not_far_wider_than_the_shapes_drawn_in_them`).

**The saucer's shot is slower than the player's — 110 steps a second against
160 — and that is the same bound in the other direction.** The box it has to hit
is the *ship*, which is 9 steps across the beam where a large rock's box is 24:

> (110/14 + 4.8) × 1.3 = 16.5 ≤ 2 × 9 = 18, with the ship's own top speed in the
> sum because the ship is the thing closing.

At 160 it would fly through the ship about as often as it hit — which is not a
crash and not a visible bug, it is a saucer that misses for no reason. Both
bounds have tests, as §7.3's does, because none of the three is observable by
playing.

**And the shape needed one change to the generator.** Every outline before this
had its first vertex straight ahead of the turtle, so the prologue was a single
`fd`. A saucer has no vertex on its centreline, and inventing one would spend a
segment on a corner that is not there — so the generator now emits
`pu rt <bearing> fd <reach> rt <turn> pd`, and emits that first turn **only for a
shape that needs it**, so the rock and ship walks come out byte-identical to what
M0 and M2 measured.

### 9.2 When a saucer comes, and which one — the arcade's own routine

Read out of the 6502 disassembly
([nmikstas/asteroids-disassembly](https://github.com/nmikstas/asteroids-disassembly)),
because "it appears every so often" turned out to hide three rules that are all
about pacing, and the first version of this section had none of them.

**One countdown for the whole game, not per level.** A reload value starts at
`$92` (146), a saucer spawning takes `$06` (6) off it, and it floors at `$20`
(32):

```
L6BD0:  LDA ScrTmrReload
L6BD4:  SBC #$06                ;decrement saucer timer by 6.
L6BD6:  CMP #$20                ;Is spawn timer below minimum value of 32?
```

So saucers arrive steadily more often the longer a player survives, whatever
level they are on, and a new game starts gentle again. Those three numbers are
this game's `sau.gap.start`, `sau.gap.step` and `sau.gap.min` unchanged — read as
**frames**, which at 14 fps is 10.4 s between the first saucers and 2.3 s
between the last.

**Which saucer is the same routine, and it is why the early game is kind:**

```
L6C12:  LDX #$02                ;Prepare to make a large saucer.
L6C14:  LDA ScrTmrReload        ;Is it still early in the asteroid wave?
L6C17:  BMI SetScrStatus        ;If so, branch to create a large saucer.
L6C19:  LDY ScoreIndex          ;Is the player's score above 30000?
L6C1E:  CMP #$30                ;If so, branch to create a small saucer.
```

`BMI` tests bit 7 of the reload value, so **while the gap is 128 or more every
saucer is large** — the first *four* of a game: the size is read before the gap
is stepped down, so the fourth spawn still sees 146 − 3×6 = 128, and $80 has bit
7 set, so the arcade counts the same four. Above
**30,000 points every saucer is small**. Between the two, a coin flip. A player
on level one therefore meets large saucers only, at the longest interval in the
game: the small one has to be *earned*, not waited for.

**And the aim tightens with the score**, which is the other half of what makes
the small saucer frightening — the arcade's becomes "extremely accurate" past
about 35,000. Here the spread runs linearly from `sau.aim.wide` (24°) at nothing
to `sau.aim.tight` (4°) at `sau.aim.score`, and stays there.

**A dead ship is not a target** (B22), and the arcade gets that for nothing:
there a destroyed ship is a *deactivated object* with no position at all until a
new one is created at the centre, so the aiming routine has nothing to read.
This port has something to read, because §7's respawn trick parks the waiting
ship **on the spawn point** so the rock pass can double as the clear-check — and
an aimed saucer therefore spent the whole death and the whole wait walking 4°
shots into the exact point the player was about to appear at, motionless, on a
point that does not move between deaths. So `saucer.fires` is guarded on `dying`
and `waiting`, and a small saucer with no target fires anywhere, as the large one
does. **The lesson is that the parked position leaked out of the collision system
it was invented for**: a value chosen to make one test cheap became an input to
an unrelated one that had no business reading it.

What this replaces was a level-based rule of this design's own invention — 90
frames at level 1, 10 fewer per level, small saucers from level 3 — which put a
saucer in front of a new player inside seven seconds and a homing one on level
three. **The arcade's pacing is the better game and it is not more code**: the
same countdown, one comparison more.

### 9.3 The saucer's shot breaks rocks, and pays nobody

§8's pair table said it did not, and priced that as twelve more pairs a frame on
the hottest loop in the game. The pricing was right and the conclusion was wrong,
because it costed the test in the wrong place: folded into `shot.on`, which
already visits every rock with its position and radius in hand, the saucer's shot
is **one comparison a rock** and the saucer itself is **one comparison and one
addition**, with no branch — and both fall out on the first test in the common
case, because both are parked at 9999 when they do not exist.

Three things follow, and all three are the arcade's:

- **A rock the saucer shoots pays the player nothing.** `split.rock` takes an
  `award` flag, so the rock still splits, still bangs and still leaves children —
  the player just loses the points. A saucer overhead is eating the board you
  were about to be paid for, which is what makes it worth chasing off rather than
  merely dodging.
- **A saucer that flies into a rock dies with it**, and neither pays.
- **`shot.on` now answers "what hit this rock"** — 1–3 a player's shot, 4 the
  saucer's, 5 the saucer — and the caller does the rest. It kept its name so that
  its measurement stays comparable across milestones.
- **Explosions.** The arcade shatters a rock into drifting line fragments.
  **As built there is exactly one explosion and it is the ship's**: ten frames,
  in which the ship comes apart into **the four segments of its own outline**,
  each floating out from where the ship died at three steps a frame. Rocks die
  without one, which is a difference from the arcade that nobody has yet asked
  to close; the cost of closing it is a per-frame list of live explosions in the
  hot path, and the measurement to justify it does not exist.

  **This is a fragment system only in name, which is why it is affordable.**
  The objection this section raised against fragments was per-frame *state* — a
  list of particles to allocate, step and reap. There is none: the fragments are
  fixed shapes at fixed places in the ship's frame, so each one is a `rt`/`fd`
  walk out from the ship's centre by a distance that grows as the countdown
  falls, then the segment itself. Four strokes a death frame against the
  **ninety** the expanding ring it replaced swept — less drawing, but four
  procedure calls where the ring was one `arc`, so more interpretation. That
  trade is bought on death frames only, which come ten at a time and never in
  the steady state.
  **The two-second freeze this section used to specify is gone.** It came from
  Galaxian, whose actors are engine-driven turtles and which can therefore
  sleep through a death; here every rock is Logo's to move, and the arcade
  keeps them drifting while the ship burns. So the death is a **countdown
  inside the frame loop** (`dying`) rather than a `wait`, the rocks carry on
  under it, and nothing in the game blocks — a player can pause or quit through
  a death.
- **Respawn: the ship waits for a clear space**, which is the arcade's rule, and
  **the scan it needs is free.** M3 used invulnerability instead — 21 blinking
  frames — on the stated grounds that waiting costs a twelve-slot scan on every
  waiting frame. That is true of a scan written as its own loop and false of this
  one: the rock pass **already** tests every rock against the ship's collision
  box, so parking the waiting ship *on* the spawn point and widening `ship.rad`
  to `clear.rad` turns that test into "is anything near where the ship wants to
  be?" at no per-rock cost at all. `ship.hit` reads a `waiting` flag and sets
  `blocked` instead of taking a life; `step.wait` reads it on the next frame, so
  the ship lands one frame after the field cleared and the frame order is
  untouched. A level starts the ship the same way, because rocks spawn at random
  positions and one can be on the origin.

  Two consequences worth stating. The blink is gone, because there is nothing to
  signal — a ship that has appeared is a ship that can be hit. And the saucer and
  its shot reach `ship.hit` too, so an area with a saucer crossing it does not
  count as clear, which is right and cost nothing to arrange.

  **The wait is capped at `wait.max` (28 frames, 2 s), and the controls are dead
  while it runs** — both added after B24. The box a rock has to leave is
  `clear.rad` *plus the rock's own radius*, 50 steps for a large one, and rocks
  cross at 0.96 steps a frame: one rock drifting through the middle can own the
  spawn point for over a hundred frames, and the worst measured over 60 respawns
  was 127 — nine seconds of empty screen. A rule with no upper bound on it is a
  hang. The cap is affordable because the clear box is 20 steps wider than the
  box that kills, so a rock still inside it at the cap is usually not touching.
  And `poll.input` guarded only `dying`, so a player could steer, thrust, fire
  and hyperspace a ship that was drawn nowhere; `waiting` now stops it at the
  same guard, with pause and quit still live above it.
- **Hyperspace.** Set the position to a random point, zero the velocity. There
  is nothing to erase — the frame clears and redraws (§3.3) — so the mechanic
  is five statements. The arcade's chance of materialising inside a rock is
  kept, since it is the mechanic's entire point, but a 1-in-8 flat chance
  rather than the original's velocity-dependent formula. The collision position
  is deliberately *not* set by the jump: `step.ship` runs after `poll.input` in
  the same frame and copies it across, so a jump can never be tested against
  the place the ship left.

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
is built only where a displayed value changes (a kill, a death, an extra ship,
a level), **in the procedure that changes it**: `add.score` for anything that
moves the score or awards a ship, `ship.hit` for a life, `setup.level` for a
level. `split.rock` used to hold the refresh, from M2 when the HUD carried the
live rock count and a kill changed it — but the HUD carries the level now, so
a kill is a *score* event and the refresh followed the value. Raised on PR #145,
and M4's saucer is the caller that would otherwise have found it out. Lives are drawn as that many **heart
glyphs** in the same string rather than as drawn ships, which keeps the whole
HUD one `write`: a ship costume would be a second drawing pass and a second
thing for `clean` to take, for a picture the font can carry.

As built at M3 the line is `SCORE 240 LEVEL 2 ♥♥♥`. The heart is **`char 16`**,
glyph 0x10 of [`devices/logo-font.h`](../devices/logo-font.h) — a spare
control-code slot, filled for this game — and `lives.word` builds the run of
them with `word` into an empty word, inside `refresh.hud` and nowhere else.
This is the design's one change to anything outside `logo/`, and it is six
bytes of font rather than an interpreter or device feature.

M2's line carried the live rock count instead of the level, because at M2 there
was nothing else to carry.

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
| Heartbeat | `[0 4]` | **square, 25 ms attack, sustained** (§11.2) | the two alternating low notes, whose interval shortens as the rock count falls **and as the wave wears on** — the game's signature |
| Fire, **and the saucer's warble** | `[1 5]` | sawtooth zap | a shot launches; a two-tone beep every third frame while a saucer is up |
| Thrust | `[2 6]` | **narrow pulse at 96 Hz**, re-triggered each held frame | held while thrust is held |
| Explosions | `[3 7]` | white noise | rock death (pitched by size), ship death, saucer death |
| **Extra ship** | `[0 4]` | the heartbeat's square, borrowed (§11.4) | a fixed burst of eight high notes, ~1.1 s, when `add.score` crosses the threshold |

The heartbeat is the retrofit's payoff, the same way the dive shriek was
Galaxian's: it is a *tempo*, not a note, so it needs a voice that keeps
sounding while the frame loop gets on with its work. The blocking `toot` this
project started with could not have produced it. It is a **countdown and not a
`remainder`**, because the period changes underneath it: a remainder against a
moving divisor either double-beats or skips on the frame a rock dies.

### 11.1 The arrangement above is not the one this section first wrote

Two corrections, and the first one is a plain error rather than a trade.

**White noise on `[2 6]` cannot exist.** Voices 3 and 7 are the noise voices and
0–2 / 4–6 are the tone voices, and asking for a noise waveform on a tone voice
is an *error*, not a shrug ([`setwave`](../reference/Pico_Logo_Reference.md#setwave)).
The arcade thrust is a noise rumble, so the honest options were to put it on the
noise pair with the explosions — where a held thrust key would silence every
explosion in the game — or to make it out of tone. It is a narrow pulse down at
96 Hz, and `test_the_timbres_are_set_once_and_match_the_voice_kinds` holds the
voice-kind rule so the next edit to this table fails a test rather than a run.

**There are five sounds and four voice-pairs, so one pair is shared, and which
one is a gameplay decision.** Fire and the warble share `[1 5]`: a fire is one
frame of zap, the warble is a beep every third frame, and — because `poll.input`
reads **one key a frame** — a player who is firing is not thrusting on that
frame anyway. Sharing with the thrust pair instead would have cut the saucer's
warning out from under a player who is running away, which is the one moment the
warning exists for.

Two smaller decisions worth recording. The **rock explosion is pitched by size**
— 1500 Hz for a large, 4500 for a small — so the split table is audible without
looking; that is one statement, because the pitch is arithmetic on `rsize`
rather than a dispatch. And **`stopsound` runs when a level ends**, not because
of tidiness but because the PSG keeps sounding on its own: a thrust rumble left
gated on follows the player back to the attract screen. It preserves the
timbres, which is why `setup.sound` runs once a game.

### 11.2 What the board said about the heartbeat (2026-08-13)

This section wrote the heartbeat three times before a speaker heard it, and the
speaker corrected two of the three things it had picked. Reported from a board:
*"the heartbeat sound is now clicks instead of being tones."* It was, and the
game was making exactly the sound it asked for.

**A 78 Hz triangle is not a note on this hardware.** A triangle's harmonics fall
away as 1/n², so at 78 Hz the third is 19 dB down at 234 Hz and the fifth 28 dB
down at 390 Hz — there was nothing in the note the PicoCalc's small speaker
could move air with, and the fundamental itself is far below where it starts to.
What a small speaker reproduces *perfectly* is a step, and the note began with
one: `setenv [0 4] [0 90 0 70]` asked for a 0 ms attack, and the square/triangle
oscillators start each gate from phase 0, so the first sample was a full-scale
jump. The note was inaudible and its onset was not, so the heartbeat was a
metronome of clicks — a *tempo* still, which is why the tempo half of the design
was never in doubt.

Three changes, and each is a different layer of the same mistake:

- **An octave up, at the same 6:5 interval** — 156 Hz and 130 Hz, exactly double
  78 and 65. Still two low notes; now with harmonics at 468, 780 and 1092 Hz.
- **A square, not a triangle.** A square's harmonics fall as 1/n, so a small
  speaker hears a low fundamental *through* them. It is the reason chip music on
  tiny speakers has audible bass at all, and the arcade heartbeat is a buzzy
  thump rather than a pure tone anyway.
- **A 25 ms attack and a sustain of 9.** The attack replaces the step with a
  ramp, and the sustain gives the note a steady part to hear as a pitch — a note
  that decays straight to zero, as `[0 90 0 70]` did, has none.

**The number that made the attack worth writing down: the envelope only moves
once a refill block, 3.5 ms** (`SOUND_RING_HALF`, [sound-design.md](sound-design.md)
§6). `steps_for` counts blocks and clamps to one, so *any* attack under 7 ms is a
full-scale step no matter what number is asked for — which is why 0 ms and the
engine's "click-free default" of 5 ms sound identical, and why 25 ms rather than
5 is what actually buys a ramp. Anything shaping a note in this interpreter has
to be written in multiples of 3.5 ms to mean anything.

### 11.3 The heartbeat's second pressure: time in the wave

M4's heartbeat read the rock count alone, so a player who stopped shooting
stopped the acceleration. The arcade's does not work that way — the cabinet's
beat quickens the longer a wave lasts, whether the player is clearing it or
hiding from it, and that is the half that makes hiding cost something.

The gap is now `beat.floor + beat.rock × rocks.alive − ⌊frame.count ÷ beat.ramp⌋`,
floored at `beat.min`. The wave clock is **free**: `frame.count` already exists
for `reclaim`, `setup.level` already zeroes it, and it is incremented inside
`play.frame`'s paused guard — so a pause does not advance the tempo, which is
the behaviour a player expects and would have had to be written deliberately
otherwise. `beat.ramp` is 140 frames, ten seconds at 14 fps, so the time term
stays *secondary* to the rock count over a normal wave and only dominates a wave
the player is dragging out. The arithmetic runs on the frames a beat fires, not
on every frame; a quiet frame is still a decrement and a failed comparison.

**`beat.min` is 3 frames and it is not tidiness.** A beat is a 110 ms note with
a 45 ms release on it, ~155 ms of sound against a 71.4 ms frame. At a two-frame
gap the beats overlap into one continuous tone and the game loses the tempo that
is the entire point of the sound; at three there is ~60 ms of silence between
them. The floor is therefore set by the note's *length*, and moving one without
the other is the mistake to watch for.

| Frames into the wave | Rocks alive | Gap | Beat |
|---|---|---|---|
| 0 | 12 | 16 frames | 1.14 s |
| 0 | 3 | 7 | 0.50 s |
| 420 (30 s) | 8 | 9 | 0.64 s |
| 700 (50 s) | 8 | 7 | 0.50 s |
| 1120 (80 s) | 4 | 3 (floored) | 0.21 s |

`test_the_heartbeat_speeds_up_as_the_wave_wears_on` holds the count still and
moves only the clock, then pins the floor and that a new wave restarts the
clock. Its sibling `test_the_heartbeat_speeds_up_as_the_board_thins` is unchanged
and still passes, because it leaves `frame.count` at zero — the two pressures are
separable by construction.

### 11.4 The extra ship's alarm, and why it takes the heartbeat's voice

The arcade announces an extra ship with a rapid high beeping, and nothing here
announced it at all — the ship count on the HUD just went up. `extra.alarm` is
that beeping: two notes at 1760 and 1170 Hz, alternating every two frames for
eight notes, about 1.1 s. High deliberately, because nothing else in the game is
a *tone* above 1100 Hz (the warble's top note), so the alarm cannot be mistaken
for a saucer.

**The voice is the whole decision, and it went to `[0 4]` with the heartbeat.**
Every other pair is worse for the same reason: an award arrives on a *scoring*
frame, and a scoring frame is very often one the player is firing on (`[1 5]`)
or being chased by a saucer on (`[1 5]` again, warbling every third frame), or
running away on (`[2 6]`). Those are exactly the sounds that would chop the
alarm into fragments, and the 1000-point small saucer is a plausible trigger for
the award in the first place. The beat is the one pair nothing else competes
for, so the alarm gets a clean second of it.

The heartbeat is not silenced by a flag. `add.score` sets
`beat.in = extra.beeps × extra.gap`, which lands the next beat on the frame
*after* the last alarm note, so the two never gate `[0 4]` in the same frame and
the tempo resumes on its own with nothing to clear. Costs are one line at a site
that runs a handful of times a game, and one comparison a frame in the loop:
`extra.alarm` tests `extra.left` **first** and returns, so a quiet frame does not
even pay the countdown the other two alternators do.

`setup.level` zeroes `extra.left` because a level end already ran `stopsound` —
an alarm still owing notes would otherwise resume into a board it did not belong
to. The corollary is that clearing a wave with the shot that pays for a ship
means hearing the level's silence instead of the alarm, which is the same deal
every other sound in the game gets at a level boundary.
`test_an_extra_ship_sounds_an_alarm_the_heartbeat_makes_room_for` holds all of
it: the burst length, the two notes, that `add.score` itself is silent, that no
beat interleaves, and that the beat comes back.

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
stands. `logo/tests/p11m3` measures the frame the game actually runs.

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

So M2 was built whole, and its harness measured it (that harness is now
`logo/tests/p11m3`: M3 added a branch to the frame, and a harness that does not
follow the frame measures a game nobody plays).

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



## 12b. M3 measured: the frame fits, the worst frame does not

**300 frames a point on a Plus 2 W** (`logo/tests/p11m3`), the same worst case —
twelve rocks with three shots live on every frame:

| rocks | body | present | **frame** | min | max | rock pass |
|---:|---:|---:|---:|---:|---:|---:|
| 6 | 25.51 | 26.54 | **52.05** | 48 | 60 | 19.08 |
| 9 | 34.07 | 26.46 | **60.54** | 56 | 66 | 27.66 |
| 12 | 42.69 | 26.52 | **69.21** | 64 | 74 | 36.27 |

> **frame = 34.86 + 2.860 n ms**, against a **71.4 ms** budget. It predicts the
> nine-rock frame at 60.60 against 60.54 measured.

**Twelve rocks fit, with 2.2 ms rather than M2's 6.0.** Correcting for the
harness's unreachable all-large board (−1.9 to −2.9 ms, §12), a real twelve-rock
frame is about **66.5 ms**.

### What the ship test cost, and it is the first estimate in this design inside its own margin

The rock-pass slope went **2.620 → 2.865 ms a rock**: the ship test is
**0.245 ms a rock, 2.9 ms at twelve**, against ~1.5 ms estimated (§18). That is
1.9× the estimate — still high, and still in the same direction as every other
estimate here, but this is the first time the miss has been smaller than the
margin it was spent from. Decomposed against §12's units it is close to its
floor: an arithmetic statement to widen the box (43 µs), one comparison with an
`abs` and a subtraction inside it (~95), and the `make` that holds the radius
for both tests (~43) — which is itself the cheap side of the choice, since
reading `rrad` twice would be a 115 µs `item` walk.

The intercept moved **33.9 → 34.86**. About 0.2 ms of that is the frame's new
branch and `step.ship`'s two extra statements, and about 0.07 is the bigger
recycle amortised over 25 frames. The remaining ~0.7 ms is not explained by
anything M3 added, and is recorded here as unexplained rather than attributed.

### The worst frame is a recycle frame, and the arithmetic says so exactly

**74 ms against 71.4** — the property M2 had just gained, lost. But it is not a
spike of unknown origin, which is the only kind the "worst frame meets the
budget" rule (§18) exists to catch:

> 69.21 (the mean twelve-rock frame) + 4.1 (one recycle) = **73.3**, against 74
> observed.

**The recycle is the finding of this run: 1.3 ms at M1, 2.2 at M2, 4.1 at M3.**
Nothing about the *frame* tripled it. A recycle walks the whole node pool, so it
scales with the **workspace** — 48 procedures and their bodies where M1 had
26 — and it will grow again at M4 when the saucer and the sound arrive. It is
0.16 ms a frame amortised, which is nothing, and a 4.1 ms bump once every 1.8 s,
which is the whole of the overshoot.

Two things follow. **The bump cannot be spread**: nothing in this interpreter
collects incrementally, and recycling *more* often makes it worse rather than
better, since the cost is the pool walk and not the garbage. And **the interval
is not the lever it looks like**: raising `reclaim.every` halves how often the
hitch lands but not how big it is, and it spends the safety margin on the one
thing that has already crashed a board (§14).

### What this does not cost

An overrun costs frame rate and not correctness — `sync` presents late rather
than failing — so a 74 ms frame is a 2.6 ms slip on one frame in 25, 3.6 % late,
on a board that measures 1.9–2.9 ms pessimistic to begin with. The rate stays at
14 fps and the board stays at twelve rocks: the alternatives are 13 fps (a 7 %
cut for a bump on 4 % of frames) or `MAX.ROCKS` 11 (−2.86 ms, and it is the one
lever that takes away the board filling up, which is what Asteroids is).

**And how it plays is now settled: the hitch is undetectable** (played
2026-08-12). A 4 ms slip once every 1.8 s cannot be seen at 14 fps, so the
overshoot is real arithmetic with no consequence, and the two levers stay
unspent.

That is worth stating as more than a result, because this design has now spent
three milestones treating "the worst frame meets the budget" as the rule a rate
must satisfy (§18). It is the right rule for an *unknown* worst frame, which is
what it was written against — a spike nobody has decomposed can be any size and
can land anywhere. It is the wrong rule for a **known, priced, periodic** one:
here the overshoot has a named cause, a fixed period and a measured size, and
`sync` turns it into lateness rather than failure. The rule should be read as
"no unexplained frame may exceed the budget", and M3's worst frame is explained
to within 0.7 ms.

### The rest of the run

| | ms |
|---|---:|
| one `shot.on` | 0.38 (0.42 and 0.53 at M2 — scene-dependent, §12a) |
| one `thrust` | 0.99 (0.86 at M2) |
| **one explosion ring** | **0.90** |
| one recycle | 4.10 |

**The explosion ring is cheap, and it is the design decision this run
vindicates.** 0.90 ms is about fifteen drawing statements for ninety strokes —
which is what "one primitive call with no interpreter between the segments"
(§9) predicted — and it *replaces* the ship rather than adding to it, so a death
frame costs about **+0.65 ms** over a live one. A fragment system would have
been per-object Logo in the hot path for the same picture.

Storage was flat: 23,415 → 23,345 nodes over the run, and the closing recycle
recovered nothing because there was nothing to recover. The present held at
**26.46–26.54 ms** at every rock count, the **sixth** independent time.

## 12c. M4 measured: the saucer was not the problem

**300 frames a point on a Plus 2 W, 2026-08-12** (`logo/tests/p11m4`), twelve
rocks with three shots live on every frame, and a fourth point with a saucer
held on screen for all 300:

| rocks | body | present | **frame** | min | max | rock pass |
|---:|---:|---:|---:|---:|---:|---:|
| 6 | 28.38 | 26.57 | **54.95** | 50 | 62 | 20.26 |
| 9 | 37.51 | 26.47 | **63.98** | 58 | 70 | 29.36 |
| 12 | 46.69 | 26.39 | **73.07** | 66 | 80 | 38.50 |
| 12 + saucer | 51.40 | 26.54 | **77.94** | 72 | 84 | 38.80 |

> **frame = 36.83 + 3.020 n ms**, against a **71.4 ms** budget. It predicts the
> nine-rock frame at 64.01 against 63.98 measured.

**Twelve rocks are 1.6 ms over with no saucer and 6.5 ms over with one.**
Correcting for the harness's unreachable all-large board (−1.9 to −2.9 ms), a
real twelve-rock frame is about **70.2–71.2 ms quiet and 75.0–76.0 with a
saucer** — so the quiet frame is at the line and the saucer frame is well past
it. Something had to change. What that something is turned out not to be a
gameplay lever.

### The saucer cost 4.87 ms, and that is the part that behaved

§12c's projection was ~2.7 ms; the board says **4.87**, which is 1.8× — the same
factor as M3's ship test and by now the normal size of a miss here. Of it,
**1.2 ms is the outline** (against ~1.4 projected, the one figure that was
right) and the remaining 3.7 ms is the stepping, the four collision pairs, the
warble and the shot in the air. The flat cost — `step.saucer`'s countdown,
`heartbeat`, `step.sau.shot`'s first comparison — landed inside the intercept
below.

### The finding: the code that did not change got 6 % slower

**The slope moved.** M3 fitted 2.860 ms a rock and M4 fits 3.020, and the rock
pass alone went from 2.865 to 3.041 ms a rock — **6.1 % on a loop whose source
M4 did not touch.** Two singles in the same run say the same thing about
identical code:

| | M3 | M4 | code changed? |
|---|---:|---:|---|
| one `shot.on` | 0.38 | **0.50** | no |
| one `thrust` | 0.99 | **1.36** | +one `sound` call (0.20) |
| one explosion ring | 0.90 | **0.90** | no |

The explosion ring is the control that makes this readable. `draw.boom` is one
`arc` — ninety strokes inside a single primitive, with no interpreter between
them — and it **did not move at all**. Everything that is *interpretation* got
slower; everything that is *primitive* stayed exactly where it was.

**The cause is name lookup.** `find_global`
([core/variables.c](../core/variables.c)) is a **linear scan of the global table
in creation order**, and a Logo file's `make`s run in file order — so a name
defined early is found faster than one defined late, on every read, for the life
of the program. M4 added about 35 constants to the *tuning* block, which sits
**above** the rock state, and pushed every one of the rock pass's lists ~35 slots
deeper. The rock pass reads about eighteen globals per rock: eight lists, six
shot positions, the ship's collision position, `ship.rad` — **over two hundred
scans a frame at twelve rocks**.

Reproduced on the host, where the mock draws nothing and the body is therefore
almost pure interpretation (minimum of seven runs each, `ms` of one frame body):

| | rock pass | whole body, no saucer | whole body, saucer |
|---|---:|---:|---:|
| M4 as first written | 0.5298 | 0.5926 | 0.6347 |
| **M4 with the state block hoisted above the tuning block** | **0.4787** | **0.5391** | **0.5701** |
| M3, for scale | 0.5008 | — | — |

and the mechanism confirmed directly: **40 dummy globals created *ahead* of the
game cost 4.8 % on the rock pass; the same 40 created *after* it cost nothing
measurable.** It is position in the scan, not the size of the table.

### What was done first: the state block moved — measured, then thrown away

The state block was hoisted **above** the tuning block in the game file, which is
a block move with no logic change. The board ran it (2026-08-12, second run of
`p11m4`):

| rocks | body | present | **frame** | min | max | rock pass |
|---:|---:|---:|---:|---:|---:|---:|
| 6 | 25.97 | 26.63 | **52.60** | 48 | 62 | 18.48 |
| 9 | 34.27 | 26.55 | **60.83** | 56 | 68 | 26.89 |
| 12 | 42.73 | 26.63 | **69.35** | 64 | 78 | 35.20 |
| 12 + saucer | 46.62 | 26.63 | **73.24** | 67 | 80 | 35.66 |

> **frame = 35.85 + 2.792 n ms** — **3.72 ms off at twelve rocks**, inside the
> 2.5–4 ms this section predicted, and the slope now sits **below M3's 2.860**.

The result worth stating plainly: **69.35 against M3's 69.21.** The entire
saucer, the whole PSG arrangement and the heartbeat cost **0.14 ms** of frame
once the lookup depth was paid back — M4's features were, in effect, free.

And the saucer's own cost fell with it, 4.87 → **3.89 ms**, because a saucer
reads about twenty globals a frame and they had been the deepest in the table.

### What was done instead: the interpreter was fixed

The file-order workaround makes *this* game fast and leaves every other program
in the tree to rediscover the same tax, so the fix moved to where the problem is.
`find_global` ([core/variables.c](../core/variables.c)) now keeps a **hash index**
beside the global table — FNV-1a over the case-folded name, open addressing, 512
entries of one byte, rebuilt on the rare erase paths. The table itself is
untouched, so `pons`, `poall` and every workspace listing print exactly what they
printed before; `tests/test_variables.c` holds the cases an open-addressed index
gets wrong (a chain broken by an erase, a slot reused by a new name, `erall`, and
case folding agreeing with the comparison it short-cuts).

**The position effect is gone.** The same host experiment that measured +4.8 %
for 40 globals ahead of the game now measures **0.1 %, in both directions**:

| | rock pass, 40 globals ahead | 40 after |
|---|---:|---:|
| linear scan | +4.8 % | 0 |
| **hash index** | **0** | **0** |

and the frame body drops a further **4.3 %** on top of everything above:

| | body, no saucer | body, saucer |
|---|---:|---:|
| M4 as first written | 0.5926 | 0.6347 |
| + state hoist (what the board's second run measured) | 0.5391 | 0.5701 |
| **+ hash index, hoist reverted** | **0.5157** | **0.5452** |

**So the game-side hoist was reverted.** With the interpreter fixed it measures
the same either way, and a game file should be organised for reading rather than
for a scan order that no longer exists. What is left in the game file is a
comment saying where the ordering rule went, so the next reader does not
reinvent it.

Cost: **512 bytes of SRAM** (91.65 % → 91.75 % on a Plus 2 W), which is the one
resource this project is genuinely short of, for a win every Logo program in the
tree collects.

**Projected for the third run**, on the same translation the second run
established (a host body cut lands at ~0.9 of its size on the board, which is
where the drawing is): **~67.7 ms quiet and ~71.2 with a saucer** at twelve
rocks — inside the 71.4 ms budget on the harness's pessimistic all-large board,
and about 68–69 on a board that can actually occur in play.

### Three things this generalises to, beyond this game

- **Adding code to a Logo program used to make the existing code slower**, and
  the effect was invisible in a diff: M4's regression was 2.2 ms in a loop nobody
  edited. That is fixed at the source now rather than worked around here.
- **A negative result worth keeping:** frame cost does *not* drift upwards
  between recycles (measured flat across a 25-frame cycle on the host), and a
  recycle costs the same whether it has one frame of garbage or fifty. Both were
  plausible explanations for the worst frame and both are wrong, which leaves the
  worst frame still only partly explained (§12e).
- **The interpreter was the right place to look**, and this design's premise —
  "no interpreter or device work is proposed" — was what nearly stopped it being
  looked at. The premise is a good default for a *game*; it is a bad default for
  a measurement that says the interpreter is the thing that changed.

## 12e. The worst frame is not fully explained, and two candidates are dead

The second run's worst frames are **78 ms quiet and 80 with a saucer**, against
means of 69.35 and 73.24. A recycle is 3.5 ms, so 72.85 and 76.74 are accounted
for and roughly **5 and 3 ms are not** — where at M3 the same arithmetic
explained the worst frame to within 0.7 ms. By §12b's own rule the interesting
kind of overshoot is the *unexplained* one, so this is the number to chase rather
than the mean.

Two explanations were tested on the host and **both are wrong**:

- **The frame does not get slower as the atom table fills.** Timed per phase
  across a 25-frame recycle cycle, the body is flat at 0.538–0.548 ms with no
  trend towards the recycle.
- **A recycle does not cost more when it has more to reclaim.** 0.105 ms after
  five frames of garbage, 0.110 after twenty-five, 0.112 after fifty — the cost
  is the pool walk, exactly as §14 says, so the harness's figure is not an
  under-measurement of the in-play one.

What is left is **board-only**, and the leading candidate is the one thing M4 put
into a running frame loop that no host can reproduce: **the PSG**. Sound is
gated by DMA on core 0, a note sounds for 70–110 ms across several frames, and
M3 — whose worst frame *was* explained — had no audio at all. A two-minute
experiment settles it: run the harness with `to heartbeat end` defined after the
game loads, so the frame gates no notes, and compare the max column. If the
spread collapses, the sound is stealing frame time asynchronously and the number
to reason about is what it steals per note, not per call.

Until then this is recorded as unexplained rather than attributed, which is the
habit that made §12b's recycle finding possible.

## 12d. M4's projection, kept for scoring

**Recorded before the run and left standing**, because scoring an estimate is
the only way this design has learned anything about its own estimates. It missed
in two different ways at once, which is new.

What follows between the rules is the projection **exactly as it was written
before the run**, against a record that already said estimates here come in
high: the drawing statement was 1.7× the estimate, M2's collision pass 2.8×,
M3's ship test 1.9×, and only the fusion beat its estimate.

---

M4 adds two kinds of cost and they are not the same kind:

| | statements | ms (projected) |
|---|---|---:|
| flat, every frame — `step.saucer`'s countdown, `heartbeat`'s countdown, `step.sau.shot`'s first comparison, three calls | ~7 | **~0.5** |
| only while a saucer is up — motion, jink and fire countdowns, three shot pairs, one ship pair, the warble | ~14 | ~0.8 |
| only while a saucer is up — the 8-segment outline and its placement | 20 | ~1.4 |
| **a frame with a saucer on it** | | **~2.7** |

Against M3's **2.2 ms** of measured headroom at twelve rocks (69.2 against
71.4), that means:

- **A quiet frame should still fit**, at about 69.7 ms.
- **A twelve-rock frame with a saucer on it should not**, at about 72.4 — over
  by 1 ms on the harness's board, and roughly *level* on a reachable one, since
  the harness's all-large board over-measures by 1.9–2.9 ms (§12).

---

**Both halves were wrong, and one of them was not on the list at all.**

| | projected | measured |
|---|---:|---:|
| flat, every frame | ~0.5 ms | the intercept moved **+1.97 ms**, part of which is this and part the lookup tax |
| a saucer on screen | ~2.7 ms | **4.87 ms** (1.8×, the usual factor here) |
| **the existing code getting slower** | **not considered** | **+0.16 ms a rock — 1.9 ms at twelve** |

The instructive one is the third. Every estimate this design has made has been
an estimate *of the thing being added*, and every one has come in high by
1.7–2.8×; that pattern is now well enough established to plan around. What no
estimate here has ever included is the possibility that **adding code makes the
code that was already there slower**, which on this interpreter it does (§12c).
The habit that follows: when a milestone adds globals, re-read the *slope* and
not only the intercept, because the slope is where a lookup tax shows up.

Two things the run was also read for, and both behaved:

- **The recycle** is **4.4 ms**, against 4.1 at M3 — it has gone 1.3 → 2.2 → 4.1
  → 4.4 across four milestones, still scaling with the workspace and no longer
  growing quickly (§14). It remains the whole of the worst-frame overshoot: 73.07
  + 4.4 = 77.5 against 80 observed, and 77.94 + 4.4 = 82.3 against 84.
- **`sound` is 0.2 ms**, timed here for the first time in this design. A frame
  gates about one note every eight frames on the heartbeat, so the sound costs
  roughly **0.03 ms a frame amortised** — §11's arrangement is not a frame-budget
  question, and the thrust rumble's per-frame retrigger costs 0.2 ms only on the
  frames thrust is held.

The present held at **26.39–26.57 ms** at every rock count, the **seventh**
independent time, and free storage went 21,628 → 21,623 → 21,623 across the whole
run: flat, with nothing for the closing recycle to recover.

## 12f. M4, third run: the arcade's rules and two more collision pairs

**300 frames a point on a Plus 2 W, 2026-08-12**, with the hash index in the
interpreter, the arcade's saucer pacing, the saucer's shot breaking rocks, the
saucer dying against them, and the wait-for-clear respawn:

| rocks | body | present | **frame** | min | max | rock pass |
|---:|---:|---:|---:|---:|---:|---:|
| 6 | 26.94 | 26.62 | **53.57** | 49 | 61 | 20.27 |
| 9 | 36.04 | 26.60 | **62.65** | 58 | 69 | 29.50 |
| 12 | 45.26 | 26.53 | **71.78** | 66 | 79 | 38.62 |
| 12 + saucer | 49.01 | 26.57 | **75.58** | 69 | 82 | 39.04 |

> **frame = 35.36 + 3.035 n ms**, predicting the nine-rock frame at 62.68
> against 62.65 measured.

The two halves moved in opposite directions, and both are explained:

- **The intercept fell 35.85 → 35.36.** That is the hash index on the flat part
  of the frame — the ship, the shots, the saucer's bookkeeping, the HUD.
- **The slope rose 2.792 → 3.035, or +0.24 ms a rock.** That is the two new
  pairs inside `shot.on`: the saucer's shot and the saucer itself, twelve rocks
  each, on every frame whether or not either exists. **+2.9 ms at twelve rocks**,
  against about 1 ms estimated from the host — the estimate was low by 3×, which
  is the same direction and roughly the same factor as every other estimate in
  this document.

`shot.on` itself measures **0.43 ms** against M4's first 0.50 *while doing five
tests instead of three*, which is the hash index paying for the features inside
the very call that added them.

### Where that leaves the budget

| | harness board | reachable board (−1.9 to −2.9) |
|---|---:|---:|
| twelve rocks, quiet | 71.78 — **0.35 over** | **68.9–69.9, fits** |
| twelve rocks, saucer up | 75.58 — 4.15 over | **72.7–73.7, 1.3–2.3 over** |

The harness's board is all *large* rocks and cannot occur in play: twelve rocks
is only reachable by splitting down, so a real twelve-rock board is small rocks
with fewer segments (§12). **So the game fits in play except on frames that carry
twelve rocks and a saucer at once, which are over by one to two milliseconds.**

**Nothing was cut, and the reasoning is §12b's rule rather than optimism.** The
overshoot is *known, priced and bounded*: it lands only while a saucer is on
screen, `sync` turns it into lateness rather than failure, and 2 ms on a 71 ms
frame is 3 % — against a 4 ms recycle hitch that M3 played and could not detect
at all. The three gameplay levers stay unspent and are re-priced at the new
slope: `MAX.ROCKS` 12 → 11 (**−3.04 ms**), the shot cap (**−1.0**, smaller than
it was because the hash made each test cheaper), the large rock at 6 → 5 segments
(−1.4). 13 fps (a 76.9 ms budget) would clear everything including the harness's
impossible board, at the cost of re-cutting every per-frame constant a third
time.

**Played and accepted, 2026-08-12: "fully playable now with no jitter, very
smooth game play."** So the one-to-two milliseconds a saucer costs on a full
board are arithmetic with no consequence, exactly as M3's 4 ms recycle hitch
was, and **no lever was spent on the whole milestone** — twelve rocks, three
shots and six-segment larges all survive, at 14 fps.

That is the third time this design has carried a measured overshoot to a player
rather than pre-emptively cutting the game to remove it, and the third time the
player could not detect it. The rule that keeps being vindicated: **`sync` turns
a bounded overrun into lateness, so the question a budget answers is not "does
every frame fit" but "can anyone tell".**

### The worst frame is still the open number

79 ms quiet and 82 with a saucer, against means of 71.78 and 75.58, with a
recycle at 3.7 — so **about 3.5 ms is unexplained**, exactly as at the second
run. §12e killed the two host-testable explanations. The remaining candidate is
board-only and is the one thing M4 put into a running frame loop: the PSG's DMA.
The experiment is still two minutes — load the game, define `to heartbeat end`,
run the harness, compare the max column — and it is the last thing this
milestone does not know about itself.

## 13. Reduced-resource choices

| Arcade Asteroids | This port | Saving |
|---|---|---|
| up to 27 rocks on screen | `MAX.ROCKS` 12, splits fill free slots only | bounds the two largest frame costs |
| 4 shots, generous range | 3 shots, ~1.2 s life | a third off the collision pair count |
| 12-vertex rocks at every size | **6 / 5 / 4** by size | 1.9 ms at twelve rocks; the small ones are the numerous ones |
| three rock outlines at every size | **one outline per size** | 2.2 ms at twelve rocks — the dispatch collapses to a single three-way test in one procedure, with no second call. The nine generated outlines stay in the file; rotation already varies how a rock reads in motion, and the variety comes back the moment M1's measurements say it can be afforded |
| 60 fps | **14 fps** | the present is 26.4 ms whatever the scene holds; 15 after M0, 14 after M2 (§12) |
| rocks shatter into drifting line fragments | 4-frame expanding `arc` ring | one primitive per frame instead of a particle system |
| both saucer sizes can coexist | one saucer at a time | one object, four collision pairs |
| the saucer's shot clears rocks out of its way | it hits only the ship | twelve pairs a frame, on the hottest loop in the game |
| saucer proportions about 2.5:1 | **1.8:1** | not a saving — a flatter saucer cannot be hit reliably at one collision sample a frame (§9.1) |
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

**`reclaim.every` is 4 frames** (B25). It was 25, on a 26× margin against the
measurement above — and the board ran out of storage anyway, because **that
deadline was measured on the wrong frame.** A frame with drifting rocks and
nothing else spends 9 cells and 45 atom bytes; a frame in a *game* — a saucer
up, shots in the air, rocks splitting — spends about 91, and dies in **89
frames** rather than 365. So 25 was a 3.6× margin wearing a 26× label. Measured
per procedure, `step.draw.all` is the frame's only allocator at all:
`step.saucer`, `step.shots`, `warble`, `draw.saucer`, `heartbeat` and the rest each
measure zero, so what moves the deadline is how much the *rock pass* has to do
and how often rocks split under it.

An adaptive `reclaim` — collect when free storage runs low, so the board's own
room decides the interval — was tried and abandoned on evidence. `nodes` reports
free **cells**, and this game does not run out of cells: at the moment the loop
dies there are 21,000 free cells and **20 bytes** of atom room. Nothing reports
atom bytes to Logo, so a frame count is the only signal available.

`test_the_reclaim_interval_stays_inside_the_busy_frame_budget` measures the
deadline on the expensive frame and fails if the interval creeps back towards
it, and `test_the_frame_loop_survives_a_squeezed_workspace` plays 2,000 busy
frames with live ballast holding the atom region down to a fifth — the board
the host does not otherwise have. **The deeper fix is still open**: the churn is
`.setitem` interning a fresh float string per rock per frame, and storing coarser
numbers would let atoms be *reused* rather than minted, which caps the working
set instead of racing it.

**A recycle costs 1.3 ms at M1, 2.2 at M2 and 4.1 at M3**, all measured on a
Plus 2 W — and the growth is not the frame's. A recycle walks the whole node
pool, so it scales with the **workspace**: 48 procedures and their bodies where
M1 had 26. Recycling ten times more often than the design first said therefore
costs 0.16 ms a frame amortised, which is nothing, and puts a **4.1 ms bump**
inside a 71.4 ms budget every 1.8 s — which is no longer nothing, because it is
the whole of M3's worst-frame overshoot (§12b). The frame loop holds flat:
**4 cells over 1,000 frames** on the host, and 70 nodes over 900 frames on the
board.

The interval is not the lever it looks like. Recycling *more* often makes the
total worse rather than better, since the cost is the pool walk and not the
garbage; recycling less often halves how often the hitch lands but not how big
it is, and spends the margin on the one thing that has already crashed a board.
**M4 should expect this number to grow again**, because the saucer and the sound
are more workspace.

**B25 took that trade the other way**, because the board crashed again. The
interval is now 4 frames, and this paragraph is exactly why that is expensive:
the bump does not get smaller, it lands on one frame in four rather than one in
25 — about 1 ms a frame amortised, and a quarter of frames carrying a 4.4 ms
hitch. That is a worse frame profile bought deliberately, for a margin measured
on the frame the game actually plays rather than on a quiet one (§14).
`logo/tests/p11m4` on hardware is what says whether the hitch is visible.

### M4 re-measured the deadline, and it moved the way §14 said it would

The deadline is **486 frames** on the host with M4 in it, against M1's 649 —
`reclaim` disabled, twelve rocks, run until the frame loop dies. Nothing about
the frame's *allocation* changed to do that: the saucer and its shot are plain
`make` on globals and mint nothing (the rule below), and the 36 atoms a frame
are still the twelve rocks' `.setitem`s. What changed is the **workspace**, which
is 17 more procedures and their bodies, and the atom region is capped at 32 KB
*or* wherever the node pool's floor has reached, whichever is lower. A bigger
program leaves less room for atoms.

`reclaim.every` 25 read as a **19× margin** against that rather than M1's 26×,
and the test required 8× — so the interval held without moving. **It should
have moved.** Both numbers were taken on a quiet frame, and B25 is what that
cost: the same 486-frame measurement on a frame with a saucer up and shots in
the air is 89, the margin was 3.6×, and the board ran out of storage a second
time. The interval is 4 and the test now measures the busy frame (§14).

**A recycle is 4.4 ms on the board at M4**, against 4.1 at M3: 1.3 → 2.2 → 4.1 →
4.4 across four milestones. The growth has flattened, which fits the cause — it
walks the node pool, and M4 added 17 procedures where M3 added 22 — and it is
still the whole of the worst-frame overshoot (73.07 + 4.4 = 77.5 against 80
observed; 77.94 + 4.4 = 82.3 against 84). Board storage was flat across the run:
21,628 → 21,623 → 21,623 nodes, with nothing for the closing recycle to recover.

The working set is flat: free storage settles at ~27,030 cells and stays there
across **5,000 frames** with saucers appearing, firing, being shot and leaving
throughout. There is a one-off step of ~220 cells as the first saucer's score
and HUD text settle, and then nothing — which is the steady state §14 describes,
not growth.

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
    ifelse 0 < :dying [step.death] [step.ship]   ; a dying ship has nothing to move
    step.shots             ; life countdown, and read the turtles back
    step.saucer            ; M4: wait, or move, jink, fire, be shot, hit the ship
    heartbeat              ; M4: a countdown, and a note every few frames
    clean                  ; buffered in sync mode since B16 (§3.1.1)
    step.draw.all          ; step, test and draw each rock, in one visit
    draw.ship
    if 0 < :sau.on [draw.saucer]   ; the guard is here, so a quiet frame pays a
                                   ; comparison rather than a call
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
  (setrefresh "sync :fps)
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

**A level ends three ways and `over` is all three of them** — the board is
clear, the last life is gone (`step.death` sets it from inside the frame), or
the player pressed Q. Which one it was is read back afterwards, by the state
machine above it, so this loop stays two lines.

**Clear means clear of saucers as well as of rocks.** A saucer still crossing
when the last rock breaks would otherwise be wiped by the next level's `cs`,
along with the points it was worth and the shot it had in the air — the player
watching a 200-point target evaporate mid-aim. So the end test is
`if 0 = :rocks.alive [if 0 = :sau.on [make "over true]]`, and `step.saucer`
spawns no replacement once the rocks are gone: it holds its countdown where it
is and returns. Both halves are needed. Without the second, a cleared board
would keep minting saucers and the wave would never end; with it, the wait is
bounded by one crossing — about six seconds — so a player who ignores the
saucer still gets their next wave.

```logo
to one.game
  attract.screen
  init.game
  make "playing true
  until [not :playing] [
    play.level
    if :quit [make "playing false]
    if :lives < 1 [make "playing false]
    if :playing [next.level]
  ]
  if not :quit [show.game.over]
end
```

Iterative and not recursive, as the two shipped shooters are: a game that called
itself for the next level would grow the stack for as long as the player kept
playing. **Q means "back to the attract screen" and not "the game ended"**, and
the difference between them is the game-over card — which is the whole reason
`quit` exists as a flag rather than being folded into `over`.

**One ordering M4 added is load-bearing and one is not.** The saucer is stepped
*after* `step.shots`, because it tests itself against the sampled shot positions
and they have to be this frame's — the same constraint the rock pass has. The
saucer's *shot*, though, is stepped from inside `step.shots` rather than from
`step.saucer`, because a shot in the air **outlives the saucer that fired it**:
the engine is flying it, and nothing about its saucer leaving or being shot
should make it vanish in mid-flight. Where `heartbeat` sits is the one that does
not matter: a note is gated by the PSG and goes on sounding by itself, so the
only consequence is that the beat reads a rock count one frame old.

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
described here in the past tense for that reason; M2's harness replaced it
(and is now `logo/tests/p11m3`).

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
(49 tests) and `logo/tests/p11m2` — renamed `p11m3` when M3 changed the frame —
which reads the rock pass apart from the rest
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

**M3 — lives, levels, deaths. BUILT, and MEASURED on hardware 2026-08-12.**
Ship × rock collisions, the explosion, respawn with a grace, lives, the extra
ship at 10,000, level advance, hyperspace, the attract screen and game over.

**Twelve rocks are 69.2 ms against the 71.4 ms budget** — `frame = 34.86 +
2.860 n`, about 66.5 on a reachable board — so the game fits, with 2.2 ms where
M2 had 6.0. The ship test cost **0.245 ms a rock** against ~1.5 ms estimated at
twelve, which is 1.9× and **the first estimate in this design to miss by less
than the margin it was spent from**.

**The worst frame is 74 ms, 2.6 over, and it is a recycle frame** — 69.2 + 4.1 =
73.3 against 74 observed. So the property M2 had just gained is lost, but not to
a spike of unknown origin, which is the only kind §18's rule exists to catch.
The finding underneath it is that **a recycle has gone 1.3 → 2.2 → 4.1 ms across
the three milestones**, scaling with the workspace rather than the frame (§12b,
§14). The rate stays at 14 fps and the board stays at twelve rocks.

**Played on hardware the same day, and it closed both open questions.** The
heart glyph reads, and **the 4 ms recycle hitch is undetectable** — so the
worst-frame overshoot is arithmetic with no consequence, and neither lever was
spent. The one thing the board sent back was a constant: the ship died *before*
rocks reached it, worst on the medium and small ones, so `ship.rad` is 6 rather
than 10 (§8). That pattern has now happened twice from the same end — a
collision constant chosen as a safety margin is generous in pixels and ruinous
in proportion on the smallest object, exactly as `shot.reach` was at M2.

**M3 is done.**

The build: `logo/games/asteroids` (48 procedures), `tests/test_asteroids.c`
(71 tests) and `logo/tests/p11m3`, which replaces `p11m2` — the frame gained a
branch, and a harness that kept calling `step.ship` unconditionally would time a
ship the game is not flying. It also prices the explosion ring on its own, since
that is the one M3 cost that is not in any per-rock figure.

Four things M3 settled that the document had not:

- **The death is a countdown, not a freeze** (§9). The design specified a
  two-second freeze, copied from Galaxian — which can sleep through a death
  because its actors are engine-driven turtles. Every rock here is Logo's to
  move, and the arcade keeps them drifting, so the explosion counts down inside
  the frame loop instead. Nothing in the game blocks, and a player can pause or
  quit through a death.
- **Invulnerability is spelled as a parked ship**, reusing the idle-shot idiom
  rather than adding a guard: `shipcx` is the ship's collision position and 9999
  means "not there". That is what stops a full board taking every life in one
  pass, and it is why the frames where the ship cannot be hit cost the same
  first comparison and no more.
- **A rock explosion is not in the game**, and the milestone list never said it
  was. §9 described rings for both; only the ship's is built, because a list of
  live rock rings is per-frame work in the hot path and no measurement justifies
  it yet.
- **`arc` sweeps with the pen it finds.** `draw.boom` arrives from a
  `pu setx sety` like every other drawing procedure in the file, and every other
  one puts the pen down inside its own walk — so the first ring was swept with
  the pen up and a death was invisible. Found by the test that counts the ring's
  strokes, which is the kind of thing only a segment count catches.

**M4 — saucer and sound. DONE: built, measured three times, and played on
hardware, 2026-08-12.** Both saucer sizes, the shot that fires back, and the full PSG
arrangement with the heartbeat.

**First run: 73.07 ms quiet and 77.94 with a saucer, against 71.4** —
`frame = 36.83 + 3.020 n`. The saucer costs **4.87 ms** while it is on screen,
1.8× its estimate, which is by now the normal size of a miss here. **The finding
was the other two thirds of the gap: the slope moved 2.860 → 3.020 ms a rock on
a loop M4 never touched**, because a global was found by scanning the table in
creation order and M4 had put 35 constants ahead of the rock state (§12c).

**Second run, after reordering the file: 69.35 quiet and 73.24 with a saucer** —
`frame = 35.85 + 2.792 n`, 3.72 ms off, and a slope *below* M3's. M3's frame was
69.21, so **the whole saucer and the whole PSG arrangement cost 0.14 ms** once
the lookup depth was paid back.

**Then the fix moved into the interpreter**, because file order makes one game
fast and leaves the rest of the tree to rediscover the tax. `find_global` keeps
a hash index now; the position effect measures zero, the frame body drops a
further 4.3 %, and the game-side reordering was reverted as unnecessary. Third
run projected at **~67.7 quiet and ~71.2 with a saucer** — inside the budget on
the harness's pessimistic board, and 68–69 on a reachable one. None of the three
priced gameplay levers was spent.

The build: `logo/games/asteroids` (65 procedures), `tests/test_asteroids.c`
(97 tests) and `logo/tests/p11m4`, which replaces `p11m3` — the frame gained
`step.saucer`, `heartbeat` and a guarded `draw.saucer`, and a harness that did
not follow it would measure a game nobody plays. It adds a **fourth measurement
point**: twelve rocks with a saucer held on screen for every frame, because M4's
cost is not one number but two — a flat one on every frame and a peak one while a
saucer is up (§12c).

Five things M4 settled that the document had not. The first is the largest thing
any milestone here has found, and it is not about Asteroids:

- **Adding code to a Logo program makes the code already in it slower**, because
  `find_global` scans the global table in creation order and a Logo file's
  `make`s run in file order. M4's regression was 2.2 ms in a loop with an
  unchanged diff. Where a `make` sits in the file is now a performance decision,
  the state block sits above the tuning block, and the fix in the *right* place —
  a hash or a move-to-front in `core/variables.c` — would pay every Logo program
  in the tree and is out of scope here (§12c).

The other three are the document being wrong rather than the code:

- **§11's thrust voice could not exist.** White noise on `[2 6]` is an *error* —
  those are tone voices — so the rumble is a narrow pulse instead, and the noise
  pair is left to the explosions, which must never be silenced by a held thrust
  key (§11.1).
- **The saucer's proportions are set by the collision bound, not by the arcade.**
  §7.3's bound read per axis rules out a flat saucer: a shot travels 11.4 steps
  between samples and a 7-step-tall target can be flown through. The alternative
  was a box half again as tall as the shape drawn in it — the exact failure this
  design has already had twice (§9.1).
- **The saucer's shot had to be slower than the player's**, for the same reason
  from the other side: the box it has to hit is a 9-step ship.
- **A name in this game is a global**, and M4 shipped a tuning constant
  `warble.b` and an alternation flag `warble.b` — one variable, not a shadow.
  The second `make` turned a frequency into `false`, and what it looked like from
  outside was `sound` being handed `false` 143 frames into a game. Found by the
  memory test, which read it as the frame loop dying early.

**M4 is done: built, measured three times, and played.** What is still open is
one number and it is not a gameplay one — §12e's unexplained worst frame.

The superseded question, kept because the answer is instructive: does the state
hoist buy enough?
The host says 9–10 % of an interpreted body; the board's body is part
rasterisation, so the saving there is smaller — a defensible 2.5–4 ms at twelve
rocks, which puts a quiet frame inside the budget and a saucer frame near but
probably still over it. If the next run says over, the choice is between the
three priced levers (`MAX.ROCKS` 12 → 11 at ~3.0 ms, the shot cap at ~2.0, the
large rock at 6 → 5 segments at ~1.4) and **13 fps**, which is a 7 % rate cut
that re-cuts every per-frame constant again and tightens all three tunnelling
bounds — they survive it, but only just: 160/13 = 12.3 against a 20-step box,
and the saucer's vertical bound goes to 17.6 against 18.

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
starts testing a stale position; a level ends when the board is clear, and a
saucer still crossing holds it open while spawning no replacement; the
shot-speed bound of §7.3; and the M2 harness — that it runs end to end, that it
measures the rock counts it reports, that it holds three shots live and the
board still, and that its frame matches the game's in drawing, physics, ship
and shot bookkeeping.

**Added for M3** (22 more, 71 in all): a rock on the ship kills it, and the
rock survives and is still drawn; **one frame takes only one life**, driven with
twelve rocks stacked on the ship, which is the test the parked-ship idiom exists
for; a ship inside its respawn grace cannot be hit on any frame of it, and *can*
be on the frame after; the explosion counts down while the rocks keep drifting,
and the last frame of it puts the ship back at the centre, stopped, facing
north and in a new grace; the last life ends the level and puts no ship back;
a dying ship draws one full ring and no hull, and the ring expands with the
countdown; the grace blinks on exactly half its frames; hyperspace stops the
ship and lands it inside the field, and sometimes kills it — the last of those
needs `(rerandom 1)`, because the mock device's hardware RNG returns a constant
and no chance in this game is observable on the host without a seed; the extra
ship at 10,000 including a score that steps *over* the boundary rather than
landing on it, and that a second one does not arrive at the same threshold;
a level advance adds a large rock up to the ceiling; the HUD line carries the
score, the level and one heart glyph a life; a dying ship answers only pause
and quit;
Q ends the game rather than the level; the state machine plays levels until the
ships run out and shows a card, and shows no card when the player quits; the
attract screen prints the score table and the keys; the game-over card prints
the final score; scoring repaints the HUD wherever the points come from; and — added after the play report — the ship's collision box
sits between the ship's own beam and the rock's longest spike plus that beam,
which fails at the `ship.rad` the board sent back. The harness-matches-the-game
test gained a **death frame**, because that is the branch M3 added to the frame.

**Added for M4** (26 more, 97 in all): both saucer outlines close and draw eight
segments, which is what a broken prologue turn would break — they are the first
shapes here that turn before they walk; `draw.saucer` reaches the outline its
size names; the boxes are held against the shapes actually drawn inside them, on
**both axes**, measured off the mock's own segment log rather than off the
constants; the two tunnelling bounds of §9.1, one in each direction; a saucer
arrives on its countdown, from an edge, at a height clear of the HUD; it crosses
in 150–220 frames and *leaves*, setting the wait for the next one, and it wraps
vertically while doing so; the wait shortens with the level to a floor; a small
saucer cannot appear below its level and appears about half the time above it;
a shot on a saucer takes the saucer, the score and the shot, at both sizes; a
saucer on the ship takes a life and the saucer, and takes nothing from a ship
inside its grace; a saucer shot kills the ship, expires on its own count, stops
and hides its turtle when it does, and **outlives the saucer that fired it**; the
small saucer fires within `sau.aim` of the ship's true bearing and the large one
does not, which is what fails if the heading conversion is mirrored; a saucer
aims at a ship it *cannot yet hit*, because a ship in its grace is parked at 9999
and aiming there would fire off the edge of the world; a frame with a saucer up
draws it and a frame without one does not; a level starts with no saucer and
nothing of the last one's in the air.

And the sound: the four timbres are set once and **match the voice kinds**, which
is the rule that makes §11's original table an error rather than a preference;
the heartbeat speeds up as the board thins and alternates two notes rather than
repeating one; firing zaps and a shot that was *not* fired does not; a rock's
death rises in pitch as the rock shrinks; a death is one long note on the noise
pair; the thrust rumble sounds on the frames thrust is held and on no others;
the warble runs while a saucer is up and stops when it leaves; and a level end
silences every voice while keeping the timbres. The harness-matches-the-game
test gained a **saucer frame**, because that is the branch M4 added to the frame,
and `test_the_m4_harness_keeps_the_saucer_off_the_plain_points` holds the thing
nothing on screen would show: `sau.first` is 140 frames against a 300-frame
point, so a saucer would otherwise average itself into three measurements that
are supposed to be without one.

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
  the ship or the shots against the same yardstick. M3 and M4 both added state
  that avoids it by construction — the ship, the saucer and the sampled shot
  positions are all plain `make` on globals, and the only lists left in a frame
  are the twelve rocks' — but "no list, no walk" is a property nobody has
  measured, only reasoned. The habit to keep: **count list walks, not
  statements.**
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
- ~~**What M3 adds to the frame is unmeasured.**~~ **Closed, and for once the
  estimate was nearly right**: the ship test is 0.245 ms a rock against ~1.5 ms
  estimated at twelve — 1.9×, where M0's drawing statement was 1.7× and M2's
  collision pass was 2.8×. Twelve rocks are 69.2 ms against 71.4 (§12b). The
  explosion ring came in at **0.90 ms** and replaces the ship rather than adding
  to it, so the "90 strokes inside one primitive call" argument (§9) holds.
- ~~**The worst frame is over the budget again, and it is the recycle.**~~
  **Closed by playing it, 2026-08-12: the hitch is undetectable.** 74 ms against
  71.4, with 69.2 + 4.1 accounting for it exactly — and a 4 ms slip once every
  1.8 s cannot be seen at 14 fps, so neither lever was spent. **It also refines
  the rule below.** "The worst frame must meet the budget" is the right test for
  an *unmeasured* worst frame, which is what it was written against: a spike
  nobody has decomposed can be any size and land anywhere. For a **known, priced,
  periodic** overshoot that `sync` turns into lateness rather than failure, the
  rule to apply is "**no unexplained frame may exceed the budget**" — and this
  one is explained to within 0.7 ms.
- **Collision tests ignore the wrap, and the playfield wraps** ([B19](bugs.md)).
  Every box test compares raw coordinates, so two objects three steps apart
  across an edge read as 317 apart and pass through each other. Drawing and
  motion both wrap, so only the arithmetic is wrong, in a band a few steps wide
  at each edge. The correction is a wrapped distance on both axes of every pair —
  about 24 extra statements a frame at twelve rocks, on a frame already at its
  budget — for correct behaviour in perhaps 5 % of the field. Left open
  deliberately.
- **A recycle scales with the workspace, and M4 is more workspace.** 1.3 ms at
  M1, 2.2 at M2, 4.1 at M3, for a frame whose per-frame allocation has barely
  moved since M1 (§14). The saucer and the sound push it further — the host's
  *deadline* moved 649 → 486 frames on the same evidence — and it is the first
  number M4's run should be read for after the frame itself.
- ~~**What M4 adds to the frame is unmeasured.**~~ **Closed, and it missed in a
  way no previous estimate had.** The saucer is 4.87 ms against ~2.7 (1.8×, the
  usual factor), but **two thirds of the overrun is not what M4 added at all** —
  it is the existing rock pass running 6 % slower for a lookup reason nobody had
  costed (§12c). 73.07 ms quiet and 77.94 with a saucer, against 71.4.
- ~~**`sound` has never been timed, at any milestone.**~~ **Closed: 0.2 ms a
  note**, about 0.03 ms a frame amortised over the heartbeat's period. §11's
  arrangement is not a frame-budget question.
- **The global table is a linear scan, and every Logo program in this tree pays
  it.** `find_global` walks the table in creation order, so a name defined late
  costs more to read than one defined early — 4.8 % on the rock pass for 40
  globals placed ahead of it, measured on the host. This design's response is
  file *order*, which is a workaround: it makes this game fast and leaves the
  next one to rediscover the same thing. The real fix is a hash or a
  move-to-front in [core/variables.c](../core/variables.c), which is interpreter
  work and out of scope for a design whose premise is that no interpreter work is
  needed — but it should be a P-number of its own, and the 4.8 % is the number to
  justify it with.
- ~~**A frame with a saucer on it may still not fit.**~~ **Closed three times
  over, the last of them by playing it** — "fully playable now with no jitter,
  very smooth game play", 2026-08-12, with a twelve-rock saucer frame measuring
  1–2 ms over on a reachable board. A bounded overshoot that `sync` turns into
  lateness is not a defect, and this is the third milestone to establish that.
  The state hoist was worth **3.72 ms** on the board — inside its predicted
  2.5–4 — taking twelve rocks to 69.35 and a saucer frame to 73.24; the hash
  index should be worth about 1.6–2.0 more. No gameplay lever was spent, and 13
  fps was not needed: it would have re-cut every per-frame constant again and
  left all three tunnelling bounds inside by under a step.
- ~~**The global table is a linear scan.**~~ **Fixed, in the interpreter**, which
  is where this design said the fix belonged and then nearly failed to do because
  its own premise says no interpreter work is proposed. A hash index costs 512
  bytes of SRAM — the one resource this project is short of — and pays every Logo
  program in the tree.
- **The worst frame is only partly explained, and the leading suspect is the
  sound.** 78 ms quiet against a 69.35 mean, with a 3.5 ms recycle accounting for
  less than half of it. Two host-testable explanations are dead (no cost drift
  between recycles; a recycle costs the same whatever it reclaims), which leaves
  a board-only cause — and the PSG's DMA is the one thing M4 added to a running
  frame loop. §12e has the two-minute experiment that would settle it.
- **Every name in this game is a global, and M4 collided two of them** — a
  `warble.b` frequency and a `warble.b` flag, which is one variable and not a
  shadow. It surfaced 143 frames into a game as `sound` being handed `false`, and
  it was found by the *memory* test rather than by anything looking at sound. The
  habit that catches the next one is a `grep` before a new `make`, and the reason
  it is a real hazard rather than a slip is that this file now has 65 procedures
  and about 70 globals sharing one namespace with no scoping to lean on.
- ~~**`ship.rad` is a feel constant with a play report waiting on it.**~~
  **Closed by playing it, 2026-08-12, and it was too generous — for the second
  time in this design and from the same end.** Ten steps killed before rocks
  reached the ship, worst on the medium and small ones; it is **6** (§8), which
  puts the box just inside where the rock's longest spike meets the ship's beam.
  The pattern is worth naming, because it has now happened twice: **a collision
  constant chosen as a safety margin is generous in pixels and *ruinous* in
  proportion on the smallest object**, and the report always comes from the
  small end. `shot.reach` 4 → 2 was the same correction at M2. The remaining
  untested side is the opposite failure — a rock that visibly clips the ship and
  does not kill it — which the new bound allows by up to half a step and which
  no report has yet described.
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
