//
//  Host-side stub for <hardware/clocks.h>.  The host reports the board's
//  default 150 MHz system clock so the mixer derives the same block size a
//  board does.  See mocks/hardware/pwm.h.
//
#pragma once

#include <stdint.h>

typedef enum { clk_sys = 0 } clock_num_t;

static inline uint32_t clock_get_hz(clock_num_t c) { (void)c; return 150000000u; }
