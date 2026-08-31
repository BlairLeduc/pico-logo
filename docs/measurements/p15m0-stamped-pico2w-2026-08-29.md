# P15 M0 Q7 — the stamped figures, Pico 2 W at 300 MHz, 2026-08-29

The run that prices §22 Q6. Same board, same clock, same 60 frames a point as
[`p15m0-fast-pico2w-2026-08-29.md`](p15m0-fast-pico2w-2026-08-29.md), and every
figure that appears in both agrees to under 1 % — the pen frame at eleven
robots reads 106.30 against 106.0, the pair loop 7.26 against 7.24, one robot
drawn 2.00 against 2.01. So the two tables can be read against each other.

> ### One robot: **0.26 ms stamped against 2.00 ms drawn — 7.7×.**
> ### Best frame: **77.43 ms**, stamped with erase-in-place, against **106.30**.

## The four configurations at eleven robots

| | body | present | **frame** |
|---|---:|---:|---:|
| pen, clear + redraw | 83.65 | 18.53 | **102.18** |
| pen, erase in place | 104.00 | 6.35 | 110.35 |
| stamped, clear + redraw | 66.25 | 18.90 | 85.15 |
| **stamped, erase in place** | **69.90** | **7.53** | **77.43** |

**Stamping inverts the erase decision.** §3 chose clear-and-redraw, and the
300 MHz pen run put the crossover at about six robots. With the figures stamped
**erase-in-place wins at every count measured** — by 13.9 ms at four robots and
by 7.7 at eleven — because in-place's cost is a second *drawing* pass and
drawing is what stamping removes. §3's question is now settled the other way,
and it took two measurements to get there.

Building the six costumes costs **16 ms, once, at startup**.

## Where the remaining time is

    stamped in-place @ 11:  77.43 ms  =  logic 55.50 + draw/erase 14.40 + present 7.53

**The logic is now 72 % of the frame.** Drawing has gone from 32.02 ms to
14.40 including the eraser, which is §15.2's 6.8 ms estimate finally within
reach of a real number — and the item that was 4.7× low is no longer the
problem. Everything left is interpretation of the AI.

| | §15.2 | pen (measured) | stamped (measured) |
|---|---:|---:|---:|
| logic | 17.1 | 55.50 | 55.50 |
| drawing | 6.8 | 32.02 | **14.40** |
| present | 14.0 | 18.53 | **7.53** |
| **frame** | **38** | **106.30** | **77.43** |

## The gate: still over, but 1.55× rather than 2.13×

50 ms is the gate. 77.43 is **1.55×** it, down from 2.13×. The four design-level
logic savings that are already identified are all in the 55.5 ms half:

| | est. saving a robot |
|---|---:|
| `iq` probes only on a cell crossing (§6.3, §15.4) — a robot moves 2 steps inside a 48 × 68 cell | ~0.95 ms |
| the pair loop gated on *live* bolts rather than all seven | ~0.40 |
| `fires?` only for robots the difficulty lets shoot | ~0.29 |

That takes the logic slope from **4.46 to ~2.82 ms a robot** and the frame to
**~59 ms**. Caching Otto as a seventh costume (he is two `arc`s in the frame
today) and gating the bolts should reach **~55 ms**.

**So the honest projection is 55–59 ms against a 50 ms gate: 17–18 fps.** Not
inside it, and no longer a different order of magnitude. Two things would close
it and both are scope rather than engineering: **nine robots instead of eleven**
(−5.6 ms at the reduced slope), or **restating the gate at 55 ms / 18 fps**,
which §15.3 picked as 50/20 by argument rather than measurement.

## Notes

**§4's split-screen saving is also low.** It says the split is worth 4.7 ms;
measured here at **7.0** (fullscreen 25.25 against splitscreen 18.25). And the
present under erase-in-place is **7.53 ms**, so the split-versus-full question
matters far less than the clear-versus-in-place one.

**The stamped in-place present (7.53) is slightly *higher* than the pen
in-place present (6.35).** The eraser is an 8-wide pen stroke and §7.1's round
caps spill, so it dirties a marginally larger rectangle than eleven thin
strokes do. 1.2 ms for a mechanism that saves 19, and it is the measured cost
of the one thing stamping genuinely gives up.

**Every unit still checks.** Arithmetic statement 24 µs against 24 predicted,
bare `repeat` 2.5 against 2.4, `.setitem` 33 µs, `item` 8.5. The frame spends
0 cells and 0 word bytes warm. Die 39.7 → 39.7 °C.

**What the harness still flatters.** It drops 19 `.setitem` calls a frame to
hold the scene still (0.63 ms), always runs seven bolts, and its logic is a
*model* of the game's — the real thing adds scoring, the HUD, the difficulty
tables, Otto's timer, the death states and room transitions. Read 77.43 as a
floor, not a frame.
