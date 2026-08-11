# Asteroids in Pico Logo (design)

Status: **no game code. §16 M0 is built and green on the host, and has not
been run on a board** — which is the gate on everything after it.

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

The clear does pay something back: resetting the dirty state means the present
that follows sends only the tiles the redraw touched. Netted out against §12's
typical scene, it is still not close:

| | clear + redraw | erase in place |
|---|---:|---:|
| clear | ~25 | — |
| drawing | 4.4 (one pass) | 8.8 (two passes) |
| present | ~5 (only what was drawn) | ~9 (dirty rows) |
| rest of body | 9.1 | 9.1 |
| **frame** | **~43.5, and it flashes** | **~26.9** |

The erase pass is the cheap half of that: **re-tracing every rock costs
4.4 ms; wiping the screen costs 25.** A vector frame is thin outlines over a
tiny fraction of 102,400 pixels, and that sparsity is the whole reason
clearing is the wrong instrument here.

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
cost moved into a full-screen 25.6 ms present, so ~39 ms typical and ~53 ms at
twelve rocks. Still behind erase-in-place at both ends, but no longer
disqualified, and it is the simpler code. That is why §3.3 keeps measuring it
rather than dropping it.

### 3.1.2 `sync`, not `refresh`

Worth stating because the two questions look like one: the refresh *mode* is
orthogonal to the erase *strategy*. `sync` mode is manual mode plus pacing —
drawing accumulates off-screen either way — so nothing about clear-and-redraw
would require plain `refresh`, and `refresh` alone would leave the game
free-running at a variable frame rate. This design uses `sync` under every
strategy in §3.

### 3.2 Erase in place

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
at **1.6–2.7 ms** for the shipped games. §12's arithmetic says the doubled
drawing costs far less than the 25 ms it saves.

Three properties make it safe:

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

### 3.3 M0 measures both

Erase-in-place is the design's default and everything below assumes it. But
"~25 ms" and "1.6–2.7 ms" come from other programs' workloads, and
clear-and-redraw is genuinely simpler code — no ordering rule, no overlap
artefact, no stale-state class of bug. So **M0 builds neither game and
measures both strategies on a static scene of 6, 9 and 12 rocks** before any
game logic is written.

The comparison is only meaningful with B16 fixed, since unfixed the clear
flashes and the question is settled on appearance rather than time. B16 is
fixed (§3.1.1), so M0 measures the real thing.

The number that decides it is **how close a 12-rock dirty region gets to the
full screen**. Twelve scattered rocks over a row-span tracker could plausibly
dirty most of it, at which point erase-in-place is paying for two drawing
passes and getting a full-screen present anyway, and the simpler strategy
wins. If that is what M0 finds, this document is wrong and §3.2 comes out.

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

### Rock state: nine parallel flat lists

```logo
make "rx    [0 0 0 0 0 0 0 0 0 0 0 0]    ; centre x            (float)
make "ry    [...]                        ; centre y
make "rdx   [...]                        ; velocity, steps/frame
make "rdy   [...]
make "rang  [...]                        ; current rotation, degrees
make "rspin [...]                        ; degrees/frame, may be negative
make "rsize [...]                        ; 0 free, 1 small, 2 medium, 3 large
make "rshape[...]                        ; 1..3, which outline
make "rrad  [...]                        ; collision radius, from size
```

Fixed length `MAX.ROCKS` = 12, mutated in place with `.setitem`, never rebuilt:
a frame that moves twelve rocks must allocate nothing (§14). `rsize` = 0 is
the free-slot marker, so allocation is a scan for the first zero.

Parallel flat lists rather than a list of records because `item` and
`.setitem` are the only indexing this Logo has, and a record would cost a
second level of both on every field access.

## 6. Drawing the vectors

### 6.1 Shape procedures are straight-line literals

Each rock outline is a Logo procedure of nothing but `fd`/`rt` with **literal
arguments**. No variables, no arithmetic, no `item`, no loop:

```logo
to rock.a.l                      ; outline A, large
  pu fd 22 rt 126 pd             ; centre -> first vertex, then face the walk
  fd 17 rt 41  fd 13 rt 52  fd 15 rt 29  fd 19 rt 56
  fd 14 rt 37  fd 16 rt 48  fd 12 rt 44  fd 18 rt 53
end
```

This shape is the whole performance argument. On the board an arithmetic
statement costs 48–98 µs and a bare primitive call with a literal is a
fraction of that ([interpreter-throughput §11.1](interpreter-throughput-design.md));
computing a vertex would cost more than drawing it. So there are **nine
procedures — three outlines × three sizes — with the scale baked into the
literals**, rather than one procedure that multiplies by a radius.
(`setmag` is not an alternative: it scales a turtle's *appearance* and
`stamp`, and explicitly does not change how far the turtle moves.)

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
unclosed rock leaves a gap that the erase still takes away correctly but that
looks broken. The script's output is pasted into the game file; it is not run
on the board, and `test_asteroids.c` walks all nine and checks each one
arrives back at the vertex it started from, which is the one property of a
pasted-in block of literals that a bad paste would break.

The numbers carry one decimal place. That is still a single literal token, so
it evaluates at exactly the cost of an integer, and it is what holds the
closure error under half a pixel — integer turns alone drift by two or three
across eight segments. There is **no turn after the final segment**: `place`
sets the heading before every pass, so it would never be read, and dropping it
is what makes the statement counts below 19/15/13 rather than 20/16/14.

Segment counts fall with size, which is both authentic and exactly the right
place to spend the saving, since small rocks are the numerous ones:

| Size | Segments | Radius | Statements per draw pass |
|---|---:|---:|---:|
| Large | 8 | 22 | 19 |
| Medium | 6 | 14 | 15 |
| Small | 5 | 8 | 13 |

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
| ← | 180 | rotate left 12° |
| → | 183 | rotate right 12° |
| ↑ | 181 | thrust |
| space | 32 | fire |
| ↓ | 182 | hyperspace |
| p | 112 | pause |
| q | 113 | quit to the attract screen |

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

Invaders repaints its HUD only when a value changes. This game cannot: rocks
wrap through the whole field including the top band, so a rock's erase pass
will punch holes through the glyphs. The fix is one line of ordering —
**the HUD is drawn last in every frame, after every erase in that frame has
already happened**:

```logo
to draw.hud
  if not equal? :hud.text :hud.shown [
    ask 0 [pu setx -155 sety 148 setpc :bg.colour  write :hud.shown]
    make "hud.shown :hud.text
  ]
  ask 0 [pu setx -155 sety 148 setpc :hud.colour  write :hud.text]
end
```

Two costs, separated: the **erase** of a stale line runs only when a value
changes, and it is the only part that builds a `sentence` and therefore the
only part that allocates. The **repaint** runs every frame from an
already-built word, at four statements and no allocation. Lives are drawn as
that many `^` characters in the same string rather than as ship glyphs, which
keeps the whole HUD one `write`.

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

Board figures below are Plus 2 W with P10's `LOGO_HOT`, from
[interpreter-throughput §11](interpreter-throughput-design.md) and
[hardware-notes §2.3](hardware-notes.md):

| Unit | Cost |
|---|---:|
| bare `repeat` iteration | 4.5 µs |
| user procedure call | 21 µs |
| arithmetic statement (`make "x (:x + 1)`) | 48 µs net, 98.5 µs raw |
| primitive call with a literal argument (`fd 17`) | **assumed 35 µs — M0 measures this** |
| full-screen present | 25.6 ms |
| one 16 × 320 tile row presented | 1.26 ms |
| dirty-sprite-tiles present, shipped games | 1.6–2.7 ms |

**The table above counts drawing statements and not the dispatch that reaches
them.** Nine outlines behind a size test and a shape test is a user procedure
call plus about four statements — perhaps 200 µs — and erase-in-place pays it
**twice per rock per frame**, which at twelve rocks is a line item the size of
the collisions. M0 measures it separately for exactly that reason. If it comes
in dear, the lever is to store each rock's outline as one of nine flat lists
rather than as a size and a shape to be tested.

The literal-argument primitive call is the number this design leans on hardest
and the one nobody has measured. It is bracketed by the 21 µs call and the
48 µs arithmetic statement, and 35 µs is the midpoint. If it turns out to be
48 µs, the drawing lines below grow by 37 % and the worst case stops fitting
40 ms — which is why M0 measures it first and why the target rate is 20 fps.

**Typical mid-level frame** — 1 large, 2 medium, 3 small rocks, 2 shots, no
saucer:

| Line item | ms |
|---|---:|
| rock drawing, erase + draw, 224 statements | 7.8 |
| rock physics, 6 × ~10 statements | 4.2 |
| ship physics + erase/draw | 2.0 |
| collisions, 12 shot×rock + 6 ship×rock | 2.7 |
| shots: life countdown, position readback | 0.8 |
| HUD, input | 0.4 |
| **body** | **17.9** |
| present, ~6 rocks' worth of dirty rows + one straddler | ~9 |
| **frame** | **~27** |

**Worst case** — 12 small rocks, 3 shots, saucer alive:

| Line item | ms |
|---|---:|
| rock drawing, 408 statements | 14.3 |
| rock physics | 8.4 |
| collisions, 36 + 12 tests | 7.2 |
| ship, saucer, shots, HUD | 4.5 |
| **body** | **34.4** |
| present | ~12 |
| **frame** | **~46** |

So: **`(setrefresh "sync 20)`, a 50 ms budget.** The typical frame has room
to spare and the worst case fits with 4 ms of margin. 25 fps is not chosen
even though the typical frame would carry it, because `sync` does not wait
when a frame overruns — it presents late, quietly — and that is exactly how
two games reached hardware at a third of their designed rate without anyone
noticing (P9 M0). A rate the *worst* frame meets is the only rate that can be
trusted without a per-frame profiler.

If M0's numbers come in worse, the levers in order: drop `MAX.ROCKS` to 10
(cuts the two largest lines by a sixth), cap shots at 2 (cuts collisions by a
third), drop the large rock to 7 segments.

## 13. Reduced-resource choices

| Arcade Asteroids | This port | Saving |
|---|---|---|
| up to 27 rocks on screen | `MAX.ROCKS` 12, splits fill free slots only | bounds the two largest frame costs |
| 4 shots, generous range | 3 shots, ~1.2 s life | a third off the collision pair count |
| 12-vertex rocks at every size | 8 / 6 / 5 by size | ~40 % off the drawing, and the small ones are the numerous ones |
| rocks shatter into drifting line fragments | 4-frame expanding `arc` ring | one primitive per frame instead of a particle system |
| both saucer sizes can coexist | one saucer at a time | one object, one collision pair |
| velocity-scaled hyperspace risk | flat 1-in-8 | no formula |
| ship has slight drag | none, plus a hard speed clamp | one statement a frame |
| 4 starting rocks, +2 a level to 11 | 3 starting, +1 a level to 5 | keeps the mid-level rock count near the typical-frame budget |

None of these touch the interpreter. Like the other three games, this is pure
Logo against primitives that already exist.

## 14. Memory

The Galaxian rule applies unchanged: **a frame must allocate nothing.** Every
rock field is `.setitem` into a pre-built flat list; positions and velocities
are floats held in those lists; `wrapc`'s output is a number, not a cell.

The two places that do allocate:

- `draw.hud` builds a `sentence` when a displayed value changes — a kill, a
  death, a level. Galaxian measured this shape at ~14 cells a repaint.
- Level setup rebuilds nothing; it writes into the existing lists.

So `play.frame` calls `reclaim`, which runs `recycle` every 250 frames —
twelve seconds at 20 fps, never per frame. `test_asteroids.c` pins both
halves: zero cells over 100 quiet frames, and a bounded count over 100 frames
of continuous scoring.

## 15. Main loop

```logo
to play.frame
  poll.input
  if not :paused [
    make "frame.count (:frame.count + 1)
    erase.all              ; pe pass over rocks, ship, saucer, rings
    step.all               ; physics: rocks, ship, saucer, shot timers
    check.hits             ; collisions, splits, scoring, deaths
    draw.all               ; pd pass over the survivors
    heartbeat              ; tempo from the live rock count
    reclaim
  ]
  draw.hud                 ; last, always, over everything (§10)
  sync
end

to play.level
  setup.level
  (setrefresh "sync 20)
  make "over false
  until [:over] [
    play.frame
    if :dying [handle.death]
    if :rocks.alive = 0 [make "over true]
  ]
  setrefresh "auto
end
```

Two orderings are load-bearing and neither is obvious:

1. **`erase.all` strictly precedes `step.all`.** The erase must run against
   the state that drew the pixels. Every other arrangement — erasing lazily,
   erasing per object interleaved with its own update — is a way of getting
   this wrong.
2. **`check.hits` sits between `step.all` and `draw.all`**, so a rock that
   dies this frame is never drawn, and one that splits has its children drawn
   in the same frame they appear.

`play.frame` is a procedure and not the body of the `until`, so a test or a
timing harness can call exactly what the game runs — the correction P9 M0
forced on Galaxian.

## 16. Milestones

**M0 — measure, before any game code.** Built and on the host tests
(`logo/tests/p11rocks`, `tests/test_asteroids.c`); **not yet run on a board**,
which is the gate. It draws a static scene of 6, 9 and 12 rocks and, for each,
times clear-and-redraw against erase-in-place with body and present read
apart; then calibrates a drawing statement, an arithmetic statement and a bare
`repeat` iteration in the same run — so §12's assumed 35 µs is bracketed
within one machine rather than against P10's numbers from another — and the
shape dispatch §12's table does not count. The decision line is the 12-rock
dirty present against the full-screen present the clear strategy pays.
Results go to a **file** on the board, not the screen. B16 (§3.1.1) is fixed,
so both strategies measure as they would ship. M0's output either confirms
§12 or rewrites §3.

Four things it does *not* do, each deliberate: it holds the scene still (a
rock steps a couple of pixels and rotates between the two passes, but both
stay inside the same 16-pixel tiles, so the dirty region is the same to within
a tile, and the body figure stays free of physics M1 will measure properly);
it presents with `refresh` rather than `sync`, since the number wanted is the
work and not the cadence; it clears with `clean` rather than `cs`, which would
restore automatic refresh and silently end the measurement; and it runs
`fullscreen`, because `screen_gfx_blit_dirty` returns immediately in text mode
and a present measured at the prompt is zero — the correction P9 had to make
to its entire first series of numbers.

**M1 — rocks only.** The 12 slots, the nine shape procedures, wrap, spin,
erase-in-place, `sync` at 20 fps. Nothing to shoot with. This is the frame
budget's real test; everything after it is cheaper per unit than what M1
already carries.

**M2 — ship and shots.** Rotation, thrust, momentum, firing, shot×rock
collisions, splitting, scoring, the HUD.

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

**In already, for M0:** the scene tables are all twelve long and each subset
holds an equal mix of sizes; all nine outlines close on themselves and draw
the segment count §6.3 claims; the walk out to the first vertex does not draw
(or every rock would wear a spoke); the dispatch reaches the outline it names;
an erase pass retraces its draw pass segment for segment and differs only in
the pen colour — §3.2's failure mode, and the reason the eraser is a colour;
every rock is drawn with a one-pixel pen (§3.2); and the harness runs end to
end and its report reaches the file, because a script that dies half way
through wastes a board session.

**To come with the game:**

- `wrapc` at both edges, exactly on the boundary, and beyond one full width.
- The split table: large → 2 medium, medium → 2 small, small → nothing; child
  velocity inherits the parent's; the slot cap yields 2 / 1 / 0 children as
  slots allow, and never writes past `MAX.ROCKS`.
- `hit?` — inside, outside, and on each edge of the square.
- Scoring, including the 10,000-point extra ship, and level advance when the
  last rock dies.
- Ship physics: the speed clamp holds at the boundary; heading wraps through
  0 and 360 in both directions.
- **The erase/draw invariant**, extended from M0's version to a moving scene.
  The intended shape — run N frames, run the erase pass alone, assert the
  canvas is empty — **is not available**: the mock's canvas is a staged
  fixture that the sensing and tile ops read, and drawn lines do not
  rasterise into it, so there are no leftover pixels to find. What is
  available is stronger for this purpose anyway: the mock records every
  segment, so the test asserts the erase pass retraces the draw pass segment
  for segment. A stale-state ordering bug shows up as a mismatched pair, and
  names which rock and which segment.
- Allocation: zero cells over 100 quiet frames (§14).

## 18. Risks

- **The literal-call cost is unmeasured** (§12). Everything else in the budget
  rests on it. M0 exists for this.
- **Present cost in a wrap-mode vector game is unmeasured too.** The shipped
  games' 1.6–2.7 ms is for compact sprites over a static background; scattered
  thin strokes across twenty tile rows, some of them straddling, is a
  different shape of dirt. It could plausibly be 6 ms or 15 ms, and §12
  guesses 9–12.
- **Erase-in-place is a new technique in this tree.** The HUD does it for one
  fixed line of text; nothing does it for a dozen moving polygons. §17's
  canvas-empty test is the mitigation.
- **Feel is the game, and it is all constants.** Rotation rate, thrust
  impulse, speed clamp, shot speed and life, small-saucer accuracy. Isolated
  at the top of the file, expected to need on-hardware iteration, and not
  knowable from the host.
- **A pico2 is unmeasured.** Scaling M5's Plus 2 W numbers by P10's 1.72× puts
  the worst frame near 79 ms, which does **not** fit 20 fps. If the pico2
  matters, `MAX.ROCKS` becomes a per-board constant — but note that nobody has
  measured either shipped game there either.
