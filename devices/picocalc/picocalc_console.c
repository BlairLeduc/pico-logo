//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements a PicoCalc device that uses standard input and output.
//

#include "picocalc_console.h"
#include "devices/console.h"
#include "devices/stream.h"
#include "editor.h"
#include "input.h"
#include "keyboard.h"
#include "lcd.h"
#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <pico/stdlib.h>
#include <pico/rand.h>

// Turtle state
float turtle_x = TURTLE_HOME_X;                         // Current x position for graphics
float turtle_y = TURTLE_HOME_Y;                         // Current y position for graphics
float turtle_angle = TURTLE_DEFAULT_ANGLE;              // Current angle for graphics
uint8_t turtle_colour = TURTLE_DEFAULT_COLOUR;          // Turtle color for graphics
uint8_t background_colour = GFX_DEFAULT_BACKGROUND;     // Background color for graphics
static LogoPen turtle_pen_state = LOGO_PEN_DOWN;        // Pen state for graphics
static bool turtle_visible = TURTLE_DEFAULT_VISIBILITY; // Turtle visibility state for graphics

// Shape system
// Shapes 1-15 stored internally as 16-bit rows (doubled from user's 8-bit)
// Shape 0 is the line-drawn turtle and doesn't use this storage
static uint16_t turtle_shapes[15][16];  // shapes[0] = shape 1, shapes[14] = shape 15
static uint8_t turtle_current_shape = 0;

// Background buffer: 16x16 pixels saved before drawing turtle
// Buffer origin X is always at (turtle_x - 8); origin Y depends on shape:
//   - shape 0:  (turtle_y - 8)   (centered on turtle position)
//   - shapes 1-15: (turtle_y - 15) (bottom row aligned with turtle_y)
static uint8_t turtle_background[16][16];
static int turtle_bg_saved_x;  // Screen X where background was saved (top-left)
static int turtle_bg_saved_y;  // Screen Y where background was saved (top-left)
static bool turtle_bg_valid = false;  // Whether background has been saved

// Boundary mode constants
typedef enum {
    BOUNDARY_MODE_FENCE,   // Turtle stops at boundary (error if hits edge)
    BOUNDARY_MODE_WINDOW,  // Turtle can go off-screen (unbounded)
    BOUNDARY_MODE_WRAP     // Turtle wraps around edges (default)
} BoundaryMode;

static BoundaryMode turtle_boundary_mode = BOUNDARY_MODE_WRAP;  // Default to wrap mode

// Screen boundary constants (in turtle coordinates, centered at origin)
#define TURTLE_MIN_X (-SCREEN_WIDTH / 2)
#define TURTLE_MAX_X (SCREEN_WIDTH / 2 - 1)
#define TURTLE_MIN_Y (-SCREEN_HEIGHT / 2)
#define TURTLE_MAX_Y (SCREEN_HEIGHT / 2 - 1)

//
//  Stream operations for keyboard input
//

static int input_read_char(LogoStream *stream)
{
    (void)stream;
    
    // Set input_active so keyboard_poll buffers F1/F2/F3 instead of switching modes
    input_active = true;
    
    while (true)
    {
        int ch = keyboard_get_key();
        
        // Convert KEY_BREAK to LOGO_STREAM_INTERRUPTED
        if (ch == KEY_BREAK)
        {
            input_active = false;
            return LOGO_STREAM_INTERRUPTED;
        }
        
        // Handle F1/F2/F3 for screen mode switching, then get another key
        if (screen_handle_mode_key(ch))
        {
            continue;  // Get next key
        }
        
        input_active = false;
        return ch;
    }
}

static int input_read_chars(LogoStream *stream, char *buffer, int count)
{
    if (!buffer || count <= 0)
    {
        return 0;
    }

    int read_count = 0;
    for (int i = 0; i < count; i++)
    {
        int ch = input_read_char(stream);
        if (ch == LOGO_STREAM_INTERRUPTED)
        {
            // Return what we have so far, or signal interrupted if nothing read
            return (read_count > 0) ? read_count : LOGO_STREAM_INTERRUPTED;
        }
        if (ch == EOF || ch < 0)
        {
            break;
        }
        buffer[i] = (char)ch;
        read_count++;
    }
    return read_count;
}

static int input_read_line(LogoStream *stream, char *buffer, size_t size)
{
    (void)stream;
    return picocalc_read_line(buffer, size);
}

static bool input_can_read(LogoStream *stream)
{
    (void)stream;
    return keyboard_key_available();
}

//
//  Stream operations for screen output
//

static void output_write(LogoStream *stream, const char *text)
{
    screen_txt_puts(text);
}

static void output_flush(LogoStream *stream)
{
    screen_txt_update();
}

//
//  Stream ops tables
//

static const LogoStreamOps picocalc_input_ops = {
    .read_char = input_read_char,
    .read_chars = input_read_chars,
    .read_line = input_read_line,
    .can_read = input_can_read,
    .write = NULL,
    .flush = NULL,
    .get_read_pos = NULL,
    .set_read_pos = NULL,
    .get_write_pos = NULL,
    .set_write_pos = NULL,
    .get_length = NULL,
    .close = NULL,
};

static const LogoStreamOps picocalc_output_ops = {
    .read_char = NULL,
    .read_chars = NULL,
    .read_line = NULL,
    .can_read = NULL,
    .write = output_write,
    .flush = output_flush,
    .get_read_pos = NULL,
    .set_read_pos = NULL,
    .get_write_pos = NULL,
    .set_write_pos = NULL,
    .get_length = NULL,
    .close = NULL,
};

//
//  Screen operations
//

static void screen_fullscreen(void)
{
    screen_set_mode(SCREEN_MODE_GFX);
}

static void screen_splitscreen(void)
{
    screen_set_mode(SCREEN_MODE_SPLIT);
}

static void screen_textscreen(void)
{
    screen_set_mode(SCREEN_MODE_TXT);
}

static const LogoConsoleScreen picocalc_screen_ops = {
    .fullscreen = screen_fullscreen,
    .splitscreen = screen_splitscreen,
    .textscreen = screen_textscreen,
};

//
//  Text Screen functions
//

static void text_clear(void)
{
    screen_txt_clear();
}

static void text_set_cursor(uint8_t column, uint8_t row)
{
    screen_txt_set_cursor(column, row);
}

static void text_get_cursor(uint8_t *column, uint8_t *row)
{
    screen_txt_get_cursor(column, row);
}

static const LogoConsoleText picocalc_text_ops = {
    .clear = text_clear,
    .set_cursor = text_set_cursor,
    .get_cursor = text_get_cursor,
};

//
//  Turtle graphics functions
//

// Helper: wrap coordinate to screen bounds
static inline int wrap_coord(int val, int max)
{
    val = val % max;
    if (val < 0) val += max;
    return val;
}

// Helper: check if turtle is visible on screen (for window mode)
static bool turtle_should_draw(void)
{
    if (!turtle_visible)
        return false;
    
    // In window mode, don't draw if turtle center is off-screen
    if (turtle_boundary_mode == BOUNDARY_MODE_WINDOW)
    {
        if (turtle_x < 0 || turtle_x >= SCREEN_WIDTH ||
            turtle_y < 0 || turtle_y >= SCREEN_HEIGHT)
        {
            return false;
        }
    }
    return true;
}

// Get the graphics frame buffer pointer
static uint8_t *get_gfx_pixel(int x, int y)
{
    // Wrap coordinates
    x = wrap_coord(x, SCREEN_WIDTH);
    y = wrap_coord(y, SCREEN_HEIGHT);
    return &screen_gfx_frame()[y * SCREEN_WIDTH + x];
}

// Save the 16x16 background area under the turtle
// For shape 0 (rotating): centered at turtle position
// For shapes 1-15 (non-rotating): bottom row at turtle_y
static void turtle_save_background(void)
{
    turtle_bg_saved_x = (int)turtle_x - 8;
    
    // Shape 0 rotates, so we need the buffer centered on turtle position
    // Shapes 1-15 don't rotate, so buffer extends above turtle position
    if (turtle_current_shape == 0)
    {
        turtle_bg_saved_y = (int)turtle_y - 8;  // Centered vertically
    }
    else
    {
        turtle_bg_saved_y = (int)turtle_y - 15; // Bottom row at turtle_y
    }
    
    for (int row = 0; row < 16; row++)
    {
        for (int col = 0; col < 16; col++)
        {
            int sx = wrap_coord(turtle_bg_saved_x + col, SCREEN_WIDTH);
            int sy = wrap_coord(turtle_bg_saved_y + row, SCREEN_HEIGHT);
            turtle_background[row][col] = screen_gfx_frame()[sy * SCREEN_WIDTH + sx];
        }
    }
    turtle_bg_valid = true;
}

// Restore the saved background (erase turtle)
static void turtle_erase(void)
{
    if (!turtle_bg_valid)
        return;
    
    for (int row = 0; row < 16; row++)
    {
        for (int col = 0; col < 16; col++)
        {
            int sx = wrap_coord(turtle_bg_saved_x + col, SCREEN_WIDTH);
            int sy = wrap_coord(turtle_bg_saved_y + row, SCREEN_HEIGHT);
            screen_gfx_frame()[sy * SCREEN_WIDTH + sx] = turtle_background[row][col];
        }
    }
    turtle_bg_valid = false;
}

// Draw shape 0: the default line-drawn turtle triangle
// Must fit within 16x16 centered on turtle position in any rotation
// Max extent from center = 7 pixels
static void turtle_draw_shape0(void)
{
    // Convert angle to radians
    float radians = turtle_angle * (M_PI / 180.0f);
    float sin_a = sinf(radians);
    float cos_a = cosf(radians);
    
    // Triangle dimensions to fit in 16x16 centered area (max 7 pixels from center)
    // Base width = 6 pixels (half_base = 3), height = 7 pixels
    // Turtle position is at center of base
    float half_base = 3.0f;
    float height = 7.0f;
    
    // Calculate the three vertices of the triangle
    // Base vertices: perpendicular to heading direction at turtle position
    float x1 = turtle_x + half_base * cos_a;
    float y1 = turtle_y + half_base * sin_a;
    float x2 = turtle_x - half_base * cos_a;
    float y2 = turtle_y - half_base * sin_a;
    
    // Tip vertex: in the direction of heading, 'height' pixels away
    float x3 = turtle_x + height * sin_a;
    float y3 = turtle_y - height * cos_a;
    
    // Draw the triangle using lines (non-XOR, with current pen color)
    screen_gfx_line(x1, y1, x2, y2, turtle_colour, false);
    screen_gfx_line(x2, y2, x3, y3, turtle_colour, false);
    screen_gfx_line(x3, y3, x1, y1, turtle_colour, false);
}

// Draw shapes 1-15: bitmap shapes that don't rotate
// Shape is 16 pixels wide (doubled from user's 8) x 16 pixels tall
// Bottom row (row 15) is at turtle_y
static void turtle_draw_bitmap_shape(void)
{
    uint16_t *shape = turtle_shapes[turtle_current_shape - 1];
    int base_x = (int)turtle_x - 8;  // Center horizontally
    int base_y = (int)turtle_y - 15; // Row 15 (bottom) at turtle_y
    
    for (int row = 0; row < 16; row++)
    {
        uint16_t row_bits = shape[row];
        if (row_bits == 0) continue;  // Skip empty rows
        
        for (int col = 0; col < 16; col++)
        {
            // MSB is leftmost column
            if (row_bits & (0x8000 >> col))
            {
                int sx = wrap_coord(base_x + col, SCREEN_WIDTH);
                int sy = wrap_coord(base_y + row, SCREEN_HEIGHT);
                screen_gfx_frame()[sy * SCREEN_WIDTH + sx] = turtle_colour;
            }
        }
    }
}

// Draw the turtle at the current position
static void turtle_draw(void)
{
    if (!turtle_should_draw())
        return;
    
    // Save background before drawing
    turtle_save_background();
    
    // Draw the appropriate shape
    if (turtle_current_shape == 0)
    {
        turtle_draw_shape0();
    }
    else
    {
        turtle_draw_bitmap_shape();
    }
}

// Clear the graphics buffer and reset the turtle to the home position
static void turtle_clearscreen(void)
{
    screen_show_field();

    // Invalidate background buffer since screen is being cleared
    turtle_bg_valid = false;

    // Clear the graphics buffer
    screen_gfx_clear();

    // Reset the turtle to the home position
    turtle_x = TURTLE_HOME_X;
    turtle_y = TURTLE_HOME_Y;
    turtle_angle = TURTLE_DEFAULT_ANGLE;
    // Note: turtle shape is NOT reset by clearscreen

    // Draw the turtle at the home position
    turtle_draw();

    // Update the graphics display
    screen_gfx_update();
}

// Move the turtle forward or backward by the specified distance
// Returns true on success, false if boundary error in fence mode
static bool turtle_move(float distance)
{
    screen_show_field();

    float old_x = turtle_x;
    float old_y = turtle_y;

    // Erase turtle at current position
    turtle_erase();

    // Calculate new position in screen coordinates
    float new_x = turtle_x + distance * sinf(turtle_angle * (M_PI / 180.0f));
    float new_y = turtle_y - distance * cosf(turtle_angle * (M_PI / 180.0f));

    // Convert to Logo coordinates (centered at origin) for boundary checking
    float logo_x = new_x - SCREEN_WIDTH / 2;
    float logo_y = new_y - SCREEN_HEIGHT / 2;

    // Handle boundary modes
    switch (turtle_boundary_mode)
    {
    case BOUNDARY_MODE_FENCE:
        // Check if new position is out of bounds (in Logo coordinates)
        // Use 0.5 threshold to align with pixel rounding: (int)(value + 0.5f)
        // This ensures coordinates that round to valid pixels are accepted
        if (logo_x < TURTLE_MIN_X - 0.5f || logo_x >= TURTLE_MAX_X + 0.5f ||
            logo_y < TURTLE_MIN_Y - 0.5f || logo_y >= TURTLE_MAX_Y + 0.5f)
        {
            // Redraw turtle at original position and don't move
            turtle_draw();
            return false;  // Boundary error
        }
        turtle_x = new_x;
        turtle_y = new_y;
        break;

    case BOUNDARY_MODE_WINDOW:
        // Allow any position, no restrictions
        turtle_x = new_x;
        turtle_y = new_y;
        break;

    case BOUNDARY_MODE_WRAP:
        // Don't wrap yet - we need unwrapped coordinates for line drawing
        // so that pixel-by-pixel wrapping happens correctly in screen_gfx_line
        turtle_x = new_x;
        turtle_y = new_y;
        break;
    }

    // Draw line if pen is down
    if (turtle_pen_state == LOGO_PEN_DOWN)
    {
        screen_gfx_line(old_x, old_y, turtle_x, turtle_y, turtle_colour, false);
    }
    else if (turtle_pen_state == LOGO_PEN_ERASE)
    {
        screen_gfx_line(old_x, old_y, turtle_x, turtle_y, GFX_DEFAULT_BACKGROUND, false);
    }
    else if (turtle_pen_state == LOGO_PEN_REVERSE)
    {
        screen_gfx_line(old_x, old_y, turtle_x, turtle_y, turtle_colour, true);
    }
    // else if LOGO_PEN_UP: do not draw anything

    // Now wrap the turtle position after drawing (for wrap mode only)
    if (turtle_boundary_mode == BOUNDARY_MODE_WRAP)
    {
        // Robust wrapping formula that handles any value (positive or negative)
        // Uses the same approach as wrap_and_round() in screen.c
        turtle_x = turtle_x - floorf(turtle_x / SCREEN_WIDTH) * SCREEN_WIDTH;
        turtle_y = turtle_y - floorf(turtle_y / SCREEN_HEIGHT) * SCREEN_HEIGHT;
    }

    turtle_draw();
    screen_gfx_update();
    return true;  // Success
}

// Reset the turtle to the home position
static void turtle_home(void)
{
    screen_show_field();

    // Erase the turtle at the current position
    turtle_erase();

    // Reset the turtle to the home position
    turtle_x = TURTLE_HOME_X;
    turtle_y = TURTLE_HOME_Y;
    turtle_angle = TURTLE_DEFAULT_ANGLE;

    // Draw the turtle at the home position
    turtle_draw();

    screen_gfx_update();
}

// Set the turtle position to the specified coordinates
// Logo coordinates: origin at center, Y increases upward (north)
// Screen coordinates: origin at top-left, Y increases downward
static void turtle_set_position(float x, float y)
{
    screen_show_field();

    // Erase the turtle at current position
    turtle_erase();

    // Convert Logo coordinates to screen coordinates:
    // - X: Logo 0 -> Screen center (SCREEN_WIDTH/2)
    // - Y: Logo 0 -> Screen center, but Y axis is flipped (Logo Y up, Screen Y down)
    turtle_x = fmodf((x + SCREEN_WIDTH / 2) + SCREEN_WIDTH, SCREEN_WIDTH);
    turtle_y = fmodf((-y + SCREEN_HEIGHT / 2) + SCREEN_HEIGHT, SCREEN_HEIGHT);

    // Draw the turtle at the new position
    turtle_draw();
    screen_gfx_update();
}

// Get the current turtle position
// Returns Logo coordinates (origin at center, Y increases upward)
static void turtle_get_position(float *x, float *y)
{
    if (x)
    {
        *x = turtle_x - SCREEN_WIDTH / 2;
    }
    if (y)
    {
        // Convert screen Y to Logo Y (flip the axis)
        *y = -(turtle_y - SCREEN_HEIGHT / 2);
    }
}

// Set the turtle angle to the specified value
static void turtle_set_angle(float angle)
{
    screen_show_field();

    // Erase the turtle at current position before changing angle
    turtle_erase();

    turtle_angle = fmodf(angle, 360.0f); // Normalize the angle
    turtle_draw();

    screen_gfx_update();
}

// Get the current turtle angle
static float turtle_get_angle(void)
{
    return turtle_angle; // Return the current angle
}

// Set the turtle color to the specified value
static void turtle_set_colour(uint8_t colour)
{
    screen_show_field();
    
    // Erase turtle at current position before changing color
    turtle_erase();

    // Set the new turtle color
    turtle_colour = colour;

    // Draw the turtle at the current position with the new color
    turtle_draw();

    screen_gfx_update();
}

// Get the current turtle color
static uint8_t turtle_get_colour(void)
{
    return turtle_colour; // Return the current turtle color
}

static void turtle_set_bg_colour(uint8_t slot)
{
    // Set the background color
    uint16_t palette_value = lcd_get_palette_value(slot);
    lcd_set_palette_value(PALETTE_BG, palette_value);
    background_colour = slot;

    // The foreground colour will be the minimum or maxium shade in the hue for contrast
    if ((slot & 0x07) < 4)
    {
        slot |= 0x07;
    }
    else
    {
        slot &= ~0x07;
    }
    palette_value = lcd_get_palette_value(slot);
    lcd_set_palette_value(PALETTE_FG, palette_value);

    screen_gfx_update();
    screen_txt_update();
}

static uint8_t turtle_get_bg_colour(void)
{
    return background_colour;
}

// Set the pen state (down or up)
static void turtle_set_pen_state(LogoPen state)
{
    screen_show_field();
    turtle_pen_state = state; // Set the pen state
}

// Get the current pen state (down or up)
static LogoPen turtle_get_pen_state(void)
{
    return turtle_pen_state; // Return the current pen state
}

// Set the turtle visibility (visible or hidden)
static void turtle_set_visibility(bool visible)
{
    if (turtle_visible == visible)
    {
        return; // No change in visibility
    }

    screen_show_field();
    
    if (turtle_visible)
    {
        // Currently visible, becoming hidden - erase it
        turtle_erase();
    }
    
    turtle_visible = visible;
    
    if (turtle_visible)
    {
        // Now visible - draw it
        turtle_draw();
    }

    screen_gfx_update();
}

// Draw or erase the turtle based on visibility
static bool turtle_get_visibility(void)
{
    return turtle_visible;
}

static void turtle_dot(float x, float y)
{
    screen_show_field();
    screen_gfx_set_point(x, y, turtle_colour);
}

static bool turtle_dot_at(float x, float y)
{
    return screen_gfx_get_point(x, y) != GFX_DEFAULT_BACKGROUND;
}

// Fill enclosed area starting from turtle position
// Respects current pen state:
// - PEN_DOWN: fill with pen colour
// - PEN_ERASE: fill with background colour  
// - PEN_UP/PEN_REVERSE: do nothing
static void turtle_fill(void)
{
    uint8_t fill_colour;

    switch (turtle_pen_state)
    {
        case LOGO_PEN_DOWN:
            fill_colour = turtle_colour;
            break;
        case LOGO_PEN_ERASE:
            fill_colour = background_colour;
            break;
        default:
            // Pen up or reverse - don't fill
            return;
    }

    screen_show_field();

    // Temporarily hide the turtle so it doesn't interfere with the fill
    bool was_visible = turtle_visible;
    if (was_visible)
    {
        turtle_erase();
    }

    // Perform the fill from the turtle's current position
    screen_gfx_fill(turtle_x, turtle_y, fill_colour);

    // Restore turtle visibility
    if (was_visible)
    {
        turtle_draw();
    }

    screen_gfx_update();
}

static int turtle_gfx_save(const char *filename)
{
    return screen_gfx_save(filename);
}

static int turtle_gfx_load(const char *filename)
{
    int result = screen_gfx_load(filename);
    screen_gfx_update();
    return result;
}

static void turtle_set_palette(uint8_t slot, uint8_t r, uint8_t g, uint8_t b)
{
    lcd_set_palette_rgb(slot, r, g, b);
    screen_gfx_update();
    screen_txt_update();
}

static void turtle_get_palette(uint8_t slot, uint8_t *r, uint8_t *g, uint8_t *b)
{
    lcd_get_palette_rgb(slot, r, g, b);
}

static void turtle_restore_palette(void)
{
    lcd_restore_palette();
    turtle_set_bg_colour(background_colour);
    
    screen_gfx_update();
    screen_txt_update();
}

// Helper to check if turtle is within visible bounds (in screen coordinates)
static bool turtle_is_on_screen(void)
{
    return turtle_x >= 0 && turtle_x < SCREEN_WIDTH &&
           turtle_y >= 0 && turtle_y < SCREEN_HEIGHT;
}

// Set fence boundary mode - turtle stops at boundary with error
static void turtle_set_fence(void)
{
    // If switching from window mode and turtle is off-screen, send to home
    if (turtle_boundary_mode == BOUNDARY_MODE_WINDOW && !turtle_is_on_screen())
    {
        turtle_erase();  // Erase at old position (if any)
        turtle_x = TURTLE_HOME_X;
        turtle_y = TURTLE_HOME_Y;
        turtle_angle = TURTLE_DEFAULT_ANGLE;
        turtle_draw();  // Draw at home
        screen_gfx_update();
    }
    turtle_boundary_mode = BOUNDARY_MODE_FENCE;
    screen_gfx_set_boundary_mode(SCREEN_BOUNDARY_FENCE);
}

// Set window boundary mode - turtle can go off-screen (unbounded)
static void turtle_set_window(void)
{
    turtle_boundary_mode = BOUNDARY_MODE_WINDOW;
    screen_gfx_set_boundary_mode(SCREEN_BOUNDARY_WINDOW);
}

// Set wrap boundary mode - turtle wraps around edges
static void turtle_set_wrap(void)
{
    // If switching from window mode and turtle is off-screen, send to home
    if (turtle_boundary_mode == BOUNDARY_MODE_WINDOW && !turtle_is_on_screen())
    {
        turtle_erase();  // Erase at old position (if any)
        turtle_x = TURTLE_HOME_X;
        turtle_y = TURTLE_HOME_Y;
        turtle_angle = TURTLE_DEFAULT_ANGLE;
        turtle_draw();  // Draw at home
        screen_gfx_update();
    }
    turtle_boundary_mode = BOUNDARY_MODE_WRAP;
    screen_gfx_set_boundary_mode(SCREEN_BOUNDARY_WRAP);
}

// Set the current turtle shape (0-15)
static void turtle_set_shape_num(uint8_t shape_num)
{
    if (shape_num > 15)
        return;
    
    if (shape_num == turtle_current_shape)
        return;
    
    screen_show_field();
    
    // Erase turtle with old shape
    turtle_erase();
    
    // Change shape
    turtle_current_shape = shape_num;
    
    // Draw turtle with new shape
    turtle_draw();
    screen_gfx_update();
}

// Get the current turtle shape number
static uint8_t turtle_get_shape_num(void)
{
    return turtle_current_shape;
}

// Get shape data for shapes 1-15
// Returns 16 bytes representing 8 columns x 16 rows
// Each byte is one row, MSB = leftmost column
// Returns false if shape_num is 0 or > 15
static bool turtle_get_shape_data(uint8_t shape_num, uint8_t *data)
{
    if (shape_num == 0 || shape_num > 15 || data == NULL)
        return false;
    
    uint16_t *shape = turtle_shapes[shape_num - 1];
    
    // Convert internal 16-bit rows to user's 8-bit rows
    // Internal format has each pixel doubled horizontally
    // So we take every other bit from the 16-bit value
    // (bits 15, 13, 11, 9, 7, 5, 3, 1 when counting from the right, LSB = bit 0)
    for (int row = 0; row < 16; row++)
    {
        uint8_t result = 0;
        uint16_t internal_row = shape[row];
        
        // Extract bits 15, 13, 11, 9, 7, 5, 3, 1 (every other bit from MSB downward)
        // These represent the original 8 columns
        for (int col = 0; col < 8; col++)
        {
            if (internal_row & (0x8000 >> (col * 2)))
            {
                result |= (0x80 >> col);
            }
        }
        data[row] = result;
    }
    
    return true;
}

// Put shape data for shapes 1-15
// Takes 16 bytes representing 8 columns x 16 rows
// Each byte is one row, MSB = leftmost column
// Returns false if shape_num is 0 or > 15
static bool turtle_put_shape_data(uint8_t shape_num, const uint8_t *data)
{
    if (shape_num == 0 || shape_num > 15 || data == NULL)
        return false;
    
    uint16_t *shape = turtle_shapes[shape_num - 1];
    
    // Convert user's 8-bit rows to internal 16-bit rows
    // Double each pixel horizontally
    for (int row = 0; row < 16; row++)
    {
        uint16_t result = 0;
        uint8_t user_row = data[row];
        
        // Each bit in user_row becomes two bits in internal row
        for (int col = 0; col < 8; col++)
        {
            if (user_row & (0x80 >> col))
            {
                // Set two adjacent bits
                result |= (0xC000 >> (col * 2));
            }
        }
        shape[row] = result;
    }
    
    // If we're currently using this shape, redraw
    if (turtle_current_shape == shape_num && turtle_visible)
    {
        screen_show_field();
        turtle_erase();
        turtle_draw();
        screen_gfx_update();
    }
    
    return true;
}

static const LogoConsoleTurtle picocalc_turtle_ops = {
    .clear = turtle_clearscreen,
    .draw = turtle_draw,
    .move = turtle_move,
    .home = turtle_home,
    .set_position = turtle_set_position,
    .get_position = turtle_get_position,
    .set_heading = turtle_set_angle,
    .get_heading = turtle_get_angle,
    .set_pen_colour = turtle_set_colour,
    .get_pen_colour = turtle_get_colour,
    .set_bg_colour = turtle_set_bg_colour,
    .get_bg_colour = turtle_get_bg_colour,
    .set_pen_state = turtle_set_pen_state,
    .get_pen_state = turtle_get_pen_state,
    .set_visible = turtle_set_visibility,
    .get_visible = turtle_get_visibility,
    .dot = turtle_dot,
    .dot_at = turtle_dot_at,
    .fill = turtle_fill,
    .set_fence = turtle_set_fence,
    .set_window = turtle_set_window,
    .set_wrap = turtle_set_wrap,
    .gfx_save = turtle_gfx_save,
    .gfx_load = turtle_gfx_load,
    .set_palette = turtle_set_palette,
    .get_palette = turtle_get_palette,
    .restore_palette = turtle_restore_palette,
    .set_shape = turtle_set_shape_num,
    .get_shape = turtle_get_shape_num,
    .get_shape_data = turtle_get_shape_data,
    .put_shape_data = turtle_put_shape_data,
};

//
// LogoConsole API
//

LogoConsole *logo_picocalc_console_create(void)
{
    LogoConsole *console = (LogoConsole *)malloc(sizeof(LogoConsole));

    if (!console)
    {
        return NULL;
    }

    logo_console_init(console, &picocalc_input_ops, &picocalc_output_ops, NULL);
    console->screen = &picocalc_screen_ops;
    console->text = &picocalc_text_ops;
    console->turtle = &picocalc_turtle_ops;
    console->editor = picocalc_editor_get_ops();

    turtle_set_bg_colour(74); // Set default background color
    screen_gfx_clear();
    screen_txt_clear();
    turtle_draw();
    screen_set_mode(SCREEN_MODE_TXT); // Start in split screen mode

    return console;
}

void logo_picocalc_console_destroy(LogoConsole *console)
{
    if (!console)
    {
        return;
    }

    free(console->context);
    free(console);
}
