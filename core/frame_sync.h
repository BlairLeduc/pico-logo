//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Frame-cadence pacing for setrefresh "sync (see the `sync` primitive).
//
//  In sync mode the display accumulates drawing (as in manual refresh) and the
//  game loop calls `sync` once per pass: it presents the frame and then blocks
//  until the next fixed period boundary. Pacing to a boundary derived from a
//  fixed baseline -- rather than sleeping a constant amount after variable work
//  -- keeps the update cadence even regardless of how much any one frame does,
//  the way a CRT game waited for vertical blank. This module owns only the
//  cadence arithmetic; presenting and sleeping stay in the primitive.
//

#ifndef FRAME_SYNC_H
#define FRAME_SYNC_H

#include <stdbool.h>
#include <stdint.h>

// Enable or disable sync mode. period_ms is the target frame length in
// milliseconds and must be >= 1 when active; it is ignored when disabling.
// Either call re-seeds the cadence baseline on the next frame_sync_wait_ms().
void frame_sync_set(bool active, uint32_t period_ms);

// True when sync mode is active.
bool frame_sync_active(void);

// The configured frame period in milliseconds (0 when inactive).
uint32_t frame_sync_period(void);

// Return to the inactive state with no cadence baseline. Called when a program
// stops or clears the screen so the prompt is never left paced.
void frame_sync_reset(void);

// Given the current clock reading `now` (milliseconds), return how many
// milliseconds to sleep to land on the next frame boundary, advancing the
// baseline by one period. The first call after activation seeds the baseline
// (so it returns a full period). If the frame overran its budget the slippage
// is dropped -- the cadence re-baselines from `now` and 0 is returned -- so the
// loop degrades to running late rather than firing a burst of catch-up frames.
// Returns 0 when sync mode is inactive.
uint32_t frame_sync_wait_ms(uint32_t now);

#endif // FRAME_SYNC_H
