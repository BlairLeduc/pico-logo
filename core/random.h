//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Hybrid randomness for random / pick / shuffle.
//
//  Default mode draws every value from the device source — the RP2350's
//  hardware TRNG on real boards — preserving true unpredictability.
//  After `rerandom` seeds it, draws come from a deterministic PCG32
//  stream instead, giving reproducible sequences for tests, debugging,
//  and classroom use. Seeded mode lasts until the next boot (or an
//  explicit reset).
//

#pragma once

#include <stdint.h>

#include "devices/io.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Return to default (device/TRNG) mode. Called at interpreter startup.
    void logo_random_reset(void);

    // Enter deterministic mode: subsequent draws follow the reproducible
    // sequence selected by `seed`.
    void logo_random_seed(uint32_t seed);

    // Draw a 32-bit value from the active source.
    uint32_t logo_random_next(LogoIO *io);

#ifdef __cplusplus
}
#endif
