//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#pragma once

#include <stdint.h>

#define GLYPH_HEIGHT 10 // Height of each glyph in pixels

typedef struct {
    uint8_t width;
    uint8_t glyphs[1280];
} font_t;

extern const font_t font_8x10; // 8x10 pixel font