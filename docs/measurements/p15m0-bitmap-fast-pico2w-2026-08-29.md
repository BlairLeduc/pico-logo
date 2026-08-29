# P15 M0, the bitmap design at 300 MHz — Pico 2 W, 2026-08-29

**The gate reading for the port as it now stands**: figures are the cabinet's
sprites, stamped, erased in place (§7). 60 frames a point.

> ### The gate fails: **77.35 ms against 50**. That is 12.9 fps.
>
>     300 MHz:  frame = 16.61 + 5.52 n     n=11: 77.35 mean, 80 worst
>                                          body slope 5.09, present slope 0.43

Down from **106.30** on the pen path — a 27 % cut — and the third independent
reading of the same number: 77.43 with vector-sourced costumes, 78.60 projected
from 150 MHz with bitmap ones, **77.35** measured here. The design is stable
under measurement.

## Q3 is sane again

    splitscreen 18.60 ms   (P13 M0 measured 18.70)
    fullscreen  25.25 ms
    the split saving is 6.65 ms   (§4 says 4.7)

The regression that made the previous run report a *negative* split saving is
fixed. Note the two presents are now different things and the report says so:
**18.60 ms is a full-canvas present** (§15.1's unit), and **7.03 ms is what the
game actually pays**, because erase-in-place only dirties the figures.

**§4's 4.7 ms is low** — the split is worth 6.65 — and it matters far less than
it used to, since the in-place present is 7.03 either way.

## Where the 77.35 ms is

| | ms | share |
|---|---:|---:|
| logic | 55.25 | **71 %** |
| drawing (erase + stamp) | 15.02 | 19 % |
| present, in place | 7.03 | 9 % |

    logic slope    4.43 ms a robot     intercept  6.54
    drawing slope  0.65               intercept  7.83
    present slope  0.43               intercept  2.32

**§15.2's drawing budget is beaten for the first time**: eleven robots cost
**2.97 ms against a predicted 4.6**, and one robot is **0.27 ms**. That is the
only line in that table this port has ever come in under. The 15.02 ms drawing
total is now the walls (3.34), the man, Otto's two `arc`s, seven bolts and the
erase pass — not the robots.

**The present scales now**, 2.75 ms at one robot to 7.03 at eleven, where
clear-and-redraw was flat at ~18.9. That is erase-in-place working exactly as
Q1 said it would.

## What is left, and it is all logic

| | est. saving a robot |
|---|---:|
| `iq` probes only on a cell crossing (§6.3, §15.4) | ~0.95 ms |
| the pair loop gated on *live* bolts, not all seven | ~0.40 |
| `fires?` only for robots the difficulty lets shoot | ~0.29 |

That takes the logic slope from **4.43 to ~2.79** and the frame to **~59 ms**.
Caching Otto as a seventh costume (he is two `arc`s in the drawing intercept)
and gating the bolts to the two or three usually live should reach **~55 ms**.

**All three are things M3 implements anyway.** They cannot be done in the
harness, because they are properties of a game that does not exist yet — a
robot only re-probes on a cell crossing if it has a previous cell to compare
against.

## The decision the gate leaves

At the projected **~3.87 ms a robot** and a **~14 ms floor**:

| | robots inside 50 ms | frame at 11 robots |
|---|---:|---:|
| as measured today | 6.0 | 77.35 ms (12.9 fps) |
| with the three logic savings | **9.3** | **~55 ms (18 fps)** |

So the choice is **nine robots at 20 fps** or **eleven at 18**. §15.3 picked
50 ms / 20 fps by argument — *"a dodging game degrades faster than a driving
one"* — and not by measurement, so restating it is as legitimate as cutting the
count. Both are §17 decisions and neither is an engineering problem.

## Everything else

| | §15.2 | measured |
|---|---:|---:|
| collision pair loop, 7 × 11 | 4.0 ms | 7.24 |
| border + eight interior walls | 0.8 | 3.34 |
| one room generated | — | 7.37 (once a room) |
| `.setitem`, eleven-element list | — | 33 µs |
| `item`, eleven-element list | ~16 µs (at 150) | 11 µs |

Calibration: arithmetic statement **22.5 µs** against §15.1's 24, bare `repeat`
**2.0** against 2.4 — the unit table is right and slightly pessimistic.

Building the six costumes: **107 ms**, against ~111 projected from the 150 MHz
run. §18's slot-swap escape is still ~18 ms a costume.

Frame allocation **0 cells and 0 word bytes** warm. Die 38.3 → 38.8 °C.
The phase timers are back to small and positive (0.03–0.33 ms).

## M0's six questions are all answered

§19's M0 asked six things and this run closes the last of them — both erase
strategies, `c + m·n` at 1/4/8/11, the split present as §15.1's control, the
pair loop alone, `.setitem` priced, and both clocks.

**One board is the whole reading, decided 2026-08-29.** Item 6 originally
wanted all three. The frame is 71 % interpretation and interpretation speed is
a property of the RP2350 core that all three carry identically; the radio this
game never touches and PSRAM nothing in a frame reaches, and the present is the
SPI wire to the same panel. A second board would re-read these numbers.

What one board does *not* settle is §18's ceilings — those are per-board heap
(B44: 56,644 / 40,832 / 47,804 bytes) and decide whether the game loads, not
what a frame costs. M6 checks that against a game that exists.
