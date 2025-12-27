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

// Graphics boundary mode (for turtle graphics)
static ScreenBoundaryMode screen_boundary_mode = SCREEN_BOUNDARY_WRAP;

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
// Used when boundary mode is WRAP
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

// Clip and round a floating point value to the nearest pixel
// Returns -1 if the value is outside the range [0, max)
// Used when boundary mode is WINDOW or FENCE
static int clip_and_round(float value, int max)
{
    // Round to nearest integer
    int pixel = (int)(value + 0.5f);
    // Clip to bounds
    if (pixel < 0 || pixel >= max)
    {
        return -1;  // Out of bounds
    }
    return pixel;
}

// Set the graphics boundary mode
void screen_gfx_set_boundary_mode(ScreenBoundaryMode mode)
{
    screen_boundary_mode = mode;
}

// Get the current graphics boundary mode
ScreenBoundaryMode screen_gfx_get_boundary_mode(void)
{
    return screen_boundary_mode;
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

bool screen_handle_mode_key(int key_code)
{
    switch (key_code)
    {
    case 0x81: // KEY_F1
        screen_set_mode(SCREEN_MODE_TXT);
        screen_txt_enable_cursor(true);
        return true;
    case 0x82: // KEY_F2
        screen_set_mode(SCREEN_MODE_SPLIT);
        screen_txt_enable_cursor(true);
        return true;
    case 0x83: // KEY_F3
        screen_set_mode(SCREEN_MODE_GFX);
        screen_txt_enable_cursor(false);
        return true;
    default:
        return false;
    }
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

// Switch screen mode without redrawing the graphics or text buffers.
//
// This is a low-level helper used by the on-device editor when it knows that the
// contents of gfx_buffer/txt_buffer and the LCD are already in the correct state
// for the new mode. It only updates the LCD scrolling region and cursor visibility
// to match the requested mode; it does *not* call screen_gfx_update() or
// screen_txt_update().
//
// In most cases, callers should use screen_set_mode(), which both changes the
// logical screen_mode and redraws the display from the frame buffers to keep the
// LCD contents and internal state in sync.
//
// Using this function when the current LCD contents do not match the in-memory
// buffers (for example after drawing or writing text in a different mode) may
// leave the display in an inconsistent or partially updated state. Only use this
// variant when you explicitly want to avoid a full redraw and you can guarantee
// that the buffers and hardware are already consistent for the new mode.
void screen_set_mode_no_update(uint8_t mode)
{
    if (mode == SCREEN_MODE_TXT || mode == SCREEN_MODE_GFX || mode == SCREEN_MODE_SPLIT)
    {
        screen_mode = mode;

        if (mode == SCREEN_MODE_TXT)
        {
            lcd_define_scrolling(0, 0);
        }
        else if (mode == SCREEN_MODE_GFX)
        {
            lcd_erase_cursor();
            lcd_define_scrolling(0, 0);
        }
        else if (mode == SCREEN_MODE_SPLIT)
        {
            lcd_erase_cursor();
            lcd_define_scrolling(SCREEN_SPLIT_GFX_HEIGHT, 0);
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
    int pixel_x, pixel_y;

    if (screen_boundary_mode == SCREEN_BOUNDARY_WINDOW || screen_boundary_mode == SCREEN_BOUNDARY_FENCE)
    {
        // Clip to screen bounds
        pixel_x = clip_and_round(x, SCREEN_WIDTH);
        pixel_y = clip_and_round(y, SCREEN_HEIGHT);
        if (pixel_x < 0 || pixel_y < 0)
        {
            return;  // Point is off-screen, don't draw
        }
    }
    else
    {
        // Wrap around edges
        pixel_x = wrap_and_round(x, SCREEN_WIDTH);
        pixel_y = wrap_and_round(y, SCREEN_HEIGHT);
    }

    gfx_buffer[pixel_y * SCREEN_WIDTH + pixel_x] = colour;
}

uint8_t screen_gfx_get_point(float x, float y)
{
    int pixel_x, pixel_y;

    if (screen_boundary_mode == SCREEN_BOUNDARY_WINDOW || screen_boundary_mode == SCREEN_BOUNDARY_FENCE)
    {
        // Clip to screen bounds
        pixel_x = clip_and_round(x, SCREEN_WIDTH);
        pixel_y = clip_and_round(y, SCREEN_HEIGHT);
        if (pixel_x < 0 || pixel_y < 0)
        {
            return GFX_DEFAULT_BACKGROUND;  // Off-screen returns background
        }
    }
    else
    {
        // Wrap around edges
        pixel_x = wrap_and_round(x, SCREEN_WIDTH);
        pixel_y = wrap_and_round(y, SCREEN_HEIGHT);
    }

    return gfx_buffer[pixel_y * SCREEN_WIDTH + pixel_x];
}

void screen_gfx_reverse_point(float x, float y)
{
    int pixel_x, pixel_y;

    if (screen_boundary_mode == SCREEN_BOUNDARY_WINDOW || screen_boundary_mode == SCREEN_BOUNDARY_FENCE)
    {
        // Clip to screen bounds
        pixel_x = clip_and_round(x, SCREEN_WIDTH);
        pixel_y = clip_and_round(y, SCREEN_HEIGHT);
        if (pixel_x < 0 || pixel_y < 0)
        {
            return;  // Point is off-screen, don't draw
        }
    }
    else
    {
        // Wrap around edges
        pixel_x = wrap_and_round(x, SCREEN_WIDTH);
        pixel_y = wrap_and_round(y, SCREEN_HEIGHT);
    }

    uint8_t *pixel = &gfx_buffer[pixel_y * SCREEN_WIDTH + pixel_x];
    *pixel = (*pixel == GFX_DEFAULT_BACKGROUND) ? foreground : GFX_DEFAULT_BACKGROUND;
}

// Draw a line in the graphics buffer using Bresenham's algorithm
// This is a true integer-only Bresenham implementation for efficiency on RP2040
void screen_gfx_line(float x1, float y1, float x2, float y2, uint8_t colour, bool reverse)
{
    // Convert float endpoints to integers (round to nearest)
    int ix1 = (int)(x1 + 0.5f);
    int iy1 = (int)(y1 + 0.5f);
    int ix2 = (int)(x2 + 0.5f);
    int iy2 = (int)(y2 + 0.5f);

    int dx = ix2 - ix1;
    int dy = iy2 - iy1;

    // Determine step direction for each axis
    int sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    int sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;

    // Work with absolute differences
    dx = (dx < 0) ? -dx : dx;
    dy = (dy < 0) ? -dy : dy;

    int x = ix1;
    int y = iy1;

    // Helper macro to plot a pixel with boundary handling
    #define PLOT_PIXEL(px, py) do { \
        int plot_x, plot_y; \
        if (screen_boundary_mode == SCREEN_BOUNDARY_WINDOW || screen_boundary_mode == SCREEN_BOUNDARY_FENCE) { \
            if ((px) < 0 || (px) >= SCREEN_WIDTH || (py) < 0 || (py) >= SCREEN_HEIGHT) { \
                /* Skip out-of-bounds pixels */ \
            } else { \
                plot_x = (px); \
                plot_y = (py); \
                uint8_t *pixel = &gfx_buffer[plot_y * SCREEN_WIDTH + plot_x]; \
                if (reverse) { \
                    *pixel = (*pixel == GFX_DEFAULT_BACKGROUND) ? colour : GFX_DEFAULT_BACKGROUND; \
                } else { \
                    *pixel = colour; \
                } \
            } \
        } else { \
            /* Wrap mode */ \
            plot_x = (((px) % SCREEN_WIDTH) + SCREEN_WIDTH) % SCREEN_WIDTH; \
            plot_y = (((py) % SCREEN_HEIGHT) + SCREEN_HEIGHT) % SCREEN_HEIGHT; \
            uint8_t *pixel = &gfx_buffer[plot_y * SCREEN_WIDTH + plot_x]; \
            if (reverse) { \
                *pixel = (*pixel == GFX_DEFAULT_BACKGROUND) ? colour : GFX_DEFAULT_BACKGROUND; \
            } else { \
                *pixel = colour; \
            } \
        } \
    } while(0)

    if (dx >= dy)
    {
        // X is the driving axis
        int err = 2 * dy - dx;
        for (int i = 0; i <= dx; ++i)
        {
            PLOT_PIXEL(x, y);
            if (err > 0)
            {
                y += sy;
                err -= 2 * dx;
            }
            err += 2 * dy;
            x += sx;
        }
    }
    else
    {
        // Y is the driving axis
        int err = 2 * dx - dy;
        for (int i = 0; i <= dy; ++i)
        {
            PLOT_PIXEL(x, y);
            if (err > 0)
            {
                x += sx;
                err -= 2 * dy;
            }
            err += 2 * dx;
            y += sy;
        }
    }

    #undef PLOT_PIXEL
}

// Flood fill using scanline algorithm
// Uses the colour as both boundary and fill colour (as per Logo spec)
// Fill starts at (x, y) and fills all pixels that are NOT the boundary colour
void screen_gfx_fill(float x, float y, uint8_t colour)
{
    // Convert to pixel coordinates
    int start_x = (int)(x + 0.5f);
    int start_y = (int)(y + 0.5f);

    // Bounds check
    if (start_x < 0 || start_x >= SCREEN_WIDTH || 
        start_y < 0 || start_y >= SCREEN_HEIGHT)
    {
        return;
    }

    // The fill colour is the same as the boundary colour
    uint8_t boundary_colour = colour;

    // Check if starting point is already the boundary colour - nothing to fill
    if (gfx_buffer[start_y * SCREEN_WIDTH + start_x] == boundary_colour)
    {
        return;
    }

    static FillSpan stack[FILL_STACK_SIZE];
    int stack_ptr = 0;

    // Find the initial span containing the start point
    int left = start_x;
    int right = start_x;
    uint8_t *row = &gfx_buffer[start_y * SCREEN_WIDTH];

    // Extend left
    while (left > 0 && row[left - 1] != boundary_colour)
    {
        left--;
    }
    // Extend right
    while (right < SCREEN_WIDTH - 1 && row[right + 1] != boundary_colour)
    {
        right++;
    }

    // Fill the initial span
    for (int i = left; i <= right; i++)
    {
        row[i] = colour;
    }

    // Push spans above and below to process
    if (start_y > 0)
    {
        PUSH_SPAN(start_y - 1, left, right, -1);
    }
    if (start_y < SCREEN_HEIGHT - 1)
    {
        PUSH_SPAN(start_y + 1, left, right, 1);
    }

    // Process the stack
    while (stack_ptr > 0)
    {
        // Pop a span
        stack_ptr--;
        int y = stack[stack_ptr].y;
        int x_left = stack[stack_ptr].x_left;
        int x_right = stack[stack_ptr].x_right;
        int dir = stack[stack_ptr].dir;

        row = &gfx_buffer[y * SCREEN_WIDTH];

        // Scan the row from x_left to x_right, finding and filling spans
        int x = x_left;
        while (x <= x_right)
        {
            // Skip boundary pixels
            while (x <= x_right && row[x] == boundary_colour)
            {
                x++;
            }
            if (x > x_right)
            {
                break;
            }

            // Found a fillable pixel, extend to find the full span
            int span_left = x;

            // Extend left beyond the parent span if possible
            while (span_left > 0 && row[span_left - 1] != boundary_colour)
            {
                span_left--;
            }

            // Find right edge
            while (x < SCREEN_WIDTH && row[x] != boundary_colour)
            {
                x++;
            }
            int span_right = x - 1;

            // Fill this span
            for (int i = span_left; i <= span_right; i++)
            {
                row[i] = colour;
            }

            // Push span in same direction
            int next_y = y + dir;
            if (next_y >= 0 && next_y < SCREEN_HEIGHT)
            {
                PUSH_SPAN(next_y, span_left, span_right, dir);
            }

            // Push span in opposite direction if we extended beyond parent
            int prev_y = y - dir;
            if (prev_y >= 0 && prev_y < SCREEN_HEIGHT)
            {
                // Push for the left extension
                if (span_left < x_left)
                {
                    PUSH_SPAN(prev_y, span_left, x_left - 1, -dir);
                }
                // Push for the right extension
                if (span_right > x_right)
                {
                    PUSH_SPAN(prev_y, x_right + 1, span_right, -dir);
                }
            }
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
    
    if (screen_mode == SCREEN_MODE_SPLIT)
    {
        lcd_scroll_clear();                       // Clear only the text area in split mode
    }
    else
    {
        lcd_clear_screen();                       // Clear the entire LCD screen in text mode
    }
    
    // Always set cursor to row 0 in the text buffer.
    // screen_txt_map_location will map this to the correct LCD row
    // (row 0 in text mode, row 24 in split mode).
    screen_txt_set_cursor(0, 0);
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

        // GFX mode is included here to maintain text buffer state for when
        // switching back to TXT or SPLIT mode. The LCD is only updated in TXT mode.
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

    else // All other characters are stored in the buffer
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

                // GFX mode is included here to maintain text buffer state for when
                // switching back to TXT or SPLIT mode. The LCD is only updated in TXT mode.
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
                    }
                    screen_txt_set_cursor(cursor_column, cursor_row);
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
                    }

                    // Update the last row written to
                    text_row = cursor_row;
                    screen_txt_set_cursor(cursor_column, cursor_row);
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
                lcd_putc(col, row, txt_buffer[row * SCREEN_COLUMNS + col]);
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
                    lcd_putc(col, SCREEN_SPLIT_TXT_ROW + display_row, txt_buffer[buffer_row * SCREEN_COLUMNS + col]);
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