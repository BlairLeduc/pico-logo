# P9 — Tile maps and smooth scrolling (design)

Status: **v1 design, drafted 2026-07-29. M0 measured 2026-08-01 — gate
FAILED (§3.3), design split (§3.4).** The bake half (`stampmap`/`stamptile`,
the C map, the render-only Turtle Trails revamp) proceeds on its own
schedule; the scrolling half (§5.3, §10) is blocked on
[P10](interpreter-throughput-design.md), whose M3 re-measure is the reopen
gate. §13's milestones are resequenced accordingly. Three scoping decisions
were taken with the user on 2026-07-29:

- **Available on all three boards**, with tiered capacity (small SRAM tier
  everywhere, large PSRAM tier on the Pico Plus 2 W) — not PSRAM-gated.
- **Turtle Trails keeps its gameplay**: same 28×36 on-screen board, no
  scrolling; its revamp uses the tile system only to build and repair the
  board and to replace its Logo-list map with the C map.
- **Both game revamps replace the shipped games in place** (`logo/games/trails`,
  `logo/games/checkrun`), not as parallel versions.

This is the revision that `checkpoint-run-design.md` §4.2 demanded before any
tilemap primitive: the case made with numbers from both games.

## 1. Goal

Make scrolling tile games — the Rally-X / Super Mario Bros. shape — practical
on the PicoCalc:

- a **tile bank** of small square tiles, drawn with the pen and snapped out of
  the canvas (the `snapsh` idiom applied to background art);
- a **map** much larger than the screen, one byte per cell;
- a **view** into the map at an arbitrary *pixel* offset, so scrolling is
  smooth rather than tile-stepped, confined to a rectangular viewport so a
  HUD can live beside it;
- a **bake path** that renders the map into the ordinary canvas once, for
  games (Turtle Trails) that want tile-built boards without a live scrolling
  background.

Nothing here is a new kind of graphics. It is the existing compositor
(`multi-sprite-design.md` §2.2/§2.4) given a second background source.

## 2. The insight: the display already composites

Since P5/M0, `gfx_buffer` holds only the canvas. Each outgoing row is built by
`compose_row()` (`devices/picocalc/screen.c`): one `memcpy` from the canvas,
then visible sprites overlaid per scanline, then palette expansion into the
DMA-fed blit pipeline (`lcd_blit_row`).

A scrolling background is the same operation one layer lower. When a map view
is active, the columns of row *y* that fall inside the viewport are filled by
sampling the tile bank at `(scroll_x + x − view_x, scroll_y + y − view_y)`
instead of the `memcpy`; columns outside the viewport still come from the
canvas. Sprites overlay both, exactly as today.

Consequences:

- **Scrolling costs nothing per pixel of offset.** The picture is never
  stored, only generated at blit time. `setscroll` is two integer writes plus
  marking the viewport dirty.
- **The canvas is untouched.** Pen drawing, `dot?`, `fill`, `savepic`, and
  `loadpic` keep seeing the canvas alone, exactly as they already see it free
  of sprites. `hidemap` restores the canvas view with no redraw cost beyond
  one present.
- **Row building stays cheap.** Filling a viewport row is ~`width/tile_size`
  16- or 8-byte copies from the bank plus two partial tiles — a few µs per
  row, well under the palette expansion that already runs per row.

## 3. Feasibility: the numbers (M0)

The cost is **the wire, not the CPU**. A scrolled frame dirties the whole
viewport, and the LCD SPI runs at 75 MHz / 16 bpp ≈ 4.69 Mpx/s
(`lcd.h LCD_BAUDRATE`). The blit is DMA-pipelined but the interpreter sits
inside the loop, so the wall time is interpreter time:

| Present region | Pixels | Wire wall time | Left of a 25 fps frame (40 ms) |
|---|---:|---:|---:|
| Full screen 320×320 | 102,400 | ~21.9 ms | ~18 ms |
| Checkpoint Run view 256×320 | 81,920 | ~17.5 ms | ~22 ms |
| One 16×16 tile (today's granule) | 256 | ~55 µs | — |

*(Measured on a Pico 2 at §3.3: 25.6 ms and 21.1 ms — this table's rate is
17–21 % optimistic. The reasoning below stands; the margins do not.)*

Map row sampling adds ~1–1.5 ms of CPU per full frame on top; palette
expansion (~10 µs/row) is already paid today and overlaps the DMA. 30 fps
full-screen (33.3 ms budget) leaves ~11 ms of Logo — not enough for a real
game loop; **25 fps is the design cadence**, matching the shipped games'
`(setrefresh "sync 25)`.

Audio is safe: interrupts stay **enabled** during blits (`lcd.c` §DMA notes —
the ST7365P tolerates SPI clock pauses mid-window), so the P8 sound ring is
serviced normally through sustained full-screen presents.

### 3.1 What M0 measures, and the gate

All on a Pico 2, with `ticks`; record the results in this section.

1. **Full-screen present wall time — measurable today, zero new code:**
   `setrefresh "manual`, change every pixel, then time `refresh`. Expected
   ~22 ms; this is the dominant term and validates the wire math above.
2. **A representative game frame body:** time one frame of the shipped
   Turtle Trails loop (5 actors) and, if available, a Checkpoint Run
   milestone-1 frame (7 cars) — the Logo side must fit in the remainder
   column above.
3. **The §4.2 before-numbers:** Checkpoint Run's sector rebuild
   (`draw.sector`, ~400 stamps; target was <120 ms) and Turtle Trails'
   `draw.board`, both timed, so the before/after comparison the Checkpoint
   Run design demanded is on the record.
4. **Audio integrity:** `play` a phrase while looping full-screen `refresh`;
   confirm no audible corruption.

**Gate:** proceed to M1 if (present wall time) + (game frame body) ≤ 40 ms
with ≥20 % headroom for both target games. Otherwise apply §15 levers and
re-measure before building any surface.

### 3.2 The harness

Three throwaway scripts, all running on today's firmware:

| Script | Gives |
|---|---|
| `logo/tests/p9m0` → `p9m0` | items 1 and 4 |
| `logo/games/checkrun` → `p9m0.checkrun` | items 2 and 3, Checkpoint Run |
| `logo/games/trails` → `p9m0.trails` | items 2 and 3, Turtle Trails |

`p9m0` works the present cost out by difference. A present only sends tiles
that are dirty, so each pass must repaint first; the repainting is timed a
second time on its own and subtracted, leaving the presents alone, and twenty
passes are averaged because `ticks` is whole milliseconds against a ~22 ms
quantity. It dirties an exact band — the leftmost *n* of the 20 tile columns,
full height — so the road-view viewport (16 columns, 256 px) is measured, not
extrapolated, and the per-column slope and the fixed per-present cost fall out
of the difference between bands. It runs in `window` boundary mode: under the
default `wrap` a stroke's round cap spills past the left edge to the right one
and dirties the whole tile row, which would make every band measure as a full
screen.

The two game entries set a round up from cold and time `draw.sector` /
`draw.board` and ten `play.frame`s in `manual` refresh, so the `sync` ending a
frame presents and returns instead of waiting for the 25 fps boundary — the
number wanted is the work, not the cadence. The frame figures include today's
small dirty-rect present, so they are an **upper bound** on the body that a
full-viewport present would be added to.

### 3.3 Results

Pico 2, 2026-08-01. Items 1 and 4 taken; items 2 and 3 still to come.

| Measurement | Expected | Measured |
|---|---:|---:|
| Present, full screen 320×320 | ~21.9 ms | **25.6 ms** |
| Present, road view 256×320 | ~17.5 ms | **21.1 ms** |
| Present, strip 64×320 | ~4.4 ms | 5.45 ms |
| Per tile column (16×320) | ~1.1 ms | 1.259 ms |
| Fixed cost per present | — | 0.41 ms |
| Checkpoint Run `play.frame`, 7 cars | ~22 ms | **258.6 ms** |
| Checkpoint Run `draw.sector`, ~400 stamps | <120 ms budget | **1,346 ms** |
| Turtle Trails `play.frame`, 5 actors | <40 ms | **87.3 ms** |
| Turtle Trails `draw.board` | — | **5,916 ms** |
| Audio under sustained presenting | clean | **clean** |

**A present costs 17–21 % more than §3's wire math.** The band figures are
internally consistent (slope 1.259 ms per tile column, intercept 0.41 ms), so
this is not measurement noise: the effective rate is **4.0 Mpx/s**, not the
4.69 Mpx/s the raw 75 MHz / 16 bpp figure gives. The difference is the work §3
treated as free or overlapped — `compose_row`'s canvas `memcpy` and palette
expansion per row, and the twenty *separate* `lcd_blit_begin`/`end` windows a
full-height present costs, since `dirty_tiles` tracks one span per 16-px tile
row and each becomes its own window. The near-zero intercept says the cost is
per pixel, not per present, so narrowing a viewport still buys back its area
share exactly as §15's first lever assumes.

Audio was clean through twelve seconds of flat-out full-screen presenting, as
§3 predicted: interrupts stay enabled during a blit.

**Consequence for the budget.** At 25 fps the gate allows 32 ms of the 40 ms
frame, so the Logo body must fit in **6.4 ms** full-screen or **10.9 ms** in
Checkpoint Run's road view — against §3's advertised ~18 ms and ~22 ms. The
road-view layout is now doing much more than banking a 20 % nicety; it is most
of the remaining headroom.

**The premise of §3 is wrong: the cost is the CPU, not the wire.** A present
is 21–26 ms. A frame body is 87 ms in Turtle Trails and 259 ms in Checkpoint
Run — three to ten times the present it was supposed to fit beside.

| Game | Present | Body | Total | Against the 32 ms gate |
|---|---:|---:|---:|---:|
| Turtle Trails | 25.6 ms | 87.3 ms | 112.9 ms | 3.5× over |
| Checkpoint Run | 21.1 ms | 258.6 ms | 279.7 ms | 8.7× over |

Neither game has ever met its frame budget, and **neither had ever been timed
on hardware** — `checkpoint-run-design.md` §4.2 says the rebuild "is the one
number in the design that is not yet measured", §13 lists profiling as
outstanding, and `turtle-trails-design.md` §11 likewise says only "profile
with `ticks`". These are the first hardware numbers for either game. They ship
at `(setrefresh "sync 25)` and actually run at about 9 fps and 4 fps; `sync`
does not wait when a frame overruns, so the games degrade quietly rather than
failing, which is why this went unnoticed.

The before-numbers are correspondingly worse than their designs assumed: the
sector rebuild is **1,346 ms against a 120 ms budget** — 11× over, and the
three §4.2 tuning levers were sized for a 120 ms problem — and Turtle Trails'
`draw.board` is **5,916 ms**, a six-second pause at every level start.

**Gate verdict: FAIL, and no §15 lever can rescue it.** Every lever narrows
the *present*, and the present is not the problem: set it to zero and both
games still miss the 32 ms gate by 2.7× and 8.1×.

### 3.4 What this changes

The finding splits the design cleanly in two, and the halves now point
opposite ways.

- **The scrolling premise fails.** §2's "scrolling costs nothing per pixel of
  offset" is still true, and irrelevant: a game that cannot reach 25 fps
  cannot scroll smoothly at 25 fps. Live map viewing (§5.3), `setscroll`, and
  the Checkpoint Run camera (§10) rest on a per-frame budget that does not
  exist.
- **The bake premise gets stronger.** `stampmap`/`stamptile` (§5.4) replace a
  1,346 ms rebuild and a 5,916 ms board build with a C loop. Those are the
  largest single stalls measured anywhere in the project, they are not
  per-frame costs, and they need no frame budget at all. This half of the
  design is *more* justified by the measurement, not less.

What the numbers actually argue for is interpreter throughput as the
prerequisite item, with the tile **bake** path as an independently worthwhile
piece of P9 that can proceed on its own. That became
**[P10](interpreter-throughput-design.md)** on 2026-08-01; §3.5 below is the
evidence it was opened on. The split it leaves P9 with:

- **Scrolling (§5.3, §10) is blocked on P10.** It needs a per-frame budget
  that does not exist yet.
- **The bake path (§5.4) is not blocked** and should proceed on its own
  schedule — it needs no frame budget, and it deletes the two largest stalls
  measured anywhere in this project.
- **The C map (§5.2) is complementary to P10, not redundant** — `tile.at`'s
  36+28 cons-cell walk sits inside `step.bugs`, which is 59 % of a Turtle
  Trails frame.

How much P10 buys P9, sized from its design (§7 there): its M1+M2 target
48 % of runtime, an upper bound of ~1.9× — a Turtle Trails frame goes from
87.3 ms to ~46 ms against the 40 ms budget. Close, but not clear on its own;
the C map attacks the remainder from the data side (the `tile.at` walk above),
and P10's §7 names it as one of the levers expected to finish the job. So the
two items converge on Trails from opposite ends. Checkpoint Run (258.6 ms) is
harder: it needs P10, the camera retrofit deleting its sector machinery (§10),
*and* game-side simplification — no single item covers a 6.5× shortfall.
**P10's M3 re-measure is the checkpoint where P9's scrolling half reopens:**
if a frame body fits under 40 ms there, a per-frame budget exists and §5.3/§10
proceed against real numbers.

### 3.5 Where the time actually goes

Answered on the host rather than by another hardware run. The host build runs
the same interpreter against the mock device, where drawing is a recorded
command instead of a rasterised one, so host timings isolate interpreter cost
from rasterisation.

| | Host | Pico 2 | Ratio |
|---|---:|---:|---:|
| Turtle Trails `play.frame` | 0.96 ms | 87.3 ms | 91× |
| Turtle Trails `draw.board` | 55.1 ms | 5,916 ms | 107× |
| Checkpoint Run `play.frame` | 3.42 ms | 258.6 ms | 76× |
| Checkpoint Run `draw.sector` | 18.0 ms | 1,346 ms | 75× |

Two things follow. **Rasterisation is a minor term** — the ratio barely moves
between a frame that draws almost nothing and a board build that draws
everything, so what the Pico is slow at is *interpreting*, not plotting
pixels. And **Checkpoint Run's 258.6 ms is an ordinary frame**: 400 host
frames produced *zero* sector crossings, because an unattended car stops at
the first wall. No rebuild is hiding in that mean.

Within a frame, the cost is simulation the tile system does not touch —
Turtle Trails: `step.bugs` 0.56 ms (59 %), `place.all` 0.26 ms (28 %),
everything else under 0.1 ms.

**The interpreter profile.** Sampling a pure `repeat [make "x (:x + 1)]` loop
(no device work at all) gives:

| Cost | Share |
|---|---:|
| Re-lexing and classifying words on every evaluation | 34 % |
| Name resolution by case-insensitive string compare | 14 % |
| Cons-cell walk and index→pointer indirection | 20 % |
| `memmove`/`memset` | 8 % |
| Actual evaluation and everything else | 24 % |

`classify_word` is the single largest leaf. The interpreter re-derives, on
*every* pass through a loop body, what each word already is — number or not,
delimiter, comment, infix — and then finds each primitive by `strncasecmp`
against the registry. Words are interned atoms, so all of this is a pure
function of the atom and could be computed once. That is roughly **48 % of
runtime spent rediscovering facts that do not change**, and it is a
memoisation problem, not an interpreter rewrite. (P10's design review later
split the 14 % bucket: part of it is *variable* resolution — `make "x`, `:x`
— which is dynamically scoped and cannot be cached on the atom; its §7.)

## 4. Availability and the memory plan

**The feature exists on all three boards.** Rendering needs no PSRAM — the
row sampler reads a few-KB bank. Memory only limits *capacity*, so capacity
tiers exactly like the HTTP transfer buffer (`core/limits.h`
`HTTP_MAX_BODY` / `HTTP_MAX_BODY_PSRAM`; allocation pattern of
`http_io_init` in `core/primitives_http.c`):

- On first `newtiles`/`newmap`, try `mem_region_alloc` (the PSRAM aux
  region). If present, the large tier applies.
- Otherwise fall back to a one-time process-lifetime SRAM heap allocation of
  the small tier. **No static arrays** — PSRAM boards reserve no SRAM, and
  boards that never touch tiles pay nothing.

Caps live in `core/limits.h`:

| Limit | SRAM tier | PSRAM tier |
|---|---:|---:|
| `TILE_BANK_SIZE` | 4 KB | 64 KB |
| `TILE_MAP_SIZE` | 4 KB | 256 KB |

Capacity that buys, per tile size (bank slots are uniform, so a slot is
`size²` bytes and the bank needs no compaction — slot *n* lives at
`pool[n·size²]`):

| | 8×8 tiles | 16×16 tiles |
|---|---:|---:|
| SRAM bank slots | 64 | 16 |
| PSRAM bank slots | 255 | 255 |
| SRAM map | 4,096 cells (e.g. 64×64) | same |
| PSRAM map | 262,144 cells (e.g. 512×512) | same |

Both revamped games fit the SRAM tier with room to spare: Checkpoint Run's
map is 32×40 = 1,280 B and needs ~10 of 16 slots at 16×16; Turtle Trails'
map is 28×36 = 1,008 B and needs ~20 of 64 slots at 8×8. `bss` on `pico2` is
485,236 B of 532,480; the 8 KB SRAM tier arrives on the heap, and the M1
acceptance check is that all three firmware presets still link and boot.

**Overflow:** `newmap` with `cols × rows` over the active tier's cap, or
`snaptile` into a slot beyond the bank's capacity, is `ERR_OUT_OF_SPACE`,
and the reference states the per-board capacities plainly, as it does for
HTTP bodies.

**PSRAM speed:** a PSRAM board always uses the PSRAM tier, and the latency
difference is hidden by construction. Bank reads go through the XIP cache
(hits at SRAM speed; a frame's visible-tile working set is ~5 KB against the
16 KB cache, walked row-major), and even an all-miss row build (~5–10 µs)
disappears under the ~68 µs the previous row spends on the SPI wire (§3).
Per-frame bank traffic is ~2.5 MB/s at 25 fps — a rounding error against QMI
read bandwidth. The one real hazard is bus contention with flash code fetch:
the sampler must be RAM-resident (`__not_in_flash_func`), the discipline
`lcd_blit_row` already follows (§7).

## 5. Model

### 5.1 Tile bank

- **Tile size is per-bank, 8 or 16**, set by `newtiles size`. This deviates
  from the roadmap's "one size chosen at M0" deliberately: the two flagship
  games need different sizes (Trails is an 8-px grid, Checkpoint Run 16-px),
  and the sampler cost is identical — size is a shift, not a branch.
  `newtiles` clears the bank; one live size at a time.
- Slots are `1..capacity−1`; **slot index is the map cell value**, so 255 is
  the highest addressable tile and cell `0` is reserved (§5.2).
- **Filled by capture:** `snaptile n` copies the `size×size` canvas region
  centred on the first active turtle into slot *n* — same region math as
  `snapsh n size size`, but **opaque**: every byte is copied verbatim,
  including the background slot. Tiles are background, not sprites; there is
  no transparent index. A game ships without a picture asset: draw each tile
  with the pen, snap it, clear, repeat.
- The bank is 8-bit palette-indexed like the canvas; `setpalette` recolours
  tiles on screen exactly as it recolours the canvas.

### 5.2 Map

- `newmap cols rows` allocates and zeroes `cols × rows` bytes, one byte per
  cell. Cell values: `0` = paint the current background colour (a plain
  `memset` run in the sampler); `1..255` = bank slot. A cell naming an empty
  slot renders as background.
- `settile col row n` / `tile col row` write and read cells, **1-based**
  like `item`. Out-of-range col/row or a cell value with no meaning to the
  renderer is the game's business; `settile` validates only bounds and
  `0..255`.
- **The map is a live data structure, not just a picture.** `tile col row`
  is O(1), so games use the map itself for collision and world queries
  instead of a parallel Logo list — Turtle Trails' decoded nested list
  (~1,050 cons cells) disappears entirely (§11).

### 5.3 View: viewport, scroll, wrap

- `showmap` activates the map as the background source over the whole
  graphics area; `(showmap x y w h)` restricts it to a viewport rectangle in
  screen pixels (top-left origin), so a HUD column or status rows stay
  canvas. `hidemap` restores the canvas everywhere.
- `setscroll x y` sets the world-pixel coordinate that appears at the
  viewport's top-left; `scroll` outputs `[x y]`. Coordinates are map-space:
  x rightward, y **downward**, matching `col`/`row` — these primitives
  address the raster, not the turtle plane, and the games convert once.
- **Sampling always wraps**, modulo the world's pixel size, in both axes.
  There is no wrap flag: a bounded world (Mario, Rally-X) is achieved by the
  game clamping its own `setscroll` values, which it must compute anyway to
  follow the player. A world narrower than the viewport tiles repeatedly —
  harmless, occasionally useful (pattern backgrounds).
- A scroll change marks the viewport rect dirty; a `settile` while shown
  marks the cell's on-screen rect (or the viewport, when wrap makes the cell
  visible more than once). Present then happens through the ordinary
  refresh policy — `sync`-mode games see the new offset at their next
  `sync`, manual games at `refresh`.

### 5.4 The bake path: `stampmap` and `stamptile`

For tile-built boards that do not scroll (Turtle Trails):

- `stampmap` renders the map once **into the canvas** through the current
  scroll and viewport, exactly as the live sampler would draw it. It is the
  tile-system's `loadpic`: a C-speed board bake replacing hundreds of Logo
  stamps or pen sweeps, after which the map may be shown live or not at all.
- `stamptile col row` re-bakes one cell at its current on-screen position —
  the repair operation (a collected item, an expired smoke cloud, an erased
  blossom) — and is a no-op for a cell outside the viewport.

Baked pixels are ordinary canvas: pen trails draw over them, `dot?` sees
them, `savepic` keeps them. This is what lets Trails keep its identity —
*the turtle's own pen paints the trail* — on a tile-built board.

## 6. Logo surface

`window` and `map` are taken by existing primitives; none of these names
collide (checked against the primitive registry).

| Primitive | Inputs | Does |
|---|---|---|
| `newtiles size` | 8 or 16 | clear the bank, set the tile size |
| `snaptile n` | slot | capture the tile under the first active turtle |
| `newmap cols rows` | ≥1 each, product ≤ tier cap | allocate and zero the map |
| `settile col row n` | 1-based; 0..255 | write a cell |
| `tile col row` | 1-based | output a cell (the O(1) world sensor) |
| `setscroll x y` | world pixels | move the view; wraps modulo world |
| `scroll` | — | output `[x y]` |
| `showmap` / `(showmap x y w h)` | screen px | map becomes the background in the viewport |
| `hidemap` | — | background reverts to the canvas |
| `stampmap` | — | bake the map into the canvas through scroll+viewport |
| `stamptile col row` | 1-based | re-bake one cell into the canvas |

Errors: `newtiles` with a size other than 8/16, `snaptile`/`settile`/`tile`
out of range → `ERR_DOESNT_LIKE_INPUT`; allocation over the tier cap →
`ERR_OUT_OF_SPACE`; map/view operations before `newtiles`/`newmap` →
`ERR_DOESNT_LIKE_INPUT` with the usual "I need a map first" phrasing rules.
All eleven get reference sections (the reference is the help text).

## 7. Architecture

- **`core/tilemap.c`** owns everything device-independent: the bank pool,
  the map, tile size, view state (shown, viewport, scroll), the caps and
  tier selection, and the row sampler
  `tilemap_fill_row(uint8_t *dst, int y, int x0, int x1)` that fills the
  intersection of a compose span with the viewport. Being core, the whole
  storage and sampling layer unit-tests natively with no mock. The sampler
  is marked `__not_in_flash_func` like `lcd_blit_row`: on the PSRAM board
  its data reads share the QMI bus with flash code fetch, and the hot loop
  must not add instruction misses on top (§4).
- **`compose_row()`** (`devices/picocalc/screen.c`) asks the sampler first:
  columns it fills are done; the remainder (outside the viewport, or map
  hidden) comes from `gfx_buffer` as today. Sprites overlay afterwards,
  unchanged. `stampmap`/`stamptile` reuse the same sampler writing into
  `gfx_buffer` instead of the compose buffer.
- **One new console op, `map_changed()`**: core state changes (scroll,
  settile, show/hide) notify the device, which computes the dirty rect from
  core accessors and marks tiles. The mock records the calls; the host
  console (no graphics) leaves view primitives erroring as turtle
  primitives do there, while `newmap`/`settile`/`tile` still work as pure
  storage.
- Sprites are **not clipped to the viewport** in v1 (the compositor has no
  clipping window today either — Turtle Trails §6.3 accepted the same edge
  bleed at its tunnels). A game whose viewport abuts a HUD hides sprites
  whose box crosses the seam; Checkpoint Run's cars pop at most one
  car-width early (§10).

## 8. Interactions with existing semantics

- **Canvas primitives are oblivious.** Pen drawing, `fill`, `dot?`,
  `colourunder`, `over?`, `savepic`, `loadpic` see the canvas only, map
  shown or not. Map sensing is `tile col row` — deterministic, O(1), and
  exactly the "map is the only source of truth" discipline the Checkpoint
  Run design already mandates. `touching?` (sprite vs sprite) is unaffected.
- **Refresh policy:** unchanged. `sync` mode is the game cadence; the map
  source only changes what a dirty tile's rows are built from.
- **Split screen / text mode:** the viewport is clipped to the graphics
  region exactly as blits already are (`screen_gfx_blit_dirty`'s limit); in
  text mode nothing presents, as today.
- **Palette:** shared. Tiles are indices; games own the palette as they do
  now.

## 9. Lifecycle

- **Boot:** no bank, no map, view hidden.
- **`cs`/`draw`:** clears the canvas as always; the bank, map, and view
  survive, like costumes (a screen clear must not tear down a game's
  world). While the map is shown, `cs` therefore visibly clears only the
  canvas regions.
- **Error unwind to toplevel / `throw "toplevel`:** `hidemap` runs (the
  `setrefresh "auto` precedent — the machine at the prompt shows the
  canvas), but bank and map **data survive** so a program can be debugged
  and `showmap` re-issued.
- **`newtiles` / `newmap`:** each re-initialises its own half. The planned
  `.reset` command frees both.

## 10. Checkpoint Run revamp

The camera replaces sector paging; the play pattern is otherwise the revised
design, unchanged.

- **View:** `(showmap 0 0 256 320)` — the road view; the instrument column
  stays canvas and turtle 7's HUD/radar code is untouched. The camera
  follows the car, clamped to `[0..256]×[0..320]` (world 512×640 px), so
  the world stays bounded exactly as §3.1's closed edge demands.
- **Map:** the 32×40 world, 16×16 tiles. The authored 40-row hex words
  remain the source of truth for *connectivity* (`road.at`, AI, driving);
  at round start they are decoded once into the C map as **visual** cells —
  road, off-road (`0`), shoulder — and a test asserts the two agree tile by
  tile, which is the §16 lesson made structural.
- **Items become cells:** checkpoints, the Turbo checkpoint, rocks, and
  smoke are `settile` writes (collect = write road back; smoke animates by
  alternating two smoke slots on its 200 ms clock). Logical state stays in
  the `flag.*`/`rock.*`/`smoke.*` lists exactly as designed; the ~10-slot
  tile set replaces shape slots 4–8, freeing sprite slots.
- **Deleted:** `draw.sector`, `restore.sector`, the sector-switch sequence
  (§3.1 steps 1–7), the 120 ms rebuild budget, and the ~400-stamp rebuild —
  the numbers M0 records become the before/after. Sector *authoring* rules
  in §3.3 survive only as world-connectivity rules.
- **Cars:** world coordinates as designed; screen position is
  `world − scroll` each frame, hidden when the box leaves the viewport
  (crossing the panel seam hides one car-width early, §7).
- **Frame budget:** ~17.5 ms present + player, six enemies, radar and HUD
  deltas in the remaining ~22 ms at 25 fps. M0's measured frame body
  decides whether this holds before any game code changes. *M0 decided: the
  body is 258.6 ms (§3.3), so this retrofit waits on P10 plus game-side work
  (§3.4) and is P9's last milestone (§13).*
- The design doc gains an as-revamped section recording this, and its §4.2
  closes with the measured comparison.

## 11. Turtle Trails revamp (render-only, gameplay identical)

Per the scoping decision: same 28×36 board, no scrolling, byte-identical
rules, speeds, AI, and sound. The tile system replaces how the board is
*stored and drawn*:

- **The C map replaces the decoded Logo map.** The 36 encoded words decode
  into `settile` writes at level setup; `tile.at`/`set.tile` become
  `tile`/`settile`. This frees ~1,050 cons cells of the game's main
  persistent allocation, removes the 36+28 cons walk per junction lookup,
  and further relaxes the frame-loop reclaim pressure recorded in §15 of
  its design.
- **The board is baked, not carved.** `newtiles 8`; at setup the game draws
  its tile set with the pen and `snaptile`s it (~20 slots: hedge core,
  edges and corner variants for the rounded look, path, speck, tunnel,
  door, nest, blossom quarters), computes each cell's visual variant from
  its neighbours once, and `stampmap` bakes the board. The two-pass pen-8
  corridor carving in `draw.board` goes; the bake is a C loop plus one
  present.
- **Pen trails stay pen trails.** The baked board is canvas, so painting is
  unchanged — trail over speck, exactly as shipped. Blossom eating becomes
  `settile` + four `stamptile`s instead of the disc blackout, then relays
  the half-trail as today.
- Shape slots 7–8 free up (bonus shape stays); tests swap list assertions
  for `tile` assertions and drop the map-decode invariants in favour of
  bank/map ones.

## 12. Budgets

SRAM (`pico2`, `bss` 485,236 B of 532,480 at design time):

| Item | Delta |
|---|---|
| Bank + map, SRAM tier (heap, on first use) | +8 KB heap |
| Core tilemap state (dims, view, tier pointers) | +~40 B |
| Static arrays | **0 B** (lazy allocation, HTTP precedent) |
| Turtle Trails node pool relief | −~1,050 cons cells at runtime |

Frame, 25 fps, worst realistic case (full-viewport scroll every frame):

| | Sector paging (Checkpoint today) | Map view (after) |
|---|---|---|
| Ordinary frame present | small dirty tiles, ~1–3 ms | full viewport, ~17.5–22 ms |
| Worst hitch | sector rebuild, ~400 stamps, ≤120 ms budget | none — worst frame = ordinary frame |
| Scroll cost | n/a (16-px steps at sector edges only) | 2 int writes + dirty mark |

The trade is explicit: every frame pays the wire, and in exchange the 120 ms
class of hitch and the entire rebuild machinery disappear.

## 13. Milestones

Resequenced 2026-08-01 after the M0 verdict: the unblocked bake half runs
first, the live view waits for P10 to create the frame budget it needs.

- **M0 — measure and gate: done, FAILED (§3.3).** The split in §3.4
  reordered everything below.
- **M1 — bank and capture:** `core/tilemap.c` storage + tiering,
  `newtiles`/`snaptile`, limits, mock recording, native tests, all three
  presets link and boot. Not blocked.
- **M2 — map storage and the bake path:** `newmap`/`settile`/`tile`, the
  sampler (writing into `gfx_buffer` via `stampmap`/`stamptile`), lifecycle
  (§9), reference sections for the storage and bake primitives. Native
  sampler tests cover offsets, wrap seams (x, y, corner), partial tiles,
  cell 0, empty slots, both tile sizes — the full corpus, exercised through
  the bake path. Not blocked.
- **M3 — Turtle Trails revamp (§11):** render-only, needs nothing beyond
  M2. Deletes the 5,916 ms `draw.board` and the ~1,050-cell Logo map;
  before/after numbers beside M0's. Not blocked — and it lands the C map
  inside `step.bugs`, the data-side half of the Trails frame problem
  (§3.4).
- **M4 — live view and scrolling — GATED on P10 M3:** `showmap`/`hidemap`,
  viewport clipping, `compose_row` integration, `map_changed` and dirty
  marking, `setscroll`/`scroll` wrap rules, remaining reference sections.
  Proceeds only when P10's re-measure shows a frame body under 40 ms. Also
  the A/B check on the Pico Plus 2 W: time the same scrolled frame from the
  PSRAM tier and confirm the present stays wire-bound (§4) — M0 ran on the
  SRAM tier and does not cover this by itself.
- **M5 — Checkpoint Run revamp (§10):** the camera replaces sector paging,
  with before/after numbers; full test suite rework; hardware soak on a
  Pico 2. Needs M4 *and* the game-side work §3.4 records — P10 alone does
  not rescue a 6.5× shortfall.

## 14. Tests

- **Native, no mock:** everything in `core/tilemap.c` — tier caps and
  fallback, allocation failures, bounds, slot arithmetic, and the sampler
  against hand-built expected rows (the same style as `test_dirty_tiles.c`).
- **Mock:** primitive surface errors, `snaptile` capture from the mock
  canvas, `map_changed` notifications and dirty rects, lifecycle across
  `cs`/error-unwind, `stampmap`/`stamptile` pixels landing in the mock
  canvas.
- **Games:** Checkpoint Run's "rendering agrees with the map" group asserts
  the decoded visual map against the authored words; Trails' map invariants
  move onto `tile`. Both keep the every-procedure-executed rule.

## 15. Levers if M0 misses

In order; each narrows the genre, so only with numbers in hand:

1. **Narrow the viewport** — present cost is linear in viewport area (the
   Checkpoint Run layout already banks 20 % this way).
2. **Half-rate scrolling** — scroll changes at 12.5 Hz while sprites present
   at 25 fps; ordinary frames go back to tile-sized dirty rects.
3. **Vertical-only smooth scroll via the panel** — `lcd_define_scrolling`
   already remaps blit rows through the hardware start-line; shifting it
   makes vertical scroll nearly free and only the newly exposed band blits.
   Costs horizontal motion and fights the split-screen text scroll, so this
   is a last resort.

## 16. Rejected alternatives

| Alternative | Why not |
|---|---|
| PSRAM-only feature | Rendering never needed PSRAM; the flagship games fit the SRAM tier; tier-gating would fork the game library across boards. |
| One compile-time tile size | The two flagship games need 8 and 16; per-bank size is a shift with no per-pixel cost. |
| Static SRAM arrays for bank/map | 8 KB of `bss` on every board including ones never using tiles; the HTTP lazy-tier precedent costs nothing when unused. |
| Compacting variable-size bank (costumes.c model) | Uniform tiles make slots fixed-stride; no offsets, no compaction, less code. |
| Wrap flag / clamped sampling in C | Games must compute camera clamps anyway; modulo sampling is one rule with no state. |
| Canvas composited *over* the map (transparency) | A per-pixel background test in the hot row path, and muddy semantics for `dot?`/`fill`; the viewport split is exact and free. |
| Solid/attribute bits in cells | Tile index semantics belong to the game; both games already keep logical state in Logo, and `tile` is a complete sensor. |
| `over?`/`colourunder` reading the map | Ambiguous under scroll and redundant next to `tile col row`; canvas-only sensing stays deterministic. |
| World-anchored turtles (`setworldpos`) | One subtraction per visible sprite per frame in Logo; not worth new per-turtle state and semantics. |
| A scrolling Turtle Trails garden | Considered and declined by the user 2026-07-29 — whole-maze visibility is genre-defining for maze-chase; revisit as a *new* game if wanted. |

## 17. Roadmap gate questions, resolved

| Open question (roadmap P9) | Resolution |
|---|---|
| Tile size | Per-bank, 8 or 16, `newtiles size` (§5.1). |
| Where the map lives, per-board size | Tiered lazy allocation, `TILE_BANK_SIZE`/`TILE_MAP_SIZE` (+`_PSRAM`) in `core/limits.h` (§4). |
| Bigger-than-screen only, or wraps | Sampling always wraps; bounded worlds clamp their own scroll (§5.3). |
| "Solid" bit for collision | No — `tile col row` is the sensor; semantics are the game's (§16). |
| How `touching?`/`over?`/`colourunder` read a map background | They don't; canvas/sprite only, documented (§8). |
| What `cs` and error unwind do | `cs` preserves bank/map/view; unwind hides the map but keeps data; `newtiles`/`newmap`/`.reset` free (§9). |

## References

- [Roadmap P9](roadmap.md#p9--tile-maps-and-smooth-scrolling-design-first) —
  the item this design closes the gate for.
- [Multi-sprite design](multi-sprite-design.md) §2 — the compositor and DMA
  blit pipeline this extends; §10 budget method.
- [Checkpoint Run design](checkpoint-run-design.md) §4.2 — the demand for
  this revision; §3 the world model the revamp keeps.
- [Turtle Trails design](turtle-trails-design.md) §15 — the as-built board
  drawing and map storage this replaces.
- `devices/picocalc/screen.c` (`compose_row`), `devices/picocalc/lcd.c`
  (blit pipeline, `LCD_BAUDRATE`, hardware scroll),
  `core/primitives_http.c` (`http_io_init` — the tiering pattern),
  `devices/picocalc/costumes.c` (the capture-pool precedent).
