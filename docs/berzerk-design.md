# Berzerk in Pico Logo (design)

Status: **M0 BUILT AND GATED 2026-08-29. The gate failed at 106.3 ms and the
figures changed because of it — the game is built to the cabinet's bitmaps,
not to vectors (§7), which brings the frame to 77.4.** Nothing else exists: no
game file, no M1. `tests/logo/p15m0` and `tests/test_p15m0.c` are the whole of
it. §15.3 carries the readings and §22 Q2 carries what is still open, which is
a frame-rate and robot-count question rather than a measurement.
**§14.3's `say` primitive shipped as [P16](say-design.md) on 2026-08-29**, and
its adoption — that item's M4 — moved here the same day, so this game speaks:
§14.2 and the M6/M7 rows of §19.

Berzerk is the third vector game in this tree and the first one that is not a
vector game in its own cabinet. Asteroids and Battlezone were XY machines
tracing a display list; Berzerk was a **raster** machine with a 256 × 223
bitmap, 8-pixel-wide sprites and a hardware pixel-intercept collision bit. So
this port has a translation to do that neither of the other two did, and
[P14](vector-direction-design.md) is the reason it is worth doing: the walls,
the bolts and the figures in Berzerk are all *line drawings pretending to be
bitmaps*, and turning them back into lines is a restoration rather than a
reinterpretation.

**Half of that survived contact with a board and half did not** (2026-08-29).
The walls and the bolts are lines and the bolts were never anything else — the
ROM plots them pixel by pixel with no pattern data. **The figures are the
cabinet's sprites**, because drawing them as lines cost 2.00 ms a robot against
0.26 stamped and eleven robots was two-thirds of the drawing pass. §7 has the
decision and the trade; read this paragraph as the thesis the measurement
tested rather than as what the port does. The Vectrex port
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

**RESULT, 2026-08-29 — clear-and-redraw wins at eleven robots, and there is no
single answer**
([`p15m0-fast-pico2w-2026-08-29.md`](measurements/p15m0-fast-pico2w-2026-08-29.md),
300 MHz).

| | 4 robots | 11 robots |
|---|---:|---:|
| erase in place — body / present | 52.33 / 4.22 | 110.58 / **8.42** |
| clear + redraw — body / present | 42.43 / 18.75 | 86.70 / **18.98** |
| **frame** | **56.55** vs 61.18 | 119.00 vs **105.68** |

**The verdict inverts with the robot count.** At four robots erase-in-place
*wins* by 4.6 ms; at eleven it loses by 13.3. The crossover is about **six
robots**, and the mechanism is plain in the columns: in-place buys a present
saving with a **fixed ceiling** — ~10 ms, set by the canvas — and pays a second
actor pass that **grows with n**.

So the argument above is wrong twice. It is wrong about the tracker: eleven
robots scattered over five columns and three rows present in **8.42 ms against
a full-canvas 18.98**, less than half, where this section said a present
touching most rows costs about what one touching all of them costs. And it is
wrong to have treated the answer as a constant — "6.8 against 0.8, not close"
compared two numbers that were both wrong and whose *ordering* is not fixed.

The 150 MHz run did not show this: at 150 clear-and-redraw won at both counts.
In-place only comes into its own at 300, because the present does not halve
with the clock and the second drawing pass does.

**Keep it live.** Eleven robots is the gate's case and clear-and-redraw wins
there, so M0's decision stands — but if §17 cuts the robot count, or if drawing
gets cheaper, this has to be re-read rather than cited.

**RE-READ 2026-08-29, and it inverted: with the figures stamped (§7.6),
erase-in-place wins at every count measured**
([`p15m0-stamped-pico2w-2026-08-29.md`](measurements/p15m0-stamped-pico2w-2026-08-29.md)).

| at 11 robots | body | present | frame |
|---|---:|---:|---:|
| pen, clear + redraw | 83.65 | 18.53 | 102.18 |
| stamped, clear + redraw | 66.25 | 18.90 | 85.15 |
| **stamped, erase in place** | **69.90** | **7.53** | **77.43** |

In-place beats clear by 13.9 ms at four robots and 7.7 at eleven, because
in-place's whole cost is a second *drawing* pass and stamping is what removes
drawing. The sentence above — "if drawing gets cheaper, this has to be re-read"
— was written on 2026-08-29 and was overtaken the same day.

**So §3's answer is: clear-and-redraw if the figures are pen strokes,
erase-in-place if they are stamps.** The strategy is not a property of the game;
it is a property of what a drawing pass costs.

**SETTLED 2026-08-29: erase-in-place**, since §7 took the stamped figures. The
harness still builds both, because Q1 is what says so and a harness that
measured only the winner could not.

**M2 built it, and it came with one condition the harness could not have
found** (2026-08-30). In the harness every frame began with `clean`, so the
eraser could spill and the next `clean` took it back. In the game the walls are
drawn **once a room**, so a spilling eraser eats a hole nothing repaints, and
§8.3's contact box became the man's own outline so he cannot stand inside a
wall for the eraser to reach. Erase-in-place on a static maze is cheap
**because** the maze is never redrawn, which is the same sentence as "nothing
repairs it".

**And the first board run said the eraser did not erase** ([B64](bugs.md)).
The fix above was inset by what this section called the cap radius, on the
reading that a round cap *extends* a stroke into a rectangle four pixels past
each end. It does the opposite: `screen_gfx_line` stamps a **filled disc** at
every point of the line, so a stroke is a stadium whose ends **pinch to a
single pixel**, and the reference says so in as many words under
[`setpensize`](../reference/Pico_Logo_Reference.md). Four rows past the end an
8-wide pen is one pixel across, not nine, so **seventeen of the man's 128
pixels were never erased** — his top four rows and bottom three, at the corners
— and he dragged a trail in every direction.

**The eraser is now three strokes of pen 3, which is the one wide pen that is
an exact square** (radius 1.5, extent truncates to 1, and the corner (1, 1) is
2 against 2.25, so all nine pixels of the 3 × 3 are in). At +1, +4 and +6 of the
sprite's left edge, run from one row inside the top to one row inside the
bottom, they cover the 8 × 16 exactly: **no pixel of the man survives and no
pixel outside him is touched**. That is a stronger guarantee than the margin
above — the eraser cannot reach a wall even standing against one — so §8.3's
extra step is now a belt rather than the braces.

**The same eraser is in `tests/logo/p15m0`, which is where §3's numbers come
from** ([B65](bugs.md), open). The in-place body of 69.90 ms at eleven robots
was measured with one pen-8 stroke a figure where a correct eraser is three
pen-3 strokes, so the erase pass is understated and in-place's 7.7 ms margin
over clear-and-redraw is the thing at risk. **M3 owns the re-read**, because M3
is where eleven robots arrive and where §3's verdict is actually spent.

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

**M0 correction, 2026-08-29: it is four *lookups*, and each one is ten
statements.** `IQ` probes four corners, so a robot pays four `cell.at` calls a
frame and `cell.at` is two divides, two `int`s, four clamps and an index — about
40 arithmetic statements a robot a frame for wall probing alone, which is the
largest single item in a logic pass §15.2 costed at 8.3 ms for all eleven and
which measured 113.55. The claim that it cannot tunnel still holds, and it is
still the right mechanism; what is wrong is the price.

**The fix is not to make the lookup cheaper but to stop doing it.** A robot
moves two steps a frame inside a 48 × 68 cell, so it needs re-probing only when
it **crosses a cell boundary** — about one frame in twenty. ~~M1 owns it.~~
**M3 owns it** (corrected 2026-08-30): M1 is the room and there is no robot in
it to re-probe, and a robot only compares the cell it is in against the cell it
was in if it has a previous cell — which is why M0 could not measure the saving
either. What M1 does own is `cell.at` itself, which is the room's whole public
interface from M2 on.

## 7. The figures

**Decided 2026-08-29, after M0: the figures are the cabinet's own sprites.**
This section used to open "the point of the exercise" and state each figure as
a segment count, because §1's thesis was that Berzerk's figures are line
drawings pretending to be bitmaps and that turning them back into lines is a
restoration. M0 priced that at **2.00 ms a robot** against **0.26 ms** for the
same figure stamped, and eleven robots at 22.0 ms was two-thirds of a drawing
pass the design had budgeted at 6.8 ms for everything.

So the trade was taken deliberately and it is worth stating plainly, because it
is the one place this port stops being what §1 said it was:

> **The walls and the bolts are still lines. The figures are the ROM's
> bitmaps.** That is what the cabinet is — `MOVE_AND_DRAW_BOLT` plots its bolts
> pixel by pixel with no pattern data (§7.2), and everything else is a sprite —
> so the port is now *more* faithful and less interesting. §1's restoration
> claim applies to the walls and the bolts, and no longer to the figures.

**`snapsh`, not `putsh`, and the reason is size rather than taste.** `putsh`
takes a 16-byte spec and **doubles every pixel horizontally** on the way in
(`turtle_put_shape_data`), so the cabinet's 8-wide robot would render 16 wide —
twice the width inside a 48-wide cell, which changes dodging and
robot-versus-wall collisions and takes the playfield off §5's 1:1. `snapsh`
captures at the size it is given. So each sprite is rendered once at startup
from its ROM bytes and picked up at 8 × 12, and the rendering costs ~10 ms a
sprite in a place nobody sees.

**Turtles cannot carry the figures**: `MAX_TURTLES` is 8 and a room holds
eleven robots, a man, Otto and seven bolts. `stamp` is the mechanism — one
turtle stamps all eleven.

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

**Read "round" literally, because it cost a board session** ([B64](bugs.md)).
A wide pen is a **filled disc** stamped at every point of the line — the
reference's [`setpensize`](../reference/Pico_Logo_Reference.md) says so — so a
stroke is a stadium and its cap **pinches to a single pixel** rather than
squaring the end off. It spills *sideways of the end point*, not *past it as a
rectangle*: an 8-wide pen is nine pixels across its own stroke and one pixel
across four rows beyond it. Nothing in this game may assume a stroke covers a
rectangle unless it is pen 3, which is the only width whose disc is a full
square (§3).

### 7.2 The bolt needed no translation at all

The BOLT structure comment in the disassembly says it outright:

> *Bolts are programatically plotted pixel-by-pixel on screen, starting from
> the "head" at X,Y and ending at the "tail" at TailX,TailY. They do not have
> pattern data in ROM.*

A Berzerk bolt **is** a line segment with a head, a tail and a direction. It is
already a vector; there was never a sprite to convert. `MaxLength` is **8** for
the player ($1F7C) and **5** for a robot ($2921), and that is the drawn length.

### 7.3 The robot is the ROM's eleven bytes

The arcade sprite is one byte wide and eleven rows tall, and the eleven bytes
are legible as a drawing:

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

The **five pattern tables at $1000–$1030 are exactly the five facing groups**
the `run` dispatch already keyed on, and they are what the five costumes are
built from:

| table | facing | first frame | eye row |
|---|---|---|---|
| $1000 | standing | $10D1 | `66` centred |
| $1013 | right / up-right / down-right | $112C | `78` |
| $101C | down | $1139 (12 rows) | `66`, feet differ |
| $1027 | left / up-left / down-left | $1155 | `1E` |
| $1030 | up | $116F | `7E` |

The eight standing frames at $1000 differ in **the eye row alone** — `66 4E 1E
7E 78 71` — which is two eyes tracking left to right and back. As costumes that
is six more slots if M3 wants the animation, or `setanim` cycling them for
nothing; as sprites it is what the cabinet does.

`$103B` is the four-frame explosion and `$1198`–`$1208` its pixel data. M3's.

### 7.4 The man is $10BF, and his nine shooting sprites were nine slots until `setrot "flip`

```
18 18 00 3C 5A 5A 5A 18 18 18 18 18 18 18 1C 10      $10BF, 8 x 16
```

This subsection used to be called "the man's arms are where the vector form
actually wins", and the argument was good: the arcade carries **nine separate
shooting sprites** ($1309–$1380, indexed through `SR.TAB` at $2067), one per
direction the arms point, and in vector form the arms are one segment drawn at
the firing heading — nine sprites collapsing to one `seth` and one stroke.

**That is exactly what the bitmap form gives up, and it is the honest cost of
§7's decision.** Nine directions are nine costumes, and with five robot facings
that is fourteen of fifteen slots before Otto, the explosion or any robot
animation. §18 carries it as a third ceiling.

M0 caches the standing frame only. M2 owns the other nine and owns the
question of whether they all have to be resident at once — a re-`snapsh` costs
~2.7 ms, which is affordable at a room transition and not in a frame.

**ANSWERED 2026-08-30 at M2, and the answer is `setrot "flip`.** The
re-`snapsh` escape was already dead — §18 re-priced it at ~18 ms and a
four-costume swap at a room transition is two dropped frames — so the working
set had to fit in fifteen, and counted honestly it did not: the man alone is
**sixteen** sprites before a robot is built. Standing ($10BF), a walk cycle of
three that runs up/up-right/right/down-right/down ($104B → $10AD, $109B,
$1089), a *second* walk cycle of three for down-left/left/up-left ($1056 →
$13B5, $13A3, $1391), the eight shooting poses and the death sprite ($12B3).

**The ROM's second walk cycle is the first one mirrored, and so are half the
shooting poses.** $1391, $13A3 and $13B5 are hand-drawn copies of $1089, $109B
and $10AD with the bits reversed — compare them byte for byte and a row or two
differs where the artist's hand did — and the eight shoot sprites are four
right-facing ones and their four mirrors. `setrot "flip` produces exactly that:
the costume mirrors for a turtle facing west, so one slot serves both sides and
the heading is the facing. **It applies to the robot too**, whose walking-left
frame ($1155, eye row `$1E`, feet `$6C`) is precisely its walking-right frame
($112C, eye row `$78`, feet `$36`) reversed — so §7.3's five facing groups are
four slots, not five.

The fifteen then allocate, with two spare:

| slots | what | when |
|---|---|---|
| 1 | the man, standing ($10BF) | M2 |
| 2–4 | the man, walking ($10AD, $109B, $1089) | M2 |
| 5–9 | the man, shooting (four facings and up/down) | M4 |
| 10–13 | the robot, four facings | M3 |
| 14–15 | spare | M5/M7 |

The explosion takes no slot — §7.6's own fallback, the Vectrex's random dots,
is what it gets — and Otto is drawn rather than stamped (§7.5). **M2 builds
four of the fifteen and not nine**: the five shooting poses arrive with the
trigger in M4, because building them at M2 costs 5 × 18 ms of startup for a
pose nothing in the milestone can reach. What M2 owed this section was the
arithmetic, and the table is it.

### 7.5 Otto is still an `arc`, and that is a gap rather than a choice

A circle, two eyes and a grin — four statements, three of them `arc`, and the
reference's own worked example draws precisely this face ([`arc`](../reference/Pico_Logo_Reference.md)).
His bounce is the arcade's: `OttoBounceOffsets` in the Vectrex port
(`00 08 0C 0E 10 12 12 12 14 …`) is a vertical offset walked from a counter, so
he moves in a straight line towards you and his *drawing* hops.

**The arcade's Otto is a sprite and this document does not have it.** His
pattern table is at `$120B` and his frames run from about `$122E`, and the
disassembly renders that whole region as Z80 instructions rather than data —
the bytes are there (`01 02 18 18`, `01 03 10 38 10`, …, a ball that grows and
shrinks, which *is* the bounce) but transcribing them out of a mis-decoded
listing is error-prone and it is M5's job, not M0's. Until then he is drawn,
not stamped, and he is the one figure in the game that still is.

### 7.6 The two deaths

**A robot** comes apart into a cloud. The arcade's four explosion frames are at
$103B with their pixel data at $1198–$1208, and they are four more costume
slots (§18) — or, if the slots are not there, the Vectrex's answer of random
dots from the death point, which is six short strokes a frame for four frames
and costs no slot at all. M3 decides against the slot budget rather than on
looks.

**The player** is *electrocuted*, which is a different thing and the arcade
gives it its own sprite ($12B3) and a 45-tick pause ($1FB6). The Vectrex wobbles
the man's scale through `PlayerFriedScales` — `08 07 06 05 05 05 06 07 08 09 08
07 06 05 06 07`, sixteen frames — and that is the version to copy, because a
figure that grows and shrinks reads as electricity where random limbs read as
noise. The pause is the arcade's 45 ticks.

~~**No sprites, no costumes, no `stamp`**~~ — **reversed 2026-08-29, see the top
of §7.** The figures are costumes stamped from the ROM's bitmaps; the walls and
the bolts are pen strokes; Otto is a pen `arc` until his sprite is transcribed
(§7.5).

**`dot` is still excluded, and that rule did not come from the vector thesis.**
It takes a *list*, a list is a cons, and a frame loop must not allocate — M0's
first board run died of exactly that (§19, B52). Nothing in a frame draws a dot.

**M0 has put a price on that sentence, and it is the largest single item in the
budget** (2026-08-29). A robot costs **2.01 ms to draw** at 300 MHz — ~45
interpreted statements for eleven strokes — and eleven of them is 22.11 ms of a
31.98 ms drawing pass. `tests/logo/p15m0` now measures the alternative beside
it, and §22 Q6 is the decision:

- **Turtles cannot carry the robots.** `MAX_TURTLES` is 8 and the room holds
  eleven robots, a man, Otto and seven bolts. One turtle per robot is not
  close. A turtle suits the *man* and Otto exactly, and `setrot "full` would
  make one costume follow the heading — which is §7.4's collapse.
- **`stamp` can**, because it copies the selected turtle's shape into the
  picture: one turtle stamps all eleven, at five statements each.
- **`snapsh` and not `putsh`.** A mono `putsh` bitmap renders at 16 × 16 — the
  rows are doubled from the user's eight bits — which is twice the cabinet's
  8 × 11, breaks §5's 1:1 and doubles the dirty rectangle. A `snapsh` costume
  renders at its own size.

**And `snapsh` is why this need not be a reversal.** It captures what is
already on the screen, so the five `rob.*` procedures stay exactly as they are
— still the artwork, still the ROM restored to lines — and run **once at
startup** rather than eleven times a frame. That is a render cache, not a
reinterpretation, and the sentence it costs is "every mark is one turtle with a
pen" rather than "the figures are line drawings".

The one thing it genuinely gives up: **erasing a stamp is not a pen colour.** A
costume carries its own pixels and `snapsh` cannot capture an eraser (background
pixels become transparent by design), so erase-in-place needs a wide background
stroke over each figure. §7.1's round-cap warning applies and is harmless for an
eraser — spilling means erasing slightly more background — but it would not be
harmless for ink.

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

**REVISED 2026-08-30 at M2: the wall box is the man's own outline, grown one
step on every side — ±5 in x and ±9 in y about his centre, which is his 8 × 16
sprite plus a margin.** Two reasons, and the first is the one above taken
further. A square box is an approximation of a figure twice as tall as it is
wide, and Malban's comment says the smaller radius *was a bug*; the cabinet's
answer is not a box at all but a pixel intercept, and a pixel intercept on an
8 × 16 sprite is exactly ±4 by ±8. The second reason is erase-in-place. §3's
strategy rubs the man out with a background stroke that covers his outline and
nothing else, and the walls are drawn **once a room** — so a man who can stand
four steps inside a horizontal wall is a man who eats a hole in it that nothing
puts back until the next doorway. The extra step on each side is the eraser's
margin: he stops with his outline one step clear of the ink.

**AND THE TEST IS OCCUPANCY, NOT CROSSING** — corrected 2026-08-30 after a
board found the hole ([B66](bugs.md)). §6.3's own phrasing, "which cell am I
in, is the edge I am crossing walled", describes a **crossing** test, and a
crossing test only fires when the man crosses the boundary the wall lies on.
Walk *parallel* to a wall and step into the cell at its end and you cross
nothing: a vertical wall covering row 0 but not row 1 can be walked onto from
below, and a horizontal wall covering one column from the side. The cabinet has
no such hole because it has no cells — a wall pixel under a man pixel raises
the intercept bit, and the only question it asks is **may he be here**.

So that is the question. A cell is 48 × 68 and his box is 10 × 18, so the box
lies across **at most one column boundary and at most one row boundary**: take
the box's four corners, and two corners in different cells means the box lies
across that boundary, with the first corner's mask saying whether a wall is
drawn along it. **Four `cell.at` calls**, against the eight a two-axis crossing
test made — and the per-axis move collapses into one test, because walls kill
and there was never anything to slide along.

**§6.3's sentence should be read as the arcade's `WALLINDEX` lookup and not as
a movement rule**, which is the correction: the table is right, the four
statements are right, and *what they are asked about* was wrong.

**And the player never consults the border bits, which is a second M2
decision.** `cell.at` clamps to the 5 × 3 grid, so a crossing test can never
see the outer wall — every point left of the playfield is in column 0 and no
crossing happens. That is right rather than a gap: the mask table carries the
border walls **at all four doorways** (§6.3), because it is what robots consult
and robots never leave the room, so reading it for the player would wall him
into a room with four visible doors. So the player meets the border the way the
cabinet's pixels meet it — a position test against the playfield rectangle,
with the doorway as the gap in it — and his whole outline has to be inside the
gap before he passes, which is what stops him clipping the stub.

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

**What ships in v1: the voice and the text.** This section was written to ship
text alone, because §14.3's speech engine did not exist; it does now
([say-design.md](say-design.md), done 2026-08-29), so v1 speaks each sentence
with `say` **and** captions it on split-screen line 26, centred, for about a
second — which is exactly what the Vectrex port does (`HumanoidString`, "GOT
YOU HUMANOID"). The caption is not a leftover: the arcade's speech is famously
hard to make out, and a player who cannot hear the room should still be told
they are a chicken. Either way the sentence *assembly* is preserved, which is
the part that makes the taunts feel alive.

The vocabulary is **one property list, not thirty procedures** — §18's
procedure ceiling is why — and a sentence is a list of word names:

```logo
to speak :words
  if empty? :words [stop]
  sayphonemes gprop "ph first :words
  speak butfirst :words
end
```

`GENERATE_ROBOT_SPEECH` ($2B97) is then `speak` over two `pick`s.
[say-design.md](say-design.md) §11 is the long form, including why the words
are stored as phoneme lists rather than as text.

### 14.3 Speech in the interpreter, priced

**Settled: B was taken.** [say-design.md](say-design.md) is the design and it
was built and gated between 2026-08-28 and 2026-08-29 — `say`, `phonemes`,
`sayphonemes`, `speaking?`, `setvoice`, `voice`, over a 41-phoneme formant
engine and NRL Report 7948's letter-to-sound rules, as a ninth source in the
PSG's refill IRQ. Two numbers below came out wrong and are worth carrying: the
flash cost is ~20 KB rather than ~10 KB (the rule table is most of the
difference), and `SAY` is **Terrapin Logo**'s, not Apple Logo's — Apple Logo II
is silent. The rest of this section is the reasoning that chose B, kept.

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
block on it. That held: `say` landed before this game was started, so §14.2's
five sentences are five calls and the on-screen text stays as a caption, which
is the outcome this paragraph named. The sentence-assembly code was written
once either way.

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
| present, `splitscreen` (240 rows) | 19.62 ms | **18.70 ms** |
| present, `fullscreen` | 26.45 ms | not measured |

**Corrected 2026-08-29, and the correction is the first thing M0 found.** This
row used to read "19.2 → **~14.0**" for the split present and "26.45 → 18.70"
for full screen, with a derivation under it explaining how 14.0 came out of the
full-screen pair. **18.70 is P13 M0's measured *splitscreen* at 300 MHz**, not
its full-screen figure — [§12.3](battlezone-design.md#L925) says so in words:
*"the present itself moves **19.62 → 18.70 ms**"*. It was copied into the wrong
row, and a splitscreen-at-300 was then derived from a pair that never existed.

So the present was **already measured on a board** and this document budgeted
three-quarters of it. The present does not need deriving and never did; it
barely moves with the clock, because it is the SPI wire. M0 read **21.95 at
150** on a Berzerk room — above P13's 19.62 because a room is drawn edge to edge
and Battlezone's scene is not — with full screen at **26.45**, matching
Battlezone exactly, so the instrument was fine and the table was not.

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

**Rebuilt to the ROM bitmaps 2026-08-29 and re-read at 150 MHz: the frame
projects to 78.60 ms, against 77.43 measured directly at 300 with the
vector-sourced costumes** ([`p15m0-bitmap-normal-2026-08-29.md`](measurements/p15m0-bitmap-normal-2026-08-29.md)).
The costumes are the same size either way, so the blit is the same and the
1.5 % is scaling error — **changing the artwork source cost nothing**, which is
worth having confirmed rather than assumed. Eleven robots stamped is **6.38 ms
against §15.2's 4.6**, the first line in that table this port has come in near.
**The logic is unchanged and is now 74 % of the frame**, which is where §15.4
points.

**GATE READING FOR THE PORT AS IT NOW STANDS — Pico 2 W at 300 MHz,
2026-08-29: 77.35 ms against 50, and M0's six questions are all answered.**
([`p15m0-bitmap-fast-pico2w-2026-08-29.md`](measurements/p15m0-bitmap-fast-pico2w-2026-08-29.md).)

    300 MHz:  frame = 16.61 + 5.52 n     n=11: 77.35 mean, 80 worst

Down from 106.30 on the pen path, and the **third independent reading of the
same number** — 77.43 with vector-sourced costumes, 78.60 projected from
150 MHz, 77.35 measured here — so the design is stable under measurement.

| | ms | share |
|---|---:|---:|
| logic | 55.25 | **71 %** |
| drawing (erase + stamp) | 15.02 | 19 % |
| present, in place | 7.03 | 9 % |

**§15.2's drawing budget is beaten for the first time**: eleven robots cost
**2.97 ms against a predicted 4.6**, one robot 0.27. The remaining 15.02 is the
walls, the man, Otto's two `arc`s, seven bolts and the erase pass.

**The three identified logic savings project to ~59 ms, and ~55 with Otto
cached and the bolts gated.** At the resulting ~3.87 ms a robot and a ~14 ms
floor, the choice is **nine robots at 20 fps or eleven at 18** — and all three
savings are things M3 implements anyway, since a robot only re-probes on a cell
crossing if it has a previous cell to compare against. **They cannot be done in
a harness.** §22 Q2.

---

The reading below is the pen design, kept because §15.2's misses are read
against it.

**RESULT, 2026-08-29 — MEASURED ON A PICO 2 W AT 300 MHz: THE GATE FAILS.**
([`p15m0-fast-pico2w-2026-08-29.md`](measurements/p15m0-fast-pico2w-2026-08-29.md),
with the 150 MHz companion beside it.)

    300 MHz:  frame = 35.13 + 6.44 n     n=11: 106.0 mean, 109 worst

**106.0 ms against 50** — 9.4 fps where this section asked for 20, and **2.8×
the §15.2 estimate**. (Confirmed on a second run at 106.30, and see the
stamped result below, which brings it to **77.43**.) The 150 MHz run projected 105.8 by P13's measured 2.059,
so the ratio transfers to a Pico 2 W unchanged and the scaling can be trusted
for whatever question comes next.

| | §15.2 | measured | factor |
|---|---:|---:|---:|
| logic | 17.1 ms | 55.28 | 3.2× |
| drawing | 6.8 | 31.98 | 4.7× |
| present | 14.0 | 18.87 | 1.3× |
| **frame** | **38** | **106.0** | **2.8×** |

**The diagnosis is exact, and it is not the one this document would have
guessed.** Every unit §15.1 tabled was right — an arithmetic statement measured
**24 µs against 24 predicted**, a bare `repeat` iteration 2.5 against 2.4, the
present 18.85 against P13's 18.70. So the whole 2.8× is a **counting** error:
§15.2 budgeted 15 statements to draw a robot and it takes ~45, and ~30 to think
one when it takes ~180.

**The budget allows 2.3 robots**: `(50 − 18.87 present − 16.26 body floor) /
6.44`. To seat eleven, the per-robot cost has to fall from 6.44 ms to **1.35 —
4.8×** — and the four design-level savings that are actually identified
(§15.4's cell-crossing note, a leaner model, gating `fires?`, gating the pair
loop on live bolts) come to about 2.7 of that between them. On the pen path this
is not a tuning problem.

**REVISED THE SAME DAY BY Q7 — the gate still fails, but at 1.55× rather than
2.13×.** With the figures stamped (§7.6, §22 Q6) and erased in place, the frame
is **77.43 ms**:

    stamped in-place @ 11:  77.43 ms = logic 55.50 + draw/erase 14.40 + present 7.53

Drawing falls from 32.02 ms to 14.40 *including* the eraser, so the item that
was 4.7× low stops being the problem and **the logic is now 72 % of the
frame**. The three identified logic savings take the slope from 4.46 to ~2.82
ms a robot and the frame to **~59 ms**; caching Otto and gating the bolts should
reach **~55**.

**So the projection is 55–59 ms against a 50 ms gate — 17–18 fps.** Two things
close it, and both are scope rather than engineering: **nine robots instead of
eleven** (−5.6 ms at the reduced slope), or **restating the gate at 55 ms / 18
fps**, which this section chose as 50/20 by argument rather than measurement.
§22 Q2 and Q6.

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

**RESULT, 2026-08-29: both were wrong, and neither was the problem.** The pair
loop is 1.8× low (7.24 ms against 4.0 at 300 MHz — the smallest miss on the
page) and the model dispatch was avoided
successfully — `time.robot` reads 2.01 ms and the series' drawing slope is
2.02, so the `run`-over-five-names dispatch costs nothing measurable. That is
the one prediction in this section that held, and it held exactly. The number nobody flagged is what
broke the budget: **`iq` calls `cell.at` four times per robot per frame, and
`cell.at` is ~10 arithmetic statements** (two divides, two `int`s, four
clamps). That is ~40 statements a robot a frame for wall probing alone, and
§6.3 sold the cell lookup as *"four statements, no pixels"* — it is four
*lookups*, and each of them is ten statements. A robot costs **9.1 ms to think**
and **2.01 ms to draw** at 300 MHz (9.1 and 4.1 at 150).

The cheapest fix available is not in this section at all: a robot only needs to
re-probe the walls when it **crosses a cell boundary**, not every frame, and it
moves two steps a frame inside a 48 × 68 cell. M1 owns that.

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

  **M0 has now found the half of that argument this entry did not anticipate,
  and it is about memory rather than speed.** `.setitem` of a *number* interns
  it, so writing a value the workspace has not held before costs word-table
  bytes — the resource that actually runs out, since
  [B25](bugs.md) died with 21,000 free nodes and 20 free bytes of word table.
  A slot holding a bounded counter settles after one pass through its range; a
  slot holding an ever-growing one never does. Berzerk's robot timers are the
  case in point, and the fix is to wrap them at 60 as the cabinet's own 60 Hz
  `TIME` does. **An array of numbers with true slot assignment would not have
  this property at all**, which is a better argument for L2 than the ~0.4 ms —
  but it is still not a blocker, because bounding a counter costs one
  `modulo`.
- **A `say` primitive (§14.3).** The one genuinely new thing this game asked
  for, and it is a language feature with its own justification. **Shipped
  2026-08-29**, before this game was started, so it is a dependency that is
  already met. It buys the frame **nothing** and the game a great deal, and at
  ~0.25 ms to translate a sentence ([say-design.md](say-design.md) §9.3) a
  spoken taunt is affordable inside the frame loop.

~~**Verdict: Berzerk needs no interpreter change to ship.**~~ **Overturned by
M0, 2026-08-29.** The reasoning above is sound line by line — there is no
trigonometry, arrays really do buy only ~0.4 ms, and `say` really is off the
critical path — and the conclusion is wrong anyway, because **the cost is not in
any one primitive**. It is in the number of statements: a robot costs ~45
statements to draw and ~190 to think, and eleven of them is the budget twice
over. A lever that makes one primitive faster cannot reach that; only fewer
statements can, whether by writing fewer (§17, and §15.4's cell-crossing note)
or by a primitive that replaces many at once (Battlezone's L4 `drawmodel`
family).

**And the frame is 90 % interpretation** — body 181 ms against a present of 19.
§4 spent a section on the split screen being "worth about 4.7 ms" and §15.1
spent a derivation on the present; both were optimising the 10 %.

**Q7 then found the change that was available and this section did not
consider**: not a faster primitive but *fewer statements*, by running the
figures' pen models once at startup and stamping the result (§7.6). That is
19.1 ms of the 32.02 ms drawing pass, and it needs no interpreter change at
all — so the verdict this section reached is right in its conclusion and wrong
in its reasoning. What Berzerk needed was not a lever but a cache.

After it, **drawing is solved and the logic is 72 % of the frame**, and the
logic's largest item is §6.3's cell probe (§15.4). The interpreter question
comes back only if that work lands and the frame is still over.

M0 did price `.setitem`, which is what this section asked for: **33 µs on an
eleven-element list at 300 MHz, four times `item`'s 8 µs** and 1.4× an
arithmetic statement. At ~40 a frame that is 1.3 ms. Real, and still not the
lever — and note it is the *write* that is dear, so the L2 case rests on
mutation rather than on indexing, exactly as this section guessed.

## 17. Reduced-resource choices

Cut from the arcade, with the reason:

| Cut | Reason |
|---|---|
| **Two-player alternating** | The 2600 manual's own choice, and the second score, the second `MAN_PTR` and the player-swap path are a third of the state for none of the game |
| **The screen scroll between rooms** | `SCROLL_UP`/`DOWN`/`LEFT`/`RIGHT` ($2175–$2419) slide the whole bitmap. Here that is a full-screen present per step — eight steps is 112 ms — and the room has to be drawn twice during it. Reconsidered at M7 as a four-step slide if the budget allows; a cut to `clean` and rebuild otherwise |
| **Colour** | The cabinet colours the walls per difficulty out of the table's fifth column and the man per player. White on black, like Asteroids and Battlezone |
| **The 2600's twelve game variations** | Rebound Otto, invincible Otto, no Otto, non-shooting robots — all 2600 inventions. The arcade has one Otto and he is invincible |
| ~~**Speech**~~ | **Uncut 2026-08-29.** `say` landed ([say-design.md](say-design.md)) before this game was written, so §14.2 speaks *and* captions |
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

**And a third the bitmap decision introduced (§7): `COSTUME_SLOTS` is 15, and
the cabinet has far more sprites than that.** Five robot facings, six eye-row
animation frames, nine of the man's shooting directions, four explosion frames
and Otto's bounce are well over fifteen before anything else. A vector model
was a *procedure* and cost flash, so this document could carry as many as it
liked; a costume is one of fifteen slots. The pool itself is not the
constraint — 8,192 bytes against about 1,300 for fifteen 8 × 12 sprites — the
**slot count** is.

The escape was going to be that a re-`snapsh` is cheap, so the fifteen would be
a *per-room* working set rather than a global inventory. **Measured 2026-08-29,
that escape is 7× more expensive than it was written as: ~18 ms a costume at
300 MHz, not ~2.7.** The ~2.7 came from the vector-sourced cache (16 ms for six
pen models); rendering a bitmap pixel by pixel is ~480 statements a sprite
against a pen walk's ~45, and the whole six now cost **229 ms at 150 / ~111 at
300**.

At startup that is invisible — once, behind an attract screen. At a room
transition, swapping four costumes is 72 ms on top of ~~the room generation's
own 7.4~~ **a room change measured at 30 ms** (M1, 2026-08-30, `fast`), so
**~102 ms — two dropped frames at 20 fps rather than a hitch**, which is a
worse answer than this paragraph was written with. So either **the working set fits in
fifteen**, or the renderer gets cheaper: drawing each row as *runs* of set bits
rather than eight pixel steps is roughly 3×, and is the obvious lever if it is
ever needed. It is the one ceiling this design acquired rather than inherited.

**CLOSED 2026-08-30 at M2: it fits in fifteen, with two spare, and the whole
saving is `setrot "flip`.** Counted straight the inventory is twenty-one and
the swap was the only way out. But the ROM's left-facing frames are its
right-facing frames mirrored — for the man's second walk cycle, for four of his
eight shooting poses, and for the robot walking left — and `setrot "flip` is
exactly that mirror, done by the engine at stamp time for no slot and no
statement. §7.4 carries the allocation: 1 standing, 3 walking, 5 shooting, 4
robot facings, 2 spare. The explosion takes §7.6's random dots rather than four
slots and Otto is drawn (§7.5). **M2 builds four of them**; M3 and M4 build
theirs when they need them.

**And a fourth, which M0 found on a board rather than in a budget: a frame must
not allocate.** Nothing in this interpreter collects on demand — `alloc_cell`
and `mem_atom` report out of space rather than collecting and retrying — so a
frame loop that spends storage has a fuse on it, and the two ways to light one
are both easy to write without noticing. `dot`, `setpos [x y]`, `list` and
`se` all cons a cell; `.setitem` of a number the workspace has not held before
interns a word. M0's harness did both and died with `out of space in rob.left`
on its first board run. The game's rule is therefore: **the eyes are strokes,
every counter written into a list is bounded, and `alloc.per.frame` reports
both `nodes` and `atoms` — warm — with zero as the only acceptable reading.**
The shipped games' `reclaim` floor ([B25](bugs.md)) is the fallback if a frame
ever has to spend, not the plan.

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
| **M0** | The measurement harness | **DONE 2026-08-29 — all six questions answered on a Pico 2 W, gate FAILED at 77.35 ms against 50** (106.30 before §7's figures changed). Erase strategy decided: **erase-in-place**, once the figures are stamps. §15.3, and the decision it leaves is §22 Q2 |
| M1 | The room | **DONE 2026-08-30.** The maze is a function of `(room.x, room.y)`; walk out and back and it is the same room. Read on a board at both clocks: a room change is **30 ms at `fast`** (worst 33) and 44 at `normal`, which is M0's parts to 3 %. §19's M1 note |
| M2 | The man | **DONE 2026-08-30.** Walls kill, doors work, and the eight directions read right on the keyboard. 35 procedures, 40 host tests, `ctest` green at 84/84, and read on a board four times: the frame fits at both clocks, and [B64](bugs.md) (the eraser) and [B66](bugs.md) (the end of a wall) came back from it. The milestone's own decision was §7.4's: `setrot "flip`, which takes the cabinet's inventory from twenty-one costumes to thirteen of fifteen. §19's M2 note |
| M3 | The robots | Seek, `IQ`, the count cycle, robots killing robots |
| M4 | The bolts | Both bolt kinds, the three firing windows, the alignment rule confirmed against MAME (§10.2) |
| M5 | Evil Otto | The timer arithmetic, walls ignored, robots eaten |
| M6 | The campaign, the sound and the voice | Both difficulty tables, lives, bonus, the attract screen, the effects off the ROM, and §14.2's four fixed sentences spoken with `say` and captioned |
| M7 | Polish | The taunts (spoken, over two `pick`s), the deaths, the play test, and the scroll if it fits |

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
6. At 150 MHz **and** 300, ~~on all three boards~~ **— on one, decided
   2026-08-29.** The frame is 71 % interpretation and interpretation speed is a
   property of the RP2350 core, which all three carry identically; the two
   hardware differences are the radio, which this game never touches (§1), and
   PSRAM, which nothing in a frame reaches — the costume pool, the node pool
   and the canvas are all SRAM. The present is the SPI wire to the same panel.
   So a second board would re-read the same numbers.

   **What one board does not settle is §18's ceilings**, which are per-board
   heap rather than per-frame time (B44: 56,644 / 40,832 / 47,804 bytes) and
   decide whether the game *loads*, not what it costs. That is an M6 check
   against a game that exists, not an M0 measurement. And **§15.5's clock
   precondition is a runtime matter, not a measured one** — the game reads
   `hw.cpu` back and says so on the attract screen (§21 risk 6), which is the
   answer for any board that declines the overclock, measured or not.

7. **The stamped alternative** (added 2026-08-29, after the gate failed):
   what a robot costs as a `snapsh` costume placed with `stamp` against the
   2.01 ms the pen model measured, both erase strategies again with the figures
   stamped, and what building the costumes costs once at startup. §7.6 and
   §22 Q6.

Its Logo-side correctness — that the generator reproduces a room, that the
eight intersections land where §6.2 says, that `IQ` clears the right bits, that
a stamped robot lands where the pen model draws it — is `tests/test_p15m0.c` on
the host, the way `test_p13m0.c` proved P13's harness was worth carrying to a
board.

#### What was built, and what the first board run said (2026-08-29)

`tests/logo/p15m0` — 54 procedures, inside §18's 100 — carries a real room
(§6's LCG, the eight intersections, the fifteen masks), the wall drawer of
§7.1, five robot models reached through §15.4's `run` dispatch, the man, Otto,
seven bolts, and a logic pass that is `seek`, `IQ`, the move, the wall lookup,
the fire test and the pair loop. `tests/test_p15m0.c` is 32 host tests.
Three things came out of building it, and each is a correction to this
document rather than to the code:

**The harness has to hold the scene completely still, and that costs more than
P11 M0's did.** P11's rocks never moved, so erase-in-place could erase where a
figure *is* and be erasing where it *was*. Berzerk's frame moves three things
by construction — the bolt advance, Otto's bounce counter, and the robot
directions `seek` writes on the first pass — and any of them moving between the
erase pass and the draw pass leaves residue the erase cannot take back. The
residue widens the tile-row spans, which inflates the in-place *present*, which
biases Q1 **against the strategy §3 already predicts will lose**. So every
position in the timing path is computed and thrown away, `p15.moved`,
`p15.probed` and `p15.hits` count the work so the report can show the logic was
not skipped, and three host tests exist for nothing else. The cost is nineteen
`.setitem` calls a frame that the body figure does not carry — which is
precisely why item 5 above prices `.setitem` on its own, so they can be added
back by hand.

**§15.2's per-robot drawing figure is going to be low, and the harness says by
how much.** The table budgets a robot at "place + 9 seg + 2 dots + dispatch",
about fifteen statements. `rob.still` draws those nine segments in about
thirty, because three pen-ups and eleven turns are statements too. The report
therefore measures one robot on its own as well as inside the series, so the
slope and the per-figure cost can be checked against each other rather than
inferred from one number. This is [Asteroids §12](asteroids-design.md#L1387)'s
72 % miss in the same place, and it is the correction most likely to come back
off the board.

**The first board run died rather than reporting, and the cause was storage
rather than time.** `p15m0` came back `out of space in rob.left`. Two things in
the harness were spending, and only one of them looks like spending:

- **The robots' eyes were `dot`s, and `dot` takes a list.** Two cells a robot a
  frame, 24 a drawing pass, 48 in an erase-in-place frame — and the file had
  already written down that the fallback was "two one-step strokes, which
  allocate nothing and cost four more statements", so the board turned a note
  into the design. §7.3 and §7.6 now say so.
- **`.setitem` of a number the workspace has not held before interns it**, and
  `r.time` was an unbounded counter written eleven times a frame. Writing the
  *same* value back costs nothing, so bounding the counter fixes it — and the
  cabinet's own `TIME` is bounded, so wrapping it at 60 is the more faithful
  port as well as the one that does not leak.

**The resource that runs out is the word table, not the node pool**, which is
[B25](bugs.md)'s finding restated: that frame loop died with 21,000 free nodes
and 20 free bytes of word table. A harness watching `nodes` alone watches the
half that is fine. `alloc.per.frame` now reads both, **warm** — the timers
intern their 60 values once, so a cold measurement charges the frame ~300 bytes
it does not actually spend — and the report leads with it, because a run that
cannot finish has no other numbers in it. Measured: 400 warm frames spend zero
cells and zero word bytes, indefinitely. The reference sentence that says
`.setitem` "allocates nothing" is logged and corrected as B52.

**The models did not fit the sprite, and the segment counts hid it.** Writing
the five walks, I lost track of the headings: they drew a 10 × 7 blob with the
right number of segments in it, and every count-based assertion passed. §3's
question is what the dirty-tile tracker charges for eleven scattered figures,
and the tracker keeps one inclusive span per 16-pixel tile row — so a model of
the wrong size measures the wrong thing however many segments it has. All five
now fill exactly the cabinet's 8 × 11 box at 1:1, anchored at the sprite's
top-left corner, and `test_every_robot_model_fits_the_8_by_11_sprite` checks
the bounding box across the whole eye cycle rather than counting anything.

**A `;` inside a `pr` list literal silently eats the rest of the report.** The
semicolon is a comment marker wherever it appears, brackets included, so one of
them in a report line swallowed the remaining twenty lines into that line's
list — and the script still ran, still wrote its file, and still contained
every substring an obvious test would look for. The guard is a maximum line
length on the written report, which is the shape the failure actually has.

### M1 — the room, and the decision it had to take (2026-08-30)

`logo/games/berzerk` — **14 procedures** against §18's budget of 100 — is the
room and nothing else: §6's LCG, the eight intersections, the fifteen masks,
§7.1's one-circuit border and its eight interior runs, `cell.at`, and a walk
from room to room. There is no man, no robot and no bolt; those are M2, M3 and
M4. `tests/test_berzerk.c` is **17 host tests** and `ctest` is green at 83/83
suites.

**The gate is a claim about a pure function, so it is put on the screen as
numbers.** "Walk out and back and it is the same room" is something a person
closes with their fingers, and what they see is a maze that *looks* the same —
eight segments have few enough shapes that two rooms often do. So the fifteen
wall masks sit under the picture in §6.3's own 5 × 3 layout, and walking out
and back is a **reading** when the fifteen numbers come back identical and an
impression when only the drawing does. The masks earn the three text rows
twice over: they are what M3's robots consult, so a room whose picture and
table disagree is a robot walking through a wall you can see.

**The one test this milestone exists for is that the picture and the table
agree**, and it is the check §6.2 says is worth writing out. The two are
computed down *different paths* from the same `rand & 3` — one produces a start
point, a heading and a length, the other sets bits into a 5-wide array through
`ix+0/ix+1/ix+5/ix+6` — and §5 warns that the disassembly's two coordinate
conventions are named backwards with respect to each other. Each half is
self-consistent under any test that checks only that half. So
`test_the_drawn_walls_agree_with_the_wall_masks` stands ten steps off both
sides of every drawn segment in sixteen rooms and asks `cell.at` what it finds:
a horizontal run must be the BOTTOM of the cell above and the TOP of the cell
below, a vertical run the RIGHT of the cell left and the LEFT of the cell
right. Verified to fail on a mutation in **each** path — the bits swapped, and
the segment's `y` negated.

**It checks each segment's MIDDLE, and that is not the whole property**
([B67](bugs.md), found on a board 2026-08-30). A run of the right shape in the
right place that simply *stops short* agrees with the table at its midpoint and
still leaves a gap at its end — which is what the leftmost column's horizontal
runs did, because that column is 52 steps wide and the ROM's run is 48.
`test_every_wall_runs_from_one_grid_line_to_the_next` states the missing half:
a wall runs from one grid line to the next, **both ends**, over sixteen rooms.

**`readchar`, not `pollkeys` — for this milestone only.** §8.2 puts the game on
key *state* and is right about the game: a man who walks while an arrow is held
needs it, and M2 brings it. M1 is turn based, one press is one room, and key
state read in a loop with nothing else to do reports the same tap three or four
times — which is the defect the Snake Temple lost a milestone to, and here it
would walk you three rooms from one press.

**Q3 is answered** (§22), because M1 is where the room coordinates live: they
wrap at 256, and the reason is that the seed is one number rather than two.

#### The board reading (2026-08-30)

**30 ms a room change at `fast`, worst 33; 44 ms at `normal`, worst 53.** M0's
parts predicted ~29 ms at 300 MHz (7.37 to generate + 3.34 to draw + a
full-canvas splitscreen present) and the transition measures **30**, so it is
exactly the sum of its measured pieces and M0's figures transfer to a game.

**The pair of clocks is the more useful half, and it corrects what was written
above.** A room change is not "roughly double at 150 MHz" — it is **1.47×** —
because it is the one cost in this game that is mostly **not** interpretation.
Split it on P13 M0's measured 2.059 interpretation ratio, `I + W = 30` and
`2.059·I + W = 44`, and it comes apart as **13.2 ms of CPU and 16.8 ms of SPI
wire**. Both halves check out against numbers measured separately: 13.2 against
M0's 10.71 ms of Logo plus the line rasteriser and the `clean`, and 16.8
against M0's directly measured **18.60 ms** full-canvas splitscreen present.
So **more than half a room change is the panel**, no interpreter lever in §16
reaches it, and the only thing that would make it cheaper is presenting less of
the canvas — which a room change, where every pixel does change, is the one
place in this game that cannot.

**The spread is CPU-side**: +3 ms over the mean at `fast` and +9 at `normal`,
so it narrows with the clock and is not the wire. The likely cause is in the
generator's own output — the eight interior segments are 48 steps or 68
depending on `rand & 3`, so a room of verticals draws about 40 % more wall than
a room of horizontals, and that is rasteriser time.

**Two consequences the later milestones should have.** A transition's worst
case is **33 ms against §15.3's 50 ms frame**, so from M2 the man can walk
through a doorway and the next room can be built *inside the frame he walks in*
with ~17 ms left for the frame's own work — the build does not need spreading
over two frames. And §18's costume escape now has its arithmetic: four
`snapsh`es at ~18 ms on top of a 30 ms transition is **~102 ms**, two dropped
frames rather than a hitch, which is the argument for a per-game working set
that fits in the fifteen slots rather than a per-room one that is swapped.

M1 does not ask for the overclock — §15.5's precondition is about a frame loop
and the frame loop arrives with M2 — so both readings above are of a game that
never touched `hw.cpu`.

### M2 — the man, and the ceiling it had to take (2026-08-30)

`logo/games/berzerk` — **35 procedures** against §18's budget of 100 — is the
room with a man in it: a `sync`-paced 20 fps frame loop, `pollkeys`/`keydown?`
on the eight directions, the arcade's stand-and-aim control, walls that kill,
four doorways that do not, and a death that holds for the ROM's 45 ticks.
`tests/test_berzerk.c` is **40 host tests** and `ctest` is green at 84/84.
There is no robot, no bolt and no Otto; those are M3, M4 and M5.

**The milestone's real work was §7.4's question, and the answer changed the
whole slot budget.** M2 was asked whether the man's nine shooting sprites have
to be resident at once. Counted honestly the question is worse than it was
posed: the man is *sixteen* sprites, because the ROM has two walk cycles of
three, one for each side. Twenty-one with the robot's five facings, against
fifteen slots, and §18's re-priced escape (~18 ms a re-`snapsh`, four at a
transition is two dropped frames) was already gone.

**`setrot "flip` is the answer and it costs nothing.** The ROM's own
left-facing frames are its right-facing frames with the bits reversed — $1391,
$13A3 and $13B5 against $1089, $109B and $10AD; four of the eight shoot poses
against the other four; the robot's $1155 against $112C, eye row and feet both
— so the engine can make the mirror at stamp time and the heading becomes the
facing. Twenty-one becomes thirteen, two slots spare, and §18's third ceiling
is closed rather than deferred to M6. **M2 builds four of the thirteen**;
speculative costumes for a trigger that does not exist yet would be 90 ms of
startup for nothing.

**Two smaller decisions, and both were forced by erase-in-place rather than
chosen.** §3 settled the strategy at M0 with a harness that began every frame
with `clean`; a game does not. The walls are drawn **once a room**, so the
eraser's round caps — §7.1's own warning, four pixels past each end of an
8-wide stroke — eat holes that nothing repaints. The stroke is now inset by the
cap radius so it covers the sprite's box exactly, and §8.3's contact box became
**the man's own outline grown one step**, ±5 by ±9 rather than the Vectrex's
±4, so he cannot stand inside a wall for the eraser to reach. The second is
better fidelity as well as a fix: a pixel intercept on an 8 × 16 sprite is
±4 by ±8, and Malban's comment beside `PlayerContactBounds2` says the square
box was a bug. `test_the_mans_outline_never_touches_a_wall` is the invariant,
driven 480 frames through rooms against the segments the game actually **drew**
rather than against the table — and that distinction is the doorways, where his
box passes the border's coordinates and must.

**The player does not read the border bits at all**, which §6.3 half-predicted
and M2 had to make true. `cell.at` clamps, so a crossing test cannot see the
outer wall; and it should not, because the mask table carries border walls at
all four doorways (it is what robots consult, and robots never leave). So the
border is a position test against the playfield rectangle with the doorway as
its gap, and the exits are $2157's own four lines transferred through §5.

#### The first board reading, and the trail (2026-08-30)

**The frame is inside its budget at both clocks, and that is the headline.**
`FRAME` reads **50 ms at `fast` and 50 at `normal`** — 50 is the *paced period*,
because `sync` waits for the next boundary and only stops waiting when a frame
overruns, so a steady 50 at 150 MHz means the body fits in 50 ms **with the
clock at half speed**. §15.5's overclock is a precondition for the eleven-robot
frame, not for this one. The worst frame is **75 ms at `fast` and 177 at
`normal`**.

**The worst frame is the room change, and it is three times M1's figure for
it.** M1 measured a transition at 30 ms at `fast` — but M1 measured
`draw.room`, and `show.room` has never been measured at all. It is twenty
cursor moves and twenty `type`s into the text plane plus a `recycle`, and it is
the only thing in a transition large enough to be the 45 ms difference. So M1's
promise in this section — "a transition's worst case is 33 ms against §15.3's
50 ms frame, so the build does not need spreading over two frames" — **is not
true as built**, and what is untrue about it is the readout rather than the
room.

**That is a measurement, so the next run makes it one.** `go.room` now times
`show.room` on its own and the screen carries `HUD` beside `ROOM`, `FRAME` and
`WORST`, so the four numbers read against each other: if `WORST` is `ROOM` plus
`HUD` plus a present, nothing else is hiding in the transition. If `HUD` is the
45 ms, the fifteen masks cost more than the room they describe and the decision
is whether M1's gate instrument survives into M3 — which is a real question,
because §19's M1 note is right that a room whose picture and table disagree is
a robot walking through a wall you can see.

**And the man left a trail in every direction**, which is [B64](bugs.md) and is
fixed above: the eraser was one pen-8 stroke and a wide pen is a disc, so
seventeen of his 128 pixels survived every frame.

#### The second reading, and where the hitch actually was (2026-08-30)

**Trails gone, and the worst frame is 76 ms at `fast` and 123 at `normal`** —
`normal` down from 177, `FRAME` still a paced 50 at both clocks. That pair
settles the attribution without a third reading, because the leftover after
M1's build-and-present (46 ms at `fast`, 79 at `normal`) scales at **1.72×**,
which is *below* P13's measured 2.059 interpretation ratio: part of it is SPI
wire, so it is not interpretation and not the room.

**It is the readout, and the mechanism is that text is not batched.**
`screen_putc` sends every character straight to the panel — `lcd_putc_attr`
plus `lcd_set_cursor_char` plus `lcd_move_cursor`, over SPI — where graphics
accumulate off-screen until `sync`. So a character costs about **a third of a
millisecond wherever it is written**, and the transition frame was writing
**141 of them**: the room line (14), the fifteen masks (45), the help line (34)
and the four timing rows (48). 141 × 0.33 is 46 ms, which is the number.

**Eighty-two of those 141 were waste and are gone.** The help line never
changes and is drawn once in `init.game`. The timing rows keep their own
once-a-second schedule and were being written a second time on every doorway.
That leaves the masks, which are not waste — they are M1's gate and M3's guard
— so they are **deferred one frame**: `show.room` marks them due and the next
frame draws them. They cost the same and land in a frame with 40 ms spare, and
the player sees them 50 ms later, which is not a thing anyone can see.

**One text job a frame is now a rule rather than a coincidence**, because the
masks and the timing rows colliding would put 93 characters in one frame and
make *that* the worst frame instead. `show.text` gives the masks the frame
after a doorway and the timing rows the next free one.

**Predicted from the measured 0.33 ms a character**, and the next board run is
the check: a doorway becomes **~35 ms at `fast` and ~52 at `normal`**, the
frame after it **~25 and ~35**. If that holds, M1's promise in this section —
"a transition's worst case is inside §15.3's 50 ms frame, so the build does not
need spreading over two frames" — is true at `fast` as it was written, and
`normal` misses it by a couple of milliseconds on the doorway alone.

#### The third reading, which closes the budget (2026-08-30)

All four numbers, from the same screen. **`fast`: HUD 22, ROOM 11, FRAME 50,
WORST 98. `normal`: HUD 40, ROOM 23, FRAME 50, WORST 118.**

**The deferral worked exactly as predicted, and the worst frame simply stopped
being the doorway.** Reconstructed:

| | `fast` | `normal` | ratio | what it is |
|---|---:|---:|---:|---|
| `ROOM` — the build alone | 11 | 23 | **2.09** | pure interpretation, and M0 predicted 10.71 at 300 MHz |
| `HUD` — 45 mask characters + a `recycle` | 22 | 40 | **1.82** | CPU and wire mixed, ~0.4 ms a character |
| doorway = ROOM + room line + present | ~36 | ~52 | | predicted ~35 and ~52 |
| **`WORST` — the FIRST frame** | 98 | 118 | 1.20 | 48 and 68 ms of work **plus a 50 ms cadence seed** |

`ROOM`'s 2.09 against P13's measured 2.059 is as clean a confirmation as this
project has taken: the room build is interpretation and nothing else. `HUD`'s
1.82 puts a character at about **0.4 ms**, which is the number §19's second
reading inferred and this one measures.

**And `WORST` was an artefact of my own instrument.** `frame_sync_wait_ms` has
no baseline on its first call, so it seeds the cadence at `now` and puts the
first boundary **one period out** — core's `test_first_wait_seeds_full_period`
says so in as many words. A frame timed across its own `sync` therefore reads
its work plus fifty. The arithmetic closes to the millisecond: `fast` is 22
(masks) + 18.6 (full present) + ~7 + **50** = 97.6 against 98 measured, and
`normal` is 40 + 18.6 + ~9 + **50** = 117.6 against 118. Two clocks, two
independent sums, both within a millisecond of the reading.

**Battlezone had already solved this and I did not carry it over**: its
`hud.tally` opens `if 2 > :frame.count [stop]`, and that guard is there for
exactly this. Berzerk takes the cause rather than the symptom — **one `sync`
before the loop**, which absorbs the seed where nothing is timing it and puts
the room on the screen at once instead of a frame later — and the first room's
masks are now drawn in `init.game` rather than deferred into the frame that is
also presenting a whole canvas for the first time. Deferring is for doorways,
which happen while the loop is running and has somewhere to put the work.

**So the frame budget is closed and every part of it is measured.** The
prediction for the next run, and there is nothing left in it that is inferred:
**`WORST` is the doorway, ~36 ms at `fast` and ~54 at `normal`**, with the mask
frame at ~29 and ~49 and an ordinary frame well under both. M1's promise — that
a transition fits inside §15.3's 50 ms frame, so the build need not be spread
over two — is then **true at `fast` as written and short by about four
milliseconds at `normal`**, which is a game that drops one frame at a doorway
when it is not overclocked. §15.5 already says the clock is a precondition.

#### The fourth reading: nothing overruns at `fast`, and a hole in the wall test (2026-08-30)

**`fast`: HUD 22, ROOM 11, FRAME 50, WORST 51.** The doorway prediction is
confirmed as far as this instrument can confirm it — **51 is one millisecond of
clock granularity over the 50 ms period, so nothing overran at all**, including
the doorway. M1's promise now holds at `fast`: the room is built inside the
frame the man walks in, with room to spare.

**But `WORST` had stopped being able to say anything else**, and that is the
instrument's fault rather than the game's. `sync` pads every frame that fits
its budget to exactly the period, so a worst taken *across* it can only ever
read 50 — it cannot tell a 20 ms frame from a 49 ms one, and the ~36 ms
prediction for the doorway is unverifiable through it. `WORST` is now the worst
**body**, taken before `sync`, which is Battlezone's own `body.ms`/`frame.ms`
split: the margin becomes visible and an overrun still reads above 50.

**And the board found the collision hole that matters** ([B66](bugs.md)):
*"man does not collide with end of wall."* §8.3 above carries the correction —
the test was **crossing** and had to be **occupancy** — and it is the more
interesting of the two findings, because the sentence it came from is §6.3's
and has been in this document since it was drafted.

#### CLOSED on a board, 2026-08-30

All three defects the play sessions found are **confirmed fixed on hardware**:
the man's trail ([B64](bugs.md)), the end of a wall ([B66](bugs.md)) and the
leftmost column's short runs ([B67](bugs.md)). M2 is done.

**What the milestone cost, and it is worth stating plainly: three defects, and
a host test could have caught every one of them.** The eraser assumed a wide
pen squares off its stroke where the reference says it stamps a disc; the wall
test asked whether the man had *crossed* an edge where the cabinet asks whether
he may *be* somewhere; and the wall drawer used the ROM's 48 where this port's
one-pixel border makes the leftmost column 52. None of the three needed a
board to find — each needed a test that asked the question the *player* asks,
and each now has one:
`test_the_erase_covers_every_pixel_the_stamp_wrote`,
`test_the_end_of_a_vertical_wall_kills_him` and
`test_every_wall_runs_from_one_grid_line_to_the_next`. The pattern in all
three is the same and M3 should carry it: **the tests checked that the game did
what I had written down, and the defects were in what I had written down.**

**The general lesson is worth carrying to M6**, which owns the score, the lives
and the `BONUS` flash: **on this panel a character costs about a third of a
millisecond and a frame has 150 of them**. A HUD that rewrites itself every
frame is not affordable, and the fix is not to write less but to write only
what changed.

**§22 Q1 is now a person's to answer.** The game ships 1:1 as this section said
it would, and whether the figures are too small to read is the one thing on
this list that a host cannot see.

**What M2 did not build.** The electrocution is M7's with the rest of the
deaths — §7.6 wants the Vectrex's `PlayerFriedScales`, which is five scales
where `setmag` has two, so it is a question rather than a transcription and the
man flickers instead. Lives are M6's, so he comes back where a new game starts
him, in the room he died in.

**One number is worth carrying forward, and it is about storage rather than
time.** An ordinary frame spends **zero cells**, measured. The once-a-second
readout does not: `setcursor [20 26]` copies its literal and a three-argument
`(type ...)` conses its arguments, about fifteen cells a call — so the readout
ends with `recycle`, which is B25's floor used as a floor, on one frame in
twenty. `test_the_word_table_converges` walks the man two thousand frames and
requires the second thousand to spend nothing at all: bounded, not
zero-per-frame, which is the property §18 actually needs.

## 20. Tests

`tests/test_berzerk.c`, mirroring `tests/test_battlezone.c`: the game is pure
Logo, so what the host can check is that it is **right**, which is the half a
board cannot see. The list below is M1 onwards; `tests/test_p15m0.c` already
covers the generator, the intersections, `IQ`, the firing windows and the
harness itself, and the game's own tests inherit those assertions rather than
repeating them.

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
  Ship 1:1 at M2 and ask. **M2 shipped 1:1 on 2026-08-30 and the question is now
  open in the only place it can be answered**: everything else on M2's list a
  host test closed, and this one it cannot see at all.
- **Q2 — 20 fps or 15?** §15.3 argues 20 on the grounds that a dodging game
  degrades faster than a driving one. M0's `c + m·n` decides whether 20 survives
  eleven robots. **Answered 2026-08-29: neither, as things stand.** The measured
  frame is `35.13 + 6.44 n`, so eleven robots cost **106.0 ms** — 9.4 fps. 20 fps
  seats **2.3** robots and 15 fps seats **4.9**. The four identified design
  savings come to roughly 2.7× on the slope between them, which would put eleven
  robots near **70 ms (14 fps)** — so even 15 needs everything to go right *and*
  a leaner floor. **This is now the item's open question rather than a detail of
  it**, and it is a scope decision, not a measurement: cut robots, cut the frame
  rate, take an interpreter lever, or stop. §17.
- **Q3 — how many rooms should a maze remember?** None, by §6.1 — but the
  cabinet's rooms repeat every 256 in each axis, and whether to reproduce that
  wrap or let the coordinates run is a one-line decision nobody has taken.
  **ANSWERED 2026-08-30 at M1, which is where the room coordinates live: they
  wrap at 256.** The argument turns out not to be about fidelity at all. The
  seed is `ROOM_X + 256 · ROOM_Y` and that is *one number*, so letting the
  coordinates run does not give an unbounded maze — it aliases room (256, 0)
  onto room (0, 1), which is a wrap nobody chose and one that makes the maze
  fold along a diagonal. Wrapping each axis reproduces the cabinet (`$4345`
  holds `ROOM_X` in L and `ROOM_Y` in H, a byte each), keeps every room's seed
  in 0–65535 where the LCG's `modulo 65536` is exact, and removes the negative
  seed that walking left out of room 0 would otherwise produce. `modulo` takes
  the sign of its divisor, so it is two lines and no test of a special case.
  The maze still remembers **nothing**; it is 65,536 rooms on a torus.
- **Q4 — does the scroll survive?** §17 cuts it and M7 reconsiders it. 112 ms
  of present for a transition is either the best 112 ms in the game or an
  irritation, and only playing it says which.
- **Q6 — pen or costume for the figures?** §7.6 and §7.3. Raised 2026-08-29,
  after M0's gate failed with drawing at 4.7× its estimate. **Priced the same
  day and the price is decisive: one robot costs 0.26 ms stamped against 2.00
  drawn — 7.7× — and the best frame goes from 106.30 ms to 77.43**
  ([`p15m0-stamped-pico2w-2026-08-29.md`](measurements/p15m0-stamped-pico2w-2026-08-29.md)).
  The costumes cost 16 ms to build, once, at startup.

  **The measurement cannot make this decision, and that is the point of
  raising it separately.** What it buys is 27 % of the frame and the difference
  between a game that runs and one that does not. What it costs is §7.6's
  sentence — "every mark this game makes is one turtle with a pen" — and that
  sentence is not decoration: it is the claim that made this port worth doing
  as a *vector* game rather than a re-implementation.

  **§1's thesis survives and §7.6's does not.** `snapsh` captures what the pen
  draws, so the five `rob.*` procedures remain the artwork and remain the ROM
  restored to lines; they run once at startup rather than eleven times a frame.
  Every figure on the screen is still a line drawing. It is no longer *drawn as
  lines every frame*, which is a different and smaller claim.

  **CLOSED 2026-08-29: costumes, from the cabinet's own bitmaps.** Not the
  vector models cached — the ROM's bytes, which are less code than the five
  hand-built pen walks and are the game's actual appearance. §7 carries the
  decision and its cost; §18 carries the fifteen-slot ceiling it introduced;
  §1's restoration claim now applies to the walls and the bolts and not to the
  figures.

- **Q5 — should `say` be phonemic, textual, or both?** Not this document's
  question, but Berzerk is the caller that would settle it: five fixed
  sentences and one assembled at runtime from a 30-word vocabulary, which is an
  argument for text with a phoneme escape.
