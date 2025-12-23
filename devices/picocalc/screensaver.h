//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Screen saver for PicoCalc LCD persistence prevention
//
//  The PicoCalc LCD is susceptible to persistence (burn-in). This module
//  provides a screen saver that cycles palette hues and shades to prevent
//  pixels from staying at the same color for too long.
//

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Screen saver timing constants
#define SCREENSAVER_IDLE_MS     (5 * 60 * 1000)  // 5 minutes before activation
#define SCREENSAVER_CYCLE_MS    (5 * 1000)       // 5 seconds between palette cycles

// Palette cycling constants
// The palette uses 4-bit hue (0-15) and 4-bit luminance (0-15)
// Default 128-color palette uses only odd luminance values (bit 0 = 1)
// 16 total hues: hue 0 = greyscale; hues 1-15 are chromatic at degrees 57,39,27,359,311,260,238,223,212,201,179,142,81,68,48
#define SCREENSAVER_NUM_HUES    (16)             // 16 hues in the palette (4-bit)
#define SCREENSAVER_NUM_SHADES  (8)              // 8 shades per hue in default palette
#define SCREENSAVER_MAX_SHADE   (8)              // Use shades 0-7 for 50% brightness (4-bit luminance)
#define SCREENSAVER_SHADE_STEP  (3)              // Increment shade by 3 each cycle (0→3→6→1→4→7→2→5→0)
#define SCREENSAVER_SHADE_CYCLES (8)             // 8 shade cycles before incrementing hue

// Initialize the screen saver module
void screensaver_init(void);

// Check if screen saver should activate based on idle time
// Call this from the keyboard wait loop
// Returns true if screen saver is active
bool screensaver_update(void);

// Notify that a key was pressed (resets idle timer and restores palette)
void screensaver_on_key_press(void);

// Check if screen saver is currently active
bool screensaver_is_active(void);
