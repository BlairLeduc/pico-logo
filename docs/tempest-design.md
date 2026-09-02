# Tempest in Pico Logo (design)

Status: **DESIGN ONLY. Nothing is built.** No game file, no harness, no M0.
Every number below is estimated or read off the disassembly; none is measured.
§16.3 carries the gate M0 has to pass and §23 carries what is open.

Tempest is the fourth arcade port in this tree and the third whose cabinet was
a genuine XY vector machine — Asteroids and Battlezone were the other two,
Berzerk was a raster that had to be translated back into lines
([P14](vector-direction-design.md)). So the drawing has no translation to do at
all. What it has instead is the thing none of the other three had: **every
object on the screen is drawn at a size that depends on how far away it is**,
and there are up to thirty-five of them.

**The disassembly is the best and the worst source this tree has had.**
[`Tempest.asm`](Tempest.asm) is der Mouse's reverse engineering of the
program ROM, 11,825 lines, and it is *unusually* good: the enemy behaviour is a
documented p-code virtual machine (§9.2), the entire 99-level difficulty ramp
is a second little table machine whose six opcodes are spelled out in the
comments (§13), and the scoring, the superzapper and the spawn rules are all
readable line by line. **And it contains none of the pictures.** `lev_x`,
`lev_y`, `lev_angle` and `lev_remap` are `.chunk` with no bytes ($b97c–$bcfc,
line 7470), and the vector ROM that holds the flipper, the tanker, the pulsar,
the fuseball, the spiker and the claw is below $2000 and is not in the file at
all. So this port gets **the whole mechanism and not one shape**: §6 and §7 are
authored, and everything else is transcribed.

**Single player only**, as Berzerk is (§18). The cabinet alternates two.

**Source of truth is the arcade**, in the order: the disassembly, then the
gameplay summary ([`tempest-gameplay.md`](tempest-gameplay.md)) for rules the
code makes hard to read. Where the two disagree the code wins and the
difference is written down — and it already has, twice, in §9.4 and §12.

---

## 1. Deliverables and source of truth

| | |
|---|---|
| Game | `logo/games/tempest` — one Logo file, no extension, no `-` or `/` in the name so `load "tempest` parses |
| Tests | `tests/test_tempest.c` (Unity + mock device), mirroring `tests/test_berzerk.c` |
| Design | this document |
| Measurement | `tests/logo/p17m0`, written to a **file** — a number on a display cannot be pasted anywhere. Times a real frame at 1, 4 and 7 enemies with twelve shots and sixteen spikes live, with the drawing pass read apart from the logic pass, the tube redraw read apart from both, and the depth divide (§3.2) priced on its own, under **both** erase strategies (§3.3) |
| Arcade | [`Tempest.asm`](Tempest.asm), der Mouse's disassembly of the program ROM. Addresses below are that file's, and line numbers are into it |
| Gameplay | [`tempest-gameplay.md`](tempest-gameplay.md) |
| Well shapes | Authored (§6.2). **Not in the disassembly** |
| Figures | Authored (§7). **Not in the disassembly** |
| High scores | `/games/tempest.scores`, beside the game file, as Asteroids, Battlezone and Berzerk do |

Play: `load "tempest` then `tempest`.

All three boards. Nothing here needs WiFi, TLS or PSRAM, so `LOGO_HAS_WIFI` and
`LOGO_HAS_TLS` are not consulted anywhere in the game. **300 MHz is a
precondition, not an optimisation**, the same call Battlezone made at its
§16.7.3 and Berzerk at its §15.5.

## 2. What the game is, mechanically

The arcade rules, kept:

- You are a **claw** on the near rim of a well, on one of **sixteen segments**.
  A knob slides you around the rim. Some wells are closed rings and you wrap;
  some are open and you stop at the ends.
- You **fire down your own segment**. Up to eight shots are in flight at once.
  A shot kills whatever is in that segment within a tolerance (§10).
- The **superzapper** clears the field, once per level; a second press in the
  same level kills one enemy. It recharges at the next level (§8.3).
- Seven enemy types across five kinds: **flipper**, **pulsar**, **tanker**,
  **spiker**, **fuseball**, plus the pulsar-tanker and fuseball-tanker, which
  are tankers carrying a different load (§9.4).
- Enemies climb the well toward you. A flipper that reaches the rim **grabs
  you and drags you down** (§9.3). A pulsar **electrifies its segment**.
  A fuseball **kills on contact**. Enemies **shoot** back.
- A **spiker** grows a spike up its segment. Spikes persist. They can be shot
  down, and at the end of the level you **fly down the well** and a spike you
  hit kills you (§11).
- Clear the field and the level ends. **Ninety-nine levels**, sixteen shapes,
  cycling; from level 99 the shape is picked at random (§13).
- Lives are lost to a grab, a shot, a pulse, a fuseball or a spike; a bonus
  life comes every N points (§12).

Reduced or removed in §18, but the list above is the game.

## 3. The central decision: the well is a lookup and a divide

Every other game in this tree draws in the plane it computes in. Tempest does
not: it computes in `(segment, along)` and draws in `(x, y)`, and the map
between them is applied to **every object, every frame**. That map is the
performance question this design exists to answer, and it comes apart into
three.

### 3.1 The rim is sixteen points, and it is computed once a level

A well is sixteen boundary vertices and the sixteen segment midpoints between
them. The arcade builds exactly this at $c235–$c2e7: it copies `lev_x[]`,
`lev_y[]` and `lev_angle[]` into `tube_x[]`, `tube_y[]`, `tube_angle[]`, then
walks the ring averaging each adjacent pair into `mid_x[]`, `mid_y[]`
($c2c5–$c2e5). **Objects sit on midpoints; the picture is drawn on vertices.**

This port keeps that shape exactly — four sixteen-element lists, built once at
level start:

```
rim.x  rim.y     16 boundary vertices, relative to the vanishing point
seg.x  seg.y     16 segment midpoints, likewise
```

`mid` is the average of two adjacent `rim`s, so §6 authors sixteen vertices and
the game derives the rest. That is sixteen `item` reads and ~64 statements once
a level, and nothing per frame.

### 3.2 Depth is one divide, and it is the number M0 must price

The well is a perspective view of a flat polygon down a tube. Every point of
the picture is therefore

```
screen = vanishing.point + scale(along) * rim.point
```

with a **single** scalar `scale` shared by both axes. The arcade stores `along`
as a byte running $10 at the player's rim to $F0 at the far end (line 303:
*"player position along tube length ... normally $10"*), and this port takes
that range **verbatim**, so every speed, hit tolerance and spike height in the
ROM transfers unchanged (§5).

Pick the camera so the far rim is one eighth of the near rim, which is what the
cabinet's picture looks like, and the projection collapses to

```
s = 32 / (16 + along)          s(16) = 1.000    s(240) = 0.125
```

**One divide, and no table.** A point is then

```
make "s 32 / (16 + :a)
make "px      :s * item :i :seg.x
make "py 40 + :s * item :i :seg.y
```

— three arithmetic statements and two `item`s, about **130 µs** at 300 MHz on
§16.1's units. That is the unit cost of this game, it is paid for every enemy,
every shot and every spike, and **§16 says nineteen of them is 2.5 ms**. If M0
finds it materially worse, the fallback is a 15-entry ladder indexed by
`int :a / 16` (an `item` at 8 µs replacing the divide at 24), which costs
about 3 % of an object's radius in position error. That fallback is cheap and
ugly and is **not** taken on speculation.

`setmag` is not an alternative. It magnifies a *costume* by an integer factor
and explicitly does nothing to how far the turtle moves
([`setmag`](../reference/Pico_Logo_Reference.md)), and shapes over 16 × 16
ignore it outright. It gives two sizes where this game needs a continuum.

### 3.3 How a frame gets erased — and the well argues for clear-and-redraw

[Asteroids §3](asteroids-design.md#L88)'s question again, and Tempest asks it
with two facts that pull the opposite way from Berzerk's.

- **The well is static for the whole level** — 32 strokes of rim and 16 radial
  lane lines, and they do not move. Erase-in-place never redraws them;
  clear-and-redraw does, every frame. §16 prices that pass at **~3.4 ms**,
  four times Berzerk's maze.
- **Every moving object sits on a lane line.** An enemy is *on* its segment's
  midpoint radial, a shot travels *down* one, a spike *is* one. So an
  erase-in-place pass paints black over the well continuously, and Berzerk
  spent two bugs on exactly that failure — [B67](bugs.md), an eraser that
  missed, and [B78](bugs.md), Otto eating the maze he walked over. Here it is
  not an edge case; it is every object on every frame.

**The prediction is clear-and-redraw**, and the reason is the second bullet
rather than the first: repairing the well behind an eraser costs more than
redrawing it, because the repair has to find which strokes were touched.

**M0 measures both anyway**, at 1, 4 and 7 enemies, because Berzerk's §3
predicted clear-and-redraw, measured it, and then *inverted* the same day when
the figures became stamps. The lesson from that page is that the erase strategy
is **not a property of the game** — it is a property of what a drawing pass
costs — and the harness that measures only the winner cannot say so.

## 4. The viewport, and where the cabinet's text goes

`splitscreen`: graphics rows 0–239, text lines 24–31 below. **Pixel row is
`-y + 160`, so the presented band is turtle `y` in `[-79, 160]` and its optical
centre is `y = +40`**, which is where §5 puts the vanishing point. The split
present is 15 tile rows instead of 20 and is worth about 4.7 ms
([Battlezone §6](battlezone-design.md#L317)).

And the bottom of the screen is where the cabinet already puts its text: the
score top-left in the real machine, but the level number, the lives and the
`AVOID SPIKES` warning (gamestate $0a, line 33) all want a line of their own,
and eight rows of text below the picture is the cheapest place in the tree to
put them. Same call as Berzerk §4 and for the same reason.

## 5. Coordinates: the cabinet's `segment` and `along`, verbatim

The picture is ours (§6, §7). **The state space is not.**

| | arcade | here |
|---|---|---|
| segment | `player_seg`, `enemy_seg` — 0–15 | 0–15, identical |
| depth | `along` — $10 at the rim, $F0 at the far end | 16–240, identical |
| speed | `spd_*_msb`/`lsb`, 8.8 fixed point, negative = toward the player | one float per type, same sign convention |
| hit tolerance | `hit_tol[type]`, from `crack_speed` ($93e0) | same number, same derivation |
| spike height | `spike_ht[seg]`, **smaller is taller** | identical, including the sense |

This is [Berzerk §5](berzerk-design.md#L237)'s 1:1 rule applied to the axis that
matters. Berzerk could take the cabinet's *pixels* as turtle steps because both
were plane coordinates; here the plane coordinates cannot transfer (they are
not in the file) but the **game** coordinates can, and they are the ones every
constant in the ROM is quoted in. It is the same simplification bought on a
different axis, and it is what makes §13's difficulty machine transcribable
rather than re-tuned.

Only two things are ours: the sixteen rim vertices (§6) and the vanishing
point, which is **(0, +40)** with a near-rim radius of about **110 steps**.
That radius is the one free parameter in the picture and §23 Q1 holds it open
until something is on a screen.

## 6. The sixteen wells

### 6.1 Which well a level uses

`get_tube_no` ($c2e8, line 8295) is four lines of arithmetic and it is worth
transcribing exactly, because the level cycle is the campaign's skeleton:

```
if level >= $62 (98):  level = (random and $5f) or $40      ; 65..99, random
tube = lev_remap[level and $0f]
```

So **the shape cycles every sixteen levels through a remap table**, and from
level 99 the level number stops climbing and a shape is drawn at random out of
the 65–99 band. `lev_remap` is `.chunk 16` with no bytes, so the *order* is
authored with the shapes (§6.2).

`lev_open[tube]` is $00 for a closed ring and $ff for an open one, and it is
consulted in eleven places — the flipper's edge reversal (`rev_if_edge`, $9eab),
the wraparound in the segment walk ($a7ad), the claw's motion, the pending
enemies' spawn segment. **One global, `open?`**, and every one of those eleven
tests is a single `if`.

### 6.2 The shapes are authored, and generated

Sixteen wells, each sixteen vertices, hand-specified as a compact polar or
Cartesian description and expanded by a script into the two sixteen-element
lists §3.1 wants — the same arrangement as
[`scripts/gen_rocks.py`](../scripts/gen_rocks.py), for the same reason: a
hand-typed ring does not close, and a well that does not close looks broken in
a way a rock does not.

The published shape order for the first sixteen levels is a circle, a square, a
plus, a cross-and-bar, a flat line, a shallow V, a deep V, a stepped W, a
figure-of-eight, a heart, a star, a clover, a triangle-in-a-triangle and so on,
with the flat line and the V family **open** and the rest closed. **This is the
one place in the document where the source is a photograph rather than a ROM**,
and §23 Q2 asks whether to go looking for `136002-138.np3` and read `lev_x[]`
out of it, which would make the wells exact instead of close.

The generator's own check is the one that matters: a closed well's sixteenth
vertex must be adjacent to its first, and an open well's must not, and
`test_tempest.c` walks all sixteen and asserts it — which is the one property a
bad paste would break.

## 7. The figures

None of these are in the disassembly. What *is* in it is
`graphic_table` ($cec8, line 9927), which is the cabinet's complete inventory
of drawn objects and therefore the list of what has to be authored:

| $cec8 entry | what it is | here |
|---|---|---|
| explosion, size 1–4 | four sizes | §7.4 |
| player shot | one shape | a `dot`, §7.3 |
| cloud of dots 1–4 | the pending enemies beyond the far rim | §9.5 |
| spiker 1–4 | four animation frames | §7.2 |
| regular (flipper) tanker | rhombus | §7.2 |
| pulsar-holding tanker, fuzzball-holding tanker | §9.4's loads | §7.2 |
| four dots orthogonal / diagonal | the pulsar's two phases | §7.2 |
| enemy shot 1–4 | four frames | §7.3 |
| hit-by-shot explosion 1–9 | a nine-frame cycle | §7.4 |
| spiked player, fuzzballed player 1–7 | the two deaths | §7.4 |
| fuzzball 1–4 | four frames | §7.2 |
| 250 / 500 / 750 | the fuseball's score, drawn where it died | §12 |

Twenty-three distinct pictures, which is already near §19's twenty-five costume
slots — and that is before the flipper, which the table does not list because
the cabinet draws it with a `vscale`d chevron pair rather than a graphic.

**So the figures are pen strokes, not costumes**, and §3.2 is why: a costume is
a fixed-size bitmap and every one of these is drawn at a size that depends on
`along`. Berzerk went the other way (its §7.6) because its figures never
changed size; this game cannot.

### 7.1 The claw

The player. A pair of chevrons opening toward the centre of the well, drawn on
the near rim across the width of one segment, so it is placed on `seg.x[i]`,
`seg.y[i]` at `along = 16` and its **heading is the segment's angle** — which
is exactly what the ROM's `tube_angle[]` is for. Six or seven strokes,
literal, in the `fd`/`rt` style [Asteroids §6.1](asteroids-design.md#L373)
established, with the scale baked in because the claw is always at `s = 1`.

**The claw is the one figure that never scales**, which makes it the cheapest
thing on the screen and the only one that could become a costume if the frame
needs it.

### 7.2 The five enemies

Each is a closed or near-closed walk of four to six strokes, authored at the
near-rim size and multiplied by `s` at draw time. That multiply is the cost
this design pays for not being able to stamp: **one arithmetic statement per
stroke length**, so a six-stroke enemy is six extra statements.

The alternative — and M0 should price it, because it is a factor of two —
is **three authored sizes with the lengths baked in**, dispatched on
`int :a / 80`, which is what the cabinet does for its explosions and its
spikers (four sizes each in `graphic_table`). It is the same trade Asteroids
made at its §6.1 when nine outlines became three.

| | strokes | notes |
|---|---:|---|
| Flipper | 4 | two linked chevrons; rotates about the rim during a flip (§9.3) |
| Pulsar | 5 | a wavy line; brightens and thickens on the pulse (§9.4) |
| Tanker | 4 | a rhombus. Three colours, one per load (§9.4) |
| Spiker | 6 | a spiral; the cabinet animates it over four frames |
| Fuseball | 6 | a body and tendrils; sits **between** segments, not on one |

### 7.3 Shots and spikes

**A player shot is a `dot`** — the cabinet's is one graphic and at `s = 0.125`
it is a pixel anyway. `dot` takes a list and therefore allocates
([B52](bugs.md), and Berzerk §18's fourth ceiling), so it is a two-step pen
stroke instead, which costs the same and conses nothing. Twelve shots at ~160
µs each including the projection is **1.9 ms**, and it is the largest single
line in §16's drawing table.

**A spike is one stroke**: from the far end of its segment down to
`spike_ht[seg]`, along the segment's midpoint radial. Sixteen of them is
sixteen projections — and the spike heights change only when a spiker grows one
or a shot cuts one, so the projected endpoints are **cached per segment** and
recomputed on change. That is the one place in this design where a cache is
obviously right, because the read rate is sixteen a frame and the write rate is
under one.

### 7.4 Explosions and the two deaths

The cabinet has four explosion sizes, a nine-frame hit flash, a seven-frame
"fuzzballed player" and a one-frame "spiked player". This port takes
[Berzerk §7.6](berzerk-design.md#L670)'s answer — **random dots at the death
point, decaying over a fixed count** — for all of them, at 1 procedure instead
of 21 pictures. It is the cheapest thing in that document and the only figure
decision it did not regret.

## 8. The player

### 8.1 The spinner has no knob

The cabinet's control is a rotary encoder read through a POKEY pot line
(`track_spinner`, $adce, line 5934), and there is no knob on a PicoCalc.
The claw's motion is therefore **key-driven with the spinner's feel
reconstructed**: `pollkeys` once a frame, then left/right held builds a
velocity over three or four frames and releasing decays it, so a tap moves one
segment and a hold sweeps. `keydown?` and not `readchar` — this is a
real-time frame loop and [`readchar` blocks](../reference/Pico_Logo_Reference.md).

**This is the largest deviation in the port and it is forced.** §23 Q3 records
that the tuning is a play-test question and cannot be settled here.

`player_seg` wraps on a closed well and clamps on an open one, which is
`open_level` again (§6.1) and two lines.

### 8.2 Fire

Eight shots, `ply_shotseg[8]` and `ply_shotpos[8]`, exactly the cabinet's
arrays. `ply_shotcnt` holds the live count so the fire test is one comparison
and not a scan. A shot is created at the claw's segment at `along = 16` and
moves **away** from the player; `move_shots` ($a18f, line 4386) walks all
twelve slots — eight friendly then four enemy — in one loop, and this port
keeps that arrangement because it makes the shot pass a single procedure over a
single pair of lists.

### 8.3 The superzapper, exactly

`check_zap` ($a83a, line 5233) and `zap_length` ($a883) are twelve lines and
they say something the gameplay summary gets nearly right and not quite:

```
zap_uses: 0, 1 or 2 — reset to 0 at each level ($a831 reset_sz)
press with zap_uses < 2  ->  zap_uses += 1, zap_running = 1
each tick: zap_running += 1; if zap_running >= zap_length[zap_uses], stop
zap_length = [ 0, $13 (19), 5, 0, 0 ]
while running, on every ODD tick from 3 up: kill the highest-numbered live enemy
```

So the **first** use runs nineteen ticks and kills one enemy every other tick
from tick 3 — nine kills, against seven slots, so it clears the field. The
**second** use runs five ticks and gets exactly one kill, at tick 3. That is
"destroys one random enemy" as the summary has it, except that it is not
random: it is the **highest-numbered live slot**, which is a detail a player
can exploit and a test can assert.

`$a8a4` clears the enemy's `$028a` low bits before killing it, so **a
superzapped tanker does not split**. That is worth having: it is the difference
between the zapper being a panic button and being a trap.

## 9. The enemies

### 9.1 Seven slots, and that is every level

`max_enm` is the `9607_4b` record `02 01 63 06` — **constant 6 for all
ninety-nine levels** — and the loops run `x` from `max_enm` down to 0, so there
are **seven enemy slots and never more**, at level 1 and at level 99 alike.
The campaign's escalation is in the *types*, the *speeds* and the *wave size*,
not in how many are on screen. That is the single most useful fact in this
document for the frame budget, and it is why §16 can be written at all.

Per-slot state, one list of seven each, following the cabinet's arrays:

| here | arcade | holds |
|---|---|---|
| `e.seg` | `enemy_seg` $02b9 | segment 0–15 |
| `e.along` | `enemy_along` $02df | 0 = empty slot, else 16–240 |
| `e.type` | `$0283` low 3 bits | 0 flipper, 1 pulsar, 2 tanker, 3 spiker, 4 fuseball |
| `e.flags` | `$0283` bits $18/$40/$80 | direction, between-segments |
| `e.mode` | `$028a` | $80 receding, $40 may shoot, $03 split-on-arrival |
| `e.pc` | `enm_move_pc` $0291 | the p-code program counter (§9.2) |
| `e.ctr` | `$0298` | the p-code's loop counter |

Seven lists of seven is seven names for forty-nine values, which is
[Berzerk §18](berzerk-design.md#L1967)'s arrangement and the reason its global
count fitted.

### 9.2 The p-code machine, and the decision about it

**This is the best thing in the ROM and the hardest call in this design.**

Enemy behaviour is a virtual machine ($9b3a, line 3570). Each enemy carries a
program counter into a byte array at $a0f7; `move_enemies` runs opcodes for
that enemy until one of them sets `pcode_run` to zero, which ends the enemy's
tick. Twenty opcodes, all documented at $9ba2 (line 3625):

```
00 halt        02 store next byte in ctr    04 skip 2 if flag==0
06 branch      08 dec ctr, branch if nonzero  0a nop
0c move per this type's speed, and handle reaching the rim
0e grow spike, reverse, convert to tanker
10 load a named global into ctr             12 start flip
14 continue/end flip                        16 reverse segment direction
18 check and maybe grab the player          1a branch if flag==0
1c set flag = enemy-is-above-its-spike      1e fuseball movement
20 check enemy-touches-player death         22 pulsar motion
24 set direction toward player              26 check for pulsing
```

and **nine entry points** into one shared 152-byte program, listed at $a0f7–
$a18e (line 4243) with the disassembler's own per-byte commentary. The nine
are: `$07` just climb; `$0b` climb eight, flip, repeat; `$19` flip constantly
with one climb between; `$24` two flips one way, three the other, forever;
`$42` the top-of-well grab loop; `$53` ride the spikes; `$61` fuseball; `$72`
pulsar; `$87` flip away from the player, then climb four.

**And the level chooses which one.** `flipper_move` is the `9607_5b` record —
a `06` opcode, meaning *indexed by position within the block of sixteen levels*
— and its sixteen bytes are

```
level in block:  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
flipper program: 07 0b 19 24 53 0b 24 19 53 87 24 19 53 07 87 24
```

So the flipper's personality is a property of the *shape* you are on, it
repeats every sixteen levels, and it is **six bytes of behaviour selected by one
table lookup**. Nothing else in this tree's four ports has that structure.

**The decision: transcribe the machine, or flatten it into nine procedures?**

- *Transcribe.* One `pcode.step` procedure over a 152-element list, dispatched
  on the opcode. Costs an `item` (8 µs) plus a dispatch per opcode, and an
  enemy tick runs two to five opcodes, so **~5 statements of overhead per
  opcode** — call it 0.6 ms for seven enemies. It is a nested interpreter
  inside an interpreter, which is the thing this tree has learned to be afraid
  of ([P10](interpreter-throughput-design.md)).
- *Flatten.* Nine procedures reached by `run` off a one-of-nine name list,
  which is what [Berzerk §15.2](berzerk-design.md#L1703) did for its robot
  dispatch and Asteroids measured at 360–398 µs a rock. The behaviour is
  identical because the programs are small and their control flow is
  structured; the *fidelity* is identical because the flattening is mechanical.

**The design's answer is flatten, and M0 does not need to measure it** — the
transcribed machine cannot be cheaper than the thing it interprets, and the
only argument for it was fidelity, which flattening does not cost. What the
p-code buys either way is the **specification**: the nine programs are the
ground truth for what a flipper does on level 5 versus level 10, and no other
source states it.

### 9.3 Flipping, and the grab

A flip is opcodes `12` (start) and `14` (continue/end), and while it runs the
`$80` bit of `$0283` is set — *"between segments"* (line 319) — and `$02cc`
holds `$80 + the current angle`. So a flipping enemy is **drawn at a fractional
segment position**, which the projection handles for nothing: interpolate
between `seg[i]` and `seg[i±1]` before scaling.

Two constants govern it. `pulsar_fliprate` is *"number of movement ticks
between pulsar flips"* — 40 then 20 at levels 17 and 18, alternating 20/40 to
level 32, a ramp down from 20 over 33–39, then alternating 20/10 to 99 — and `flip_top_accel` is *"the ratio by which flipper
flips at top-of-tube are accelerated"*, which is `02` for levels 1–32 and `03`
from 33 (and is decremented on **easy** for levels below 17, $9348). So a
flipper on the rim flips two or three times as fast as one in the well, which
is why a rim full of flippers is the thing that kills you.

The grab is the `$42` entry point: set `ctr` to 4, run `18` (*check and maybe
grab player*) four times, then start a flip, reload `ctr` from
`flip_top_accel`, and loop. **A grab is therefore a four-tick window, four
times per flip**, and if the claw is on that segment during one of them the
player is taken. `$0201` gets `$80` set (line 296) and the death is a
drag down the well.

### 9.4 Pulsars, fuseballs, spikers, tankers

**Pulsar.** Entry `$72`. Two globals drive the pulse: `pulse_beat` (the
`9607_3f` record — 4 for levels 1–48, 6 for 49–64, 8 for 65–99) and `pulsing`,
and once per tick after all movement `pulsing += pulse_beat` ($9b56). When the
sign bit of `pulsing` flips from 0 to 1 the pulse sounds, and `pulse_beat` is
negated when `pulsing` reaches $0F ascending and $C1 descending ($9b8c) — so
it is a **triangle wave**, not a sawtooth, and the pulse is its peak. Opcode
`26` tests it, and a pulsar caught at the peak in the player's segment kills.

**Pulsars fire, and only from level 60.** `pulsar_fire` is the `9607_03`
record `02 3c 63 40` — $40 for levels $3c (60) through 99 and zero below —
ORed into the pulsar's mode byte at $9aac. **On hard, $937a sets it
unconditionally.** [`tempest-gameplay.md`](tempest-gameplay.md) says *"aside
from the Fuseball and Pulsar, enemies can shoot"*, and the ROM says the pulsar
shoots from level 60. The ROM wins (§1).

**Fuseball.** Entry `$61`, opcode `1e`. Never shoots — its mode byte is `$00`
at $9b06 with nothing ORed in. It moves on the *edges* between segments, not on
midpoints, and two per-level globals steer it: `fuzz_move_flg` (the `$40`/`$80`
bits, from record `9607_6b`, values 0/$40 for levels 17–32, $40/$c0 for 33–48,
$c0 from 49) and `fuzz_move_prb`, *"the chance of fuzzballs doing something in
certain regions of the tube"* — $dc for levels 1–16, $c0 for 17–39, a ramp for
40–64, $e6 from 65. Its speed is **twice the flipper's**, computed by shifting
`spd_flipper` left one at $93ba.

**Spiker.** Entry `$00`: move, opcode `0e` (*grow spike, reverse, convert to
tanker*), branch if converted, loop. `spiker_hop` ($a028) and the `$9fcc`
block hold the growth. **Spike heights are stored small-is-tall** — the wave's
starting height is `wave_spikeht`, a `06` record indexed by position in the
block of sixteen:

```
level in block:  1  2  3   4   5   6   7   8   9  10  11  12  13  14  15  16
spike_ht:       00 00 00  e0  d8  d4  d0  c8  c0  b8  b0  a8  a0  a0  a0  a8
```

Zero means no spike. So **levels 1, 2 and 3 of every block of sixteen start
clean** — which is exactly why `min_spikers` is documented as *"1–3:0 … 17–19:0"*
(line 180): two independent tables agreeing, and the best consistency check in
the disassembly.

**Tanker.** Entry `$07`, just climb. Its mode byte is `tanker_load[y] or $40`
where `y` is `random and 3` and `tanker_load` is four bytes: the first two are
hard-wired to 1 at $93d7, the third is the `9607_43` record (1 for levels 1–32,
3 for 33–40, 2 from 41) and the fourth is `9607_47` (1 for 1–48, 3 from 49).
The low two bits of the mode byte are what the enemy splits into on reaching
`along = $20` — `01` two flippers, `02` two pulsars, `03` two fuseballs
(line 322). So **a tanker's load is drawn from a bag of four that changes with
the level**, half of it always flippers, and the pulsar-tanker and
fuseball-tanker of the gameplay summary are that bag's other two slots.

### 9.5 Spawning: minima, maxima, and one clever rule

`create_enemies` ($98a2, line 3217) and the block at $9a26 are the whole of it,
and it is three passes:

1. **Unsatisfied minima first.** For each type, if `n_type < min_type` and the
   type is available, make one. `min_*` come from §13's tables; the
   disassembler has already reduced them to plain English at lines 177–185
   (*"flipper 1–4:1, 5–99:0"*, *"pulsar 1–16:0, 17–32:3, 33–99:1"*, and so on).
2. **The spiker/tanker rule.** If both are available, look at the shortest
   spike in the well; if it is **taller than $cc** make a spiker, else make a
   tanker ($9a40). One comparison, and it is why a well that has been shot
   clean fills back up with spikers.
3. **Otherwise, a weighted random walk.** Start at `random and 3`, then go
   round the five types up to four times, taking the first with a nonzero
   minimum *and* availability ($9a61).

Enemies arrive as **pending** dots beyond the far rim — `pending_seg[64]` and
`pending_vid[64]`, sixty-four slots, seeded at $9250 with random segments —
and `enemies_pending` is set to `wave_enemies` at level start
(`set_enm_and_spikes`, $9234). The four `cloud of dots` graphics
(`graphic_table`) are what they are drawn as. Sixty-four pending slots against
seven live ones is the wave's queue, and this port keeps it as **a count and a
segment list**, not sixty-four slots: nothing reads a pending slot except the
drawing pass, and drawing them as a scattering of dots at the far rim needs the
count only. That is §18's one structural cut.

## 10. Shots

Twelve slots, eight friendly and four enemy, in one pass ($a18f). Friendly
shots move away from the player, enemy shots toward. `enm_shotmax` is the
`9607_0f` record — `04 01 09` giving 1,1,1,2,3,2,2,3,3 for levels 1–9, then 2
for 10–64 and 3 from 65 — and it caps how many enemy shots exist at once.
`shot_holdoff` (record `9607_0b`) is *"after firing, an enemy cannot fire until
at least this many ticks have passed"*: `08 01 14 50 fd` is a **linear ramp**,
80 at level 1 falling by 3 a level to level 20, then 20 for 21–64 and 10 from
65. So the enemies' rate of fire is the campaign's clearest single curve, and
it is four numbers.

A shot hits an enemy in the same segment within `hit_tol[type]` of it. That
tolerance is not a constant: `crack_speed` ($93e0) derives it **from the type's
speed** — three left shifts to split the 8.8 fixed-point speed into msb and
lsb, then `tol = (~msb + 13) >> 1`. At level 1 a flipper's speed byte is $d4,
which cracks to msb $fe (−2), lsb $a0, so the flipper moves **1.375 units of
`along` per tick** and its hit tolerance is **7**. Faster enemies get larger
tolerances, which is the ROM quietly making sure a shot cannot step over an
enemy between ticks. It is four lines and it must be transcribed, not
simplified to a constant, or fast levels become unhittable.

`$02f2` (line 351) slows a friendly shot that has hit a spike, *"so that shots
that hit a spike can also, or perhaps instead, hit the spiker that is growing
it"*. Kept, because it is the difference between a spiker being killable and
not.

## 11. Spikes and the warp

Spikes are `spike_ht[16]` (line 378) and the marker byte `$039a[16]` (line
370): `$80` when a spiker grew it, `$c0` when a player shot cut it — *"$80 =
don't draw bright dot at end, $40 = draw mini-explosion at end"*. Two bits,
two visual states, and a shot that hits a spike scores **1 point** (§12).

When the field is clear the level ends and the player **flies down the well**:
gamestate $0e is *"zooming off the end of a level"* and $20 is *"descending
down tube at level end"* (lines 36–38), with `zoomspd_lsb`/`msb` as the rate.
`player_along` grows from $10 (line 304: *"increases when going down tube"*),
and a spike in the player's segment that the claw reaches is death — that is
`$81` in `$0201`, *"player spiked while going down tube"* (line 299). The
`AVOID SPIKES` message is `$0123`'s $80 bit (line 349) and gamestate $0a.

**The descent is the same projection run backwards** and costs nothing new: the
claw's `along` climbs, `s` shrinks, and the well is redrawn each frame anyway
under §3.3's clear-and-redraw. It is the cheapest set piece in the game and it
should be built early — M2, with the spikes — because it is the only thing that
proves the projection reads as depth rather than as a shrinking picture.

## 12. Scoring

`inc_score` ($ca6c, line 9302) works in BCD out of a pair of tables at $caf1
(low byte) and $caf9 (middle byte), indexed 0–7, and `$a3c5` maps enemy type to
index:

| index | value | who |
|---:|---:|---|
| 0 | 0 | — |
| 1 | **150** | flipper (type 0) |
| 2 | **200** | pulsar (type 1) |
| 3 | **100** | tanker (type 2) |
| 4 | **50** | spiker (type 3) |
| 5 | **250** | fuseball, near |
| 6 | **500** | fuseball, middle |
| 7 | **750** | fuseball, far |

The fuseball's three values are picked at $a31a: `random and 7`, and if the
result is 3 or more it becomes 0 — so **250 with probability 3/8, 500 with 1/8,
750 with 1/8** and 250 the rest. Not a distance, as it is often described; a
weighted draw. And `$a3c5`'s entry for type 4 is `01`, so a fuseball killed
through the *type* path scores 150 — the 250/500/750 path is a different call
site. Both are kept.

A shot that hits a spike adds **1** ($a21e–$a22a sets $29 to 1 with x=$ff, the
"use $29/$2a/$2b" path).

**The bonus life** is `bonus_life_each` in tens of thousands, out of
`bonus_pts_tbl` at $d6f7 — `.chunk 8 ; 2 1 3 4 5 6 7 0`, so the operator's
setting selects 20k, 10k, 30k … and 0 for none. The check at $cab0–$cadc
compares the high score byte against it after every addition, which makes it a
**latch on the hundred-thousands digit**, not a modulo — the same shape as
Berzerk's, and the same trap.

**Initial lives** come from `init_lives_tbl` at $d6ff — `.chunk 4 ; 3 4 5 2`.
Three is the default.

**And the level-select bonus.** Tempest lets you start at a higher level and
pays you for it, and both tables are in the file: `startlevtbl` ($91fe) is
1, 3, 5, 7, 9, 11, 13, 15, 17, 20, 22, 24, 26, 28, 31, 33, 36, 40, 44, 47, 49,
52, 56, 60, 63, 65, 73, 81 (as $00, $02, $04 … one-based level minus one), and
`start_bonus` ($91c6) is the matching BCD list: 0, 6000, 16000, 32000, 54000,
74000, 94000, 114000, 134000, 152000, 170000, 188000, 208000, 226000, 248000,
266000, 300000, 340000, 382000, 415000, 439000, 472000, 531000, 581000,
624000, 656000, 766000, 898000 (the low `00` is not stored — $91b7–$91b9 write zero
into `$29`). **Twenty-eight rows, both transcribed**, and the level-select
screen is one of the two things in this game a player recognises before they
have played it.

## 13. The campaign is a table machine, and it is transcribable

The whole 99-level difficulty ramp is **twenty-eight parameters computed by a
28-entry pointer table at $9607 driving a six-opcode interpreter at $92d6**
(line 2550). The disassembler documents the opcodes at $968f (line 2929) and
they are worth quoting exactly, because they are the port's specification:

```
lwb = ((lev - 1) & 15) + 1        ; position within the block of sixteen
b[] = the bytes after the two level-bound bytes

02  A = b[0]                                  record is 4 bytes
04  A = b[lev - ltmin]                        record is 3 + (ltmax-ltmin+1)
06  A = b[lwb - ltmin]                        record is 3 + 16
08  A = b[0] + (lev - ltmin) * b[1]           record is 5 bytes
0a  A = b[0] + spd_flipper_lsb                record is 4 bytes
0c  A = b[(lev - ltmin) & 1]                  record is 5 bytes
```

Each parameter is a list of records, each record guarded by a level range, and
a record whose range does not match is skipped; a `00` opcode terminates and
yields zero. **In Logo that is one procedure of about thirty lines over one
list**, and it produces every one of the twenty-eight per-level globals
exactly.

The parameters, in the order the pointer table gives them:

| global | what it controls | § |
|---|---|---|
| `pulsar_fire` | pulsars may shoot from level 60 | 9.4 |
| `flip_top_accel` | rim flip speed multiplier, 2 then 3 | 9.3 |
| `shot_holdoff` | enemy reload, 80 falling to 10 | 10 |
| `enm_shotmax` | enemy shots on screen, 1 → 3 | 10 |
| `min_flippers` / `max_flippers` | 1/4 early, 0/5 late | 9.5 |
| `min_pulsars` / `max_pulsars` | none below 17, then 3 and up to 5 | 9.5 |
| `min_tankers` / `max_tankers` | 0/0 then 1/1 rising to 1/3 | 9.5 |
| `min_spikers` / `max_spikers` | 0 until level 4, 2/4 mid-game | 9.5 |
| `min_fuzzballs` / `max_fuzzballs` | from level 11, up to 4 | 9.5 |
| `$0157` | the well position several checks compare against | — |
| `pulse_beat` | 4, 6, 8 — the pulse rate | 9.4 |
| `tanker_load+2`, `+3` | the bag the tanker's load is drawn from | 9.4 |
| `max_enm` | **6, always** | 9.1 |
| `wave_enemies` | 10 at level 1 to 61 at level 99 | below |
| `wave_spikeht` | the starting spike height | 9.4 |
| `flipper_move` | which of the nine p-code programs | 9.2 |
| `fuzz_move_flg`, `fuzz_move_prb` | fuseball steering | 9.4 |
| `spd_flipper_lsb` … `spd_fuzzball` | the five speeds | 10 |
| `enm_shotspd_lsb` | enemy shot speed — an `0a` record, so it is **$c0 *plus* the flipper's speed**, not a constant | 10 |

`wave_enemies` is the campaign in one line — a `04` table for levels 1–16
(10, 12, 15, 17, 20, 22, 20, 24, 27, 29, 27, 24, 26, 28, 30, 27), then five
linear ramps: 20 +1/level for 17–26, flat 27 for 27–39, 29 +1 for 40–48, 31 +1
for 49–64, 35 +1 for 65–80, 43 +1 for 81–99. **Sixty-one enemies in a wave at
level 99, seven at a time.**

**Difficulty settings are three lines on top of it** ($9328): *easy* drops
`enm_shotmax` by one, slows the flipper by a ninth and drops `flip_top_accel`
below level 17; *hard* raises `enm_shotmax` (capped at 3), speeds the flipper,
raises `wave_enemies` by an eighth and turns on `pulsar_fire` everywhere.
Ported, because it is nine lines and it is the operator setting the cabinet is
remembered for.

## 14. Colour

**This port is in colour, and that is a break with the other three games in the
tree.** Asteroids, Battlezone and Berzerk are white on black; Berzerk went to
three colours only at its M6 and only because the ROM's attribute logic forced
it. Tempest is a colour game in a way none of those are — the level's colour
scheme is *how you know which sixteen you are on*, the summary calls out one
band that is deliberately invisible (65–80, *"black"*), and a monochrome
Tempest is a different game.

The colour tables are **not** in the disassembly — they live in the colour RAM
the vector generator reads — so the scheme is authored, one palette per block
of sixteen levels, keyed off `(level - 1) / 16` exactly as `lev_remap` is keyed
off `(level - 1) & 15`. Six bands to level 99, and the fifth (65–80) is the
invisible one: the well is drawn in the background colour and only the objects
show.

The cost is one `setpc` per group of strokes, which
[Berzerk §13.1](berzerk-design.md#L1351) measured as free once the figures stop
being per-pixel. The frame draws in four groups — well, spikes, enemies,
player and shots — so it is **four `setpc`s a frame**, about 0.1 ms.

## 15. Sound

The cabinet has two POKEYs, eight channels, driven by a table-walking engine at
$cd0a (line 9690) over descriptor blocks starting at $cbd1. The blocks *are* in
the file as bytes; the channel-allocation masks above them ($cb91–$cbc1) are
`.chunk` but annotated with their contents, so the mapping from effect to
channel pair is readable. `sound_pulsar` ($cd06) is the one named entry point
and the others are reached by their mask byte — $2f, $6f, $7f, $9f, $af, $bf,
$3f, $cf.

Our PSG is three tone plus one noise per ear with ADSR
([sound-design.md](sound-design.md)), which is a POKEY and a half, so the
mapping is direct. **The frequencies and envelopes come off the disassembly at
M7, not out of this document** — walking the descriptor blocks is the method
[Battlezone §16.14](battlezone-design.md#L3394) used and
[Berzerk §14.1](berzerk-design.md#L1443) repeated, and it is the reason both of
those games sound right.

**No speech.** Tempest does not talk, so `say` is not adopted here.

The one sound this game cannot do without is **the pulsar's**, because it is
the warning that the pulse is coming and §9.4's triangle wave is otherwise
invisible until it kills you. It is the first effect M7 builds.

## 16. Frame budget

Estimated, not measured. Every figure is derived from board measurements of the
other three games and the derivation is stated so M0 can say which step was
wrong. [Berzerk §15.2](berzerk-design.md#L1703) was **2.8× low** because it
counted statements wrong, not because its units were wrong; that correction is
the reason §16.2 counts statements explicitly.

### 16.1 Units, at 300 MHz

From [Berzerk §15.1](berzerk-design.md#L1672), which took them from
[Battlezone §3.1](battlezone-design.md#L119) and had them confirmed on a board
to 4 %:

| unit | **300 MHz** |
|---|---:|
| arithmetic statement, a global | **~24 µs** |
| bare `repeat` iteration | **~2.4 µs** |
| drawing statement, 17 steps, pen down | **~33 µs** |
| drawing statement, 200 steps, pen down | **~63–120 µs** |
| `item` on a short list | **~8 µs** |
| `.setitem` on a short list | **~33 µs** |
| present, `splitscreen` (240 rows) | **18.70 ms** |

### 16.2 The estimate, at seven enemies and twelve shots

**Projection** (§3.2), 3 arithmetic + 2 `item` = ~130 µs a point:

| | points | ms |
|---|---:|---:|
| enemies | 7 | 0.9 |
| shots | 12 | 1.6 |
| spikes | 0 (cached, §7.3) | 0.0 |
| claw | 1 | 0.1 |
| | | **2.6** |

**Drawing:**

| | statements | ms |
|---|---:|---:|
| well: 16 rim strokes + 16 radials, long lines | 32 | 2.6 |
| well: far rim, 16 short strokes | 16 | 0.5 |
| spikes, 16 × (place + stroke) | 32 | 1.1 |
| enemies, 7 × (place 4 + 5 strokes + 5 scale multiplies) | 98 | 2.9 |
| shots, 12 × (place 2 + 1 stroke) | 36 | 1.1 |
| claw | 8 | 0.3 |
| | | **8.5** |

**Logic:**

| | ms |
|---|---:|
| enemy p-code step, 7 × ~30 statements | 5.0 |
| enemy list reads and writes, 7 × 10 | 1.5 |
| shots × enemies, worst 12 × 7 with a segment test first | 3.0 |
| spawn, pulse, timers, difficulty | 1.0 |
| player: input, move, fire, contact | 0.8 |
| HUD | 0.5 |
| | **11.8** |

**Body 22.9 ms, present 18.7 ms, frame ≈ 41.6 ms — about 24 fps.**

### 16.3 The gate

> **M0 passes if the worst frame — seven enemies, twelve shots, sixteen
> spikes, one flipper flipping — is inside 50 ms at 300 MHz.** That is 20 fps,
> and the estimate has 8.4 ms of headroom against it.

Twenty rather than fifteen, for Berzerk's reason: a game where you dodge and
aim degrades faster with frame time than one where you drive.

### 16.4 The three numbers most likely to be wrong

1. **The well's redraw, 3.1 ms.** Sixteen radial lines are the longest strokes
   in any game in this tree — ~110 steps each — and §16.1's 200-step row spans
   63 to 120 µs. If the top of that range is right, the well alone is 2 ms more
   than budgeted and §3.3's erase decision changes.
2. **The enemy's five scale multiplies.** This is the cost of not being able to
   stamp (§7), it is 0.8 ms of the drawing pass, and §7.2's three-fixed-sizes
   fallback removes it entirely. M0 prices both.
3. **The shot × enemy loop, 3.0 ms.** The only quadratic in the budget, and the
   same line Berzerk flagged and then measured at 4.0. The segment test is one
   comparison and should cut it by a factor of eight — but only if the segment
   lists are read once into the loop rather than per pair, which is a way of
   writing it and not a property of it.

## 17. Interpreter levers, priced

- **No trigonometry.** Sixteen fixed segments, a projection that is one scalar
  multiply. `sincos` buys nothing. The claw's heading comes off the authored
  `tube_angle` list, not out of `towards`.
- **`min`/`max`** are still absent and still worth having on their own merits
  (Battlezone L1). This game clamps `player_seg`, `along` and `spike_ht`.
  ~0.2 ms. Not a reason for anything.
- **Arrays (L2).** Sixteen-element lists read ~60 times a frame and seven-element
  lists ~70. Berzerk measured `item` at 8 µs fixed and `.setitem` at 33 —
  both dominated by fixed cost, so O(1) indexing saves ~0.3 ms. **Still not the
  demonstrated need**, and the mutation half of the argument (Berzerk §16's
  finding that `.setitem` of a number *interns* it) applies here with more
  force, because `along` is a continuous value written seven times a frame.
  §19's fourth ceiling is how this design answers that instead.
- **A projection operation** — Battlezone's L3, unbuilt. `(project seg along)`
  returning a two-element list would replace §16.2's 2.6 ms with perhaps 0.8,
  and it is the one lever this game has a real case for. It is also the one
  that allocates a list per call unless it outputs into globals, which is the
  design problem that stopped L3 the first time.

**Verdict: no interpreter change is needed to ship, and one is worth
considering if M0 comes in over.** That verdict is stated the way Berzerk's was
— and Berzerk's was overturned by its own M0, so it is written to be checked
rather than cited.

## 18. Reduced-resource choices

| Cut | Reason |
|---|---|
| **Two-player alternating** | Berzerk's call. `other_pl_data[18]`, `curplayer`, the score swap at $92b4 and the second `p*_level`/`p*_lives` pair are a third of the state for none of the game |
| **The 64 pending-enemy slots** | Nothing reads a slot except the drawing pass (§9.5). A count plus a short segment list draws the same scattering of dots |
| **The self-test, service mode, EAROM, coinage, the copyright checksums** | Cabinet furniture. Roughly a quarter of the ROM by line count |
| **The nine-frame hit flash and the seven-frame fuzzballed death** | §7.4's random dots, as Berzerk did |
| **The 250/500/750 score drawn at the kill point** | The three graphics at $cf1e–$cf22. The points still score; they are not drawn floating in the well |
| **The attract mode's demo play** | The attract screen is Asteroids', Battlezone's and Berzerk's — title, controls, top ten |

Kept although it is tempting to cut: **the level-select screen and its bonus**
(§12), because it is the thing a player recognises; **the p-code programs'
per-level selection** (§9.2), because it is the campaign's texture and it is
six bytes; and **`crack_speed`'s derived hit tolerance** (§10), because a
constant makes late levels unhittable.

## 19. Memory, and the four ceilings

All four are inherited, three of them found by Battlezone the expensive way.

**`MAX_PROCEDURES` is 128** and Battlezone defines exactly 128, so its file
cannot take a 129th `to` and the failure is silent — the last definition in the
file goes missing. Tempest's budget is **100 procedures**, with a test naming
the count. Berzerk shipped at 84 with more object types; this game has fewer
figures and more tables, so 100 should be comfortable.

**`MAX_GLOBAL_VARIABLES` is 254** and what matters is the *peak*. §13 alone
mints twenty-eight names, §9.1 seven lists, §3.1 four, and the campaign,
scoring and HUD perhaps thirty more. Budget **200 peak, 54 free**, enforced by
a test that **plays a game** rather than reading the source.

**`LOGO_SHAPE_MAX_SLOT` is 25** and this game should need **none of them**
(§7). If the claw becomes a costume it needs one. That is the first design in
this tree with slack on that ceiling, and it is a direct consequence of §3.2:
everything scales, so nothing stamps.

**And the one that will actually bite: a frame must not allocate.** Nothing in
this interpreter collects on demand, so a frame loop that spends storage has a
fuse on it. Two ways to light one, both easy to write here:

- `dot`, `setpos [x y]`, `list` and `se` all cons a cell. §7.3 is why the shots
  are strokes and not `dot`s, and every placement is `setx`/`sety`, never
  `setpos`.
- **`.setitem` of a number the workspace has not held before interns a word**
  ([B52](bugs.md)). `e.along` is a *continuous* value written seven times a
  frame, which is the worst case for this rule that any game in the tree has
  had — Berzerk's counters were bounded integers and it still died once
  ([B25](bugs.md), 21,000 free nodes and 20 free bytes of word table).

**So `along` is quantised.** The arcade's is a byte with an 8-bit fraction
kept separately (`enemy_along_lsb`, line 333), and this port keeps that split
for exactly this reason: the **integer** part goes in `e.along` and takes one
of 225 values, and the fraction lives in a second list quantised to 1/16, which
takes one of sixteen. The word table then settles after one pass through the
range instead of never settling. `alloc.per.frame` reports `nodes` and `atoms`
warm, with **zero as the only acceptable reading**, and the harness reports it
first because a run that cannot finish has no other numbers in it.

## 20. Milestones

Each leaves `ctest --preset=tests` green, and each ends with a board reading.

| M | What | Gate |
|---|---|---|
| **M0** | The measurement harness | The six questions of §20.1, on a Pico 2 W at 300 MHz, written to a file. **Gate: the worst frame inside 50 ms** (§16.3) |
| **M1** | The well and the claw | Sixteen shapes built from §6's generator, §3's projection, the claw on the rim, the spinner reconstruction (§8.1), open and closed wells. Nothing moves in the well yet. Gate: the well reads as depth, and a lap of the rim is smooth |
| **M2** | Shots, spikes and the descent | Eight player shots, the spike array and its two marker bits, shooting spikes down, and §11's end-of-level descent with the spike death. Gate: the descent proves the projection |
| **M3** | The flipper | §9.2's nine programs flattened, §9.3's flip and the rim grab, `crack_speed`'s speeds and tolerances, the player's death, one life lost. Gate: a flipper that flips the way level 1 flips and level 5 flips, and they differ |
| **M4** | Tankers, spikers and the wave | §9.5's three-pass spawner, `min_*`/`max_*`, `wave_enemies`, the tanker's split at `along = $20`, the spiker's growth and conversion, level clear. Gate: a full wave of level 1 is playable and finishes |
| **M5** | Pulsars, fuseballs and enemy fire | §9.4's triangle-wave pulse and the lane kill, the fuseball's edge walk, the four enemy shots with `shot_holdoff` and `enm_shotmax`. Gate: all five enemy types on one screen inside the budget |
| **M6** | The campaign | §13's table machine and all twenty-eight parameters, the 99-level cycle and the random shape from 99, §8.3's superzapper, §12's scoring, the bonus life, the level-select screen and its bonus, the HUD, the attract screen. Gate: level 1 and level 40 are recognisably different games |
| **M7** | Colour, sound and polish | §14's six palettes including the invisible band, §15's effects walked out of the descriptor blocks, the deaths, the high-score table, and the play test |

### 20.1 M0 — the harness, and what it must answer

Modelled on `tests/logo/p15m0`, writing to a file. Six questions, and the
harness is not done until each has a number:

1. **What does §3.2's projection cost?** Timed on its own, 100 points, both as
   the divide and as the 15-entry ladder. This is the number the whole design
   rests on.
2. **What does the well cost to redraw?** Thirty-two long strokes plus sixteen
   short ones, timed as a pass. §16.4's first suspect.
3. **Erase in place, or clear and redraw?** (§3.3.) Both built, at 1, 4 and 7
   enemies, body and present read apart. Berzerk's §3 inverted its own answer
   inside a day; this one is measured, not argued.
4. **What does an enemy cost to draw** with five scale multiplies, against
   three fixed sizes with the lengths baked in? (§7.2.)
5. **What does the shot × enemy pass cost** at 12 × 7, with and without the
   segment test hoisted? (§16.4's third suspect.)
6. **Does a frame allocate?** `nodes` and `atoms` warm, zero required. §19.

One board is the whole reading, deliberately: this game has no PSRAM path and
no radio, so the Pico 2 W at 300 MHz is the case, and a second board would
measure the same interpreter.

## 21. Tests

`tests/test_tempest.c`, Unity plus the mock device, mirroring
`tests/test_berzerk.c`. The game exposes `init.game`, `setup.level` and
`play.frame` as the three names a headless driver needs — the arrangement
Berzerk's §20 asked for and `test_bench_throughput` uses to take a game as a
subject.

What must be tested rather than played:

- **The well generator closes.** Sixteen shapes, each checked that vertex 16 is
  adjacent to vertex 1 on a closed well and is not on an open one (§6.2).
- **The projection is monotone.** `s(16) = 1`, `s(240) = 0.125`, and a point
  never moves outward as `along` grows.
- **§13's table machine reproduces the ROM.** All twenty-eight parameters at
  levels 1, 2, 4, 11, 16, 17, 20, 33, 49, 60, 65, 81 and 99, against values
  read out of the disassembly by hand. This is the largest test in the file and
  it is what makes the campaign a transcription rather than a guess.
- **The nine p-code programs.** Each entry point driven for twenty ticks with a
  fixed player position, asserting the flip/move sequence the byte listing at
  $a0f7 describes.
- **The superzapper's two lengths.** Nineteen ticks clears the field; five kills
  exactly one; a third press does nothing; the next level recharges (§8.3).
- **`crack_speed`.** $d4 → msb $fe, lsb $a0, tolerance 7 (§10).
- **The scoring table**, all eight indices, and the fuseball's weighted draw.
- **A frame allocates nothing**, warm, over 300 frames.
- **The procedure and global counts**, by playing a game and reading `nodes`
  and the global count, not by reading the source (§19).

## 22. Risks

1. **The well's redraw is the whole erase decision** and §16.4 says the
   estimate could be 60 % low. If it is, M0 chooses erase-in-place and §3.3's
   second bullet becomes real work: an eraser that runs along a lane line has
   to repair it.
2. **The projection may not read as depth.** Sixteen lanes converging to a
   point at 1:8 is a photograph of the cabinet, not a measurement of it, and
   §5's 110-step near radius is a guess. M1 is the earliest this can be known
   and §23 Q1 holds it open until then.
3. **The spinner.** §8.1 is the port's one forced deviation and the only one
   that touches the thing the player's hands do. Tempest is a game about
   *sweeping* the rim, and a key that repeats is not a knob.
4. **Seven enemies is not many, and the frame still might not fit.** Berzerk's
   estimate missed by 2.8× on statement count with better-understood figures
   than these. §16.2 counts 98 statements to draw seven enemies; if that is
   45 each as Berzerk's robots turned out to be, the drawing pass triples.
5. **The wells and the figures are authored**, so "faithful" has a limit this
   port cannot argue past without the vector ROM. §23 Q2.

## 23. Open questions

**Q1 — the near-rim radius and the 1:8 far ratio.** §5 sets them from the
cabinet's appearance. Neither can be judged before M1 puts a well on a screen,
and both are single constants. *Answer at M1.*

**Q2 — do we go and get `lev_x[]`?** The level geometry and every figure are in
the vector ROM, which this disassembly does not cover. If the ROM image can be
read, sixteen wells and a dozen shapes become exact and §6.2 and §7 stop being
authored. If it cannot, they stay authored and the port is faithful in
mechanism and close in picture. *This is the user's call and it changes what
M1 builds.*

**Q3 — how the spinner is reconstructed.** §8.1 proposes held-key acceleration.
The alternatives are one segment per key repeat (precise, slow) and a
continuous rim position with sub-segment resolution (smooth, and it changes
what `player_seg` means everywhere). *Answer at M1, on a board, by playing it.*

**Q4 — does colour survive the frame budget?** §14 prices it at four `setpc`s.
If M0 says the drawing pass is tight, the invisible band at 65–80 becomes free
and the other five become a question. *Answer at M0.*

**Q5 — flatten the p-code, or transcribe it?** §9.2 answers *flatten* on an
argument rather than a measurement. The measurement is cheap and M3 is where it
would be taken, but only if flattening turns out to lose behaviour the byte
listing has and the nine procedures do not. *Answer at M3.*
