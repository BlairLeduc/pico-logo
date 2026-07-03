//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Hybrid randomness implementation. See random.h for the design.
//

#include "random.h"

static bool seeded_mode = false;
static uint64_t pcg_state;

// Minimal PCG32 (O'Neill, pcg32_random_r with a fixed stream constant).
// Chosen for excellent statistical quality at ~10 lines and only 64-bit
// arithmetic, which the RP2350 handles cheaply.
#define PCG_MULT 6364136223846793005ULL
#define PCG_INC 1442695040888963407ULL

static uint32_t pcg32_next(void)
{
    uint64_t old = pcg_state;
    pcg_state = old * PCG_MULT + PCG_INC;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
}

void logo_random_reset(void)
{
    seeded_mode = false;
}

void logo_random_seed(uint32_t seed)
{
    // Standard PCG initialisation: absorb the seed, then advance once so
    // the first output already depends on every seed bit.
    pcg_state = 0;
    (void)pcg32_next();
    pcg_state += seed;
    (void)pcg32_next();
    seeded_mode = true;
}

uint32_t logo_random_next(LogoIO *io)
{
    if (seeded_mode)
    {
        return pcg32_next();
    }
    return logo_io_random(io);
}
