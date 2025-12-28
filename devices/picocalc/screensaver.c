//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Screen saver for PicoCalc LCD persistence prevention
//

#include <string.h>

#include "pico/stdlib.h"

#include "screensaver.h"
#include "lcd.h"
#include "screen.h"
#include "devices/palette.h"

// External reference to background colour from picocalc_console.c
extern uint8_t background_colour;

// Palette backup (256 entries of RGB565)
static uint16_t palette_backup[256];

// Screen saver state
static bool screensaver_active = false;
static uint64_t last_activity_us = 0;
static uint64_t last_cycle_us = 0;

// Palette cycling state
static uint8_t shade_offset = 0;      // Current shade offset (0-7)
static uint8_t shade_cycle_count = 0; // How many shade cycles completed (0-7)
static uint8_t hue_offset = 0;        // Current hue offset (0-15)

//
// Initialize the screen saver
//
void screensaver_init(void)
{
    screensaver_active = false;
    last_activity_us = time_us_64();
    last_cycle_us = 0;
    shade_offset = 0;
    shade_cycle_count = 0;
    hue_offset = 0;
}

//
// Backup the current palette
//
static void backup_palette(void)
{
    for (int i = 0; i < 256; i++)
    {
        palette_backup[i] = lcd_get_palette_value(i);
    }
}

//
// Restore the backed-up palette
//
static void restore_palette(void)
{
    for (int i = 0; i < 256; i++)
    {
        lcd_set_palette_value(i, palette_backup[i]);
    }
}

//
// Cycle the palette colors
//
// For each of the 128 LCD palette slots:
//   - Calculate new hue and shade with offsets
//   - Look up the RGB565 value from palette_16bit[(hue << 4) | shade]
//   - Shades are limited to 0-7 for 50% brightness
//
// Upper 126 slots (128-253) are copies of lower 128
// Slots 254/255 are foreground/background derived from background_colour
//
static void cycle_palette(void)
{
    // Update lower 128 palette slots with cycled colors
    for (int i = 0; i < 128; i++)
    {
        // Original position: hue = i / 8, shade_index = i % 8
        int orig_hue = i / SCREENSAVER_NUM_SHADES;
        int orig_shade_index = i % SCREENSAVER_NUM_SHADES;

        // Calculate new hue and shade with offsets
        int new_hue = (orig_hue + hue_offset) % SCREENSAVER_NUM_HUES;
        
        // Apply shade offset within 0-7 range for 50% brightness
        // Cycling by 3: 0→3→6→1→4→7→2→5→0 visits all 8 darker shades
        int new_shade = (orig_shade_index + shade_offset) % SCREENSAVER_NUM_SHADES;

        // Look up RGB565 from palette_16bit using (hue << 4) | shade
        uint16_t rgb565 = palette_16bit[(new_hue << 4) | new_shade];
        
        lcd_set_palette_value(i, rgb565);
    }

    // Copy lower 128 slots to upper slots (128-253)
    for (int i = 128; i < 254; i++)
    {
        lcd_set_palette_value(i, lcd_get_palette_value(i - 128));
    }

    // Handle slots 254 (foreground) and 255 (background) using the same
    // logic as turtle_set_bg_colour() but with cycled values
    
    // Get the cycled background color from the transformed slot
    uint8_t bg_slot = background_colour;
    if (bg_slot < 128)
    {
        // Background slot has already been transformed, just use it
        uint16_t bg_value = lcd_get_palette_value(bg_slot);
        lcd_set_palette_value(255, bg_value);
        
        // Foreground: use contrasting shade (same logic as turtle_set_bg_colour)
        uint8_t fg_slot = bg_slot;
        if ((bg_slot & 0x07) < 4)
        {
            fg_slot = bg_slot | 0x07;  // Use brightest shade in hue
        }
        else
        {
            fg_slot = bg_slot & ~0x07; // Use darkest shade in hue
        }
        uint16_t fg_value = lcd_get_palette_value(fg_slot);
        lcd_set_palette_value(254, fg_value);
    }
    else
    {
        // Background slot is in upper range, use cycled copy
        uint16_t bg_value = lcd_get_palette_value(bg_slot - 128);
        lcd_set_palette_value(255, bg_value);
        
        // Use a contrasting foreground
        uint8_t mapped_slot = bg_slot - 128;
        uint8_t fg_slot = mapped_slot;
        if ((mapped_slot & 0x07) < 4)
        {
            fg_slot = mapped_slot | 0x07;
        }
        else
        {
            fg_slot = mapped_slot & ~0x07;
        }
        uint16_t fg_value = lcd_get_palette_value(fg_slot);
        lcd_set_palette_value(254, fg_value);
    }
}

//
// Refresh the display with the current palette
//
static void refresh_display(void)
{
    // Update both graphics and text areas to reflect palette changes
    uint8_t mode = screen_get_mode();
    
    if (mode == SCREEN_MODE_GFX || mode == SCREEN_MODE_SPLIT)
    {
        screen_gfx_update();
    }
    if (mode == SCREEN_MODE_TXT || mode == SCREEN_MODE_SPLIT)
    {
        screen_txt_update();
    }
}

//
// Advance to the next palette cycle state
//
static void advance_cycle(void)
{
    // Increment shade offset by 3 (mod 8)
    // Cycle: 0→3→6→1→4→7→2→5→0
    shade_offset = (shade_offset + SCREENSAVER_SHADE_STEP) % SCREENSAVER_NUM_SHADES;
    
    // Count each shade cycle
    shade_cycle_count++;
    
    // After 8 shade cycles, increment hue
    if (shade_cycle_count >= SCREENSAVER_SHADE_CYCLES)
    {
        shade_cycle_count = 0;
        hue_offset = (hue_offset + 1) % SCREENSAVER_NUM_HUES;
    }
}

//
// Update screen saver state
// Call this from the keyboard wait loop
// Returns true if screen saver is active
//
bool screensaver_update(void)
{
    uint64_t now_us = time_us_64();
    uint64_t idle_us = now_us - last_activity_us;
    uint64_t idle_ms = idle_us / 1000;

    if (!screensaver_active)
    {
        // Check if we should activate
        if (idle_ms >= SCREENSAVER_IDLE_MS)
        {
            // Activate screen saver
            screensaver_active = true;
            backup_palette();
            last_cycle_us = now_us;
            
            // Initial cycle
            cycle_palette();
            refresh_display();
        }
    }
    else
    {
        // Screen saver is active, check if we should cycle
        uint64_t since_cycle_ms = (now_us - last_cycle_us) / 1000;
        
        if (since_cycle_ms >= SCREENSAVER_CYCLE_MS)
        {
            // Time to cycle the palette
            advance_cycle();
            cycle_palette();
            refresh_display();
            last_cycle_us = now_us;
        }
    }

    return screensaver_active;
}

//
// Handle key press - restore palette and reset state
// Returns true if screensaver was active (display may need full refresh)
//
bool screensaver_on_key_press(void)
{
    last_activity_us = time_us_64();

    if (screensaver_active)
    {
        // Restore the original palette
        restore_palette();
        refresh_display();
        
        // Reset state
        screensaver_active = false;
        shade_offset = 0;
        shade_cycle_count = 0;
        hue_offset = 0;
        
        return true;  // Was active, display needs refresh
    }
    
    return false;  // Was not active
}

//
// Check if screen saver is currently active
//
bool screensaver_is_active(void)
{
    return screensaver_active;
}
