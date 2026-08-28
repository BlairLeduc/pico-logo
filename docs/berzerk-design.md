# Berzerk in Pico Logo (design)

Status: **DESIGN, nothing built.** M0 is a measurement and it is the gate.

Berzerk is the third vector game in this tree and the first one that is not a
vector game in its own cabinet. Asteroids and Battlezone were XY machines
tracing a display list; Berzerk was a **raster** machine with a 256 × 223
bitmap, 8-pixel-wide sprites and a hardware pixel-intercept collision bit. So
this port has a translation to do that neither of the other two did, and
[P14](vector-direction-design.md) is the reason it is worth doing: the walls,
the bolts and the figures in Berzerk are all *line drawings pretending to be
bitmaps*, and turning them back into lines is a restoration rather than a
reinterpretation. The Vectrex port
([`berzerk-source-vectrex.asm`](berzerk-source-vectrex.asm)) is the proof —
somebody already did this in 1982 for a machine with no raster at all, kept the
arcade's 5 × 3 room grid, its wall masks and its bolt structure byte for byte,
and only changed how the ink got onto the screen.

**Single player only.** The cabinet supports two alternating players; the
Atari 2600 manual opens with *"BERZERK is for one player only"*, which is the
version this port takes. §17.

**Source of truth is the arcade**, in the order: the disassembly, then the two
manuals for rules the code makes hard to read, then the Vectrex for how to draw
it. Where the three disagree, the arcade wins and the deviation is written down
(§17).

---

## 1. Deliverables and source of truth

| | |
|---|---|
| Game | `logo/games/berzerk` — one Logo file, no extension, no `-` or `/` in the name so `load "berzerk` parses |
| Tests | `tests/test_berzerk.c` (Unity + mock device), mirroring `tests/test_battlezone.c` |
| Design | this document |
| Measurement | `tests/logo/p15m0`, written to a **file** — a number on a display cannot be pasted anywhere. Times a real frame at 1, 4, 8 and 11 robots, with the drawing pass read apart from the logic pass and the present read apart from both, under **both** erase strategies (§3) |
| Arcade | [`berzerk-source-arcade.asm`](berzerk-source-arcade.asm), Scott Tunstall's reverse engineering, 2026. Addresses below are that file's |
| Vectrex | [`berzerk-source-vectrex.asm`](berzerk-source-vectrex.asm), Fred Taft's disassembly with Malban's 2019 rewrite |
| Manuals | [Atari 2600](berzerk-manual-atari-2600.md), [Vectrex](berzerk-manual-vectrex.md) |
| High scores | `/games/berzerk.scores`, beside the game file, as Asteroids and Battlezone do |

Play: `load "berzerk` then `berzerk`.

All three boards. Nothing here needs WiFi, TLS or PSRAM, so `LOGO_HAS_WIFI` and
`LOGO_HAS_TLS` are not consulted anywhere in the game. **300 MHz is a
precondition, not an optimisation** (§15.4), the same call Battlezone made at
its §16.7.3.

## 2. What the game is, mechanically

The arcade rules, kept:

- You are a stick figure in a room of **electrified walls**. Touching a wall
  kills you. Touching a robot kills you. A robot's bolt kills you.
- The room has **four exits**, one centred on each side. Walk out of one and
  you are in the next room, entered on the opposite side at the same height.
  The rooms go on forever in all four directions.
- The room holds **0 to 11 robots**. They hunt you, they walk into walls and
  each other and die doing it, and after the first few hundred points they
  shoot back.
- You fire in the **eight** directions, two bolts on screen at once. A bolt
  stops at a wall. Robots fire in eight directions too, but only when they are
  roughly lined up with you (§10.3), and never more than the difficulty allows.
- Clear the room and you get **10 points a robot** on top of the 50 each; a
  robot killed by another robot, by a wall or by Otto pays you just the same.
- **Evil Otto** is a bouncing smiley who arrives on a timer, walks through
  walls, cannot be killed, and kills robots as happily as he kills you. Every
  robot you kill delays him by a little. Once he is in the room your only
  answer is the door.
- Leave a room with robots still alive and the robots **call you a chicken**.

Reduced or removed in §17, but the list above is the game.

## 3. The central decision: how a frame gets erased

This is [Asteroids §3](asteroids-design.md#L88)'s question again, and Berzerk
asks it with two facts Asteroids did not have:

- **The maze is static.** Sixteen line segments at most, and they do not move
  for the life of the room. Erase-in-place never has to redraw them;
  clear-and-redraw does, every frame.
- **There are up to twenty moving objects**, not twelve, and each is *small* —
  a robot is 8 × 11 arcade pixels, against a large rock's 40.

Those two pull in opposite directions, so the prediction has to be argued
rather than assumed.

**The prediction is clear-and-redraw, and the reason is the row-span tracker.**
The dirty-tile tracker keeps one inclusive span per 16-pixel tile row
([Asteroids §3.3](asteroids-design.md#L222)), so *scattered* is expensive
whatever the object size: eleven robots spread over five columns and three rows
put a wide span into most of the fifteen rows the split screen presents, and a
present that touches most rows costs about what a present that touches all of
them costs. Erase-in-place would then pay a second full drawing pass — §15
prices it at **6.8 ms** — and buy back a present saving that the tracker will
not give it. Against that, it saves redrawing the maze, which §15 prices at
**0.8 ms**. 6.8 against 0.8 is not close.

**M0 measures both anyway**, at 4 and 11 robots, because Asteroids' §12
predicted the opposite of what its M0 read and the correction was worth having.
The harness is `tests/logo/p11rocks` with a room in it.

Both strategies present with `sync`, so neither shows a frame being drawn; the
refresh *mode* is orthogonal to the erase *strategy*
([Asteroids §3.1.2](asteroids-design.md#L146)).

## 4. The viewport is 240 rows, and the text rows are the cabinet's own HUD

`splitscreen`: graphics rows 0–239, text lines 24–31 below. Two things follow.

**It is worth about 4.7 ms.** The split present is 15 tile rows instead of 20 —
19.2 ms against 26.45 at 150 MHz measured
([Battlezone §6](battlezone-design.md#L317), §12.2), which at 300 MHz is
**14.0 against 18.70** (§15.1 derives it).

**And the bottom of the screen is where the cabinet puts its text anyway.** The
score is bottom-right, the clear-room bonus flashes bottom-left ($249F prints
`BONUS` at (0x60, 0xD5)), the remaining lives are a row of little men beside
the score ($25B5, character $80), and the robots' taunts (§14) have nowhere
else to go. Free authenticity, the same way it was for Battlezone.

**Geometry.** Pixel row is `-y + 160`, so the presented band is turtle
`y` in `[-79, 160]` and its optical centre is **`y = +40`**, not 0. Every
screen constant in this document is cut against that.

## 5. The playfield, at 1:1

The arcade room is **244 × 204 pixels**, walls included: the left wall is at
x = 4, the right at x = 248, the top at y = 0 and the bottom at y = 204
($256F–$2586 draw all four). It divides into a **5 × 3 grid of cells, each
48 × 68**, with column boundaries at x = 56, 104, 152, 200 and row boundaries
at y = 68, 136 — which is exactly what `WALLINDEX` ($1CE7) tests against, at a
two-pixel offset for the sprite's corner.

**This port uses the arcade's pixels as its turtle steps, 1:1**, with the
playfield centred at `(0, +40)`:

```
turtle x = arcade x - 126        x in [-122, +122]
turtle y = 142 - arcade y        y in [ -62, +142]
```

Two hundred and forty-four steps of 320, two hundred and four rows of 240.
Every constant in the disassembly — hit boxes, bolt lengths, speeds, spawn
tables, the 48 × 68 cell — then transfers **verbatim**, which is the single
biggest simplification available and the lesson
[Battlezone §16.10](battlezone-design.md#L2788) paid to learn late. It also
keeps the cabinet's *relative* scale honest: a robot is 11 of 223 rows there
and 11 of 240 here, 4.9 % against 4.6 %.

**The alternative, if the play test says the figures are too small**, is
`fullscreen` at 1.25× — a 305 × 255 playfield, +4.7 ms of present, and every
constant multiplied at the drawing seam only. §22 Q1.

**The naming trap in the disassembly.** `DRAW_VERTICAL_WALL` ($264C) steps `L`
by 48 and `DRAW_HORIZONTAL_WALL` ($2662) steps `H` by 72, and for the wall
drawer `H` is the *vertical* axis and `L` the horizontal one — the opposite of
`WALLINDEX`, which takes `H` as x. So the routine names are backwards with
respect to the screen: "vertical wall" draws a horizontal run one cell wide.
The two conventions are checked against each other in §6.2 and they are
consistent; only the names are not.

## 6. The room is generated, not stored — and it is a function of where you are

This is the best thing in the Berzerk ROM and it ports for nothing.

### 6.1 The seed is the room's own coordinates

```
2540: ld hl,($4345)      ; ROOM_X in L, ROOM_Y in H
2543: ld ($435C),hl      ; = RNG_SEED
```

`RNG_SEED = ROOM_X + 256 · ROOM_Y`, set at the top of every room build. So the
maze **and** the robot placement that follows it are a pure function of the
room's coordinates: walk out of a room and back into it and you get the same
room. Nothing is stored, and an infinite maze costs two globals.

The generator ($2678) is a 16-bit LCG whose output is the high byte:

```
seed := (7 · seed + 0x3153) mod 65536
random := seed >> 8                       ; 0..255
```

In Logo that is two statements, and it must be **this** generator rather than
`random`, because the whole point is reproducing the room. (`rerandom` reseeds
the interpreter's PRNG, which is a different stream and is not indexable by
room coordinate.)

### 6.2 Eight intersections, four choices each

`CREATE_ROOM` ($25EB) is called twice — at `(x=56, y=68)` and at
`(x=56, y=136)`, i.e. once per interior row boundary — and each call steps x by
48 while x < 220, so it visits **x = 56, 104, 152, 200**. Eight interior grid
intersections in all. At each one it draws exactly one wall segment, chosen
from four by `random and 3`:

| `rand & 3` | segment | cells it walls |
|---|---|---|
| 2 | horizontal, one cell to the **right** | bottom of the cell above-right, top of the cell below-right |
| 3 | horizontal, one cell to the **left** | bottom of above-left, top of below-left |
| 0 | vertical, one cell **above** | right of above-left, left of above-right |
| 1 | vertical, one cell **below** | right of below-left, left of below-right |

That is the entire maze generator: **eight segments, one per intersection.**
Nothing checks connectivity, nothing prevents a sealed pocket, and the rooms
that come out are the rooms everybody remembers.

The bit bookkeeping is the `set` instructions at $2617–$264B against `ix+0`,
`ix+1`, `ix+5`, `ix+6` — an index into a 15-byte, 5-wide, row-major array where
`+1` is the next column and `+5` the next row, which is what makes the mapping
above readable at all. It is worth writing out because it is the check that the
two coordinate conventions in §5 agree, and they do.

### 6.3 The wall table *is* the collision system

`MAZE_ZONES` is 15 bytes, one per cell, each a mask:

```
bit 0 = wall on this cell's LEFT     bit 2 = wall on its TOP
bit 1 = wall on its RIGHT            bit 3 = wall on its BOTTOM
```

It is pre-loaded from the template at $268C (an `ldir` at $2567) and then the
eight segments set their bits into it:

```
row 0:  05 04 04 04 06
row 1:  01 00 00 00 02
row 2:  09 08 08 08 0A
```

**The border cells are walled on all four outer sides even where the exits
are** — cell (row 1, col 0) carries a LEFT wall although the left doorway is
exactly there. That is not a bug; the table is what *robots* consult, and
robots never leave the room. The player's exits are handled by his own position
test (§8.3), which is why the arcade needs both.

**And this is where the port stops emulating hardware.** The cabinet does
wall collision by *drawing*: sprites and bolts go into "magic RAM" in XOR mode
and an `intercept` bit says whether a pixel landed on a pixel ($157A, in
`MOVE_AND_DRAW_BOLT`). We have no such bit and must not fake one with
`colourunder`, which would be a read-back per pixel. **The wall mask replaces
it**, and the arcade already has all the machinery: `WALLINDEX` maps a point to
a cell and a mask, `IQ` ($1C6E) probes four points around a robot and clears
the directions those masks forbid, and the Vectrex port
(`MapPointToQnMakeRel`, C899/C89A) does exactly the same thing for *everything*
including bolts. So:

> **A wall test is: which cell am I in, is the edge I am crossing walled.**
> Four statements, no pixels, and it cannot tunnel.

## 7. The models

The point of the exercise. Each figure is stated as a segment count because
that is what §15 spends.

### 7.1 The walls are the easy half, and the border is one closed path

Sixteen segments at most: eight border runs and up to eight interior ones. The
border is four sides each broken by one doorway, and drawn naively that is
sixteen `setpos`-and-`fd` statements. Drawn as **one circuit with the pen
toggling**, it is twelve statements and no `setpos` at all:

```logo
to draw.border
  pu setpos [-122 142] seth 90 pd
  fd 96  pu fd 48  pd fd 96      ; top, door at column 2
  rt 90
  fd 68  pu fd 68  pd fd 68      ; right, door at row 1
  rt 90
  fd 96  pu fd 48  pd fd 96      ; bottom
  rt 90
  fd 68  pu fd 68  pd fd 68      ; left
end
```

Interior segments do need placing, but there are at most eight and each is one
`setpos`, one `seth` and one `fd`.

**Pen size stays 1.** A 2-pixel pen would read better and it costs round caps
that spill outside the stroke, which is the artefact that once made a
present-cost harness measure every frame as a full screen
([hardware-notes §9.1](hardware-notes.md)).

### 7.2 The bolt needed no translation at all

The BOLT structure comment in the disassembly says it outright:

> *Bolts are programatically plotted pixel-by-pixel on screen, starting from
> the "head" at X,Y and ending at the "tail" at TailX,TailY. They do not have
> pattern data in ROM.*

A Berzerk bolt **is** a line segment with a head, a tail and a direction. It is
already a vector; there was never a sprite to convert. `MaxLength` is **8** for
the player ($1F7C) and **5** for a robot ($2921), and that is the drawn length.

### 7.3 The robot is a box with eyes that scan

The arcade sprite is 8 × 11, and the eleven bytes are legible as a drawing:

```
3C  ..####..     head
66  .##..##.     eyes            <- this row is the only one that animates
FF  ########     shoulders
BD  #.####.#     body
BD  #.####.#
BD  #.####.#
3C  ..####..     hips
24  ..#..#..     legs
24  ..#..#..
24  ..#..#..
66  .##..##.     feet
```

Eight frames of the standing animation ($1000) differ **only in that second
byte** — `66 4E 1E 7E 78 71` — which is two eyes tracking left to right and
back. In vector terms that is a head outline, a shoulder bar, a body, two legs,
two feet and **two dots whose x offset comes from a six-element cycle**: about
**9 segments and 2 dots**, and the animation is one `item` rather than eight
sprite frames. The walking frames ($1013, $1027, $1030) move the feet; the same
trick applies.

### 7.4 The man's arms are where the vector form actually wins

The arcade carries **nine separate shooting sprites** ($1309–$1380, indexed
through `SR.TAB` at $2067) — one per direction the arms point — plus three
running frames each way. In vector form the arms are **one segment drawn at the
firing heading**:

```logo
to draw.man                ; ~7 segments
  ; head, spine, two legs, two feet
  ...
  seth item :fire.dir :dirs
  pu bk 3 pd fd 6          ; both arms, as one stroke
end
```

Nine sprites collapse to one statement and a table lookup. This is the single
clearest illustration of why Berzerk wants to be a vector game, and it is
worth the sentence in the file that says so.

### 7.5 Otto is an `arc`

A circle, two eyes and a grin — four statements, three of them `arc`, and the
reference's own worked example draws precisely this face ([`arc`](../reference/Pico_Logo_Reference.md)).
His bounce is the arcade's: `OttoBounceOffsets` in the Vectrex port
(`00 08 0C 0E 10 12 12 12 14 …`) is a vertical offset walked from a counter, so
he moves in a straight line towards you and his *drawing* hops.

### 7.6 The two deaths

**A robot** comes apart into a cloud — the arcade has four explosion frames
($103B, 16 × 17 pixels), the Vectrex draws random dots from the death point.
Six short segments at random headings over four frames, shrinking, is both and
costs 6 statements a frame for four frames.

**The player** is *electrocuted*, which is a different thing and the arcade
gives it its own sprite ($12B3) and a 45-tick pause ($1FB6). The Vectrex wobbles
the man's scale through `PlayerFriedScales` — `08 07 06 05 05 05 06 07 08 09 08
07 06 05 06 07`, sixteen frames — and that is the version to copy, because a
figure that grows and shrinks reads as electricity where random limbs read as
noise. The pause is the arcade's 45 ticks.

**No sprites, no costumes, no `stamp`.** Every mark this game makes is one
turtle with a pen, which would make it the first game in the tree that is
*entirely* pen — Asteroids still carries four shot-carrier turtles with a
two-pixel dot on them, and Berzerk's bolts are lines, so it does not need them.

## 8. The player

### 8.1 Position, speed and the tick

The arcade runs its objects off a `TIME`/`TPRIME` counter at 60 Hz
(`MOVE_ANIMATE_VECTOR`, $27A9): the object moves when `TIME` hits zero and
`TIME` reloads from `TPRIME`. **The player's `TPRIME` is 2** ($2004), so he
moves one pixel every other tick — 30 pixels a second, crossing the 244-wide
room in 8.1 seconds.

At 20 fps one of our frames is three of the cabinet's, so the port keeps arcade
*units* and scales the *rate*: a per-frame velocity of `3 / TPRIME` arcade
pixels. The player is **1.5 px/frame**, and positions are floats, which they
are anyway. Nothing else in the disassembly has to be converted.

### 8.2 Input, and the arcade's one clever control

`pollkeys` once a frame, then `keydown?` — level for the controls that hold,
edge for the ones that fire, which is the rule
[Asteroids §7.3](asteroids-design.md#L562) wrote down.

Arrows are 180 (left), 181 (up), 182 (down), 183 (right); **two held at once is
a diagonal**, which is what makes an eight-direction game playable on this
keyboard at all. SPACE fires, `Z` pauses, `ESC` quits to the attract screen —
the same three as Battlezone, because a key you press without thinking should
not move between games.

The control worth porting exactly is this, from the 2600 manual and visible at
$1EEF: *"if you depress the fire button while moving the Joystick, your man
will stand and fire lasers in any direction you move the Joystick."* The fire
test comes **before** the movement test, and `FIRE` ($1F33) zeroes both
velocities. So **SPACE plus a direction stands still and shoots that way**;
direction alone walks. That is the whole aiming system and it is two lines.

### 8.3 Walls kill, and doors do not

Wall contact is §6.3's cell test against the player's box. The Vectrex uses a
**smaller box for walls than for bullets** — `PlayerContactBounds2` is ±4
against `PlayerContactBounds`'s ±5/−9 — and Malban's comment says why:

> *radius for wall check was lower than radius for bullet check -> shot thru
> wall possible! when player near wall!*

Which is a bug report from 2019 that this port gets for free. Take the ±4 box
for walls and check the bolt's spawn point against the same table.

The exits are a position test, not a wall test ($2157):

| leaves when | room | re-enters at |
|---|---|---|
| `x` ≥ 246 | `ROOM_X + 1` | `x = 8` |
| `x` ≤ −4 | `ROOM_X − 1` | `x = 230` |
| `y` < 2 | `ROOM_Y − 1` | `y = 185` |
| `y` ≥ 190 | `ROOM_Y + 1` | `y = 6` |

The other axis is preserved, so you come in where you went out. A new game
starts at `x = 30, y = 100` — just inside the left doorway — from
`DEFAULT_PLAYER_STATE` ($187F), which is also where the three lives come from.

## 9. The robots

Eleven slots, held as **parallel flat lists** the way Asteroids holds its rocks
([§5](asteroids-design.md#L308)), not as 88 globals: `r.x`, `r.y`, `r.dir`,
`r.state`, `r.time`, `r.eye`, `r.shot`. Seven lists of eleven, seven globals.

### 9.1 How many, and where

`$2117`–`$2145` is the loop, and it is a rejection sampler:

```
counter := 22
loop:  if random() >= threshold:  place a robot
       counter := counter - 2;  until zero
```

Eleven passes, so **at most eleven robots**, which is what the Vectrex manual
says. The `threshold` is `$434A`, and each room does `A := A + 0x60` as **BCD**
with `daa` ($20D8), so it cycles `60, 20, 80, 40, 00` and repeats with period
five. Robot count per room is therefore not a ramp at all — it is a five-room
cycle:

| threshold | P(robot per slot) | expected robots |
|---:|---:|---:|
| `$20` | 87.5 % | 9.6 |
| `$80` | 50.0 % | 5.5 |
| `$40` | 75.0 % | 8.3 |
| `$00` | 100 % | 11 |
| `$60` | 62.5 % | 6.9 |

Position comes from an eleven-entry table indexed by the same counter, plus
`random and 31` of jitter in each axis ($2127–$213D). The Vectrex's copy of
that table is legible (`RobotStartingPositionTable`, y,x pairs) and is the one
to transcribe.

### 9.2 Seek is four comparisons

`SEEK` ($23EF) is the whole AI:

```
dx := player.x - robot.x     ; LEFT if negative, RIGHT if positive, none if 0
dy := (player.y + 2) - robot.y   ; UP / DOWN, same
dir := horizontal_bits + vertical_bits
```

Direction is the four DURL bits — `LEFT 1, RIGHT 2, UP 4, DOWN 8` — and the
`+2` is the arcade compensating for the player being taller than a robot. That
is it: robots walk straight at you and their entire tactical repertoire is
walking into things.

### 9.3 `IQ` is the wall avoider, and it is deliberately stupid

`IQ` ($1C6E) probes the cell masks at the robot's four corners — offsets
`(−4, −4)`, `(+12, −4)`, `(+12, +15)`, `(−4, +15)` — ORs them in pairs and
clears any desired direction whose edge is walled:

```
DOWN  blocked if (top-left | top-right)     has BOTTOM
UP    blocked if (bottom-left | bottom-right) has TOP
RIGHT blocked if (top-left | bottom-left)   has RIGHT
LEFT  blocked if (top-right | bottom-right) has LEFT
```

Malban's note on the Vectrex version is the important one:

> *wall collision detection always is done to the walls north, south, west,
> east but never diagonal — this is the original code. therefore on "corners"
> robots tend to slide into walls...*

**Do not fix this.** Robots killing themselves on corners is a scoring
mechanism the manuals both describe and a strategy the 2600 manual teaches
("you can influence them by your movement, causing them to shoot at and collide
with each other or run into walls"). There is also a shortcut worth keeping:
if the robot and the player are in the *same* cell, `IQ` returns the desired
direction untouched ($1C92) — no probing at all.

Robot speed is `ROBOT_SPEED`, which starts at **5** and **decrements by one per
room, floor 1** ($20E1). The player is 2. So robots begin slower than you and
are faster than you from the fifth room on, permanently.

## 10. Bolts

### 10.1 Slots

Two player bolts ($437B, $4383) and `RBOLTS` robot bolts ($438F), `RBOLTS`
coming from the difficulty table (§13) and being **0 below 300 points** — the
manuals' "robots don't shoot in the first maze".

### 10.2 Motion, and one thing to confirm on a board

A bolt advances one pixel per processing pass and its drawn length grows to
`MaxLength`. `HANDLE_PLAYER_BOLTS` ($14F3) processes the array **twice** per
frame: two slots the first time, then `(mod + 2) and 7` slots the second, where
`mod` is the difficulty table's second column. So the low slots are processed
twice a frame and the high ones once — **player bolts travel at twice a robot
bolt's speed until the score is high enough**, which is what the 2600 manual
means by "robots move and shoot slower than your man". At `mod = 5` everything
moves at the player's rate.

That reading is from the loop structure rather than from a comment, so **M3
confirms it against MAME** before it becomes a number in the file. It is the
one mechanism in this document that is inferred rather than read.

### 10.3 A robot fires only when nearly lined up

`SHOOT` ($287F) decides the bolt's direction from the deltas, and refuses
entirely if none of three windows matches:

| test | result |
|---|---|
| `dx` in −2 … +5 | vertical shot |
| `dy` in −4 … +6 | horizontal shot |
| `abs(dy) − abs(dx)` in −10 … +5 | diagonal shot |
| otherwise | **no shot** |

So a robot must be within a few pixels of your row, your column, or your
diagonal. The 2600 manual's advice — *"Unlike you, robots cannot shoot on the
diagonal. If you stand diagonally to one you will be out of its line of fire"*
— is **wrong about the arcade**; the third window is right there. It is right
about the 2600. This port follows the arcade and §17 records the divergence.

The robot stops dead before firing ($28EF zeroes both velocities), with a
comment that is the best line in the disassembly: *"If you didn't do this, the
robot could walk into the bolt it's just fired and blow itself up."*

### 10.4 What a bolt hits

Wall: §6.3's cell-edge test. Actor: the arcade's own rectangle test
(`COLLISION_DETECTION`, $15CB) is *the sprite's own width and height*, read out
of the pattern header — 8 × 11 for a robot, 8 × 16 for the man. The Vectrex
reduces both to boxes (`RobotContactBounds` ±6, `PlayerContactBounds` +5/−9,
+5/−5) and this port takes the boxes, because we have no pattern headers to
read.

**A bolt checks the player first and then every robot** ($15A4, $15AB) — which
is what makes a robot's stray shot kill another robot and pay you for it. That
is a rule, not an accident, and §12 keeps it.

## 11. Evil Otto

- **He arrives on a timer.** `OTTO_TIME := ROBOT_SPEED + RSAVED + RBOLTS`
  ($2ABC) — so a fast, crowded, well-armed room buys you *more* time, which is
  backwards until you notice it is the room that is already hard. It counts
  down one per activation and **every robot you kill adds two back** (`inc (hl)`
  twice in `BLAM`, $2486).
- **He starts where you came in**, clamped away from you: if the player's
  x < 24 Otto starts at x = 2, if x ≥ 230 he starts at 248, and if y ≥ 180 he
  starts at 160 ($2A9D–$2AB9). The 2600 manual's *"Evil Otto always enters
  where the man enters"* is this code.
- **He walks through walls.** He is steered by `SETDIR` ($2B39) and never by
  `IQ`, so nothing consults a wall mask on his behalf. One omission, one rule.
- **He kills robots** by touching them, and you are paid for those
  (`CheckForRobotHitByOtto` in the Vectrex; `BLAM` reached the same way in the
  arcade). The 2600 manual's strategy of putting robots between you and Otto is
  real.
- **He cannot be shot.** The 2600's rebound and invincible variants are 2600
  variations; the arcade has one Otto and he is invincible. §17.
- He speaks on arrival: *"INTRUDER ALERT! INTRUDER ALERT!"* ($2ADB → $2BDE).

## 12. Scoring

| | |
|---|---|
| A robot, however it died | **50** ($2480, `UPDATE_SCORE` with B=1, C=5) |
| Every robot in a cleared room, again | **10 each** ($2491–$249D, looping `RSAVED` times) and the word `BONUS` on screen |
| Bonus life | at 5,000 or 10,000, a DIP switch ($237A/$2391), **once only** — `XTRAMEN` latches |
| Lives | 3 to start |

The Vectrex chose "add life every 5000" and this port does the same, once,
because that is the arcade's own switch and the more generous of its two
settings.

`UPDATE_SCORE` is BCD, which we do not need; a number is a number. What we do
need is its *interface*, because the difficulty table (§13) indexes on the
score's decimal digits.

## 13. The campaign is the score, and it is a table

`C.WALLS` ($369F) runs at every room build and sets the difficulty from the
**score**, not the room number, out of one of two tables. Below 10,000 points
it indexes on the hundreds-and-thousands digits; above, on a combined
ten-thousands digit. Each entry is `threshold, RBOLTS, bolt-slot modifier,
RWAIT, colour`:

**Below 10,000** ($3794):

| score < | robot bolts | mod | RWAIT |
|---:|---:|---:|---:|
| 300 | **0** | 0 | 80 |
| 1,500 | 1 | 0 | 80 |
| 3,000 | 2 | 0 | 20 |
| 4,500 | 3 | 0 | 10 |
| 6,000 | 4 | 0 | 10 |
| 7,500 | 5 | 0 | 15 |
| 9,000 | 1 | 1 | 60 |
| — | 1 | 1 | 50 |

**From 10,000** ($37BC): thresholds at 10k, 11k, 13k, 15k, 17k, 19k and above,
with `RBOLTS` and `mod` climbing together 1→5 and `RWAIT` falling 45 → 5.

Two things this says that no manual does. **The difficulty resets at
7,500** — five bolts drops back to one — and then climbs again on the second
table with the *modifier* raised, which is the 2600 manual's "at maze 16, the
robots reset to move slowly again but their firing speeds remain equal to your
man's", expressed in the arcade's own currency. And **`RWAIT` is separately
decremented by 10 per room, floor 10** ($20F7), on top of whatever the table
set, so the table sets the level and the room walks it down.

The fifth column is a colour, which this port has no use for; the game is white
on black like the other two.

**This whole section is a table and two lookups**, and it is the cheapest
faithfulness in the document.

## 14. Sound, and the voice

### 14.1 The effects

The cabinet's sound board is a 3-tone-plus-noise chip driven by an **audio
bytecode interpreter** ($1D12–$1E1F, sixteen opcodes: relative branch, loop,
move byte/word, store immediate, add, arithmetic shift, fill) whose programs
write to phantom registers `TCR1..3`, `TMR1..3`, `NOISE`, `VOL1..3`. Effects
are numbered by **priority** and a lower-priority effect will not interrupt a
higher one:

| effect | priority | at |
|---|---:|---|
| player fires | 0 | `SFIRE` $33BD |
| robot explodes | 1 | `SBLAM` $348A |
| robot fires | — | `SRFIRE` $34E7 |
| player electrocuted | 3 | `SFRY` $3439 |
| extra life | — | `SXLIFE` $3866 |

Our PSG has eight voices, ADSR and stereo pairs, and this maps directly: three
tone voices plus noise, with the priority rule as a single global holding the
running effect's number. **The frequencies and envelopes come off the
disassembly at M6, not out of this document** — the bytecode has to be walked
to read them, which is the method
[Battlezone §16.14](battlezone-design.md#L3394) used and the reason its sound is
right.

### 14.2 The voice, which is the hard one

Berzerk talks, and it is the thing everyone remembers. Thirty words
($01–$1D: `KILL ATTACK CHARGE GOT SHOOT GET IS ALERT DETECTED THE IN IT THERE
WHERE HUMANOID COINS POCKET INTRUDER NO ESCAPE DESTROY MUST NOT CHICKEN FIGHT
LIKE A ROBOT`), assembled into sentences at runtime:

- **"INTRUDER ALERT! INTRUDER ALERT!"** when Otto arrives ($2C4A).
- **"THE HUMANOID MUST NOT ESCAPE"** / **"THE INTRUDER MUST NOT ESCAPE"** when
  you leave a room having cleared it ($2BE4).
- **"CHICKEN! FIGHT LIKE A ROBOT!"** when you leave one that you have not
  ($2C13) — and it sets the `IS_CHICKEN` flag, so the game remembers.
- **"GOT THE HUMANOID, GOT THE INTRUDER"** when you die ($2C40).
- A random taunt built from `<GET|CHARGE|ATTACK|DESTROY|SHOOT|KILL>` plus
  `<THE CHICKEN|IT|THE HUMANOID|THE INTRUDER>` on a countdown
  (`GENERATE_ROBOT_SPEECH`, $2B97, `TALK_TIMER`).

**What ships in v1: the text.** Split-screen line 26, centred, for about a
second — which is exactly what the Vectrex port does (`HumanoidString`, "GOT
YOU HUMANOID"), and it preserves the sentence *assembly*, which is the part
that makes the taunts feel alive. It costs one `setcursor` and one `type`.

### 14.3 Speech in the interpreter, priced

Raised while this was being written, and it deserves a straight answer.

**The output path is not the problem.** The PSG is PWM slice 5 at an 11-bit,
73.2 kHz carrier with a **36.6 kHz software mix rate** and a DMA half-buffer
refill ([sound-design.md](sound-design.md) §6). A speech engine is a ninth
source into that mixer. There is no new hardware question.

**Emulating the cabinet's chip is the wrong target, and for one reason.** The
part is a word-addressed speech chip — MAME's Berzerk driver identifies it as a
TSI **S14001A** — driven through port $44 with a busy poll ($1745–$1765). The
decoder is small and well documented. But *the voice is not in the decoder, it
is in the speech ROM*, and that ROM is Stern's copyrighted data. A chip
emulation with no ROM says nothing, and the ROM cannot live in this repository.
So "emulate the chip" delivers an empty box, and the box is the easy half.

**Three ways to get a voice, and a recommendation:**

| | what it is | cost | verdict |
|---|---|---|---|
| A | S14001A emulation, user supplies the ROM | ~400 lines of C, ~1 KB of state | **No.** Unshippable data, and a primitive nobody without a ROM can use |
| B | **A formant synthesiser and a `say` primitive** | ~10 KB of code and tables, no data files, arbitrary text | **Yes, as its own roadmap item** |
| C | Our own 30 words, recorded and shipped as ADPCM | ~2 KB a word, ~60 KB of flash, plus a sample-playback primitive | Fallback |

**B is the right shape** and Berzerk is the demonstrated need the roadmap waits
for. A SAM-class formant engine — the 1982 lineage, about 6 KB on a 1 MHz
6502 — produces exactly the era of robot voice this game wants, needs no data
files, and gives *every* Logo program a voice rather than giving one game a
sound effect. Apple Logo had `SAY`; the precedent is the right age for this
interpreter. Sizing it honestly: ~10 KB of flash, a few hundred bytes of SRAM
for the frame state, one voice slot in the mixer, and a `say`/`sayphonemes`
pair in the reference.

**It is a separate design and a separate roadmap entry**, and Berzerk must not
block on it. §14.2's on-screen text is the shipped behaviour; if `say` lands,
Berzerk's five sentences become five calls and the text stays as a caption. The
sentence-assembly code is written once either way.

## 15. Frame budget

Estimated, not measured. Every figure here is derived from board measurements
of *other* games, and the derivation is stated so M0 can say which step was
wrong. [Asteroids §12](asteroids-design.md#L1387) was **72 % low** on drawing
statements because it bracketed a primitive that rasterises between two that
only compute; that correction is applied here.

### 15.1 Units, at 300 MHz

From [Battlezone §3.1](battlezone-design.md#L119) (Pico 2 W / Plus 2 W,
150 MHz) divided by the measured interpretation ratio of **2.06**
([§12.3](battlezone-design.md#L925)):

| unit | 150 MHz | **300 MHz** |
|---|---:|---:|
| arithmetic statement, a global | 48.5–53.5 µs | **~24 µs** |
| bare `repeat` iteration | 4.5–5 µs | **~2.4 µs** |
| drawing statement, 17 steps, pen down | 66–68 µs | **~33 µs** |
| drawing statement, 200 steps, pen down | 130–248 µs | **~63–120 µs** |
| `item` on a short list | ~16 µs | **~8 µs** |
| present, `splitscreen` (240 rows) | 19.2 ms | **~14.0 ms** |
| present, `fullscreen` | 26.45 ms | 18.70 ms |

The split present is derived, not measured: full-screen went 26.45 → 18.70, and
since the SPI wire does not care what the CPU is doing, that splits as
**10.95 ms of wire and 15.5 ms of CPU**; three-quarters of `10.95 + 15.5/2` is
14.0. M0 reads it directly and the derivation is the control.

### 15.2 The estimate, at eleven robots

**Drawing:**

| | statements | ms |
|---|---:|---:|
| border, one circuit (§7.1) | 12 | 0.5 |
| interior walls, 8 × 3 | 24 | 0.3 |
| robots, 11 × (place + 9 seg + 2 dots + dispatch) | ~165 | 4.6 |
| the man | 12 | 0.4 |
| Otto | 6 | 0.2 |
| bolts, 7 × 4 | 28 | 0.8 |
| | | **6.8** |

**Logic:**

| | ms |
|---|---:|
| robots: seek + `IQ` + move + fire test, 11 × ~30 statements | 8.3 |
| robot list reads and writes, 11 × 14 `item`/`.setitem` | 1.2 |
| bolts × robots, worst case 7 × 11 with a cheap gate first | 4.0 |
| player: input, move, wall, exit | 0.8 |
| player × 11 robots, touch | 0.8 |
| Otto: move + 12 contacts | 1.0 |
| HUD, timers, difficulty | 1.0 |
| | **17.1** |

**Body 24 ms, present 14 ms, frame ≈ 38 ms — about 26 fps.**

### 15.3 The gate

> **M0 passes if the worst frame — eleven robots, seven bolts, Otto live —
> is inside 50 ms at 300 MHz.** That is 20 fps, and the estimate has 12 ms of
> headroom against it.

Twenty is the target rather than fifteen because the cabinet's *player* runs at
30 Hz (`TPRIME` 2 of 60) and an eight-direction dodging game gets worse as the
frame gets coarser, in a way a tank game does not.

### 15.4 The two numbers most likely to be wrong

**The collision loop.** 77 pairs is the worst case and 3 × 11 is typical, but
"typical" is not what a budget is for. The gate is a cheap first test — one
comparison on `abs(dy)` before the four that follow — and if that is not enough
the fallback is bucketing robots by cell, which costs bookkeeping the arcade
does not do. M0 measures the pair loop on its own.

**The per-robot model dispatch.** Asteroids measured its nine-way shape
dispatch at **360–398 µs per rock per pass** — two procedure calls and up to
four `if`s, each evaluating an `item` — which at eleven robots would be 2 ms at
300 MHz and is not in the table above. It is avoidable and the avoidance is
specified: **one list of five procedure names indexed by facing group, reached
with `run`.** One `item`, one `run`, no `if` chain. Robots have five facing
groups in the ROM ($252D) and the man has three, so nothing needs nine.

### 15.5 The clock is a precondition

At 150 MHz the body doubles to 48 ms and the present is 19.2: a 67 ms frame,
which is 15 fps with nothing to spare and no room for the worst frame. Berzerk
requires `hw.setcpu "fast` and refuses to start without it, the same call
[Battlezone §16.7.3](battlezone-design.md#L2173) made. The reference is explicit
that 300 MHz is an overclock and that a chip may decline it; the game says so
on the attract screen.

## 16. Interpreter levers, priced

The user has put interpreter work on the table. Berzerk's answer is unusual:
**it does not want the levers the other two games wanted.**

- **No trigonometry at all.** Eight directions, axis-aligned walls, boxes for
  hit tests. `sincos` would save nothing; there is nothing to save.
- **`min`/`max`** are still absent from the reference and still worth adding on
  their own merits (Battlezone L1). Berzerk clamps in three or four places.
  ~0.2 ms. Not a reason for anything.
- **Arrays (L2).** Battlezone measured `item` at **~16 µs fixed plus 0.0041 µs
  an element** and concluded that at four-element lists O(1) indexing removes
  the part that is already nothing. Berzerk's lists are **eleven** long and it
  reads them ~150 times a frame, so it is a better case — but only ~0.4 ms
  better, because the fixed cost dominates at eleven too. **Still not the
  demonstrated need.** What would change that verdict is a *mutation*
  primitive: `.setitem` on a list is the write half and Berzerk does ~40 a
  frame. M0 prices `.setitem` explicitly, because nothing in the tree has.
- **A `say` primitive (§14.3).** The one genuinely new thing this game asks
  for, and it is a language feature with its own justification. It buys the
  frame **nothing** and the game a great deal.

**Verdict: Berzerk needs no interpreter change to ship.** It has one to ask
for, and that one is not on the frame's critical path.

## 17. Reduced-resource choices

Cut from the arcade, with the reason:

| Cut | Reason |
|---|---|
| **Two-player alternating** | The 2600 manual's own choice, and the second score, the second `MAN_PTR` and the player-swap path are a third of the state for none of the game |
| **The screen scroll between rooms** | `SCROLL_UP`/`DOWN`/`LEFT`/`RIGHT` ($2175–$2419) slide the whole bitmap. Here that is a full-screen present per step — eight steps is 112 ms — and the room has to be drawn twice during it. Reconsidered at M7 as a four-step slide if the budget allows; a cut to `clean` and rebuild otherwise |
| **Colour** | The cabinet colours the walls per difficulty out of the table's fifth column and the man per player. White on black, like Asteroids and Battlezone |
| **The 2600's twelve game variations** | Rebound Otto, invincible Otto, no Otto, non-shooting robots — all 2600 inventions. The arcade has one Otto and he is invincible |
| **Speech** | §14.2/§14.3. Text now, a voice if `say` lands |
| **Coins, bookkeeping, CMOS, the demo attract mode, the cocktail cabinet, four languages** | Cabinet furniture. The attract screen is Asteroids' and Battlezone's — title, controls, top ten |
| **The Vectrex's blocked doors** | Malban's addition at high skill, not the arcade's |

Kept although it is tempting to cut: **robots killing each other and killing
themselves on corners** (§9.3), because it is a scoring mechanism and a
strategy, not a defect.

## 18. Memory, and the two ceilings

Both are hard and both were found by Battlezone the expensive way.

**`MAX_PROCEDURES` is 128** and Battlezone defines exactly 128, so its file
cannot take a 129th `to` and the failure mode is silent — the *last* definition
in the file goes missing. Berzerk's budget is **100 procedures**, with the test
naming the count, and the two games can never share a workspace.

**`MAX_GLOBAL_VARIABLES` is 254** and what matters is the *peak*, not the
load-time count: Battlezone loads at 186 and peaks at 229, because fifty of its
names are minted the first time a procedure runs. Berzerk's budget is **220
peak, 16 free**, enforced by a test that **plays a game** rather than reading
the source.

Berzerk's own shape helps on both counts. Seven eleven-element lists is seven
names for what would otherwise be 77. The maze needs **no storage at all** — it
is regenerated from `room.x`, `room.y` (§6.1) — and the wall masks are one
fifteen-element list. There are no models to hold: every figure is a straight-line
procedure of `fd`/`rt` literals, which is
[Asteroids §6.1](asteroids-design.md#L373)'s rule and costs flash, not SRAM.

## 19. Milestones

Each leaves `ctest --preset=tests` green, and each ends with a board reading.

| M | What | Gate |
|---|---|---|
| **M0** | The measurement harness | §15.3: worst frame inside 50 ms at 300 MHz, and the erase strategy decided by measurement |
| M1 | The room | The maze is a function of `(room.x, room.y)`; walk out and back and it is the same room |
| M2 | The man | Walls kill, doors work, the eight directions read right on the keyboard |
| M3 | The robots | Seek, `IQ`, the count cycle, robots killing robots |
| M4 | The bolts | Both bolt kinds, the three firing windows, the alignment rule confirmed against MAME (§10.2) |
| M5 | Evil Otto | The timer arithmetic, walls ignored, robots eaten |
| M6 | The campaign and the sound | Both difficulty tables, lives, bonus, the attract screen, the effects off the ROM |
| M7 | Polish | The taunts, the deaths, the play test, and the scroll if it fits |

### M0 — the harness, and what it must answer

Modelled on `tests/logo/p11rocks` and `tests/logo/p13m0`, writing to a file:

1. **Both erase strategies**, at 4 and 11 robots, body and present read apart.
   §3 predicts clear-and-redraw; the number decides.
2. **The frame at 1, 4, 8 and 11 robots**, so the budget is `c + m·n` and the
   slope can be argued about separately from the floor.
3. **The split present**, as the control on §15.1's derivation. If it reads
   13.5–14.5 ms the derivation held; anything else and the unit table is wrong
   before the game is written.
4. **The collision pair loop on its own**, at 7 × 11, because §15.4 says it is
   the number most likely to be wrong.
5. **`.setitem` on an eleven-element list**, which nothing in this tree has
   priced.
6. At 150 MHz **and** 300, on all three boards, because §15.5 turns on it.

Its Logo-side correctness — that the generator reproduces a room, that the
eight intersections land where §6.2 says, that `IQ` clears the right bits — is
`tests/test_p15m0.c` on the host, the way `test_p13m0.c` proved P13's harness
was worth carrying to a board.

## 20. Tests

`tests/test_berzerk.c`, mirroring `tests/test_battlezone.c`: the game is pure
Logo, so what the host can check is that it is **right**, which is the half a
board cannot see.

- **The generator.** `(room.x, room.y)` in, the same fifteen wall masks and the
  same eight segments out, every time. A short list of coordinates with
  hand-computed masks, taken from the LCG by hand.
- **The room is reproducible.** Walk right and back and assert the masks and
  the robot list are identical. This is the one test that proves §6.1.
- **`IQ` in both directions** — a robot against a wall may not walk into it, a
  robot in open ground keeps every bit it asked for, a robot in the player's
  own cell is not probed at all.
- **The three firing windows** (§10.3), at their boundaries, including the
  case that refuses to fire.
- **The exits**, all four, each asserting the room coordinate moved and the
  other axis was preserved.
- **The difficulty tables**, both, at every threshold and either side of it.
- **Scoring**: 50 a robot however it died, the `RSAVED × 10` bonus, one bonus
  life and only one.
- **The two ceilings** (§18): the procedure count named, and the global peak
  measured by *playing* a game.
- **The frame is drivable headless** — `init.game`, `setup.room`, `play.frame`
  — so `tests/test_bench_throughput.c` can take Berzerk as a third game
  subject, which is what [P14 §10](vector-direction-design.md) asked for when
  it noted the guard is thin at two.

## 21. Risks

1. **The collision pair loop** (§15.4). The only part of the budget with a
   quadratic in it.
2. **Eleven figures may be too small to read at 1:1** (§5). The escape is
   `fullscreen` at 1.25× and it costs 4.7 ms, which the budget has.
3. **The bolt double-processing** (§10.2) is inferred from a loop, not read
   from a comment. If it is wrong, robot bolts are twice as fast as they should
   be from the first room, which is a difficulty error and not a crash.
4. **The 100-procedure budget** is a guess until M3. Battlezone hit 128 exactly
   and had to be economised late; the cost of doing that at M6 is real.
5. **`say` (§14.3) is a separate project** and this game must not acquire a
   dependency on it. If it slips, Berzerk ships with captions, which is the
   Vectrex's own answer.
6. **A board that will not overclock** (§15.5) cannot play this game. The
   reference already documents that as a property of the chip; the attract
   screen has to say it too.

## 22. Open questions

- **Q1 — 1:1 or 1.25×?** §5. Answerable only by a person looking at a board.
  Ship 1:1 at M2 and ask.
- **Q2 — 20 fps or 15?** §15.3 argues 20 on the grounds that a dodging game
  degrades faster than a driving one. M0's `c + m·n` decides whether 20 survives
  eleven robots.
- **Q3 — how many rooms should a maze remember?** None, by §6.1 — but the
  cabinet's rooms repeat every 256 in each axis, and whether to reproduce that
  wrap or let the coordinates run is a one-line decision nobody has taken.
- **Q4 — does the scroll survive?** §17 cuts it and M7 reconsiders it. 112 ms
  of present for a transition is either the best 112 ms in the game or an
  irritation, and only playing it says which.
- **Q5 — should `say` be phonemic, textual, or both?** Not this document's
  question, but Berzerk is the caller that would settle it: five fixed
  sentences and one assembled at runtime from a 30-word vocabulary, which is an
  argument for text with a phoneme escape.
