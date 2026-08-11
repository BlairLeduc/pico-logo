//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Host-side stand-in for devices/picocalc/lcd.c (see fake_lcd.h).
//

#include <string.h>

#include "fake_lcd.h"
#include "lcd.h"

static uint8_t panel[WIDTH * HEIGHT];
static uint16_t palette[256];
static int clear_count;
static int rectangle_count;
static int blit_row_count;
static uint64_t clock_us;

// Open blit window (lcd_blit_begin .. lcd_blit_end)
static int win_x, win_y, win_w, win_rows;

void fake_lcd_reset(void)
{
    memset(panel, FAKE_LCD_UNWRITTEN, sizeof(panel));
    clear_count = 0;
    rectangle_count = 0;
    blit_row_count = 0;
    clock_us = 0;
    win_x = win_y = win_w = win_rows = 0;
}

uint8_t fake_lcd_panel_point(int x, int y)
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
    {
        return FAKE_LCD_UNWRITTEN;
    }
    return panel[y * WIDTH + x];
}

int fake_lcd_clear_count(void) { return clear_count; }
int fake_lcd_rectangle_count(void) { return rectangle_count; }
int fake_lcd_blit_row_count(void) { return blit_row_count; }

void fake_lcd_advance_us(uint64_t us) { clock_us += us; }

uint64_t time_us_64(void) { return clock_us; }

//
//  Writes that reach the panel
//

void lcd_clear_screen(uint8_t bg_colour)
{
    clear_count++;
    memset(panel, bg_colour, sizeof(panel));
}

void lcd_solid_rectangle(uint8_t colour, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    rectangle_count++;
    for (int row = y; row < y + height && row < HEIGHT; row++)
    {
        for (int col = x; col < x + width && col < WIDTH; col++)
        {
            panel[row * WIDTH + col] = colour;
        }
    }
}

void lcd_blit_begin(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    win_x = x;
    win_y = y;
    win_w = width;
    win_rows = height;
}

void lcd_blit_row(const uint8_t *row)
{
    blit_row_count++;
    if (win_rows <= 0 || win_y >= HEIGHT)
    {
        return;
    }
    for (int i = 0; i < win_w && win_x + i < WIDTH; i++)
    {
        panel[win_y * WIDTH + win_x + i] = row[i];
    }
    win_y++;
    win_rows--;
}

void lcd_blit_end(void) { win_rows = 0; }

//
//  Everything else screen.c calls: enough to link, nothing to observe
//

void lcd_set_palette_value(uint8_t slot, uint16_t colour) { palette[slot] = colour; }
uint16_t lcd_get_palette_value(uint8_t slot) { return palette[slot]; }
void lcd_set_foreground(uint8_t slot) { (void)slot; }
void lcd_set_background(uint8_t slot) { (void)slot; }
void lcd_putc_attr(uint8_t column, uint8_t row, uint16_t packed) { (void)column; (void)row; (void)packed; }
void lcd_define_scrolling(uint16_t top_fixed_area, uint16_t bottom_fixed_area) { (void)top_fixed_area; (void)bottom_fixed_area; }
void lcd_scroll_clear(uint8_t bg_colour) { (void)bg_colour; }
void lcd_scroll_up(uint8_t bg_colour) { (void)bg_colour; }
void lcd_move_cursor(uint8_t x, uint8_t y) { (void)x; (void)y; }
void lcd_set_cursor_char(uint16_t packed) { (void)packed; }
void lcd_draw_cursor(void) {}
void lcd_erase_cursor(void) {}
void lcd_enable_cursor(bool cursor_on) { (void)cursor_on; }
bool lcd_cursor_enabled(void) { return false; }
void lcd_init(void) {}
