//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Costume pool for the multi-sprite turtles (design doc §4.2).
//
//  A costume is the one and only form a turtle shape takes: 8-bit
//  palette indexed pixels, row-major, with LOGO_SHAPE_TRANSPARENT
//  showing the background and LOGO_SHAPE_PEN the wearing turtle's pen
//  colour. Sizes range from 8x8 to 32x32, any rectangle. Slots 1-15 hold
//  one each, whether it was written by hand with putsh or captured off
//  the canvas with snapsh -- there is no second store and no second
//  format, so nothing shadows anything.
//
//  Pixel data is allocated from a fixed pool; replacing or deleting a
//  costume frees its block and the pool compacts, so fragmentation never
//  wastes space. A full pool makes costume_put fail (the caller reports
//  ERR_OUT_OF_SPACE).
//
//  This module is hardware-free so it can be unit-tested on the host;
//  core/limits.h, its only include, is a plain header of constants.
//

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "core/limits.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define COSTUME_SLOTS 16        // slots 1-15 usable; 0 is the vector turtle
#define COSTUME_POOL_SIZE 8192  // SRAM tier (design doc §10)

// Reset the pool: all slots empty
void costumes_clear(void);

// Store a colour costume in a slot (1-15), replacing any previous one.
// Returns false if the slot or dimensions are out of range or the pool
// cannot hold the pixels.
bool costume_put(uint8_t slot, uint8_t w, uint8_t h, const uint8_t *pixels);

// Fetch a colour costume. Returns false if the slot holds none.
// The pixel pointer stays valid until the next put/delete on any slot.
bool costume_get(uint8_t slot, uint8_t *w, uint8_t *h, const uint8_t **pixels);

// Remove a colour costume (no-op if the slot holds none)
void costume_delete(uint8_t slot);

// Bytes still available in the pool
int costume_pool_free(void);

#ifdef __cplusplus
}
#endif
