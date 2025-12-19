//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  PicoCalc screen driver
//
//  This driver provides a simple interface to the LCD display on the PicoCalc.
//  It supports full-screen text mode, full-screen graphics mode, and split screen mode.
//  The screen is a 320x320 pixel display with a 5x10 pixel or 8x10 pixel font.
//

#include <errno.h>
#include <math.h>
#include <string.h>

#include <pico/stdlib.h>

#include "screen.h"
#include "fat32.h"
#include "devices/font.h"
#include "devices/logo-font.h"
#include "devices/console.h"
#include "lcd.h"

//
//  Frame buffers for the screen
//

//  The GFX frame buffer: each pixel is 8-bits (points into a palette)
static uint8_t gfx_buffer[SCREEN_WIDTH * SCREEN_HEIGHT] = {0};

//  The text frame buffer: each character is 8-bits (ASCII code)
static uint8_t txt_buffer[SCREEN_COLUMNS * SCREEN_ROWS] = {0};

// The screen can be in one of three modes:
//
// 1. Text screen
//    This mode uses the LCD's text capabilities and does not use the frame buffer.
//
// 2. Fullscreen (graphics)
//    This mode uses the frame buffer to display graphics on the entire screen.
//
// 3. Split screen with graphics on top and text on bottom
//    This mode uses the top 240 lines for graphics and the bottom 80 lines for text.
//

static uint8_t screen_mode = SCREEN_MODE_TXT;

// Text state
static const font_t *screen_font = &logo_font;       // Default font for text mode
static uint16_t text_row = 0;                        // The last row written to in text mode
static uint16_t foreground = TXT_DEFAULT_FOREGROUND; // Default foreground colour (white)
static uint16_t background = TXT_DEFAULT_BACKGROUND; // Default background colour (black)
static uint8_t cursor_column = 0;                    // Cursor x position for text mode
static uint8_t cursor_row = 0;                       // Cursor y position for text mode
static bool cursor_enabled = true;                   // Cursor visibility state for text mode

//
//  Helper functions
//

// Wrap and round a floating point value to the nearest pixel in the range [0, max)
static int wrap_and_round(float value, int max)
{
    // Wrap into [0, max)
    value = value - floorf(value / max) * max;
    // Round to nearest integer
    int pixel = (int)(value + 0.5f);
    // Wrap again in case rounding pushed out of bounds
    pixel = ((pixel % max) + max) % max;

    return pixel;
}

// Helper function to scroll the text buffer up one line
static void screen_txt_scroll_up(void)
{
    // Move all rows up by one using memmove (handles overlapping memory correctly)
    memmove(txt_buffer,
            txt_buffer + SCREEN_COLUMNS,
            (SCREEN_ROWS - 1) * SCREEN_COLUMNS * sizeof(uint8_t));

    // Clear the last row using memset
    memset(txt_buffer + (SCREEN_ROWS - 1) * SCREEN_COLUMNS,
           0x20,
           SCREEN_COLUMNS * sizeof(uint8_t));
}

// Get the location for the cursor in TXT or SPLIT mode
// Return true is location visible, false if not
static bool screen_txt_map_location(uint8_t *column, uint8_t *row)
{
    if (screen_mode == SCREEN_MODE_GFX || screen_mode == SCREEN_MODE_TXT)
    {
        // Get the current cursor position in text mode
        if (column)
        {
            *column = cursor_column;
        }

        if (row)
        {
            *row = cursor_row;
        }

        return screen_mode == SCREEN_MODE_TXT;
    }

    // Check if the cursor is within the visible text area
    int16_t start_row = text_row - (SCREEN_SPLIT_TXT_ROWS - 1);
    if (start_row < 0)
    {
        start_row = 0;
    }

    if (cursor_row >= start_row && cursor_row < start_row + SCREEN_SPLIT_TXT_ROWS)
    {
        if (column)
        {
            *column = cursor_column;
        }
        if (row)
        {
            *row = SCREEN_SPLIT_TXT_ROW + cursor_row - start_row;
        }
        return true; // Cursor is visible
    }

    return false; // Cursor is not visible
}

//
//  Screen mode functions
//

uint8_t screen_get_mode()
{
    return screen_mode;
}

void screen_set_mode(uint8_t mode)
{
    if (mode == screen_mode)
    {
        return; // No change
    }

    if (mode == SCREEN_MODE_TXT || mode == SCREEN_MODE_GFX || mode == SCREEN_MODE_SPLIT)
    {
        screen_mode = mode;

        if (mode == SCREEN_MODE_TXT)
        {
            // In text mode, we don't use the frame buffer
            lcd_define_scrolling(0, 0); // All scrolling area in full-screen text mode
            screen_txt_update();
            // lcd_move_cursor(cursor_column, cursor_row); // Move the cursor to the current position
        }
        else if (mode == SCREEN_MODE_GFX)
        {
            // In full-screen graphics mode, we clear the screen
            lcd_erase_cursor();
            lcd_define_scrolling(0, 0); // No scrolling area in full-screen graphics mode
            screen_gfx_update();
        }
        else if (mode == SCREEN_MODE_SPLIT)
        {
            lcd_erase_cursor();
            lcd_define_scrolling(SCREEN_SPLIT_GFX_HEIGHT, 0); // Set scrolling area for text at the bottom
            screen_gfx_update();
            screen_txt_update();
        }
    }
}

void screen_show_field()
{
    if (screen_mode == SCREEN_MODE_GFX || screen_mode == SCREEN_MODE_SPLIT)
    {
        return;
    }
    screen_set_mode(SCREEN_MODE_SPLIT);
}

//
//  Graphics functions
//

// Get the graphics frame buffer
uint8_t *screen_gfx_frame()
{
    return gfx_buffer;
}

// Clear the graphics buffer
void screen_gfx_clear(void)
{
    memset(gfx_buffer, GFX_DEFAULT_BACKGROUND, sizeof(gfx_buffer)); // Clear the graphics buffer

    if (screen_mode == SCREEN_MODE_GFX)
    {
        lcd_clear_screen(); // Clear the LCD screen in graphics mode
    }
    else if (screen_mode == SCREEN_MODE_SPLIT)
    {
        // Clear the graphics area in split mode
        lcd_solid_rectangle(background, 0, 0, SCREEN_WIDTH, SCREEN_SPLIT_GFX_HEIGHT);
    }
}

// Draw a point in the graphics buffer
void screen_gfx_set_point(float x, float y, uint8_t colour)
{
    int pixel_x = wrap_and_round(x, SCREEN_WIDTH);
    int pixel_y = wrap_and_round(y, SCREEN_HEIGHT);

    gfx_buffer[pixel_y * SCREEN_WIDTH + pixel_x] = colour;
}

uint8_t screen_gfx_get_point(float x, float y)
{
    int pixel_x = wrap_and_round(x, SCREEN_WIDTH);
    int pixel_y = wrap_and_round(y, SCREEN_HEIGHT);

    return gfx_buffer[pixel_y * SCREEN_WIDTH + pixel_x];
}

void screen_gfx_reverse_point(float x, float y)
{
    int pixel_x = wrap_and_round(x, SCREEN_WIDTH);
    int pixel_y = wrap_and_round(y, SCREEN_HEIGHT);

    uint8_t *pixel = &gfx_buffer[pixel_y * SCREEN_WIDTH + pixel_x];
    *pixel = (*pixel == GFX_DEFAULT_BACKGROUND) ? foreground : GFX_DEFAULT_BACKGROUND;
}

// Draw a line in the graphics buffer using Bresenham's algorithm
void screen_gfx_line(float x1, float y1, float x2, float y2, uint8_t colour, bool reverse)
{
    // Calculate the number of steps based on the longest axis
    float dx = x2 - x1;
    float dy = y2 - y1;
    int steps = (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)));

    if (steps == 0)
    {
        // Single point
        screen_gfx_set_point(x1, y1, colour);
        return;
    }

    float x_inc = dx / steps;
    float y_inc = dy / steps;

    float x = x1;
    float y = y1;

    if (reverse)
    {
        uint8_t *pixel;

        // XOR mode: invert the pixel colour
        for (int i = 0; i <= steps; ++i)
        {
            int pixel_x = wrap_and_round(x, SCREEN_WIDTH);
            int pixel_y = wrap_and_round(y, SCREEN_HEIGHT);
            
            pixel = &gfx_buffer[pixel_y * SCREEN_WIDTH + pixel_x];
            *pixel = *pixel == GFX_DEFAULT_BACKGROUND ? colour : GFX_DEFAULT_BACKGROUND;

            x += x_inc;
            y += y_inc;
        }
    }
    else
    {
        for (int i = 0; i <= steps; ++i)
        {
            screen_gfx_set_point(x, y, colour);

            x += x_inc;
            y += y_inc;
        }
    }
}

// Write the frame buffer to the LCD display
void screen_gfx_update(void)
{
    if (screen_mode == SCREEN_MODE_GFX)
    {
        lcd_blit(gfx_buffer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
    else if (screen_mode == SCREEN_MODE_SPLIT)
    {
        // Blit the graphics area only
        lcd_blit(gfx_buffer, 0, 0, SCREEN_WIDTH, SCREEN_SPLIT_GFX_HEIGHT);
    }
    // In text mode, we don't update the display
}

int screen_gfx_save(const char *filename)
{
    // Save the current graphics buffer to an 8-bit indexed color BMP file
    fat32_file_t file;
    fat32_error_t err = fat32_create(&file, filename);
    if (err != FAT32_OK)
    {
        return (err == FAT32_ERROR_FILE_EXISTS) ? EEXIST : EIO;
    }

    size_t written;

    // --- BMP FILE HEADER ---
    uint8_t file_header[BMP_FILE_HEADER_SIZE] = {
        'B', 'M', // Signature
        BMP_FILE_SIZE & 0xFF,
        (BMP_FILE_SIZE >> 8) & 0xFF,
        (BMP_FILE_SIZE >> 16) & 0xFF,
        (BMP_FILE_SIZE >> 24) & 0xFF,
        0, 0, // Reserved1
        0, 0, // Reserved2
        BMP_PIXEL_DATA_OFFSET & 0xFF,
        (BMP_PIXEL_DATA_OFFSET >> 8) & 0xFF,
        (BMP_PIXEL_DATA_OFFSET >> 16) & 0xFF,
        (BMP_PIXEL_DATA_OFFSET >> 24) & 0xFF};
    fat32_write(&file, file_header, BMP_FILE_HEADER_SIZE, &written);

    // --- DIB HEADER (BITMAPINFOHEADER) ---
    uint8_t dib_header[BMP_DIB_HEADER_SIZE] = {0};
    dib_header[0] = BMP_DIB_HEADER_SIZE; // Header size
    dib_header[4] = (SCREEN_WIDTH) & 0xFF;
    dib_header[5] = (SCREEN_WIDTH >> 8) & 0xFF;
    dib_header[6] = (SCREEN_WIDTH >> 16) & 0xFF;
    dib_header[7] = (SCREEN_WIDTH >> 24) & 0xFF;
    dib_header[8] = (SCREEN_HEIGHT) & 0xFF;
    dib_header[9] = (SCREEN_HEIGHT >> 8) & 0xFF;
    dib_header[10] = (SCREEN_HEIGHT >> 16) & 0xFF;
    dib_header[11] = (SCREEN_HEIGHT >> 24) & 0xFF;
    dib_header[12] = BMP_COLOUR_PLANES;     // Planes
    dib_header[14] = BMP_COLOR_DEPTH;       // Bits per pixel
    dib_header[16] = BMP_COMPRESSION;       // Compression (0 = none)
    dib_header[20] = BMP_PIXEL_DATA_SIZE & 0xFF;
    dib_header[21] = (BMP_PIXEL_DATA_SIZE >> 8) & 0xFF;
    dib_header[22] = (BMP_PIXEL_DATA_SIZE >> 16) & 0xFF;
    dib_header[23] = (BMP_PIXEL_DATA_SIZE >> 24) & 0xFF;
    dib_header[24] = (BMP_PIXELS_PER_METER & 0xFF);
    dib_header[25] = (BMP_PIXELS_PER_METER >> 8) & 0xFF;
    dib_header[28] = (BMP_PIXELS_PER_METER & 0xFF);
    dib_header[29] = (BMP_PIXELS_PER_METER >> 8) & 0xFF;
#define BMP_DIB_COLORS_USED_OFFSET        32  /* DIB header byte offset: Colors used */
#define BMP_DIB_IMPORTANT_COLORS_OFFSET   36  /* DIB header byte offset: Important colors */
    dib_header[BMP_DIB_COLORS_USED_OFFSET] = 0;      // Colors used (0 = all)
    dib_header[BMP_DIB_IMPORTANT_COLORS_OFFSET] = 0; // Important colors (0 = all)

    fat32_write(&file, dib_header, BMP_DIB_HEADER_SIZE, &written);

    // --- COLOR PALETTE (256 entries, BGRA format) ---
    // Convert RGB565 palette to BGRA
    for (int i = 0; i < 256; i++)
    {
        uint16_t rgb565 = lcd_get_palette_value(i);
        
        // Extract RGB565 components (5-6-5 bits)
        uint8_t r5 = (rgb565 >> 11) & 0x1F;
        uint8_t g6 = (rgb565 >> 5) & 0x3F;
        uint8_t b5 = rgb565 & 0x1F;
        
        // Convert to 8-bit RGB (expand to full range)
        uint8_t r8 = (r5 * 255 + 15) / 31;
        uint8_t g8 = (g6 * 255 + 31) / 63;
        uint8_t b8 = (b5 * 255 + 15) / 31;
        
        // Write as BGRA
        uint8_t palette_entry[4] = {b8, g8, r8, 0};
        fat32_write(&file, palette_entry, 4, &written);
    }

    // --- PIXEL DATA (bottom-up, with row padding) ---
    // BMP rows must be padded to 4-byte boundaries
    uint8_t padding[3] = {0, 0, 0};
    int padding_bytes = (4 - (SCREEN_WIDTH % 4)) % 4;
    
    for (int y = SCREEN_HEIGHT - 1; y >= 0; y--)
    {
        fat32_write(&file, gfx_buffer + y * SCREEN_WIDTH, SCREEN_WIDTH, &written);
        if (padding_bytes > 0)
        {
            fat32_write(&file, padding, padding_bytes, &written);
        }
    }

    fat32_close(&file);
    return 0;
}

int screen_gfx_load(const char *filename)
{
    // Load an 8-bit indexed color BMP file into the graphics buffer and palette
    fat32_file_t file;
    fat32_error_t err = fat32_open(&file, filename);
    if (err != FAT32_OK)
    {
        return (err == FAT32_ERROR_FILE_NOT_FOUND) ? ENOENT : EIO;
    }

    size_t bytes_read;

    // --- READ AND VALIDATE BMP FILE HEADER ---
    uint8_t file_header[BMP_FILE_HEADER_SIZE];
    err = fat32_read(&file, file_header, BMP_FILE_HEADER_SIZE, &bytes_read);
    if (err != FAT32_OK || bytes_read != BMP_FILE_HEADER_SIZE)
    {
        fat32_close(&file);
        return EIO;
    }

    // Check signature
    if (file_header[0] != 'B' || file_header[1] != 'M')
    {
        fat32_close(&file);
        return EINVAL;
    }

    // Get pixel data offset
    uint32_t pixel_offset = file_header[10] | (file_header[11] << 8) | 
                           (file_header[12] << 16) | (file_header[13] << 24);

    // --- READ AND VALIDATE DIB HEADER ---
    uint8_t dib_header[BMP_DIB_HEADER_SIZE];
    err = fat32_read(&file, dib_header, BMP_DIB_HEADER_SIZE, &bytes_read);
    if (err != FAT32_OK || bytes_read != BMP_DIB_HEADER_SIZE)
    {
        fat32_close(&file);
        return EIO;
    }

    // Extract image properties
    int32_t width = dib_header[4] | (dib_header[5] << 8) | 
                    (dib_header[6] << 16) | (dib_header[7] << 24);
    int32_t height = dib_header[8] | (dib_header[9] << 8) | 
                     (dib_header[10] << 16) | (dib_header[11] << 24);
    uint16_t bits_per_pixel = dib_header[14] | (dib_header[15] << 8);

    // Validate dimensions and format
    if (width != SCREEN_WIDTH || height != SCREEN_HEIGHT || bits_per_pixel != 8)
    {
        fat32_close(&file);
        return EINVAL;
    }

    // --- READ COLOR PALETTE ---
    uint8_t palette_data[256 * 4];
    err = fat32_read(&file, palette_data, 256 * 4, &bytes_read);
    if (err != FAT32_OK || bytes_read != 256 * 4)
    {
        fat32_close(&file);
        return EIO;
    }

    // Convert BGRA palette to RGB565 and update the LCD palette
    for (int i = 0; i < 256; i++)
    {
        uint8_t b = palette_data[i * 4 + 0];
        uint8_t g = palette_data[i * 4 + 1];
        uint8_t r = palette_data[i * 4 + 2];
        
        // Convert 8-bit RGB to RGB565
        uint16_t r5 = (r * 31 + 127) / 255;
        uint16_t g6 = (g * 63 + 127) / 255;
        uint16_t b5 = (b * 31 + 127) / 255;
        uint16_t rgb565 = (r5 << 11) | (g6 << 5) | b5;
        
        lcd_set_palette_value(i, rgb565);
    }

    // --- READ PIXEL DATA ---
    // Seek to pixel data offset
    if (fat32_seek(&file, pixel_offset) != FAT32_OK)
    {
        fat32_close(&file);
        return EIO;
    }

    // Calculate row padding
    int padding_bytes = (4 - (SCREEN_WIDTH % 4)) % 4;
    uint8_t padding[3];

    // Read pixel data (bottom-up)
    for (int y = SCREEN_HEIGHT - 1; y >= 0; y--)
    {
        err = fat32_read(&file, gfx_buffer + y * SCREEN_WIDTH, SCREEN_WIDTH, &bytes_read);
        if (err != FAT32_OK || bytes_read != SCREEN_WIDTH)
        {
            fat32_close(&file);
            return EIO;
        }
        if (padding_bytes > 0)
        {
            if (fat32_read(&file, padding, padding_bytes, &bytes_read) != FAT32_OK || bytes_read != padding_bytes)
            {
                fat32_close(&file);
                return EIO;
            }
        }
    }

    fat32_close(&file);
    return 0;
}

//
//  Text functions
//

// Get the text frame buffer
uint8_t *screen_txt_frame()
{
    return txt_buffer;
}

// Clear the text buffer
void screen_txt_clear(void)
{
    text_row = 0;                                 // Reset the text row to the top
    memset(txt_buffer, 0x20, sizeof(txt_buffer)); // Clear the text buffer
    lcd_clear_screen();                           // Clear the LCD screen in text mode
    screen_txt_set_cursor(0, screen_mode == SCREEN_MODE_SPLIT ? SCREEN_SPLIT_TXT_ROW : 0);
}

// Update the text display
void screen_txt_set_cursor(uint8_t column, uint8_t row)
{
    cursor_column = column < MAX_COLUMN ? column : MAX_COLUMN; // Ensure column is within bounds
    cursor_row = row < SCREEN_ROWS ? row : SCREEN_ROWS - 1;    // Ensure row is within bounds

    screen_txt_map_location(&column, &row);
    lcd_move_cursor(column, row);
}

// Get the current cursor position
void screen_txt_get_cursor(uint8_t *column, uint8_t *row)
{
    if (column)
    {
        *column = cursor_column;
    }
    if (row)
    {
        *row = cursor_row;
    }
}

// Enable or disable the cursor in text mode
void screen_txt_enable_cursor(bool cursor_on)
{
    if (screen_txt_map_location(NULL, NULL))
    {
        cursor_enabled = cursor_on;   // Set the cursor visibility state
        lcd_enable_cursor(cursor_on); // Enable or disable the LCD cursor
    }
    else
    {
        // If the cursor is not visible, we can just set it to the top
        cursor_on = false;
        lcd_enable_cursor(false); // Disable cursor in split mode
    }
}

// Draw the cursor at the current position
void screen_txt_draw_cursor(void)
{
    uint8_t column, row;
    if (screen_txt_map_location(&column, &row))
    {
        // The cursor is visible, we can draw it
        lcd_move_cursor(column, row); // Move the cursor to the current position
        lcd_draw_cursor();            // Draw the cursor at the current position
    }
}

// Erase the cursor at the current position
void screen_txt_erase_cursor(void)
{
    uint8_t column, row;
    if (screen_txt_map_location(&column, &row))
    {
        // The cursor is visible, we can draw it
        lcd_move_cursor(column, row); // Move the cursor to the current position
        lcd_erase_cursor();           // Draw the cursor at the current position
    }
}

// Function to put a character at the current cursor position
// Returns true if the screen scrolled up
bool screen_txt_putc(uint8_t c)
{
    bool scrolled = false;
    if (c == '\n' || c == '\r')
    {
        // Move to next line
        cursor_column = 0;
        cursor_row++;

        if (screen_mode == SCREEN_MODE_TXT || screen_mode == SCREEN_MODE_GFX)
        {
            if (cursor_row >= SCREEN_ROWS)
            {
                // Scroll the text buffer up one line
                screen_txt_scroll_up();
                if (screen_mode == SCREEN_MODE_TXT)
                {
                    lcd_scroll_up(); // Scroll the LCD display up one line in TXT mode
                }
                cursor_row = SCREEN_ROWS - 1;
                scrolled = true;
                screen_txt_set_cursor(cursor_column, cursor_row);
            }
        }
        else
        {
            // Calculate the starting row in the buffer to display
            int16_t start_row = text_row - (SCREEN_SPLIT_TXT_ROWS - 1);
            if (start_row < 0)
            {
                start_row = 0;
            }

            // In split mode, we need to check the text area height
            if (cursor_row >= start_row + SCREEN_SPLIT_TXT_ROWS)
            {
                // Scroll the text buffer up one line
                if (text_row == SCREEN_ROWS - 1)
                {
                    // If we are at the last row, scroll the text area up
                    screen_txt_scroll_up();
                }
                else
                {
                    // Just increment the text row
                    text_row++;
                    start_row++;
                }

                lcd_scroll_up(); // Scroll the LCD display up one line in split mode
                cursor_row = start_row + SCREEN_SPLIT_TXT_ROWS - 1;
                scrolled = true;
                screen_txt_set_cursor(cursor_column, cursor_row);
            }
        }

        // Update the last row written to
        text_row = cursor_row;
    }
    else if (c == '\b') // Backspace
    {
        if (cursor_column > 0)
        {
            cursor_column--;
        }
        else if (cursor_row > 0)
        {
            cursor_row--;
            cursor_column = MAX_COLUMN;
        }
        else
        {
            // At top-left, do nothing
            return false;
        }

        txt_buffer[cursor_row * SCREEN_COLUMNS + cursor_column] = 0; // Clear the character
        if (screen_mode == SCREEN_MODE_TXT || screen_mode == SCREEN_MODE_SPLIT)
        {
            if (screen_mode == SCREEN_MODE_SPLIT)
            {
                // Calculate the starting row in the buffer to display
                int16_t start_row = text_row - (SCREEN_SPLIT_TXT_ROWS - 1);
                if (start_row < 0)
                {
                    start_row = 0;
                }
                if (cursor_row >= start_row && cursor_row < start_row + SCREEN_SPLIT_TXT_ROWS)
                {
                    // Redraw the character at the cursor position
                    lcd_putc(cursor_column, SCREEN_SPLIT_TXT_ROW + cursor_row - start_row, ' ');
                    lcd_move_cursor(cursor_column, SCREEN_SPLIT_TXT_ROW + cursor_row - start_row);
                }
            }
            else
            {
                // In text mode, we can simply clear the character
                lcd_putc(cursor_column, cursor_row, ' ');
                screen_txt_set_cursor(cursor_column, cursor_row);
            }
        }
    }

    else if (c >= 0x20 && c < 0x7F) // Printable characters
    {
        // Store character in buffer
        if (cursor_row < SCREEN_ROWS && cursor_column < SCREEN_COLUMNS)
        {
            txt_buffer[cursor_row * SCREEN_COLUMNS + cursor_column] = c;
            if (screen_mode == SCREEN_MODE_TXT || screen_mode == SCREEN_MODE_SPLIT)
            {
                if (screen_mode == SCREEN_MODE_SPLIT)
                {
                    // Calculate the starting row in the buffer to display
                    int16_t start_row = text_row - (SCREEN_SPLIT_TXT_ROWS - 1);
                    if (start_row < 0)
                    {
                        start_row = 0;
                    }
                    if (cursor_row >= start_row && cursor_row < start_row + SCREEN_SPLIT_TXT_ROWS)
                    {
                        // Redraw the character at the cursor position
                        lcd_putc(cursor_column, SCREEN_SPLIT_TXT_ROW + cursor_row - start_row, c);
                        lcd_move_cursor(cursor_column + 1, SCREEN_SPLIT_TXT_ROW + cursor_row - start_row);
                    }
                }
                else
                {
                    // In text mode, we can simply clear the character
                    lcd_putc(cursor_column, cursor_row, c);
                    lcd_move_cursor(cursor_column + 1, cursor_row);
                }
            }
            cursor_column++;

            // Wrap to next line if needed
            if (cursor_column >= SCREEN_COLUMNS)
            {
                cursor_column = 0;
                cursor_row++;
                if (cursor_row >= SCREEN_ROWS)
                {
                    // Scroll the text buffer up one line
                    screen_txt_scroll_up();
                    if (screen_mode == SCREEN_MODE_TXT || screen_mode == SCREEN_MODE_SPLIT)
                    {
                        lcd_scroll_up(); // Scroll the LCD display up one line in TXT mode
                    }
                    cursor_row = SCREEN_ROWS - 1;
                    scrolled = true;
                    lcd_move_cursor(cursor_column, cursor_row);
                }
            }
        }
    }

    return scrolled;
}

// Function to put a string at the current cursor position
// Returns true if the screen scrolled up
bool screen_txt_puts(const char *str)
{
    bool scrolled = false;
    if (!str || !*str) // Check for null or empty string
    {
        return false;
    }

    while (*str)
    {
        if (screen_txt_putc(*str))
        {
            scrolled = true;
        }
        str++;
    }

    return scrolled;
}

// Write the text buffer to the LCD display
void screen_txt_update(void)
{
    bool cursor_enabled = lcd_cursor_enabled(); // Save the current cursor state
    lcd_enable_cursor(false);                   // Disable the cursor while updating

    if (screen_mode == SCREEN_MODE_TXT)
    {
        // In full-screen text mode, we clear the screen and redraw the text
        for (uint8_t row = 0; row < SCREEN_ROWS; row++)
        {
            for (uint8_t col = 0; col < SCREEN_COLUMNS; col++)
            {
                uint8_t c = txt_buffer[row * SCREEN_COLUMNS + col] & 0xFF;
                lcd_putc(col, row, c > 0 && c < 0x7F ? c : ' ');
            }
        }
    }
    else if (screen_mode == SCREEN_MODE_SPLIT)
    {
        // In split screen mode, we copy:
        // From and including the cursor_row and the preceeding number of rows equal to the text area height
        // If there are not enough rows, we insert empty rows after the cursor_row

        // Clear the text area first
        //        lcd_scroll_clear();

        // Calculate the starting row in the buffer to display
        int16_t start_row = text_row - (SCREEN_SPLIT_TXT_ROWS - 1);
        if (start_row < 0)
        {
            start_row = 0;
        }

        // Display the visible rows
        for (uint8_t display_row = 0; display_row < SCREEN_SPLIT_TXT_ROWS; display_row++)
        {
            int16_t buffer_row = start_row + display_row;

            if (buffer_row < SCREEN_ROWS)
            {
                // Copy this row from the buffer to the display
                for (uint8_t col = 0; col < SCREEN_COLUMNS; col++)
                {
                    uint8_t c = txt_buffer[buffer_row * SCREEN_COLUMNS + col];
                    lcd_putc(col, SCREEN_SPLIT_TXT_ROW + display_row, c > 0 && c < 0x7F ? c : ' ');
                }
            }
            // If buffer_row >= SCREEN_ROWS, the row remains empty (already cleared)
        }
    }
    // else for full-screen graphics mode, we do not update the text display

    lcd_enable_cursor(cursor_enabled); // Restore the cursor state
}

//
//  Screen initialization
//

// Initialize the screen
void screen_init()
{
    // Initialize the display
    lcd_init();

    // Set for a default of split screen
    screen_set_mode(SCREEN_MODE_TXT);

    // Set foreground and background colors
    lcd_set_foreground(foreground);
    lcd_set_background(background);

    // Disable cursor
    screen_txt_enable_cursor(true);
}