//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Defines the screen interface for PicoCalc device.
//

#pragma once

#include "pico/stdlib.h"
#include "devices/font.h"
#include "devices/console.h"

// Screen modes
#define SCREEN_MODE_TXT (0)   // Full-screen text, no graphics
#define SCREEN_MODE_GFX (1)   // Full-screen graphics, no text
#define SCREEN_MODE_SPLIT (2) // Split screen with graphics on top and text on bottom

// Screen dimensions
#define SCREEN_WIDTH (320)                                                            // Width of the screen in pixels
#define SCREEN_HEIGHT (320)                                                           // Height of the screen in pixels
#define SCREEN_COLUMNS (SCREEN_WIDTH / GLYPH_WIDTH)                                   // Max Number of text columns that fit on the screen
#define SCREEN_ROWS (SCREEN_HEIGHT / GLYPH_HEIGHT)                                    // Number of text rows that fit on the screen
#define SCREEN_SPLIT_GFX_HEIGHT (240)                                                 // Height of the graphics area in split mode
#define SCREEN_SPLIT_TXT_HEIGHT (SCREEN_HEIGHT - SCREEN_SPLIT_GFX_HEIGHT)             // Height of the text area in split mode
#define SCREEN_SPLIT_TXT_ROW (SCREEN_HEIGHT - SCREEN_SPLIT_TXT_HEIGHT) / GLYPH_HEIGHT // Start row of text rows in split mode
#define SCREEN_SPLIT_TXT_ROWS (SCREEN_SPLIT_TXT_HEIGHT / GLYPH_HEIGHT)                // Number of text rows in split mode

// Text definitions
#define TXT_DEFAULT_FOREGROUND (254) // Default foreground color (light blue)
#define TXT_DEFAULT_BACKGROUND (255) // Default background color (blue)

// Text definitions
#define GFX_DEFAULT_BACKGROUND (255) // Default background color (blue)

// Screen boundary modes for graphics
typedef enum {
    SCREEN_BOUNDARY_FENCE,   // Error if turtle hits edge (not used for drawing)
    SCREEN_BOUNDARY_WINDOW,  // Clip drawing to screen bounds
    SCREEN_BOUNDARY_WRAP     // Wrap coordinates around edges (default)
} ScreenBoundaryMode;

// BMP definitions (8-bit indexed color)
#define BMP_FILE_HEADER_SIZE (14)                                                                           // Size of the BMP file header in bytes
#define BMP_DIB_HEADER_SIZE (40)                                                                            // Size of the DIB header in bytes
#define BMP_PALETTE_SIZE (256 * 4)                                                                          // 256 colors, 4 bytes each (BGRA)
#define BMP_BYTES_PER_PIXEL (1)                                                                             // Bytes per pixel for 8-bit indexed
#define BMP_ROW_SIZE (((SCREEN_WIDTH * BMP_BYTES_PER_PIXEL + 3) / 4) * 4)                                   // Size of a row in bytes (padded to 4-byte boundary)
#define BMP_PIXEL_DATA_SIZE (BMP_ROW_SIZE * SCREEN_HEIGHT)                                                  // Size of the pixel data in bytes
#define BMP_COLOR_DEPTH (8)                                                                                 // Color depth in bits per pixel
#define BMP_COMPRESSION (0)                                                                                 // Compression type (BI_RGB, no compression)
#define BMP_COLOUR_PLANES (1)                                                                               // Number of color planes
#define BMP_PIXELS_PER_METER (2835)                                                                         // Pixels per meter for X and Y (default)
#define BMP_PIXEL_DATA_OFFSET (BMP_FILE_HEADER_SIZE + BMP_DIB_HEADER_SIZE + BMP_PALETTE_SIZE)               // Offset to pixel data
#define BMP_FILE_SIZE (BMP_FILE_HEADER_SIZE + BMP_DIB_HEADER_SIZE + BMP_PALETTE_SIZE + BMP_PIXEL_DATA_SIZE) // Total file size

// Scanline fill algorithm with a fixed-size stack
// Each stack entry stores a scanline segment to process
// Stack size of 1024 handles most reasonable shapes on a 320x320 screen
#define FILL_STACK_SIZE 1024
typedef struct {
    int16_t y;       // Y coordinate of the scanline
    int16_t x_left;  // Left X of the segment
    int16_t x_right; // Right X of the segment
    int8_t dir;      // Direction: 1 = down, -1 = up
} FillSpan;

// Helper macro to push a span onto the stack
#define PUSH_SPAN(Y, XL, XR, D) do { \
    if (stack_ptr < FILL_STACK_SIZE) { \
        stack[stack_ptr].y = (Y); \
        stack[stack_ptr].x_left = (XL); \
        stack[stack_ptr].x_right = (XR); \
        stack[stack_ptr].dir = (D); \
        stack_ptr++; \
    } \
} while(0)

// Function prototypes

// Screen mode functions (TXT, GFX, SPLIT)
uint8_t screen_get_mode(void);
void screen_set_mode(uint8_t mode);
void screen_set_mode_no_update(uint8_t mode);  // Switch mode without redrawing buffers
void screen_show_field(void);
bool screen_handle_mode_key(int key_code);  // Handle F1/F2/F3 screen mode keys, returns true if handled

// Graphics functions
uint8_t *screen_gfx_frame(void);
void screen_gfx_clear(void);
void screen_gfx_set_boundary_mode(ScreenBoundaryMode mode);
ScreenBoundaryMode screen_gfx_get_boundary_mode(void);
void screen_gfx_set_point(float x, float y, uint8_t colour);
uint8_t screen_gfx_get_point(float x, float y);
void screen_gfx_line(float x1, float y1, float x2, float y2, uint8_t colour, bool reverse);
void screen_gfx_fill(float x, float y, uint8_t colour);
void screen_gfx_update(void);
int screen_gfx_save(const char *filename);
int screen_gfx_load(const char *filename);

// Text functions
uint8_t *screen_txt_frame(void);
void screen_txt_clear(void);
bool screen_txt_putc(uint8_t c);
bool screen_txt_puts(const char *str);
void screen_txt_set_cursor(uint8_t column, uint8_t row);
void screen_txt_get_cursor(uint8_t *column, uint8_t *row);
void screen_txt_enable_cursor(bool cursor_on);
void screen_txt_draw_cursor(void);
void screen_txt_erase_cursor(void);
void screen_txt_update(void);

// Screen initialization
void screen_init(void);