# PicoCalc hardware integration — what we know

Written 2026-08-05 as a knowledge transfer out of [Pico Logo](../../pico-logo/README.md) and
into a new C sprite/tile/PSG game engine for the PicoCalc. Everything here was
learned by building and measuring on real hardware; where a number is a
measurement it says which board it came from and when, and where it is an
estimate it says so.

The audience is someone writing a game engine in C against the Pico SDK. Pico
Logo is an interpreter, so its frame budget was dominated by interpretation —
a C engine deletes that term entirely and inherits only the hardware costs.
That distinction matters when reading §9: **most of what limited games here
does not limit you; the wire and the audio deadline do.**

---

## 1. The machine

The PicoCalc is a carrier board: a Raspberry Pi Pico–form-factor RP2350 module,
a 320×320 SPI LCD, a 4×N keyboard behind a small on-board microcontroller
("southbridge"), a stereo PWM audio out, an SD card slot, and a battery gauge.
The RP2350 talks to all of it over three buses.

| Peripheral | Bus | Pins | Speed | Source |
|---|---|---|---|---|
| LCD (ST7789P / ST7365P class) | `spi1` | SCL 10, SDI 11, SDO 12, CS 13, D/CX 14, RST 15 | **75 MHz** | [lcd.h](../../pico-logo/devices/picocalc/lcd.h) |
| Southbridge (keyboard, battery, backlight, power) | `i2c1` | SDA 6, SCL 7 | **10 kHz**, addr `0x1F` | [southbridge.h](../../pico-logo/devices/picocalc/southbridge.h) |
| SD card | `spi0` | MISO 16, CS 17, SCK 18, MOSI 19, detect 22 | 400 kHz init → **25 MHz** | [sdcard.h](../../pico-logo/devices/picocalc/sdcard.h) |
| Audio | PWM slice 5 | GPIO 26 (left), 27 (right) | 73.2 kHz carrier | [sound.c](../../pico-logo/devices/picocalc/sound.c) |

Three RP2350 boards fit the carrier, and they are not equivalent:

| Board | Flash | PSRAM | Radio | Consequence |
|---|---|---|---|---|
| Pico 2 | 4 MB | — | — | SRAM-only; tightest budget |
| Pico 2 W | 4 MB | — | WiFi | WiFi stack costs SRAM |
| Pimoroni Pico Plus 2 W | 16 MB | 8 MB | WiFi | PSRAM aux region; the only board with room |

**Different flash parts mean different instruction-fetch behaviour** (§9.2).
We burned real time comparing a Pico 2 measurement against a Plus 2 W one and
drawing a conclusion from the difference. Don't. One board per comparison.

### 1.1 SRAM is the scarce resource

520 KB total, and Pico Logo's firmware sat at **87–96 % of RAM** depending on
preset. Oversized static buffers do not fail at link time in an obvious way —
they fail at runtime as an out-of-memory panic during init. For a game engine
the arithmetic that matters:

| Buffer | Size | Note |
|---|---:|---|
| 320×320 @ 8 bpp indexed canvas | **102,400 B** | what we shipped |
| 320×320 @ 16 bpp RGB565 | 204,800 B | **impossible** — never consider it |
| Double-buffered 8 bpp | 204,800 B | also impossible |
| Two DMA line buffers (320 px × 2 B × 2) | 1,280 B | the palette-expansion pipeline |
| Compose scratch row | 320 B | |
| Audio DMA ring | 2,048 B | must be power-of-2 **aligned** |
| Audio voice queues (8 × 64 × 6 B) | 3,072 B | |
| Costume/sprite pixel pool | 8,192 B | tunable |
| Tile bank + map (SRAM tier) | 4 KB + 4 KB | 64 KB + 256 KB on PSRAM |

An 8-bit indexed canvas plus a 256-entry RGB565 palette expanded at blit time
is the only framebuffer model that fits. That single decision shapes everything
in §2–§4.

### 1.2 PSRAM (Plus 2 W only)

PSRAM appears as a memory-mapped window on QMI chip-select 1, brought up by the
SDK before `main()`. Two things we learned:

- **Verify it before trusting it.** The SDK's init checks the chip ID but not
  that the window works at full QMI speed. We write four patterns at spread
  offsets, `xip_cache_clean_all()` + `xip_cache_invalidate_all()` to force a
  real round trip, and read back — a marginal QMI config then degrades to
  SRAM-only instead of handing the allocator a region that hardfaults on use.
  See `psram_verify()` in [main.c](../../pico-logo/devices/picocalc/main.c#L92).
- **PSRAM reads go through the same 16 KB XIP cache as flash code fetch.**
  Cached reads are SRAM-speed; a cache-hostile access pattern is not, and it
  contends with instruction fetch. §9.4 has the measurement that made this
  concrete.

---

## 2. The display

### 2.1 What the panel is

A 320×320 visible panel on a controller with **480 rows of frame memory**.
RGB565, 16 bpp on the wire. The extra 160 rows are not wasted: they back the
controller's hardware vertical scroll.

Init sequence (gamma, power, VCOM, `MADCTL 0x48` for BGR order,
`COLMOD 0x55` for 16 bpp, `INVON`, frame rate, `VSCRDEF`, `SLPOUT`) is in
[lcd_init()](../../pico-logo/devices/picocalc/lcd.c#L726). It is transcribed from working
code and known-good; copy it rather than rederiving from the datasheet.

**Timing gotcha that costs a week if you miss it.** The controller needs a
minimum **40 ns chip-select high pulse** before a RAM write. In practice that
means `spi_set_format()` and the `D/CX` GPIO write must happen *before*
lowering `CS`, not after — the two instructions are what create the required
gap. Every 16-bit write path in [lcd.c](../../pico-logo/devices/picocalc/lcd.c#L146) carries
a "DO NOT MOVE" comment for exactly this reason.

**75 MHz is a qualified PicoCalc operating point.** It has been exercised on
hardware and is reliable when the 40 ns CS-high requirement above is preserved.
The 75 MHz present measurements in this document are therefore the initial
engine budget, not an unverified projection. They do not authorize a driver to
omit the CS-high interval or to assume that a higher SPI clock is safe.

### 2.2 The blit pipeline

An 8-bit indexed canvas has to be expanded to RGB565 on the way out. Naively —
`palette[src[i]]` fed to a blocking SPI write per pixel — the CPU paces the
wire and a full screen costs ~22 ms of *pure stall*. The fix:

```
lcd_blit_begin(x, y, w, h)   → set CASET/RASET/RAMWR window, hold CS, 16-bit format
  lcd_blit_row(row)  × h     → expand row N into line buffer A
                                while DMA streams line buffer B to the SPI TX FIFO
lcd_blit_end()               → wait out the last DMA, drain the wire, release CS
```

Details that matter:

- **One DMA channel, configured once**, DREQ-paced by `spi_get_dreq(spi1, tx)`,
  `DMA_SIZE_16`, read-increment on, write-increment off. Per row you only set
  the read address and the transfer count. See
  [lcd_init()](../../pico-logo/devices/picocalc/lcd.c#L756).
- **Two line buffers, ping-ponged.** Expansion of row *n+1* overlaps the
  transfer of row *n*. The CPU's visible cost per row drops from ~68 µs
  (wire-paced) to ~5–8 µs (memory-paced).
- **Poll for completion; do not take an interrupt.** The CPU has useful work
  (expanding the next row) while it waits, and — critically — leaving the LCD
  DMA interrupt-free means the audio engine owns `DMA_IRQ_0` uncontended (§5).
- **Drain properly at the end.** The DMA only fills the TX FIFO. You must wait
  for `SSPSR_BSY` to clear, discard everything the full-duplex SPI clocked into
  the RX FIFO, and clear the overrun flag, or the next transaction inherits
  garbage. [lcd_blit_end()](../../pico-logo/devices/picocalc/lcd.c#L255).
- **Interrupts stay enabled for the whole blit.** The controller tolerates SPI
  clock pauses mid-window, so an audio refill IRQ can preempt a blit freely.
  This is load-bearing — see §5.4.
- **Restore 8-bit SPI format** when the window closes; command writes are
  8-bit.

### 2.3 Measured throughput — the single most important number

Theoretical: 75 MHz / 16 bpp = **4.69 Mpx/s**. Measured (Pico 2, 2026-08-01,
by timing forced full-region presents twenty times and subtracting the
repaint cost):

| Region | Pixels | Wall time |
|---|---:|---:|
| Full screen 320×320 | 102,400 | **25.6 ms** |
| 256×320 (a 16-column viewport) | 81,920 | **21.1 ms** |
| 64×320 strip | 20,480 | 5.45 ms |
| Per 16-px tile column (16×320) | 5,120 | 1.259 ms |
| Fixed cost per present | — | 0.41 ms |

**The effective rate is 4.0 Mpx/s, 17–21 % below the wire math**, and the
band figures are internally consistent (slope 1.259 ms/column, intercept
0.41 ms) so it is not noise. The difference is the per-row work the arithmetic
treated as free: the canvas `memcpy` into the compose buffer, palette
expansion, and the fact that a full-height present with one dirty span per tile
row costs **twenty separate blit windows**, not one.

The near-zero intercept is the useful part: **cost is per pixel, not per
present**, so narrowing a viewport buys back exactly its area share, and
splitting a present into several windows is nearly free.

For a C engine, this is your frame ceiling:

| Redraw strategy | Cost | Ceiling |
|---|---:|---:|
| Full screen every frame | 25.6 ms | ~39 fps, nothing left over |
| 256×320 viewport every frame | 21.1 ms | ~47 fps |
| Dirty sprite tiles only | 1.6–2.7 ms (measured) | not the constraint |

### 2.4 Hardware vertical scroll

`VSCRDEF` defines top-fixed / scroll / bottom-fixed bands; `VSCSAD` sets which
frame-memory line appears at the top of the scroll band. Pico Logo uses this
for text scrolling and split-screen: `lcd_scroll_up()` bumps the offset by one
glyph height and clears the newly exposed line — a full-screen text scroll for
the cost of one line of pixels.

The price is that **every blit must remap its y through the scroll offset**
(see the top of [lcd_blit_begin()](../../pico-logo/devices/picocalc/lcd.c#L202)), including
the wrap around the 480-row frame memory. Get it wrong and drawing lands
somewhere plausible-but-wrong, which is a miserable class of bug.

**Unexploited lever for a game engine:** vertical-only smooth scrolling is
nearly free this way — shift `VSCSAD` and blit only the newly exposed band.
It costs you horizontal motion and fights split-screen text scrolling, which
is why Pico Logo never took it, but a vertical scroller (shmup, endless
runner) would get its background for ~1 ms a frame instead of 25.

---

## 3. Dirty-region tracking

Presenting the whole screen because one 16×16 sprite moved is the difference
between 25.6 ms and 0.4 ms. What we settled on
([dirty_tiles.h](../../pico-logo/devices/picocalc/dirty_tiles.h)):

- The screen is a **20×20 grid of 16×16-pixel tiles**.
- Per tile *row*, keep one **inclusive span** `[min..max]` of tile columns —
  40 bytes of state for the whole screen.
- Marking is O(1): clamp the rect, widen the spans it covers.
- Flushing walks ≤20 rows and issues one window + blit per dirty span.

A span over-approximates disjoint dirt within a row. That is deliberate: at
this resolution the waste is at most one tile row, far cheaper than the
bookkeeping exact rectangles would need.

Rejected, with reasons:

| Alternative | Why not |
|---|---|
| Single full-width y-range (what we had first) | A 16×16 sprite move blits 5,120 px where 256 would do — 20× waste. Two sprites at opposite ends dirty everything between them. |
| Per-scanline x-extents | 320 × 4 B = 1.3 KB of state, and one window setup per row. No better than tiles in practice. |
| Exact rectangle lists | Allocation and merge complexity for marginal gain at this granularity. |

Practices worth copying:

- **Over-mark rather than under-mark.** Sprites in wrap mode get marked both
  clamped *and* wrapped (`dirty_tiles_mark_rect_wrap` splits a straddling box
  into up to four on-screen rects). A few extra tiles cost microseconds;
  a missed tile leaves a stale sprite on screen forever.
- **Snapshot the dirty state and clear it *before* sending**, so writes that
  happen during the blit are tracked for the next present
  ([screen_gfx_blit_dirty](../../pico-logo/devices/picocalc/screen.c#L970)).
- **Marking overhead is negligible** and we measured it: one mark per row over
  288 rows is ~20 µs against a 7.6 ms bake. We suspected it as a bottleneck,
  measured it, and left it alone. Don't optimise this.
- The tracker is deliberately **free of SDK dependencies** so it unit-tests on
  the host. That was worth it.

---

## 4. Sprites: a per-scanline software compositor

### 4.1 The model

The decision that simplified everything: **sprites are not in the canvas.**

- `gfx_buffer` holds only the background/canvas layer.
- Sprites are state: position, size, palette-index mask pointer, colour,
  visibility, z-order.
- Each outgoing row is built at blit time —
  [compose_row()](../../pico-logo/devices/picocalc/screen.c#L406) `memcpy`s the canvas
  segment, then overlays each visible sprite's row for that y, then hands the
  row to the palette-expanding blit pipeline.

This is what a TMS9918A or Atari player-missile did in silicon, done in
software — which removes every hardware restriction (no 4-sprites-per-scanline
flicker, no single-colour limit, no fixed sprite size) and *adds* correctness:

- No save-under buffers, no restore-in-reverse-order hazard, no per-move
  erase/redraw churn.
- Moving a sprite writes nothing to the canvas — it only dirties the old and
  new tiles.
- Canvas readback (hit-testing a pixel, flood fill, screenshot to BMP) sees a
  clean canvas that sprite pixels can never contaminate.

Two mask kinds, worth having both: **mono** (nonzero byte → paint the sprite's
colour, cheap and tiny) and **indexed colour** (byte is a palette slot, 255
reserved as transparent).

### 4.2 Cache the rendered raster

Rotation, mirroring, and integer scaling are applied **once, into a cached
raster**, invalidated only when the shape/angle/scale/style changes
([turtle_update_raster](../../pico-logo/devices/picocalc/picocalc_console.c#L482)). The
compositor then always blends a pre-baked w×h image, so per-frame cost is
independent of those features. A 32×32 nearest-neighbour rotate is ~1 K pixel
transforms — microseconds — and it happens on change, not per frame.

### 4.3 Pixel storage pool

Variable-size sprite pixels (8×8 to 32×32) come from a **fixed pool with
compaction on free** ([costumes.c](../../pico-logo/devices/picocalc/costumes.c)), 8 KB in
SRAM. Compaction keeps fragmentation from wasting the pool; a full pool is an
error, not a silent truncation. Tiles, by contrast, are uniform-size so their
bank needs no compaction at all (§6) — pick the simpler structure whenever
sizes are fixed.

### 4.4 Cost

Per dirty row: one bbox test per sprite plus an overlay loop over the covered
sprites' widths. Measured on hardware, a frame presenting only its dirty sprite
tiles costs **1.64 ms** (independent profiler) to **2.4 ms** (game harness) —
about 2–6 % of a 40 ms frame. **For a game that moves sprites over a static
background, the display is not the problem.** That is measured, not assumed.

---

## 5. Audio: a software PSG through PWM

There is no I2S DAC on the PicoCalc. PWM is the hardware.

### 5.1 The output path

- **One PWM slice (5) drives both ears** — GPIO 26 is channel A, GPIO 27 is
  channel B, and the slice's two compare values live in *one 32-bit register*.
  So a stereo frame is a single 32-bit word: `left | (right << 16)`. One DMA
  stream, not two.
- **Wrap 2048 → 73.2 kHz carrier, 11-bit resolution.** The carrier is
  ultrasonic, so no filtering is needed beyond what the speaker does.
- **Two DMA channels chained to each other**, each paced by the slice's wrap
  DREQ, ping-ponging through a two-half ring (256 slots × 4 B per half). When a
  half drains, that channel's IRQ refills it. Chaining is what makes playback
  gapless.
- Each mixed frame is written **OVERSAMPLE = 2** times, giving a ~36.6 kHz mix
  rate — 128 mixed frames per half, i.e. **one refill deadline every ~3.5 ms**.

### 5.2 The ring must wrap in hardware

The whole ring (both halves, 2,048 B) is **power-of-2 sized and aligned**, with
`channel_config_set_ring()` on the read address. This is the safety net for IRQ
starvation: if the refill IRQ is delayed past a half draining, a non-wrapping
DMA marches into adjacent RAM and plays it as audio — an audible burst of
static. With the hardware wrap it cleanly replays the ring instead: silence at
idle, a brief stutter under a note.

You need this because **flash program/erase masks interrupts with XIP offline
for tens of milliseconds** (§7), which no amount of careful IRQ design avoids.

Also: when re-arming a chained channel in its IRQ, reset **both** the read
address and the transfer count. A chain trigger reloads neither. Resetting only
the address leaves count at zero, so the next chain completes instantly and
storms the IRQ.

### 5.3 The mixer

Eight voices — three tone + one noise per ear — mixed in the refill IRQ on
core 0, integer math throughout
([sound.c](../../pico-logo/devices/picocalc/sound.c#L273)):

- Phase-accumulator oscillators (square, variable-duty pulse, triangle,
  sawtooth) — `phase += (freq << 32) / mix_rate`, waveform from the top bits.
- 16-bit LFSR for noise, clocked on phase wrap, in white and periodic modes,
  so noise is *pitched*.
- ADSR per voice, advanced **once per refill block** (3.5 ms granularity —
  inaudible for ms-scale envelopes, and it keeps the per-sample loop tight).
- A 16-entry **2 dB-per-step volume ladder** (the TI/Atari curve), scaled to
  0..256, so level 0 is off and 15 is full scale.
- Four voices summed per ear, `>> 2` for headroom, saturate to ±1024, bias to
  mid — four simultaneous full-volume voices cannot wrap.
- **Everything in the IRQ path is `__not_in_flash_func`**, so it survives flash
  writes and does not add XIP misses.

Cost: ~35 cycles/voice/frame × 8 × 36.6 kHz ≈ **7 % of core 0**, in interrupt
context, regardless of what the foreground is doing.

The op layer is a lock-free SPSC queue per voice — the foreground appends at
the tail, the IRQ pops at the head, with one slot reserved so full and empty
are distinguishable. Writes from thread context wrap in
`save_and_disable_interrupts()` only for the few instructions that touch shared
voice state.

### 5.4 Three bring-up findings — the expensive lessons

All three were "audible clicking" with different causes. The common thread:
**the refill has a hard 3.5 ms deadline and anything that delays it is
audible.**

1. **The DMA ring-wrap** (§5.2). Symptom: clicks at idle, static on a screen
   mode switch. Cause: resetting the DMA read address inside the IRQ, so a
   masked IRQ let the DMA run off the end of the buffer.
2. **The LCD driver was masking interrupts.** Symptom: stutter under any large
   screen update — a full redraw held IRQs off for ~22 ms. Cause: the driver
   wrapped every screen op in `save_and_disable_interrupts()`, purely because a
   cursor-blink *timer* drew to the LCD from IRQ context. Fix: the timer only
   sets a flag; the drawing happens in the foreground wait loop. All interrupt
   masking was then deleted from the LCD driver. **The invariant that came out
   of this: the LCD is only ever touched from thread context, never from an
   interrupt handler.** Write it at the top of your driver, as we did.
3. **IRQ priority.** Symptom: with (2) fixed, a sustained note still clicked
   ~10×/s. Cause: the keyboard poll timer blocks 4–5 ms on the 10 kHz I²C bus
   *inside a timer IRQ* every 100 ms, and same-priority NVIC interrupts cannot
   preempt each other. Fix: `irq_set_priority(DMA_IRQ_0, 0x40)` — above the
   default `0x80` — so the audio refill preempts everything else. The I²C
   peripheral runs autonomously and is unharmed.

**The rule to carry forward:** any handler that can run longer than 3.5 ms at
default priority will click. Either keep handlers short or keep them below the
audio IRQ's priority. Flash program/erase is the one accepted violator.

---

## 6. Tile maps

Built and shipped (bake path); the live scrolling path was designed, gated, and
deferred because an *interpreted* frame could not afford it. For a C engine the
gate reads differently — see §9.6.

### 6.1 Storage

- **Uniform tile size per bank, 8 or 16 px**, chosen when the bank is created.
  Uniform means slot *n* lives at `pool[n × size²]` — no offset table, no
  compaction, and sampling is a shift not a branch.
- **Map is one byte per cell.** Value 0 = background fill; 1..255 = bank slot.
  A map is a *live data structure*, not a picture: O(1) `tile(col,row)` is the
  game's collision and world query, which deletes any parallel structure.
- **Lazily allocated and tiered**: try the PSRAM aux region first, else one
  process-lifetime SRAM allocation. **Zero static bytes** — a program that never
  uses tiles pays nothing. Capacities: 4 KB bank / 4 KB map on SRAM, 64 KB /
  256 KB on PSRAM.

### 6.2 Sampling and baking

The sampler is a pure function —
`tilemap_fill_row(dst, y, x0, x1, bg)` — that fills a span of one row by
sampling the bank at the scrolled world position, wrapping modulo the world
size in both axes. That one function serves two consumers:

- **Bake**: write it into the canvas once (a level's background).
- **Live view**: call it from `compose_row()` instead of the canvas `memcpy`
  for columns inside the viewport, so a scrolled background is **generated,
  never stored**. Scrolling then costs two integer writes plus a dirty mark —
  no memory move at all, and no second framebuffer (which would not fit, §1.1).

Wrapping unconditionally, with no clamp flag, was the right call: a bounded
world is achieved by the game clamping its own camera, which it must compute
anyway.

### 6.3 What the bake measured, and the lead it left

Baking a 224×288 board (64,512 px) takes **7.45–7.6 ms** on a Plus 2 W,
repeatable across four runs and two firmware layouts. Against the Logo code it
replaced (5,916 ms) that is a 794× win and the feature justified itself
instantly.

But **7.6 ms for a memcpy-shaped loop on a 150 MHz core is slow** — ~118 ns per
pixel, only ~2× faster than the SPI wire. We chased it:

| Suspect | Verdict |
|---|---|
| Per-row dirty marking | **Red herring** — ~20 µs total, by inspection and arithmetic. |
| Code not RAM-resident | **Disproved by experiment.** Moved the sampler and its helpers into SRAM (+2 KB): 7.45 → 7.6 ms, i.e. no change or slightly worse. Reverted — a measured no-op has no business costing 2 KB. |
| **Data path** | **The leading explanation.** The tile pools land in the PSRAM aux region while the canvas is in SRAM, so the bake is a PSRAM→SRAM copy in 8-byte runs. That fits ~118 ns/px far better than instruction fetch does. |

Two things for a new engine: **put the tile bank in SRAM if it fits** (a
typical bank is 3–4 KB), and if you build a live scrolling view, price the
sampler *first* — it runs once per row per frame there instead of once per
level, and the old ~1–1.5 ms/frame estimate assumed SRAM.

---

## 7. Storage, flash, and the interrupt hole

- **SD card** on `spi0` at 25 MHz with a FAT32 driver; card-detect on GPIO 22.
- **Internal flash** carries a LittleFS filesystem for local files.
- **Flash program/erase is the one place interrupts must be masked.** The
  recipe: disable interrupts → run the bootrom erase/program (which executes
  from RAM with XIP offline) → restore. While XIP is down, *any* flash-resident
  ISR would hardfault, and the source buffer must not be a pointer into
  flash/XIP rodata.

Consequences for a game engine:

- Audio stutters for the duration of a save. The ring-wrap (§5.2) turns this
  from static into a repeat of the last 3.5 ms. Accepted, and worth telling
  the player: don't save mid-music.
- **Never do flash I/O inside a frame loop.** Level loads, save games, and
  high-score writes belong at scene boundaries.
- If you use core 1, every flash write needs `multicore_lockout` around it,
  because the other core cannot execute from flash while flash is being
  programmed. This is easy to get right once and easy to forget when a new
  writer appears later.

---

## 8. Input: the southbridge

The keyboard is not wired to the RP2350. An on-board MCU owns it and exposes a
register interface over **I²C at 10 kHz**, address `0x1F`:

| Register | Function |
|---|---|
| `0x04` | key state |
| `0x05` | LCD backlight (read/write) |
| `0x08` | reset |
| `0x09` | key FIFO |
| `0x0A` | keyboard backlight |
| `0x0B` | battery |
| `0x0E` | power off |

Write the register number, then read two bytes; OR `0x80` into the register for
a write. All transfers use `*_timeout_us` variants — a wedged bus must not hang
the machine.

What we learned:

- **Polling only.** There is no interrupt line. Pico Logo polls the FIFO from a
  100 ms repeating timer and buffers into a 32-entry ring. That is fine for a
  REPL and **too slow for a game** — you will want to poll per frame (25–60 Hz)
  or faster from your frame loop.
- **Each poll costs 4–5 ms on the bus at 10 kHz**, and Pico Logo does it inside
  a timer IRQ, which is what forced the audio priority fix (§5.4). In a game
  engine, poll from the frame loop in thread context instead — it is the same
  cost but it lands where you can account for it.
- **Guard the bus with a busy flag.** `sb_available()` is an atomic bool the
  timer checks before starting a transfer, so a foreground battery read and a
  background key poll never interleave on the bus.
- **The FIFO reports press/release events with a state byte**, so modifier
  state (Ctrl/Shift/Alt) is tracked host-side. For a game this is what you want
  anyway: build your own held-key bitmap from press/release events rather than
  asking the register for level state.
- Battery, both backlights, a timed power-off, and a timed reset are all
  available through the same interface — cheap wins for a polished game
  (dim the backlight on pause, show battery in the HUD).

---

## 9. Performance: everything we learned by measuring

This section is the reason the document exists. Most of it is about *how to
measure on this machine*, which turned out to matter more than any individual
optimisation.

### 9.1 Measure on hardware, in the mode you ship

Three separate times, a number that looked authoritative was an artefact:

- **Text-mode presents are free.** The blit function returns immediately when
  the screen is in text mode. A profiling harness that ran from the prompt
  therefore reported a present of 0.25 ms. Every "frame time" in a months-long
  series was actually a *body* time with no present in it. The corrected
  harness switches to graphics mode itself.
- **The same bug hid a 25 ms cost.** Clearing the screen in text mode is a bare
  `memset`; in graphics mode it is a full-panel fill — ~25 ms. A level-setup
  routine "regressed" from 20 ms to 66 ms purely because it was finally being
  measured in the mode it runs in.
- **Boundary-mode interactions.** A present-cost harness had to run in
  clipping mode, because in wrapping mode a stroke's round cap spilled past the
  left edge to the right one and dirtied the entire tile row, making every
  measurement read as a full screen.

Corollaries we now follow: **always carry a control** — a quantity in each run
that should *not* change (we used a fixed present cost across five runs; when
it finally moved 2 %, that told us we were at the noise floor). And expect
**~2 % run-to-run spread** on this machine; anything smaller is not a result.

### 9.2 Instruction fetch through the XIP cache is a first-order cost

The biggest single finding of the project. Code executes from flash through a
**16 KB XIP cache**, shared by both cores and by PSRAM data. Moving hot
functions into SRAM with `__not_in_flash_func` produced:

| Tier | What moved | Frame | Speedup | SRAM cost |
|---|---|---:|---:|---:|
| — | baseline | 81.0 ms | | |
| 1 | expression evaluator (4 functions) | 65.5 ms | **1.24×** | 6.0 KB |
| 2 | call path + variables (8 functions) | 53.2 ms | **1.23×** | 3.6 KB |
| 3 | loop + run-list path | 48.1 ms | 1.105× | 1.7 KB (free — fit in existing alignment padding) |
| 4 | frames + bindings | 47.0 ms | 1.024× | 2.0 KB |

**1.72× cumulative for 13.6 KB of SRAM**, on a Plus 2 W, verified each time by
checking that the symbols moved from `0x1…` to `0x2…` in the image.

Three lessons generalise beyond an interpreter:

1. **Returns halve every tier.** Stop when the curve says to, not when the
   budget runs out. Tier 5 would have cost 10 KB for a few percent.
2. **Every tier moved the boundary, and whatever was left adjacent to it got
   worse.** Tier 1 improved the evaluator and regressed procedure calls by 29 %;
   tier 2 fixed calls and regressed the loop by 67 %; tier 3 fixed the loop and
   regressed calls again. Piecemeal residency has an *unstable cost surface*,
   because the flash residue is re-laid-out against the cache each time. It is
   self-limiting (the regressing line is always smaller than the set that
   improved) but it means **you must re-measure the whole profile after every
   move**, not just the thing you moved.
3. **It is not universal.** The same technique applied to the tile-bake loop
   produced *nothing* (§6.3). Instruction fetch dominates code that is large
   and branchy; a tight copy loop is data-bound and does not care. Measure
   before spending SRAM.

### 9.3 Sizes to keep in mind

The functions worth moving were the big branchy ones (a 3.1 KB expression
evaluator) *and* the tiny constantly-called ones (120–164 B accessors, "the
best value per byte"). Whole-module residency is not the unit; individual hot
functions are.

### 9.4 Where the data lives matters as much as where the code lives

Covered in §6.3: a PSRAM→SRAM bulk copy in small runs ran at ~8.7 Mpx/s when a
150 MHz core should do far better, and no code-placement change touched it.
For an engine: **keep per-frame working sets in SRAM**; use PSRAM for capacity
(large maps, sprite banks loaded per level), not for the hot path — or at
least, measure before assuming the XIP cache hides it.

### 9.5 Small `memcpy` calls are call overhead

The tile sampler copies a tile run at a time — at 8-px tiles that is 28 runs of
8 bytes per row, 8,064 calls for one board bake. A copy small enough that the
call is the cost. If your inner loop is bounded by tile size, hand-unroll or
specialise for the two sizes you support.

### 9.6 The frame budget, and why a C engine is a different machine

Pico Logo's 25 fps design cadence gave a 40 ms frame. Serially, a frame costs
`body + present`:

| | body | present | total |
|---|---:|---:|---:|
| Shipped game, dirty sprites only | 40.15 ms | 2.40 | 42.55 |
| Same body, scrolled road view | 40.15 | 21.1 | 61.25 |
| Same body, scrolled full screen | 40.15 | 25.6 | 65.75 |

That is why scrolling was shelved: the *interpreter* ate the whole frame, and a
scrolled present needs 21–26 ms of what is left. A frame body of 40 ms was
~790 interpreted operations at ~48 µs each.

**A C engine deletes that entire column.** Your body is native code; the
constraints that remain are:

- the **wire** — 25.6 ms full screen, 1.26 ms per tile column, linear in area;
- the **3.5 ms audio refill deadline**;
- **SRAM**, which forbids double buffering.

So the design space opens right up: a serial full-screen scrolling path at
30 fps is plausible only if all foreground work fits in ~7 ms. The new engine
does not rely on that narrow serial margin: its full-screen scrolling contract
requires core 1 to own rendering and presentation, subject to the explicit gate
in `engine-design.md`. Dirty-rect sprite games remain far less constrained.

### 9.7 The unexploited lever: core 1

**Core 1 has never been started in this project.** And the blit is ~85 % core 0
blocked in `dma_channel_wait_for_finish_blocking` — about 22 ms per full-screen
frame doing nothing. Moving the present to core 1 makes a frame
`max(body, present)` instead of `body + present`.

What we worked out before shelving it (in
[concurrent-present-design.md](../../pico-logo/docs/concurrent-present-design.md)):

- **The usual objection does not apply.** Double-buffering the canvas is 100 KB
  and impossible — but a *generated* background (§6.2) is never stored, so
  there is no buffer to double. Core 1 needs read-only access to the map and
  bank for the duration of a frame, plus a snapshot of the sprite table (a few
  hundred bytes). That is a far smaller synchronisation problem.
- **The audio path does not move.** The LCD DMA is polled, not
  interrupt-driven, and the audio synth owns `DMA_IRQ_0` on core 0. Putting the
  blit on core 1 contends for no interrupt.
- **The unmeasured risk is contention**: both cores fetch instructions through
  one 16 KB XIP cache, and §9.2 showed this project is acutely sensitive to
  that. The proposed probe is deliberately dumb — core 1 blits a static buffer
  in a loop, core 0 runs the normal workload, compare core 0's timings with and
  without. Correctness and tearing are irrelevant to the probe.
- Flash writes then need `multicore_lockout` (§7).

For a C engine this is a much easier call than it was here: there is no
interpreter with a shared mutable heap, so "core 1 owns the display" is a clean
ownership split rather than a locking problem. **It is the largest single lever
available on this hardware.** The engine design makes this a required
architecture for full-screen scrolling and defines an early feasibility gate;
the Pico Logo evidence here seeds that gate but does not replace running it in
the engine's shipping configuration.

### 9.8 Levers, in order

If you need to buy present time:

1. **Narrow the viewport.** Cost is exactly linear in area; a HUD column that
   stays static buys back its share.
2. **Half-rate scrolling** — scroll at 12.5 Hz while sprites present at 25 Hz;
   ordinary frames go back to tile-sized dirty rects. Halves the average
   present.
3. **Hardware vertical scroll** (§2.4) — nearly free vertical motion, at the
   cost of horizontal motion.
4. **Use the qualified 75 MHz SPI clock.** It is tested with the required 40 ns
   CS-high pulse. Higher clocks remain panel-limited and untested.
5. **Core 1** (§9.7) — required by the engine for full-screen scrolling and
   subject to its explicit feasibility gate.

---

## 10. Checklist for the new engine

Things that will bite, in rough order of how much time they cost us:

- [ ] `spi_set_format()` and `D/CX` before `CS` low — the 40 ns CS-high rule.
- [ ] Drain the SPI RX FIFO and clear the overrun flag at the end of every DMA
      blit; restore 8-bit format.
- [ ] Never touch the LCD from an interrupt handler. Timers set flags.
- [ ] Never mask interrupts around a blit — the audio has a 3.5 ms deadline.
- [ ] Audio DMA ring: power-of-2 **and aligned**, with the hardware read-wrap.
- [ ] Re-arm both read address **and** transfer count when re-arming a chained
      DMA channel from its IRQ.
- [ ] Audio IRQ priority above default (`0x40`); nothing else may run long at
      default priority.
- [ ] Every function on the audio IRQ path `__not_in_flash_func`.
- [ ] Verify PSRAM with a real round-trip before handing it to an allocator.
- [ ] Remap y through the vertical-scroll offset in *every* blit path, or don't
      use hardware scroll at all.
- [ ] Over-mark dirty regions; snapshot-and-clear before sending.
- [ ] No flash writes inside a frame loop; `multicore_lockout` if core 1 runs.
- [ ] Poll the keyboard from the frame loop, not a timer IRQ; guard the I²C bus
      with a busy flag; build held-key state from press/release events.
- [ ] Budget SRAM up front: one 100 KB canvas is most of what you have.
- [ ] Profile in the screen mode you ship, with a control quantity, expecting
      2 % spread, on one board.

---

## Source pointers

Everything above is implemented in this repository:

| Subject | File |
|---|---|
| LCD driver, DMA blit pipeline, hardware scroll | [devices/picocalc/lcd.c](../../pico-logo/devices/picocalc/lcd.c) |
| Framebuffer, compositor, dirty presentation, refresh policy | [devices/picocalc/screen.c](../../pico-logo/devices/picocalc/screen.c) |
| Dirty-tile tracker (portable, unit-tested) | [devices/picocalc/dirty_tiles.c](../../pico-logo/devices/picocalc/dirty_tiles.c) |
| Sprite pixel pool with compaction | [devices/picocalc/costumes.c](../../pico-logo/devices/picocalc/costumes.c) |
| Cached sprite raster, rotation/scale | [devices/picocalc/picocalc_console.c](../../pico-logo/devices/picocalc/picocalc_console.c) |
| PSG synth: PWM + chained DMA + mixer | [devices/picocalc/sound.c](../../pico-logo/devices/picocalc/sound.c) |
| Southbridge I²C register interface | [devices/picocalc/southbridge.c](../../pico-logo/devices/picocalc/southbridge.c) |
| Keyboard polling, buffering, modifiers | [devices/picocalc/keyboard.c](../../pico-logo/devices/picocalc/keyboard.c) |
| Flash-safe write recipe | [devices/picocalc/picocalc_flash.c](../../pico-logo/devices/picocalc/picocalc_flash.c) |
| PSRAM verification and boot order | [devices/picocalc/main.c](../../pico-logo/devices/picocalc/main.c) |
| 256-colour palette (indices → RGB565) | [devices/palette.h](../../pico-logo/devices/palette.h) |
| Tile bank, map, row sampler | `core/tilemap.c` |

And the design records with the full measurement history:

- [multi-sprite-design.md](multi-sprite-design.md) — the display pipeline
  rework (§2), the compositor, and a survey of how period machines did sprites.
- [tilemap-scrolling-design.md](tilemap-scrolling-design.md) — tile storage,
  the sampler, and §3.3/§13 for every present and frame measurement taken.
- [sound-design.md](sound-design.md) — the PSG design and §12's three hardware
  bring-up findings.
- [interpreter-throughput-design.md](interpreter-throughput-design.md) — §11 is
  the XIP-cache/RAM-residency investigation in full.
- [concurrent-present-design.md](concurrent-present-design.md) — the core 1
  proposal, its arithmetic, and the probe that would settle it.
