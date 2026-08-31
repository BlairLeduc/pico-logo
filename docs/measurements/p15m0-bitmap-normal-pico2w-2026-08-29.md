# P15 M0 with the ROM bitmaps — Pico 2 W at 150 MHz, 2026-08-29

The first run of the rebuilt harness: figures are the cabinet's sprites,
stamped, erased in place (§7). Taken at **`normal`**, so it is not the gate;
the harness says so itself and projects the 300 MHz figure.

## The rebuild reproduced the stamped result, which is the main thing

    150 MHz:  frame = 47.9 + 10.60 n     n=11: 153.92 mean, 161 worst
    projected to 300 (body / 2.06, present unscaled): 78.60 ms

Against **77.43 ms** measured directly at 300 with the vector-sourced costumes
([`p15m0-stamped-pico2w-2026-08-29.md`](p15m0-stamped-pico2w-2026-08-29.md)).
The costumes are the same size either way, so the blit is the same and the
1.5 % is scaling error. **Changing the artwork source cost nothing**, which is
what it should have done and is worth having confirmed rather than assumed.

| | measured (150) | projected (300) | direct 300, vector-sourced |
|---|---:|---:|---:|
| logic | 114.28 | 55.5 | 55.50 |
| drawing (erase + stamp) | 30.48 | 14.8 | 14.40 |
| present, in place | 7.55 | 7.55 | 7.53 |
| **frame** | **153.92** | **78.60** | **77.43** |

One robot stamped: **0.58 ms at 150**, which is 0.28 at 300 against the 0.26
measured directly. Consistent.

## Two things this run found, and one of them is mine

### Q3 was measuring the wrong thing, and the board is what noticed

    splitscreen 8.45 ms
    fullscreen  8.00 ms
    the split saving is -0.45 ms

A fullscreen present *below* the splitscreen one is impossible, and the figures
are less than half P13's. **`time.present` had been switched to the in-place
frame along with the series**, and in-place never `clean`s — so it was timing a
handful of dirty rows rather than a canvas. Q3 is the control on §15.1's unit
table and the unit is a **full-canvas** present, which is what P13 measured at
19.62/18.70.

Fixed: `time.present` times `frame.clear` again, and the report now says
explicitly that Q3's figures are full-canvas presents while the game's own is
the in-place one in the series. `test_the_present_control_times_a_full_canvas`
is the guard — the mock does not model dirty regions, but it can see whether
the timed frame redrew the walls, and the in-place frame never does. It fails
on the regression.

**Q3's numbers from this run are void.** The full-canvas present at 150 is
P13's 19.62 and this harness's own earlier 21.95; the in-place present, which
is what the game will pay, is **7.55 ms**.

### Building the costumes costs 229 ms, not the ~16 the vector version did

**~38 ms a costume at 150, ~18 at 300 — and §18 said ~2.7.** That estimate came
from the vector-sourced cache (16 ms for six models); rendering a bitmap
pixel by pixel is ~480 statements a sprite against a pen walk's ~45, and the
board priced the difference.

At startup it does not matter: ~111 ms at 300, once, behind an attract screen.

**What it breaks is §18's escape clause.** The fifteen-slot ceiling was going to
be survivable because "a re-`snapsh` costs ~2.7 ms, affordable at a room
transition". At **18 ms a costume** a transition that swaps four is 72 ms on top
of the room generation's own 7.4, which is a visible hitch rather than a frame.
So either the working set fits in fifteen slots, or the renderer gets cheaper —
drawing each row as *runs* of set bits rather than eight pixel steps is roughly
3× and is the obvious lever. **M2 and M3 decide; nothing is built for it now.**

## Everything else

| | §15.2 | measured (150) |
|---|---:|---:|
| collision pair loop, 7 × 11 | 4.0 ms | 15.00 |
| eleven robots stamped | 4.6 | **6.38** |
| border + eight interior walls | 0.8 | 6.92 |
| one room generated | — | 15.26 |
| `.setitem`, eleven-element list | — | 68 µs |
| `item`, eleven-element list | ~16 µs | 19 µs |

Calibration: arithmetic statement **48 µs**, bare `repeat` **5 µs** — both in
P11 M0's 150 MHz ranges (42–44.5 and 4.5–7), so the board is where it was.

**Eleven robots stamped is 6.38 ms against §15.2's 4.6** — the first line in
that table this port has ever come in near. Drawing is no longer the problem.

**The logic is unchanged at 114.28 ms**, as it must be: it is the same code
whatever the figures are made of, and at **74 % of the frame** it is now the
whole problem. §15.4's cell-probe note is the next lever.

Frame allocation **0 cells and 0 word bytes** warm. Die 36.9 → 34.4 °C.

**Cosmetic**: the phase timers read **−1.38 ms** at eleven robots. `frame.split`
mirrors `frame.inplace` and ran marginally faster than it over 60 frames, which
at ~1 % of 150 ms is noise rather than a negative cost. Worth a glance if it
ever grows.

## Next

Re-run at `fast` for the gate proper. The projection says **78.6 ms**, and
§15.4's logic work is what stands between that and 50.
