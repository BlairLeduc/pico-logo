//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Host-side stand-in for devices/picocalc/lcd.c, so tests can compile
//  devices/picocalc/screen.c natively and see which side of the buffer a
//  write landed on: the canvas, or the panel.
//
//  The "panel" is what the LCD is showing. Only lcd_clear_screen,
//  lcd_solid_rectangle and the row-fed blit put pixels there.
//

#pragma once

#include <stdint.h>

// Reset the panel to FAKE_LCD_UNWRITTEN, zero the counters, reset the clock.
void fake_lcd_reset(void);

// Palette index the panel holds where nothing has been drawn.
#define FAKE_LCD_UNWRITTEN (77)

// Read the panel back. Outside the panel reads as FAKE_LCD_UNWRITTEN.
uint8_t fake_lcd_panel_point(int x, int y);

// Calls that wrote the panel without going through the blit pipeline.
int fake_lcd_clear_count(void);      // lcd_clear_screen
int fake_lcd_rectangle_count(void);  // lcd_solid_rectangle
int fake_lcd_blit_row_count(void);   // rows streamed by lcd_blit_row

// Drive the clock screen_gfx_update()'s rate limiter reads.
void fake_lcd_advance_us(uint64_t us);
