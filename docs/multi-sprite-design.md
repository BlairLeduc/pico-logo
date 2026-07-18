# P5 — Multi-sprite turtles and the display pipeline (design)

Status: **draft v2 for review** — this is the design gate for
[P5 in the improvements roadmap](improvements-roadmap.md#p5--multi-sprite-turtles-with-collision-design-first).
No implementation until this is agreed. Open questions and behaviour
changes in §12 were resolved with the user on 2026-07-04; no open
questions remain.

v2 adds a survey of period and modern multi-turtle Logos (§3) and redesigns
the sprite model around it (§4–§8): full-colour variable-size costumes,
autonomous motion (`setspeed`), rotation styles, `stamp`/`snapsh`, richer
sensing, and an explicit position on turtle-as-process concurrency. The
display-pipeline rework (§2) is unchanged from v1 and remains the
prerequisite milestone.

---

## 1. Where the time goes today

The PicoCalc display path is:

```
turtle primitive
  → erase turtle (restore save-under buffer into gfx_buffer)
  → draw line / move
  → redraw turtle (save background, stamp shape into gfx_buffer)
  → screen_gfx_update()            ── rate-limited to 60 Hz
      → lcd_blit(dirty row band)   ── BLOCKING CPU loop over SPI
```

Three independent costs multiply:

1. **The blit is synchronous and CPU-paced.** `spi_write16_pixels_blocking`
   feeds the SPI FIFO one pixel at a time (`palette[src[i]]`), so the
   interpreter core is stalled for the whole transfer. At 75 MHz / 16 bpp
   the wire moves ~4.7 Mpx/s:

   | Region | Pixels | CPU-blocked time |
   |---|---|---|
   | 1 row (full width) | 320 | ~68 µs |
   | 16-row band (one turtle) | 5,120 | ~1.1 ms |
   | Full screen | 102,400 | ~22 ms |

2. **Dirty tracking is full-width row bands.** A single y-range is kept
   (`dirty_y_min..dirty_y_max`). A 16×16 turtle update therefore blits
   5,120 px where 256 would do (20× waste). Two sprites at the top and
   bottom of the screen dirty *everything between them*. A wrapped line
   marks the whole screen (`screen_gfx_line` falls back to 0..319).

3. **The turtle lives inside the canvas.** Every move does
   erase-restore/save/stamp of a 24×24 or 16×16 region in `gfx_buffer`,
   and those regions dirty rows too. With N sprites this becomes N
   erase/redraw pairs per frame *and* an ordering problem: overlapping
   save-under sprites restore stale background unless erased in strict
   reverse order. (It is also why `dot?` can currently "see" the turtle's
   own pixels.)

Worst case for P5 as-is: 8 sprites scattered vertically → the dirty band
is effectively the whole screen → ~22 ms blocked per frame → the
interpreter loses >100 % of a 60 Hz budget before running any Logo.

## 2. Display pipeline redesign (M0 — ships before any sprite work)

Three changes, each independently valuable, in one milestone:

### 2.1 Tile-based dirty tracking

Replace the single row band with a 16×16-pixel tile grid: 20×20 = 400
tiles. Per tile-row, track the dirty span as `x_min_tile..x_max_tile`
(20 × 2 bytes) plus a per-tile-row dirty flag — ~60 bytes total.

- Marking stays O(1) per drawing op (`mark_dirty(x0,y0,x1,y1)` expands
  spans; callers that today call `screen_gfx_mark_dirty(y0,y1)` pass
  x extents too — the information is already at every call site).
- Flushing walks ≤20 tile rows and issues one `lcd_set_window` + blit per
  dirty span. Window setup is ~11 bytes of 8-bit SPI (~1.5 µs) — noise.
- Minimum granule is one tile (256 px ≈ 55 µs); a moving 16×16 sprite
  costs at most 4 tiles old + 4 tiles new ≈ 0.4 ms wire time, a 32×32
  costume at most 9+9 tiles ≈ 1 ms, vs ~2.2–4.4 ms of full-width bands
  today.
- Wrapped lines mark the tiles they actually touch (the Bresenham loop
  already computes wrapped per-pixel coordinates).

Rejected: per-scanline x-extents (320 × 4 B = 1.3 KB, one window setup per
row, no better than tiles in practice); exact rect lists (allocation and
merge complexity for marginal gain over tiles at this resolution).

### 2.2 DMA blit with a pipelined palette-expansion line buffer

The 8-bit framebuffer must be palette-expanded to RGB565 on the way out
(a 16-bpp shadow buffer would be 200 KB — impossible). But the expansion
does not have to be serialized with the wire:

- Two line buffers (2 × 640 B). CPU expands line *n+1* while DMA streams
  line *n* to the SPI at wire speed. Completion is polled (no IRQ; the CPU
  has useful work — expanding the next line — while it waits).
- Interpreter-visible cost per line drops from ~68 µs (wire-paced loop) to
  ~5–8 µs of memory traffic; the wire time still passes but overlaps with
  expansion of subsequent lines and, at the end of a span, the CPU only
  waits out the final line.
- Net: a full-screen flush costs the CPU ~3–4 ms instead of 22 ms; typical
  sprite frames cost well under 1 ms of CPU.
- The existing `lcd_disable_interrupts()` discipline around blits is kept:
  the LCD is only ever touched from the interpreter core, and CS is held
  for the duration of one span transfer exactly as today.
- Side benefit: `lcd_putstr`/editor text rendering can adopt the same path
  later, but that is out of scope for P5.

Deferred (rejected for now): moving blits to core 1. It buys true
zero-cost presents, but the LCD is shared with the text console, editor,
and scroll paths that all run on core 0 — every `lcd_*` call site would
need a cross-core mutex, and tearing/ownership bugs are the classic
result. The DMA pipeline gets ~90 % of the benefit with none of the
locking. Revisit only if M1 profiling shows presents still dominate.

### 2.3 Refresh policy: automatic by default, manual on request

Both mechanisms, user-selectable, because they serve different programs:

- **Automatic (default, unchanged semantics):** drawing primitives mark
  dirty state; a rate-limited present (existing 60 Hz limiter) pushes
  dirty tiles. With 2.1 + 2.2 this is cheap enough that ordinary turtle
  graphics and simple games never need to think about it.
  `screen_gfx_flush()` before blocking ops (keyboard wait, `sleep`, I/O)
  stays, so the picture is always current when the program pauses.

- **Manual:** the program takes control of frame boundaries:

  ```logo
  setrefresh "manual
  repeat 1000 [each [fd 2 rt who * 3] refresh]
  setrefresh "auto
  ```

  - `setrefresh "auto | "manual` — select the policy. In manual mode
    drawing primitives touch only SRAM and dirty state; nothing reaches
    the LCD.
  - `refresh` — present the dirty tiles **now**, regardless of policy or
    rate limiter. Does not change the policy.
  - `refreshmode` — outputs `auto` or `manual`.
  - Safety valves: `setrefresh "auto` is restored on `throw "toplevel`,
    error unwind to the REPL, and `cs`/`draw` — a program that forgets
    `refresh` cannot leave the machine showing a stale screen at the
    prompt.

  Rationale for two orthogonal primitives instead of FMSLogo's
  `refresh`/`norefresh` toggle pair: in FMSLogo, `refresh` both presents
  and re-enables auto mode, so a game loop cannot present a frame while
  *staying* manual without immediately calling `norefresh` again. Keeping
  "policy" (`setrefresh`) and "present" (`refresh`) separate makes the
  game-loop idiom one word per frame.

Manual mode also gives batch drawing for free (`setrefresh "manual`,
draw a complex scene, `refresh`) — the whole scene appears in one
~3–4 ms present instead of accumulating dozens of partial blits.

### 2.4 Sprites move out of the canvas: a scanline compositor

The single biggest simplification for multi-sprite. Today the turtle is
*stamped into* `gfx_buffer` with save-under. Instead:

- `gfx_buffer` holds **only the canvas** (pen trails, fills, background).
  Pen-down movement draws into it exactly as today.
- Sprites exist only as state: position, heading, costume, colour,
  visibility. They are **composited into the outgoing line buffer during
  the blit** (§2.2): while expanding row *y*, for each visible turtle
  whose bounding box covers *y*, overlay its costume pixels for that row.
  This is what the period machines' video hardware did (TMS9918A sprite
  planes, Atari player-missile graphics) — done in software, which lifts
  every hardware restriction: no 4-sprites-per-scanline flicker, no
  single-colour limit, no fixed 8×16 grid.

Consequences, all good:

- **No save-under buffers** (deletes `turtle_background_shape0` /
  `turtle_background_bitmap`, `turtle_erase`, `turtle_save_background`
  and the ordering hazard entirely; frees ~832 B and removes the
  erase→dirty→blit churn on every move).
- Moving a pen-up turtle dirties only its old + new tiles (the compositor
  needs the tiles re-presented, but nothing is written to the canvas).
- `dot?`, flood `fill`, and `gfx_save` (BMP) all see a **clean canvas** —
  sprites can never contaminate the drawing. (Today the saved BMP
  includes the turtle glyph; the design changes that. Judged a fix, not a
  regression. Flagged as a visible behaviour change.)
- Wrap mode: the compositor wraps per-row x/y exactly as `wrap_coord`
  does today, so a sprite straddling an edge renders on both sides.

## 3. Survey: multi-turtle and sprite Logos, 1981→now

What each system got right, verified against primary sources where they
survive (manual scans; see Sources at the end).

### TI Logo / TI Logo II (TI-99/4A, 1981–82) — *sprites as autonomous objects*

32 hardware sprites (TMS9918A VDP) alongside one drawing turtle.
`TELL SPRITE 1`, `TELL [1 2 3]`, `TELL :ALL` (`ALL` is literally the list
0–31, so **named groups fall out of `MAKE "SQUADRON [1 2 3 4]`** — group
naming costs nothing). `CARRY :ROCKET` assigns a shape (5 built-ins, user
shapes via the `MAKESHAPE` grid editor), `SETCOLOR` one of 16 colours
(colour 0 = invisible). The signature feature: **`SETSPEED 20` sets a
sprite gliding autonomously** — it keeps moving, driven by the VDP
interrupt, while the interpreter does something else entirely, and even
while you type at the prompt. `SV x y` / `XVEL` / `YVEL` expose the
velocity as a vector. `FREEZE` / `THAW` pause and resume the whole moving
world. Sprites cannot hold a pen (hardware planes don't touch the
bitmap); lower-numbered sprites render on top; ≥5 sprites on a scanline
flicker out (hardware limit). **No collision detection at all** — games
had to compare coordinates. The manual teaches costume-swap animation
(flapping wings) as an idiom: `MAKESHAPE`-two-frames, `CARRY` in a loop.

*Take:* autonomous velocity is the "parallelism for free" feature; group
naming via plain lists; freeze/thaw; costume-swap animation; also a
warning — sprites without collision push all the hard work back on the
programmer.

### Atari Logo (LCSI, 1983) — *events as first-class citizens*

4 turtles (player-missile graphics), `TELL 1` / `TELL [0 1 2 3]`; the
turtles draw with pens like real turtles. 15 editable 8×16 mono shapes
(`EDSH` full-screen editor, `GETSH`/`PUTSH`/`SETSH` — the model for our
current shape primitives), per-turtle colour (`SETC`, 128 colours),
`SETSP` autonomous speed ("Logo is free to do other things while the
turtles are moving"). The crown jewel: **`WHEN` demons**. `WHEN 4 [RT
180]` arms a demon on event #4; the primitives `OVER t pen` and
`TOUCHING t1 t2` *output event numbers*, so `WHEN TOUCHING 1 2 [RT 180]`
reads declaratively. The event table (22 events) covers turtle-pair
collisions, turtle-over-drawing per pen number (0–2), **joystick button,
joystick moved, and a once-per-second timer** — demons were a general
event system, not just collision. A fired demon hands its instruction
list to whichever turtles are currently addressed; `WHEN n []` disarms;
`CS` clears all demons.

*Take:* demons — including timer and input events, not only collision;
turtle-over-colour sensing (`OVER`); demons cleared on reset as a safety
property. Also a lesson: numeric event codes needed an appendix table to
decode — conditions should be *expressions*, not magic numbers.

### TRS-80 Color Logo (Radio Shack, 1982) — *turtles as processes*

The most radical model of the era. `HATCH 1 BOX 50 30 60` **spawns turtle
1 as an independent process running procedure `BOX`** — up to 254 of
them, plus master turtle 0. The scheduler is cooperative round-robin:
each turtle executes *one command (or one control-statement evaluation)
per turn*. A turtle whose procedure finishes vanishes from the screen;
`VANISH` lets one remove itself; `ME` outputs your own number. Sensing:
`NEAR t` outputs the Manhattan distance to turtle *t*. Communication is
**mailboxes**: `SEND 2 1` leaves message 1 for turtle 2; `MAIL 1`
receives (0 = no mail); address 255 = "first taker". The manual builds a
two-player chase game as three processes: a keyboard-polling turtle
`SEND`ing steering messages to a runner and a chaser. Shapes are defined
as **strings of turtle commands** (`SHAPE URRFFFLLDFFFF…` — F/B step,
R/L turn 45°, U/D pen) that draw the glyph; shapes auto-rotate with
heading in 45° steps and render by XOR so a turtle "can pass over a
picture without destroying it". Limitations: process state was tiny, no
demons, integer-only dialect, and turtle 0 running out of work halts
everyone (the manual warns to "assign turtle 0 the most complex
procedure").

*Take:* turtle-as-process is the purest realization of "multiple turtles
= simple parallelism", and its one-command-per-turn scheduler maps
directly onto our step-trampoline evaluator (§8). `NEAR` (distance) is a
sensing primitive we should have. Mailboxes only matter once processes
exist. Shape-as-turtle-program is charming but couples glyph quality to
45° strokes; we get the same "draw your own sprite" spirit more powerfully
via `snapsh` (§4.4).

### BBC Logos (Acornsoft, Logotron; 1983–85)

Surveyed for completeness: strong, standards-setting single-turtle
dialects (Acornsoft's was among the most complete Logos of the era) with
floor-robot support, but nothing multi-sprite to mine. Included here so
the survey is honest about where the ideas *aren't*.

### LogoWriter / MicroWorlds (LCSI, 1985 / 1993)

LogoWriter: 4 turtles with a shapes bank, **`STAMP`** (bake the current
costume into the canvas — instant scenery), and **`COLORUNDER`** (sense
the canvas colour beneath the turtle). MicroWorlds generalized to many
named, clickable turtles and real background processes: **`launch
[instrs]`** runs an instruction list concurrently (cooperatively
time-sliced), `forever [...]`, `clickon`; every button/turtle can carry
a "many times" behaviour. This is `HATCH` grown up: processes belong to
the environment, not to a turtle.

*Take:* `stamp` and colour-under sensing; `launch` as the cleaner shape
for future concurrency (a process is just a running list, not a new kind
of turtle).

### StarLogo / NetLogo (MIT/Northwestern, 1994→) and Scratch (MIT, 2007→)

StarLogo/NetLogo scaled "multiple turtles" to thousands of agents:
`ask turtles [...]` blocks, breeds, `hatch` (which there means *clone*,
not *spawn a process* — a naming collision with Color Logo worth
avoiding), patches as a sensed environment. Scratch is the modern
consensus for kid-facing game machines: sprites with **multi-costume
full-colour appearance, rotation styles (all-around / left-right /
don't-rotate), size scaling**, event hats (`when green flag clicked`,
`when touching`), **broadcast/receive messages**, and implicit
concurrency (every script is a cooperative thread). Its costume model —
several named frames per sprite, switched freely, drawn in an editor —
is exactly TI's flapping-wings idiom made first-class.

*Take:* rotation styles and size scaling (cheap in a software
compositor); costume lists for animation; broadcast as the modern
mailbox; confirmation that event-driven + cooperative concurrency is the
end-state — and that we should reserve the good names for it.

### Synthesis — what Pico Logo should steal

| Idea | From | Into this design |
|---|---|---|
| Addressing: `tell`/`ask`/`each`/`who`, lists, named groups via `make` | Atari, TI | §5 (unchanged from v1, plus group idiom documented) |
| Autonomous motion: `setspeed`, `freeze`/`thaw` | TI, Atari | §4.5 |
| Full-colour, variable-size, multi-frame costumes | Scratch (modernizing TI/Atari 1-bit shapes) | §4.2 |
| Rotation styles per turtle | Scratch (Color Logo's auto-rotate, minus the 45° limit) | §4.3 |
| Draw-your-own costumes: `snapsh` from the canvas | Color Logo's spirit, LogoWriter's `stamp` inverted | §4.4 |
| `stamp`, `colourunder`/`over?` | LogoWriter, Atari `OVER` | §4.4, §6 |
| `distance` between turtles | Color Logo `NEAR` | §6 |
| Demons on *expressions* (not event numbers), incl. non-collision conditions | Atari `WHEN`, modernized | §7 |
| Turtle-as-process / `launch` | Color Logo, MicroWorlds | §8 — designed-for, deferred |
| Broadcast messages | Scratch (Color Logo mailboxes) | §8 — arrives with processes |
| Pixel-true collision `touching?` | (nobody had it right: TI had none, Atari had hardware bboxes) | §6 |

## 4. Sprite model (M1)

### 4.1 Count and state

`MAX_TURTLES 8` in `core/limits.h`. Atari-generous, keeps worst-case
compositor and collision costs flat; the budget (§10) shows 16 is a
one-line change later. All eight are full turtles — pens included
(Atari's model, not TI's pen-less sprites): a gliding turtle with its pen
down draws a trail, which no hardware-sprite Logo could offer.

Per-turtle state (device side): position, heading, pen state/colour,
costume number, animation frame list + rate, speed, rotation style,
scale, visibility ≈ 40 B; plus a cached *rendered raster* (costume after
rotation/scale/paint, regenerated only when one of those changes) drawn
from the costume pool (§4.2). Turtle 0 boots exactly as today's single
turtle; 1–7 boot hidden at home. Z-order: **lower number on top** (TI's
rule — turtle 0 is the hero).

### 4.2 Costumes: full-colour, variable size, pooled

The Atari 8×16 mono grid was a hardware artifact. Costumes become:

- **Size 8×8 up to 32×32**, any rectangle in that range.
- **Two kinds**, distinguished per costume:
  - *Mono* (1 bit/px): a mask painted in the turtle's pen colour at
    composite time — exactly today's shapes, and `getsh`/`putsh`'s
    16-number list format keeps working unchanged (it defines a mono
    8×16, doubled to 16×16 as today).
  - *Colour* (8 bit/px, palette-indexed): pixels rendered verbatim, one
    reserved index (255) = transparent. The full 256-slot palette —
    period sprites dreamed of this.
- **Pool-allocated**, following the house pattern for tiered memory
  (`mem_word` blobs / HTTP aux buffer): a fixed SRAM pool (8 KB) on all
  boards, replaced by a PSRAM-backed pool (64 KB) on the Pico Plus 2 W.
  Costume slots 1–15 remain (reference compatibility); a full pool
  reports `ERR_OUT_OF_SPACE`. 8 KB holds e.g. 15 colour 16×16s + change;
  colour 32×32s (1 KB each) are the PSRAM board's luxury, and the
  reference says so plainly.
- Shape 0 stays the built-in vector triangle, rotating smoothly with
  heading as today.

### 4.3 Rotation and scale (compositor features, free of hardware)

Per turtle:

- `setrot "full | "flip | "fixed` — Scratch's three styles. `fixed`
  (default for bitmap costumes) matches TI/Atari behaviour; `flip`
  mirrors left/right when heading crosses 180° (covers most game art);
  `full` rotates the costume to the heading (nearest-neighbour into the
  cached raster on heading change — a 32×32 rotate is ~1 K pixel
  transforms, microseconds at 150 MHz).
- `setmag 1 | 2` — integer scale into the cached raster (TI's `BIG`/
  `SMALL`, but per-turtle).

Both only touch the cached raster; the compositor always blends a
prebaked w×h image, so per-frame cost is independent of these features.

### 4.4 Costumes from the canvas: `snapsh` and `stamp`

Two inverses that make the canvas the costume editor:

- `snapsh n w h` — capture the w×h canvas region centred on the (first
  active) turtle into costume *n* as a colour costume. Transparency is
  defined by palette index, not appearance: canvas pixels whose index
  equals the current background slot at capture time map to the
  transparent index (255); all other indices are copied verbatim.
  (Corollary: changing the background colour later does not retroactively
  alter captured costumes.) The Logo-native way to make a sprite: *draw
  it with the pen you already know, then pick it up.* (Color Logo defined
  shapes in turtle language; this is that idea without the 45° strokes.)
- `stamp` — composite the current costume into the canvas at the
  turtle's position (LogoWriter). Scenery, trails of glyphs, particle
  effects — one primitive.

A grid costume editor in the style of `EDSH`/`MAKESHAPE` (the PicoCalc
has a full-screen editor precedent) is a natural follow-on, out of P5's
scope; `snapsh` + `putsh` + BMP load cover creation until then.

### 4.5 Autonomous motion: `setspeed`, `freeze`, `thaw`

The TI/Atari feature that made sprites feel alive — and the cheapest
"parallel processing" there is:

- `setspeed n` — the turtle advances along its heading at *n* steps per
  second, continuously, without program attention. `setspeed 0` stops.
  Applies per turtle (active set, as any command). Speed is state like
  heading: `speed` queries it.
- **Tick source:** motion advances at the existing evaluator hook (top of
  `eval_instruction`, time-gated to the frame cadence) *and* in the
  device idle loop (the keyboard-wait path that already calls
  `screen_gfx_flush`) — so turtles keep gliding while the program
  computes **and while the user types at the prompt**, exactly the TI
  magic. Advancement is wall-clock-scaled (`speed × Δt`), so speeds are
  honest regardless of interpreter load.
- Pen semantics apply: a gliding pen-down turtle draws its trail (a
  first — no hardware-sprite Logo could). Fence mode stops the turtle at
  the boundary (speed forced to 0 — observable by a demon); wrap wraps.
- `freeze` / `thaw` — pause/resume all autonomous activity (motion,
  costume animation, demons) globally. TI's names; also the panic button
  for classrooms.
- Animation: `setsh [2 3 4]` sets a costume *list* — frames cycle at
  10 fps by default, `setanim n` tunes it (0 = hold). TI's flapping-wings
  idiom as one word. (Implementation: per-turtle frame index + divider in
  the same tick.)

Autonomous motion/animation is cleared by `cs`/`draw` and on
error-unwind to the REPL alongside refresh policy; demons are cleared by
`cleardemons` and on error-unwind only, not by `cs` (revisited
2026-07-18, §7).

## 5. Addressing: `tell`, `ask`, `each`, `who`

Unchanged from v1, with the group idiom now documented:

- `tell 3` / `tell [0 1 2]` — set the active set. Out-of-range →
  `ERR_BAD_INPUT`; duplicates collapse. Since `tell` takes a list,
  **named groups are free**: `make "flock [1 2 3]  tell :flock` — TI's
  pattern, zero new machinery.
- Every turtle **command** fans out over the active set in ascending
  order; every **query** answers for the lowest active turtle
  (documented rule; Atari practice).
- `ask 2 [fd 10]` / `ask [1 3] [...]` — run with the set temporarily
  replaced; restored on error/throw (the `pause` save/restore
  discipline).
- `each [fd who * 10]` — run once per active turtle in ascending order
  with the set narrowed to it; `who` outputs a number when one turtle is
  active, a list otherwise — so inside `each`, `who` is the current
  turtle's number, giving per-turtle differentiation without new binding
  machinery.
- Screen-global primitives (`cs`, `setbg`, boundary modes, palette,
  refresh policy, `freeze`/`thaw`) ignore the active set.
- Boot and `cs`: `tell 0`, turtles 1–7 re-hidden at home — single-turtle
  programs run byte-identically; multi-turtle is strictly opt-in.

Device interface: one added op, `select(n)`, routing the existing
stateful single-turtle ops; core iterates the active set. Mock and
PicoCalc grow per-turtle arrays behind it; host keeps `turtle == NULL`
and degrades exactly as today.

## 6. Sensing and collision (M2)

- `touching? t1 t2` — `true` when the two turtles' *rendered* pixels
  overlap: bbox reject, then mask AND at the relative offset over the
  cached rasters (≤32 rows of shifted word ops — hundreds of cycles).
  Both must be visible; wrap-mode offsets computed modulo screen, so
  edge-straddling contact counts. Pixel-true collision at rotation/scale
  — better than any period machine (Atari's hardware gave bboxes at 8×16;
  TI gave nothing).
- `over? colour` — `true` when any canvas pixel under the (first active)
  turtle's mask has palette index `colour` (Atari `OVER` + LogoWriter
  `COLORUNDER`, expression-shaped). Senses *drawings*: lines, fills,
  stamps — the maze wall, the finish line. `colourunder` (outputs the
  canvas colour at the turtle's position) comes along for one extra
  getter.
- `distance t` — Euclidean distance from the first active turtle to
  turtle *t* (Color Logo's `NEAR`, with real geometry; single precision
  like all math here). Complements the existing `towards`.

All three are implemented **in core** against `select` + queries + shared
raster data — device-independent and fully unit-testable on the mock.

## 7. Events: `when` demons (M3)

```logo
when [touching? 0 1] [make "score :score + 1 crash]
when [over? 12] [setspeed 0]
when [key?] [steer readchar]
```

- `when [cond] [instrs]` arms a demon; `when [cond] []` disarms the demon
  with the `equal?` condition; `(when)` prints the table. `MAX_DEMONS 8`
  in `core/limits.h`. Conditions are ordinary expressions — Atari's whole
  22-entry event-number appendix (collisions, joystick, once-per-second
  timer) collapses into "whatever you can write": `touching?`, `over?`,
  `key?`, `distance 1 < 20`, timer comparisons.
- **Poll point:** top of `eval_instruction` beside the interrupt/freeze/
  pause checks, time-budgeted (~20 ms) so tight loops aren't taxed; also
  polled from the device idle loop so demons stay armed at the prompt,
  exactly like Atari's (an armed demon firing while you type is the
  point, not a bug — it's what makes it a game machine).
- **Edge-triggered:** fires on the condition's false→true transition
  (per-demon `was_true` bit) — contact fires once, not every poll.
- **Re-entrancy:** actions run in a nested evaluator on the C stack (the
  `pause` precedent; parent trampoline TCO state untouched). Polling is
  suppressed while any action runs — no nesting, no storms. Actions run
  to completion, sequentially, in arming order.
- **Errors/throws:** an error in an action reports and stops the program
  normally; `throw "toplevel` unwinds to the REPL; a `throw` escaping the
  action is an error (same rule as `pause`).
- **Lifetime:** autonomous motion, animation, and manual refresh are
  cleared by `cs`/`draw` and on error-unwind to toplevel. Demons are
  cleared by `cleardemons` and on error-unwind, but **not** by `cs`
  (revisited 2026-07-18: demons grew into a general-purpose tool — HTTP
  handlers, timers — and a screen clear must not tear down event
  handlers; error-unwind keeps *nothing acts on its own after an
  error*). `freeze` suspends demons; `thaw` resumes them.

## 8. Concurrency: the position on turtle-as-process

Color Logo's `HATCH` (each turtle a round-robin process, one command per
turn) and MicroWorlds' `launch` (background instruction lists) are the
real thing, and the user-visible model we would eventually want is
MicroWorlds': **`launch [instrs]` — a process is a running list, not a
special kind of turtle** (it composes with `tell`/`ask`, and avoids
Color Logo's "turtle 0 ran out of work" trap and NetLogo's `hatch`-means-
clone naming collision).

It is genuinely implementable here — and still deferred from P5:

- *For:* the evaluator is already a step trampoline (`eval_steps.c`);
  "one step per process per turn" is Color Logo's scheduler verbatim. The
  interrupt-check hook is the natural yield point.
- *Against (now):* each process needs its own frame arena and error
  context. Frame arenas are the SRAM item we cannot multiply casually
  (~61 KB total headroom); PSRAM boards could back extra arenas, but then
  the flagship feature would be tier-gated. Re-entrancy rules
  (workspace mutation, `catch`/`throw`, `to` during a process) need their
  own design gate.
- *The bet:* `setspeed` + costume animation + demons deliver the felt
  experience of concurrency — things move, react, and collide while the
  program (or the user at the prompt) does something else — at a tiny
  fraction of the complexity. TI Logo proved this combination carries a
  whole game culture; it never had processes at all.

So: P5 ships the autonomous trio; `launch` becomes a P6 candidate with
its own design doc ([`launch-design.md`](launch-design.md), drafted
2026-07-12) once M1–M3 usage shows where it's needed. Scratch-
style `broadcast`/message demons belong to that future design (they are
the inter-process mailbox; with one thread of control, demons on shared
variables already cover it). This design keeps the door open: per-turtle
state is already independent, demons already run in nested evaluators,
and nothing below assumes a single locus of execution.

## 9. Device interface changes (`console.h`)

- Add `select(uint8_t n)` (routes existing stateful turtle ops).
- Costume ops generalize: `put_shape_data`/`get_shape_data` keep their
  16-`uint8_t` mono form; add sized/colour variants
  (`put_costume(n, w, h, kind, pixels)` / `get_costume(...)`), plus
  `stamp()` (backs the `stamp` primitive) and `snap_costume(n, w, h)`
  (backs `snapsh` — the C name spells out what the primitive
  abbreviates).
- Per-turtle setters already exist (`set_shape`, `set_visible`, …); new
  ones: `set_speed`, `set_rotation_style`, `set_scale`, `set_anim`.
- Motion/animation tick: one device-driven `turtle_tick(dt)` invoked from
  the poll hook and idle loop (device owns positions; core owns demons).
- Mock records everything (rasters, ticks, composited rects) for tests;
  host stays `turtle == NULL`.

Rejected (unchanged from v1): index parameter on every op; moving all
turtle state into core.

## 10. Budgets

SRAM (against ~61 KB free: `bss` 458,684 B of 520 KB on `pico2`, minus
stacks/heap):

| Item | Delta |
|---|---|
| Per-turtle state, 8 turtles (§4.1) | +~320 B |
| Cached rendered rasters (worst: 8 × 32×32 @ 8-bit) | in pool ↓ |
| Costume + raster pool (SRAM tier; PSRAM tier 64 KB on Plus 2 W) | +8 KB |
| Tile dirty structures (§2.1) | +~60 B |
| 2 × DMA line buffers (§2.2) | +1,280 B |
| Demon table (8 × cond/action refs + flags) | +~100 B |
| Save-under buffers deleted (§2.4) | −832 B |
| **Net** | **≈ +9 KB** (worst case, pool fully committed) |

Frame cost, worst realistic case (8 visible 16×16 sprites all gliding,
pen trails, 60 Hz, auto mode):

| | Today (row bands, blocking) | After M0 (tiles + DMA + compositor) |
|---|---|---|
| Dirty area | ~full screen | ≤ 64 tiles (old+new per sprite) |
| Wire time / frame | ~22 ms | ~3.5 ms |
| CPU blocked / frame | ~22 ms (>130 % of budget) | < 1 ms (~5 %) |

Compositor row cost: per dirty row, bbox test × 8 + overlay of covered
sprites (≤ 32 px each). Rotation/scale/paint never appear here — they
live in the cached raster, regenerated only on change.

## 11. Phasing

Each milestone independently shippable; gate = all tests green + three
firmware presets linking (SRAM check) + reference sections (they *are*
the help text).

- **M0 — display pipeline:** tile dirty tracking, DMA line-pipelined
  blit, `setrefresh`/`refresh`/`refreshmode`, compositor carrying the
  existing single turtle. Acceptance: existing turtle tests pass; mock
  records blit rects proving small moves no longer dirty full-width
  bands; `repeat 3600 [fd 1 rt 1]` wall-clock improves on hardware.
- **M1 — sprite model + addressing:** `select` + per-turtle arrays,
  `tell`/`ask`/`each`/`who`, costume pool (mono compat + colour kinds),
  `setrot`/`setmag`, `stamp`/`snapsh`, z-order. Acceptance:
  single-turtle programs byte-identical; mock tests for fan-out, `each`
  narrowing, `ask` restore-on-throw, costume round-trips (`putsh`→
  `getsh`, `snapsh`→`stamp`).
- **M2 — sensing:** `touching?`, `over?`, `colourunder`, `distance`;
  wrap-mode cases; rotation-aware masks. All on mock.
- **M3 — autonomy + events:** `setspeed`/`speed`, `freeze`/`thaw`,
  `setanim`, `when` demons, idle-loop ticking, lifetime rules. Tests via
  mock time source + scripted conditions. **Landed:** `core/demons.c`
  (edge-triggered table, `MAX_DEMONS` 8, `DEMON_POLL_MS` 20 budget, fresh
  nested evaluator with re-entrancy suppression); poll at the instruction
  point and the picocalc prompt idle loop; new device ops `set_speed`/
  `get_speed`/`set_anim`/`turtle_tick` and a monotonic `ticks_ms` hardware
  op; error-unwind clears demons and stops motion, `cs` stops motion
  only (revisited 2026-07-18: `cleardemons` owns demon teardown, §7).
  Decisions made where
  the spec was open: speed is **turtle steps per second**, `setanim` is
  `setanim first last interval_ms` (frame range + ms per frame, 0 stops),
  and `freeze`/`thaw` suspend **both** demons and autonomous motion.
- **(P6 candidate, own design gate):** `launch` processes + broadcast
  messages (§8).

New reference sections: `tell`, `ask`, `each`, `who`, `touching?`,
`over?`, `colourunder`, `distance`, `when`, `setspeed`, `speed`,
`freeze`, `thaw`, `setanim`, `setrot`, `setmag`, `stamp`, `snapsh`,
`setrefresh`, `refresh`, `refreshmode`; updates to `setsh`/`getsh`/
`putsh` (costume lists, colour kinds) and `savepic` (sprites no longer
baked into saved images).

## 12. Risks and open questions

- **DMA + scroll interaction:** `lcd_blit` remaps y through the vertical
  scroll offset; the DMA path keeps that remapping in window setup (low
  risk; split-screen scroll matrix must run on hardware).
- **Idle-loop ticking** reaches into the device keyboard-wait path;
  needs care not to fight the screensaver/editor (they own the screen in
  their modes — ticking suspends outside graphics modes).
- **Wall-clock motion vs reproducibility:** `setspeed` advancement is
  Δt-scaled, so exact pixel trajectories vary with load; games don't
  care, but a `rerandom`-style deterministic test needs the mock's
  virtual clock (it has one).
- **Behaviour changes — signed off (2026-07-04):** (a) saved BMPs no
  longer contain the turtle glyph; (b) `dot?` no longer sees turtle
  pixels; (c) queries with multiple active turtles answer for the
  lowest; (d) colour costumes render verbatim, ignoring the pen colour
  (mono costumes still paint in it).
- **Decided (2026-07-04):** `tell` with a number outside
  `0..MAX_TURTLES-1` is `ERR_BAD_INPUT` — no silent clamping; programs
  stay portable to a future 16-turtle build and typos surface
  immediately.
- **Decided (2026-07-04):** `over?` / `colourunder` answer for the first
  active turtle only, like every other query; address a specific turtle
  with `ask 2 [over? 12]`. No variadic special case.
- **Decided (2026-07-04):** demons stay armed at the REPL prompt,
  Atari-style — actions may interleave with typed input echo; `freeze`
  is the escape hatch and `cs`/error-unwind clears them. (Revisited
  2026-07-18: `cs` no longer clears demons — `cleardemons` and
  error-unwind do; see §7.)

## 13. Rejected alternatives (summary)

| Alternative | Why not |
|---|---|
| Core-1 render loop | LCD shared with console/editor on core 0; DMA pipeline captures ~90 % of the win without cross-core locking. Revisit after M1 profiling. |
| 16-bpp shadow framebuffer | 200 KB — does not exist on this SRAM budget. |
| Per-scanline dirty x-extents / exact rect lists | More state and setup for no practical gain over 16×16 tiles. |
| Save-under sprites (extend today's model to N) | O(N) erase/redraw churn, ordering hazards, sprites contaminate `dot?`/`fill`/BMP, ~832 B/turtle. |
| FMSLogo `refresh`/`norefresh` toggle semantics | Presenting re-enables auto; policy and present are orthogonal here. |
| Index parameter on every turtle device op | Interface churn for no capability over `select`. |
| Atari numeric event codes for `when` | Needed an appendix table to read; expression conditions subsume the entire table. |
| Color Logo stroke-string shapes | 45°-stroke glyphs and XOR rendering; `snapsh` keeps the draw-your-own spirit with full colour. |
| TI pen-less sprites | Our sprites are turtles; a gliding pen-down turtle drawing a trail is a feature no hardware sprite could offer. |
| `hatch`/`launch` processes in P5 | Frame-arena SRAM and re-entrancy rules deserve their own gate; `setspeed` + demons deliver the felt concurrency first (§8). |
| Hardware-faithful limits (4/line, 1-bit, fixed 8×16) | The compositor is software; the limits were never the idea. |

## Sources

- [TRS-80 Color Logo manual (Tandy, 1982) — colorcomputerarchive.com](https://colorcomputerarchive.com/repo/Documents/Manuals/Programming/TRS-80%20Color%20Logo%20(Tandy).pdf) — HATCH, VANISH, ME, NEAR, SEND/MAIL, SHAPE strings, scheduler ("a turn is a single turtle command").
- [Atari LOGO: Introduction to Programming Through Turtle Graphics (Atari, 1983) — atarimania.com](https://www.atarimania.com/documents/Atari_LOGO_Introduction_to_Programming_Through_Turtle_Graphics.pdf) — TELL, SETSP, SETC, EDSH/GETSH/PUTSH/SETSH, WHEN demons, OVER/TOUCHING, Appendix B event table (joystick, once-per-second).
- [Sprites, a Turtle, and TI LOGO / TI Logo manual text — archive.org](https://archive.org/details/tibook_ti-logo) — TELL SPRITE / :ALL, CARRY, SETCOLOR, SETSPEED, SV/XVEL/YVEL, FREEZE/THAW, BIG/SMALL, MAKESHAPE, 4-per-line flicker, lower-number-on-top, no pen.
- [Antic v2n6 review: "Atari Logo Looking Good" — atarimagazines.com](https://www.atarimagazines.com/v2n6/logo.html) — dynamic turtles, demon overview.
- [MicroWorlds vocabulary (LCSI)](http://www.lcsi.ca/pdf/microworldsex/microworlds-ex-vocabulary.pdf) and [MicroWorlds — Wikipedia](https://en.wikipedia.org/wiki/MicroWorlds) — launch/forever/clickon process model.
- [Logo (programming language) — Wikipedia](https://en.wikipedia.org/wiki/Logo_(programming_language)) — dialect landscape, Acornsoft/BBC context.
- NetLogo `hatch`/`ask` (ccl.northwestern.edu) and Scratch (scratch.mit.edu) semantics from their current documentation.
