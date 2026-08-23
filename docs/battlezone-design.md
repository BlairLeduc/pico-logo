# Battlezone in Pico Logo (design)

Status: **M0 MEASURED on a Plus 2 W, 2026-08-23. The gate is passed and the
game is built at L0.** `frame = 40.09 + 8.043 n` for the typical frame and
`51.87 + 7.635 n` for the worst, so the design's scene — three obstacles and one
enemy — is **64.2 ms against a 66.7 ms budget at 15 fps**, with the worst frame
at 74.8 and over it. Two levers close that and both were already in this
document or are free: **culling the horizon** (§8.4, measured at 10.8 ms) and
**moving the hot path off `local` and onto globals** (§13 L0.5, measured at
1.31× on the projection, 3.9 ms). With both: **49.5 ms typical, 60.1 worst**,
and 18 fps is in reach.

M0 rewrote §3, §8.4, §10, §12 and §13, and §16.2 has the whole result. The
headline corrections:

- **A long line is not nearly free.** §10 inferred from `screen_gfx_line` that a
  200-step edge would cost about what a 17-step one does. It costs **twice** —
  130 µs against 66 — and that was this document's largest unmeasured number.
- **The 180× ratio holds, but §3's unit did not.** The board's arithmetic
  statement is **53.5 µs**, not the 43 this document budgeted, because the
  budget's figure was P11 M0's and the *host* side of the calibration measured a
  different construct (§3.1). Like for like the ratio is 167×.
- **A box projects in 3.145 ms against 2.16 predicted; the enemy in 5.18 against
  6.0.** The column trick (§8.2) beat its estimate; the box did not.
- **The split-screen present saves 8.05 ms**, better than §6's 6.6.
- **The harness's own body column came back 0.00 ms** and had to be recovered
  from the min/max columns, for a reason worth §16.1's paragraph: `measure`'s
  accumulator was called `b`, and `cam.setup` writes a global `b`.

Every board figure below is now a reading. Where an estimate survives it is
marked as one.

[Asteroids](asteroids-design.md) was the first game in this tree that was not a
sprite game. Battlezone is the first that is not a **2D** game, and the thing
that makes it interesting is that the 1980 cabinet and this one have the same
problem in the same place: an XY monitor traced a display list, and Pico Logo's
turtle *is* a display list. What the arcade machine had that we do not is a
Math Box — an AMD 2901 bit-slice coprocessor sitting beside the 6502 for the
sole purpose of doing the matrix arithmetic the 6502 could not. §13 is this
design's version of that question, and M0 answered it: **no Math Box is
needed.**

---

## 1. Deliverables and source of truth

| | |
|---|---|
| Game | `logo/games/battlezone` — one Logo file, no extension, no `-` or `/` in the name so `load "battlezone` parses |
| Tests | `tests/test_battlezone.c` (Unity + mock device), mirroring `tests/test_asteroids.c` |
| Design | this document |
| Measurement | `tests/logo/p13m0`, and its Plus 2 W run is kept verbatim in [`measurements/p13m0-plus2w-2026-08-23.txt`](measurements/p13m0-plus2w-2026-08-23.txt). **Run on one board of three; a Pico 2 W has not seen it (§16.2).** Times a real frame at 1, 2, 4 and 8 visible objects, with the projection pass read apart from the drawing pass and the present read apart from both. It writes its numbers **to a file**, because numbers on a display cannot be copied off it. `tests/test_p13m0.c` (23 tests) proves it is worth carrying to a board: the projection against hand-computed coordinates, the culling in both directions, and that the script reaches its last line with the report in the file |
| Model generator | `scripts/gen_models.py`, host-side, output pasted in (§8.3) |

Play: `load "battlezone` then `battlezone`.

All three boards. Nothing here needs WiFi, TLS or PSRAM, so `LOGO_HAS_WIFI` and
`LOGO_HAS_TLS` are not consulted anywhere in the game.

## 2. What the game is, mechanically

The arcade rules, kept:

- You are a tank on a flat plain, seen through a periscope. You have two
  treads: left and right, forward and reverse each. Both forward is drive,
  opposed is rotate on the spot.
- The plain is scattered with **obstacles** — cubes and pyramids — that block
  movement and block shots, and can be hidden behind.
- The horizon carries a **mountain range**, a **volcano** and a **crescent
  moon**. They are at infinity: they scroll with your heading and never with
  your position. They are the only way to tell that you are turning.
- One enemy at a time: a **tank** (hunts you), a **supertank** (faster,
  smarter), a **missile** (flies straight at you, dodgeable), or a **saucer**
  (drifts, harmless, worth points).
- Both you and the enemy fire one shell at a time. A hit is an explosion of
  drifting line fragments and a pause.
- Being hit **cracks the screen** — a static shatter overlay that stays until
  you respawn.
- Radar in the top centre: a sweeping wedge over a circle, with a blip for the
  enemy.

Reduced or removed in §14, but the list above is the game.

## 3. The calibration, and what M0 did to it

Before M0 every board figure here was `host × 180`, calibrated against P11 M0's
board numbers. **The ratio survived; the unit it was applied to did not.**

### 3.1 What the ratio was, and the mistake inside it

P11 M0 priced four units on a Plus 2 W
([asteroids-design.md §3.4](asteroids-design.md#L267)); the right-hand column is
what M0 read on the same board on 2026-08-23:

| Unit | P11 M0 | **P13 M0** |
|---|---:|---:|
| arithmetic statement (`make "x :x + 1`, `x` a `local`) | 42–44.5 µs | **53.5 µs** |
| bare `repeat` iteration | 4.5–7 µs | **5 µs** |
| drawing statement, 17 steps, pen down | 59.5–60.3 µs | **66 µs** |
| drawing statement, 200 steps, pen down | not measured | **130 µs** |
| present, full screen | 26.3 ms | **27.25 ms** |
| present, 240 rows (`splitscreen`) | not measured | **19.2 ms** |

**The 180× was derived from a host loop that did not match the board loop.**
This document's host figure — 0.24 µs — came from `repeat 200000 [make "x :x + 1]`
typed at top level, where `x` is a **global**. The board harness, like P11's,
puts the same loop inside a procedure where `x` is a **`local`**. Measured
like for like on the host, a local costs **0.32 µs**, so the ratio is
`53.5 / 0.32 = 167×` — inside 8 % of the 180 this document used, and the
estimates built on it stand.

What does not stand is §12's 43 µs unit. **The game's hot loops use locals, so
the unit is 53.5 µs**, 1.24× more, and that is most of why the projection
figures came in above their predictions.

### 3.2 One number M0 could not explain

P11 M0 measured 42–44.5 µs for this statement on this board on 2026-08-11.
`calib.arith` in `tests/logo/p11rocks` and `time.arith` in `tests/logo/p13m0`
are the same four lines with the same `local`. Twelve days later it is 53.5.

**1.24× on the interpreter's most-used unit is not a rounding difference, and
this design cannot say which way it goes.** Either something between those dates
made a local arithmetic statement slower — which would be a regression touching
every game in the tree, not just this one — or the two harnesses differ in a way
neither file makes visible. `p11rocks` is still in the tree and still runs, and
re-running it on the same board is a ten-minute experiment that settles it. It
is logged as an open question (§19.5) rather than assumed away in either
direction.

Suggestive but not conclusive: on the host, a `local` costs 0.32 µs against a
global's 0.213 — **1.5×** — and P11 M4 gave `find_global` a hash index while the
local path kept its frame walk. If the same gap exists on the board it is a
lever (§13 L0.5) whether or not it is also the answer here.

## 4. The central decision: where the projection pipeline runs

### 4.1 What a frame has to compute

Per vertex, camera at `(px, pz)` with heading `ph`, focal length `k`:

```
dx = wx - px                       2 statements
dz = wz - pz
zc = dz*cos(ph) + dx*sin(ph)       1 statement   (cos/sin hoisted per frame)
if zc <= near: cull                1 statement
iz = k / zc                        1 statement
sx = (dx*cos(ph) - dz*sin(ph)) * iz  1 statement
sy = (wy - eye) * iz               1 statement
```

Seven statements a vertex, and every one of them costs 43 µs whether it
multiplies two floats or copies a word. **That is the whole problem.** The
arithmetic is nothing — an RP2350 does single-precision multiply in a cycle and
divide in about fourteen — and the interpretation is everything.

The prototype runs. A 40×40 box at world `(0,150)` with the camera at heading
30°, printed as `x, y_bottom, y_top` per ground column:

```
-112 -20 46
-101 -25 59
-209 -30 71
-194 -23 53
```

Correct perspective — the nearer columns have the larger vertical spread — and
correct culling: the same box at `(0,-150)` reports `culled` on all four
columns. `window` clips the `x = -209` corner off-screen without distorting the
line. **Nothing in §4.1 needs a new primitive.** The question is only what it
costs.

### 4.2 Measured in Logo, on the host, scaled — and what the board said

A full frame kernel — 32-point horizon, twelve obstacles culled, every
survivor projected, one ten-vertex enemy tank — was written in the idiom this
tree uses (flat parallel global lists, `item` with `repcount`, constants
hoisted) and timed at **260 µs a frame on the host**. Scaled: **46.8 ms**.

Broken into parts, and the parts reconcile with the whole to 3 %:

| Part | Host | Board (×180) |
|---|---:|---:|
| horizon, 32 azimuth points | 56 µs | 10.1 ms |
| cull 12 obstacles | 28 µs | 5.0 ms |
| project one box (4 ground columns) | 12 µs | 2.16 ms |
| project enemy tank, 10 vertices | 60 µs | 10.8 ms |
| *(9 of the 12 obstacles survive the cull)* | | |
| **sum of parts** | | **45.3 ms** |
| **whole frame, measured** | 260 µs | **46.8 ms** |

**The board's per-box figure came in at 3.145 ms against this table's 2.16, and
the enemy at 5.18 against 10.8** — the estimate that was too low and the one
that was too high, and §3.1's 53.5 µs unit accounts for the first.

Add a 27.25 ms full-screen present and the naive frame is 73 ms before a single
line is drawn, before input, physics, collisions, radar, HUD or sound. **The
naive frame does not fit.** Three structural decisions close it, and none of
them is a compromise on what the game is.

### 4.3 The three things that close it

**Ground columns, not vertices** (§8.1). Battlezone's obstacles are
axis-aligned boxes and pyramids standing on a flat plain. All four corners of a
box share the same two rotated half-offsets, `s·cos(ph)` and `s·sin(ph)`,
hoisted once a frame for every box in the world; and the top and bottom
vertices of a column share both `sx` and `1/z`. **Eight vertices cost four
transforms and one divide each.**

**Whole-object near culling, not edge clipping** (§9). The thing that normally
makes 3D painful in an interpreter is the edge that straddles `z = 0`: it
projects to garbage and needs a real clip. Battlezone does not need one. Cull
any object whose nearest column is inside the near plane — you cannot drive
into an obstacle anyway, and the arcade's own tank stops you.

**A 240-row viewport** (§6). Split mode presents 240 rows, not 320, and the
saving is 6.6 ms for nothing.

### 4.4 The decision, and M0's answer to it

The decision taken on 2026-08-21 was to **build L0 — the whole game in Logo, no
new primitives — and gate at M0**, with §13's four tiers of interpreter help
priced but unspent. The reason for not spending them early is P11 §12's, which
made the mistake twice: it priced three levers, spent none, and then found the
real cost somewhere none of them reached.

**M0 ran on 2026-08-23 and L0 holds** (§16.2). The typical frame is 64.2 ms
against 66.7, and the two levers that close the worst frame are the horizon cull
this document already specified and a change of variable scope that costs
nothing. **No new primitive is needed and none is being built.** §13 keeps the
tiers because the game may still grow into them, and L4 remains the answer if a
richer scene is ever wanted — but it is not this game's answer.

## 5. B48 — the single-statement line to a computed point (fixed)

This was the one thing that blocked M0, and it was found by trying to write the
draw pass.

A projected wireframe is a sequence of arbitrary screen points. Drawing it
wants one statement per point, with the pen down. Pico Logo had three
candidates and all three failed:

- **`(setpos x y)`** — documented at
  [Pico_Logo_Reference.md:1201](../reference/Pico_Logo_Reference.md#L1201) as a
  variadic form, and **not implemented**. `prim_setpos` was `REQUIRE_ARGC(1)`
  registered with arity 1, so `(setpos 10 20)` answered *"setpos doesn't like
  10 as input"*. Behaviour that contradicts the reference is a bug, so this was
  **B48** rather than a roadmap item.
- **`setpos list :x :y`** — works, and allocates. Measured: **two cons cells a
  call**. A frame drawing 70 edges mints 140 cells; at 15 fps that is 2,100 a
  second against a 32,752-cell pool, so the game would need a `recycle` every
  fifteen seconds, and a recycle is a visible hitch. (P11 §12b found a recycle
  frame is the worst frame in Asteroids, and that is Asteroids spending five
  cells a frame, not 140.) Driving 100,000 of these on the host without a
  reclaim reaches *"Out of space"*, which is the demonstration.
- **`setx :x sety :y`** — two statements, and **geometrically wrong**: each
  moves on one axis only, so with the pen down it draws an L, not a line.
  Asteroids never noticed because it only ever uses the pair with the pen
  **up**.

**Fixed 2026-08-23** ([bugs.md](bugs.md)): `prim_setpos` gained an `argc == 2`
branch beside its existing one, in the shape `prim_arctan` already uses for its
two-input form. Six tests in `test_primitives_turtle.c`, three of which fail on
the old code — the form is accepted, a pen-down `(setpos 100 50)` draws **one**
line from (0,0) to (100,50) rather than two segments, and its cost does not
grow with the iteration count while `setpos list :x :y` measurably does.

**`setpos` was the only primitive affected.** `towards`, `dot`, `dot?`,
`setcursor` and `settextcolor` also take an xy list and the reference documents
no variadic form for any of them, so they are consistent with their
documentation and were left alone. Giving them one would be a feature; this
design does not need it.

## 6. The viewport is 240 rows, and that is worth 8.05 ms

`clean` in manual or `sync` mode calls `dirty_tiles_mark_all()`, so a
clear-and-redraw frame presents everything the mode makes visible. In split
mode that is not everything:

```c
int limit = (screen_mode == SCREEN_MODE_SPLIT) ? SCREEN_SPLIT_GFX_HEIGHT
                                               : SCREEN_HEIGHT;
...
if (y0 >= limit)
    break;  // Spans iterate top-down; the rest are in the text area
```
— [screen.c:996](../devices/picocalc/screen.c#L996)

`SCREEN_SPLIT_GFX_HEIGHT` is 240. So `splitscreen` costs 15 tile rows instead
of 20: predicted **19.7 ms instead of 26.3**, and M0 read **19.2 against
27.25 — an 8.05 ms saving, better than the 6.6 predicted**, with the prediction
for the split half itself inside half a millisecond. And the bottom eight text lines are exactly
where the cabinet puts the score and the "ACTIVATE" prompts, so this is free
authenticity rather than a sacrifice.

**The consequence for geometry.** Pixel row is `-y + 160`
([picocalc_console.c:812](../devices/picocalc/picocalc_console.c#L812)), so the
presented band is rows 0–239, which is turtle **y ∈ [−79, 160]**. The optical
centre of the viewport is therefore `y = +40`, not `y = 0`, and every screen-space
constant in this design is cut against that:

- horizon at `y = 40`
- viewport 320 wide × 240 tall, half-width 160, half-height 120
- `k = 260` gives a 63° horizontal field of view, close to the cabinet's

**Checked at M0 and clean.** The 80 rows `clean` marks and the present never
sends cost nothing measurable: the split present is flat at 19.2–19.8 ms across
every point of the series, from one object to eight, exactly as a mode-level
constant should be.

## 7. The world and the camera

- The plain is 4,000 × 4,000 turtle steps, and it **wraps** — drive far enough
  in one direction and the obstacle field repeats, which is how the arcade's
  unbounded plain was faked and is one modulo a frame rather than a boundary.
- The camera is `(px, pz, ph)` with the eye 12 steps above the plain. There is
  no pitch and no roll: a tank stays flat, which removes an entire axis from
  every transform and is not a simplification of the original.
- Motion is per-tread. `left.tread` and `right.tread` are each −1, 0 or +1 from
  the key state; the pair drives forward speed `(l + r)` and turn rate
  `(r − l)`. That is the cabinet's dual-stick control on a keyboard, and it is
  two statements.
- `cos ph` and `sin ph` are hoisted into `:cs` and `:sn` **once a frame**, which
  is the single most important hoist in the file: it is the difference between
  two trig calls a frame and two per vertex.

## 8. The models

### 8.1 A box is four columns, not eight vertices

An obstacle is axis-aligned and stands on the ground, so its eight vertices are
four `(x, z)` ground columns lifted to two heights. Per frame, once, for all
boxes:

```
make "a :half * :cs
make "b :half * :sn
```

Per box, the four columns are `(dx±a, dz±b)` and `(dx±b, dz∓a)` — eight adds,
no multiplies. Per column: one compare (near cull), one divide, one multiply
for `sx`, and two more for the two heights, which **share the divide**.

A pyramid is better still: four ground columns and one apex, and the apex is
the box's own centre column lifted, so it costs one extra divide.

### 8.2 The enemy is the expensive object and it is the one that rotates

An enemy tank has its own heading, so its vertices need a real rotation before
the camera transform — two more multiplies and two more adds a vertex, and a
`cos`/`sin` pair a frame for the enemy. Measured at ten vertices: **10.8 ms**,
the largest single item in the frame.

Two levers, both taken:

- **Eight vertices, not ten.** The arcade tank is a hull box, a turret box and
  a gun line. Eight vertices reads as a tank at this resolution.
- **The hull is columns too.** A tank hull is a box that happens to be rotated,
  so §8.1's trick applies with the enemy's own `cos`/`sin` in place of the
  world's. Only the turret and gun need general vertices.

Estimated at **6.0 ms**, and it is an estimate: unlike the box figure, nothing
has measured a column-form tank. M0 measures it.

### 8.3 Models are generated, not hand-typed

As P11 §6.3 did for the rock outlines: `scripts/gen_models.py` emits the vertex
and edge lists, and the output is pasted into the game file. Hand-typing a
vertex table is how you get a tank with one edge going to the wrong corner and
no way to see which.

### 8.4 The horizon is at infinity and is the most expensive thing in the frame

The mountain range, the volcano and the moon have no depth: screen x is
`(azimuth − ph) × scale`, wrapped, and screen y comes straight from a stored
table. No divide, no rotation — and **M0 measured a 32-point scan at 13.845 ms**,
against this section's estimated 10.1, because four statements a point at
§3.1's 53.5 µs is what four statements a point costs.

**That is 21 % of a frame spent on a backdrop, and it is the single largest
line item after the present.** The cull stops being an optimisation and becomes
part of the design.

**How many points survive it? Seven, not the twelve this section first wrote** —
found while building M0's harness, and arithmetic rather than measurement.
Thirty-two points over 360° at 5.06 steps a degree sit 57 steps apart, and a
320-step viewport holds about seven, which
`test_the_horizon_cull_keeps_about_seven_of_thirty_two_points` pins.

At M0's measured **0.433 ms a point**, culling to seven is **3.0 ms and saves
10.8**. That is the largest single lever this design has, it costs two
statements to find the first and last visible index, and it is what makes the
budget in §12 work.

Seven is **too coarse to be a mountain range**: one peak every 9° of a 63°
view. So the table wants to be denser, and the cull is what makes density
affordable — a 96-point table culled to ~21 is 9.1 ms and does not fit, while
the same table scanned whole is 41 ms and is absurd. **The honest reading is
that the table's density is bounded by this line item at about 40 points**, and
a range that needs more detail than that wants §19.1's tilemap rather than more
Logo statements.

## 9. Near culling replaces clipping

An object is drawn if **every** column has `zc > near`, and skipped otherwise.
Not "any column" — one corner behind you is enough to swing the projection
through infinity and throw a line across the whole screen.

This is wrong in exactly one visible way: an obstacle you are pressed against
vanishes rather than filling the view. The arcade prevents it with collision,
this game prevents it the same way, and the near plane sits just outside the
tank's own collision radius so the two can never disagree.

Lateral clipping needs nothing: `window` lets the turtle leave the screen and
`screen_gfx_line` skips out-of-bounds pixels per pixel
([screen.c:759](../devices/picocalc/screen.c#L759)), so a line from `x = -209`
to `x = 40` draws its visible half and costs the rest only in loop iterations.
**`wrap` would be a disaster here** — a wireframe that wrapped would smear
across the screen — so the game sets `window` at startup and never changes it.

That is easier to write than to remember. M0's harness was built from this
document and did not set it, and the default folded a horizon point at
x = −170.8 round to +149.2 and stroked a 263-step segment across the whole
view. It looked like a projection fault. `window` is now a top-level statement
in the harness with this paragraph's reason attached, and
`test_no_horizon_segment_spans_the_whole_screen` sweeps the camera through 360°
in 5° steps so no boundary mode can quietly come back.

## 10. Drawing

`clean` and redraw, for the reason P11 §3.3 measured: a scattered vector scene
dirties most tile rows either way, so an erase pass buys almost nothing and
costs a second traversal. A wireframe horizon line spans the full width by
itself, which settles it before the argument starts.

An edge is `(setpos x y)` with the pen down — **one statement, one straight
line, nothing allocated** — which is what B48's fix bought. A closed quad is one pen-up
`setpos` and four pen-down ones. Per box: **12 edges in 20 statements** — this
section first guessed sixteen, and the four it missed are the pen-ups the
vertical edges need, since a vertical does not start where the last one ended.
`test_a_box_draws_twelve_edges` pins the count.

**Line length is not nearly free, and this was the largest unmeasured number in
the document.** The inference was that `screen_gfx_line` marks its dirty region
**once, from the accumulated bounding box** rather than per pixel
([screen.c:748](../devices/picocalc/screen.c#L748)), and that the inner loop is
only a bounds test and a byte store — so a 200-pixel edge should cost barely
more than the 17-pixel `fd` P11 measured at 60 µs.

**M0 says 130 µs against 66.** The dirty marking is indeed once per line; the
per-pixel loop is what costs, at about **0.35 µs a step** over the 183 extra
steps. The macro does a bounds test, two comparisons against the accumulated
box and a byte store per pixel, out of flash, and that is not free at 150 MHz
however cheap it looks in C.

So the estimate of 55 edges at 60 µs — 3.3 ms — was **half** of what a
wireframe's drawing costs, and this section's "should" was doing more work than
it admitted. The measured drawing pass is §12's biggest correction after the
horizon.

**One consequence for the game rather than the budget:** an edge's cost is
proportional to its length on screen, so a near object costs more to draw than a
far one, and the frame gets more expensive as the player closes on something.
That is the opposite of what a culling budget usually does and it is worth
knowing before a level is laid out.

## 11. The rest of the frame

**Collisions** are cheap because there are so few pairs: your shell against the
enemy and against each obstacle, the enemy's shell against you, and you against
each obstacle. With eight obstacles in the table that is ~18 box tests, and
B19's warning applies — the plain wraps, so the comparisons must wrap too, and
this game gets that right from the start rather than logging it as Asteroids
did.

**The radar** is one `arc` (a C-side primitive, ~0.2 ms), a swept wedge that is
two `setpos` lines, and a blip. ~3.0 ms with the bearing arithmetic.

**The gunsight** is a fixed overlay: about eight lines from a straight-line
procedure with no arithmetic in it at all, exactly as P11 §6.1 writes an
outline. ~1.5 ms with the HUD text.

**Sound** on the PSG: a two-voice engine idle whose pitch follows tread speed,
a noise burst for the cannon, a pitched explosion, and the rising two-tone
alarm when an enemy is in front of you. The arrangement follows
[sound-design.md](sound-design.md) and P11 §11's lesson — the alarm is a
*tempo* as much as a pitch, and that is what makes a hunting supertank
frightening. Budgeted inside the 3.0 ms line below.

## 12. Frame budget — measured

At **15 fps — a 66.7 ms budget**. M0 measured 200 frames at each of 1, 2, 4 and
8 visible objects plus the enemy, in `splitscreen`, presenting with `refresh`:

| Objects | Typical frame | Worst frame |
|---:|---:|---:|
| 1 | 49 ms | 57 ms |
| 2 | 56 | 69 |
| 4 | 71 | 84 |
| 8 | 105 | 112 |

**`frame = 40.09 + 8.043 n` typical and `51.87 + 7.635 n` worst**, both fitting
all four points. At this design's scene — three obstacles and one enemy —
that is **64.2 ms typical and 74.8 worst**.

Read apart, an object costs **7.80 ms**: 3.67 to project and 4.14 to draw. The
flat term splits as 19.8 present, 13.8 horizon, 5.5 enemy projection and 2.5
enemy drawing.

| | Predicted | **Measured** |
|---|---:|---:|
| present, 240 rows | 19.7 | **19.8** |
| horizon, 32 points scanned whole | 3.9 *(assumed culled)* | **13.8** |
| project 3 obstacles | 6.5 | **11.0** |
| draw 3 obstacles | *(in "edges")* | **12.4** |
| project the enemy | 6.0 | **5.5** |
| draw the enemy, 13 edges | *(in "edges")* | **2.5** |
| everything else in the flat term | 11.7 | **−1.0** |
| **total at three obstacles** | **51.7** | **64.2** |

**12.5 ms over, and 10.8 of it is the horizon** — which this document always
said would be culled and which the harness deliberately did not cull, so that
the lever could be priced. The rest is §3.1's unit: 53.5 µs against 43 is 1.24×,
and the projection lines carry it almost exactly.

### 12.1 The two levers that close it, both measured

**Cull the horizon: −10.8 ms** (§8.4). Two statements to find the first and
last visible index, seven points instead of thirty-two. This was always the
plan.

**Move the hot path off `local`: −3.9 ms** (§13 L0.5). `project.box` holds seven
locals and `project.enemy` sixteen, and on the host a `local` read costs 0.32 µs
against a global's 0.213. Rewritten with globals, the projection measures
**1.31× faster** for an identical result. It is free, it needs no interpreter
change, and it is the one lever this document did not think of.

**With both: 49.5 ms typical and 60.1 worst**, against 66.7. That is 17 ms of
headroom on the typical frame and 6.6 on the worst — more than Asteroids
shipped with, and enough that **18 fps (55.6 ms) is worth trying at M4** for the
typical frame, though not for the worst.

§14's two gameplay levers stay unspent and are re-priced at the measured slope:
**obstacles 3 → 2 is −8.04 ms**, and the horizon table 32 → 20 points is −5.2
before the cull and −1.7 after it.

**The present is 30 % of the closed frame and no game-side lever reaches it.**
Same finding as P11 §3.3, and it generalises: on this display a vector game pays
a fixed tax a sprite game does not.

## 13. Interpreter levers, priced

None of these is ruled out. Each is stated with what it buys *this* game, what
it costs to build, and — the test that matters — whether it is worth having
without this game.

**M0's verdict: none of them is needed.** L0.5 below is not an interpreter
change at all, and with it and the horizon cull the game fits. The tiers stay
because the game may grow into them and because two of them are worth having on
their own merits, but nothing here is on this game's critical path.

### L0.5 — globals instead of locals in the hot path

Not an interpreter change. A change to how the game is written, found by M0 and
worth more than any of the tiers below.

P11 M4 gave `find_global` a hash index, and nothing did the same for the local
path, which still walks the frame. On the host a `local` read costs **0.32 µs
against a global's 0.213 — 1.5×** — and §3.1 shows that same gap is most likely
what separates M0's 53.5 µs arithmetic statement from P11 M0's 42–44.5.

Rewriting `project.box`'s seven locals as globals measures **1.31× on the whole
projection**, host, for a bit-identical result. On the measured board figures
that is **3.9 ms off a three-obstacle frame** (§12.1).

The price is real and worth stating: every hot-path temporary becomes a global,
in a language with one flat namespace and no shadowing, which is exactly the
hazard Asteroids' header spends a paragraph on and the hazard that cost this
design M0's body column (§16.1). The mitigation is a prefix — `pb.` for
`project.box`'s temporaries, `pe.` for `project.enemy`'s — and a test that reads
the names back out of the source.

**Buys this game:** 3.9 ms, and it is half of what the gate needed.
**Verdict: take it**, with the naming discipline, at M1.

**And it generalises.** Every game in this tree uses `local` in its frame loop.
If the board confirms the host's 1.5×, that is a tree-wide finding and belongs
in the roadmap as an interpreter question — memoising a local's slot on the atom
the way M2 did for procedure names — rather than as a Battlezone note. §19.5.

### L1 — small maths: `sincos`, `min`, `max`

`(sincos a)` outputting `[sin cos]` saves two statements a frame here and
nothing anywhere else. **Not worth it.**

`min` and `max` are a different matter: they are absent from the reference
entirely, every dialect has them, and clamping is three statements without them
and one with. They belong in the roadmap's *cheap wins* table on their own
merits, and this game is a user of them rather than a reason for them.

**Buys this game:** ~0.3 ms. **Verdict:** add `min`/`max` as a cheap win,
independent of Battlezone. Skip `sincos`.

### L2 — arrays

Already on the roadmap, already deferred, with the note *"O(1) indexing; needs
a new object kind (likely blob-backed). Wait for demonstrated need."*

Measured on the host, `item` costs about **0.09 µs fixed plus 0.0041 µs an
element**; scaled, **16 µs plus 0.73 µs an element**. So `item 12` of a
twelve-list is ~25 µs and `item 128` of a 128-list is ~110 µs. (P11 M2's
often-quoted "~115 µs for a twelve-element walk" is a pre-P10-M5 figure and no
longer holds; the interpreter got much faster underneath it.)

Battlezone's frame does roughly 44 `item` calls on lists of 8 to 32 in the
*projection*, and M0 showed the *drawing* pass does far more: `draw.box` reads
two `item`s per `setpos`, forty a box, and at ~18 µs each that is **0.72 ms of
the measured 3.82 ms box** — 19 %.

But arrays do not fix that, and this is the useful part of the finding: the cost
is the **fixed** ~16 µs of an `item` call, not the walk, because these lists are
four elements long. O(1) indexing removes the part that is already almost
nothing. What removes the rest is not reading the list at all — which is L4.

**That is an honest and disappointing answer, and it is the one to record.**
Arrays are a good language feature; at this scene size they are not this game's
lever. They become one only if the model tables grow past ~64 entries, which is
what happens if L4 is *not* taken and the game grows anyway.

**Buys this game:** under 0.5 ms, and M0 makes that firmer rather than
softer. **Verdict:** Battlezone is not the demonstrated need the roadmap is
waiting for, and now there is a board measurement saying why.

### L3 — a projection operation

`setview` fixing the camera in C, and `project` transforming one point:

```
setview :px :pz :ph          ; once a frame; C computes and keeps sin/cos
make "p project :wx :wy :wz  ; once a vertex
```

Collapses seven statements to two. About 32 projections a frame, saving ~5
statements each: **~6.9 ms**.

**And it has an interface problem this design cannot solve cleanly.** An
operation outputs one object. A projected point is two numbers. Outputting
`[sx sy]` allocates a cons pair per vertex — 32 a frame, 480 a second, which is
B48's disease with a different spelling. The alternatives are worse: a
`projectx`/`projecty` pair that recomputes, or a primitive with a side effect on
two globals, which is not a Logo operation at all.

**Buys this game:** ~6.9 ms, minus an allocation it reintroduces. **Verdict:**
the awkwardness is a signal. If the pipeline is going into C, the natural seam
is not one point — it is one model.

### L4 — a model display list, in the shape of the tilemap family

```
newmodels 8                          ; allocate slots
setmodel 1 [x y z x y z ...] [edges] ; define once, at load
setview :px :pz :ph                  ; once a frame
drawmodel 1 :ox :oz :oh              ; once per object per frame
```

`drawmodel` transforms, near-culls, projects and strokes a whole wireframe in
C, in the current pen colour, through the existing turtle device op — so it
honours `window`, the dirty tracker and `sync` exactly as `fd` does, and
composes with everything drawn around it.

**What it is worth.** Boxes (6.5 ms), enemy (6.0) and their edge statements
(~2.5 of the 4.0) collapse to four `drawmodel` statements plus the C work: 4 ×
43 µs, ~40 vertices at well under a microsecond each, and the same
rasterisation that was being paid anyway. Call it **0.5 ms against 15.0 — a
14.5 ms saving, and it is the only lever here that changes what the game can
be.** The budget in §12 buys three obstacles and one enemy; with L4 it buys
twelve obstacles, a detailed enemy, the missile and the saucer, at a higher
frame rate.

**What it costs.** Perhaps 250 lines of C, a slot table in `core/limits.h`
(24 vertices and 32 edges over 8 slots is about 3 KB — and SRAM is nearly full,
so this is a real cost and not a rounding error), reference sections, mock
device support and tests.

**The precedent, which is strong.** This tree has done exactly this twice.
`arc` is already a C-side multi-segment stroke. And P9's `stampmap` took Turtle
Trails' board build from **5,916 ms to 7.6 ms** by moving a pen-carved loop into
C — a 780× win that nobody argues made Trails stop being a Logo program.

**The objection, which is also real.** These games exist partly to show what
Logo on this machine can do, and a `drawmodel` primitive puts the interesting
part in C. There is a version of this where Battlezone is a thin Logo shell
around a 3D engine, and that game is worth less than a slower one written in
Logo.

**Verdict: not needed, and not being built.** M0 closed the gate at L0 (§16.2),
so this game does not spend it. It stays here as the answer to a different
question — *what would a richer Battlezone cost?* — and M0 sharpens that answer
rather than retiring it: at a measured 7.80 ms an object, twelve obstacles is
94 ms of objects alone and is unreachable in Logo at any frame rate worth
playing. **If this tree ever wants a 3D game denser than three obstacles, L4 is
not an optimisation, it is the enabling condition.** Worth opening as a roadmap
item on its own merits — a vector-3D primitive family for *any* game, which is
how the tilemap family was justified — and worth doing on a day when someone
wants that game.

### The one that is not on this list

A general bytecode body for the evaluator, which P10 considered and rejected.
Nothing here reopens it: L4 gets 14.5 ms for 250 lines of C, and a bytecode
rewrite would have to beat that against the whole interpreter's risk.

## 14. Reduced-resource choices

Cut from the arcade, with the reason:

| Cut | Reason |
|---|---|
| Multiple simultaneous enemies | The frame is linear in visible vertices. The arcade sends one enemy at a time anyway |
| The obstacle field's true density | Eight in the table, three visible. §12's largest single lever if it misses |
| Ground texture / detail below the horizon | The cabinet has none — the plain is empty black. Free authenticity, again |
| Pitch and roll | A tank stays flat. Removes an axis from every transform |
| The Math Box's smooth object rotation at range | Objects pop in at the far cull distance rather than fading |

Two gameplay levers held in reserve for M0, priced at §12's rates: **visible
obstacles 3 → 2** (−2.2 ms) and **horizon points 32 → 20** (−1.5 ms).

## 15. Memory

The rule P11 §14 established holds: **a frame allocates**, and the contract is
a flat working set rather than zero, because `.setitem` of a number interns it.
Battlezone's per-frame minting is roughly the camera constants, the projected
coordinates the game keeps, and nothing else — smaller than Asteroids' 36 atoms
a frame at twelve rocks, because there are fewer objects.

`reclaim` on a countdown, as every game in this tree now has, and B48's fix is
what keeps the cons pool out of it entirely (§5).

Everything else is fixed-size flat global lists set up once at load: the
obstacle field, the model tables, the horizon profile.

## 16. Milestones

**M0 — measure, before any game code.** `tests/logo/p13m0`, on both a Pico 2 W
and a Plus 2 W, because §3's ratio was calibrated on one board and the
`pico2` preset does not carry P10 M5's flash tiering.

It answers five questions, in this order. **B48 is fixed, so the draw pass it
measures is the one the shipped game would run** — which was the precondition,
since a harness written around an allocation the game would not make measures a
game nobody plays:

1. **What does a long line cost?** A 200-pixel pen-down `(setpos x y)`, 1,000
   times, against P11's 60 µs for a 17-pixel `fd`. §10's largest unmeasured
   number.
2. **What does the split-mode present cost?** `clean` + a full redraw +
   `refresh` in `splitscreen`, against §6's predicted 19.7 ms and P11's
   measured 26.3 full-screen.
3. **Does §3's 180× hold on a board?** Re-run the two calibration loops. If the
   ratio is not ~180 on the Plus 2 W, every estimate in this document moves
   together and §12 is re-derived rather than re-argued.
4. **What does one box actually cost?** The column-form projection of §8.1,
   against 2.16 ms.
5. **What does the column-form enemy cost?** §8.2's 6.0 ms is the estimate this
   design is least confident in.

Four things M0 must not do, taken from P11 M0's list because each was learned
the hard way there: it presents with `refresh` and not `sync`, since the number
wanted is the work and not the cadence; it clears with `clean` and not `cs`,
which would restore automatic refresh and silently end the measurement; it runs
in `splitscreen` deliberately and records that it did, because
`screen_gfx_blit_dirty` returns immediately in text mode and a present measured
at the prompt is zero; and it holds the scene still, so that a moving object
does not quietly change the dirty region between passes.

**The gate.** §12 predicts 51.2 ms. If M0's re-derived budget is under 66.7,
build the game at L0. If it is between 66.7 and 71.4, take the two gameplay
levers in §14 and re-measure. If it is over 71.4, **stop and choose from §13**,
and the choice is L4.

### 16.1 What building the harness already found

Three things, none of which needed a board, and all of which are the argument
for writing the harness before the game rather than beside it:

- **The horizon cull keeps seven points, not twelve** (§8.4). Arithmetic, not a
  measurement — and it turns the table's density into a design decision instead
  of an accident.
- **`window` is not optional and is easy to forget** (§9). The harness was
  written from this document, did not set it, and drew a line across the screen.
- **A box is 20 statements, not 16** (§10). The four pen-ups the verticals need.

- **`local` does not protect the world from a name** — the defect that cost M0's
  body column, and it is worth more than a bullet. `measure`'s body accumulator
  was called `b`; `cam.setup` declares no locals and does `make "b :half * :sn`;
  Logo is dynamically scoped, so the callee's `make` found the caller's `local`
  and wrote the accumulator once a frame, with `sn` at 0 and the camera at
  heading 0. **Every body figure on the first board run came back 0.00 ms**, and
  nothing about the report looked wrong — the present, the split and the min/max
  columns were all correct, which is what made it survive review.

  It is not the hazard Asteroids' header warns about (one flat namespace, two
  meanings, one variable). It is the reverse: the names are in *different*
  scopes and the callee still reaches the caller's. Every accumulator in
  `measure` is now prefixed `m.`, and
  `test_the_frame_does_not_write_the_measure_accumulators` **reads the names
  back out of the Logo source** and probes each one, so renaming a temporary to
  `b` fails the test rather than quietly reintroducing the defect. A hardcoded
  list would have pinned the names; reading them pins the property.

  **The run was still usable.** `min` and `max` are computed from the same
  per-frame total the body column failed to accumulate, so §12's series is
  recovered from those two columns, and the projection/drawing split — measured
  inside `frame.body`, whose accumulator was never touched — reconciles with
  them to within 2 ms at every point. That is luck, and the fix is in.

And one thing about the harness's own honesty. The phase timers that split
projection from drawing are four statements an object — up to 36 a frame, about
1.5 ms at an arithmetic statement's 43 µs — which is a tenth of what the split
is trying to resolve. P11 M4 stated its own 0.15 ms and moved on; at ten times
that, stating it is not enough. So the series runs **two frames**: `frame.raw`,
which is what the frame costs, and `frame.body`, which is the same statements
with the timers in. The difference is reported as the instrumentation charge
rather than estimated, and `test_the_timed_and_untimed_frames_draw_the_same_thing`
is what stops the two drifting apart.

### 16.2 M0's result (Plus 2 W, 2026-08-23)

The five questions, answered:

| | Predicted | **Read** | |
|---|---:|---:|---|
| Q1 long line, 200 steps | ~60 µs | **130 µs** | §10 was wrong; length costs ~0.35 µs a step |
| Q1 short line, 17 steps | 60 µs | **66 µs** | P11's figure holds |
| Q2 present, `splitscreen` | 19.7 ms | **19.2 ms** | inside half a millisecond |
| Q2 present, `fullscreen` | 26.3 ms | **27.25 ms** | saving is 8.05, better than 6.6 |
| Q3 arithmetic statement | 43 µs | **53.5 µs** | §3.1; the ratio holds, the unit did not |
| Q3 bare `repeat` iteration | 5 µs | **5 µs** | exact |
| Q4 one box projected | 2.16 ms | **3.145 ms** | 1.46× over, and §3.1 accounts for it |
| Q5 the enemy projected | 6.0 ms | **5.18 ms** | the column trick beat its estimate |
| one box drawn, 12 edges | ~1.2 ms | **3.82 ms** | Q1 plus forty `item` reads |
| horizon, 32 points scanned | 10.1 ms | **13.845 ms** | and the cull is now mandatory |

**The gate.** §16 set it at: under 66.7 ms build at L0; 66.7–71.4 spend §14's
gameplay levers; over 71.4 stop and take L4. The typical frame at three
obstacles is **64.2 ms** and the worst is **74.8**. Taking the gate on the
typical frame — which is what "under 66.7" meant, and what P11 M3 and M4 both
took theirs on — **the answer is L0**, and §12.1's two levers bring the worst
frame to 60.1 as well. **No §14 lever is spent and no interpreter change is
made.**

**What M0 did not measure, and should have.** It ran on a Plus 2 W only. §3
says plainly that the `pico2` preset does not carry P10 M5's flash tiering and
that a Pico 2 will be slower by an unmeasured amount, and §16 Q3 exists to catch
exactly that. `p13m0` is unchanged and runs on any of the three boards; **M1
should not start until a Pico 2 W has run it**, because a 1.2× board difference
would put the closed frame at 59 ms and a 1.5× one would put it over.

**M1 — the world and the camera.** Plain, obstacles, tread controls, horizon,
the gunsight. No enemy, no shells, no radar. This is the milestone that proves
the projection is right, because a wrong transform is obvious the moment you
drive past a cube and it does not go where a cube goes. First hardware play
test: does driving *feel* like Battlezone at this frame rate?

**M2 — the enemy.** One tank, its hunt logic, both shells, collisions,
explosions, the radar. This is the frame-budget milestone — the one that
corresponds to P11's M2, which is the one that missed by 19.7 ms.

**M3 — the game.** Lives, scoring, the enemy sequence (tank → missile →
supertank → saucer), the cracked screen, the attract screen, the high score
table, sound.

**M4 — tuning.** Played, then cut.

## 17. Tests

`tests/test_battlezone.c` on the mock device, mirroring `test_asteroids.c`'s
shape. The ones that are specific to this game:

- **The projection is right.** Known camera, known world point, known screen
  coordinate, computed by hand. This is the test that would have caught a
  transposed `cs`/`sn` in the second term, which is the classic error and is
  invisible until you turn.
- **Culling is conservative.** An object with *any* column inside the near
  plane is skipped; one entirely in front is not. Both halves, because the
  "any" version is the one that throws a line across the screen.
- **The plain wraps in the arithmetic and not only in the drawing.** B19 is the
  precedent: Asteroids got the picture right and the comparison wrong, and it
  shipped that way.
- **The harness frame is the game's frame.** P11's
  `test_the_harness_frame_matches_the_game_frame` exists because a harness that
  drifts from the game measures a game nobody plays. Same test, from the start.
- **The horizon cull keeps the visible points.** Off-by-one at the field-of-view
  edge is a mountain that flicks in and out as you turn.

## 18. Risks

| Risk | Where it bites | What answers it |
|---|---|---|
| Long-line drawing cost is 2–3× the short-line figure | §12's 4.0 ms becomes 10 | M0 Q1, first thing measured |
| The column-form enemy does not hold its estimate | §12 by up to 5 ms | M0 Q5 |
| Split mode's unsent dirty rows cost something | §6's 6.6 ms saving shrinks | M0 Q2 |
| 180× does not hold on a Pico 2 | Every estimate here, on one of three boards | M0 Q3, run on both boards |
| The projection is subtly wrong in a way play reveals and tests do not | M1 | Hardware play test at M1, not M3 |
| The game needs L4 and L4 makes it a C game | §13's honest objection | Decided at M0's gate, with a number |

## 19. Open questions

1. **Does a P9 tilemap layer compose underneath turtle graphics?** If it does,
   the horizon is nearly free (§8.4) and this is the cheapest 3.9 ms in the
   document. Nothing in this tree establishes it either way.
2. **Is `splitscreen`'s text area usable while `sync` is driving the graphics
   half?** §6 assumes the score can live there and be written with `print`
   rather than costing graphics rows.
3. **Should the enemy's hunt logic run every frame?** At 15 fps a decision
   every third frame is invisible and costs a third as much. P11's saucer
   pacing is the precedent for taking the arcade's own numbers rather than
   inventing one.
4. **`min`/`max` (§13 L1)** — worth opening as a cheap win regardless of this
   game?
5. **Why is a local arithmetic statement 53.5 µs when P11 M0 measured 42–44.5
   on the same board twelve days earlier?** (§3.2.) Either the interpreter
   regressed — which would touch every game in the tree — or the two harnesses
   differ invisibly. `tests/logo/p11rocks` still runs and settles it in ten
   minutes. **This is the one open question that is not about Battlezone**, and
   it should be answered before M1 spends L0.5 on the strength of a host
   measurement.
6. **Does a Pico 2 W run this frame?** (§16.2.) M0 measured one board of three.
