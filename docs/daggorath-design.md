# Dungeons of Daggorath in Pico Logo (design)

> P17. Design drafted 2026-09-02, **gated on its own M0 measurement**.
> Sibling designs: [`battlezone-design.md`](battlezone-design.md) (P13, the
> first 3D vector game), [`berzerk-design.md`](berzerk-design.md) (P15),
> [`asteroids-design.md`](asteroids-design.md) (P11).

---

## 1. Deliverables and source of truth

| | |
|---|---|
| Game | `logo/games/daggorath` — one Logo file, no extension, no `-` or `/` in the name so `load "daggorath` parses |
| Data | `logo/games/daggdata` — the five mazes and the vector lists, read with `open`/`readlist` (§7.4, §11.2). Data, not code, so it costs no procedure slots |
| Tests | `tests/test_daggorath.c` (Unity + mock device), mirroring `tests/test_berzerk.c` |
| Design | this document |
| Measurement | `tests/logo/p17m0`, all board runs kept verbatim under [`measurements/`](measurements/). It writes its numbers **to a file**, because numbers on a display cannot be copied off it |
| Generator | `scripts/gen_daggorath.py`, host-side, output written to `logo/games/daggdata` (§7.4, §11.2) |
| Source of truth | `docs/DungeonsOfDaggorath/*.ASM` — the 1982 DynaMicro 6809 source, 9,866 lines. **Every rule in this document cites the file and routine it came from.** Where this document and the ROM disagree, the ROM is right. Licensing is not a one-liner and an earlier draft of this row got it wrong: see [`PROVENANCE.md`](DungeonsOfDaggorath/PROVENANCE.md), which transcribes the 2002 grant verbatim and says plainly that it names an individual |

| Depends on | **[P18](roadmap.md#p18--interpreter-work-for-dungeons-of-daggorath)** — five interpreter items this design asked for. Its M0–M2 (`MAX_PROCEDURES` 128 → 192, opaque `write`, `setpendash`) come **before** P17 M1; its M3 (arrays) and M4 (a sound glide) come **after** P17 M0 and M6 respectively, because those are the measurements that decide whether they are needed at all |

Play: `load "daggorath` then `daggorath`.

All three boards. Nothing here needs WiFi, TLS or PSRAM, so `LOGO_HAS_WIFI` and
`LOGO_HAS_TLS` are not consulted anywhere in the game.

**`hw.setcpu "fast` is a precondition** (§12.1). Daggorath is turn driven
rather than a frame loop, so unlike Battlezone and Berzerk it would *run* at
150 MHz — but the clock is what buys the three things this port most wants:
a `MOVE` that answers in one beat rather than two, room to afford §8's dot
fade, and a heartbeat a redraw cannot make late. The game asks for the clock at startup and **gives back
what it found** at the end, which is `logo/games/battlezone`'s `restore.clock`
and the reason [B50](bugs.md) was reachable from ordinary play before it.

## 2. What the game is, mechanically

You are in a 32 × 32 maze seen in first-person wireframe. You type commands
at a prompt. Nothing waits for you: creatures walk, your torch burns down and
your heart beats while you are still typing.

- **The dungeon is five levels of a 32 × 32 grid.** Each cell records what is
  on each of its four sides: passage, door, secret door or wall
  (`DGNGEN.ASM`). Every level is generated from a **fixed seed**, so the
  dungeon is the same in every game anyone has ever played — which is why
  Daggorath maps were traded on paper.
- **You see up to nine cells ahead** down a straight corridor. Everything is
  drawn as a flat 2D wireframe scaled by distance — there is no 3D projection
  anywhere in the ROM (§6). Creatures ahead of you, objects on the floor, and a
  creature standing in the passage immediately to your left or right (the
  "peek-a-boo", `VIEWER.ASM:PDRAW`) all draw into the same picture.
- **Light is the whole atmosphere.** Brightness falls with distance and with
  how much torch you have left. The ROM dims a line by plotting only every Nth
  pixel of it; that dot fraction is the grey level, and §8 turns it back into
  grey.
- **Your heart is the health bar.** It beats faster as you take damage and as
  you carry more. Too fast and you faint; faster still and you die
  (`HUPDAT.ASM`). It is drawn *and* audible, and the sound is the single most
  important thing in the port (§9).
- **You have two hands and a backpack.** `GET`, `PULL`, `STOW`, `DROP`, `USE`,
  `REVEAL`, `INCANT`, `EXAMINE`. Objects are unidentified until you `REVEAL`
  them, and revealing costs power you may not have.
- **Combat is one swing per command.** `ATTACK LEFT` / `ATTACK RIGHT`. Swinging
  costs you energy whether or not you connect, and in the dark you mostly miss
  (§10).
- **Creatures pick things up.** A creature's highest priority is loot, so
  dropping something buys you a turn (`CRETUR.ASM`, the routine's own header
  comment says so).
- **The way down is not always there.** Levels 1↔2 and 2↔3 have ladders and
  holes. **Level 3 → 4 has neither: the only way down is to kill the plain
  wizard** (§7.5). From level 5 there is no way back up at all.
- **You win by taking the Ring of Ohm from the crescent wizard and saying its
  name.**

## 3. The one-line summary of the port

The ROM is a **round-robin cooperative scheduler** (`COMMON.ASM`, jiffy /
tenth / second / minute queues) driving **fifteen command handlers** and a
**display-list interpreter** over **static coordinate tables**. Every one of
those three has a natural Logo shape. This is a much easier port than
Battlezone, and §6 is why.

---

## 4. The screen

### 4.1 The layout

The CoCo screen is 256 × 192 monochrome pixels: rows 0–151 are the viewer,
row 152 is the status line, rows 160–191 are the four-line text area
(`COMDAT.ASM`, `TXTEXA`/`TXTSTS`/`TXTPRI`).

`splitscreen` gives us 320 × 240 of graphics over eight text lines (24–31),
and the split present is a flat **19.8 ms at 150 MHz, 18.7 at 300**
(battlezone §12, measured on two boards). That present is the floor under
every redraw in this game.

**The mapping is a uniform 1.25×**, and it is uniform on purpose: the CoCo's
pixels in this mode are square (256 × 192 on a 4:3 screen), so a
non-uniform stretch to fill 240 rows would make every corridor 26 % too tall
and every wireframe creature wrong. 256 × 1.25 = **320 exactly**, and
152 × 1.25 = **190**.

```
 rows   0–189   GRAPHICS   the viewer, 320 x 190      (turtle y  +160 .. -28.75)
 rows 190–239   GRAPHICS   the status line: hands by `write`, heart as a turtle
 lines 24–31    TEXT       eight lines: messages and the scrolling command line
```

**Everything the CoCo drew into its bitmap is drawn into ours**, and that is
almost all of it. The CoCo screen in this game has **no text mode at all** — it
is one 256 × 192 graphics bitmap, and every character in the game is blitted
into it. `COMDAT.ASM` gives all three text control blocks bases of
`D0$BAS + n*32`, which are addresses *inside the display*, and
`PEXAM.ASM:EXAMIO` sets the `EXAMINE` block's base to `P.VDBAS`, the video
display base itself. So the status line goes in the picture, where the ROM puts
it, drawn with `write`.

**The graphics window is a 40 × 24 character grid, and that is not a
coincidence to be grateful for so much as an identity to use.** The glyph is
8 × 10 ([`devices/font.h`](../devices/font.h)), the split graphics band is
320 × 240, so `write` addresses exactly **40 columns** — the same 40 as the
text screen — and exactly **24 rows**, which is exactly the number of text rows
the split screen hides. Anything the ROM lays out on a character grid lays out
on ours unchanged, including the 19-row `EXAMINE` overlay.

The status line is therefore one `write` at turtle-space `y = -55`, with the
two hand names justified to columns 0 and 39 as `STATUS.ASM:STATUX` justifies
them to 0 and 31, and the heart between them.

### 4.1a The heart is a turtle, not a drawing

The heart alternates between two glyph pairs once per beat — small,
large, small (`COMMON.ASM:CLK32`, `I.SHL`/`I.LHL`, 16 × 8 CoCo pixels). Neither
drawing it nor `stamp`ing it is right, because both put ink in the picture and
then have to take it out again on the next beat, at the fastest rate anything in
this game moves.

**So the heart is turtle 1, wearing one of two costumes.** Two 16 × 16 shapes
via `putsh` (2 of 25 slots), parked at the centre of the status band, and a beat
is one statement:

```logo
tell 1  setsh ifelse :heart.big [2] [1]
```

The compositor draws turtles *over* the picture rather than into it, so there
is nothing to erase, nothing to repair when the viewer does its `clean` and
redraw, and no dirty-rectangle bookkeeping. It is the cheapest thing on the
jiffy path, which is what it needs to be — at a faint-threshold heart rate it
fires twenty times a second.

Two costumes rather than one at `setmag 1`/`setmag 2`, because the ROM's small
and large hearts are **different glyphs and not one glyph scaled**, and because
a magnified 16 × 16 shape is exactly at the 32 × 32 compositor limit with
nothing in hand.

`PUSE.ASM:USC210` clears `HEARTF` when a scroll puts the map up — the beat
carries on, the picture of it stops — which for us is `ht` on turtle 1 and `st`
when the view comes back.

### 4.1b Text in the picture: what `write` costs, and what erases it

Three facts about `write`, read out of the code rather than assumed, because
the whole of §4.1 rests on them.

**It is one statement per string, not per character.** `prim_write`
([primitives_turtle.c:1544](../core/primitives_turtle.c#L1544)) formats into a
256-byte *stack* buffer and hands the whole string to `screen_gfx_text`
([screen.c](../devices/picocalc/screen.c)), which blits it in one C call. So a
40-character status line costs **one interpreter statement plus 40 glyphs of
blit** — about 24 µs of statement and 15–30 µs of pixels at 300 MHz, call it
**50 µs**, against a 38.8 ms redraw (§12). It is 0.1 % of a frame, and it
allocates nothing: no cons cell, no interned word.

**It is part of the drawing.** The glyphs go into `gfx_buffer`, so `clean`
takes them with everything else — which is the answer to *when does the status
line get erased*: **it never does, because the redraw already did.** The order
is `clean`, draw the view, `write` the status, and the status line costs one
extra statement on a path that was going to run anyway. The ROM works the same
way round (`SWI STATUS` then `SWI PUPDAT`), so a status change forcing a redraw
is faithful rather than a compromise.

**And `write` ignores the pen mode.** `turtle_draw_text` passes `cur->colour`
whatever the pen state is, and `screen_gfx_text` sets only the pixels a glyph
lights — it does not clear behind them. So `pe write :old` does **not** erase,
and `write`-ing spaces over old text erases nothing either. There is exactly
one eraser for text in the picture and it is `clean`. This is a limitation
rather than a defect — `write` is documented as drawing in the current pen
colour and that is what it does — but it is the fact that decides §4.1c.

### 4.1b(i) The one thing this design cannot draw, and the primitive that would

`STATUS.ASM:STATUX` sets the status block's inverse flag to the **complement**
of `VDGINV`, so the CoCo's status row is always the opposite polarity to the
view above it: dark glyphs on a light bar where the view is light-on-dark, and
the other way round on an inverted level (§4.3). That is a **filled character
cell**, and §4.1b is the reason we cannot draw one — `setpc` colours the glyph
and nothing paints the cell behind it. §4.3's "the status line inverts for
free" is therefore half true as things stand: the glyph flips, the bar does not
exist.

**An optional-argument `write` would buy it outright**, in the parenthesised
idiom this dialect already uses for `(toot d l r)`, `(sound v f d vol)` and
`(setrefresh "sync rate)`:

| form | glyph | cell |
|---|---|---|
| `write text` | pen colour | untouched — **unchanged, and every existing program keeps it** |
| `(write text fg)` | `fg` | untouched |
| `(write text fg bg)` | `fg` | filled with `bg` |

and the status line becomes `(write :status 0 255)` on a normal level and
`(write :status 255 0)` on an inverted one — 255 being the background colour
number `setbg` already documents, which makes "erase this text" spell
`(write :old 255 255)` with no new sentinel invented. Colours follow
`setpc`/`setbg` (0–254 plus 255) rather than `settextcolor`'s `[fg bg]` list of
0–15, because `write` is a graphics primitive that draws in the pen colour and
should keep graphics conventions.

Transparency stays the default because `write`'s documented job is labelling a
picture, and an opaque default would punch a rectangle through every existing
use of it.

**It is a small change.** `primitive_register("write", 1, ...)` sets the arity
for *unparenthesised* calls only — `sound` is registered at 3 and accepts 4 —
so nothing about the registration moves; `prim_write` gains `argc == 2` and
`argc == 3` branches in the shape [B48](bugs.md) gave `prim_setpos` in August,
and `screen_gfx_text` already walks every pixel of every cell and already marks
the whole cell extent dirty, so opaque is *store unconditionally, fg or bg by
the glyph bit* — one line in the inner loop. Beyond that: the `draw_text` vtable
entry, the mock, a reference entry and tests.

**This is [P18](roadmap.md#p18--interpreter-work-for-dungeons-of-daggorath), and it goes
first** (decided 2026-09-02). It is a language item rather than a game one, so
it lives in the roadmap on its own and not inside P17 — but M1 assumes it
rather than shipping half an inversion, so P17 starts when P18's two gates are
passed. An earlier draft of this section had the game adopting it
opportunistically, the arrangement P15 §14.3 had with `say`; that is the weaker
plan here, because `say` was an addition and this is a thing the ROM does on
every level.

**And it is worth being honest about the customer count.** Of the three things
it buys, only the inverse bar is something this game cannot otherwise do; taking
`write` out of the pen's way — the redraw sets the pen twenty-odd times a frame
for §8's ramp — and updating the status line without a redraw are conveniences.
The three-argument form alone carries the case; the two-argument form falls out
of the same branch for nothing, which is the only reason to take both.

### 4.1c What stays in the text window, and why it is only one thing

The rule that falls out of §4.1b: **things that change when the picture changes
go in the picture; the one thing that changes *between* pictures goes in the
text window.**

The command line is that one thing. It changes a keystroke at a time, dozens of
times between two redraws, and in the picture each keystroke would cost either
a full redraw (39 ms per character — unusable) or an erase that §4.1b says does
not exist. The text window overwrites and scrolls for nothing, has a cursor,
and is the thing a REPL-shaped game wants. So lines 24–31 carry the messages
and the prompt, and nothing else.

**Two things get easier because the status line left.** The eight text lines are
now all one region — against the ROM's four — so the message area can simply
`print` and scroll the way the text screen already scrolls, and the "never
print onto line 31" rule from an earlier draft of this design is gone with the
status line it was protecting. What remains from that paragraph is the trap
that has bitten this tree before and still applies to anything written on
either half: nothing wraps at 40 columns, `--` inside a list literal lexes as
two words, and a `;` inside a list literal eats the rest of the procedure.

### 4.2 The three display modes are the ROM's three display modes

`DSPMOD` is a pointer to whichever routine builds the picture, and there are
exactly three: `VIEWER`, `MAPPER`, `EXAMIN`. **All three are graphics, the
screen never changes mode, and `splitscreen` is set once at startup.**

| ROM | Ours |
|---|---|
| `VIEWER` — the forward view | the picture (§6) |
| `MAPPER` — the map a scroll shows | the picture (§13) |
| `EXAMIN` — the inventory listing | `clean`, then one `write` a line |

`EXAMIN` is the one that changed most from an earlier draft, which switched to
`textscreen` and put the listing on the hidden lines 0–23. That was clever and
it was not what the ROM does. `PEXAM.ASM:EXAMIO` **clears the screen** (`SWI
ZFLOP`) and then writes text into the display base — the listing replaces the
view rather than overlaying it, and the status line and prompt stay where they
are underneath. Ours is the same two steps: `clean`, then up to 19 `write`s on
the 40 × 24 grid of §4.1, at about **0.7 ms** all told. It stays up until the
next command, exactly as `DSPMOD` does.

### 4.3 Odd levels are inverted, and that is free

**And the status line inverts with it, for free.** `STATUS.ASM:STATUX` sets the
status block's inverse flag to the *complement* of `VDGINV`, so the CoCo's
status row is always the opposite polarity to the view above it. Because our
status line is `write` in the pen colour (§4.1b) rather than text-screen text,
that is `(write :status 0 255)` on a normal level and `(write :status 255 0)` on
an inverted one, once [P18](roadmap.md#p18--interpreter-work-for-dungeons-of-daggorath)
lands — where a text-window status line would have needed `settextcolor` and
`setbg` juggled against each other.

`NEWLVL.ASM:NLVL50` sets `VDGINV` to `-(level & 1)`: internal levels 0, 2, 4 —
**displayed 1, 3, 5** — draw light on dark, and displayed 2 and 4 draw dark on
light, with the status line inverted the other way. It is the ROM's cheapest
and most memorable trick, and `setbg` + a swapped grey ramp (§8) buys it for
two statements at level entry.

---

## 5. Where the game's clock comes from

The ROM's `CLOCK` IRQ fires at 60 Hz and drives five queues: jiffy (1/60 s),
tenth (6 jiffies), second, minute, hour, plus a scheduler queue of ready tasks
(`CD.ASM:Q.JIF`…`Q.SCD`, `COMMON.ASM:CLK40`). Tasks run to completion and
reschedule themselves by returning a delay and a queue.

**We rebuild that, in Logo, as one loop.** The alternative — `when` demons —
is wrong here for a reason worth writing down: there are **eight** demon slots
(`MAX_DEMONS`) and Daggorath runs **up to 32 creature tasks at once**
(`COMCRE.ASM:CREGEN` caps the population at 32). Demons cannot express it, and
a hand-written pass over the creature table can.

```logo
to tick                       ; one pass of the scheduler
  make "now ticks
  heart.tick                  ; jiffy   — the beat, sound and picture
  creature.tick               ; tenth   — every live creature's own delay
  slow.tick                   ; second  — LUKNEW, HSLOW
  minute.tick                 ; minute  — BURNER; and CREGEN every 5
end
```

Each timer is a global holding the wall-clock `ticks` value it is next due at,
which is what the ROM's countdown fields are. `ticks` is milliseconds and
monotonic; a jiffy is 16.67 ms.

**Input is polled, never blocked on.** `readchar` waits, which would stop the
world; `key?` does not. `pollkeys` is also wrong here — it *discards* the
typed characters, and this game is a typing game.

```logo
to play
  until [:over] [tick  if key? [human rc]]
end
```

`human` is `HUMAN.ASM:HUMAN` almost line for line: echo the character, buffer
it, handle backspace, and on carriage return parse and dispatch. The ROM's
`PLAYER` task does exactly this once per jiffy.

**One consequence, and it is the good kind of faithful.** The ROM's sound
effects are blocking bit-bangers — the game task stops while a `WHOOSH` plays,
and only the IRQ (the heartbeat) keeps running. Ours block too, so §9.4 gives
the long effects a `tick` between their steps and the heart keeps its time.

---

## 6. The renderer, which is the good news

**There is no 3D in Dungeons of Daggorath.** `VIEWER.ASM` walks forward from
the player's own cell, one cell at a time, up to `RANGE` = 9, and at each step
draws a handful of **fixed 2D outlines scaled about a fixed centre** by a
scale factor read out of a ten-entry table. That is the whole renderer.

```
VIEWER:  range := 0 ; cell := player cell
  loop:  scale := NORSCL[range]                     ; VIEWER.ASM:SETSCL
         extract the cell's four wall codes         ; VIEW12
         draw left / forward / right feature        ; FLATAB, VIEW20
         draw a creature in this cell               ; VIEW30
         peek left and right for a creature         ; PDRAW
         draw a ladder / hole, else the ceiling line
         draw every unowned object in this cell
         if the forward side is not a passage: stop ; VIEW60
         step forward, range := range + 1, until range > 9
```

Compare Battlezone, which needed a projection pipeline, a view cone, a near
plane and a floor. Daggorath needs **a multiply and an add per coordinate.**

### 6.1 The scale tables, verbatim

`NORSCL`, radix-7 (÷128), indexed by range (`VIEWER.ASM`):

| range | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|---|
| `NORSCL` | 200 | 128 | 80 | 50 | 31 | 20 | 12 | 8 | 4 | 2 |
| `HLFSCL` | 255 | 156 | 100 | 65 | 40 | 26 | 16 | 10 | 6 | 3 |

Range 1 is 128/128 — **exactly 1:1** — so the coordinate tables in the ROM are
literally what you see one cell ahead. `HLFSCL` is the half-step table for the
one intermediate frame `MOVE` draws (§6.4); a backward half step reads the
same table shifted one entry along (`BAKSCL` = `HLFSCL`+1), which is a pun in
the assembler and a `+ 1` on an index here.

### 6.2 The transform, reduced to two multiplies

A vector list is a run of `y,x` byte pairs (y first — the ROM stores them that
way because y ≤ 191 leaves 192–255 free for control codes). A point is scaled
about the centroid `VCNTRX` = 128, `VCNTRY` = 76 and drawn.

Composing that with §4.1's 1.25× gives, per range step, three constants and,
per point, two multiplies and two subtractions:

```
k    = 1.25 * scale                 x = k*X - kx0
kx0  = 128 * k                      y = c - k*Y
c    = 65 + 95 * scale
```

Check: at range 1, `k` = 1.25, `kx0` = 160, `c` = 160. `X` = 0 → `x` = −160,
`X` = 255 → `x` = 158.75, `Y` = 0 → `y` = 160 (screen row 0), `Y` = 151 →
`y` = −28.75 (row 189). The centroid (128, 76) lands at turtle (0, 65).

**Keep the tables as the ROM's own bytes.** The alternative — pre-transforming
into turtle coordinates offline — costs exactly the same two multiplies at draw
time and throws away the ability to diff our tables against the assembler
source. So `logo/games/daggdata` holds `16 27 38 64 114 64 136 27` for the left
wall, which is `VARC.ASM:LWALL` and nothing else.

### 6.3 The list walk is M0's question

Drawing wants one statement per point with the pen down. `(setpos x y)` is the
right statement — the two-input form exists since B48 (2026-08-23) and does not
allocate, where `setpos list :x :y` mints two cons cells a call.

What is *not* settled is how to walk 100–250 numbers a redraw without
allocating and without an O(n²) index walk. Three candidates, and **M0 must
price all three**:

1. **`item :i :vl` with a running index.** Simplest, and quadratic: a 60-point
   creature is 120 `item` calls averaging 60 link steps each.
2. **`first` / `butfirst` down the list.** Linear, and the question is whether
   `butfirst` of a list allocates or returns the existing tail. Measure
   `nodes` and `atoms` either side of 1,000 walks, **warm** — a cold reading
   charges the first pass for interning and looks exactly like a leak.
3. **One list per polyline, `foreach`.** Reads best; costs a lambda call per
   point.

If all three are too slow there are two levers, and the roadmap already names
both. The near one is a **display-list primitive** — the ROM's own `VCTLSX` in
C, taking a list, a scale and a fade — about eighty lines of `prim_` work, and
exactly the shape battlezone §13 L4 priced and did not need. The far one is
**arrays** — [P18 M3](roadmap.md#p18--interpreter-work-for-dungeons-of-daggorath),
deferred on the roadmap since P13 for want of a demonstrated need and
**explicitly gated on this measurement**: P13 looked like that need and measured
out as not being it, so P17 does not get to assume it is. Both stay levers, not
plans, until M0 says which.

### 6.4 The two animations, which are the ROM's whole sense of motion

**Turning** (`PTURN.ASM:LRTURN`/`RLTURN`). The screen is cleared to two
horizontal lines at y = 16 and y = 136 at 1:1, and a vertical line sweeps
across it — left to right in steps of 32 for a left turn, right to left for a
right turn, twice for `TURN AROUND` — each line drawn and immediately erased.
Eight strokes. It is the reason a turn *feels* like a turn on a machine with
no rotation.

**Moving** (`PTURN.ASM:PMOVE`). One intermediate frame at `HLFSCL` before the
step, then the step and the real frame. Two redraws per `MOVE`, which is what
§12's budget is cut against. A side-step draws a single sweeping line instead.

---

## 7. The world

### 7.1 The cell

One byte, two bits per side, `00` passage `01` door `10` secret door `11` wall,
packed **N E S W** from the low bits up (`DGNGEN.ASM`). `$FF` — solid on all
four sides — means *not part of the maze*, and is how `STEPOK` and `FNDCEL`
know where you may not stand.

### 7.2 The maze is generated from a fixed seed, and we ship the result

`DGNGEN` seeds a 24-bit LFSR from `LVLTAB` — `$73 $C7 $5D $97 $F3` for the
five levels — carves **500 cells** with a random-walk that refuses to clear a
2 × 2 block, walls in everything it did not carve, then punches **70 doors and
45 secret doors**.

**We do not run that on the board, and the reason is arithmetic.** `RANDOM.ASM`
is a 24-bit shift register whose feedback is the parity of four taps, shifted
**eight times per byte returned**. In Logo that is ~9 primitive calls a step,
72 a byte, **≈ 1.75 ms even at §12.1's 300 MHz** — and a level generation needs
several thousand of them on top of the carve itself. Eight to fifteen seconds
per `CLIMB` is not a port, it is a punishment.

So `scripts/gen_daggorath.py` implements `RANDOX` and `DGNGEN` exactly and
emits all five mazes into `logo/games/daggdata`. This is **more** faithful, not
less: the shipped mazes are bit-identical to the 1982 ones, and a Daggorath map
drawn on paper in 1983 still works.

**Everything else stays random at run time,** and that is also faithful:
`DGEN90` spins the generator by the seconds counter before anything else uses
it, so creature positions, object placement, movement and combat rolls were
never reproducible. Those use Logo's `random`.

### 7.3 Which is also why all the loot starts on monsters

`ONCE.ASM:CINI40` creates every object in the game from `OMXTAB`, marks each
one **creature-owned**, and `NEWLVL.ASM:NLVL40` hands them round-robin to the
living creatures of their level. Nothing is lying on the floor at the start of
a game. You get gear by killing things or by finding what a dead thing dropped.

The distribution walks *down* from a start level and wraps: a count of 6
starting at level 1 puts one each on 1, 2, 3, 4, 5 and then 1 again
(`CINI44`, `CMPB #5 / BLE`).

### 7.4 The data file

`logo/games/daggdata`, read once at `daggorath` with `open` / `setread` /
`readlist`, holds:

- **the five mazes**, 32 lines of 32 numbers each — 5,280 nodes and ~256
  distinct interned numbers, which is nothing against a 32,752-cell pool and a
  32 KB word table;
- **the vector lists** (§11.2), flattened out of the ROM's relative-nybble
  encoding by the generator;
- **the vertical features table** (§7.5) and the four small stat tables of §11.

It is data, not code, so it costs **no procedure slots** — and §14 says why
that is the binding constraint in this port.

### 7.5 The ladders and the holes, and the level 3 wall

`VFTTAB` (`COMCRE.ASM`) is a sequence of `kind,row,col` triples separated by
`-128` markers, and the same physical group serves as level *N*'s way **down**
and level *N+1*'s way **up**. `PCLIMB.ASM` then applies the rule that makes
Daggorath Daggorath: **`CLIMB DOWN` accepts a hole or a ladder; `CLIMB UP`
accepts a ladder only.**

Reading the table out gives the level graph:

| | down from | how |
|---|---|---|
| 1 → 2 | ladders at (0,23) and (28,30), holes at (15,4) and (20,17) | up: the two ladders |
| 2 → 3 | ladder at (2,3), holes at (3,31), (19,20), (31,0) | up: the one ladder |
| **3 → 4** | **nothing** | — |
| 4 → 5 | holes at (0,31), (5,0), (22,28), (31,16) | **up: nothing — they are all holes** |

**Level 3 has no way down at all.** The only route to level 4 is
`PATTK.ASM:ENDGAM`: kill the plain wizard — the single `WIZ0` on level 3 — and
he strips you of everything but what is in your hands and your torch, hangs a
**200-unit weight penalty** on you, and drops you on level 4 at a random cell.
And once you are on level 5 you cannot climb out, because every feature between
4 and 5 is a hole.

None of that is a special case in the code. It is four rows of a table and one
comparison, and it produces the whole shape of the game.

---

## 8. Light, and the fade — dots by default, grey by switch

`VCTLST.ASM:SETFAX`:

```
A := (magic? MLIGHT : RLIGHT) - 7 - RANGE
A >= 0   ->  full brightness
A <= -7  ->  invisible, draw nothing
else     ->  VCTFAD := BITMSK[8+A]        ; 1, 2, 4, 8, 16, 32, 64
```

and `VECTOR.ASM` then plots **one pixel in every `VCTFAD`+1** along the line.
On a one-bit display that dot fraction *is* the grey level. So:

| `A` | dot fraction | grey |
|---:|---:|---:|
| ≥ 0 | 1/1 | 255 |
| −1 | 1/2 | 128 |
| −2 | 1/3 | 85 |
| −3 | 1/5 | 51 |
| −4 | 1/9 | 28 |
| −5 | 1/17 | 15 |
| −6 | 1/33 | 8 |
| −7 | 1/65 | 4 |
| ≤ −8 | 0 | — draw nothing |

**The `VCTFAD` column is the whole of the fade, and P18 M2 gives it to us
verbatim.** `setpendash (VCTFAD + 1)` plots one pixel in every *n* along a
stroke, which is `VECTOR.ASM:VECT30`'s `DEC FADCNT / BNE VECT40` and nothing
else. So the ROM's fade is **one statement per vector list**, a dotted stroke
costs the same as a solid one, and the numbers in the middle column above go
straight into the primitive.

**Dots are therefore the default.** An earlier draft of this section had it the
other way round for a reason that no longer holds — see §8.1.

### 8.1 Grey is the option, and it is still worth having

The right-hand column is the same table read as luminance: on a one-bit display
a dot fraction *is* a grey level, so `setpalette` on eight entries and a `setpc`
per vector list reproduces the same fade as a solid stroke. On an inverted level
(§4.3) the ramp is `255 - v` and the background is white.

**Why keep it, now that dots are free?**

The CoCo's output was composite video into a television, and a
one-pixel-on-four-off pattern on that display did not read as dots — it read as
a dim line, because the encoder and the phosphor did the averaging the table
describes. Grey is a faithful rendering of *what the player saw*, and on our
panel it is exact where the CoCo's was approximate.

**Why it is not the default.** Our display is a sharp 320 × 320 LCD at 1.25×, so
a CoCo pixel is 1.25 of ours and the gaps do **not** blur away. At that scale
the pattern reads as texture: the far end of a corridor breaks into speckle and
things at the edge of the torchlight shimmer rather than dim. That texture is
part of how the game is remembered, and no grey level reproduces it. With P18 M2
it also costs nothing, and a faithful port does not pay nothing for the
approximation when the real thing is the same price.

So both ship, `make "dagg.fade` chooses, and the seam is one procedure:

```logo
to stroke :x0 :y0 :x1 :y1 :fad
  ifelse :dots [setpendash :fad + 1] [setpc item :fad + 1 :greys]
  pu (setpos :x0 :y0) pd (setpos :x1 :y1)
end
```

Two statements either way, no DDA in Logo, no `dot` (which takes a list and
conses two cells a call), and **nothing allocated in either mode**.

### 8.2 What the fade used to cost, kept because it is why P18 M2 exists

Before the dashed pen, dot mode meant walking the line in Logo: six statements
a dot — two accumulator updates and four to lay a 1.25-pixel dash — against one
statement for a whole solid stroke.

| torch | light | dots in a nine-deep corridor | added to a redraw at 300 MHz |
|---|---:|---:|---:|
| Solar (fresh) | 13 | ~22 | ~3 ms |
| Lunar (fresh) | 10 | ~90 | ~13 ms |
| Pine (fresh) | 7 | ~410 | ~58 ms |
| Pine (dying) | 2 | ~600 | ~85 ms |

**Free when your torch is good and dearest exactly when it is dying** — the
moment the game is at its most tense — against a 100 ms redraw budget (§12) that
the bottom row missed. That shape is why this section once carried three
fallbacks and a milestone gated on a photograph, and it is the whole case for
P18 M2: **a five-line change in `screen_gfx_line` deletes every row of that
table.** The numbers are kept here so that the case does not have to be made
again, and so that a board which somehow cannot take the primitive has a costed
fallback to retreat to.

### 8.3 Two light channels, and they matter

`RLIGHT` lights ordinary things;
`MLIGHT` lights magical ones — secret doors, magical creatures, and objects,
which `VIEWER.ASM:VIEW52` deliberately draws **twice**, once under each. A
magical torch shows you secret doors that regular light will not. The player's
base light is 0 (`PRLITE`), so **without a lit torch you see nothing at all**,
and `PUPDAT.ASM:PSUB10` adds the burning torch's two values on top.

Torches, from `DTABAS.ASM:XXXTAB`:

| torch | minutes | regular | magic |
|---|---:|---:|---:|
| SOLAR | 60 | 13 | 11 |
| LUNAR | 30 | 10 | 4 |
| PINE | 15 | 7 | 0 |
| DEAD | — | 0 | 0 |

`COMPLR.ASM:BURNER` runs once a minute, decrements the timer, and **clamps
each light value down to the timer** as it falls — so a SOLAR torch begins
dimming with thirteen minutes left, and dies at five.

---

## 9. Sound

The ROM's sound is 623 lines of bit-banged 6-bit DAC: pitch sweeps, noise
trains, detuned pairs through decay envelopes, and noise through an
attack/decay envelope (`SOUNDS.ASM`). We have a PSG with eight voices, ADSR per
voice, four tone waveforms and two noise waveforms — which is a **better**
instrument than the one it was written for, and the job is to use it to make
the same noises, not nicer ones.

### 9.1 Voice allocation

| voices | job |
|---|---|
| 0, 4 | **the heartbeat, reserved.** Nothing else ever touches them, because `sound` on a busy voice flushes it and the beat must not be cuttable |
| 1, 5 | tone A — sweeps, squeaks, the first of a detuned pair |
| 2, 6 | tone B — the second of a detuned pair |
| 3, 7 | noise — rattles, whooshes, thuds, explosions |

Both ears always get the same thing. **There is no panning, and this is
settled rather than deferred** (§19): a creature that announces which side it
is on tells you something the CoCo did not, and in a game whose whole tension is
not knowing what is in the dark, that is not a small addition. The peek-a-boo
(§6) is the ROM's own way of saying *something is beside you*, and it says it
with a shape at the edge of the picture, which is as much as the player is
entitled to know.

### 9.2 Deriving the frequencies

The ROM has no frequencies in it — it has delay counts. `SNSQK2` toggles the
DAC between full and zero either side of `SNWAIT X`, and counting the 6809's
cycles gives a half period of ≈ 62 + 8X and ≈ 51 + 8X, so at the CoCo's
0.895 MHz E clock:

```
f  ≈  895000 / (113 + 16 X)          duration of a sweep from Xa to Xb
                                     ≈ Σ (113 + 16 X) / 895000
```

which turns every sweep in the file into a frequency range and a duration:

| effect | used by | ROM | derived | duration |
|---|---|---|---|---|
| `SQUEAK` | spider | X 32 → 1 | 1.4 → 6.9 kHz rising | 13 ms |
| `MSQUEK` ×10 (`PHASER`) | ring attack | X 64 → 1 | 0.79 → 6.9 kHz ×10 | 450 ms |
| `MSQUEQ` ×4 (`GLUGLG`) | flask | X 128 → 1 | 0.41 → 6.9 kHz ×4 | 655 ms |
| `WHOOP` | scroll | X 256 → 1 | 0.21 → 6.9 kHz | 620 ms |
| `BEOOP` | blob | X 1280 → 2048 step 48 | 43 → 27 Hz falling | — |
| `RATTLE` | viper | 10 noise bursts, silence between | white noise | — |
| `PSSST` / `PSSHT` | scorpion / wraith | 3 and 2 bursts | white noise | — |
| `GROWL`/`GRAWL`/`SNARL` | giant 1 / giant 2 / balrog | noise, ramped attack then decay, 3 rates | noise + ADSR | — |
| `CLANG`/`KLANK`/`KKLANK`/`CLANK` | shield / knight 1 / knight 2 / **being hit** | two detuned tones, decay | two tone voices | — |
| `KLINK` | **hitting a creature** | high tone + noise, instant attack, short decay | tone + noise | — |
| `WHOOSH` | sword | noise, fast attack, slow decay | noise + ADSR | — |
| `CHUCK` | torch lit | noise, decay only | noise + ADSR | — |
| `BDLBDL` | wizard | 8 random squeaks, then `KABOOM` | — | — |
| `THUD` / `BANG` / `KABOOM` | wall / creature death / — | descending noise "boomer" | noise sweep | — |

**These are derived, not measured, and M6's gate is a listening test** against
a recording of the original. The derivation is here so that when a number is
wrong there is something to correct rather than something to guess again.

The detuned pairs are the ROM's own bytes — `CLANG` $64/$24, `KKLANK` $32/$12,
`KLANK` $AF/$36, `CLANK` $19/$09, through the same `f = 895000/(113+16X)` — and
they are why a knight and a shield sound related but not the same.

### 9.3 Volume is already in the ROM

`CRETUR.ASM:CWLK20` plays a creature's sound when it moves within
`max(|Δrow|,|Δcol|) ≤ 8` **and** `min ≤ 2` of you, half the time, at volume
`255 − 31 × distance`. Ours is `(sound [1 5] f ms (255 - 31*d)/17)`, which puts
it on the PSG's 0–15 scale. **This is the game's sonar** — it is how you know
something is coming down the corridor before you can see it — and it is worth
getting exactly right.

### 9.4 The heartbeat

`COMMON.ASM:CLK30`, once per jiffy: count down `HEARTC`; at zero reload it from
`HEARTR`, flip the one-bit speaker, and toggle the heart glyph between small
and large.

```
HEARTR  =  (64 * PPOW) / (PPOW + 2 * PDAM)  -  19      jiffies between beats
```

At the start (`PPOW` 160, `PDAM` 0) that is 45 jiffies — **750 ms, 80 beats a
minute**. Half-damaged it is 13 jiffies. At `HEARTR ≤ 3` you faint; you come
round above 4; and you die when `PDAM > PPOW`.

Ours is a short low thump on voices 0/4 with a percussive envelope, and the
same tick redraws the heart at its other size. **The beat is the game's clock
and the player's health bar at once**, and every long sound effect in §9.2
calls `tick` between its steps so the beat keeps time through it — which is
what the CoCo's IRQ did for free while the sound routine blocked the game.

Fainting is a set piece worth keeping (`HUPDAT.ASM:HUPD30`): the light is
walked down one step at a time with a full redraw at each, until the screen is
black. Waking up walks it back.

### 9.5 The wizard speaks — a deliberate departure

`say` shipped with P16 and `PATTK.ASM` and `PINCAN.ASM` contain three
speeches: *"ENOUGH! I TIRE OF THIS PLAY…"*, *"PREPARE TO MEET THY DOOM!"* and
*"BEHOLD! DESTINY AWAITS THE HAND OF A NEW WIZARD…"*, plus the death message
*"YET ANOTHER DOES NOT RETURN…"*.

The CoCo printed them. **We propose to print them *and* speak them**, with
`setvoice` pitched low, because a wizard fading in out of the dark to tell you
he is bored of you is the one moment in this game that wants a voice. It is an
addition and is flagged as one: `make "dagg.voice "false` turns it off, and the
text is unchanged either way.

---

## 10. The numbers, extracted

Every table below is transcribed from `DTABAS.ASM` and is what the port must
reproduce.

### 10.1 Creatures

Delays are in tenths of a second (`Q.TEN`). Offence and defence are radix-7
percentages: **defence is a filter, so lower is better armour.**

| # | creature | move | attack | mag off | mag def | phys off | phys def | power |
|---:|---|---:|---:|---:|---:|---:|---:|---:|
| 0 | Spider | 2.3 s | 1.1 s | 0 | 255 | 128 | 255 | 32 |
| 1 | Viper | 1.5 | 0.7 | 0 | 255 | 80 | 128 | 56 |
| 2 | Stone Giant (club) | 2.9 | 2.3 | 0 | 255 | 52 | 192 | 200 |
| 3 | Blob | 3.1 | 3.1 | 0 | 255 | 96 | 167 | 304 |
| 4 | Knight I | 1.3 | 0.7 | 0 | 128 | 96 | 60 | 504 |
| 5 | Stone Giant (axe) | 1.7 | 1.3 | 0 | 128 | 128 | 48 | 704 |
| 6 | Scorpion | 0.5 | 0.4 | 255 | 128 | 255 | 128 | 400 |
| 7 | Knight II | 1.3 | 0.7 | 0 | 64 | 255 | 8 | 800 |
| 8 | Wraith | 0.3 | 0.3 | 192 | 16 | 192 | 8 | 800 |
| 9 | Balrog | 0.4 | 0.3 | 255 | 5 | 255 | 3 | 1000 |
| 10 | Wizard (plain) | 1.3 | 0.7 | 255 | 6 | 255 | 0 | 1000 |
| 11 | Wizard (crescent) | 1.3 | 0.7 | 255 | 6 | 255 | 0 | 8000 |

Populations, `COMDAT.ASM:CMTTAB`, by displayed level:

| level | population |
|---|---|
| 1 | 9 spiders, 9 vipers, 4 stone giants I, 2 blobs |
| 2 | 2 spiders, 4 vipers, 6 blobs, 6 knights I, 6 stone giants II |
| 3 | 4 blobs, 6 stone giants II, 8 scorpions, 4 knights II, **1 plain wizard** |
| 4 | 8 scorpions, 6 knights II, 6 wraiths, 4 balrogs |
| 5 | 2 each of the first seven, 4 knights II, 4 wraiths, 8 balrogs, **1 crescent wizard** |

`COMCRE.ASM:CREGEN`, every five minutes: if the level holds fewer than 32
creatures, add one to the count of a random type in 2–9. **It increments the
table, not the dungeon** — the creature appears the next time you enter the
level. Subtle, cheap, and exactly why coming back up is a bad idea.

### 10.2 Objects

| object | class | reveal | mag off | phys off | first level | count | special |
|---|---|---:|---:|---:|---:|---:|---|
| Supreme ring | ring | 255 | 0 | 5 | 4 | 1 | 3 charges → **Final** |
| Joule ring | ring | 170 | 0 | 5 | 3 | 1 | 3 → Energy |
| Elvish sword | sword | 150 | 64 | 64 | 3 | 1 | |
| Mithril shield | shield | 140 | 13 | 26 | 3 | 2 | filters 64 / 64 |
| Seer scroll | scroll | 130 | 0 | 5 | 2 | 3 | map **with** creatures and objects |
| Thews flask | flask | 70 | 0 | 5 | 2 | 3 | **+1000 power** |
| Hoth ring | ring | 52 | 0 | 5 | 1 | 1 | 3 → Ice |
| Vision scroll | scroll | 50 | 0 | 5 | 1 | 3 | map, walls only |
| Abye flask | flask | 48 | 0 | 5 | 1 | 6 | **+80 % of power as damage** |
| Hale flask | flask | 40 | 0 | 5 | 1 | 4 | **heals all damage** |
| Solar torch | torch | 70 | 0 | 5 | 1 | 4 | 60 min, 13 / 11 |
| Bronze shield | shield | 25 | 0 | 26 | 1 | 6 | filters 96 / 128 |
| Vulcan ring | ring | 13 | 0 | 5 | 0 | 1 | 3 → Fire |
| Iron sword | sword | 13 | 0 | 40 | 0 | 4 | |
| Lunar torch | torch | 25 | 0 | 5 | 0 | 8 | 30 min, 10 / 4 |
| Pine torch | torch | 5 | 0 | 5 | 0 | 8 | 15 min, 7 / 0 |
| Leather shield | shield | 5 | 0 | 10 | 0 | 3 | filters 108 / 128 |
| Wooden sword | sword | 5 | 0 | 16 | 0 | 4 | |

Weights by class: flask 5, ring 1, scroll 10, shield 25, sword 25, torch 10.

The four incantable rings become **Fire**, **Ice**, **Energy** (255 / 255,
three charges, and `PATTK.ASM` guarantees a ring always hits) and **Final** —
the Ring of Ohm, which ends the game. A spent attack ring turns into a **Gold**
ring, which is worthless and weighs one unit and is the ROM's little joke.

`PINCAN.ASM` requires the **whole word, not an abbreviation** (`FULFLG`), which
is the only place in the parser that does.

### 10.3 Combat, exactly

`PATTK.ASM:ATTACK` — to hit:

```
n     = number of times attacker.pow subtracts from 4*(def.pow - def.dam)
index = max(0, 15 - n)
bonus = 10*(index-3)  if index >= 3   else  -25*(3-index)
hit   = random(0..255) + bonus >= 127
```

which runs from 97 % against something nearly dead to 21 % against something
fifteen times your size.

`DAMAGE` — how much:

```
def.dam += (att.pow * att.magoff/128) * def.magdef/128
        +  (att.pow * att.physoff/128) * def.physdef/128
```

The player's unshielded filters are 128/128; a shield in **either** hand
replaces them with its own if they are lower, and the better of the two hands
wins (`CRETUR.ASM:SHIELD`).

Costs, all of them in damage to yourself:

| action | cost | source |
|---|---|---|
| a step in any direction | `weight/8 + 3` | `PTURN.ASM:PMOV90` |
| **a step into a wall** | the same, plus a `THUD` | `PMOV90` runs after `PSTEP` fails — §19.3 |
| a swing | `power * (magoff+physoff) / 1024` | `PATTK.ASM:PATT10` |
| recovery | `damage -= damage/64`, once per heartbeat | `COMPLR.ASM:HSLOW` |

**Swinging the Elvish sword costs eight times what the wooden one does**, which
is the whole economy of the game in one line.

And the dark: if you have no torch or a dead one, `PATT22` throws away three
hits in four. Rings are exempt.

Killing something gives you **an eighth of its power**, capped near 32,767, and
drops everything it was carrying at its feet.

---

## 11. The picture data

### 11.1 What is in the ROM

| file | contents | approx points |
|---|---|---:|
| `VARC.ASM` | 12 architectural outlines — left/forward/right × passage/door/secret door/wall — plus the two peek-a-boo marks | ~90 |
| `VERT.ASM` | ladder up, ladder down, hole up, hole down, ceiling line | ~50 |
| `VOBJ.ASM` | the six object outlines seen on the floor | ~40 |
| `D3.ASM` | spider, scorpion, blob, wraith, balrog, both stone giants | ~350 |
| `D4.ASM` | both knights, the viper, and the three wizards | ~380 |

Roughly 900 points, 1,800 numbers, ~1,900 nodes and a few hundred interned
values. Comfortable.

**The look-*down* views do not exist**, and that is not our omission:
`missing-macros.asm` defines `DFLASK`…`DTORCH` as zero with the note that the
definitions were *"removed before ROM went to Radio Shack for production"*. The
1982 game shipped without them. So does this one.

### 11.2 The generator flattens the encoding

The ROM stores a list two ways at once. Absolute pairs are `y,x` bytes.
`SVORG y,x` starts a **relative run**, in which each subsequent byte is two
signed 4-bit nybbles — high is Δy, low is Δx — each **doubled** and added to
the running point (`missing-macros.asm`, `VCTLST.ASM:VCTREL`). `V$NEW` ($FF)
lifts the pen, `V$END` ($FE) ends, and `V$JSR`/`V$JMP`/`V$RTS` let one list
call another — which the ladder lists use.

`scripts/gen_daggorath.py` interprets all of that and emits **flat absolute
polylines**: a list of runs, each run a flat `y x y x …` of bytes, each run one
pen-down stroke. The Logo side then has one loop and no control codes at all.

Two things the generator must also do, because they are the only way to know
the tables were read right:

- **inline the `V$JSR` targets** (`FLUP` = `LADDER` then `HOLEUP`);
- **emit a reference render**, an ASCII or PNG dump of every list at range 1,
  checked in beside the design, so a wrong nybble is visible rather than
  merely present.

---

## 12. The budget

Daggorath does not have a frame rate. It has **redraws**, and they happen when
you act (`PUPDAT`) or when a creature moves within eight cells of you and
`LUKNEW` picks it up, at most **twice a second** (`COMPLR.ASM:LUKNEW`,
`SCHED$ 3,Q.TEN`).

Predicted from battlezone §12's measured units on a Pico 2 W — 48.5 µs a
statement and 0.98 µs a pixel of stroke at 150 MHz, **2.059× on interpretation
at 300** (measured, and the flat term minus the present moved 2.05× with it),
and a split present of 19.8 ms falling to 18.7:

| | statements | 150 MHz | **300 MHz** |
|---|---:|---:|---:|
| the cell walk, 10 ranges × the wall/creature/object logic | ~250 | 12.1 | **5.9** |
| ~110 points drawn, at 5 statements each (walk, ×2, −2, `setpos`) | ~550 | 26.7 | **13.0** |
| stroke pixels, ~2,500 | | 2.5 | **1.2** |
| the status line, one `write` (§4.1b) | 1 | 0.1 | **0.05** |
| **present** | | 19.8 | **18.7** |
| **a redraw** | | 61.1 | **≈ 38.8 ms** |
| **a `MOVE`** — the half step and the step (§6.4) | | 122 | **≈ 78 ms** |
| a redraw with §8's dots, **once P18 M2 lands** | | | **≈ 38.8** — a dashed stroke is a stroke |

**The gate: a worst-case redraw under 100 ms, measured at 300 MHz**, with the
150 MHz figure taken alongside it so the ratio can be checked against
battlezone's. Worst case is a nine-deep corridor with a creature in the near
cell, a creature peeking on each side, and three objects on the floor — that is
what `tests/logo/p17m0` builds.

**§6.3 is the number most likely to be wrong**, by a lot in either direction: a
quadratic `item` walk would triple the 13.0, and a display-list primitive would
delete it. Which is why M0 measures the walk before anything is built on it.

### 12.1 The clock is a precondition

The 300 MHz clock is not here to make a frame rate — there is no frame rate. It
is here for three things, in order:

1. **A `MOVE` at 78 ms instead of 122.** The CoCo did the same two redraws in
   about two 60 Hz frames. 78 ms reads as *the game answered*; 122 reads as
   *the game thought about it*. That difference is the whole feel of a game
   whose only verb is typing a command and watching the corridor change.
2. **Headroom that is not spent before the game is written.** §12's 38.8 ms is
   a prediction with §6.3's list walk inside it, and §6.3 is the number most
   likely to be wrong. At 150 MHz a redraw that came in at twice the estimate
   would be over budget; at 300 it would still fit. The clock is the margin on
   the one figure this design cannot yet stand behind.
3. **The scheduler's resolution.** §5 polls the keyboard and services up to 32
   creature tasks between redraws; the heartbeat wants jiffy resolution
   (16.7 ms) and a redraw is the one thing that can make it late. Halving the
   redraw halves the worst jitter on the beat.

The mechanics, all of them already solved in this tree: the game calls
`hw.setcpu "fast` at startup and **restores what it found** on exit, which is
`logo/games/battlezone`'s `restore.clock` — a session already fast stays fast,
and a game does not leave the machine changed rather than merely played on.
`hw.setcpu` silences sound and rebuilds the wireless bus as it switches, both
of which settle before it returns, so the call goes **before** the title
screen makes any noise. [B50](bugs.md) — `fast` not retuning the PSRAM QMI
timing, which corrupted the *editor* after a session at 300 MHz — was fixed
2026-08-23 and confirmed on a Plus 2 W, so a Plus 2 W is safe to edit on after
playing.

**What it costs.** 300 MHz is outside the RP2350's ratings and whether a
particular chip takes it is a property of that chip. Every board in this tree
has, and battlezone §12.3 measured the die going 24.3 → 26.9 °C over a
200-frame run, so thermals are not a constraint. **A board that refuses the
clock refuses the game**, and the game says so rather than quietly running at
half speed — the one line of `catch` that turns a hardware fact into a
sentence. §18 carries it as the risk it is.

---

## 13. The map

`MAPPER.ASM` paints the 32 × 32 grid, white for cells that cannot be occupied
and black for the rest, plus marks for objects (Seer scroll only), creatures,
vertical features and you. On our 320 × 240 band a cell is 10 × 7 pixels.

The naive version is 7,168 strokes and takes a second. The version to write is
`setpensize 7` and **one horizontal stroke per run of consecutive wall cells in
a row** — about 200 strokes for a real level, well under 20 ms. The marks are
short strokes at the cell centre, and the colours become greys: wall 255,
object mid, creature bright, you brightest.

A Vision scroll shows walls only; a Seer scroll shows everything
(`PUSE.ASM:USC100`/`USC200`, `MAPFLG`). And while the map is up the heart
**stops being drawn** (`CLR HEARTF`) though it keeps beating — the ROM reuses
the status line, and we simply stop redrawing the heart. The map stays up until
your next command.

---

## 14. The procedure table is the binding constraint

`MAX_PROCEDURES` is **128** today and becomes **192** at
[P18 M0](roadmap.md#p18--interpreter-work-for-dungeons-of-daggorath), which this
design asked for and which goes first. `logo/games/battlezone` defines exactly
128 and had to be held there. Overflow silently drops **the last `to` in the
file** rather than the one you just added, so the guard matters whatever the
number is. Daggorath is a bigger *program* than Battlezone even though it is a
smaller *frame*: fifteen commands, a parser, a scheduler, a renderer, creature
AI, seventeen sound effects and an endgame.

A first sketch comes to ~105. Against 128 that was not comfortable; against 192
it is — **and the three rules below still apply**, because the sketch is a
sketch, and because the shape they produce is better anyway: the ROM already
keeps its sounds and its commands in tables, and there is no reason to unpack
them into procedures merely because there is now room.

1. **Tables are data, not procedures** (§7.4). The mazes alone would be eight
   procedures if they were code.
2. **The sound effects are one dispatcher over a table**, not seventeen
   procedures — which is what `SOUNDS.ASM:SNDTAB` already is.
3. **The command handlers are a dispatch table**, which is what
   `DTABAS.ASM:DISPAT` already is; `run` over a table row costs one procedure,
   not fifteen.

`test_the_game_fits_the_procedure_table` is written **at M1**, not at the end,
and it names the count the way battlezone's does — measured **in play**, not at
load, because the globals raise found fifty names that are minted the first time
a procedure runs rather than by a top-level `make`.

The global table (254 slots) is the better-known budget and is not expected to
bind: battlezone peaks at 237 because it puts every hot-path temporary in the
flat namespace to buy 1.31× on the frame, and Daggorath has no frame to buy.

---

## 15. Milestones

Each closes on a gate that can be checked without a board unless it says
otherwise.

**M0 — the harness, and the three questions.**
`tests/logo/p17m0`. Builds the worst-case scene of §12 out of hand-written
tables and times it 200 times, reading the walk, the transform, the strokes and
the present apart from one another, into a file. Answers: (1) which of §6.3's
three list walks, and does it allocate — `nodes` and `atoms` warm; (2) does the
§8 grey ramp read as a fade on a real panel, **and does the §8.1 dot stroke
read as the original's texture beside a photograph of it**; (3) does §4.1's
1.25× land where the arithmetic says; (4) does every board take
`hw.setcpu "fast`. *Gate: a worst-case redraw under 100 ms at 300 MHz on all
three boards, with the 150 MHz figure taken alongside it.*

**M1 — the dungeon and the view.**
`scripts/gen_daggorath.py`, `logo/games/daggdata`, the cell walk, the
architectural lists, `MOVE` and `TURN` with both animations, the grey ramp, the
inverted levels including the status bar (which is why
[P18](roadmap.md#p18--interpreter-work-for-dungeons-of-daggorath) goes first), and
`hw.setcpu "fast` with `restore.clock` behind it.
*Gate: walking level 1 from (16, 11) matches a published Daggorath map, checked
cell by cell against the generator's own render; and the procedure-table test
exists.*

**M1a — the grey ramp** (§8.1), M1's last commit and the only optional
milestone in the list. Dots are M1's default and come free with P18 M2, so what
is left is the alternative: eight `setpalette` entries and the other half of one
`ifelse`. *Gate: a side-by-side against a screen photograph of the original,
both modes, at a fresh Solar torch and a dying Pine one — which is the one gate
in this design decided by a pair of eyes.*

**M2 — the command line and the clock.**
The scheduler of §5, the parser (`PARSER.ASM`/`TOKEN.ASM` — four-letter
abbreviations, `FULFLG` for `INCANT`), the text furniture of §4.1b–§4.1c, the heart
drawn and beating, fainting, damage recovery, death. *Gate: the heart's rate
tracks `HUPDAT`'s formula within a jiffy over a scripted damage ramp; the
`write`-n status line survives a redraw and every line of it measures 40
columns or fewer.*

**M3 — objects.**
OCBs, the bag, two hands, `GET` `PULL` `STOW` `DROP` `EXAMINE` `USE` `REVEAL`
`INCANT`, the status line, weight, torches and both light channels, the map and
both scrolls. *Gate: the §10.2 table round-trips — every object can be found,
revealed, named and used, and a Pine torch dies at five minutes.*

**M4 — creatures.**
The CCB table, `CMOVE` and its preference walk, the peek-a-boo, combat both
ways, creature loot, death, and the approach sounds of §9.3. *Gate: the §10.3
combat arithmetic matches hand-computed cases at both ends of the index range;
32 creatures schedule without the redraw missing its 100 ms.*

**M5 — the levels and the endgame.**
`CLIMB`, `VFTTAB`, the level graph of §7.5 including the level 3 wall, both
wizards, the ring riddle, victory, and `ZSAVE`/`ZLOAD`. *Gate: a scripted
playthrough reaches level 5 and wins.* **Note B65** — writes past 256 bytes on
the internal filesystem fail on a board — so `ZSAVE` writes to `/sd` until that
is fixed, and says so if there is no card.

**M6 — sound.**
Every effect in §9.2, the wizard's voice, the attract mode
(`HUMAN.ASM:PLAY20`, the ROM's own autoplay tables, on level 3 with an iron
sword, pine torch and leather shield). *Gate: a listening test against a
recording of the original — a spider, a knight, a wraith, a sword swing, a
torch, an explosion and the heartbeat, identified by ear without labels.*

---

## 16. Reduced-resource choices

Kept in reserve, in the order they would be spent:

- **The attract mode** (M6) is the first thing to go: it is the ROM's autoplay
  table and a demo dungeon and it buys the player nothing.
- **`ZSAVE`/`ZLOAD`** are cassette routines (`COMMON.ASM:SAVE`/`LOAD`) with no
  gameplay behind them.
- **The wizard fade-in/out** (`MISC.ASM:WIZIX`/`WIZOX`) is a set piece, not a
  mechanic.
- **The turn and half-step animations** are two of the four redraws in a busy
  second, and dropping them halves the worst case. They would be missed.

§8's fade is **not** on this list in either mode, because with
[P18 M2](roadmap.md#p18--interpreter-work-for-dungeons-of-daggorath) a dashed
stroke and a solid one cost the same and there is nothing to save by dropping
one. §8.2 keeps the pre-P18 figures for the one case where that stops being
true.

Nothing in §7, §8 or §10 is on this list. The tables are the game.

---

## 17. Tests

Host tests, mock device, mirroring `tests/test_berzerk.c`:

- **the tables** — every row of §10.1, §10.2 and §7.5 read back out of
  `daggdata` and compared against constants transcribed here, so a generator
  bug is a failing test rather than a wrong game;
- **the transform** — §6.2's `k`/`kx0`/`c` against hand-computed corners at
  ranges 0, 1 and 9, and the centroid at (0, 65);
- **the cell walk** — a hand-built corridor renders the expected sequence of
  lists, and stops at the first non-passage;
- **the fade** — §8's table both ways up, including "draw nothing" at ≤ −8;
- **the combat arithmetic** — §10.3 at index 0, 3 and 15;
- **the heart** — `HUPDAT`'s formula, the faint threshold and the wake
  threshold;
- **the level graph** — that level 3 has no way down and level 5 no way up;
- **the budgets** — the procedure table (§14) and a warm redraw that spends
  zero nodes and zero atoms;
- **the text** — every status and message line's rendered width ≤ 40, measured
  by `type` against the mock with the output cleared.

---

## 18. Risks

| | |
|---|---|
| **§6.3's list walk** | the one number the whole budget rests on, and the only one M0 exists to answer. Lever: a `VCTLSX` display-list primitive |
| **The procedure table** | 128 slots against a ~105 sketch, and overflow points at the wrong line (§14). Mitigated by the three rules, guarded from M1 |
| **The generator's nybble decoding** | a wrong sign bit gives a creature that looks *almost* right. Mitigated by §11.2's reference render, checked in |
| **B65** | blocks `ZSAVE` to the internal filesystem (M5) |
| **The sound derivation** | §9.2 is cycle counting, not measurement, and M6's gate is the ear |
| **A board that refuses 300 MHz** | the clock is a precondition (§12.1) and the game refuses rather than halving. No board in this tree has refused; the exposure is a chip, not a design |
| **P18 slipping** | P17 M1 wants three of its five items (§1). None is large, and §8.2 records what the fade costs without M2 if it comes to that |

---

## 19. Decisions taken, and what is still open

Four of these were open when the design was drafted and were settled the same
day (2026-09-02).

**Settled.**

1. **The heart is turtle 1 wearing one of two costumes** (§4.1a), not a drawing
   and not a `stamp`. The question as originally written was confused: it read
   as though the status line were text-window text. **It is not, and §4.1 was
   rewritten because of this question**: on a CoCo there is no text mode at all
   — the status line, the messages and the `EXAMINE` overlay are glyphs blitted
   into the same 256 × 192 bitmap — so the status line is `write` in the
   picture, and only the scrolling command line stays in the text window
   (§4.1b–§4.1c). The heart is the one part of it that cannot be `write` (our
   font is ASCII-only and has no heart glyph) and the one part that changes
   twenty times a second, so it is a turtle costume: the compositor draws it
   over the picture instead of into it, and there is nothing to erase.
2. **No panning** (§9.1). Settled, not deferred, and §16 no longer lists it.
3. **A `MOVE` into a wall costs energy.** `PTURN.ASM:PMOV90` runs after `PSTEP`
   has failed, so the ROM charges you `weight/8 + 3` for a step you did not
   take, and plays `A$THUD` while it does. It reads like a bug at first sight
   and it is not one: **it hurts to walk into a wall**, and the cost is the
   game saying so. Kept, with the thud.
4a. **`write` gains optional `fg` and `bg` colours, and it goes first.** Opened
   as [P18](roadmap.md#p18--interpreter-work-for-dungeons-of-daggorath) (2026-09-02). It
   is the only way to draw the ROM's inverse status bar (§4.1b(i)), and M1
   assumes it rather than shipping half an inversion.
4. **The object distribution keeps its wrap past level 5** (§7.3). `CINI44`
   allows a level 5 that `CMTTAB` has no row for, so a handful of objects may be
   unreachable in a given game. Kept: it is what the ROM does, and an
   unreachable Bronze shield is indistinguishable, from inside the dungeon, from
   one a creature is carrying two levels down.

**Still open, and each has a milestone that closes it.**

5. **How to walk 200 numbers a redraw** (§6.3) — three candidates, and the one
   number the whole budget rests on. **M0** measures all three; the levers if
   none land are a display-list primitive or arrays.
7. **Dots or grey?** (§8, §8.1) P18 M2 makes them the same price, so what was
   a budget question is now purely a question of which looks more like the
   original on a sharp panel — and the answer may differ between a fresh torch
   and a dying one. Dots are the default. **M1a** decides, against a photograph
   of a real CoCo, and it is the one milestone in this design whose gate is a
   pair of eyes.
