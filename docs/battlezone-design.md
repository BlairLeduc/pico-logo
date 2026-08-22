# Battlezone in Pico Logo (design)

Status: **v1, drafted 2026-08-21. The M0 gate has not been run.** Every board
figure below is an *estimate*: a host measurement scaled by a ratio calibrated
against P11's board numbers (§3). Nothing here has touched hardware, and this
document expects to be rewritten by M0 the way
[asteroids-design.md](asteroids-design.md) was rewritten five times.

Two findings from the feasibility pass are load-bearing and are stated up front:

- **The projection pipeline in pure Logo costs 46.8 ms a frame** for a naive
  scene of nine boxes and one enemy (§4.2). That is not a rejection — a
  disciplined scene fits in 51.2 ms against a 66.7 ms budget (§12) — but the
  frame is linear in *visible vertices*, and the object cap is this design's
  `max.rocks`.
- **There is no way to draw a line to a computed point in one statement.**
  `(setpos x y)` is documented in the reference and is not implemented; the
  working alternative allocates two cons cells per line. Logged as **B48**
  (§5). M0 cannot run honestly until it is fixed.

[Asteroids](asteroids-design.md) was the first game in this tree that was not a
sprite game. Battlezone is the first that is not a **2D** game, and the thing
that makes it interesting is that the 1980 cabinet and this one have the same
problem in the same place: an XY monitor traced a display list, and Pico Logo's
turtle *is* a display list. What the arcade machine had that we do not is a
Math Box — an AMD 2901 bit-slice coprocessor sitting beside the 6502 for the
sole purpose of doing the matrix arithmetic the 6502 could not. §13 is this
design's version of that question, and the answer is deliberately deferred to a
measurement rather than taken now.

---

## 1. Deliverables and source of truth

| | |
|---|---|
| Game | `logo/games/battlezone` — one Logo file, no extension, no `-` or `/` in the name so `load "battlezone` parses |
| Tests | `tests/test_battlezone.c` (Unity + mock device), mirroring `tests/test_asteroids.c` |
| Design | this document |
| Measurement | `tests/logo/p13m0` — times a real frame at 1, 2, 4 and 8 visible objects, with the projection pass read apart from the drawing pass and the present read apart from both. It writes its numbers **to a file**, because numbers on a display cannot be copied off it |
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

## 3. The calibration this design is built on

Every board figure below is `host × 180`, plus a per-statement drawing cost
taken directly from P11's board measurement. That ratio was not assumed; it
was measured and then checked against a known board result.

P11 M0 priced four units on a Plus 2 W
([asteroids-design.md §3.4](asteroids-design.md#L267)):

| Unit | Board |
|---|---:|
| arithmetic statement (`make "x :x + 1`) | 42–44.5 µs |
| drawing statement (`fd 17`, pen down) | 59.5–60.3 µs |
| bare `repeat` iteration | 4.5–7 µs |
| present, full screen | 26.3 ms (1.26 ms per 16-px tile row) |

Running the same two loops on this host gives 0.24 µs and 0.04 µs, so the
ratios are **179×** for an arithmetic statement and 125× for a bare loop
iteration. Take 180×.

**The check.** Asteroids' `step.draw.all` was re-run on the host with its
collision calls stubbed: 11.1 µs a rock. Scaled, 2.00 ms; plus roughly twelve
turtle statements an outline at ~55 µs, 0.66 ms; total **2.66 ms** against the
board's measured **3.035 ms a rock**. Within 12 %, on a number this design did
not fit to. The model is good enough to size a game with and not good enough
to settle a 2 ms question — which is what M0 is for.

**What the ratio is not.** It is this host against a Plus 2 W with P10 M5's
`__not_in_flash_func` tiering enabled. The `pico2` preset does not enable that
tiering (P10 §11.3), so a Pico 2 will be slower and by an unmeasured amount.
M0 runs on both boards or its numbers only describe one.

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

### 4.2 Measured in Logo, on the host, scaled

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

Add a 26.3 ms full-screen present and the naive frame is 73 ms before a single
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

### 4.4 The decision

**Build L0 — the whole game in Logo, no new primitives — and gate at M0.**

§13 prices four tiers of interpreter help, from a `sincos` operation to a
`drawmodel` primitive that would put the entire pipeline in C and take the
per-object cost from 2.16 ms to about 0.05. They are real levers and this
design does not rule any of them out; the user asked that they stay on the
table and they are on it. What this design refuses to do is **spend them
before M0 says which one is needed**, because that is the mistake P11 §12 made
twice: it priced three levers, spent none of them, and then found the real cost
somewhere none of them reached.

## 5. B48 — there is no single-statement line to a computed point

This is the one thing that blocks M0, and it was found by trying to write the
draw pass.

A projected wireframe is a sequence of arbitrary screen points. Drawing it
wants one statement per point, with the pen down. Pico Logo has three
candidates and all three fail:

- **`(setpos x y)`** — documented at
  [Pico_Logo_Reference.md:1201](../reference/Pico_Logo_Reference.md#L1201) as a
  variadic form, and **not implemented**. `prim_setpos` is `REQUIRE_ARGC(1)`
  and is registered with arity 1
  ([primitives_turtle.c:1991](../core/primitives_turtle.c#L1991)), so
  `(setpos 10 20)` answers *"setpos doesn't like 10 as input"*. Behaviour that
  contradicts the reference is a bug, so this is **B48**, not a roadmap item.
- **`setpos list :x :y`** — works, and allocates. Measured: **two cons cells a
  call**. A frame drawing 70 edges mints 140 cells; at 15 fps that is 2,100 a
  second against a 32,752-cell pool, so the game needs a `recycle` every
  fifteen seconds, and a recycle is a visible hitch. (P11 §12b found a recycle
  frame is the worst frame in Asteroids, and that is Asteroids spending five
  cells a frame, not 140.) Driving 100,000 of these on the host without a
  reclaim reaches *"Out of space"*, which is the demonstration.
- **`setx :x sety :y`** — two statements, and **geometrically wrong**: each
  moves on one axis only, so with the pen down it draws an L, not a line.
  Asteroids never noticed because it only ever uses the pair with the pen
  **up**.

So B48 is not a nicety. It is the difference between an allocation-free frame
and a periodic hitch, and it is a five-line fix in a primitive that is already
documented to do it. **M0 depends on it and nothing else in this design does** —
the rest of the game is unaffected by which spelling wins.

## 6. The viewport is 240 rows, and that is worth 6.6 ms

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
of 20: **19.7 ms instead of 26.3**. And the bottom eight text lines are exactly
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

**This wants checking at M0 and not before.** The present-side saving is
certain from the source; what is not certain is whether `clean` in split mode
leaves the text area alone in practice, or whether the 80 rows of dirty state
it marks and never sends cost anything measurable on the next present.

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

### 8.4 The horizon is at infinity and is still not cheap

The mountain range, the volcano and the moon have no depth: screen x is
`(azimuth − ph) × scale`, wrapped, and screen y comes straight from a stored
table. No divide, no rotation — and 32 points still cost **10.1 ms**, because
four statements a point at 43 µs is what four statements a point costs.

Culling to the field of view is the fix: compute the first and last visible
index (two statements) and loop over the ~12 that can be on screen. **3.9 ms.**

The alternative worth naming and not taking yet: put the range on a **P9
tilemap layer** and scroll it, which would make it very nearly free. Whether a
tilemap composes underneath turtle graphics is not established anywhere in this
tree, and finding out is a bigger question than 3.9 ms justifies. Left in §19.

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

## 10. Drawing

`clean` and redraw, for the reason P11 §3.3 measured: a scattered vector scene
dirties most tile rows either way, so an erase pass buys almost nothing and
costs a second traversal. A wireframe horizon line spans the full width by
itself, which settles it before the argument starts.

An edge is `(setpos x y)` with the pen down — **one statement, one straight
line, nothing allocated** — once B48 is fixed. A closed quad is one pen-up
`setpos` and four pen-down ones. Per box: 12 edges in about 16 statements.

Line **length** is nearly free, which is the piece of luck this design has and
Asteroids did not need. `screen_gfx_line` marks its dirty region **once, from
the accumulated bounding box** rather than per pixel
([screen.c:748](../devices/picocalc/screen.c#L748)), and the inner loop is a
bounds test and a byte store. So a 200-pixel box edge should cost barely more
than the 17-pixel `fd` P11 measured at 60 µs.

**"Should" is doing work in that sentence.** It is inference from the source,
not a reading, and it is the single largest unmeasured number in this document:
55 edges at 60 µs is 3.3 ms, and at 150 µs it is 8.3. **M0's first job is to
draw a 200-pixel line 1,000 times and settle it.**

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

## 12. Frame budget

At **15 fps — a 66.7 ms budget**. The rate is not a preference and §12's
successor sections in P11 are the precedent for changing it once, early, before
anything per-frame is tuned against it.

| | ms |
|---|---:|
| present, 240 rows (`splitscreen`) | 19.7 |
| `clean` | 0.3 |
| horizon, culled to ~12 of 32 points | 3.9 |
| cull 8 obstacles | 3.3 |
| project 3 visible boxes/pyramids | 6.5 |
| project 1 enemy, 8 vertices in column form | 6.0 |
| ~55 edges drawn | 4.0 |
| radar | 3.0 |
| gunsight and HUD | 1.5 |
| input, tread physics, shells, collisions, sound | 3.0 |
| **total** | **51.2** |

**15.5 ms of headroom, and it should all be regarded as spoken for.** P11 M2
opened 6 ms short by its own estimate and measured **19.7 ms** over, with the
whole miss in a cost nothing had priced. This budget has two numbers that could
do the same thing: the drawing statement at long lengths (§10) and the
column-form tank (§8.2).

**The present is 38 % of the frame and no game-side lever reaches it.** That is
the same finding P11 §3.3 made and it generalises: on this display a vector
game pays a fixed tax that a sprite game does not.

## 13. Interpreter levers, priced

None of these is ruled out. Each is stated with what it buys *this* game, what
it costs to build, and — the test that matters — whether it is worth having
without this game.

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

Battlezone's frame does roughly 44 `item` calls on lists of 8 to 32, so the
walks cost **~1.1 ms** — and arrays would remove perhaps 0.7 of that.

**That is an honest and disappointing answer, and it is the one to record.**
Arrays are a good language feature; at this scene size they are not this game's
lever. They become one only if the model tables grow past ~64 entries, which is
what happens if L4 is *not* taken and the game grows anyway.

**Buys this game:** ~0.7 ms. **Verdict:** Battlezone is not the demonstrated
need the roadmap is waiting for. Say so there.

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

**Verdict: gated, not decided.** Build the game at L0. If M0 says §12 holds,
ship it and open L4 as a roadmap item on its own merits — a vector-3D primitive
family for *any* game, which is how the tilemap family was justified. If M0
says §12 misses by more than the two gameplay levers in §14 can cover, L4 is
the lever to spend, and it is the only one on this list large enough to matter.

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
what keeps the cons pool out of it entirely.

Everything else is fixed-size flat global lists set up once at load: the
obstacle field, the model tables, the horizon profile.

## 16. Milestones

**M0 — measure, before any game code.** `tests/logo/p13m0`, on both a Pico 2 W
and a Plus 2 W, because §3's ratio was calibrated on one board and the
`pico2` preset does not carry P10 M5's flash tiering.

It answers five questions, in this order, and **B48 must be fixed first** or
the fourth and fifth are measuring the wrong program:

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
