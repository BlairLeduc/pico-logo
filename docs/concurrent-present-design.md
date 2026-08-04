# Concurrent present on core 1 (design)

Opened 2026-08-04, out of [P9](roadmap.md#p9--tile-maps-and-smooth-scrolling-design-first)
§13.6's budget arithmetic. P9's scrolling half was shelved because a scrolled
frame must re-send the whole screen every frame, and 21–26 ms of present
against a 40 ms frame leaves 14–19 ms of Logo — a quarter of what a shipped
game uses. This note asks whether the second core removes that constraint,
and proposes measuring the answer before building anything.

**Status: nothing built. §6 is a measurement, and it is the whole of the
current proposal.**

## 1. Goal

Make a frame cost `max(body, present)` instead of `body + present`, by moving
the blit to core 1.

**Non-goal: making the interpreter faster.** Logo's semantics are
single-threaded and the heap is shared mutable state; nothing in this note
touches `core/`'s evaluator, and P10 §11.6 already recorded that the
interpreter's own lever is spent. This is about the ~85 % of a present that
core 0 currently spends asleep.

## 2. The evidence: core 0 sleeps through the present

Two facts, both checked in the tree on 2026-08-04.

**Core 1 has never been started.** `pico/multicore.h` is included by
`devices/picocalc/lcd.c` and `devices/picocalc/southbridge.c`, but no
multicore function is called anywhere in `core/` or `devices/` — no
`multicore_launch_core1`, no lockout, nothing. The core is idle from boot.

**The blit already overlaps everything it can, and then blocks.**
`lcd_blit_row` (`devices/picocalc/lcd.c`) expands row *N* into one of two line
buffers while the DMA ships row *N−1*, then calls
`dma_channel_wait_for_finish_blocking`. So palette expansion is already hidden
behind the wire; what is left is genuinely just waiting on SPI.

The arithmetic agrees. A full screen is 102,400 px at 16 bpp over a 75 MHz
SPI: **21.9 ms of wire** against the **25.6 ms** present P9 M0 measured
(§3.3 there). The residue is `compose_row`'s canvas `memcpy`, palette
expansion, and twenty separate blit windows. **About 85 % of a full-screen
present is core 0 blocked in a DMA wait** — roughly 22 ms per scrolled frame,
and that is the entire prize.

## 3. What it buys — and what it does not

| | serial (today) | present on core 1 |
|---|---|---|
| frame cost | body + present | max(body, present) |
| scrolling, full screen (present 25.6) | body ≤ **14.4 ms** | body ≤ **40 ms** |
| scrolling, road view (present 21.1) | body ≤ **18.9 ms** | body ≤ **40 ms** |

That is ~2.6× on the scrolling budget, which is larger than everything P10
delivered put together (1.72×) and larger than any lever P9 §15 lists.

**But it does almost nothing for the games that exist.** Turtle Trails
presents only its dirty sprite tiles: **2.4 ms** of a 42.55 ms frame
(§13.6 there). Core 1 would reclaim about 2 ms — **5 %**, which is not worth a
concurrency model. Checkpoint Run is the same shape.

So the honest summary is: **this does not improve what we have. It changes
what can be built.** The value is entirely conditional on wanting a scrolling
game, and it should not be started for any other reason.

## 4. Why the usual memory objection does not apply

The textbook way to blit concurrently is to double-buffer the canvas. That is
another `SCREEN_WIDTH * SCREEN_HEIGHT` = **102,400 B**, against **~44 KB** free
on `pico+2w` and **~52 KB** on `pico2w`. It does not fit, and on this project
SRAM is the resource that panics `repl_init`. If a second canvas were required
this design would be over.

It is not required, because **a scrolled background is generated, not stored.**
That is P9 §2's founding insight: when a map view is active the row's source is
the tile bank sampled at `(scroll_x + x, scroll_y + y)`, not a `memcpy` from
`gfx_buffer`. The picture is never held anywhere, so there is no buffer to
double.

What core 1 would actually need to reach:

| Data | Size | Contract |
|---|---:|---|
| the map | 4 KB (SRAM tier) | read-only for the duration of a frame |
| the tile bank | ≤ 4 KB SRAM / 64 KB PSRAM | read-only for the duration of a frame |
| a sprite snapshot | ~15 entries, a few hundred bytes | copied at frame start |
| `compose_buf`, `lcd_dma_line[2]` | 320 B + 1,280 B | move to core 1's exclusive ownership |

So the synchronisation problem shrinks from *double-buffer 100 KB* to **"the
map and bank must not be written while a present is in flight, and the sprite
table is snapshotted."** `settile` and `stamptile` become the two primitives
that must interlock with a present; everything else is already read-only
during a blit.

One inherited piece of luck: **the LCD DMA is polled, not interrupt-driven**
(`dma_channel_wait_for_finish_blocking`), while the P8 sound synth takes
`DMA_IRQ_0` through `irq_add_shared_handler` on whichever core installs it —
core 0. So moving the blit to core 1 does not move an interrupt, does not
contend for `DMA_IRQ_0`, and leaves the audio path exactly where it is. The
sound handler is already `__not_in_flash_func`.

## 5. Ownership model (sketch, deliberately unsettled)

Not proposed for decision — recorded so §6's probe is understood as *not*
depending on it.

- Core 1 owns the LCD: `lcd_blit_*`, `compose_row`, `compose_buf`,
  `lcd_dma_line`. Core 0 stops calling them.
- `sync` becomes: snapshot the sprite table, hand core 1 a frame token, return.
  The *next* `sync` waits for the previous present to retire before
  snapshotting again — so the interpreter blocks only if it outruns the
  display, which is the behaviour `(setrefresh "sync 25)` already documents.
- `settile`/`stamptile`/`newmap`/`newtiles` and canvas drawing block until the
  in-flight present retires. Coarse, and correct; a finer rule needs numbers.
- Flash writes (`load`, `save`, the editor) need `multicore_lockout` around
  them, because the RP2350 cannot execute from flash while flash is being
  programmed.

## 6. M0 — the contention probe, and the gate

**This is the only thing proposed for now.** The prize is known (§3); what is
unknown is the price, and it is not a design question but a measurement.

Two effects could eat the win, and neither can be reasoned about from here:

1. **XIP cache contention.** Both cores fetch instructions from flash through
   one 16 KB cache. P10 §11.2–§11.6 established that this project is *acutely*
   sensitive to that — its entire 1.72 × came from moving 13.6 KB of evaluator
   out of flash. A second core fetching through the same cache could quietly
   tax core 0's interpreter.
2. **SRAM bus contention.** Core 1 reading map and bank while core 0 runs an
   interpreter whose hot path is now also in SRAM.

**The probe needs none of §4 or §5.** Correctness and tearing are irrelevant,
because the question is only what happens to *core 0*:

- Core 1 runs a bare loop: `lcd_blit_begin`, 320 × `lcd_blit_row` over a
  static buffer, `lcd_blit_end`, repeat. No map sampling, no sprite
  compositing, no handoff, no locking. It exists to saturate the LCD and the
  buses.
- Core 0 runs `p9m0.trails` exactly as it does today.
- **Compare core 0's simulation and drawing totals** — 25.80 ms and 13.45 ms,
  body 39.25 (§13.7) — against the same figures with core 1 running. The
  present column is meaningless here, since core 1 owns the LCD; ignore it.
- Also time core 1's own blit loop, to see whether contention slows the
  *present* as well as the body.

Both halves are RAM-resident already (`lcd_blit_row` is `__not_in_flash_func`),
so this probes the interpreter's flash sensitivity, which is the case that
matters.

**The gate.** Express the result as the body budget a scrolling game would
get, `40 ms ÷ (1 + inflation)`, against the **14.4 ms** it gets serially:

| core 0 body inflation | scrolling budget | verdict |
|---|---:|---|
| 0 % | 40 ms | 2.8× — build it |
| 20 % | 33 ms | 2.3× — build it |
| 50 % | 27 ms | 1.9× — still worth it |
| ≥ 100 % | ≤ 20 ms | the cores are fighting; abandon |

**Proceed if inflation is under 50 %.** The gate is deliberately generous
because the serial budget is so small that even heavy contention triples it;
what the probe is really looking for is *pathology* — a result that says the
two cores cannot share this chip — not a fine margin. A result over 100 %
kills the design, and that is worth knowing for one throwaway harness.

## 7. Risks beyond the gate

- **Testability.** The mock device has no concurrency model, and this project's
  rule is to write a test rather than debug on hardware. A race in the
  present handoff is the first defect class here that the mock cannot reach —
  the same gap B11 exposed in device code, but harder. §10.
- **Flash writes.** `multicore_lockout` around every flash program is easy to
  get right once and easy to forget when a new writer appears.
- **The PSRAM finding.** §13.2 records that the tile pools land in PSRAM on
  every board measured, so core 1's sampler would read from PSRAM per row per
  frame. That cost is unmeasured and is P9 M4's first measurement regardless of
  which core runs it; it is not caused by this design, but it compounds with it.
- **A second core is not free to reason about.** Every future device change
  acquires a "which core?" question.

## 8. Milestones

- **M0 — contention probe and gate (§6).** A throwaway harness. Nothing else
  starts until this returns a number.
- **M1 — ownership and handoff:** core 1 owns the LCD, `sync` snapshots and
  hands off, writers interlock (§5). Measured against today's games, which
  should be *unchanged* — a 2 ms win and no regression is the pass.
- **M2 — scrolled compose on core 1:** P9 M4's live view, with the row source
  switched to the map sampler. This is where P9 and this design merge.
- **M3 — a game that scrolls,** which is P9's open design question and the
  only reason any of this exists.

M1 is worth building on its own only if M0 is very good; otherwise M1 and M2
should land together, since M1 alone buys 2 ms.

## 9. Rejected alternatives

| Alternative | Why not |
|---|---|
| Double-buffer the canvas | 102,400 B against ~44 KB free; SRAM is what panics `repl_init`. §4. |
| Async present via a DMA interrupt on core 0 | Palette expansion is per row, so this is ~320 interrupts a frame at ~70 µs spacing, and the same shared-state problem as core 1 with a harder state machine. Core 1 spinning is simpler than an ISR. |
| Run the interpreter on core 1 | Shared mutable heap, dynamically scoped variables, and single-threaded Logo semantics. Not a concurrency problem this language has. |
| Move the sound synth to core 1 instead | Sound is ~nothing per frame and already an IRQ; the present is the 22 ms. |
| PIO for palette expansion | Expansion is already hidden behind the DMA (§2); it is not on the critical path. |
| Raise `LCD_BAUDRATE` | The wire is 21.9 ms of a measured 25.6; a faster clock helps the present but is an unrelated (and panel-limited) change. Worth its own note, not this one. |

## 10. Tests

- **Native:** the ownership rules are testable without concurrency if the
  handoff is a small state machine — snapshot taken, present in flight, writer
  blocked, present retired. Model it as one, and test that on the host.
- **Mock:** extend the mock device with a "present in flight" flag so
  `settile`-during-present is a native test rather than a hardware race.
- **Hardware:** M0's probe; then a soak with the sound synth running, since
  audio integrity under sustained presenting is already a P9 M0 check that
  passed (§3.3) and must keep passing with the blit on the other core.

## References

- [P9 tile map design](tilemap-scrolling-design.md) §2 (the picture is
  generated, not stored), §3.3 (present costs), §13.6–§13.7 (the budget this
  note attacks).
- [P10 interpreter throughput](interpreter-throughput-design.md) §11.2–§11.6 —
  the XIP-cache sensitivity that §6's gate is measuring.
- [Multi-sprite design](multi-sprite-design.md) §2.4 — `compose_row` and the
  per-scanline overlay core 1 would inherit.
- `devices/picocalc/lcd.c` (`lcd_blit_row`, the DMA wait, `LCD_BAUDRATE`),
  `devices/picocalc/screen.c` (`compose_row`, `gfx_buffer`),
  `devices/picocalc/sound.c` (`DMA_IRQ_0` shared handler, core 0).
