# Asteroids in Pico Logo (design)

Status: **M0 and M1 done, both measured on a Plus 2 W on 2026-08-11.** M0
rewrote this document — the frame clears and redraws rather than erasing in
place (§3.3), the rate is 15 fps rather than 20 (§12), the outlines are three
rather than nine (§13). M1 is the game with rocks only, and it **fits: 60.5 ms
at twelve rocks against a 66.7 ms budget**, `frame = 30.5 + 2.507 n`. It also
shows **M2 opening about 6 ms over**, which is M2's first decision (§12).

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
| Tests | `tests/test_asteroids.c` (Unity + mock device), mirroring `tests/test_galaxian.c` — **written**, covering the M0 harness until there is a game to cover |
| Design | this document |
| Measurement | `logo/tests/p11rocks`, a timing harness in the shape of `logo/tests/p10games` — **written, not yet run on a board**; it writes its numbers to a file, because numbers on a display cannot be copied off it |
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
make "rrad  [...]                        ; collision radius, from size
```

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
to place :i
  pu  setx (item :i :rx)  sety (item :i :ry)  seth (item :i :rang)
end
```

Because the walk is entirely turtle-relative, `seth` before it rotates the
whole polygon about the stored centre. A rotating rock costs exactly what a
still one costs. This is the single largest thing turtle graphics buys over a
stamped costume, which would need one bitmap per angle.

`setx`/`sety` and not `setpos`, following Invaders and Galaxian: `setpos`
takes a list, and building one per object per frame allocates.

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

Five strokes: a triangle plus the two rear notch lines, drawn the same way.
Thrust adds a sixth and seventh — the flame — on frames where thrust is held,
alternating on/off every other frame so it flickers as the arcade one does.

```logo
to draw.ship
  pu fd 10 rt 145 pd  fd 15 rt 110  fd 8 rt 110  fd 15 rt 145  fd 10
end
```

## 7. Motion and input

### 7.1 Rocks

Constant velocity, wrapped:

```logo
to step.rock :i
  .setitem :i :rx  (wrapc ((item :i :rx) + (item :i :rdx)))
  .setitem :i :ry  (wrapc ((item :i :ry) + (item :i :rdy)))
  .setitem :i :rang ((item :i :rang) + (item :i :rspin))
end

to wrapc :v
  if :v > 160 [output (:v - 320)]
  if :v < -160 [output (:v + 320)]
  output :v
end
```

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
  make "svx (:svx + (0.55 * (sin :sh)))
  make "svy (:svy + (0.55 * (cos :sh)))
  make "spd (sqrt ((:svx * :svx) + (:svy * :svy)))
  if :spd > 7 [make "svx (:svx * 7 / :spd)  make "svy (:svy * 7 / :spd)]
end
```

The speed clamp is what stops the ship becoming unplayable, and it is the
first constant to tune on hardware. `sin`/`cos` take degrees and Logo's
heading is clockwise-from-north, which is what `seth` wants — so the ship's
heading variable and its drawing heading are the same number, with no
conversion anywhere.

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
nose, `pu`, `st`, `setspeed 220`. From then on the engine flies and wraps it
— `setspeed` "obeys `wrap`, `window` and `fence` exactly as `forward` would"
— and Logo's only per-frame duty is counting the shot's life down. `pu` is
mandatory: a shot turtle with its pen down would draw a permanent trail
across the canvas.

## 8. Collisions and scoring

Every test is a circle overlap on numbers Logo already holds. Written as
nested `if`s with a cheap reject first, so the common case is one comparison:

```logo
to hit? :ax :ay :bx :by :r
  if (abs (:ax - :bx)) > :r [output "false]
  if (abs (:ay - :by)) > :r [output "false]
  output "true
end
```

A square test, not a circle: it is two statements instead of a squared-
distance expression, and against a jagged rock whose outline is nowhere near
its bounding circle anyway, the extra reach in the corners is not perceptible.
This is the same "close enough, and stated" reasoning as Invaders' ±10 bitmap
anchor tolerance.

The pair counts are the frame's second-largest line item (§12), so the tests
are folded into loops that already exist:

| Pair | Where | Worst-case tests/frame |
|---|---|---:|
| shot × rock | inside the rock loop, over the ≤3 live shots | 36 |
| ship × rock | same loop, one more test | 12 |
| shot × saucer | once per shot | 3 |
| ship × saucer, ship × saucer shot | once each | 2 |

Shot positions are read once a frame with `ask :n [xcor]` / `[ycor]` into a
flat list, never re-read inside the rock loop.

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

**A real `play.frame` on a Plus 2 W**, 300 frames a point, `logo/tests/p11m1`:

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
call and to rule out 20.

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
come. So M2 opens with a decision rather than a keyboard, and the levers are
now priced against a measured slope of 2.507 ms a rock:

| Lever | Saving |
|---|---:|
| `MAX.ROCKS` 12 → 10 | **5.0 ms** |
| segregate rocks into three per-size lists (kills the dispatch) | ~2.2 ms |
| cap shots at 2 rather than 3 | ~2.5 ms |
| large rock 6 → 5 segments | ~1.4 ms |

Any two of the first three clear it. **The first is the one that costs the
game something** — ten slots makes the split cap bind more often — so it is
the last to reach for, not the first.

So: **`(setrefresh "sync 15)`, a 66.7 ms budget** — down from the 20 fps this
document opened with, and the change is not a tuning preference but the
present. A full-screen present is 26.3 ms whatever the scene holds, so **half
of a 50 ms frame is gone before a rock is drawn**, and no game-side lever
reaches it. 20 fps is available only at seven or eight rocks, which makes the
split cap bind on almost every kill and takes away the thing Asteroids is
about — a board that fills up. Twelve rocks at 15 fps keeps the game and
misses smoothness; eight rocks at 20 fps keeps smoothness and misses the game.

**M1 confirmed the rate and left it with no room above.** The worst frame at
twelve rocks is 67 ms against a 66.7 ms budget, so 15 fps is the highest rate
this game can be trusted at — and `sync` presents late rather than failing, so
an overrun costs frame rate and not correctness. That graceful failure is
exactly why P9's two games shipped at a third of their designed rate with
nobody noticing, and exactly why every number in this section is measured
rather than argued.

A rate the *worst* frame meets is the only rate that can be trusted without a
per-frame profiler, which is the lesson P9 M0 paid for.

## 13. Reduced-resource choices

| Arcade Asteroids | This port | Saving |
|---|---|---|
| up to 27 rocks on screen | `MAX.ROCKS` 12, splits fill free slots only | bounds the two largest frame costs |
| 4 shots, generous range | 3 shots, ~1.2 s life | a third off the collision pair count |
| 12-vertex rocks at every size | **6 / 5 / 4** by size | 1.9 ms at twelve rocks; the small ones are the numerous ones |
| three rock outlines at every size | **one outline per size** | 2.2 ms at twelve rocks — the dispatch collapses to a single three-way test in one procedure, with no second call. The nine generated outlines stay in the file; rotation already varies how a rock reads in motion, and the variety comes back the moment M1's measurements say it can be afforded |
| 60 fps | **15 fps** | the present is 26.3 ms whatever the scene holds (§12) |
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
and 1.7 s at 15 fps. `test_the_reclaim_interval_stays_inside_the_atom_budget`
measures the deadline rather than assuming it and fails if the interval creeps
back towards it.

**A recycle costs 1.3 ms**, measured on a Plus 2 W (`p11m1`), so recycling ten
times more often than the design first said costs 0.05 ms a frame amortised
and puts a 1.3 ms bump inside a 66.7 ms budget every 1.7 s. It is invisible,
and the tighter interval is free. Free storage over 900 frames: 27,017 →
27,015.

The rule to write to is **"a frame must not allocate anything it does not hand
back, and must hand it back long before the deadline"** — no `sentence`, no
`list`, no `fput` on the frame path. The HUD text is rebuilt only where a
displayed value changes, which at M1 is level setup and nothing else;
`wrapc` outputs a number rather than a cell. `test_asteroids.c` pins both
halves: zero cells over 100 quiet frames, and a bounded count over 100 frames
of continuous scoring.

## 15. Main loop

```logo
to play.frame
  poll.input
  if not :paused [
    make "frame.count :frame.count + 1
    step.all               ; physics: rocks, ship, saucer, shot timers
    check.hits             ; collisions, splits, scoring, deaths
    clean                  ; buffered in sync mode since B16 (§3.1.1)
    draw.all               ; one pass over the survivors
    draw.hud               ; the clean took it too, so it goes back every frame
    heartbeat              ; tempo from the live rock count
    reclaim
  ]
  sync
end

to play.level
  setup.level
  (setrefresh "sync 15)
  make "over false
  until [:over] [
    play.frame
    if :dying [handle.death]
    if :rocks.alive = 0 [make "over true]
  ]
  setrefresh "auto
end
```

**This loop is three procedures shorter than the one this design opened
with**, and that is clear-and-redraw's second dividend after the 20 ms. There
is no erase pass, so there is no rule that the erase must run against the
state that drew the pixels — the ordering bug §3.2 called this design's
signature failure mode cannot be written. There is no stale-state class of bug
at all: every frame draws the world from the world's current state, and
nothing on the canvas outlives a frame.

One ordering is still load-bearing, and it is the obvious one:
**`check.hits` sits between `step.all` and `clean`**, so a rock that dies this
frame is never drawn, and one that splits has its children drawn in the same
frame they appear.

One that is *no longer* load-bearing, and is worth naming because §10 was
built around it: the HUD does not have to be drawn last. It is drawn last only
because that reads in the order things appear.

`play.frame` is a procedure and not the body of the `until`, so a test or a
timing harness can call exactly what the game runs — the correction P9 M0
forced on Galaxian.

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
`tests/test_asteroids.c` (23 tests), and `logo/tests/p11m1`, which times a
real frame at 6, 9 and 12 rocks with the body and the present read apart —
the split P9 M5 wished it had.

Two things M1 settled on the host before the board saw it. It **disproved this
document's memory rule** — an Asteroids frame does allocate, ~36 atoms a
frame at twelve rocks, and the contract is a flat working set rather than a
zero (§14). And the harness has to spell `play.frame` out again minus its
`sync`, because the present must be timed on its own and `sync` is the last
thing the frame does; `test_the_harness_frame_matches_the_game_frame` drives
both from one state and requires the same drawing and the same physics, since
a harness frame that drifts from the game measures a game nobody plays.

**M2 — ship and shots.** Rotation, thrust, momentum, firing, shot×rock
collisions, splitting, scoring, the HUD. **It opens over budget by about
6 ms** on M1's measured slope (§12), so the first thing it does is spend two
of the four priced levers — and `MAX.ROCKS` is the last of them to reach for,
because ten slots is the only one that costs the game something.

**M3 — lives, levels, deaths.** Ship explosion, respawn, level advance,
attract screen, game over, hyperspace.

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

**To come with the rest of the game:**

- `wrapc` at both edges, exactly on the boundary, and beyond one full width.
- The split table: large → 2 medium, medium → 2 small, small → nothing; child
  velocity inherits the parent's; the slot cap yields 2 / 1 / 0 children as
  slots allow, and never writes past `MAX.ROCKS`.
- `hit?` — inside, outside, and on each edge of the square.
- Scoring, including the 10,000-point extra ship, and level advance when the
  last rock dies.
- Ship physics: the speed clamp holds at the boundary; heading wraps through
  0 and 360 in both directions.
- **The frame draws the world and nothing else.** Run N frames on the mock,
  then `clean` and one `draw.all`, and assert the recorded segments match the
  previous frame's exactly. Under erase-in-place this test was the file's most
  valuable, because a stale-state ordering bug showed up as leftover pixels
  and as nothing else; under clear-and-redraw there is no stale state to get
  wrong, and this is a cheap regression guard rather than a mitigation.
- Allocation: zero cells over 100 quiet frames (§14).

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
- **Feel is the rest of the game, and it is all constants** — and 15 fps
  changes all of them. Rotation rate, thrust impulse, speed clamp, shot speed and life,
  small-saucer accuracy are all per-*frame* quantities against a frame that is
  now 66.7 ms rather than 50, so every one of them needs re-cutting by a third
  before it is even worth iterating on. This is the hazard flagged for
  Galaxian in P10's log (per-frame constants against per-second `setspeed`
  motion) arriving from the other direction. Isolated at the top of the file,
  expected to need on-hardware iteration, and not knowable from the host.
- **The physics third of the frame is still estimated** (§12), and the worst
  case fits by 0.5 ms. M1 measures it. This is now the largest open number in
  the design.
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
