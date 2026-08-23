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

//
//  The text plane.
//
//  lcd.c writes text cells through lcd_putc_attr rather than through the blit
//  pipeline, and scrolls them with the panel's own vertical-scroll region, so
//  the text side needs its own recording to be visible to a test at all.
//  Cells are addressed in LCD text rows (0-31), which is what screen.c has to
//  get right in split mode.
//

// Character last written to an LCD text cell, or 0 if nothing was written
// there since the reset. Outside the plane reads as 0.
uint8_t fake_lcd_text_char(int column, int row);

// LCD text row/column the cursor was last moved to.
int fake_lcd_cursor_row(void);
int fake_lcd_cursor_column(void);

// How many times the panel's scrolling region has been scrolled or cleared.
int fake_lcd_scroll_up_count(void);
int fake_lcd_scroll_clear_count(void);
