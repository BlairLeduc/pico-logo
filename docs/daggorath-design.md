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
| Data | The five mazes and the vector lists, generated into `logo/games/daggorath` itself between two markers (§7.4, §11.2). Top-level `make` lines, not code, so they cost no procedure slots — and the game ships as one file |
| Tests | `tests/test_daggorath.c` (Unity + mock device), mirroring `tests/test_berzerk.c` |
| Design | this document |
| Measurement | `tests/logo/p17m0`, all board runs kept verbatim under [`measurements/`](measurements/). It writes its numbers **to a file**, because numbers on a display cannot be copied off it |
| Generator | `scripts/gen_daggorath.py`, host-side, rewriting the marked block inside `logo/games/daggorath` (§7.4, §11.2) |
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
the split screen hides.

**The rows carry over from the CoCo unchanged; the columns do not.** The
`EXAMINE` overlay is still 19 rows (`TXTEXA` is 32 × 19), but every column
number `EXAMIN` uses is a *fraction of its screen width* rather than a fixed
offset — both headers centred (`LDD #10` is (32 − 12) / 2, the `LEAX 12,X`
before `BACKPACK` is (32 − 8) / 2), the rule the full width, the second entry
on a line half way across (`ADDD #16 / ANDB #$F0`). So they are recomputed for
40 — 14, 16, a 40-character rule and a tab stop at 20 — rather than left at 32
with eight columns of nothing down the right-hand side. The longest name in
the game is fourteen characters, so a second entry at column 20 still ends
inside the screen. The status line is the same identity read the other way:
`STATUX` justifies to columns 0 and 31 of 32, and ours to 0 and 39 of 40.

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
the 40 × 24 grid of §4.1, at about **0.7 ms** all told, with the column numbers
re-derived for 40 as §4.1 describes. It stays up until the next command,
exactly as `DSPMOD` does — which is why `LOOK` is a command.

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

**M2 built two of those four, and the two it left out are empty rather than
deferred.** `LUKNEW` has nothing to do until a creature or a burning torch
sets `NEWLUK` (M4, M3) and `BURNER` needs a torch, so they arrive with the
things that give them work instead of as procedures that do nothing.
`HSLOW` is not on the second queue at all: `COMPLR.ASM` returns `HEARTR` and
`Q.JIF`, so damage recovery runs at the heart's own rate — it is a jiffy task
that reschedules itself a heartbeat out. And `heart.tick` is a **separate
entry point** from `tick`, not just its first line, because it is the half a
long blocking effect keeps calling (§9.4) and re-entering the whole scheduler
from inside `HUPDAT` would recurse straight back into it.

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
source. So the generated block holds `[16 38 114 136] [27 64 64 27]` for the left
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

**And the order is load-bearing, which this section did not say and M1 got
backwards** ([B84](bugs.md), reported off a board during M2). `PREVU` sets
`PDIR` and calls `PUPSUB`, which builds the new view **in the backplane** —
invisibly; the sweep then plays on the *visible* screen, which `TURN00` has
just blanked down to those two horizontal lines; and only `PTUR90`'s
`DEC UPDATE / SYNC` flips the new view in. So the sequence is **turn, animate,
show** — never draw the new picture and then animate over it, because the
erase half of each stroke would cut a full-height stripe out of it. `PMOV30`
and `PMOV40` are the same shape (`PSTEP` builds, `RLTURN`/`LRTURN` animates,
`PMOV90` shows), and a *blocked* side-step skips the animation but still
shows. We have no backplane, but we do not need one: the animation begins by
erasing the screen anyway, so "build invisibly, then flip" and "animate on a
blank screen, then draw" put the same pixels in front of the player.

The strokes are at CoCo columns **8, 40 … 232** left-to-right (`LRTU10` loads
`#8`) and **248, 216 … 24** right-to-left, so the two sweeps are not mirror
images of each other.

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
emits all five mazes into `logo/games/daggorath`. This is **more** faithful, not
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

### 7.4 The data, and why it is not a second file

The generator writes into `logo/games/daggorath` itself, between
`; BEGIN GENERATED DATA` and `; END GENERATED DATA`:

- **the five mazes**, one `make "rows lput [ … ] :rows` line per row —
  8,134 nodes and ~256 distinct interned numbers, which is a quarter of the
  32,752-cell pool and nothing against a 32 KB word table;
- **the vector lists** (§11.2), flattened out of the ROM's relative-nybble
  encoding by the generator and *already split* into the parallel ys/xs
  lists §6.3 wants, one list to a line;
- **the vertical features table** (§7.5) and the four small stat tables of §11.

It is data, not code, so it costs **no procedure slots** — and §14 says why
that is the binding constraint in this port.

**Confirmed on hardware 2026-09-03**, one file, loading and playing as it did
from two.

**It began as a separate `daggdata` read with `open`/`setread`/`readlist`,
and moving it inline cost nothing.** Measured on the host from a bare
workspace, after `recycle`: reading the mazes from a file leaves **27,025**
free nodes, the identical data as whole-row list literals leaves **27,082** —
57 *better*, that being the loader procedure that no longer exists. Word-table
use is the same to within 30 entries. So a game that was two files to ship is
one, and `dagg.load`, `dagg.ys.of`, `dagg.xs.of` and `dagg.read.runs` are all
deleted along with the file-not-found failure mode.

**Two constraints shape the emitted form, and one of them is a trap.**
`load` buffers only `to … end` blocks; every other line is lexed and run on
its own, so a literal may not span lines and each maze row has to be one
`LOAD_MAX_LINE`-sized line (32 numbers is up to 151 characters against a
256-byte limit). That breaks this tree's 40-column source rule, and the
obvious repair — emitting six numbers a line and reassembling the row with
`se` — is the trap: it produces the same node count but retains **~10,900
word-table entries** where the whole-row literal retains none. The 40-column
rule is a rule about *source you read*; it loses to the word table here, and
the generated block is marked as generated precisely so nobody reflows it.

**What this forecloses.** All five mazes are now resident from `load`, where a
file could have been read one level at a time on `CLIMB` — 6,507 nodes, a
fifth of the pool, that four unvisited levels are holding. If M5 finds that
RAM binding, the fix is not to bring the file back but to emit each row as a
64-character hex *word* and expand only the current level, which keeps one
file and cuts the resident maze to a fifth.

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
else     ->  VCTFAD := BITMSK[8+A]        ; 1, 2, 4, 8, 16, 32
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
| ≤ −7 | 0 | — draw nothing |

**There are six fade levels and no 1/65** — an earlier version of this table
had seven and put the cutoff at −8, which is the pseudocode above read
backwards. `SFAD10` does `DECB` and *then* `CMPA #-7 / BLE SFAD30`, so A = −7
takes the darkness branch before `LDB A,X` is ever reached and `BITMSK`'s
`BIT6` is unreachable data. The `+1` is not decoration either: `VECTOR` does
`INC VCTFAD` before it loads `FADCNT` and `DEC VCTFAD` again at `VECT99`, so
the byte in the table is one *less* than the period. Corrected 2026-09-03
([B85](bugs.md)) while confirming [B84](bugs.md); M1 had shipped the
seven-entry reading.

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
base light is 0 (`PRLITE`) until the endgame, so **without a lit torch you see
nothing at all**, and `PUPDAT.ASM:PSUB10` adds the burning torch's two values
on top.

Torches, from `DTABAS.ASM:XXXTAB`:

| torch | minutes | regular | magic |
|---|---:|---:|---:|
| SOLAR | 60 | 13 | 11 |
| LUNAR | 30 | 10 | 4 |
| PINE | 15 | 7 | 0 |
| DEAD | — | 0 | 0 |

`COMPLR.ASM:BURNER` runs once a minute, decrements the timer, and **clamps
each light value down to the timer** as it falls — so a SOLAR torch begins
dimming with thirteen minutes left, and is called DEAD at five.

**A torch has three phases, not two, and M3 is where that turned up.**
`BURNER`'s only stopping condition is a timer of *zero*; `CMPA #5 / BGT`
renames the object and nothing else. So a torch burns at its full value while
the timer is above it, then **dims** — because each light is clamped down to
the timer — then is renamed **DEAD** at five, and *goes on dimming* to nothing
over those last five minutes. A Pine torch: 7 for eight minutes, 6, 5, dead at
ten, then 4, 3, 2, 1, 0. The gate's "a Pine torch dies at five minutes" is the
*timer* reading five, not the clock reading five.

**The two do not line up, and that is deliberate rather than sloppy.**
`PATTK.ASM:PATT22` tests `CMPA #T.TOR5` — the **name**, not the light — so from
the minute a torch is renamed you are fighting in the dark and throwing away
three hits in four, while the thing is still visibly lighting the corridor. A
Lunar torch (30 / 10 / 4) still has its magic light at **full 4** for two
minutes after it is called dead, because 4 is below the five the timer stopped
at. Every torch, every minute, is checked against `BURNER` transcribed into C
in `test_every_torch_burns_the_way_burner_says_it_does` — the same oracle shape
as the heart's `HUPD20` test, so the two agree by arithmetic rather than
because the same person wrote both.

**And `PRLITE` is zero for the whole game except the endgame.** `COMDAT.ASM`
never sets it, but `PATTK.ASM`'s ring riddle does — `LDD #$0713 / STD PRLITE`,
seven regular and nineteen magic — which is M5's, and is why the last scene is
the only one you can see without a torch.

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

**That is the comment at the head of `HUPDAT.ASM`, and the code below it
computes something one larger.** `HUPD20` divides by repeated subtraction and
does `INC T6` *before* `BCC`, so the subtraction that goes negative is counted
too: the quotient is `floor(64P / (P + 2D)) + 1`. At the start (`PPOW` 160,
`PDAM` 0) that is **46 jiffies — 766 ms, 78 beats a minute**, not the 45, 750
and 80 this paragraph used to claim; half-damaged it is 14, not 13. §1's rule
decides it — where the design and the ROM disagree the ROM is right — and M2
ships the ROM's arithmetic with `HUPD20` itself, transcribed into C, as the
test's oracle rather than the closed form the Logo evaluates.

At `HEARTR ≤ 3` you faint (`HUPD30`, `CMPA #3 / BGT`); you come round *above*
4 (`HUPD40`, `CMPA #4 / BLE`), which is deliberately not the same number; and
you die when `PDAM > PPOW`. At 160 power those three thresholds are 153, 142
and 161 damage, so **fainting is the last eight points before dying**.

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
| **Rime** ring | ring | 52 | 0 | 5 | 1 | 1 | 3 → Ice |
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

**One of those names is the macro's and not the game's.** `DTABAS.ASM`'s
`OBJXXX` calls the level-1 ring `HOTH`, and an earlier version of this table
copied that. `TOKEN.ASM`'s `ADJTAB` — which is what `PARSER` matches and
`STATUS.ASM:OBJNAM` prints — holds **`RIME`**, and `HOTH` is not a word this
game knows. Corrected at M3, which is the **third** time this design has been
caught reading a macro instead of the table it generates (`LVLTAB`'s "five
seeds" at M1, `CMDTAB`'s "four-letter abbreviations" at M2). The repair is
structural rather than another correction: `scripts/gen_daggorath.py` now
*decodes* `TOKEN.ASM`'s packed five-bit strings for every name and reads
`DTABAS.ASM`'s macro calls for every number, and pairs the two by position
with the **object class** — which both tables carry — as the cross-check.
Twenty-five agreements is what makes the pairing safe, and it is what says
`HOTH` and `RIME` are the same ring.

**And an object wears its generic's numbers until you reveal it.**
`OBIRTH.ASM:GENVAL` re-fills every new shield, sword and torch from
`LEATHER`, `WOODEN` or `PINE`, keeping only its own reveal requirement, and
`PREVEA.ASM:PREV00`'s second `OCBFIL` is what gives them back. So an
unrevealed Mithril shield really does filter like a leather one — the
information economy of this game is not cosmetic. Flasks, rings and scrolls
are `-1` in `GENVAL` and keep their real parameters from birth; you still
cannot read the adjective, but drinking one does what it does.

**For a torch that is not only its light but its lifetime**, because
`XXXTAB` is where all three of a torch's bytes live. An unrevealed Solar torch
burns for **fifteen minutes at 7/0**, not sixty at 13/11, and nothing
distinguishes it from a real Pine torch while it does. `PREV00`'s `OCBFIL`
then restores all three — **including the timer**, so revealing a torch you
have already been burning refills it. That is a strategy the ROM hands the
player and it falls out of one line of `OBIRTH`.

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
the status line, and we simply stop redrawing the heart.

**The map stays up until your next *keystroke*, not your next command**, and
`HEARTF` is how the game knows: `HUMAN.ASM:HMAN10` tests it at the top of
every character and, if it is clear, runs `INIVU` and re-prompts *before* that
character is buffered. `HMAN70` is the other half — no prompt while the map is
up, because there is nowhere to put one.

**An unrevealed scroll does nothing at all.** `USC200` sets `MAPFLG` and then
`TST P.OCREV,U / BNE USC199` returns without touching `DSPMOD`. Revealing a
scroll is what makes it work, and it is not consumed by use.

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

**M0 — the harness, and the three questions. Confirmed on a Pico Plus 2 W,
2026-09-02; Pico 2 and Pico 2 W still to run.**
`tests/logo/p17m0`. Builds the worst-case scene of §12 out of hand-written
tables and times it 200 times, reading the walk, the transform, the strokes and
the present apart from one another, into a file. Answers: (1) which of §6.3's
three list walks, and does it allocate — `nodes` and `atoms` warm; (2) does the
§8 grey ramp read as a fade on a real panel, **and does the §8.1 dot stroke
read as the original's texture beside a photograph of it**; (3) does §4.1's
1.25× land where the arithmetic says; (4) does every board take
`hw.setcpu "fast`. *Gate: a worst-case redraw under 100 ms at 300 MHz on all
three boards, with the 150 MHz figure taken alongside it.*

**The board passed at 53.2 ms against the 100 ms gate — with the scene 67 %
bigger than this section predicted.** `p17m0`'s hand-written worst case came to
184 points, not the ~110 §12 guessed, because every range draws a real left and
right wall plus a placeholder ceiling line rather than the sparser mix a real
corridor would show; the gate held anyway, with 47 ms to spare. The present
measured 19.95 ms against §12's predicted 19.8 — as close a match as this
design makes anywhere. Temperature rose 27.3 → 29.5 °C over the run, confirming
battlezone §12.3's finding that thermals are not a constraint here either.
`hw.setcpu "fast` took cleanly, switching and reading back `fast`.

**Q1's answer is `foreach`, and it is a genuine surprise.** All three
candidates read **zero** `nodes` and `atoms` delta over 1,000 walks — so
§6.3's open question about `butfirst` resolves clean: it does not allocate,
and neither does anything else here. But the timing does not favour the
no-alloc linear candidate the way §6.3 expected going in. Isolated on the
55-point creature: `foreach` 23.2 µs, `first`/`butfirst` 25.9 µs, `item` 28.2
µs. On the full 184-point scene the ordering holds: `foreach` 97.8 ms,
`item` 105.4 ms, `first`/`butfirst` 109.5 ms — `item`'s running index is not
even the slowest of the three, let alone quadratically bad, at this list
length. **M1 should draw with `foreach`**, per §6.3's own candidate 3: it
reads best, it does not allocate, and this board says it is also the fastest
of the three — the rare case where all three considerations agree.

**M1 — the dungeon and the view. Done 2026-09-02; confirmed on hardware
2026-09-03, after three bugs the host had no way to see, and again after the
data moved into the game file (§7.4) and made it one file to ship.**
`scripts/gen_daggorath.py`, the generated maze block, the cell walk, the
architectural lists, `MOVE` and `TURN` with both animations, the grey ramp, the
inverted levels including the status bar (which is why
[P18](roadmap.md#p18--interpreter-work-for-dungeons-of-daggorath) goes first), and
`hw.setcpu "fast` with `restore.clock` behind it.
*Gate: walking level 1 from (16, 11) matches a published Daggorath map, checked
cell by cell against the generator's own render; and the procedure-table test
exists.*

**The map gate is met, and it found a bug.** M1 originally shipped on internal
consistency alone — no published Daggorath map had turned up, so the carve/
wall/door port was checked line-by-line against `DGNGEN.ASM`, with
`docs/DungeonsOfDaggorath/daggdata-reference.txt` checked in for eyeball review
(§11.2) and `check_maze()` asserting, for all five levels, exactly 500 open
cells, 70 regular and 45 secret door bit-pairs, and full connectivity.
`docs/DungeonsOfDaggorath/Levels/` closed that gap: five SVGs of the real
dungeon, one cell to a 50 × 50 white square, doors drawn on the cell edges.
`check_against_published_map()` in the generator now diffs every carved cell
and every interior edge of all five levels against them, and it fails the
generator outright if they disagree.

**Every one of the five levels was wrong when that gate was first run** — 468
of level 1's 500 cells in the wrong place ([B83](bugs.md)). `RNDCEL` masks its
first random draw and `TFR A,B` copies it into **B, the column**, then draws
again into **A, the row**; the port read the two the other way round, which
transposed every maze about its diagonal. All five now match the published maps
exactly, cell for cell and door for door — which is the first real evidence
that `RANDOX`, all three `DGNGEN` phases and the `LVLTAB` sliding window are
right, rather than merely self-consistent.

**And the player start is `(16, 11)`, not `(12, 22)`.** `ONCE.ASM:GAME10` does
`LDD #$100B / STD PROW` before a real game — row 16, column 11, exactly where
`level1.svg` draws the player's blue dot. `COMDAT.ASM`'s `FCB 12 / FCB 22` sits
in the ONCE-only init block, i.e. the attract-mode DEMO's position; `(12, 22)`
is not a carved cell in the true level 1 at all, so the game had been starting
inside rock. This section's original `(16, 11)` was right and the commit-time
note that "nothing in this tree derives it from a source file" was wrong.
The procedure-table test
(`test_the_game_fits_the_procedure_table`) exists in `tests/test_daggorath.c`,
a static count of `"to "` lines against `MAX_PROCEDURES`, matching
`test_battlezone.c`'s own version rather than the "measured in play" one
§14 describes — that turned out to be the *global*-table test, not the
procedure count (battlezone's `test_the_game_fits_the_global_table_with_room_to_spare`).

**Confirmed on hardware 2026-09-03: the view reads as the original.** M1 was
recorded as done on 2026-09-02 and was not — it had never actually been seen
on a board. What a board found, and what only a board could have found in one
case:

| | Found by | |
|---|---|---|
| The whole view drawn in the background colour, and odd levels unenterable | a board (blank split screen) | [B81](bugs.md) |
| The forward face never drawn — `:dagg.forward` loaded and dead | reading the code against §6 | [B82](bugs.md) |
| Every maze the wrong dungeon; the player starting inside rock | `Levels/`, once it existed | [B83](bugs.md) |

The lesson is not "test on hardware" — it is that **each of the three needed a
different kind of oracle, and the host had none of them.** A colour that means
"the background" draws lines the mock records happily; a missing draw call is
invisible unless something knows what should have been drawn; and a maze can
satisfy every invariant its own generator knows how to check. The gates that
now exist are one per row: a test that refuses slot 255 and refuses a line in
the background colour, a test that the cell walk draws the forward face, and
`check_against_published_map()`.

**One porting bug worth recording, because the ASM reads the same way both
ways.** `MAKDOR`'s retry (`BITB A,Y; BNE MDOR10`) rejects back to `MDOR10` —
re-rolling the *cell* as well as the direction. A first port only re-rolled
the direction, which spins forever the instant `RNDCEL` lands on a cell whose
sides are all already wall/door/secret-door (`scripts/gen_daggorath.py` hung
past two minutes before this was found). The fix is the one-loop shape now in
`_place_one_door`. A related, and welcome, discovery: DGNGEN's own maze
invariant — a wall bit only ever appears on a side facing a `$FF` cell or the
grid edge, never between two carved cells — means `STEPOK`'s target-cell-`$FF`
check and `VIEW60`'s wall-bit check are two faithful views of the same fact,
and a synthetic test maze has to preserve that pairing or one of the two
correct behaviours reads as a bug (see `tests/test_daggorath.c`'s
`build_synthetic_corridor`).

**`LVLTAB` is a sliding window, not five single-byte seeds.** `LDX #LVLTAB;
LDB LEVEL; ABX` indexes by *one* byte a level, not three, so each level's
3-byte `SEED` reuses two bytes of the level before it: level *L*'s seed is
`LVLTAB[L..L+2]` over the full seven-byte table
(`$73 $C7 $5D $97 $F3 $13 $87`). This section's own paraphrase above ("$73
$C7 $5D $97 $F3 for the five levels") reads as five independent seeds and
is not what the ASM does; `scripts/gen_daggorath.py`'s `LVLTAB` comment has
the corrected reading.

**Mazes load as 32 rows of 32 cells, not one flattened 1024-item list.**
The first version of `dagg.load` built one flat list a level with
`sentence`/`lput` in a loop — an O(n²) copy — and ran a bounded heap out of
space loading the real five-level file on the host REPL (the per-test
fixture is one level and never hit it). `dagg.cell` does two `item` lookups
instead of one now; nothing else changed.

**The heaviest data-format decision, and why it stayed simple.** VARC.ASM's
twelve architectural lists and `VERT.ASM:CELINE` are all either plain
absolute `y,x` pairs or `SVORG`/`SVECT` macro calls whose own arguments are
already absolute coordinates — the assembler encodes the `V$REL` nybble
bytes, not the source. So `gen_daggorath.py`'s `RAW` table transcribes
`LPEEK`/`RPEEK` as the encoded bytes it hand-computed from the macro calls
and runs them through a real `V$REL` decoder (verified by hand against both
peeks' five points each) rather than skipping straight to the absolute
points — the decoder is what M3/M4 will need for `D3.ASM`/`D4.ASM`'s
creature data, and building it now against a known-correct case is cheaper
than building it blind later.

**M1a — the grey ramp** (§8.1), M1's last commit and the only optional
milestone in the list. Dots are M1's default and come free with P18 M2, so what
is left is the alternative: eight `setpalette` entries and the other half of one
`ifelse`. *Gate: a side-by-side against a screen photograph of the original,
both modes, at a fresh Solar torch and a dying Pine one — which is the one gate
in this design decided by a pair of eyes.*

**M2 — the command line and the clock. Done 2026-09-03; not yet seen on a
board.**
The scheduler of §5, the parser (`PARSER.ASM`/`TOKEN.ASM` — any unambiguous
prefix, `FULFLG` for `INCANT`), the text furniture of §4.1b–§4.1c, the heart
drawn and beating, fainting, damage recovery, death. *Gate: the heart's rate
tracks `HUPDAT`'s formula within a jiffy over a scripted damage ramp; the
`write`-n status line survives a redraw and every line of it measures 40
columns or fewer.*

**The gate is met exactly rather than within a jiffy, and the reason is that
the oracle changed.** `test_the_heart_rate_tracks_hupdats_own_division` runs a
nine-point damage ramp against `HUPD20`'s repeated-subtraction loop
transcribed into C, not against the closed form the Logo evaluates, so the two
have to agree by arithmetic and not by transcription. They agree to the
jiffy at every point — and in agreeing they contradict this design's own §9.4,
which had paraphrased the ROM's *comment*: the beat is 46 jiffies at full
health and not 45. §9.4 now records the ROM's arithmetic and why. The status
line measures 40 columns by `type` against the mock with the output cleared,
survives a redraw (it is written *inside* `dagg.redraw`, because §4.1b's
`clean` is the only eraser there is), and lands at column 0 of the character
row the status band starts on.

**"Four-letter abbreviations" was a misreading of the wrong table, and it is
the second time this design has been caught reading a macro instead of the
data.** `DTABAS.ASM`'s `CMDXXX` macro carries a four-letter name beside each
command — `ATTK`, `INCN`, `REVE`, `ZSAV` — and those are *assembler symbols*
(`T.ATTK`, `M$ATTK`), not what the player types. `TOKEN.ASM`'s `CMDTAB` holds
the **full words**, and `PARSER.ASM:PARS12` stops comparing when the *token*
runs out, so a command matches on any prefix and `ATTK` matches nothing at all
because it is not one. Two matches are an error rather than a preference
(`PARS20`'s `TST PARFLG / BNE PARS90`), which is why `Z` is neither `ZLOAD`
nor `ZSAVE` while `M` is `MOVE`. This is the same shape of mistake as
`LVLTAB`'s "five seeds" at M1: the macro is not the table.

**`T.BAK`'s string is `BACK`.** The CoCo manual prints `MOVE BACKWARD`;
`TOKEN.ASM` holds four characters, so `BACKWARD` fails the prefix test
outright and `BACK`, `BA` and `B` all pass it.

**Three ROM behaviours were kept that a tidier port would have lost.**
`PMOV90` charges `(weight / 8) + 3` damage on every accepted `MOVE` *whether
or not the step happened*, so walking into a wall costs what walking costs.
`HSLOW` recovers `ceil(damage / 64)` — `ASRD6` shifts a **negated** damage
right six places, which floors, so one point of damage still heals — and
reschedules itself `HEARTR` jiffies out, reading `HEARTR` *after* `HUPDAT`, so
recovery is slowest exactly when you are most hurt. And `HUPD42` walks the
light back one step *further* than `HUPD30` walked it down (it increments,
then tests `CMPA OLIGHT / BLE`), so you wake up at `OLIGHT + 1`; that is the
ROM's arithmetic and it is not corrected here.

**Two things the port had to decide for itself.** The line buffer is a **list
of one-character words**, not a growing word: a word interns, and buffering a
keystroke at a time as one word would put every prefix of every line ever
typed into the word table, which [P15](roadmap.md#p15--berzerk-design-first)
found is the **same arena** `nodes` reports on. That is what
`test_a_warm_redraw_spends_no_nodes_and_no_atoms` guards: it measures at two
loop lengths and compares them, because running any instruction list costs a
node or two of its own and a fixed cost would otherwise read as a leak. And **`load` runs a file a line at a time**, so a list
literal cannot span lines: the fifteen-row `CMDTAB` arrives as seven `se`
statements rather than one 320-character line, which is a shape the maze block
had already been forced into for a different reason (§7.4).

**The one departure from the ROM is ESC.** `DEATH` ends in `BRA *` and
`PLAY10` turns everything that is not a letter into a space, so there is no
key that means "stop"; `dagg.key` intercepts 27 before the conversion, which
is `logo/games/berzerk`'s own convention.

**Run on a Pico Plus 2 W the same day, and it found two more M1 bugs — both
in the turn animation, and both invisible to the host for the reason M1's own
post-mortem names: the mock has no oracle for what a picture looks like after
something erases part of it.** The report was *"I remember animation when
turning but is missing from the port"* and *"in the distance some lines are
not drawn (or erased?)"*, followed by *"when I `turn right` I see dots
(breaks) on the horizontal lines"* — which is one bug wearing three faces.
M1 drew the new view, presented it, and *then* ran the sweep over it, so every
stroke's `pe` cut a full-height stripe through the finished picture: long
lines came back holed, short ones (the distant ones) were erased end to end.
And none of it read as motion, because the game runs `setrefresh "manual` and
the sweep never called `refresh` — the damage sat unpresented until the next
heartbeat put it on the panel. §6.4 now records the ordering that was missing
from it. [B84](bugs.md), with [B85](bugs.md) — a seventh fade period the ROM
cannot reach — found while confirming it against `VCTLST.ASM`.

58 tests, all passing; the four animation gates were each checked against the
old code before the fix. **Confirmed on a Pico Plus 2 W 2026-09-03 with the
fixes in**: the view, the status bar with the heart beating in it, the
scrolling command line, and both animations. The sweep's dwell — `wait 13`,
the one number in the file that is an estimate rather than a transcription
(`VECTOR` at roughly fifty 0.895 MHz cycles a point, 118 points, drawn and
erased) — reads right at that value and was not tuned.

**What the board has still not exercised is the half of M2 you cannot reach by
playing it.** Fainting is 153 damage and death is 161, which at seven damage a
`MOVE` is twenty-three steps of walking into a wall; both are host-tested
against `HUPD20` and neither has been seen on a panel. The faint set piece in
particular is sixteen full redraws back-to-back and its cost is a board's to
report, so it stays on M3's list rather than being counted as done here.

**M3 — objects. Done and confirmed on a Pico Plus 2 W, 2026-09-03, over
four board runs — the first milestone in this port to come up right on a board
the first time, and the only defect any of those runs found was M2's.**
OCBs, the bag, two hands, `GET` `PULL` `STOW` `DROP` `EXAMINE` `USE` `REVEAL`
`INCANT`, the status line, weight, torches and both light channels, the map and
both scrolls. *Gate: the §10.2 table round-trips — every object can be found,
revealed, named and used, and a Pine torch dies at five minutes.* **Both halves
met**: `test_every_object_can_be_born_revealed_and_named` walks all twenty-five
types through `OBIRTH` → `REVEAL` → `OBJNAM`, and
`test_a_pine_torch_dies_at_five_minutes` walks a torch through all fifteen of
its minutes against `BURNER`. 95 tests, all passing. `LOOK` came with them,
because `DSPMOD` is sticky and it is the only way back from `EXAMINE`.

**The board run was six keystrokes and every one of them a different
thing.** `E`, `P L T`, `U L`, `P R SW`, `L` — from a black screen to a lit
corridor with a sword in hand, then walking the dungeon until the torch burned
out. That sequence confirms, in order: the inventory screen on its new
40-column grid (§4.1); the parser's *any unambiguous prefix* rule three times
over, including `SW` where `S` would have been ambiguous between `SCROLL`,
`SHIELD` and `SWORD`; `PULL` off the bag and `USE` stowing the torch it lights;
the two-hand status line naming a real object; `LOOK` coming back from
`EXAMINE`, which is the only way back and the reason `LOOK` was pulled into
this milestone; and `BURNER` running on the wall clock for fifteen minutes with
the redraw and the heartbeat going the whole time. It is also the ROM's own
attract-mode opening (`TOKEN.ASM:AUTTAB` is `EXAMINE` / `PULL RIGHT TORCH` /
`USE RIGHT` / `LOOK`) arrived at independently, which is the strongest
statement available that the opening is the shape the game intends.

**And the board saw the torch move through the backpack listing.** It *left*
the listing when pulled and came back **in reverse video** when used — which is
three separate pieces of `PGET.ASM`/`PUSE.ASM` agreeing in one visible step:
`PPULL` unlinking it from the bag list and clearing `PTORCH`, `PUSE12`'s
`PSTOW0` pushing it back at the **head** (so a burning torch is always the
first thing in your backpack), and `EXAM32`'s `CMPX PTORCH / COM P.TXINV,U`
picking out that one row. The reverse row is
[P18](roadmap.md#p18--interpreter-work-for-dungeons-of-daggorath) M1's
three-argument `write` earning its keep a **second** time: §4.1b(i) made the
case for it on the status bar alone and said the other uses were conveniences,
and this is one of them turning out to be a mechanic — it is how you know at a
glance which of two torches is the one that is burning.

**The game now starts in the dark, and that is the milestone's largest visible
change.** `PRLITE` is zero and `COMDAT.ASM` never sets it, so until you `PULL`
the pine torch out of the bag and `USE` it, `SETFAX` draws nothing at any
range — which is why the ROM's own attract mode opens `EXAMINE` / `PULL RIGHT
TORCH` / `USE RIGHT` / `LOOK` (`TOKEN.ASM:AUTTAB`). M1 and M2 had `dagg.light`
pinned at 8 as an admitted placeholder; the placeholder is gone.

**The third "the macro is not the table", and this time the repair is
structural.** §10.2 above called the level-1 ring `HOTH`, because
`DTABAS.ASM`'s `OBJXXX` macro does. `TOKEN.ASM` calls it **`RIME`**. Rather
than correct one more name by hand, `scripts/gen_daggorath.py` now *decodes*
`TOKEN.ASM`'s packed five-bit strings for every name and parses
`DTABAS.ASM`'s macro calls for every number, and pairs the two tables by
position with the **object class** — which both carry — as the cross-check.
It also checks `CMDTAB` and `DIRTAB`, which stay hand-written in the game file
because they are code, against the same decoder. Twenty-five class agreements
is what makes positional pairing safe, and it is the reason this class of
mistake cannot happen a fourth time in these tables.

**Three things the ROM does that a tidier port loses.** `PDROP` never clears
`PTORCH` and does not need to — `USE` stows the torch it lights and `PULL` is
the only way back out of the bag, so a burning torch can never be in a hand.
Every failure in `PUSE` and `PREVEA` past the hand parse is **silent**
(`PUSE24` is a bare `RTS`), which is how you find out what an unrevealed object
is without being told. And `BURNER` renames a torch `DEAD` at five minutes and
then goes on dimming it to nothing over those five — §8.3 now records it.

**Four port decisions.** The bag is a Logo list and the floor is a second one,
where the ROM threads both through `P.OCPTR`: `OFIND`'s flat walk over all 65
`OCB`s is 650 walks a redraw at ten ranges, tens of milliseconds against §12's
100 ms, so the *unowned* objects are kept in their own list — in OCB order, so
`OFIND` still answers in the order the ROM answers in. `SETFAD` moved from
once a range to once a **vector list**, which is where the ROM has it and the
only way a secret door can be lit by a magical torch while the wall in front of
it is not; it costs about fifty calls a redraw. `MAPPER` writes raw `$00`/`$FF`
and never reads `VDGINV`, so on the CoCo the map is the one picture that does
not invert with the level — ours inverts with it, because `clean` fills with
the level's own background and a white flash between two dark pictures is worse
than the departure. And the twelve parallel `OCB` lists are built with `fput`
and not `lput`: `lput` copies the list to append one cell, so twelve 65-cell
lists that way is 26,000 cells of garbage against a 32,752-cell pool, and it
ran out of space on the eleventh.

**One M1/M2 defect fell out of making the light real** ([B86](bugs.md)):
`HUPD30` decrements `MLIGHT` *before* the redraw and `RLIGHT` *after* it, and
the port decremented `RLIGHT` first — sixteen frames either way, ending at the
same −8, but each one drawn a step darker than the ROM draws it, and `MLIGHT`
not walked at all. Invisible while the light was a constant 8 and a real
difference now that it is a torch.

**The budgets.** 100 procedures of 192 and **108 globals of 254** at the peak
— the global count is taken for the first time here, because M3 put twelve
parallel lists and thirty-odd names into that table in one go. A warm redraw
still spends **zero** nodes and zero atoms with an object on the floor, which
is what `OFIND` being a cursor rather than a list buys.

**A second board run closed the object round trip, and drew the one thing
the generator had never had eyes on.** `D R` put the wooden sword **on the
ground and it read as a sword**; `G R SW` took it back and it was gone from the
floor; `S R` and `P R SW` took it into the bag and out again. That is
`VIEWER.ASM:VIEW52` — the floor-object pass inside the cell walk — seen for the
first time, and `FSWORD` is the one list in `VOBJ.ASM` with a **pen lift** in it
(`V$NEW` between the blade and the hand guard), so it is the only object whose
shape says the generator's `V$NEW` decode came out *right* rather than merely
plausible. §11.2 asked for a reference render precisely because a wrong nybble
gives something that looks almost correct; a sword that reads as a sword on a
panel is that check, made by an eye.

**And a third run found an M2 defect that four runs had walked past**
([B87](bugs.md)): *"I don't have a cursor in the input section."* There was
none — `screen_txt_enable_cursor(true)` happens in exactly one place,
`devices/picocalc/input.c`'s line reader, and `dagg.play` polls with
`key?`/`rc` and never enters it, because §4.1c says this is a typing game that
must not block. M2's own comment said as much and drew the wrong conclusion
from it: "our text window has a real cursor." **The ROM draws its own**, and
the port had dropped it — `M$PROM1` is `FCB I.CR,I.DOT` and falls straight into
`M$CURS`, so `PROMPT` is four characters and not two, and `HMAN20` prints
`M$CURS` again after every echoed keystroke. Its *bytes* do not transcribe,
because `I.BS` on a CoCo moves the cursor and ours clears the cell it lands on
— so the underline is drawn with the cursor left **past** it, and whatever
comes next backs over it and erases it for free. Four sequences, each doing
here what it does there. **It is the same shape as this design's three
"the macro is not the table" findings, one level down**: the ROM's byte stream
is not the terminal's, and copying it would have looked right in the source and
drawn nothing.

**M3 is now confirmed to the exact limit of what M3 can reach.** `GET`,
`PULL`, `STOW`, `DROP`, `EXAMINE`, `USE`, `INCANT` and `LOOK` have all been
typed on a board, with the status line, the floor objects, the reverse-video
torch and a fifteen-minute burn-out under them. What is left — `REVEAL`, the
three flasks and the map — needs an object the player has no way to get: you start with a
wooden sword and a pine torch, everything else in the dungeon stays
creature-owned until `NEWLVL.ASM:NLVL40` has creatures to hand it to, and
`ONCE.ASM:GAME30` **reveals** whatever it puts in your bag (`CLR P.OCREV,X`) —
so the first unrevealed object in a real game comes off a corpse. `REVEAL` is
therefore gated on M4 rather than on another run.

**`INCANT` is the exception, because it does not read `P.OCREV` at all**, and
a board took it. `PINCAN` wants a ring with a live `P.OCXXX+1`, and `GAME30`'s
reveal does not touch that — so the ROM's own two-table seam (`GAMDAT` for a
game, `DEMDAT` for the attract mode, `GAME20` choosing between them with a
pointer) is all it needed: `make "dagg.gamdat [12 15]` before `daggorath`
starts you with a Vulcan ring and a pine torch, and **`I FIRE` made a Fire ring
on a Pico Plus 2 W**. `dagg.objwt` stays at `GAME10`'s own 35 rather than the
11 those two weigh, so a step costs 7 instead of 4; nothing else changes.

**`I FIRE` is also both halves of the parser's asymmetry in four keystrokes.**
`I` is an unambiguous prefix of the only command beginning with it, and
`PARS12` takes it; `FIRE` had to be spelled out, because `PINCAN` is the one
place in the game that tests `FULFLG` and `FIR` matches nothing. The same line
would have been rejected either way round, and it was not. So the map stays the one screen nothing has
costed: 1,024 cells of scan (a
`foreach` a row, not 1,024 `item` calls) plus a couple of hundred strokes, off
§12's redraw budget because it is a screen you ask for, but redrawn twice a
second by `LUKNEW` while it is up. The per-list `SETFAD` is the other
unmeasured change to the redraw itself, and it went unnoticed on the board,
which is the most that can be said for it until something times it.

**And a session is fifteen minutes long, for a reason that is faithful.**
Everything in the dungeon starts creature-owned (§7.3), so the pine torch
`GAMDAT` hands you is the only light in the game until M4 puts creatures in it
to kill. When it burns out you are in the dark for good. That is the ROM's own
economy arriving early rather than a limitation of the port — but it does mean
M4 is what makes this game playable for longer than a torch.

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
  the generated block and compared against constants transcribed here, so a generator
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
   none land are a display-list primitive or arrays. **Answered on a Pico
   Plus 2 W, 2026-09-02: `foreach`** — fastest of the three, no allocation,
   and the candidate §6.3 already said reads best. Pico 2 and Pico 2 W still
   to confirm, though nothing here is expected to differ by board.
7. **Dots or grey?** (§8, §8.1) P18 M2 makes them the same price, so what was
   a budget question is now purely a question of which looks more like the
   original on a sharp panel — and the answer may differ between a fresh torch
   and a dying one. Dots are the default. **M1a** decides, against a photograph
   of a real CoCo, and it is the one milestone in this design whose gate is a
   pair of eyes.
