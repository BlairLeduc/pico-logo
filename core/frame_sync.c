//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Frame-cadence pacing for setrefresh "sync. See frame_sync.h.
//

#include "frame_sync.h"

static bool s_active = false;
static uint32_t s_period_ms = 0;
static uint32_t s_deadline = 0;      // next frame boundary, in clock ms
static bool s_have_deadline = false; // false until the baseline is seeded

void frame_sync_set(bool active, uint32_t period_ms)
{
    s_active = active;
    s_period_ms = active ? period_ms : 0;
    s_have_deadline = false; // re-seed the cadence on the next wait
}

bool frame_sync_active(void)
{
    return s_active;
}

uint32_t frame_sync_period(void)
{
    return s_period_ms;
}

void frame_sync_reset(void)
{
    s_active = false;
    s_period_ms = 0;
    s_deadline = 0;
    s_have_deadline = false;
}

uint32_t frame_sync_wait_ms(uint32_t now)
{
    if (!s_active || s_period_ms == 0)
    {
        return 0;
    }

    if (!s_have_deadline)
    {
        // Seed the cadence: the first boundary sits one period out from now.
        s_deadline = now;
        s_have_deadline = true;
    }

    s_deadline += s_period_ms;

    // Signed difference so the comparison survives a 32-bit clock wrap.
    int32_t remaining = (int32_t)(s_deadline - now);
    if (remaining < 0)
    {
        // The frame overran a full period. Drop the accumulated slippage and
        // restart the cadence from now, so we neither sleep nor chase the lost
        // time with a run of zero-length frames.
        s_deadline = now;
        return 0;
    }

    return (uint32_t)remaining;
}
