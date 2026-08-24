# Battlezone in Pico Logo (design)

Status: **M0 MEASURED on all three boards, 2026-08-23. The gate passes at L0 on
every one of them, and M0 is closed.**

| | Pico 2 W | Plus 2 W | Pico 2 |
|---|---:|---:|---:|
| `frame = c + m·n`, mean | `44.18 + 7.167 n` | `45.62 + 7.736 n` | `46.62 + 7.998 n` |
| at three obstacles and one enemy | **65.7 ms** | **68.8 ms** | **70.6 ms** |
| worst frame at three | 70.6 | 73.7 | 73.5 |
| **with §12.1's two levers** | **51.6 ms** | **54.1 ms** | **55.1 ms** |
| worst frame with both levers | 56.5 | 58.9 | 58.0 |

Against a 66.7 ms budget at 15 fps. The two levers are **culling the horizon**
(−10.3 to −10.8 ms), which this document always specified and the harness
deliberately did not do so the lever could be priced, and **moving the hot path
off `local` onto globals** (−3.8 to −3.9), which is free. Neither is an
interpreter change. **No new primitive is needed and none is being built.**

All three boards ran the fixed harness and every figure below is a reading. The
two Plus 2 W runs reproduce to **within 1 %** on every number the budget uses.

**The Pico 2 first measured 116.4 ms at three obstacles — 85.3 even with both
levers — and it was not this game's fault.** `LOGO_HOT_IN_RAM` was off for the
`pico2` preset, so P10 M5's flash tiering was not in that firmware. Turning it
on took the same board, on the same day, from **116.4 to 70.6 ms — 1.65×** —
with both of the run's controls unmoved. §16.3 is the whole experiment, and it
is the strongest evidence for P10 M5's diagnosis in the tree.

M0 rewrote §3, §8.4, §9, §10, §12 and §13. The corrections, in the order they
matter:

- **A long line is not nearly free, and how unfree it is depends on the board.**
  §10 inferred that a 200-step edge would cost about what a 17-step one does.
  It costs **twice on a Plus 2 W (130–131 µs) and nearly four times on a
  Pico 2 W (248 µs)** — a per-step cost of 0.35 µs against 0.98, while the short
  line is identical on both. This is the largest board difference in the run, it
  runs *opposite* to every other one, it **reproduces across two Plus 2 W runs
  to 0.8 %**, and it is unexplained (§19.7). It has a design
  consequence: §9's near plane is now cut for edge length, not just for the
  projection singularity.
- **The Pico 2 W is 5–11 % faster than the Plus 2 W at interpreting**, which is
  the reverse of what §3 expected — and §3's expectation was right about the
  board it actually named. The untiered `pico2` build is 2.1–2.4× slower than
  either (§16.3).
- **The 180× ratio holds; §3's unit did not.** The board's arithmetic statement
  is 48.5–53.5 µs, not 43, because this document's *host* figure measured a
  global at top level while every board harness measures a `local` inside a
  procedure (§3.1). Like for like the ratio is 167×.
- **A box projects in 2.97–3.195 ms against 2.16 predicted; the enemy in
  4.97–5.4 against 6.0.** The column trick (§8.2) beat its estimate on every
  board.
- **The split-screen present is 19.2–19.8 ms on all three boards**, against
  §6's predicted 19.7 — the closest prediction in the document, and the run's
  most useful control.

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
| Measurement | `tests/logo/p13m0`, with all five board runs kept verbatim under [`measurements/`](measurements/): `pico2w`, `plus2w…b`, `pico2` and `pico2-tiered` (the two halves of §16.3's controlled experiment), and the first `plus2w` run, kept because §16.1 is about it and its `body` column is void. **M0 is closed (§16.3.1).** Times a real frame at 1, 2, 4 and 8 visible objects, with the projection pass read apart from the drawing pass and the present read apart from both. It writes its numbers **to a file**, because numbers on a display cannot be copied off it. `tests/test_p13m0.c` (23 tests) proves it is worth carrying to a board: the projection against hand-computed coordinates, the culling in both directions, and that the script reaches its last line with the report in the file |
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

| Unit | P11 M0 (Plus 2 W) | **P13 M0, Pico 2 W** | **P13 M0, Plus 2 W** |
|---|---:|---:|---:|
| arithmetic statement (`make "x :x + 1`, `x` a `local`) | 42–44.5 µs | **48.5 µs** | **53.5 µs** |
| bare `repeat` iteration | 4.5–7 µs | **4.5 µs** | **5 µs** |
| drawing statement, 17 steps, pen down | 59.5–60.3 µs | **68 µs** | **66 µs** |
| drawing statement, 200 steps, pen down | not measured | **248 µs** | **130 µs** |
| present, full screen | 26.3 ms | **26.0 ms** | **27.25 ms** |
| present, 240 rows (`splitscreen`) | not measured | **19.8 ms** | **19.2 ms** |

An untiered **`pico2` build reads 113 µs for the same arithmetic statement** —
2.33× the Pico 2 W — while its bare `repeat` iteration is **4.5 µs, identical**.
Two numbers from one run, one 2.33× apart and the other exact, and the only
difference between the builds is which functions live in SRAM. §16.3.

**The Pico 2 W interprets 5–11 % faster than the Plus 2 W**, and that is the
reverse of what this section expected. §3 warned that the `pico2` preset does
not carry P10 M5's flash tiering and that a Pico 2 would therefore be slower;
`pico2w` *does* carry it, so the warning never applied to this board, and the
board that has PSRAM turns out to be the slower one at everything except
plotting pixels. **The `pico2` preset remains unmeasured** and is the one this
design still cannot speak for.

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
  **`(l − r)`**. That is the cabinet's dual-stick control on a keyboard, and it
  is two statements. **The sign reads backwards and is the physical one**: a
  tank whose *right* tread runs forward pivots to the **left**, so a clockwise
  turn — an increasing Logo heading — is `l > r`. This section said `(r − l)`
  until M1 drove it (§16.4).
- **One key per tread per direction**, and the keys sit where the sticks do:
  `1` runs the left tread forward and `Q` runs it back, `0` runs the right tread
  forward and `P` runs it back, so each hand drives its own side. `]` fires,
  space pauses, `Esc` quits. Driving straight is both forward keys, a pivot is
  one of each, and a one-tread arc is one key — `1` alone is the left tread
  forward against a stopped right one, which is forward *and* clockwise, an arc
  to the right; `1` and `P` together oppose, the sum is zero and the tank spins
  right on the spot at twice the rate. Nine tread pairs out of four keys with no
  mapping in between, so nothing has to clamp. M1 shipped the arrows over an
  intent layer instead — up/down a forward intent, left/right a turn intent,
  summed and clamped into the pair — which is a steering wheel wearing a tank's
  controls; §7 always said the motion is per tread, and this is the input
  finally saying the same thing. It retires the M4 question of whether a direct
  key pair feels better.
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
has measured a column-form tank. M0 measures it. **M0 read 5.3 ms** (Pico 2 W)
for 13 edges — a hull box and a gun line, with **no turret box**, which is the
shape M2 ships for the reason B48 set: the shape that ships is the shape that
was measured. A turret is four more columns and twelve more edges, about 3 ms at
stock, and it is M4's call with a price on it.

**Two corrections M2 made to this section (§16.5).** The half-offset is
`ehalf × (sin eh, cos eh)` and not `(cos eh, sin eh)` — this compass has heading
0 down +z — and M0's harness used the second, which mirrors the hull square and
points the gun at 90° − eh. Invisible at a fixed heading and fatal in a game
that aims it. And **the second half-offset is free**: the right offset is the
forward one turned 90°, turning 90° commutes with the camera's rotation, so the
camera-frame right offset is `(pzc, −pxc)` read straight off the forward one.
Four statements, not six.

**The turret is bought at M4 (§16.8).** The price it was refused at stands — it
is four more columns and twelve more edges — and M3's board run left 18 ms of
peak headroom to pay it out of. The barrel went with it: one line became a thin
box lying down, eight edges over four divides, because a vertical offset does not
change z (§16.7.1). Thirteen edges became **32**.

### 8.3 Models are generated, not hand-typed

As P11 §6.3 did for the rock outlines: `scripts/gen_models.py` emits the vertex
and edge lists, and the output is pasted into the game file. Hand-typing a
vertex table is how you get a tank with one edge going to the wrong corner and
no way to see which.

**It was never written, and M2 settled why rather than deferring it again.**
Every model in this game is generated *by arithmetic on the board* rather than
read out of a table: an obstacle is four ground columns from `half`, the enemy is
four from `ehalf` and its own rotation, the horizon is a height table you can
read as a silhouette off the numbers, and the gun is two vertices. There is
nothing to emit. The hazard this section exists for is real and it did land at
M2 — as a **transposed pair of trig calls** (§16.5) rather than as a mistyped
vertex, which a generator would not have caught and a test that aims the gun
did.

### 8.3a The ground is one line, and the range needs it

The plain is `eye` below the camera and screen y is `hz − eye/z × k`, so as z
goes to infinity the ground rises to exactly `hz` and stops. **One flat line
across the view at that y is the true horizon** — the boundary where the plain
meets the sky — and it is the whole of what this game draws below the peaks.

**Added at M2, from playing it**, which is what M1's hardware pass existed for.
It also cost the gunsight its two horizontal arms, which lay along it (§11).
Without it the mountain range reads as *a squiggle hanging in space* rather than
as a ridge standing on something: a wireframe silhouette is a line and not a
filled shape, so nothing anchors its lower edge, and there is nothing else below
the peaks to say where the ground is. §14 cuts ground texture on the grounds that
the cabinet has none, which is true and is exactly why the cabinet's own horizon
has to do this job by itself.

It is a **separate procedure from `horizon`**, and that is not tidiness: this
segment spans the whole screen deliberately, and a *horizon* segment that spans
the whole screen is the signature of the `wrap` defect M0 hit (§16.4). Keeping
them apart lets `test_no_horizon_segment_spans_the_whole_screen` go on watching
what it was written to watch instead of carrying an exception.

It is also **the most expensive single line the game draws** — a full-width edge
at §10's measured per-step costs is about 0.38 ms of pixel loop on a Pico 2 W and
0.18 on a Plus 2 W, plus three statements. It buys the whole scene a floor.

### 8.3b A scale is not a tangent, and the mountains slipped against the world

**§8.4 said `(azimuth − ph) × scale` — "linear rather than `k · tan`, which is
what the cabinet's own backdrop amounts to and saves a tangent a point". The
saving was real and the approximation was not free.** Everything else in the
game projects through `k · xc/zc`, which for a direction is `k · tan(bearing)`,
and a straight line against a tangent is not the same curve twice.

**The invariant it broke is the one that makes a 3D scene cohere: in a pure
pivot, distance does not matter.** A rotation changes every bearing by the same
amount, so a peak at infinity and a cube in front of you, at the same place on
screen, must move by the same number of pixels — distance cancels out of
`k · xc/zc` entirely. `mn.sc` was 5.06, which is `160 / 31.61°`: fitted so the
linear map is exact at the *screen edge*. That put the whole of the error in the
middle of the view and gave it a sign flip:

| bearing | mountain moves | object moves | error |
|---:|---:|---:|---:|
| 0° (centre) | 15.18 | 13.63 | **+11.4 %** |
| 10° | 15.18 | 13.92 | +9.0 % |
| 20° | 15.18 | 15.14 | +0.2 % |
| 30° (near the edge) | 15.18 | 17.63 | **−13.9 %** |

(one 3° frame, in turtle steps). The mountains scrolled at a **constant** rate
while the world accelerated toward the edges. Over a second of turning, a cube
and a peak that started together finished **32 steps apart** at the centre, or
12 the other way near the edge.

**Found by watching it turn**, which is the only way it shows — every still
frame is plausible. That is the second defect in this design invisible to a
screenful of correct-looking geometry; §16.5's enemy gun was the first.

Fixed by giving the horizon and the moon the same `k · tan` as everything else.
The cost is one statement and one `tan` a point, about **0.6 ms** at 150 MHz,
and it retires `mn.sc`, `mn.dx` and the increment trick with them: the spacing
is no longer uniform, so each point takes the tangent of its own running angle.
That angle is never wrapped and never leaves ±45°, so the tangent is bounded by
1 and the singularity is unreachable. **The moon's cull moved from steps into
degrees** for the same reason — it can be anywhere in the circle, and `tan` of a
bearing near 90° is not a number to hand to `setpos`.

`test_a_pivot_moves_the_horizon_and_the_world_together` pins the invariant
directly: it reads a peak and a projected point through one 3° turn and requires
the same displacement. On the old map it reads 15.18 against 13.63.

**One thing this does *not* explain.** The report that prompted it was that in
the arcade the mountains move *slower* in turns than nearer objects. A correct
projection cannot do that from a pure pivot at any distance. What would is the
**pivot centre sitting behind the eye** — a tank rotates about its hull while
the periscope is forward of it, so a turn also swings the eye sideways, which
moves near objects and cannot move something at infinity. At 20 steps of offset
against a target 300 away that is about +7 % on the near object. It is a feel
change rather than a correctness fix, and it needs its own collision handling
since the eye then moves during a pure turn, so it is **M4's** (§19.8).

### 8.4 The horizon is at infinity and is the most expensive thing in the frame

The mountain range, the volcano and the moon have no depth: screen x is
`k × tan(azimuth − ph)` and screen y comes straight from a stored table. No
divide, no rotation — and **M0 measured a 32-point scan at 13.845 ms**,
against this section's estimated 10.1, because four statements a point at
§3.1's 53.5 µs is what four statements a point costs.

**That is 21 % of a frame spent on a backdrop, and it is the single largest
line item after the present.** The cull stops being an optimisation and becomes
part of the design.

**How many points survive it? Seven, not the twelve this section first wrote** —
found while building M0's harness, and arithmetic rather than measurement.
Thirty-two points over 360° sit about 57 steps apart near the centre of the
view, and a
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

**M0 gives the near plane a second job, and it is the binding one.** An edge
costs 0.35 µs a step on a Plus 2 W and **0.98 on a Pico 2 W** (§10), so a
frame's drawing cost is proportional to how much screen its edges cover — and
what governs that is the near plane, because projected size goes as `k·h/z`. At
`near` = 8 a 40-step box projects 1,300 steps tall, four screens, and a dozen of
its edges at 0.98 µs a step is most of a frame.

So `near` is cut for **edge length**, not for the singularity: at `near` = 60 a
box tops out at 173 steps, which is one screen and about 170 µs an edge on the
slower board. The collision radius then follows the near plane rather than the
other way round, which is the same relationship the arcade had — you cannot get
close enough to an obstacle for it to fill the view.

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

**M0 says 130 µs on a Plus 2 W and 248 on a Pico 2 W, against 66 and 68 for the
short one.** The dirty marking is indeed once per line; the per-pixel loop is
what costs, at **0.35 µs a step on the Plus 2 W and 0.98 on the Pico 2 W** over
the 183 extra steps. The macro does a bounds test, two comparisons against the
accumulated box and a byte store per pixel, out of flash, and that is not free
at 150 MHz however cheap it looks in C.

**The 2.8× between the boards is the largest difference in the run and it runs
the wrong way.** The Pico 2 W is faster at everything else and slower at this;
the *short* line is identical on both, so it is specifically the per-pixel cost
and not the call. The series does not contradict it — a box at z = 300 has
35-step edges, where the boards agree — which is why the series' drawing slope
follows interpretation (3.74 ms an object on the Pico 2 W against 4.14 on the
Plus 2 W) while Q1 goes the other way. Unexplained; §19.7.

**The design consequence is §9's**, and it is the useful part: cost is
proportional to on-screen edge length, so the near plane is what bounds it.

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

**What "getting it right" turned out to mean (§16.5): wrap once, in one place.**
M2 hoists the whole obstacle field into camera-relative `(obx, obz)` in
`ob.scan`, once a frame, after the tank's move commits — and after that every
collision is **two comparisons** against a number that is already small, and
there is exactly one `modulo` pair in the file that can be wrong. It costs
3.9 ms and saves about 8 across four callers, but the reason to write it this way
is that **B19 is a defect a single wrapped table cannot have**. The one caller
that cannot read the current table is the tank's own move, which has to be
refused before it is committed, so `blocked?` takes an *offset* rather than a
position and the tank passes its step delta against last frame's table.

**The enemy's shell against the player is two comparisons and no arithmetic at
all**, because the player is at the origin of the camera-relative frame every
offset in the file is already in.

**The radar** is one `arc` (a C-side primitive, ~0.2 ms), a swept wedge that is
two `setpos` lines, and a blip. ~3.0 ms with the bearing arithmetic — **and
there is no bearing arithmetic** (§16.5). A radar is a top-down view in the
*camera's* frame, and the enemy's camera-frame coordinates are exactly the
`(xc, zc)` its projection already computes, so the blip is three statements with
no arctangent and no distance. The whole radar is about 1 ms, and the face of it
is the far plane: `rd.sc = rd.r / far`, so an enemy you could draw is an enemy
you can see on the glass.

**The gunsight** is a fixed overlay: about eight lines from a straight-line
procedure with no arithmetic in it at all, exactly as P11 §6.1 writes an
outline. ~1.5 ms with the HUD text.

**It has no horizontal line in it, and §8.3a is why.** It was two long arms at
y = 40 with the verticals hanging off them — and 40 is `hz`, so the arms lay
exactly along the ground line. Two things drawn on the same row of pixels in two
colours are one thing as far as the eye is concerned: the sight read as part of
the horizon and the horizon as part of the sight.

So the shape moved off that row: **two brackets facing each other**, eight
edges. A bar 30 above the aiming point with its ends turning *down* toward it, a
bar 30 below with its ends turning *up*, and a stalk leaving outward along the
centreline from each. Every number is mirrored about y = 40 — bars at ±30, teeth
reaching back in to ±15, stalks from ±30 out to ±60, ±30 of width — and that
symmetry is the part to preserve if it is redrawn, because a sight that is not
symmetric about its aiming point says the gun is somewhere it is not.

**The bars are horizontal, and that was never the problem.** What matters is not
the direction of a stroke but that no stroke lies on `hz` or crosses it — which
is what `test_no_part_of_the_gunsight_lies_along_the_horizon` checks, and why it
has no opinion about the shape. **The gap is the point**: a shell flies at eye
height and so appears at y = `hz`, dead centre when aimed, which is on the
horizon and is exactly where the target is. The middle thirty rows are left
empty, so the sight frames what it is pointing at without ever covering it.

**Sound** on the PSG: a two-voice engine idle whose pitch follows tread speed,
a noise burst for the cannon, a pitched explosion, and the rising two-tone
alarm when an enemy is in front of you. The arrangement follows
[sound-design.md](sound-design.md) and P11 §11's lesson — the alarm is a
*tempo* as much as a pitch, and that is what makes a hunting supertank
frightening. Budgeted inside the 3.0 ms line below.

## 12. Frame budget — measured on two boards

At **15 fps — a 66.7 ms budget**. M0 measured 200 frames at each of 1, 2, 4 and
8 visible objects plus the enemy, in `splitscreen`, presenting with `refresh`.

| Objects | Pico 2 W | Plus 2 W | Pico 2 |
|---:|---:|---:|---:|
| 1 | 51.41 ms | 53.31 ms | 54.61 ms |
| 2 | 58.45 | 61.14 | 62.63 |
| 4 | 72.85 | 76.60 | 78.62 |
| 8 | 101.53 | 107.50 | 110.60 |

| | Pico 2 W | Plus 2 W | Pico 2 |
|---|---|---|---|
| mean | `44.18 + 7.167 n` | `45.62 + 7.736 n` | `46.62 + 7.998 n` |
| worst | `46.96 + 7.878 n` | `49.39 + 8.096 n` | `48.48 + 8.339 n` |
| **at three obstacles and one enemy** | **65.7 / 70.6 ms** | **68.8 / 73.7 ms** | **70.6 / 73.5 ms** |
| an object costs | 7.26 ms | 7.88 ms | 8.11 ms |
| — of which projection | 3.52 | 3.69 | 3.77 |
| — of which drawing | 3.74 | 4.19 | 4.33 |

**The three boards span 7 %** — 65.7, 68.8 and 70.6 ms — with the Pico 2 W
fastest and the Pico 2 slowest, and the same 7 % separates their arithmetic
statements (48.5, 53.5, 54 µs). Whatever the game is cut for, it is cut for all
three at once.

| | Predicted | **Pico 2 W** | **Plus 2 W** |
|---|---:|---:|---:|
| present, 240 rows | 19.7 | **19.8** | **19.8** |
| horizon, 32 points scanned whole | 3.9 *(assumed culled)* | **13.2** | **13.8** |
| project 3 obstacles | 6.5 | **10.6** | **11.1** |
| draw 3 obstacles | *(in "edges")* | **11.2** | **12.6** |
| project the enemy | 6.0 | **5.3** | **5.6** |
| draw the enemy, 13 edges | *(in "edges")* | **2.4** | **2.5** |
| everything else in the flat term | 11.7 | **3.2** | **3.5** |
| **total at three obstacles** | **51.7** | **65.7** | **68.8** |

**14–17 ms over, and 10–11 of it is the horizon** — which this document always
said would be culled and which the harness deliberately did not cull so the
lever could be priced. Most of the rest is §3.1's unit: 48.5–53.5 µs against 43.

### 12.1 The two levers that close it, both measured

There is a third, larger than both and untried: **overclocking** (§12.3).

**Cull the horizon: −10.3 ms (Pico 2 W), −10.8 (Plus 2 W)** (§8.4). Two
statements to find the first and last visible index, seven points instead of
thirty-two. This was always the plan.

**Move the hot path off `local`: −3.8 to −3.9 ms** (§13 L0.5). `project.box`
holds seven locals and `project.enemy` sixteen, and on the host a `local` read
costs 0.32 µs against a global's 0.213. Rewritten with globals, the projection
measures **1.31× faster** for an identical result. Free, no interpreter change,
and the one lever this document did not think of.

| | Pico 2 W | Plus 2 W | Pico 2 |
|---|---:|---:|---:|
| mean frame at three, as measured | 65.7 | 68.8 | 70.6 |
| with both levers | **51.6** | **54.1** | **55.1** |
| worst frame with both levers | **56.5** | **58.9** | **58.0** |

Against 66.7. That is **11.6–15.1 ms of headroom on the mean and 7.8–10.2 on the
worst, on every board** — more than Asteroids shipped with. **18 fps (55.6 ms)
fits the Pico 2 W's mean and nothing else**, so 15 fps is the rate and 18 is a
question for M4 rather than a plan.

§14's two gameplay levers stay unspent and are re-priced at the measured slope:
**obstacles 3 → 2 is −7.2 to −7.9 ms**, and the horizon table 32 → 20 points is
−4.9 to −5.2 before the cull and about −1.6 after it.

**The present is ~37 % of the closed frame and no game-side lever reaches it.**
Same finding as P11 §3.3, and it generalises: on this display a vector game pays
a fixed tax a sprite game does not.

### 12.3 Overclocking — measured, and it is the largest lever in the document

`hw.setfrequency` (2026-08-23) takes the RP2350 from its rated 150 MHz to as
much as 300. Two boards' worth of runs are in
[`measurements/`](measurements/); the confirming pair is
`p13m0-clkperi-fixed-plus2w-2026-08-23.md`.

    150 MHz:  frame = 45.64 + 7.485 n     n=3: 68.1 mean, 71.1 worst
    300 MHz:  frame = 31.40 + 3.635 n     n=3: 42.3 mean, 44.3 worst

**1.610× on the frame, and the parts separate exactly as this section predicted
before any of it ran.** The slope is **2.059×** — pure interpretation, against a
clock ratio of 2.000. The flat term minus the present is **2.05×**. And the
present itself moves **19.62 → 18.70 ms**, which is the CPU share of it and
nothing else: the wire does not care what the CPU is doing.

**Closed** (horizon culled, hot path on globals): **35.0 ms mean and 37.0
worst**, against 53.1 / 56.0 at stock.

**The die barely warms**: 24.3 → 26.9 °C over a 200-frame run at 300 MHz, and
25.7 → 26.6 in the earlier sweep. Thermals are not a constraint.

**And that is not a peculiarity of these two boards** (2026-08-24, from reading
around rather than from a run here, so it is second-hand where everything else
in this section is measured): 300 MHz is widely reported as safe on the RP2350
generally, without heatsinking and without thermal trouble, so a board that
*refuses* the clock is rare rather than merely absent from this bench. That
matters to §16.7.3 and not to the budget — the frame numbers above stand on
their own measurements either way. What it supports is the **decision to require
the clock**, which until now rested on three boards all happening to take it.

### 12.3.1 It got there the hard way: clk_peri

The first sweep read the present at **58.95, 59.45 and 57.4 ms** at 200, 250 and
300 — three times the figure, and *flat*, which is the shape that gives the
cause away.

**`clk_peri` does not follow `clk_sys`.** This document, and the code, assumed it
did. `set_sys_clock_pll` parks clk_peri on the **USB PLL at 48 MHz** whenever the
system PLL moves (SDK `clocks.c`, `PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK`,
default 0). So `spi_set_baudrate(LCD_SPI, 75000000)` was dividing 48 MHz, not
300, and could only deliver 24 — the wire went from 16.4 ms to 51.2 and stayed
there at every clock, because 48 MHz does not care what clk_sys is doing.
Predicted from that model: 58.4 ms. Measured: 57.4–59.45.

Fixed by setting the define. The re-run put the present at **18.70 ms against a
predicted 18.0, and the whole frame within 4.2 %** — so the model that diagnosed
the bug also priced the fix.

**The control is what caught it**, and it is the reason this section is not
still wrong. Before the run it said: *"if the present column reads 19.3–19.8 ms
at 300 MHz, the SPI divisor was restored and `ticks` is still honest. If it
reads ~10, the timer moved."* It read 58, which is neither, and the flatness
across three clocks named the cause within minutes. A sweep reporting only a
frame total would have shown an overclock that bought nothing and left no way to
tell why.

### 12.3.1a The dividers are coarse, and 200 MHz is a trap

The SPI divides clk_peri by an even prescale times a post-divider, so **only
clocks that reach 75 MHz exactly keep the display at full speed**: 150 gives 75
(÷2) and 300 gives 75 (÷4), but 200 gives **50** and 250 gives 62.5.

**An overclock to 200 MHz makes the interpreter 1.33× faster and the display
1.5× slower, and is worth 0.3 ms.** 250 is worth about 10. **300 is worth 26 and
is the only overclock that leaves the display where it started.**

### 12.3.1b What it buys is scene, not frame rate

The interesting consequence is not that Battlezone could run faster. It is what
the frame will hold. Closed, at 300 MHz, `frame = 25.32 + 3.223 n`:

| | at 150 MHz | at 300 MHz |
|---|---:|---:|
| objects at 15 fps | 5 | **12** |
| objects at 18 fps | 3 | 9 |
| objects at 20 fps | 2 | 7 |
| objects at 24 fps | — | 5 |

**Twelve obstacles at 15 fps is the arcade's density**, and §13's L4 verdict said
of exactly that number: *"at a measured 7.80 ms an object, twelve obstacles is
94 ms of objects alone and is unreachable in Logo at any frame rate worth
playing."* At 300 MHz an object is 3.22 ms and twelve is 38.7. **The clock is
the enabling condition L4 was being held for**, and L4 is now the answer to a
question that has already been answered another way.

This is a decision for M4 rather than now, and it is a real one: three obstacles
at 24 fps, or twelve at 15. The cabinet ran fast *and* dense, and this machine
will not do both.

### 12.3.2 The wireless bus, and picking the right seam

The cyw43 bus was the *first* peripheral a board found, an hour before clk_peri.
At 250 MHz a Pico 2 W filled the console with `[CYW43] error: hdr mismatch`: the
driver talks to the chip over a PIO SPI clocked at clk_sys/2, so the bus went
from 37 MHz to 62 and it started answering with garbage headers. Same mistake as
the LCD divisor, in a peripheral this design did not think to check.

**The first fix refused the change while the radio was up**, on the grounds that
the SDK applies that divider only when the bus is brought up, and that tearing
down `cyw43_arch` to force a re-init would take lwIP with it — leaving every PCB
the HTTP server holds dangling. The reasoning was sound and **the seam was
wrong.**

`cyw43_spi_init` and `cyw43_spi_deinit` are the **bus half of the driver on its
own**. Deinit unclaims the PIO state machine and the two DMA channels and nulls
`bus_data`; init claims them back and applies the current divider. Neither
touches the chip, the firmware, the driver's state or lwIP. So the change now
takes the bus down, moves the clock, and rebuilds the bus at the new rate —
under the driver's own lock, with the bus down across the switch so nothing can
transact at a rate that is briefly wrong. **The association survives, a server
keeps listening, and the radio may be up.** A frame in flight during the change
may need retransmitting; that is the whole cost.

**Validated on hardware, 2026-08-23.** An overclock with the radio already up
produces no `hdr mismatch` and leaves the radio working.

The general lesson is the one worth keeping: *the first seam that makes a
problem go away is not always the one that solves it.* Refusing was correct and
cheap and would have shipped a permanent restriction — "overclock before you
touch WiFi, and remember the status LED is on the wireless chip" — to avoid a
twelve-line function.

### 12.3.3 What the sweep cost, and what it bought

Two peripherals were missed before a board found them: the cyw43 bus (§12.3.2)
and clk_peri (§12.3). Both are the same mistake — assuming something derived
from the system clock would follow it — and in the second case the assumption
was *backwards*, which no amount of reading the design would have caught.

**The control worked, and it is the reason this section is not still wrong.**
§12.3 said, before the run: *"if the present column reads 19.3–19.8 ms at
300 MHz, the SPI divisor was restored and `ticks` is still honest. If it reads
~10, the timer moved."* It read 58, which is neither, and the flatness across
three clocks named the cause within minutes. A sweep that had reported only a
frame total would have shown an overclock that bought nothing and left no way
to tell why.

**What this does not make safe.**

### 12.2 The harness reproduces

The two Plus 2 W runs — the first with the §16.1 defect, the second without —
are eleven identical measurements taken twice:

| | run 1 | run 2 | |
|---|---:|---:|---:|
| arithmetic statement | 53.5 µs | 53.5 µs | 0.0 % |
| one box projected | 3.145 ms | 3.145 ms | 0.0 % |
| horizon, 32 points | 13.845 ms | 13.84 ms | 0.0 % |
| the enemy projected | 5.18 ms | 5.185 ms | 0.1 % |
| one box drawn | 3.82 ms | 3.805 ms | 0.4 % |
| present, `splitscreen` | 19.2 ms | 19.3 ms | 0.5 % |
| 200-step edge | 130 µs | 131 µs | 0.8 % |
| 17-step edge | 66 µs | 67 µs | 1.5 % |
| present, `fullscreen` | 27.25 ms | 26.45 ms | **2.9 %** |

**Every number the budget uses reproduces inside 1 %.** The one that does not is
`fullscreen`, which the game never enters and which appears only as Q2's
control — so the "split saving" line moves 8.05 → 7.15 ms between runs while
the split half itself moves 0.1. Read the split figure, not the difference.

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
design M0's body column (§16.1). The mitigation is a prefix — M1 uses `p.` for
the projection, `ob.` for the field scan, `mt.` for the horizon and `tk.` for the
tank — and a test that reads the names back out of the source.

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

**Verdict: not needed, not being built, and now largely answered by something
else.** M0 closed the gate at L0 (§16.2), so this game does not spend it. This
section then argued it was the *enabling condition* for a denser one — *"at a
measured 7.80 ms an object, twelve obstacles is 94 ms of objects alone and is
unreachable in Logo at any frame rate worth playing."*

**§12.3.1b retires that argument.** At 300 MHz an object costs 3.22 ms closed,
and twelve is 38.7 — the arcade's density at 15 fps, with no new primitive. The
clock got there first, for a build flag and a twelve-line function against L4's
250 lines of C, a slot table in `core/limits.h` and the objection that it puts
the interesting part of a Logo game in C.

What is left for L4 is the case beyond a dozen objects, or a machine that cannot
be overclocked. Still worth opening as a roadmap item on its own merits — a
vector-3D primitive family for *any* game, the way the tilemap family was
justified — but it is no longer this design's insurance policy, and nothing here
is waiting on it.

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

**Neither is a stock-clock fallback any more (§16.7.3).** M2 measured the first
one in that role and it does not close the gap — two obstacles at 150 MHz is a
peak frame of 77.0 against 66.7 — so M3 removed the fallback and made the fast
clock a precondition instead. Both levers survive as what they were priced as:
things **M4** may spend, on a board that has already taken the clock.

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

### 16.2 M0's result (the two tiered boards, 2026-08-23)

The five questions, answered. Predictions are this document's as of
2026-08-21:

| | Predicted | **Pico 2 W** | **Plus 2 W** | |
|---|---:|---:|---:|---|
| Q1 long line, 200 steps | ~60 µs | **248 µs** | **131 µs** | §10 wrong, and board-dependent |
| Q1 short line, 17 steps | 60 µs | **68 µs** | **67 µs** | P11's figure holds on both |
| Q2 present, `splitscreen` | 19.7 ms | **19.8** | **19.3** | the closest prediction here |
| Q2 present, `fullscreen` | 26.3 ms | **26.0** | **26.45** | the only figure that moves between runs (§12.2) |
| Q3 arithmetic statement | 43 µs | **48.5** | **53.5** | §3.1; ratio holds, unit did not |
| Q3 bare `repeat` iteration | 5 µs | **4.5** | **5** | exact |
| Q4 one box projected | 2.16 ms | **2.97** | **3.145** | §3.1's unit accounts for it |
| Q5 the enemy projected | 6.0 ms | **4.965** | **5.185** | the column trick beat its estimate |
| one box drawn, 12 edges | ~1.2 ms | **3.365** | **3.805** | Q1 plus forty `item` reads |
| horizon, 32 points scanned | 10.1 ms | **13.18** | **13.845** | the cull is now mandatory |

**The gate.** §16 set it at: under 66.7 ms build at L0; 66.7–71.4 spend §14's
gameplay levers; over 71.4 stop and take L4. The unculled frame at three
obstacles is **65.7 ms on a Pico 2 W and 68.8 on a Plus 2 W** — it straddles the
first threshold.

**That straddle is not the decision, and reading it as one would be a mistake
this document set up.** The harness scans all 32 horizon points because §8.4
priced the cull as a lever; the *designed* frame culls, and culled it is 51.6
and 54.1 ms. **The answer is L0 on both boards, with room.** No §14 lever is
spent and no interpreter change is made.

### 16.2.1 A correction this design owes its own §12

The first version of §12, written from the Plus 2 W run alone, reported **64.2
ms** for the typical frame. That was the **`min` column — the best frame of 200,
not the typical one** — chosen because the harness defect (§16.1) had destroyed
the `body` column and `min` was what remained.

The Pico 2 W run, with the defect fixed, showed the mean sitting **4.4 ms above
the min**, which put the Plus 2 W's real figure at an estimated 68.5. **The
Plus 2 W has since been re-run and it is 68.8** — the cross-board inference was
good to 0.3 ms.

**That does not make publishing the minimum acceptable, and the two facts should
not be allowed to cancel.** The number this document carried for two days was
optimistic by 4.6 ms against a gate it was 1 ms away from failing, and the
recovery that fixed it was only available because a *second board* happened to
run afterwards. The lesson is not "recover carefully" — it is that **a recovered
number must carry the arithmetic that recovered it**, and §12's first version
did not say it was reading a minimum. It does now, and both boards are readings.

### 16.3 The Pico 2 failed the gate, and the cause was a build flag

Measured 2026-08-23 on a `pico2` build:

| Objects | mean | best | worst |
|---:|---:|---:|---:|
| 1 | 86.40 ms | 81 | 88 |
| 2 | 101.46 | 96 | 107 |
| 4 | 131.53 | 129 | 136 |
| 8 | 191.20 | 186 | 193 |

`frame = 71.52 + 14.968 n`. At three obstacles and one enemy: **116.4 ms mean**,
and **85.3 even with both of §12.1's levers spent** — over the 71.4 ms
"stop and take L4" threshold by 14 ms. A Pico 2 could run a **one**-obstacle
Battlezone at 15 fps and nothing more.

**It is not the game, and the run says so in its own controls.** Every figure
that interprets is 2.1–2.4× its Pico 2 W counterpart:

| | Pico 2 | Pico 2 W | ratio |
|---|---:|---:|---:|
| arithmetic statement | 113 µs | 48.5 µs | **2.33×** |
| one box projected | 6.65 ms | 2.97 ms | **2.24×** |
| the enemy projected | 10.805 ms | 4.965 ms | **2.18×** |
| horizon, 32 points | 29.63 ms | 13.18 ms | **2.25×** |
| **bare `repeat` iteration** | **4.5 µs** | **4.5 µs** | **1.00×** |
| **present, `splitscreen`** | **19.75 ms** | **19.8 ms** | **1.00×** |

The last two lines are the finding. A bare loop iteration and a full present are
*identical* on the two boards while everything between them is 2.3× apart, so
the difference is not "the board is slower" — it is confined to exactly the code
paths P10 M5 moved into SRAM.

**`LOGO_HOT_IN_RAM` was off for the `pico2` preset.** Not for want of room.
`core/hot.h` said why, in the file, all along:

> `pico2` does not — not because it cannot afford it (its 21 KB is a 108 KB op
> stack left at 768 from the single-board era, not the board) but because no one
> here has one to boot.

Someone booted one. The flag was turned on and the **same board re-ran the same
harness on the same day**:

| | untiered | tiered | |
|---|---:|---:|---:|
| arithmetic statement | 113 µs | **54 µs** | 2.09× |
| one box projected | 6.65 ms | **3.195 ms** | 2.08× |
| the enemy projected | 10.805 ms | **5.4 ms** | 2.00× |
| horizon, 32 points | 29.63 ms | **14.635 ms** | 2.02× |
| one box drawn | 7.115 ms | **3.92 ms** | 1.82× |
| **frame at three obstacles** | **116.4 ms** | **70.6 ms** | **1.65×** |
| *bare `repeat` iteration* | *4.5 µs* | *4.5 µs* | *1.00×* |
| *present, `splitscreen`* | *19.75 ms* | *19.45 ms* | *1.02×* |

**One board, one harness, one day, one variable, and the two controls do not
move.** P10 M5 measured this effect once, on one game, on one board, and had to
argue that a 1.72× was the tiering rather than anything else in the frame; this
is the same claim with a control group. It is the strongest evidence for that
diagnosis in the tree, and it was found by a game that was not looking for it.

RAM goes **86.16 % → 88.89 %**, +14.3 KB, matching P10 M5's 13.6 and under the
Plus 2 W's 91.29 %. The op stack stays at 768 — 108 KB of single-board-era
value against the other presets' 256 — untouched, and it is where headroom
would come from if this board ever needs some.

**The prediction was 65.7 ms "within a few per cent"; the reading is 70.6, 7 %
high.** So the Pico 2 is the slowest of the three tiered boards rather than the
twin of the Pico 2 W, and the radio is not the only difference between them.
Closed, it is **55.1 ms** and it passes with 11.6 ms to spare.

### 16.3.1 M0 is closed

All three boards measured, all three inside the budget with §12.1's levers,
nothing in §16's five questions unanswered, and the clock swept and confirmed on
two of them (§12.3). **M1 may start.**

What M1 inherits, settled rather than assumed: the horizon is culled and the hot
path is on prefixed globals from the first line rather than retrofitted (§12.1);
`near` is cut for edge length (§9); and the rate-versus-density choice §12.3.1b
opens is M4's, not M1's — M1 should be written so that `max.obstacles` is a
constant and not an assumption.

One loose thread that is **not** a gate: §19.7's per-pixel anomaly, which the
tiering experiment sharpened rather than settled — see there for the one-minute
experiment that would close it.

**M1 — the world and the camera.** Plain, obstacles, tread controls, horizon,
the gunsight. No enemy, no shells, no radar. This is the milestone that proves
the projection is right, because a wrong transform is obvious the moment you
drive past a cube and it does not go where a cube goes. First hardware play
test: does driving *feel* like Battlezone at this frame rate?

### 16.4 M1 is built, and it is waiting on a board

`logo/games/battlezone` and `tests/test_battlezone.c` (34 tests). Plain,
obstacles, treads, horizon, gunsight; no enemy, no shells, no radar. Everything
§16.3.1 said M1 inherits is in it from the first line rather than retrofitted:
the horizon is culled, the hot path is on `p.`/`ob.`/`mt.`/`tk.`-prefixed
globals, `near` is 60 and cut for edge length, and `max.obstacles` is a constant
`draw.field` reads.

**The predicted frame, at three visible objects on a Pico 2 W, is about 49.8 ms
against 66.7** (46.5 as first written; §16.4.2 corrects the scan line), built from M0's measured parts rather than from adjectives:

| | ms | where it comes from |
|---|---:|---|
| present, 240 rows | 19.8 | M0 Q2, and no game-side lever reaches it |
| project 3 obstacles | 8.1 | M0's 10.6 closed by §12.1's globals lever |
| draw 3 obstacles | ~10 | M0's 11.2, less the pyramids' four missing edges |
| horizon, 10 points of 40 | 2.1 | §8.4's cull, at M0's per-point rate |
| the field and collision scans | ~6.2 | new to M1 — two walks of the eight-entry table, at three statements an axis (see the 40-column note below). §16.4.2 corrects this line from 3.8 |
| treads, gunsight, HUD | 2.2 | §11's estimates |
| moon, when it is in view | ~0.5 | 90 device-side chords, fixed |
| **total** | **~49.8** | corrected in §16.4.2; the board reads 50–53 driving at cubes |

That leaves about 20 ms, which is roughly what M2's enemy (7.7), radar (3.0) and
shells will want — so M1 fitting is not the same as M2 fitting, and M2 remains
the frame-budget milestone.

**The line to distrust is the drawing one.** M0 measured three boxes at z = 160
to 640, whose edges are 20–60 steps; M1's near plane lets a cube come to 80,
where it is 173 steps tall and its twelve edges cost proportionally more (§10).
The play test should read the HUD while driving *at* something, not while
looking at the horizon.

### 16.4.1 The first readout measured the wrong thing, and the board caught it

The play test reported **"low to mid 20s"** against the 46.5 ms predicted above,
and the interesting part is that **nothing was wrong with the prediction.**

`play.frame` timed from the top of the frame to just before `sync`, on the
reasoning M0 established — `sync` waits out the rest of the frame and would
report the wait rather than the work (§16). That reasoning is half true and the
missing half is the largest line item in the budget: **`sync` presents and *then*
waits.** The reference says so in as many words. So the figure on the screen
excluded the present, which is **19.8 ms of the 66.7** on all three boards and
does not vary with the scene. 46.5 − 19.8 = **26.7**, and against a scene holding
two or three objects rather than three the low twenties is where it belongs.

**A readout that cannot be compared against the budget it is quoted next to is
worse than no readout**, and this one was being read against 66.7 with 44 ms of
headroom that did not exist. The HUD now shows two numbers:

| | what it brackets | what it answers |
|---|---|---|
| **BODY** | top of frame to just before `sync` | the Logo work — what §12.1's levers move |
| **FRAME** | top of one frame to the top of the next | whether the rate is being made: 66–67 while there is headroom, above it when there is not |

The present is FRAME − BODY − the wait, and Logo cannot separate those two
because `sync` is one primitive. M0 measured it at 19.8 ms and flat, so it is a
constant to subtract rather than a number to watch — **the frame's cost is
BODY + 19.8**, and FRAME is the check on whether that fits.

**Both are averaged over `hud.every` frames — about a second — with the peak
carried alongside**, because a figure that changes fifteen times a second cannot
be read off a screen by somebody driving; that was the second thing the play test
reported. The peak is the half that matters for the budget, since it is where a
`recycle` spike shows. The readout also carries the **clock**, read once through
a `catch`, because §12.3 makes 150 and 300 MHz a 1.6× difference on the frame and
a number nobody can attribute to a clock is worth nothing.

`test_the_frame_timer_brackets_the_present` reads the two assignments back out of
the Logo source and requires one on each side of the `sync`. Nothing on the host
can time either — `ticks` is milliseconds and a host frame is microseconds — so
what is pinned is the property that broke rather than the number.

### 16.4.2 M1 measured on hardware: it makes the rate, with room

**FRAME never read above 67 at any point in the play test**, which is 15 fps
being made on every frame — not the mean, every frame, since the peak column
would have caught one. BODY read **25 ms looking at the horizon and low 30s
driving at cubes**. The frame's cost is BODY + the 19.8 ms present:

| | BODY | frame | against 66.7 |
|---|---:|---:|---:|
| horizon, few objects near | 25 | **44.8** | 21.9 ms spare |
| driving at cubes | ~31 | **~50.8** | ~15.9 ms spare |

**§16.4 predicted 46.5 and the worst case is ~51 — 9 % over, in exactly the line
this section flagged as the one to distrust.** Drawing is the item that moved,
which is §9 and §10's whole argument: an edge costs 0.35–0.98 µs a step, M0
measured boxes whose edges were 20–60 steps, and M1's near plane lets a cube
reach 173. The frame does get more expensive as the player closes on something,
as §10 said it would, and the difference between the two rows above — about 6 ms
— is that effect measured for the first time.

**One line of §16.4's table was underpriced and the re-derivation is worth
keeping**, because it is arithmetic rather than measurement and could have been
done before the board ran. The field and collision scans were put at 3.8 ms on
the assumption of a dense wrapped delta; the 40-column rule turned each delta
into three statements, so each scan is six statements an obstacle plus its tests
— eight all told — and two scans over eight obstacles is **64 + 64 statements,
about 6.2 ms**, not 3.8. Corrected, the table totals **~49.8**, and the board
says 50–53. The lesson is the one the 40-column note already states and this is
the number attached to it: *splitting a statement to fit the editor's width costs
frame time, and the cost has to be re-derived where it lands rather than left in
the estimate it replaced.*

**A defect fell out of decomposing those numbers, and it is a gameplay one.**
`max.obstacles` was spent on the near side of the near cull — an object counted
against the cap as soon as it passed the *far* cull, which is a distance test, so
obstacles **behind** the camera consumed the budget and an obstacle in front went
undrawn. With eight on a 1,600-step plain and a 700-step cull, about three
survive the far cull on a typical frame and the cap is three, so it bound nearly
every frame and roughly half of what it was spent on was behind you. What that
looks like from the driving seat is a cube dead ahead that simply never appears —
found by arithmetic, not by seeing it, which is the argument for decomposing a
reading rather than accepting it. The cap now counts objects actually **drawn**;
the rejected projection is still paid for and that is fine, since it early-outs
after six statements and the table is only eight entries long.
`test_obstacles_behind_the_camera_do_not_crowd_out_the_one_in_front` puts three
behind and one ahead and requires the one ahead; it draws nothing on the old
code.

### 16.4.3 At 300 MHz the present is the majority of the frame

Same play test, same board, `hw.setfrequency 300`:

| | BODY at 150 | BODY at 300 | ratio |
|---|---:|---:|---:|
| horizon | 25 | **11** | 2.27 |
| driving at cubes | ~31 | **15** | 2.07 |

FRAME never above 67 at either clock. Adding the present back — **18.7 ms at 300
MHz, not 19.8**, since §12.3.1 measured its CPU share moving and §12.3.1a's
divider arithmetic is why 300 is the overclock that keeps the LCD at 75 MHz at
all — the frame is:

| | 150 MHz | 300 MHz |
|---|---:|---:|
| horizon | 44.8 | **29.7** |
| driving at cubes | 50.8 | **33.7** |
| present's share of the frame | 39 % | **55 %** |

**The interpretation half behaves exactly as §12.3 measured it**: 2.07× on the
loaded scene against a clock ratio of 2.000 and a measured interpretation slope
of 2.059. The 2.27× on the horizon scene is **not** a faster-than-the-clock
result and should not be read as one — it is two uncontrolled sittings. The
readout is integer milliseconds, so ±1 on an 11 is ±9 %; the camera was not in
the same place; and the object-cap fix (§16.4.2) landed between the two runs, so
the later scene draws *more*. A like-for-like clock comparison wants one sitting,
one camera position, and `hw.setfrequency` switched underneath it — which now
works with the radio up (§12.3.2), so it is one line at the prompt.

**What it buys is scene, not frame rate, and §12.3.1b now has a measurement
instead of a projection.** 33 ms of the budget is free at 300 MHz. At M0's
overclocked slope of 3.22 ms an object that is about **ten more objects — twelve
or thirteen in view at 15 fps**, which is the arcade's density and the number
§13's L4 verdict called *"unreachable in Logo at any frame rate worth playing."*
Or spend it on rate: 33.7 ms fits **24 fps** (41.7) at the present density and
the horizon scene fits 30. **Both of those rate figures are means, and §16.6
retires them**: M2 read the peak as well, a rate is made by the worst frame, and
the answer there is 18 fps and not 24. Both are M4's call, and the caution that comes with
the rate half is Asteroids' — `turn.rate` and `tread.step` are per-frame
constants, so changing `fps` re-cuts them or the tank turns and drives 1.6× too
fast.

**And the shape of the problem has changed.** The present was 39 % of the frame
at stock and is **55 %** at 300 MHz: past this point more than half of every
frame is the SPI wire to the panel, which no game-side lever reaches and the
clock barely moves. Culling harder, drawing fewer edges and L0.5-style rewrites
all buy from the shrinking half. §6's 240-row viewport remains the only thing
that has ever moved this number, and it has already been spent.

### 16.4.4 M1 is closed

**"Playability was good."** That is the question §16 set for this milestone —
*does driving feel like Battlezone at this frame rate?* — and it is the only one
a test could not have answered. The projection is right, the frame fits at both
clocks with room, §19.2 is answered (the split screen's text area is usable under
`sync`, once B49 was fixed), and the four defects M1 turned up along the way —
B49, the object cap, the frame timer and the readout's readability — are fixed.

**M2 may start**, and it inherits three things settled rather than assumed: 15 fps
and density rather than rate (§16.4.3); `hw.setcpu "fast` at startup with `hw.cpu`
**read back** and `max.obstacles` cut if the board refused, since a board played
at a density it cannot draw is worse than one played at the stock density; and a
readout that already reports BODY, FRAME and the clock, so the frame-budget
milestone has its instrument before it has its enemy.

**Four things M1 settled that the design had left open or guessed at.**

1. **`max.obstacles` is spent before the object, not after it.** Written the
   obvious way — count the object, draw it, then test the cap — a cap of zero
   draws one object, because the count never equals the cap at the moment it is
   read. M4 turns this number, so the number has to mean what it says at every
   value including its floor. `test_max_obstacles_is_a_constant_the_frame_reads`
   sets it to zero and requires an empty screen.
2. **The horizon needs no seam guard at all**, and M0's did. M0 walked the table
   in index order and drew in screen order, which steps from +171 to −171 at the
   table's seam and needed a pen-lift to catch it (§8.4). M1 walks in **screen**
   order — it computes the first visible index and increments, wrapping only the
   table *index* with `modulo` and never the running azimuth — so consecutive
   points are always adjacent on screen and the polyline is one unbroken run.
   The guard, the `mtn.runs` counter and the fold-back arithmetic all disappear,
   and `test_no_horizon_segment_spans_the_whole_screen` still sweeps the circle
   because the *cause* it was written for was `wrap`, not the seam.
3. **`int` truncates toward zero, which is not a floor, and the horizon is where
   that bites.** The first visible index is `int ((ph − arc) / step)`, and at a
   heading under 36° that expression is negative, where truncation returns an
   index one too high and leaves a gap at the left edge — a mountain range that
   stops short of the frame for one tenth of the circle. Shifting by 360 makes
   the argument always positive, at which point `int` *is* a floor.
   `test_the_horizon_covers_the_whole_view_at_every_heading` walks 3° at a time
   for that reason: a spot check at 0, 90, 180, 270 passes on the broken version.
4. **The tread sign is the physical one and it is the opposite of the obvious
   one.** §7 says the pair drives forward speed `(l + r)` and turn rate
   `(r − l)`, which reads naturally and is backwards in Logo's compass: a tank
   whose *right* tread runs forward pivots to the **left**, so an increasing
   heading needs `l > r`. M1 uses `(l − r)`. This is not a correction to §7 so
   much as the sign §7 never fixed, and it is the kind of thing that is invisible
   in a test that only drives forward.

**The 40-column rule costs about 2 ms a frame, and it is worth writing down
because it is a real trade rather than a formatting preference.** Every line in
`logo/games/battlezone` fits the width of the PicoCalc's own editor, which is
what makes it readable on the machine it runs on — and Logo has no line
continuation, so a statement that does not fit has to be *split into more
statements*. The wrapped obstacle delta is the case: one dense expression on a
wide screen, three statements here (translate by a hoisted camera offset, fold
with `modulo`, recentre), which at 48.5 µs a statement over eight table entries
and two axes is most of the 2 ms. The horizon's first index is two statements
for the same reason. The cost is paid where it is cheapest to pay — table scans
rather than per-vertex work — but it is not zero, and a milestone that reports
a frame figure should say what is in it.

**The models are still not generated.** §8.3 says `scripts/gen_models.py` emits
the vertex and edge lists, and M1 has no vertex list to emit: an obstacle is
four ground columns computed from `half`, and the horizon is a height table you
can read as a silhouette straight off the numbers. The hazard §8.3 exists for —
an edge going to the wrong corner, with no way to see which — arrives with the
enemy tank at M2, and so does the generator.

**Two open questions M1 now puts in front of a board rather than answering.**
§19.2 asked whether the split screen's text area is usable while `sync` drives
the graphics half; `draw.hud` writes there every frame, so the first play test
answers it by looking. And §19.5's arithmetic-statement discrepancy is still
open — M1 spent L0.5 on the strength of a host measurement, as §16.3.1 directed,
and `tests/logo/p11rocks` still settles it in ten minutes.

**M2 — the enemy.** One tank, its hunt logic, both shells, collisions,
explosions, the radar. This is the frame-budget milestone — the one that
corresponds to P11's M2, which is the one that missed by 19.7 ms.

### 16.5 M2 is built, and it is waiting on a board

**Everything in M2's list is in `logo/games/battlezone`**: the enemy tank and
its hunt, both shells, all four collision pairs, the explosions, the radar and
the fire key — plus **the ground line** (§8.3a), which is not on M2's list and
came from driving M1: without it the mountain range reads as a squiggle hanging
in space rather than as a ridge, and there is nothing else below the peaks to
tell the driver where the ground is. **It cost the gunsight its two horizontal
arms**, which were at y = 40 and so lay exactly along it (§11) — it is two
brackets facing each other now, clear of that row in both directions. `tests/test_battlezone.c` is
73 tests, 35 of them M2's. What is
*not* here is the number this milestone exists to produce — M2 is the
frame-budget milestone, and a frame budget needs a board. §16.6 is that board.

**The predicted addition is about 17 ms**, at M0's rates and M1's measured
frame:

| | Pico 2 W at 150 | at 300 |
|---|---:|---:|
| M1's measured frame, driving at cubes (§16.4.3) | 50.8 | 33.7 |
| project the enemy (M0 Q5) | 5.3 | 2.6 |
| draw the enemy, 13 edges (M0) | 2.4 | 1.2 |
| `ob.scan`, 64 statements | 3.9 | 1.9 |
| four collision scans, ~2.5 statements an obstacle | 2.4 | 1.2 |
| the two shells, stepped and drawn | 2.1 | 1.0 |
| the radar and the hunt | 1.4 | 0.7 |
| the ground line, a full-width edge (§8.3a) | 0.5 | 0.4 |
| the horizon's tangent, one statement + `tan` a point (§8.3b) | 0.6 | 0.3 |
| **total** | **69.4** | **43.0** |

**Which is why M2 asks for the clock.** 69.4 is over 66.7 and 43.0 is under it
with 24 ms to spare, so the overclock is this milestone's enabling condition and
not a luxury — exactly as §12.3.1b predicted it would be for *something*, and
this is the something. `battlezone` calls `hw.setcpu "fast` and reads `hw.cpu`
**back**, and at M2 it cut `max.obstacles` from three to two if the board
refused — §14's held-in-reserve lever, worth 7.3 ms at stock. The readout
carries the answer next to the milliseconds it explains.

*(The cut is gone. M2's own measurement below showed it does not close the gap,
and M3 removed the fallback and made the clock a precondition — §16.7.3.)*

**Five things M2 settled that this document had left open, guessed at, or got
wrong.**

1. **M0's enemy model had its gun pointing at 90° − eh, and it took a game to
   notice.** The harness built the enemy's half-offset from `(cos eh, sin eh)`
   where this compass wants `(sin eh, cos eh)`, so the hull square was mirrored
   and the barrel ran at right angles to the facing. With a fixed heading and
   nothing aiming down the barrel that is *invisible*, which is precisely why it
   survived a measurement run and would have been copied into the game — the
   hull is square, so the mirroring shows only as a rotation, and the gun was the
   only witness. It matters here because `hunt` aims the gun and `enemy.fires`
   shoots along it: the shipped bug would have been a tank that faces you and
   shoots sideways. `test_the_gun_points_where_the_enemy_faces` checks three
   headings, and the third — facing straight away, where the barrel must
   *foreshorten to nothing* rather than swing across the view — is the one that
   catches a sign as well as a transposition.
2. **The second half-offset is free, and M0 paid for it.** The enemy's right
   half-offset is its forward one turned 90°, and turning 90° **commutes with
   the camera's rotation** — so the camera-frame right offset is `(p.pz, −p.px)`
   read straight off the forward one. Two multiplies and two adds a frame go
   away, and §8.2's "six statements buy what would otherwise be four multiplies
   a corner" is now four.
3. **One wrapped scan a frame, and B19 becomes unreachable.** M1 wrapped the
   obstacle field twice — in `draw.field` and in `blocked?` — and M2 would have
   made it five times, since the enemy and both shells each need the same eight
   tests. So the wrap is hoisted into `ob.scan`, which folds all eight obstacles
   into camera-relative `(obx, obz)` once, and every collision after it is two
   comparisons. It costs 3.9 ms and saves about 8. **The reason to write it this
   way is not the milliseconds**: B19 is one collision test out of several
   getting the wrap wrong, and a single predicate over a single wrapped table
   cannot have that defect. §11 said "this game gets that right from the start";
   this is what getting it right turned out to mean.
4. **The table belongs to the camera it was hoisted against, and the tank's own
   collision is the one caller that cannot wait for it.** A move has to be
   refused *before* it is committed, and the rescan follows the commit — so
   `blocked?` takes an **offset** rather than a position, and the tank passes its
   step delta against last frame's table. That is exact rather than approximate.
   The cost is a real invariant: anything that teleports the camera must rescan,
   and two M1 tests that moved it with a bare `make` had to start going through
   `camera_at`. The game teleports in exactly one place, `battlezone`, which
   rescans.
5. **A spawn is the only way anything can arrive inside an obstacle, and an
   enemy that does can never leave.** Every other placement in this game is a
   move `blocked?` refused, so nothing is ever *inside* a guard radius — except
   a respawn, which arrives somewhere rather than driving there. And the guard is
   symmetric: an enemy 50 steps from a cube's centre has every candidate step
   refused in every direction, so it sits in the middle of a cube shooting at you
   for the rest of its life. `spawn.enemy` therefore tests and re-rolls up to
   four times. `test_a_spawn_is_re_rolled_out_of_an_obstacle` lays eight
   obstacles on the spawning ring so a quarter of all bearings are blocked, and
   with the re-roll removed it reads 20 of 60 rather than 0.

**§8.3's model generator has no input, and that is now settled rather than
deferred.** M1 closed saying the hazard §8.3 exists for — an edge going to the
wrong corner with no way to see which — "arrives with the enemy tank at M2, and
so does the generator." It did not, because **every model in this game is
generated by arithmetic on the board rather than read out of a table**: an
obstacle is four ground columns from `half`, the enemy is four ground columns
from `ehalf` and its own rotation, the horizon is a height table you can read as
a silhouette, and the gun is two vertices. There is nothing for
`scripts/gen_models.py` to emit. The hazard is real and it landed as predicted —
it just landed as a *transposed pair of trig calls* rather than as a mistyped
vertex, and a generator would not have caught it. A test that aims the gun did.

**§8.2's turret box is not here, and the reason is the discipline B48 set.**
The section lists a hull box, a turret box and a gun line; M0 priced a hull box
and a gun line at 13 edges, and **the shape that ships is the shape that was
measured**. A turret is four more columns and twelve more edges — about 3 ms at
stock — and adding it is M4's call with a price attached rather than M2's
unpriced flourish.

**Two smaller things, both cheap and both worth knowing.** The radar needs **no
trigonometry at all**: a radar is a top-down view in the *camera's* frame, and
the enemy's camera-frame coordinates are exactly the `(e.xc, e.zc)` the
projection already computes, so the blip is three statements and the face of the
radar is the far plane (`rd.sc = rd.r / far`). And the enemy's shell hits the
player in **two comparisons and no arithmetic**, because the player sits at the
origin of the camera-relative frame every offset in the file is already in — the
cheapest collision in the game is the one that matters most.

**What the first hardware run has to answer**, in this order: does the frame
still make 66.7 ms with the enemy in it; is the readout's BODY figure near the
69.4/43.0 predicted above at each clock, since a large miss means the collision
or the hunt estimates are wrong rather than the enemy's; and does the enemy play
— whether `e.step`, `e.turn`, `e.range` and `e.reload` make something that hunts
you rather than something that circles. The last is M4's material and only a
board can produce it.

**Two open questions M2 leaves in front of a board.** Whether the object cap
should rise at 300 MHz — §12.3.1b's twelve-at-15-fps is now reachable with the
enemy in the frame, and M2 deliberately did not spend it, because density is M4's
decision and this milestone's job was to find out what the enemy costs. And
§19.5's arithmetic-statement discrepancy, still open and still one run of
`tests/logo/p11rocks`.

### 16.6 M2 measured on hardware, and the enemy is what it found

**Both clocks, both numbers, with a tank shooting at you** — the loaded case,
which is the one the milestone exists to price:

| with the enemy in the frame | BODY | MAX BODY | + present | frame | peak frame |
|---|---:|---:|---:|---:|---:|
| 150 MHz | 44 | 65 | 19.8 | 63.8 | **84.8** |
| 300 MHz | 22 | 33 | 18.7 | 40.7 | **51.7** |

**At 300 MHz the rate is made on every frame**, not on average: the worst frame
is 51.7 against 66.7, with **15 ms still in hand**. That is the answer M2 exists
to produce and the milestone passes on it.

**At 150 MHz it is made on the average and lost on the peak.** 63.8 fits with 2.9
ms to spare and the worst frame is **84.8 — over by 18.1**. P11's M2 is the
milestone that missed by 19.7 ms, this document has said so in six places, and
the number this game misses its stock-clock peak by is **18.1**. The overclock is
not this milestone's convenience, it is the thing standing between Battlezone and
the same failure, exactly as §12.3.1b said it would be for *something*.

Against the prediction, at the clock the game actually runs at:

| at 300 MHz | predicted | measured |
|---|---:|---:|
| M1's body, driving at cubes (§16.4.3) | 15 | 15 |
| M2's additions | 9.3 | **7** |
| **body** | **24.3** | **22** |
| the present (§16.4.3, 18.7 at this clock) | 18.7 | 18.7 |
| **frame** | **43.0** | **40.7** |

**Read the body against 24.3 and not against 43.0**, which is the trap §15 built
the two-number readout for: 43.0 is a *frame* and the readout's BODY stops before
the present. M1's first board run was compared against the wrong half of exactly
this pair and the prediction was blamed for it. Done properly the prediction is
**5 % high on the frame and 9 % high on the body**, and at 150 it is 49.6 body
predicted against 44 measured — 11 % high, the same direction and the same size.
M0's rates are good to about a tenth at both clocks.

**The peak is 1.5× the average, and it is interpretation.** 65/44 is 1.48 and
33/22 is 1.50, and — the number that says what the spike is made of — **the peak
halves with the clock just as cleanly as the average does**: 44 → 22 is exactly
2.0× and 65 → 33 is 1.97×, against a clock ratio of 2.000 and §12.3's measured
interpretation slope of 2.059. A spike made of present, SPI or any fixed cost
could not do that. It is Logo work, which means it is `recycle`, which means
§18's reclamation is the only lever that reaches it.

**What the headroom actually buys, and it is less than the average says.** 26 ms
of the 66.7 is free at 300 MHz *on the mean*, but a frame rate has to be made by
the worst frame and not the mean one:

| at 300 MHz | mean 40.7 | peak 51.7 |
|---|---|---|
| 24 fps (41.7) | fits by 1.0 | **misses by 10.0** |
| 20 fps (50.0) | fits by 9.3 | **misses by 1.7** |
| 18 fps (55.6) | fits by 14.9 | fits by 3.9 |
| 15 fps (66.7) | fits by 26.0 | fits by 15.0 |

So **18 fps is what the peak supports** — not the 24 the mean appears to offer,
and 20 misses by 1.7 ms, which is inside the readout's own integer resolution and
is therefore a coin toss rather than a margin. §16.4.3's "33.7 fits 24 fps" was
computed from a mean for the same reason, and it should be read as 18 as well.
The other lever, **scene**, is unaffected by any of this and is the better buy:
15 ms of peak headroom at M0's overclocked 3.22 ms an object is **four more
objects** at the rate that already works. Both stay M4's call, and the caution
§16.4.3 attached to the rate half now lives in the file beside the constants it
governs — `turn.rate` and `tread.step` are **per frame**, so `fps` cannot move
without re-cutting them.

**And the refused-clock fallback does not work.** `battlezone` cuts
`max.obstacles` three to two if the board would not take `"fast`, which is worth
7.8 ms at M0's stock rate: peak body 65 → 57.2, peak frame **77.0**, still 10.3
over 66.7. Getting the *peak* inside 66.7 at 150 MHz needs the obstacle field
gone entirely, which is not a game. What does fit is a **rate** cut on that path
— two obstacles at 12 fps is 77.0 against 83.3, in hand by 6.3 — and that is a
decision rather than a measurement, so it is recorded here and left to M3.
**M3's answer was to delete the path rather than tune it (§16.7.3)**: a game on a
board that cannot draw its frame is not a goal, and a 12 fps variant is a
different game kept alive for a board nobody has. It is
a contingency and not the main path: every board this game has run on has taken
the clock.

**All four of M2's open board questions are closed.** §19.2's split screen
positions its text correctly under `sync` (B49 confirmed on a board). The new
per-tread controls report the key codes the file assumes. The frame budget is the
table above. And the enemy plays — the question §16 said only a board could
answer, and the answer was **no**.

#### 16.6.1 The stand-off and the hit box were one number, and neither knew it

**The enemy came straight at you with perfect aim.** It was not a feeling; it is
arithmetic, and both halves of it were sitting in this document unchecked
against each other:

- The player is a `tk.hit` box, **30 steps** half-width, tested in the
  camera-relative frame (§9).
- The enemy fires along **its own heading**, and `hunt` lets it fire once it is
  within `e.aim` = 7° of you. A shot is therefore thrown sideways by
  `d · tan(e.aim)`.
- `d · tan 7° < 30` for every `d` inside **244 steps**. Inside that radius the
  shot cannot miss, whatever the aim error does.
- `e.range`, the stand-off it closes to and holds, was **240**.

So the tank drove to the one distance from which it could not miss, and stopped
there. This is the same failure shape as the collision radius and the near plane
(§9) — two constants that only mean anything relative to each other, written down
in different sections, never compared — and it is now a test in the same shape as
that one.

**Moving the stand-off alone would not have fixed it.** `hunt` turns in `e.turn`
= 4° steps and stops the moment it is inside `e.aim`, so the error it fires with
is whatever that last step left, and **against a player holding still it is the
same error on every shot**. The tank does not miss sometimes and hit sometimes;
it hits every time or misses every time, for a whole approach. That is a coin
flipped once, not an aim, and no stand-off distance turns it into one.

**Both halves therefore moved.** `e.range` 240 → **400**, which is 283 even on
the diagonal where `e.d`'s Manhattan distance is furthest from the true one; and
a per-shot aim error `e.wob` of **±10°**, applied in `enemy.fires`, which is two
`sin`/`cos` calls **a shot** rather than a frame — against a 20 ms body that is
not a number worth thinking about. At the stand-off, 283 · tan 10° is **50 steps
against a 30 box**, so the cone genuinely straddles the player: driven over forty
shots dead-on at the stand-off, **15 hit and 25 missed**.

The two tests are `test_the_enemy_cannot_park_where_it_cannot_miss` — the
constants against each other, the §9 shape — and
`test_the_enemys_aim_varies_from_shot_to_shot`, which drives the forty shots and
requires both outcomes. It asserts *both*, not a ratio: the ratio is exactly what
M4 tunes, and a test that pinned it down would fight the tuning. **The mock's
hardware random source is a constant 42**, so that test calls `rerandom` first,
which is the precedent the spawn re-roll test already set.

#### 16.6.2 90 degrees a second is too fast to aim with

`turn.rate` was **3 degrees a frame per unit of tread difference**, which at 15
fps is 90°/s on a pivot and 45 on a one-tread arc. A board says that is too fast
to aim with, and it is **2** now — 60 on the pivot, 30 on the arc. The number
that moved is the one the player feels; nothing about the frame changed.

This is where §16.4.3's caution earns its keep, and it now lives beside the two
constants rather than only here: **both are per frame**, so they are really
per-second figures multiplied by `fps`, and neither can be read on its own.
Spend the headroom above on rate and these two have to move the other way, or
the tank turns and drives faster for no reason a player can see.

**M3 — the game.** Lives, scoring, the enemy sequence (tank → missile →
supertank → saucer), the cracked screen, the attract screen, the high score
table, sound.

### 16.7 M3 is built, and measured

`logo/games/battlezone` is the game now rather than the engine: three tanks, the
arcade's score table, the four enemies on a ring, the shattered periscope, an
attract screen with ten high scores kept in `/games/battlezone.scores`, and the
four sounds §11 asked for. **124 host tests, 84/84 ctest green**, and all three
presets link.

**Measured on both boards and the budget is closed (§16.7.4a); the play test
broke the enemy and §16.7.4b is the fix.**

**What M3 costs the frame is almost nothing, and that is the point of having
done it last.** The score, the lives and the enemy sequence are arithmetic on
*events* and not on frames; the sound is four gates a frame at worst; the only
new per-frame drawing is the shatter, and only while you are cracked. Both new
models are **cheaper than the tank**:

| | edges | divides |
|---|---:|---:|
| tank / supertank (§8.2) | 13 | 6 |
| saucer | 12 | 5 |
| missile | 4 | 4 |
| cracked screen (while cracked) | 12 | 0 |

So there is no frame in this game more expensive than the worst one M2 measured
at 51.7 ms, and the board run is a confirmation rather than a gate.

#### 16.7.1 §8 has no missile and no saucer, and both found a symmetry

The one thing M3 had to invent. Both are cut the way the tank is — the camera's
rotation folded in **once**, the perspective divides counted — and each turned
out to have a symmetry the tank does not, which is why both came in under it.

**A missile is a spike with four fins, and it draws in four edges.** Two
savings, and neither is an approximation. First, *a vertical offset does not
change z*, so the fins above and below the tail sit at the tail's own (x, z) and
share its divide: two of the six vertices are free, and since the divide is the
expensive part of a vertex that is most of what a fin costs. Second — the one
that surprised the count — *the upper tip, the tail and the lower tip are three
points on one straight line*, so a single stroke through the middle point is the
same ink as two strokes out of it. Four fins, three edges, one pen-up where
`draw.box` needs four for its verticals. The design predicted five edges before
the test counted four.

**A saucer is a square bipyramid, and it is rotationally symmetric.** Its own
heading never enters the transform, which is the whole saving: the four rim
points sit at ±r along the *world* axes, so the camera turns them into one
offset pair and its own perpendicular — §8.2's free-90-degrees trick without
even the multiply that earns it there, two statements where the hull needs four.
Four rim points plus the centre is five divides, and the dome and the keel share
the centre's.

The shape is right for the reason that matters rather than for economy. A tank's
periscope is 12 steps off the plain and the saucer floats at 90, so it is seen
very nearly **edge-on** — a plate with a dome above and a hull below, which is
exactly what a rim quad and two apexes draw from that angle. It is the only view
this game has, and the model is cut for it.

#### 16.7.2 The three booleans, and why the frame never asks what it is looking at

Four enemies could have been four comparisons in every procedure that touches
one. Instead a spawn reads a **row** — `set.kind`, one procedure per kind, all
fifteen numbers together — into the live `e.*` names, and leaves three booleans
behind: `e.gun` (has a barrel, draws one, fires), `e.ram` (kills by *arriving*,
which is the missile and only the missile), `e.drift` (does not hunt, does not
fire, and is not stopped by an obstacle, which is the saucer flying over the
plain at 90 steps). After that **the frame costs exactly nothing for having four
kinds**: `hunt`, `move.enemy`, both collisions and the draw dispatch each read a
boolean where they would have compared a kind number, and a boolean read is
cheaper than a comparison.

**One block a kind, and §16.6.1 is why.** The failure that cost M2 a play test
was two constants that only mean something against each other written down in
different sections and never compared — the stand-off and the hit box. Eight
parallel lists indexed by kind is that failure with a mechanism. A row keeps an
enemy's numbers where they can be read together, and `e.wide` and its stand-off
are three lines apart in every one of them.

**And the invariant is now a test over every kind that carries a gun**, which is
what M3 needed it to be: the supertank is "smarter" by wobbling **7°** rather
than 10, and a *tighter* cone at the same distance is a cone that fits inside
the hit box — M2's defect, reopened by the fix for it. So the supertank's
stand-off had to go **out** rather than in: 420 against the tank's 400, which
puts 297 · tan 7° = 36.5 steps against a 30 box.

*(That is the reasoning as it stood before the board ran, and the tangent in it
is what §16.7.4b retires: an angular error is the wrong SHAPE whatever its size,
and moving a stand-off cannot fix it because the player picks the range. The
supertank is still the wider stand-off and the tighter cone; the cone is now
measured in steps.)*

#### 16.7.3 The fallback is gone: the fast clock is a precondition

§16.6 measured the refused-clock fallback and found it does not work — two
obstacles at 150 MHz is a peak frame of 77.0 against 66.7, still over by 10.3 —
and left "a rate cut to 12 fps would fit it" to M3 as a decision rather than a
measurement. M3 first took it, then **removed the whole path**, which is the
better reading of the same measurement: *getting a game onto a board that cannot
draw its frame is not a goal.*

**Only one thing ever runs this game, and it is 300 MHz.** At 150 the worst frame
is 84.8 ms against 66.7 — P11 M2's failure with a different name on it — and no
arrangement of the two levers rescues it. Two obstacles is still over by 10.3.
Getting the *peak* inside at stock needs the obstacle field gone entirely, which
is not a game. And 12 fps with two obstacles, which does fit, is a different and
slower game maintained for a board nobody has: every board this has run on took
the clock, and §12.3 records why that is the expected case rather than luck —
300 MHz is safe on this silicon generally, unheatsinked, so a board that refuses
is rare and not merely absent from this bench.

So `clock` **answers a question instead of cutting the scene**. It asks, reads
`hw.cpu` back — the hardware and not the request, so a refusal cannot report an
overclock that bought nothing — and outputs whether this board can play. A board
that says no gets a message with the measurement in it rather than a game.

**What the fallback cost while it existed was not its four constants.** It was
that `fps` and `max.obstacles` became things `clock` *wrote*, so neither could be
read as a tuning number, and every per-frame constant in the file had to be
argued against a rate that might move underneath it at startup. §16.6.2's caution
— that `turn.rate` and `tread.step` are per frame and cannot be read without
`fps` — was true and unusable while `fps` was decided by the board. Both are
plain constants again, `test_the_clock_does_not_write_the_tuning` pins that, and
**M4 is the only thing that turns either**.

It is also 4 globals back, which against §16.7.4's 25-slot margin is not nothing.

#### 16.7.4 The interpreter ran out of globals, and this game is why

**M3 does not fit in 192 global variables, and M2 did not have room to spare
either: it stood at 189.** That is not a defect, it is the bill for §13's L0.5 —
the frame is 1.31× faster because every hot-path temporary lives in the flat
global namespace, and the price of that decision was always going to be paid in
slots. Turtle Trails, the fattest program in this tree before Battlezone, is
about 119.

Both halves were done. **The file was economised**: every single-use constant
that had been hoisted for readability went back to its call site — the four
score values, the four kind numbers, every sound frequency and duration, the
name-field geometry — and the two new projections borrow `p.` temporaries the
tank's projection has already finished with rather than taking their own. That
is 34 names. **And `MAX_GLOBAL_VARIABLES` went 192 → 254**, which is the ceiling
of the current representation rather than a round number: `global_hash` holds
slot + 1 in a `uint8_t`, and a static assert pins it.

**The number that matters is the peak and not the load-time count**, and the
difference between them is what a board reported. Battlezone is **186 entries
after `load` and 229 once a game has been played**: fifty of its names — every
`p.` temporary in the three projections, the `mt.` ones in the horizon,
`tk.dx`/`tk.dz`/`tk.guard` in the collisions, `e.b` and `e.d` in the hunt,
`e.left` in `set.kind` — are minted the first time a procedure that uses them
runs, not by a top-level `make`. So **nothing observable at load says the table
is about to fill**, which is precisely the failure a Pico 2 W hit: on firmware
built before the cap moved, the file loaded, the attract screen ran, and
`init.game` then died with `Out of space in spawn.enemy` — `spawn.enemy` being
simply the procedure that happened to be executing when the last slot went.
Reproduced on the host by building the suite at 192: load 186, attract 186,
`init.game` fails at exactly 192.

`test_the_game_fits_the_global_table_with_room_to_spare` therefore **plays a
game** rather than reading the source, and asserts the peak against a stated
budget of 16 free slots. 229 of 254 leaves 25, so it passes by 9 — and that
margin is a budget rather than slack: a player's own program, a startup file or
a profiler loaded beside the game all come out of the same table.

It costs **992 B of SRAM** (62 slots × a 16-byte `Variable`), measured as +0.19
points on all three boards — 88.89 / 86.38 / 91.29 % to 89.08 / 86.57 / 91.48.
It costs **nothing in time**: `find_global` has gone through a hash index since
P11 M4, so a read is a hash and a probe whatever the table holds, and the line in
`limits.h` that said otherwise predated that index and is corrected.

#### 16.7.4a M3 measured on hardware, and the play test broke the enemy again

**Both boards, at 300 MHz, and they read the same:**

| with the enemy in the frame | BODY | MAX BODY | + present | frame | peak frame |
|---|---:|---:|---:|---:|---:|
| M2 | 22 | 33 | 18.7 | 40.7 | 51.7 |
| **M3, Plus 2 W** | **22** | **30** | 18.7 | **40.7** | **48.7** |
| **M3, Pico 2 W** | **22** | **30** | 18.7 | **40.7** | **48.7** |

**M3 is free**, which is what §16.7 predicted and is worth stating plainly: lives,
scoring, four enemy kinds, two new models, the cracked screen and the sound
together did not move the body, and the *peak* came down 3 ms. 18 ms in hand on
the worst frame against 66.7. The budget question is closed.

**The two boards reading identically is not something §19.7 predicted.** Its table
has the Pico 2 W's per-step line cost at 0.984 µs against the Plus 2 W's 0.350 —
3.3× apart on the same silicon — and a frame that is a third drawing should show
that. It does not, at 1 ms of readout resolution. That is evidence and not a
resolution; §19.7's one-minute experiment is still the thing that settles it.

**And the play test broke the enemy, for the second milestone running and in the
same place.** *"The tanks never miss."*

#### 16.7.4b The aim error was an angle, and an angle is the wrong shape

§16.6.1 caught the enemy coming straight at you with perfect aim, diagnosed it as
`e.range` against `tk.hit`, and fixed it by moving the stand-off to 400 and
adding a per-shot angular wobble of ±10°. **It then validated that fix at exactly
one distance** — the stand-off — where the tank does miss 55 % of the time, and
called it done.

A board found the rest of the curve. Measured against a stationary player:

| tank fires from | old model | new model |
|---:|---:|---:|
| 400 steps | 19 / 40 hit | 29 / 40 |
| 283 | 27 | 19 |
| 200 | 33 | 19 |
| 150 | **40 / 40** | 20 |
| 100 | **40 / 40** | 20 |
| 60 | **40 / 40** | 21 |

**An angular error is thrown `d · tan(wob)` sideways, so it shrinks to nothing as
the range does — and the range is not the enemy's to choose, because the player
can always drive closer.** Inside 170 steps the tank could not miss; inside 244
the supertank could not, and the supertank is *newer than the fix*, added by M3
with a tighter 7° cone precisely because tighter meant smarter. **No value of an
angular `e.wob` fixes this. The shrinking is the shape of the error, not its
size**, which is why two rounds of moving the stand-off did not help.

So the error is now **a lateral offset at the target, in steps** — `e.wide`,
turned into an angle for the range it is fired over, three statements and one
`arctan` a shot. That is what a gunner's error actually is, and it makes accuracy
range-independent by construction.

**The invariant collapses to one comparison in one unit.** §16.6.1's version
joined three numbers in two units with a tangent —
`e.range / √2 · tan(e.wob) > tk.hit` — and was wrong twice. The new one is
**`e.wide > tk.hit`**: a shot thrown fewer steps sideways than the half-width of
the box it is aimed at cannot miss. Both numbers are in steps and they sit three
lines apart in every kind row.

**And the test now sweeps range instead of asserting at a point.**
`test_no_range_is_a_range_the_enemy_cannot_miss_from` drives forty shots at each
of six ranges from point blank to beyond the stand-off, for every kind that
carries a gun, and requires **both outcomes at every one of them**. On the old
model it fails with `kind 1 at 60 steps: 40 hit, 0 missed of 40` — the board's
sentence, in a test. It asserts both and never a ratio, because the ratio is what
M4 tunes.

**What the fix leaves standing is the mechanic the game is supposed to have.**
Movement was always the player's lever and it still works — a shell takes `d/32`
frames to arrive and the player moves 8 steps a frame, so a lateral drive
displaces `d/4` steps before it lands:

| tank fires from | player still | player strafing |
|---:|---:|---:|
| 400 steps | 29 / 40 hit | **0 / 40** |
| 283 | 19 | 3 |
| 200 | 19 | 14 |
| 100 | 20 | 24 |
| 60 | 21 | 26 |

So standing still is punished at every range and moving saves you beyond ~200
steps, while up close nothing does. That is Battlezone. **The old model had the
same gradient but with a floor of certain death under it**, and the floor was the
whole of what the board was complaining about.

**One more number the board moved: the missile was 11 steps a frame against the
treads' 8** — 1.4× — and *"very fast"*. It is 9 now, 1.1×. The margin and not the
speed is what decides whether a thing is dodgeable, and the property that matters
is only that it cannot be outrun.

#### 16.7.5 Four things building it found

**Two spawns for one death, and nothing on the screen would have said so.** A
missile sets its own `e.boom` and *then* kills you, so both countdowns start on
the same frame and run out on the same one — and `step.enemy` spawns from its
counter a moment after `respawn` spawns from the tank's. The ring advanced twice
and one of the two enemies was never seen. It is invisible from the driving seat,
because there is an enemy out there either way; the test is on the sequence
position and not on the picture. The enemy's own countdown wins, because it is
the one that was already going to fire. This is the same shape as §16.6.1 and
§9's near plane: **two pieces of state that only mean something against each
other, written in different procedures**, and it is the third time this design
has found one.

**`init.game` arms `setrefresh "sync`, and on the host `sync` really waits.**
Every test that set a game up and then drove frames was sleeping a real 1/15 s a
frame; the suite went from a second to minutes and looked exactly like a hang.
The tests now go through one helper that disarms it, and the reason is written
where the helper is.

**The mock's sound gate log holds 64 entries and a centred pair spends two of
them a note.** The alarm test counted 36 notes into a 32-note log and then
concluded the alarm was one note rather than two — a test failing for a reason
that had nothing to do with the thing it was testing. It now checks the two
notes *first* and counts inside the budget.

**`show.game.over` flushes the key ring before it reads a name**, which is the
guard that stops the keypress that ended the game arriving as the first letter
of a name — and it therefore eats a test's queued input. `read.name` is driven
directly and `show.game.over` gets a stub, which is the shape `test_asteroids.c`
already arrived at for the same reason.

#### 16.7.6 What only a board can answer

The frame budget, as ever, and this time it is a confirmation and not a gate:
the table above says no frame is more expensive than M2's worst, so the readout
should land where M2's did. What it is really being asked is whether **the game
plays** — whether a missile is dodgeable at 15 fps, whether the shatter takes
enough of the view to hurt without making the game unplayable, whether the
engine idle and the closing alarm do what §11 claims for them, and whether the
saucer reads as a saucer from a tank's eye height. Every one of those is M4's
material and none of them is a number.

### 16.8 M4: the models get their detail, and §8.2's turret is bought

The first thing the play test asked for. Three shapes changed, and the two
arguments the design had already made about each of them are what made it cheap.

| | before | after | divides |
|---|---:|---:|---:|
| tank / supertank | 13 edges | **32** | 6 → 12 |
| missile | 4 | **12** | 4 → 5 |
| shell (each) | 1 | **12** | 1 → 4 |
| worst-case frame | 135 edges | **176** | |

**A tank is a hull, a turret and a barrel.** §8.2 refused the turret at M0 —
*"another 3 ms at stock, and M0 measured the hull-and-gun form... adding it is
M4's call with a price attached"* — and this is M4 paying it. The barrel stops
being one line and becomes a thin box lying down: eight corners over **four**
divides, because §16.7.1's rule holds here too and the top and bottom of each
corner share one. Eight edges, and the base ring is not drawn because it is
inside the turret.

**The turret needs no cull test**, which is worth stating because it looks like
an omission. It sits inside the hull's footprint, so every column of it is
further from the near plane than the hull column beneath it: if the hull passed,
it passes.

**And a tank is thirty-two edges without one new list**, which is the part that
had to be designed rather than written. Project-then-draw would want three more
four-element lists per part — nine lists for three boxes — against a file 18
slots from its ceiling (§16.7.4). So `draw.enemy` **refills the same three
columns between parts**: hull, draw, turret, draw, barrel, draw. It is the only
object in this file whose projection and drawing interleave, and the reason is
the global table rather than the frame.

**A missile is a long pyramid with fins, and it is the saucer's solid.** Both are
a four-point ring with an apex either side: a *wide* ring with short apexes reads
as a plate, a *narrow* ring with a long nose reads as a dart. One `draw.spindle`
serves both, twelve edges over five divides. **The ring is the fins** — a dart
whose ring is wider than its body has them by construction, and separate fin
spikes would have wanted four more divides and four more `cx` slots than exist.

**A shell is a cube**, which is the largest single item here: two in the air is
24 edges where it was 2. Its half-width is **2.5** — it was 5 and a board said
halve it, because a 10-step round is a crate next to a 28-step tank. What a shell
has to do is *grow* as it closes rather than be big: 2.5 is about two pixels at
the far plane and thirteen at 100 steps, which is the range where reading it
matters. The dash was defended on the grounds that a shell has no
size worth transforming; it has, up close, and **a round coming at you is the one
object in this game whose range you most need to read**. A dash gives you nothing
to judge it by and a cube grows. It is axis-aligned to the *world* rather than to
its flight — nothing about a shell's roll is observable — which is the saucer's
two-statement trick again.

**What it costs, and the prediction held.** 41 more edges and 16 more divides on
the worst frame; the estimate was **3–4 ms**, taking the peak to about 52.
Measured on a board at 300 MHz:

| | BODY | MAX BODY | + present | frame | peak frame |
|---|---:|---:|---:|---:|---:|
| M3, enemy in the frame | 22 | 30 | 18.7 | 40.7 | 48.7 |
| **M4, enemy in the frame** | **26** | **33** | 18.7 | **44.7** | **51.7** |
| M4, enemy and a shot | 28 | — | 18.7 | 46.7 | — |

**+4 ms on the mean and +3 on the peak**, against 3–4 predicted. The peak frame is
51.7 against 66.7 with **15 ms still in hand**, which is the same peak M2 read
before any of M3 or M4 existed. A shell cube is **+2 ms**, so both in the air at
once is a mean of about 30 and a peak near 36 — a frame of ~55, still 11 ms
clear.

**What the two readings do not support is a per-edge rate.** Solving the tank's
19 edges and 6 divides against the shell's 11 and 3 gives edges a cost of zero
and divides 0.67 ms each, which cannot be right — `turret.columns` is five
statements a divide and that would make a statement 130 µs. Two points at 1 ms of
readout resolution is not enough to separate the drawing from the projection, and
the honest figure is the total.

It also costs **7 globals**: 236 of 254, leaving 18 against the 16 the budget
test enforces. The next model that wants a temporary will have to find one — and
that, rather than the frame, is now the binding constraint on adding detail.

#### 16.8.1 Lines all over: `/` binds tighter than `-`

The first play test of §16.8 saw the shell cubes and the tank's barrel throwing
lines across the screen. It was one defect in three places, and it is the
plainest kind this file has produced.

Every perspective divide here is `k / z` for a range `z`. The three parts §16.8
added wrote theirs **inline**:

```
make "p.iz :k / :p.za - :p.px      ; wrong
make "p.iz :k / (:p.za - :p.px)    ; right
```

`/` binds tighter than `-`, so the first is `(k / za) - px`. At 300 steps that is
**0.867 − 8 = −7.13** where the range wanted 0.839 — negative, and an order of
magnitude out — so every vertex of the turret, the barrel and the shell cube
landed somewhere arbitrary.

**Nothing older had it**, and the reason is a habit rather than luck: every
projection written before §16.8 hoists its range into a `p.z*` first and then
divides by a bare name, which cannot be mis-parsed. The three new procedures did
not need the range afterwards, so they inlined it, and inlining is what exposed
the precedence.

**The edge-count tests were blind to it, and that is the lesson.** Twelve edges
drawn through nonsense coordinates is still twelve edges — every model test in
§16.8 passed while the picture was wrecked, and the board is what found it. So
the tests now check **where the vertices land**: `test_every_part_of_a_tank_lands_on_the_tank`
and `test_a_shell_cube_lands_on_the_shell` require every column of every part to
sit within a generous bound of the object's own centre. The bound is loose on
purpose — it is catching a projection that has come apart, not tuning a
silhouette — and it fails on the old code with *"the turret column 1 is at
x −69.7, 69.7 from the object's centre"*.

**And one thing the picture showed that was not a defect:** the barrel came out
of the *bottom* of the turret, level with the hull top, because `e.bt`/`e.bb`
were cut from `e.te`. They now straddle the turret's mid-height, which is where a
gun comes out of a tank.

#### 16.8.2 Two steering schemes, and the keys that do not move with them

*"I need a better control scheme."* M2 replaced the arrows with one key per
tread and wrote that it **retired** the M4 question of whether a direct key pair
feels better. It did not retire it — it answered it for one player. A tank's
controls are the thing people disagree about hardest, so both ship and **`C` on
the attract screen chooses**.

| | | |
|---|---|---|
| **arrows** (default) | ↑↓ drive, ←→ turn | a forward intent and a turn intent summed into the pair |
| **treads** | 1/Q left, 0/P right | one key per tread per direction, M2's |

**The clamp is the whole of the arrow scheme.** Forward *and* right sums to left
2, right 0, and a tread has three states — without it the tank would drive at
double speed whenever it turned. `clamp1` existed for exactly this, went with the
arrows at M2, and comes back inline (six statements, no call, no local).

**It costs one global, not three.** `tk.r` and `tk.s` are *borrowed* for the two
intents: `step.tank` writes both the moment `treads` returns — `tk.r` from the
tread difference and `tk.s` from the sum — so their values here cannot outlive
the call. Only `arrows` is new, which matters at 237 of 254 (§16.7.4).

**And the keys that are not steering no longer move with the scheme.** SPACE
fires, **Z** pauses, ESC quits, in both. Space was the pause key and `]` the fire
key, which is a fine layout for hands already on 1/Q and 0/P and a strange one
for hands on the arrows. *A control you press without thinking should not depend
on a menu you set once.* `]` is kept as a second fire key because it is where a
right hand on 0/P already is.

`test_fire_pause_and_quit_are_the_same_in_both_schemes` drives all three under
both settings rather than under the default, which is the only way that claim is
worth making. The steering is **session state** — `init.game` does not touch it,
so a new game keeps it, and a test says so.

**M4 — tuning.** Played, then cut.

## 17. Tests

`tests/test_battlezone.c` on the mock device, mirroring `test_asteroids.c`'s
shape. **38 at M1's close; 73 at M2's; 125 at M3's.** The ones that are specific
to this game:

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
  drifts from the game measures a game nobody plays. **M1 has no separate
  harness to drift**: the frame reports its own cost on the HUD (`frame.ms`,
  everything but `sync`), so the thing being measured is the thing being played.
  The test comes back the moment a `p13m*` script spells a frame out again.
- **The horizon cull keeps the visible points.** Off-by-one at the field-of-view
  edge is a mountain that flicks in and out as you turn.

M2's additions, and the failure each exists for:

- **The gun points where the enemy faces.** Three headings, including facing
  straight away, where the barrel must foreshorten to nothing rather than swing
  across the view. This is the test that caught M0's transposed half-offset
  (§16.5), and the only one that could have: the hull is square, so the same
  error shows there only as a rotation. It reads the *barrel's* columns from
  §16.8 on, and it is joined by **the turret sits on the hull and inside it** —
  the second half of which is what lets the turret skip the near-plane test.
- **A shell is stopped by an obstacle, including across the seam.** B19's own
  case, in this game, against the single wrapped table that is supposed to make
  it impossible.
- **A spawn is re-rolled out of an obstacle.** Eight obstacles laid on the
  spawning ring, and a *rate* rather than a guarantee: 0 of 60 with the re-roll
  and 20 of 60 without it.
- **The frame moves everything before it draws anything.** Read out of the Logo
  source, like the frame-timer test, because it is one frame of one picture and
  nothing on the host can see it by playing. A shell stepped after the drawing
  would hit a tank the player had already watched explode.
- **The entry point sets the game up.** `battlezone` ends in a loop only a
  keypress leaves, so no test in this tree has ever called a game's entry point
  — and M2 put the clock, the first spawn and a dozen resets in this one. This
  runs its statements, from the source, up to the loop it cannot enter.
- **The clock is asked for and read back, and a refusal cuts the field.** Both
  halves, through the mock's settable clock and through a mock with no settable
  clock at all.

M3's additions, and the failure each exists for:

- **The aim error is wider than the box it is aimed at**, and **no range is a
  range the enemy cannot miss from.** The first is `e.wide > tk.hit`, one
  comparison in one unit, over every kind that carries a gun. The second drives
  forty shots at each of six ranges from point blank outwards and requires both
  outcomes at every one — which is the test §16.6.1 needed and did not have, its
  version having fired only from the stand-off, the one distance the old model
  was safe at (§16.7.4b).
- **Every kind sets a whole row.** `set.kind` chooses by comparing against four
  literals, so a kind matching none of them would leave the *previous* enemy's
  numbers in place — a supertank wearing a saucer's score, and nothing on the
  host or the board that says so.
- **The frame draws the model that matches the kind.** A saucer drawn as a tank
  would be twelve edges either way, so the assertion is that the edge count
  *moves* with the kind: 13, 4, 13, 12.
- **A saucer's outline does not turn with its heading.** It is rotationally
  symmetric and that is the whole saving; an outline that turned would mean the
  heading had leaked into the transform and the projection was doing work the
  symmetry exists to remove.
- **A saucer flies over an obstacle a tank is stopped by.** Both halves, in the
  same test, because "it flies" is one boolean in one procedure and the only
  proof it is wired up is a tank grounded in the same place.
- **The shatter is static.** Every stroke's endpoints, twice, not just the
  count: what is stored is a bearing and two lengths, and a shatter re-rolled
  each frame would be a snowstorm rather than damage to the glass.
- **A bonus tank arrives on stepping over the boundary.** A 5,000-point saucer
  can carry a score from 14,000 to 19,000 without ever equalling 15,000, which
  is what a `remainder` would miss.
- **The fast clock is a precondition, and a refused board is told why.** Three
  tests: `clock` answers true and false for the two boards; it writes neither
  `fps` nor `max.obstacles` on either path, which is what makes both readable as
  tuning numbers again; and a refused board gets the message and **no game**,
  checked by stubbing `one.game` and asserting it never ran.
- **The session asks for the clock before any game.** M2 asked once per game;
  M3 asks once per session, because ESC now returns to an attract screen and a
  fourth game should not pay for a wireless bus teardown again.

## 18. Risks

| Risk | Where it bites | What answers it |
|---|---|---|
| Long-line drawing cost is 2–3× the short-line figure | §12's 4.0 ms becomes 10 | M0 Q1, first thing measured |
| The column-form enemy does not hold its estimate | §12 by up to 5 ms | M0 Q5 — measured at 5.3 ms against 6.0 predicted; **closed** |
| Split mode's unsent dirty rows cost something | §6's 6.6 ms saving shrinks | M0 Q2 |
| 180× does not hold on a Pico 2 | Every estimate here, on one of three boards | M0 Q3, run on both boards |
| The projection is subtly wrong in a way play reveals and tests do not | M1 | Hardware play test at M1, not M3 |
| The game needs L4 and L4 makes it a C game | §13's honest objection | Decided at M0's gate, with a number |

## 19. Open questions

1. **Does a P9 tilemap layer compose underneath turtle graphics?** If it does,
   the horizon is nearly free (§8.4) and this is the cheapest 3.9 ms in the
   document. Nothing in this tree establishes it either way.
2. ~~**Is `splitscreen`'s text area usable while `sync` is driving the graphics
   half?**~~ **Answered at M1, once B49 was fixed: yes.** `draw.hud` writes
   there every frame and a board reads it, which is why §6's assumption that the
   score can live in the text area rather than cost graphics rows holds — and
   M3's score row is written on the event rather than on the tally for exactly
   that reason.
3. ~~**Should the enemy's hunt logic run every frame?**~~ **Answered at M2: no,
   one frame in three.** `hunt` decides the turn, the drive and the fire and
   leaves those three *intents* behind; the two frames in between act on them,
   so the enemy turns and drives every frame and only re-aims on the third. At
   15 fps that is invisible and it is the difference between 0.6 ms a frame and
   0.2.
4. **`min`/`max` (§13 L1)** — worth opening as a cheap win regardless of this
   game?
5. **Why is a local arithmetic statement 53.5 µs when P11 M0 measured 42–44.5
   on the same board twelve days earlier?** (§3.2.) Either the interpreter
   regressed — which would touch every game in the tree — or the two harnesses
   differ invisibly. `tests/logo/p11rocks` still runs and settles it.

   **The run itself takes seconds, not the "ten minutes" this used to say** —
   that was an estimate of the *effort*, and it read as a duration. The
   calibration loops are 2,000 iterations and the frame passes are 60 frames,
   so a board is finished almost at once. The number wanted is the line
   `arithmetic statement` in `/p11rocks.txt`, which the script appends to
   because figures on the PicoCalc's display cannot be copied off it.

   **Two things will spoil the comparison if they are not checked first.** The
   board must be at the STOCK clock — `p11rocks` predates `hw.setcpu` and does
   not set one, so a session left at 300 MHz reads about half and looks like a
   spectacular improvement. And `erall` before loading is not optional the way
   it once was: the script's own header warns about `MAX_PROCEDURES`, but
   Battlezone now leaves **237 of 254 globals** behind (§16.7.4), and a
   workspace still holding it has nowhere to put `p11rocks`' own. **This is the one open question that is not about Battlezone**, and
   it should be answered before M1 spends L0.5 on the strength of a host
   measurement.
6. ~~**Does a *tiered* Pico 2 run this frame?**~~ **Answered at M0: yes, and
   the prediction was 7 % low** (§16.3). It reads 70.6 ms against a predicted
   65.7 and closes to **55.1 with §12.1's levers, 11.6 ms in hand** — so a
   tiered Pico 2 is the *slowest* of the three boards rather than the twin of
   the Pico 2 W, and the radio is not the only difference between them.
8. **Should the tank pivot about a point behind the eye?** (§8.3b.) A real tank
   rotates about its hull while the periscope sits forward of that, so a turn
   swings the eye sideways as well as rotating it — which makes near objects
   outrun the horizon by ~7 % at 300 steps and is the likeliest explanation for
   what the cabinet looks like in a turn. Two or three statements a frame, and
   the catch is that the eye then moves during a pure turn, so a pivot needs the
   collision test a drive gets. **M4**, with the rest of the feel.

7. **The per-step cost of a line differs 3.3× between two boards of the same
   silicon.** Four builds, and the tiering experiment turned this from a puzzle
   into a well-posed question:

   | | fixed part | per step |
   |---|---:|---:|
   | Pico 2, untiered | 162 µs | 0.273 µs |
   | Pico 2, tiered | **63 µs** | **0.295 µs** |
   | Pico 2 W | 68 µs | **0.984 µs** |
   | Plus 2 W | 67 µs | 0.350 µs |

   **The fixed part is explained, and the middle two rows are the proof.**
   Moving the interpreter into SRAM cut it 2.57× and left the per-step cost
   within 8 % — so the fixed half is interpretation and the per-step half is
   `screen_gfx_line`, which lives in `devices/`, is flash-resident on every
   build, and the tiering never touched. A controlled experiment, by accident.

   **What is left is narrow and strange.** A Pico 2 and a Pico 2 W are the same
   silicon on the same board differing in the radio, and their per-step costs
   are **0.295 and 0.984 µs** — the same loop over the same SRAM canvas at the
   same clock, 3.3× apart, with the Plus 2 W at 0.350 siding with the Pico 2.

   **The one-minute experiment:** run `p13m0` on a Pico 2 W with the radio never
   started, and compare Q1 alone. If cyw43 housekeeping is stealing time it will
   show most in the longest single statement in the harness — a 200-step edge is
   ~250 µs against a 17-step edge's 68 — which is exactly the shape of the
   anomaly. If the number does not move, the Pico 2 W is doing something to the
   pixel loop that three other builds are not.

   §9's near plane is cut around this rather than for it, so the game is safe
   either way.
