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

// The text plane, in LCD text cells, plus the state lcd.c keeps around it.
static uint8_t txt_cells[COLUMNS * ROWS];
static int cursor_col_at, cursor_row_at;
static int scroll_up_count, scroll_clear_count;
static int scroll_top_row, scroll_bottom_row; // [top, bottom) in text rows

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
    memset(txt_cells, 0, sizeof(txt_cells));
    cursor_col_at = cursor_row_at = 0;
    scroll_up_count = scroll_clear_count = 0;
    scroll_top_row = 0;
    scroll_bottom_row = ROWS;
}

uint8_t fake_lcd_text_char(int column, int row)
{
    if (column < 0 || column >= COLUMNS || row < 0 || row >= ROWS)
    {
        return 0;
    }
    return txt_cells[row * COLUMNS + column];
}

int fake_lcd_cursor_row(void) { return cursor_row_at; }
int fake_lcd_cursor_column(void) { return cursor_col_at; }
int fake_lcd_scroll_up_count(void) { return scroll_up_count; }
int fake_lcd_scroll_clear_count(void) { return scroll_clear_count; }

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
void lcd_putc_attr(uint8_t column, uint8_t row, uint16_t packed)
{
    if (column < COLUMNS && row < ROWS)
    {
        // The low byte of a packed cell is the character; see TXT_PACK.
        txt_cells[row * COLUMNS + column] = (uint8_t)(packed & 0xFF);
    }
}

// The fixed areas are in pixels and bound a scrolling region between them.
void lcd_define_scrolling(uint16_t top_fixed_area, uint16_t bottom_fixed_area)
{
    scroll_top_row = top_fixed_area / GLYPH_HEIGHT;
    scroll_bottom_row = (HEIGHT - bottom_fixed_area) / GLYPH_HEIGHT;
}

void lcd_scroll_clear(uint8_t bg_colour)
{
    (void)bg_colour;
    scroll_clear_count++;
    for (int r = scroll_top_row; r < scroll_bottom_row; r++)
    {
        memset(&txt_cells[r * COLUMNS], 0, COLUMNS);
    }
}

// The real one moves the panel's scroll offset; what a caller sees afterwards
// is the region's rows shifted up by one with the last row blank, so that is
// what this records.
void lcd_scroll_up(uint8_t bg_colour)
{
    (void)bg_colour;
    scroll_up_count++;
    for (int r = scroll_top_row; r + 1 < scroll_bottom_row; r++)
    {
        memcpy(&txt_cells[r * COLUMNS], &txt_cells[(r + 1) * COLUMNS], COLUMNS);
    }
    if (scroll_bottom_row > scroll_top_row)
    {
        memset(&txt_cells[(scroll_bottom_row - 1) * COLUMNS], 0, COLUMNS);
    }
}

void lcd_move_cursor(uint8_t x, uint8_t y) { cursor_col_at = x; cursor_row_at = y; }
void lcd_set_cursor_char(uint16_t packed) { (void)packed; }
void lcd_draw_cursor(void) {}
void lcd_erase_cursor(void) {}
void lcd_enable_cursor(bool cursor_on) { (void)cursor_on; }
bool lcd_cursor_enabled(void) { return false; }
void lcd_init(void) {}
