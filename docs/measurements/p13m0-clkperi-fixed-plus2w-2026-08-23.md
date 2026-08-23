# P13 M0 at 150 and 300 MHz — Plus 2 W, 2026-08-23, clk_peri fixed

The run that confirms `PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1`. The
present is back where it belongs at both clocks, and design §12.3.1's model —
built from the SPI divider the SDK would pick — held on every line.

| | 150 MHz | 300 MHz | predicted at 300 | error |
|---|---:|---:|---:|---:|
| present, `splitscreen` | 19.62 ms | **18.70 ms** | 18.0 | 3.9 % |
| body @ 3 objects | 48.5 | **23.6** | 22.6 | 4.3 % |
| frame @ 3 objects | 68.1 | **42.3** | 40.6 | 4.2 % |
| arithmetic statement | 58.5 µs | 27 µs | — | — |
| one box projected | 3.10 ms | 1.56 | — | — |
| horizon, 32 points | 14.315 | 6.97 | — | — |

    150 MHz:  frame = 45.64 + 7.485 n     n=3: 68.1 mean, 71.1 worst
    300 MHz:  frame = 31.40 + 3.635 n     n=3: 42.3 mean, 44.3 worst

**1.610× on the frame.** The parts separate cleanly: the slope is **2.059×**
(pure interpretation), the flat term minus the present is **2.05×**, and the
present itself moves 19.62 → 18.70, which is the CPU share of it and nothing
else. Exactly the split §12.3 predicted before any of this ran.

**Closed** (horizon culled, hot path on globals): **35.0 ms mean and 37.0 worst
at 300 MHz**, against 53.1 / 56.0 at stock.

**Thermals**: 24.3 → 26.9 °C over a 200-frame run at 300 MHz.

## Two things to note

**The 150 MHz baseline drifts between runs.** Frame at three objects has read
65.5, 68.8 and 68.1 across three sittings on this board, and the arithmetic
statement 52.5, 53.5 and 58.5 µs — about ±2.5 % on the frame and ±5 % on the
statement. Figures quoted to 0.1 ms should be read against that. The 300 MHz
run is the more consistent of the two.

**`fullscreen` reads 24.85 ms at both clocks**, which is the same figure to
three digits and is not what the model expects (25.4 at 150, 24.1 at 300 — both
inside 3 %, so it is coincidence rather than a pinned number). It remains the
noisiest figure in the harness, it is Q2's control only, and the game never
enters that mode. Read the split figure.
