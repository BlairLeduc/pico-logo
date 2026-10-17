# PicoCalc hardware notes

A hardware reference for anyone writing a C application against the Pico SDK on
a **ClockworkPi PicoCalc**. It describes the machine — what is wired to what,
what each peripheral will and will not do, and what it costs — so that you can
design your own graphics, audio and input systems rather than inherit ours.

**Provenance.** Everything about the PicoCalc itself was learned by building and
measuring [Pico Logo](../README.md) on real hardware. Where a number is a
measurement it says which board it came from and when; where it comes from a
datasheet or the SDK it says so. The RP2040 material (Pico, Pico W) is
**datasheet and SDK only** — this project has never built for those parts, and
those lines are flagged where they appear.

**Pico Logo appears here only as a worked example.** Links into
`devices/picocalc/*` are there so you can read one implementation of a recipe,
not because you should copy its architecture. Where a decision was ours rather
than the hardware's, the text says so and moves on.

**Contents**

1. [The machine](#1-the-machine) · 2. [The processor board](#2-the-processor-board) ·
3. [Clocks and overclocking](#3-clocks-voltage-and-overclocking) ·
4. [Display](#4-the-display) · 5. [Audio](#5-audio) ·
6. [Keyboard and southbridge](#6-keyboard-battery-backlight-power) ·
7. [Storage](#7-storage) · 8. [On-chip odds and ends](#8-on-chip-odds-and-ends) ·
9. [Making it fast](#9-making-it-fast) · 10. [Bring-up checklist](#10-bring-up-order-and-trap-checklist) ·
11. [Where to look next](#11-where-to-look-next)

---

## 1. The machine

The PicoCalc is a **carrier board**, not a computer. It supplies a 320×320 SPI
LCD, a 4×N keyboard behind its own microcontroller, stereo PWM audio, an SD
slot, a battery and charger, and two backlights. The computer is a
Raspberry Pi Pico–form-factor module you plug into it, and everything on the
carrier reaches that module over **three buses and a pair of PWM pins**.

| Peripheral | Bus | Speed | Where |
|---|---|---|---|
| LCD (ST7789P / ST7365P class) | `spi1`, 4-wire | 75 MHz | [lcd.h](../devices/picocalc/lcd.h) |
| Southbridge — keyboard, battery, backlights, power | `i2c1`, addr `0x1F` | 10 kHz | [southbridge.h](../devices/picocalc/southbridge.h) |
| SD card | `spi0` | 400 kHz init → 25 MHz | [sdcard.h](../devices/picocalc/sdcard.h) |
| Audio | PWM slice 5, both channels | carrier is yours to choose | [sound.c](../devices/picocalc/sound.c) |

### 1.1 GPIO map

Everything the carrier drives, plus the pins the processor board itself claims.
GP numbers are the Pico header numbering and are **the same on all five boards**
— the Pimoroni Plus 2 W maps its RP2350B pins so that GP0–GP29 land where a Pico
puts them, which is why no board conditional appears anywhere in this tree's pin
definitions.

| GP | Used by | Function |
|---:|---|---|
| 0–5 | — | free (GP0/GP1 are the SDK's default UART0) |
| 6 | carrier | `i2c1` SDA → southbridge |
| 7 | carrier | `i2c1` SCL → southbridge |
| 8–9 | — | free |
| 10 | carrier | `spi1` SCK → LCD SCL |
| 11 | carrier | `spi1` TX → LCD SDI |
| 12 | carrier | `spi1` RX ← LCD SDO |
| 13 | carrier | LCD CS — **plain GPIO, driven by hand** |
| 14 | carrier | LCD D/CX — plain GPIO |
| 15 | carrier | LCD RST — plain GPIO |
| 16 | carrier | `spi0` RX ← SD MISO |
| 17 | carrier | SD CS — plain GPIO |
| 18 | carrier | `spi0` SCK |
| 19 | carrier | `spi0` TX → SD MOSI |
| 20–21 | — | free |
| 22 | carrier | SD card detect, **active low**, needs an internal pull-up |
| 23 | *board* | SMPS mode (non-W) / `WL_REG_ON` (W) |
| 24 | *board* | VBUS sense (non-W) / CYW43 data (W) |
| 25 | *board* | status LED (non-W) / CYW43 CS (W) |
| 26 | carrier | PWM5 A → audio **left**, via an RC low-pass into the amplifier |
| 27 | carrier | PWM5 B → audio **right** |
| 28 | — | free (ADC2) |
| 29 | *board* | VSYS/3 on ADC3 (non-W) / CYW43 clock (W) |

So a PicoCalc application has roughly **eleven free GPIOs** (0–5, 8–9, 20–21,
28) plus, on a Plus 2 W, the RP2350B's extra pins that are not on the header.
"Free" here means *no driver in this tree touches it*; confirm against the
PicoCalc schematic before you route a signal to one, because the carrier may
still connect it.

### 1.2 Four things the hardware does not give you

Design around these from the start; each one has cost a project time.

- **No tearing-effect (TE) signal.** Six wires reach the panel and none of them
  is TE, so you cannot sync a present to the panel's own refresh. Tearing is
  managed by *when and in what order* you send, not by waiting for a strobe.
- **No keyboard interrupt.** The southbridge has no interrupt line to the
  processor (the pin exists in its firmware and is commented out). Input is
  polled, always.
- **No audio DAC.** PWM is the output hardware. Whatever you want to hear, you
  synthesize or decode in software.
- **No battery-backed real-time clock.** The RP2350's AON timer (RP2040's RTC)
  loses time at power-off. Wall-clock time comes from the network on a W board,
  or from the user.

---

## 2. The processor board

### 2.1 The five boards

| Board | Part | Cores | SRAM | Flash | PSRAM | Radio |
|---|---|---|---:|---:|---:|---|
| Pico | RP2040 | 2 × M0+, 200 MHz max | 264 KB | 2 MB | — | — |
| Pico W | RP2040 | 2 × M0+, 200 MHz max | 264 KB | 2 MB | — | CYW43439 |
| Pico 2 | RP2350A | 2 × M33, 150 MHz | 520 KB | 4 MB | — | — |
| Pico 2 W | RP2350A | 2 × M33, 150 MHz | 520 KB | 4 MB | — | CYW43439 |
| Pimoroni Pico Plus 2 W | RP2350**B** | 2 × M33, 150 MHz | 520 KB | 16 MB | 8 MB | CYW43439 |

Pico Logo ships for the bottom three; the RP2040 rows are here because the
carrier accepts them, not because they have been exercised.

### 2.2 RP2040 versus RP2350, in the terms that change your code

| | RP2040 | RP2350 |
|---|---|---|
| Core | Cortex-M0+ | Cortex-M33 (or Hazard3 RISC-V, switchable at boot) |
| Float | **no FPU** — single precision is a bootrom software library | **single-precision FPU in hardware**; double is still software |
| SRAM | 264 KB | 520 KB |
| DMA channels | 12 | 16 |
| PWM slices | 8 | 12 |
| PIO blocks | 2 × 4 SMs | 3 × 4 SMs |
| ADC channels | 4 + temperature | 4 + temp (RP2350A) / 8 + temp (RP2350B) |
| Bank 0 GPIOs | 30 | 30 (A) / 48 (B) |
| External QSPI | flash only | flash **and** a second chip select for PSRAM |
| XIP cache | 16 KB | 16 KB |
| Also | — | HSTX, TrustZone, signed boot |

Two consequences worth stating plainly:

- **Use `float`, not `double`, everywhere.** On RP2350 that is a hardware
  instruction and on RP2040 it is the cheaper of two software paths. A stray
  `double` literal (`2.0` instead of `2.0f`) silently promotes an entire
  expression.
- **An RP2040 build is a different memory problem, not a slower one.** A 320×320
  8-bit framebuffer is 100 KB: 19 % of an RP2350's SRAM and **39 % of an
  RP2040's**. If you intend to support a Pico, size the framebuffer question
  first and everything else after.

**Erratum RP2350-E9** affects GPIOs used as inputs with the internal *pull-down*
enabled: the pad can latch at an intermediate level instead of reading low.
Prefer active-low inputs with pull-ups (which is what the carrier does for card
detect), or fit an external pull-down, and read the current errata list in the
RP2350 datasheet before wiring a button to a spare pin.

### 2.3 SRAM is the scarce resource

On RP2350, 520 KB total. Pico Logo's firmware sits at **87–96 % of RAM**
depending on preset, and the failure mode is worth knowing: an oversized static
buffer does not fail at link time in any obvious way — it fails at runtime as an
out-of-memory panic during init, from whichever allocation happened to run last.

Framebuffer arithmetic, since it dominates everything else:

| Buffer | Bytes | Verdict |
|---|---:|---|
| 320×320 @ 8 bpp indexed | 102,400 | fits, with room to work |
| 320×320 @ 16 bpp RGB565 | 204,800 | 39 % of SRAM; possible but crowds everything |
| Double-buffered 8 bpp | 204,800 | same cost, and see §4.10 |
| Double-buffered 16 bpp | 409,600 | **impossible** |
| Two DMA line buffers (320 px × 2 B × 2) | 1,280 | what a streaming blit needs |
| One 16 bpp tile row band (320 × 16 px) | 10,240 | a banded renderer's whole working set |

**An 8-bit indexed canvas with a 256-entry RGB565 palette expanded at blit time**
is the model that leaves you room for a program. It is not the only one — a
band renderer that never stores a full frame is cheaper still (§4.10) — but
16 bpp full-frame plus double buffering is off the table on every board.

Keep fixed capacities in one header rather than scattered `#define`s; you will
be trading them against each other repeatedly.

### 2.4 PSRAM — Plus 2 W only

8 MB, on QMI chip select 1 (GPIO 47), brought up by the SDK before `main()` and
appearing as a memory-mapped window. Three things to know:

- **Verify it before trusting it.** The SDK's init checks the chip ID but not
  that the window works at speed. Write distinct patterns at spread offsets,
  `xip_cache_clean_all()` + `xip_cache_invalidate_all()` to force a real round
  trip, then read back. A marginal QMI configuration then degrades to SRAM-only
  instead of handing your allocator a region that hardfaults on use. See
  `psram_verify()` in [main.c](../devices/picocalc/main.c).
- **PSRAM reads go through the same 16 KB XIP cache as instruction fetch from
  flash.** Cached reads are SRAM-speed; a cache-hostile access pattern is not,
  and it contends with your code. §9.3 has the measurement.
- **Its QMI timing is computed from `clk_sys` once, before `main`.** Change the
  system clock and it is wrong — see §3.

Use PSRAM for **capacity** (level data, sprite banks, audio samples, a text
buffer) and SRAM for the **per-frame working set**.

### 2.5 The status LED

Not on the PicoCalc's front panel — it is the small LED on the processor board,
visible through the case. It is a different piece of hardware on each board:

- **Pico / Pico 2**: `PICO_DEFAULT_LED_PIN`, GPIO 25, a plain `gpio_put`.
- **Any W board**: there is no such pin. The LED is `CYW43_WL_GPIO_LED_PIN`,
  WL_GPIO 0 **on the wireless module**, reached by `cyw43_gpio_set`.

SDK 2.1's `pico_status_led` hides that difference and is the reason no board
conditional is needed. But the consequence does not go away: **lighting the LED
on a W board powers the radio**, so the first call costs the CYW43 firmware
upload (about a second) and leaves the module up.

If you use both the LED and Wi-Fi, bring the driver up **once**:
`status_led_init()` with no argument builds its own `async_context` and calls
`cyw43_driver_init` itself, which a later `cyw43_arch_init` then trips over.
Pass `status_led_init_with_context(cyw43_arch_async_context())` after your own
Wi-Fi init instead — one context, one driver, one initialised flag. This costs
72 bytes of otherwise-dead SRAM on a W board and is still the right trade.

### 2.6 Wi-Fi

Present on Pico W, Pico 2 W and Plus 2 W (all CYW43439). Two hardware facts
matter more than the driver API:

- **The radio eats SRAM.** The driver plus lwIP is a large fixed cost on a board
  that has no PSRAM to move it to. On a Pico 2 W this is the difference between
  budgets, not a rounding error.
- **TLS effectively needs PSRAM.** mbedTLS's handshake heap does not fit
  alongside a framebuffer in 520 KB. Pico Logo's rule — plain HTTP on a Pico
  2 W, HTTPS only on the Plus 2 W — is a memory consequence, not a policy.

The driver reaches the chip over a **PIO SPI whose divider is fixed against
`clk_sys` when the bus comes up** (§3).

### 2.7 Getting code onto the board, and getting output back

- **SWD is the practical path.** The PicoCalc leaves the module's debug pads
  reachable; a CMSIS-DAP probe plus OpenOCD flashes and resets in one step, with
  no case-opening to reach BOOTSEL. See [flash.sh](../flash.sh) for the exact
  invocation (`target/rp2350.cfg`, `adapter speed 5000`).
- **UF2 over USB works** if you can reach the BOOTSEL button.
- **`printf` has nowhere to go by default.** Pico Logo disables both USB and
  UART stdio (`pico_enable_stdio_usb(target 0)`, likewise UART) and installs its
  own `stdio_driver_t` that writes to the LCD. Decide early which you want: your
  own screen driver, USB CDC for a host log, or UART on GP0/GP1. You can have
  all three — `stdio_set_driver_enabled` chains them.

---

## 3. Clocks, voltage and overclocking

The SDK's default `clk_sys` is **150 MHz** on RP2350 and **125 MHz** on RP2040,
but neither default is the part's limit, and the two parts differ in whether
going faster is *supported* or merely *possible*.

**RP2040 is rated to 200 MHz**, not the 133 MHz its original datasheet
specified — Raspberry Pi re-rated the part, and the SDK carries the higher
figure as the fastest *supported* clock. It is not the default, for backward
compatibility; you opt in with `PICO_USE_FASTEST_SUPPORTED_CLOCK=1` (or by
setting `SYS_CLK_KHZ` yourself). Two things come with it, both visible in
`hardware/clocks.h`: the SDK switches to a 1200 MHz VCO ÷6 ÷1 for the system
PLL, and — because 200 MHz needs more than the stock rail —
`SYS_CLK_VREG_VOLTAGE_AUTO_ADJUST` turns itself on and raises the core to
**1.15 V** during clock init, with a settle delay. That is a supported
configuration, not an overclock.

**RP2350 is rated to 150 MHz and the SDK offers nothing above it.** It will run
well past that — Pico Logo runs whole games at 300 MHz on all three of its
boards — but **everything above 150 MHz is outside the datasheet**, and you are
raising the rail yourself (the rail note at the end of this section).

Either way, the interesting part is not whether the core survives. It is that
five other things are derived from `clk_sys`, and each of them breaks
differently.

**Measured, Plus 2 W, 2026-08-23** (interpreter throughput, so pure CPU work):

| `clk_sys` | CPU work | speedup | clock ratio |
|---:|---:|---:|---:|
| 150 MHz | 52.5 µs/statement | 1.000 | 1.000 |
| 200 | 39.5 | 1.329 | 1.333 |
| 250 | 31.5 | 1.667 | 1.667 |
| 300 | 25.5 | 2.059 | 2.000 |

Inside 3 % at every point: **CPU-bound code is purely clock-bound here.** Die
temperature went 24.3 → 26.9 °C over a 200-frame run at 300 MHz. Thermals are
not a constraint at any of these clocks.

What you must fix up when you move the clock:

1. **`clk_peri` does not follow `clk_sys` by default.** `set_sys_clock_pll`
   parks it on the USB PLL at 48 MHz, so an `spi_set_baudrate(75 MHz)` quietly
   delivers 24 MHz and your display gets *slower* the faster you clock the CPU —
   we measured a present going 19.6 → 58 ms, flat across every overclock,
   because 48 MHz does not care what `clk_sys` is doing. Build with
   `PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1`, then **re-apply your SPI
   baud rate after every clock change**.
2. **The SPI dividers are coarse, and it decides which overclocks are worth
   taking.** The SPI divides `clk_peri` by an even prescale times a post-divider,
   so only clocks that reach the panel's rate *exactly* keep the display at full
   speed:

   | `clk_sys` | LCD SPI actually gets |
   |---:|---:|
   | 150 MHz | **75 MHz** (÷2) |
   | 200 | 50 MHz |
   | 250 | 62.5 MHz |
   | 300 | **75 MHz** (÷4) |

   An overclock to 200 MHz makes your code 1.33× faster and your display
   **1.5× slower**. 300 MHz is the only overclock that is free on both.

   This table is about `clk_peri`, not about which part you are on, so it
   applies just as much to an RP2040 taking its supported 200 MHz: a Pico at
   200 MHz drives this panel at **50 MHz**, so every figure in §4.7 stretches by
   half again. Nothing here has measured that — but the wire term is arithmetic,
   and on an RP2040 the display, not the core, is what you would be trading
   away.
3. **PSRAM QMI timing is computed once, before `main`.** Divider, RX delay, max
   select and min deselect all come from `clk_sys` at runtime init. At 300 MHz
   three of them are wrong in the unsafe direction simultaneously, and the part
   does not fault — it returns occasional bad data. This was found as scattered
   corruption in a text buffer. Recompute and write `qmi_hw->m[1].timing`
   immediately after the PLL moves; see `picocalc_retune_psram()` in
   [picocalc_hardware.c](../devices/picocalc/picocalc_hardware.c). It is one
   register write — do **not** use `psram_reinitialize()`, which tears XIP down.
4. **The CYW43 bus divider is fixed at bus bring-up.** It divides `clk_sys` by 2,
   so at 250 MHz the chip is clocked at 62 MHz instead of 37 and answers with
   garbage headers. Scale the divider with the clock
   (`cyw43_set_pio_clkdiv_int_frac8`). A *running* radio can be retuned by
   bringing only the bus down and up — `cyw43_spi_deinit` / `cyw43_spi_init` —
   which preserves the association, lwIP state and open sockets. Tearing down
   `cyw43_arch` is the wrong seam.
5. **A PWM audio carrier is `clk_sys` divided by your wrap**, so the sample rate
   and every rate derived from it move with the clock. Recompute, do not
   hard-code (§5.2).

And the voltage: the core rail is 1.10 V, which is what 150 MHz is rated
against. Raise to `VREG_VOLTAGE_1_20` **before** the PLL goes up and lower it
**after** it comes back down, with a millisecond to settle, so the rail is never
the lower of the two while the clock is the higher.

Flash XIP also runs at the new clock, and there is little you can do about it —
but the bootrom's divider is conservative, 300 MHz has run whole test sessions
on every board, and the failure mode if flash could not keep up would be a hang
at the moment of the switch rather than quiet corruption.

---

## 4. The display

### 4.1 The panel

A **320×320 IPS panel** on an ST7789P/ST7365P-class controller with **480 rows
of frame memory**. RGB565, 16 bits per pixel on the wire. The extra 160 rows of
memory are not wasted — they back the controller's hardware vertical scroll
(§4.8).

### 4.2 Wiring and SPI setup

Four-wire SPI on `spi1`: SCK on GP10, MOSI on GP11, MISO on GP12, with **CS
(GP13), D/CX (GP14) and RST (GP15) driven as ordinary GPIOs**. The SDK's SPI
peripheral does not manage chip select here; you do, and that turns out to
matter (§4.3).

`LCD_BAUDRATE` is **75 MHz**. The ST7789P datasheet says 62.5 MHz; 75 MHz is a
*qualified operating point on this board* — exercised on hardware, reliable, and
the source of every present measurement below — but it is qualified only while
the CS timing rule holds. It does not license going higher.

### 4.3 The 40 ns chip-select rule — the one that costs a week

The controller requires a minimum **40 ns CS-high pulse** before a RAM write.
At 75 MHz you cannot insert that with a delay loop without paying for it on
every transaction. The trick is to let the instructions you already need supply
the gap:

```c
// DO NOT MOVE. These two lines must precede the CS low, not follow it:
// they are what creates the required CS-high interval.
spi_set_format(LCD_SPI, 16, 0, 0, SPI_MSB_FIRST);
gpio_put(LCD_DCX, 1);          // data
gpio_put(LCD_CSX, 0);          // ...now assert CS
```

Every 16-bit write path in [lcd.c](../devices/picocalc/lcd.c) carries that
comment. Get it wrong and you see intermittent corrupt pixels that look like a
wiring or clock problem.

### 4.4 Initialisation

The sequence — software reset, positive/negative gamma, power control 1–3, VCOM,
`MADCTL 0x48` (BGR filter order, top-to-bottom, left-to-right), `COLMOD 0x55`
(16 bpp), interface mode, frame-rate control, `INVON`, display inversion
control, display function control, entry mode, two manufacturer commands,
`VSCRDEF`, `SLPOUT` — is in
[`lcd_init()`](../devices/picocalc/lcd.c). It is transcribed from working code
and is known good on this panel. **Copy it rather than rederiving it from the
datasheet**; the gamma and power tables in particular are panel-specific and the
datasheet defaults do not look right on this glass.

The two timing requirements in it are real: a ≥10 µs reset pulse followed by
**120 ms** before `SLPOUT` (5 ms is the reset minimum, 120 ms is the sleep-out
one), and ≥5 ms after the software reset. Clear the frame memory *before*
`DISPON` or the user sees a screen of power-on garbage.

### 4.5 Addressing and colour

`CASET` (columns) and `RASET` (rows) set an inclusive window; `RAMWR` then
streams pixels into it, left to right, top to bottom, wrapping at the window
edge. Window setup is three commands and eight bytes of parameter — at 75 MHz,
under a microsecond of wire, which is why **splitting a present into many
windows is nearly free** (§4.7).

RGB565 on the wire, big-endian: high byte first. The controller expands to
18 bits internally; entry mode `0x06` sets the conversion so that r(0) = b(0) = 0.

### 4.6 Getting pixels out

Three approaches, in ascending order of effort and descending order of CPU cost:

**(a) Blocking writes.** `spi_write16_blocking` per row or per pixel. Simple,
correct, and the CPU is stalled on the wire the whole time: a full 320-px row is
68 µs of doing nothing, ~22 ms for a screen. Fine for setup and text, wrong for
a frame loop.

**(b) One DMA channel, two line buffers, ping-ponged.** The pattern that fits
this panel:

```
lcd_blit_begin(x, y, w, h)     set CASET/RASET/RAMWR, 16-bit format, CS low, hold it
  lcd_blit_row(row) × h        expand row N into line buffer A
                               while DMA streams line buffer B to the SPI TX FIFO
lcd_blit_end()                 wait out the last DMA, drain the wire, release CS
```

The channel is configured **once** — DREQ-paced by `spi_get_dreq(spi1, true)`,
`DMA_SIZE_16`, read increment on, write increment off. Per row you set only the
read address and the transfer count. The CPU's visible cost per row drops from
~68 µs to ~5–8 µs (whatever your row transform costs), and it is spent doing
useful work rather than waiting.

Four details that are not optional:

- **Poll for completion; do not take an interrupt.** You have useful work — the
  next row — while you wait, and leaving the LCD DMA interrupt-free means your
  audio engine owns its DMA IRQ uncontended (§5.4).
- **Drain properly at the end.** The DMA only fills the TX FIFO. Wait for
  `SSPSR_BSY` to clear, discard everything the full-duplex SPI clocked into the
  RX FIFO, and clear the overrun flag (`SSPICR_RORIC`), or the next transaction
  inherits garbage.
- **Restore 8-bit format** when the window closes; commands are 8-bit.
- **Leave interrupts enabled for the whole blit.** The controller tolerates SPI
  clock pauses mid-window, so an audio refill IRQ may preempt a blit freely.
  This is load-bearing — see §5.7.

**(c) PIO.** Not needed here and never built: the SPI peripheral plus one DMA
channel already saturates the wire at 75 MHz, and PIO would buy you a
different pixel format or a wider bus that the panel does not have. Spend the
state machines elsewhere.

### 4.7 What the wire costs

Theoretical ceiling at 75 MHz, 16 bpp: **4.69 Mpx/s**, i.e. 21.8 ms for
320×320.

Measured on a **Pico 2, 2026-08-01**, timing forced full-region presents twenty
times through an 8-bpp-indexed → RGB565 expanding pipeline:

| Region | Pixels | Wall time |
|---|---:|---:|
| Full screen 320×320 | 102,400 | **25.6 ms** |
| 256×320 (a 16-column viewport) | 81,920 | 21.1 ms |
| 64×320 strip | 20,480 | 5.45 ms |
| One 16-px column, full height (16×320) | 5,120 | 1.259 ms |
| Fixed cost per present | — | 0.41 ms |

**The effective rate is 4.0 Mpx/s — 17–21 % below the wire math.** The figures
are internally consistent (slope 1.259 ms per column, intercept 0.41 ms), so the
gap is real work, not noise: the source copy, the palette expansion, and the
fact that a full-height present with one dirty span per tile row is *twenty*
blit windows rather than one.

Two conclusions to design against:

- **Cost is per pixel, not per present.** The intercept is 0.41 ms for the whole
  screen. Narrowing a viewport buys back exactly its area share; splitting a
  present into several windows is nearly free. A HUD column that never changes
  is worth its area in milliseconds every frame.
- **A native RGB565 source streams closer to the 21.8 ms wire figure**, because
  it deletes the expansion. That is the trade against §2.3's memory arithmetic:
  8 bpp costs ~17 % of present time and saves 100 KB.

Frame ceilings, if you present serially on one core:

| Strategy | Present cost | Ceiling |
|---|---:|---:|
| Full screen every frame | 25.6 ms | ~39 fps with nothing left over |
| 256×320 viewport every frame | 21.1 ms | ~47 fps |
| Dirty tiles only, a few sprites moving | 1.6–2.7 ms | not the constraint |

### 4.8 Hardware vertical scroll

`VSCRDEF` defines top-fixed / scrolling / bottom-fixed bands within the 480 rows
of frame memory; `VSCSAD` sets which memory line appears at the top of the
scrolling band. Bump the offset and the whole band moves with **no pixels sent
at all** — you then only draw the newly exposed line or band.

The price is that **every blit must remap its y through the scroll offset**,
including the wrap around 480 rows. Get it wrong and drawing lands
somewhere plausible-but-wrong, which is a miserable class of bug. If you use
hardware scroll, put the remap inside your single `blit_begin` and give no other
path direct access to `RASET`.

For a game this is the cheapest lever on the machine: **a vertical scroller gets
its background for the cost of one exposed band instead of 25 ms**. It buys
nothing for horizontal motion, and it fights any other use of the scroll offset
(text scrolling, split screens) — you get one scroll register, so pick one
customer for it.

### 4.9 Readback and tearing

The MISO line is wired and `RAMRD` exists, so the frame memory *can* be read
back — but the controller's read timing is far slower than its write timing, so
this is a diagnostic, not a data path. **Keep your own copy of anything you need
to read.**

There is no TE line (§1.2). If you present a moving object faster than the panel
scans, you will see tearing. The mitigations available are ordering ones: keep
presents small and localised so a tear affects one tile rather than a whole
sprite; present the moving band before the static one; or accept it — at 320×320
and typical panel refresh, most games do.

### 4.10 Choosing a framebuffer model

The hardware constrains this more than taste does. Four models that work, with
what each costs:

| Model | SRAM | Present cost | Notes |
|---|---:|---|---|
| 8 bpp indexed full frame + palette | 100 KB | 25.6 ms full, linear in area | what §2.3 recommends; readback is cheap |
| 16 bpp full frame | 200 KB | ~21.8 ms full | fastest wire, crowds everything else |
| Band renderer (N-row strip, regenerated) | 5–20 KB | wire-rate, but you redraw *everything* every frame | no canvas to read back; scrolling is free |
| Dirty-region 8 bpp | 100 KB + tracker | 1–3 ms typical | best for a static background with moving objects |

**Double buffering is not available** at any of these sizes, so if you need
tear-free full-screen motion the answer is a band renderer (which is
double-buffered a strip at a time) or the hardware scroll, not a second frame.

If you do go dirty-region: a **20×20 grid of 16×16 tiles, one inclusive
`[min..max]` column span per tile row**, is 40 bytes of state for the whole
screen, O(1) to mark and ≤20 windows to flush. Over-mark rather than under-mark
— a few extra tiles cost microseconds and a missed tile leaves a stale sprite on
screen forever. Snapshot the dirty state and clear it *before* sending, so
writes that happen during the blit are tracked for the next present. Marking
overhead is negligible and we measured it (~20 µs over 288 rows against a 7.6 ms
draw); do not optimise it. The tracker in
[dirty_tiles.c](../devices/picocalc/dirty_tiles.c) is deliberately free of SDK
dependencies so it can be unit-tested on a host, which was worth doing.

### 4.11 Backlight

The LCD backlight is **not** a PWM pin on the processor — it is register `0x05`
on the southbridge (§6). Levels are stepped to multiples of 16 and clamped to
16–240 by the keyboard MCU, so a smooth fade is 15 steps, not 256. Dimming on
pause is a cheap piece of polish and costs one I²C transaction.

---

## 5. Audio

### 5.1 The output stage

There is no DAC. The PicoCalc routes **GP26 (left) and GP27 (right) through an
RC low-pass filter into an amplifier**. Two facts make the output stage almost
free:

- GP26 and GP27 are **channels A and B of the same PWM slice** (slice 5 on every
  board — `(gpio >> 1) & 7` — but ask `pwm_gpio_to_slice_num` and never hard-code
  it). One slice clocks both ears sample-locked, and the two compare registers
  share **one 32-bit word**: `left | (right << 16)`. A stereo frame is a single
  32-bit write, so you need one DMA stream, not two.
- The slice's wrap event is a **DMA pacing signal** (`pwm_get_dreq(slice)`), so
  a DMA channel can stream compare values from a buffer at exactly the sample
  rate with zero CPU per sample.

### 5.2 Choosing a carrier and a sample rate

The PWM wrap sets both at once, and they trade against each other:

```
carrier Hz    = clk_sys / wrap
resolution    = log2(wrap) bits
sample rate   = carrier / oversample
```

At 150 MHz, a wrap of **2048 gives a 73.2 kHz carrier at 11 bits**. That is the
sweet spot on this hardware: ultrasonic, so the RC filter and the speaker remove
it without any filtering of your own, and 11 bits is more dynamic range than the
speaker resolves. Writing each mixed frame twice (oversample 2) gives a
**36.6 kHz sample rate** — an 18.3 kHz Nyquist limit, comfortably above anything
the output stage passes.

Push the wrap up for resolution and the carrier drops into the audible band;
push it down for carrier headroom and you lose bits. If you want 12 bits, you
need the clock: at 300 MHz a wrap of 4096 still leaves a 73 kHz carrier.

**Recompute these from `clock_get_hz(clk_sys)` at init and again on any clock
change** (§3), rather than baking 73,242 into a constant.

### 5.3 Streaming without gaps

Two DMA channels, each **chained to the other**, ping-ponging through a two-half
ring; each is paced by the slice's wrap DREQ and writes the 32-bit compare word.
When a half drains, that channel's IRQ refills it. Chaining is what makes
playback gapless — there is never a moment when no channel is armed.

Two hardware details that are not obvious and cost real debugging time:

- **Size and align the whole ring to a power of two, and use
  `channel_config_set_ring()` on the read address.** This is your safety net for
  IRQ starvation. If a refill is delayed past a half draining, a non-wrapping
  DMA marches into adjacent RAM and plays it as an audible burst of static; with
  the hardware wrap it cleanly replays the ring instead — silence at idle, a
  brief stutter under a note. You need this because **flash program/erase masks
  interrupts with XIP offline for tens of milliseconds** (§7.2), which no amount
  of careful IRQ design avoids.
- **When you re-arm a chained channel from its IRQ, reset both the read address
  and the transfer count.** A chain trigger reloads neither. Reset only the
  address and the count is still zero, so the next chain completes instantly and
  storms the IRQ.

Ring size sets your latency and your deadline. 256 slots per half at 2×
oversample is 128 frames, i.e. **one refill every ~3.5 ms** — for 2,048 bytes of
SRAM. Halve it for latency, double it for slack.

### 5.4 The deadline, and IRQ priority

**Everything about audio on this machine reduces to one number: the refill
deadline.** At 3.5 ms, any handler that can run longer than that at the same
NVIC priority will produce audible clicks, because same-priority interrupts
cannot preempt each other.

There is one such handler on this board whether you write it or not: **a
southbridge poll blocks 4–5 ms on the 10 kHz I²C bus** (§6). Put that in a timer
IRQ at default priority and a sustained note clicks about ten times a second.

So: `irq_set_priority(DMA_IRQ_0, 0x40)` — above the default `0x80` — so the
audio refill preempts everything else. The I²C peripheral runs autonomously and
is unharmed by being preempted. And put **every function on the refill path**
behind `__not_in_flash_func`, so it survives flash writes and adds no XIP misses
to a latency-critical path.

Flash program/erase is the one accepted violator of the deadline. Everything
else is a bug.

### 5.5 What a mixer costs

A software PSG — 8 voices (three tone plus one noise per ear), phase-accumulator
oscillators, 16-bit LFSR noise, per-voice ADSR, integer throughout — costs about
**35 cycles per voice per frame**: 8 voices × 36.6 kHz ≈ **7 % of one core at
150 MHz**, in interrupt context, regardless of what the foreground is doing.
That is the budget for a synth. Sample playback is cheaper; anything with a
filter per voice is not.

Techniques that made that number achievable, all of them portable to whatever
you build:

- **Phase accumulators**: `phase += (freq << 32) / sample_rate`, take the
  waveform from the top bits. No trig, no tables, exact frequency.
- **Advance envelopes once per refill block, not per sample.** At 3.5 ms
  granularity this is inaudible for millisecond-scale envelopes and it keeps the
  inner loop to an add and a lookup.
- **A logarithmic volume ladder** (16 steps of 2 dB, the TI/Atari curve) scaled
  to 0..256. Linear volume sounds wrong and costs the same.
- **Fix your headroom by construction.** Four voices summed per ear, `>> 2`,
  saturate, bias to mid: four simultaneous full-scale voices cannot wrap. Wrap
  is not a soft failure — it is a full-scale discontinuity.
- **Crest factor is not loudness.** A square wave has a crest factor of 1 and
  speech has 4–8, so matching *peaks* leaves speech ~20 dB below a tone in the
  RMS the ear reads as loudness. If you mix synthesis and samples, tune by ear
  and expect the sample to need several times the "correct" gain.

For the foreground-to-IRQ handoff, a **lock-free SPSC queue per voice** (one
reserved slot so full and empty are distinguishable) removes almost all critical
sections; the few instructions that touch shared voice state go inside
`save_and_disable_interrupts()`.

### 5.6 Other things you could do with this output stage

- **Sample playback** — the DMA path is format-agnostic; anything you can turn
  into 11-bit stereo frames at your chosen rate plays.
- **Mono at double resolution** — not available: the two compare registers are
  physically separate outputs. You would sum in software and write both.
- **I²S to an external DAC over PIO** — possible in principle, but there is no
  DAC on the carrier and no free pins near the audio jack, so this means
  hardware work.
- **1-bit / PDM tricks** — the RC filter is sized for a ~73 kHz carrier; anything
  that puts energy lower will be heard.

### 5.7 Three bring-up findings

All three presented as "audible clicking" and all three had different causes.
The common thread is §5.4's deadline.

1. **The DMA ring wrap** (§5.3). Clicks at idle, static on a mode switch,
   because the IRQ reset the read address and a masked IRQ let the DMA run off
   the end of the buffer.
2. **The LCD driver was masking interrupts.** Stutter under any large screen
   update, because the driver wrapped every screen operation in
   `save_and_disable_interrupts()` — purely because a cursor-blink *timer* drew
   to the LCD from IRQ context. The fix was to make the timer set a flag and do
   the drawing in the foreground, after which all interrupt masking was deleted
   from the LCD driver. **The invariant that came out of it: the LCD is only ever
   touched from thread context, never from an interrupt handler.** Write that at
   the top of your display driver.
3. **IRQ priority** (§5.4), the keyboard poll case.

---

## 6. Keyboard, battery, backlight, power

None of this is wired to the processor. An **STM32F103R8T6 on the PicoCalc
mainboard** owns the key matrix, both backlights, the battery gauge and the
power rail, and exposes all of it as an **I²C slave at address `0x1F`, 10 kHz**.

Protocol: write the register number, then read two bytes. **OR `0x80` into the
register number to write it.** Always use the `*_timeout_us` variants — a wedged
bus must not hang the machine.

| Reg | Name | Read gives | Notes |
|---|---|---|---|
| `0x01` | `VER` | firmware version | |
| `0x04` | `KEY` | FIFO depth (low 5 bits), caps bit 5, num bit 6 | |
| `0x05` | `BKL` | LCD backlight | writable; stepped to multiples of 16, clamped 16–240 |
| `0x08` | `RST` | — | **a read resets the MCU** after 1 s; a write resets after *value* seconds |
| `0x09` | `FIF` | `[state, key]`, one event per read | `[0,0]` when empty |
| `0x0A` | `BK2` | keyboard backlight | writable, steps of 32 |
| `0x0B` | `BAT` | percent in bits 0–6, **bit 7 = charging** | refreshed by the MCU every 20 s, not on demand |
| `0x0C` | `C64_MTX` | 10 bytes of raw matrix bitmap | reply is longer than two bytes |
| `0x0D` | `C64_JS` | arrows + Enter as a C64-style joystick | |
| `0x0E` | `OFF` | — | write to power off; value is clamped to ≥6 and used as a **delay in seconds** |

Registers `0x02` (`CFG`), `0x03` (`INT`), `0x06` (`DEB`) and `0x07` (`FRQ`) are
**not in the firmware's dispatch** and reply `[0,0]`. That matters: `CFG` is
where you would ask for raw keys instead of MCU-resolved modifiers, and you
cannot.

### 6.1 What it costs, and how to poll it

- **Each transaction costs 4–5 ms of wall time** at 10 kHz. That is the single
  most expensive routine operation on this machine, and it is why §5.4 exists.
- **Poll from your frame loop, in thread context** — at 25–60 Hz — not from a
  timer IRQ. Same cost, but it lands somewhere you can account for it and it
  cannot preempt anything.
- **Guard the bus with a flag.** An atomic bool checked before starting a
  transfer keeps a foreground battery read and a background key poll from
  interleaving.
- **Drain more than one FIFO entry per poll.** The FIFO holds 31 entries and a
  held key produces an event every 100 ms on top of press/release pairs; a
  one-event-per-tick consumer falls behind and the machine feels laggy.
- **The MCU resets its own I²C bus** if it sees no transaction for 2.5 s (after
  the first 10 s of uptime). A game that stops polling for seconds will come
  back to a slave that has just re-initialised.

### 6.2 The event model, and what to build from it

Every FIFO entry is a `[state, keycode]` pair — pressed, held, released — and the
modifier keys report as their own events (`0xA1`–`0xA5`). The MCU scans the
matrix every ~26 ms and a held key produces **nothing for 300 ms, then one event
per 100 ms**.

That cadence is a *typing* cadence, and it is wrong for a game. Build a
**held-key bitmap from press/release events** — 256 bits is 32 bytes — and ask
"is this key down now" instead of consuming a character stream. A frame loop
reading one buffered character per frame consumes slower than the repeat
produces, so the backlog grows and the ship keeps turning after the player lets
go; and one character per frame means two keys can never be held at once. Keep a
separate "pressed since last frame" latch for edge-triggered actions.

### 6.3 The keymap decides which chords exist

**Shift is resolved in the MCU, not host-side, and it can swallow a key
entirely.** Each key has a character and an *alternate*; a shifted non-letter is
replaced by its alternate, and if the alternate is zero **nothing is sent at
all**. Shift + Left, Shift + Right, Shift + Space and Shift + Backspace are
therefore unreachable by any host code. Ctrl and Alt are different — they pass
the key through unchanged alongside a modifier event, so Ctrl chords are yours
to define.

Several keys exist *only* as shifted alternates: Home is Shift+Tab, End is
Shift+Del, PgUp/PgDn are Shift+Up/Down, Break is Shift+Esc, Insert is
Shift+Enter or Alt+I. And Alt+`,`/`.`/Space/`B` are consumed by the MCU itself
for the backlights and battery display.

**Read the keymap before designing a binding.** The full register map, event
cadence, matrix tables and the list of swallowed chords are in
[keyboard-firmware-notes.md](keyboard-firmware-notes.md), which is the authority
for this section.

---

## 7. Storage

### 7.1 SD card

`spi0` at **400 kHz for initialisation, then 25 MHz** (the SD specification's
maximum in SPI mode). Card detect is on **GP22, active low**, and needs an
internal pull-up. 512-byte blocks. If you would rather not pull in a
filesystem library, a block driver plus a FAT32 layer is about 3,000 lines here
([sdcard.c](../devices/picocalc/sdcard.c),
[fat32.c](../devices/picocalc/fat32.c)).

The SD card is on a different SPI instance from the display, so the two do not
contend for the bus — but they do contend for **your DMA channels and your
time**. Card I/O is not something to do inside a frame.

### 7.2 Internal flash, XIP, and the interrupt hole

Code executes from flash through the QMI in execute-in-place mode, cached by a
**16 KB XIP cache** shared by both cores (and, on a Plus 2 W, by PSRAM reads).
Any spare flash above your firmware is usable as storage — Pico Logo reserves
2 MB on the 4 MB boards and 8 MB on the 16 MB board, carved off the top, and puts
a LittleFS in it. Geometry is the SDK's: **4096-byte sectors, 256-byte pages.**

**Flash program/erase is the one place interrupts must be masked**, and the
recipe is not negotiable:

```c
uint32_t save = save_and_disable_interrupts();
xip_cache_clean_all();          // push dirty PSRAM lines out first
flash_range_erase(offset, FLASH_SECTOR_SIZE);   // bootrom, runs from RAM, XIP down
xip_cache_invalidate_all();     // drop now-stale flash/PSRAM cache lines
restore_interrupts(save);
```

Four consequences:

- **While XIP is down, any flash-resident ISR would hardfault.** Everything on
  an interrupt path that can fire during a write must be `__not_in_flash_func`.
- **The source buffer for a program must live in RAM.** The bootrom programs
  directly from it with XIP exited, so a pointer into flash rodata faults.
- **One sector or one page per interrupts-off window.** That bounds the stall to
  a single flash operation even for a large transfer — and the stall is still
  tens of milliseconds, so **audio stutters for the duration of a save**. The
  DMA ring wrap (§5.3) turns that from static into a repeat. Tell the player not
  to save mid-music, and never do flash I/O inside a frame loop: level loads,
  save games and high-score writes belong at scene boundaries.
- **If core 1 is running, wrap every flash write in `multicore_lockout`** — the
  other core cannot execute from flash while flash is being programmed. This is
  easy to get right once and easy to forget when a second writer appears later.

---

## 8. On-chip odds and ends

### 8.1 Die temperature

The RP2350's own sensor, so it costs no bus time and cannot be blocked by a
wedged southbridge. **The ADC channel differs between parts and the SDK already
hides it**: input 4 on RP2350A (Pico 2, Pico 2 W, QFN-60) and input 8 on
RP2350B (Plus 2 W, QFN-80), because the bigger package adds four user inputs
below it. `ADC_TEMPERATURE_CHANNEL_NUM` is `NUM_ADC_CHANNELS - 1` and
`NUM_ADC_CHANNELS` comes from `PICO_RP2350A` in the board header, so **no board
conditional belongs in your code**.

Two things the sensor's accuracy decides:

- **Average a burst.** A conversion is 12 bits over 3.3 V — about 0.24 °C per
  LSB, and noisy at that. Sixteen conversions cost ~32 µs total, which is
  nothing beside a 4.6 ms southbridge read.
- **Round the answer.** The datasheet formula (`27 - (V - 0.706) / 0.001721`) is
  *uncalibrated*, with several degrees of part-to-part offset, and it reads the
  die, which sits above the room by however hard the chip is working. A tenth of
  a degree is already generous.

Test it by *loading* the processor and requiring the reading to rise: a wrong
channel returns a floating GPIO, which reads as a plausible number.

The ADC block can be initialised lazily and the bias left on; `adc_init()`
touches no GPIO function, so the audio PWM on GP26/27 is unaffected.

### 8.2 The remaining ADC inputs

GP26 and GP27 are ADC0/ADC1 on the silicon but are the audio outputs here, so
they are gone. **GP28 (ADC2) is the one free analogue input** on a 30-pin board.
GP29 is VSYS/3 on non-W boards and the radio clock on W boards — on a W board,
reading VSYS means wrapping the access in `cyw43_thread_enter`/`exit`, and the
battery gauge on the southbridge (§6) is a better source anyway.

### 8.3 USB

The module's USB port is reachable through the case. It is available for CDC
stdio, for mass storage, or for a USB host stack — but see §2.7: nothing routes
`printf` there unless you ask.

---

## 9. Making it fast

This section is mostly about **how to measure on this machine**, which turned out
to matter more than any individual optimisation.

### 9.1 Measure on hardware, in the mode you ship

Three separate times a number that looked authoritative turned out to be an
artefact of the mode the harness ran in:

- **Presents can be free by accident.** A blit that returns early in text mode
  reported a present of 0.25 ms. Every "frame time" in a months-long series was
  a *body* time with no present in it.
- **The same bug hid a 25 ms cost.** Clearing the screen in text mode is a bare
  `memset`; in graphics mode it is a full-panel fill. A setup routine
  "regressed" from 20 ms to 66 ms purely because it was finally being measured in
  the mode it runs in.
- **Boundary and clipping modes interact with dirty tracking.** In one wrapping
  mode, a stroke's round cap spilled off the left edge onto the right and
  dirtied the entire tile row, making every measurement read as a full screen.

Corollaries we now follow:

- **Always carry a control** — a quantity in each run that should *not* change.
  When it finally moved 2 %, that told us we had reached the noise floor.
- **Expect ~2 % run-to-run spread** on this machine. Anything smaller is not a
  result. (A 150 MHz baseline read 65.5, 68.8 and 68.1 ms across three sittings
  on the same board.)
- **One board per comparison.** Different flash parts mean different
  instruction-fetch behaviour; we burned real time drawing a conclusion from the
  difference between a Pico 2 measurement and a Plus 2 W one.
- **Write measurements to a file.** A number on a 320×320 screen cannot be
  copied off it.

### 9.2 Instruction fetch through the XIP cache is a first-order cost

The largest single finding of this project. Code runs from flash through a
16 KB cache shared by both cores and by PSRAM data. Moving hot functions into
SRAM with `__not_in_flash_func` produced, on a Plus 2 W:

| Tier | What moved | Frame | Speedup | SRAM cost |
|---|---|---:|---:|---:|
| — | baseline | 81.0 ms | | |
| 1 | 4 hot functions (3.1 KB of expression evaluator) | 65.5 | **1.24×** | 6.0 KB |
| 2 | 8 more (call path, variable lookup) | 53.2 | **1.23×** | 3.6 KB |
| 3 | loop and run-list path | 48.1 | 1.105× | 1.7 KB (fit in existing padding) |
| 4 | frames and bindings | 47.0 | 1.024× | 2.0 KB |

**1.72× cumulative for 13.6 KB of SRAM**, verified each time by checking that
the symbols moved from `0x1…` to `0x2…` in the image.

Three lessons that generalise well beyond an interpreter:

1. **Returns halve every tier.** Stop when the curve says so, not when the
   budget runs out.
2. **Every move relays the flash residue against the cache, and whatever is left
   adjacent to the boundary gets worse.** Tier 1 improved the evaluator and
   regressed procedure calls 29 %; tier 2 fixed calls and regressed the loop
   67 %; tier 3 fixed the loop and regressed calls again. It is self-limiting —
   the regressing line is always smaller than the set that improved — but **you
   must re-measure the whole profile after every move**, not just the thing you
   moved.
3. **It is not universal.** The same technique applied to a tight tile-copy loop
   produced *nothing* (§9.3). Instruction fetch dominates code that is large and
   branchy; a copy loop is data-bound and does not care. Measure before spending
   SRAM.

The functions worth moving were the big branchy ones **and** the tiny
constantly-called accessors (120–164 bytes, the best value per byte). Whole
modules are the wrong unit; individual hot functions are the right one.

### 9.3 Where the data lives matters as much as where the code lives

A bulk copy from PSRAM into an SRAM canvas, in 8-byte runs, measured **7.45–7.6
ms for 64,512 pixels** on a Plus 2 W — about 118 ns per pixel, only ~2× faster
than the SPI wire, on a 150 MHz core. We chased it:

| Suspect | Verdict |
|---|---|
| Per-row bookkeeping | **Red herring** — ~20 µs total. |
| Code not RAM-resident | **Disproved by experiment.** Moving the loop and its helpers to SRAM (+2 KB) gave 7.45 → 7.6 ms: no change, or slightly worse. Reverted. |
| **The data path** | **The leading explanation.** Source in PSRAM, destination in SRAM, small runs. That fits 118 ns/px far better than instruction fetch does. |

So: **keep per-frame working sets in SRAM.** Use PSRAM for capacity, not for the
hot path — or at least measure before assuming the XIP cache hides it.

### 9.4 Small `memcpy` calls are call overhead

A sampler that copies one 8-pixel tile run at a time makes 28 calls per row and
8,000 for a screen. At that size the call *is* the cost. Hand-unroll, or
specialise for the two or three run lengths you actually support.

### 9.5 Core 1 is the largest unexploited lever

**Core 1 has never been started in this project**, and the DMA blit leaves core 0
about 85 % idle inside `dma_channel_wait_for_finish_blocking` — roughly 22 ms per
full-screen frame doing nothing. Moving the present to core 1 makes a frame
`max(body, present)` instead of `body + present`.

What we worked out before shelving it:

- **The usual objection does not apply.** Double-buffering a canvas is 100 KB and
  impossible — but a *generated* background (a band renderer, a tile sampler) is
  never stored, so there is no buffer to double. Core 1 needs read-only access to
  the world data for the duration of a frame plus a snapshot of the object table:
  a few hundred bytes, and a far smaller synchronisation problem.
- **The audio path does not move.** The LCD DMA is polled, not interrupt-driven,
  and the synth owns `DMA_IRQ_0` on core 0. Putting the blit on core 1 contends
  for no interrupt.
- **The unmeasured risk is XIP contention.** Both cores fetch through one 16 KB
  cache, and §9.2 shows how sensitive that is. The probe is deliberately dumb:
  core 1 blits a static buffer in a loop while core 0 runs the normal workload;
  compare core 0's timings with and without. Correctness and tearing are
  irrelevant to the probe — run it before you design around the answer.
- **Flash writes then need `multicore_lockout`** (§7.2).

For a C application this is a much easier call than it was for an interpreter
with a shared mutable heap: "core 1 owns the display" is a clean ownership split
rather than a locking problem.

### 9.6 Levers, in order

If you need to buy present time:

1. **Narrow the viewport.** Cost is exactly linear in area (§4.7). A static HUD
   column buys back its share every frame.
2. **Present less often than you simulate.** Scroll at half rate while objects
   move at full rate; ordinary frames go back to tile-sized dirty rects.
3. **Hardware vertical scroll** (§4.8) — nearly free vertical motion, at the cost
   of horizontal motion and of the scroll register.
4. **Overclock to 300 MHz, not 200 or 250** (§3). It is the only step that
   doubles the CPU without slowing the display.
5. **Core 1** (§9.5) — the biggest one, and the one nobody here has measured.

---

## 10. Bring-up order and trap checklist

A working order for `main()`, with the dependencies that force it:

1. **Southbridge I²C** — nothing else needs it, but the backlight does, and a
   dead bus is the first thing you want to know about.
2. **LCD** — reset, init sequence, clear frame memory, *then* `DISPON`.
3. **Keyboard** — on top of the southbridge.
4. **Audio** — claims two DMA channels and `DMA_IRQ_0`; do it before anything
   else claims channels opportunistically.
5. **PSRAM verification** (Plus 2 W), *before* handing the region to an
   allocator and before any flash mount, since flash writes drive the same QMI.
6. **Filesystems** — internal flash, then SD.
7. **Wi-Fi** — lazily, on first use; it costs a second and a lot of SRAM.

The traps, in rough order of how much time each one cost us:

- [ ] `spi_set_format()` and the `D/CX` write go **before** CS low — the 40 ns
      CS-high rule (§4.3).
- [ ] Drain the SPI RX FIFO and clear the overrun flag at the end of every DMA
      blit; restore 8-bit format.
- [ ] Never touch the LCD from an interrupt handler. Timers set flags.
- [ ] Never mask interrupts around a blit — audio has a 3.5 ms deadline.
- [ ] Audio DMA ring: power-of-two **and aligned**, with the hardware read wrap.
- [ ] Re-arm both read address **and** transfer count when re-arming a chained
      DMA channel from its IRQ.
- [ ] Audio IRQ priority above default (`0x40`); nothing else may run long at
      default priority.
- [ ] Every function on the audio IRQ path `__not_in_flash_func`.
- [ ] Verify PSRAM with a real round trip before handing it to an allocator.
- [ ] Re-apply the SPI baud rate, the PSRAM QMI timing, the CYW43 divider and
      the audio carrier after **any** change to `clk_sys` (§3).
- [ ] Remap y through the vertical-scroll offset in *every* blit path, or do not
      use hardware scroll at all.
- [ ] Over-mark dirty regions; snapshot and clear before sending.
- [ ] No flash writes inside a frame loop; `multicore_lockout` if core 1 runs.
- [ ] Poll the keyboard from the frame loop, not a timer IRQ; guard the I²C bus
      with a busy flag; build held-key state from press/release events.
- [ ] Budget SRAM up front: one full-frame canvas is most of what you have.
- [ ] Profile in the mode you ship, with a control quantity, expecting 2 %
      spread, on one board.

---

## 11. Where to look next

**Upstream hardware.** The PicoCalc's schematic, and the keyboard MCU's firmware
as an Arduino sketch, are in ClockworkPi's repository:
<https://github.com/clockworkpi/PicoCalc>. The keyboard sketch is the authority
on which key combinations exist at all.

**One implementation of each recipe above**, in this repository:

| Subject | File |
|---|---|
| LCD driver, DMA blit pipeline, hardware scroll | [devices/picocalc/lcd.c](../devices/picocalc/lcd.c) |
| Framebuffer, per-scanline compositor, dirty presentation | [devices/picocalc/screen.c](../devices/picocalc/screen.c) |
| Dirty-tile tracker (portable, unit-tested) | [devices/picocalc/dirty_tiles.c](../devices/picocalc/dirty_tiles.c) |
| PSG synth: PWM + chained DMA + mixer | [devices/picocalc/sound.c](../devices/picocalc/sound.c) |
| Southbridge I²C register interface | [devices/picocalc/southbridge.c](../devices/picocalc/southbridge.c) |
| Keyboard polling, buffering, modifiers, key-state bitmaps | [devices/picocalc/keyboard.c](../devices/picocalc/keyboard.c) |
| SD card and FAT32 | [devices/picocalc/sdcard.c](../devices/picocalc/sdcard.c), [fat32.c](../devices/picocalc/fat32.c) |
| Flash-safe write recipe | [devices/picocalc/picocalc_flash.c](../devices/picocalc/picocalc_flash.c) |
| PSRAM verification and boot order | [devices/picocalc/main.c](../devices/picocalc/main.c) |
| Clock changes, PSRAM retune, temperature, status LED, Wi-Fi | [devices/picocalc/picocalc_hardware.c](../devices/picocalc/picocalc_hardware.c) |
| Per-board build settings | [CMakePresets.json](../CMakePresets.json) |

**Design records with the full measurement history**, if you want the workings
rather than the conclusions:

- [keyboard-firmware-notes.md](keyboard-firmware-notes.md) — the keyboard MCU in
  full: register map, event cadence, keymap, swallowed chords.
- [multi-sprite-design.md](multi-sprite-design.md) — the display pipeline
  rework, a software sprite compositor, and a survey of how period machines did
  sprites in silicon.
- [tilemap-scrolling-design.md](tilemap-scrolling-design.md) — tile storage, a
  row sampler that generates a scrolled background instead of storing it, and
  every present and frame measurement taken.
- [sound-design.md](sound-design.md) — the PSG design, and §12's hardware
  bring-up findings.
- [interpreter-throughput-design.md](interpreter-throughput-design.md) — §11 is
  the XIP-cache residency investigation in full.
- [concurrent-present-design.md](concurrent-present-design.md) — the core 1
  proposal, its arithmetic, and the probe that would settle it.
- [littlefs-filesystem-design.md](littlefs-filesystem-design.md) — the reserved
  flash region and the safe-write path.
- [measurements/](measurements/) — raw runs, including the 2026-08-23 clock
  sweep behind §3.
