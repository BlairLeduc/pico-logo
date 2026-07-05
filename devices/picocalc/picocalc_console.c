//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Implements a PicoCalc device that uses standard input and output.
//

#include "picocalc_console.h"
#include "core/limits.h"
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

// Rendered turtle raster sizing for the screen compositor. Shape 0 (the
// line-drawn triangle) rasterizes into 24x24 to accommodate rotation;
// bitmap shapes 1-15 use 16x16.
#define SHAPE0_RASTER_SIZE 24
#define BITMAP_RASTER_SIZE 16

// Per-turtle state. The stateful console ops act on the turtle selected
// by turtle_op_select() (the `cur` pointer); turtle `n` maps to sprite
// id `n` in the compositor, so lower-numbered turtles composite on top.
// The raster is the rendered glyph mask the compositor overlays at blit
// time (the glyph is never written into the canvas); it is rebuilt
// lazily when the shape, the heading (shape 0 only), or the shape data
// changes.
typedef struct
{
    float x, y;             // Position in screen coordinates
    float angle;            // Heading in degrees (0 = north)
    uint8_t colour;         // Pen colour
    LogoPen pen_state;
    bool visible;
    uint8_t shape;          // Current shape number (0-15)
    uint8_t raster[SHAPE0_RASTER_SIZE * SHAPE0_RASTER_SIZE];
    bool raster_valid;
    uint8_t raster_shape;   // shape the raster was built for
    float raster_angle;     // heading it was built at (shape 0)
} PicoTurtle;

static PicoTurtle picoturtles[MAX_TURTLES];
static uint8_t current_turtle = 0;
static PicoTurtle *cur = &picoturtles[0];

_Static_assert(MAX_TURTLES <= SCREEN_MAX_SPRITES,
               "each turtle needs a compositor sprite slot");

// Route the stateful turtle ops to turtle n (console select op)
static void turtle_op_select(uint8_t n)
{
    if (n < MAX_TURTLES)
    {
        current_turtle = n;
        cur = &picoturtles[n];
    }
}

// Boot state: every turtle at home facing north, pen down, default
// colour and shape; only turtle 0 visible.
static void turtles_init(void)
{
    for (int i = 0; i < MAX_TURTLES; i++)
    {
        PicoTurtle *t = &picoturtles[i];
        t->x = TURTLE_HOME_X;
        t->y = TURTLE_HOME_Y;
        t->angle = TURTLE_DEFAULT_ANGLE;
        t->colour = TURTLE_DEFAULT_COLOUR;
        t->pen_state = LOGO_PEN_DOWN;
        t->visible = (i == 0) && TURTLE_DEFAULT_VISIBILITY;
        t->shape = 0;
        t->raster_valid = false;
    }
    current_turtle = 0;
    cur = &picoturtles[0];
}

uint8_t background_colour = GFX_DEFAULT_BACKGROUND;     // Background color for graphics

// Shape system (shared by all turtles)
// Shapes 1-15 stored internally as 16-bit rows (doubled from user's 8-bit)
// Shape 0 is the line-drawn turtle and doesn't use this storage
static uint16_t turtle_shapes[15][16];  // shapes[0] = shape 1, shapes[14] = shape 15

// Boundary mode constants
typedef enum {
    BOUNDARY_MODE_FENCE,   // Turtle stops at boundary (error if hits edge)
    BOUNDARY_MODE_WINDOW,  // Turtle can go off-screen (unbounded)
    BOUNDARY_MODE_WRAP     // Turtle wraps around edges (default)
} BoundaryMode;

static BoundaryMode turtle_boundary_mode = BOUNDARY_MODE_WRAP;  // Default to wrap mode

typedef struct PicoCalcConsoleContext
{
    LogoConsole *console;
} PicoCalcConsoleContext;

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
    PicoCalcConsoleContext *ctx = (PicoCalcConsoleContext *)stream->context;
    int initial_depth = 0;

    if (ctx && ctx->console)
    {
        initial_depth = ctx->console->input_syntax_depth;
    }

    return picocalc_read_line(buffer, size, initial_depth);
}

static bool input_can_read(LogoStream *stream)
{
    (void)stream;
    
    // Set input_active so keyboard_poll buffers F1/F2/F3 instead of switching modes
    input_active = true;
    
    // Consume and handle any mode-switching keys (F1/F2/F3)
    // so key? only returns true for keys that readchar will actually return
    while (keyboard_key_available())
    {
        char ch = keyboard_peek_key();
        if (screen_handle_mode_key(ch))
        {
            // Consume the mode-switching key
            keyboard_get_key();
            continue;
        }
        // Found a non-mode-switching key
        input_active = false;
        return true;
    }
    input_active = false;
    return false;
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
//  Error output stream — writes in error color (red) then restores default
//

static void error_output_write(LogoStream *stream, const char *text)
{
    screen_txt_set_foreground(TXT_ERROR);
    screen_txt_puts(text);
    screen_txt_set_foreground(TXT_DEFAULT_FOREGROUND);
}

static void error_output_flush(LogoStream *stream)
{
    screen_txt_update();
}

static const LogoStreamOps picocalc_error_output_ops = {
    .read_char = NULL,
    .read_chars = NULL,
    .read_line = NULL,
    .can_read = NULL,
    .write = error_output_write,
    .flush = error_output_flush,
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
    .set_refresh_auto = screen_gfx_set_refresh_auto,
    .get_refresh_auto = screen_gfx_get_refresh_auto,
    .refresh_now = screen_gfx_present,
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

static void text_set_foreground(uint8_t index)
{
    screen_txt_set_foreground(index);
}

static uint8_t text_get_foreground(void)
{
    return screen_txt_get_foreground();
}

static void text_set_background(uint8_t index)
{
    screen_txt_set_background(index);
}

static uint8_t text_get_background(void)
{
    return screen_txt_get_background();
}

static const LogoConsoleText picocalc_text_ops = {
    .clear = text_clear,
    .set_cursor = text_set_cursor,
    .get_cursor = text_get_cursor,
    .set_foreground = text_set_foreground,
    .get_foreground = text_get_foreground,
    .set_background = text_set_background,
    .get_background = text_get_background,
};

//
//  Turtle graphics functions
//

// Helper: check if turtle is visible on screen (for window mode)
static bool turtle_should_draw(void)
{
    if (!cur->visible)
        return false;
    
    // In window mode, don't draw if turtle center is off-screen
    if (turtle_boundary_mode == BOUNDARY_MODE_WINDOW)
    {
        if (cur->x < 0 || cur->x >= SCREEN_WIDTH ||
            cur->y < 0 || cur->y >= SCREEN_HEIGHT)
        {
            return false;
        }
    }
    return true;
}

// Plot a line into the turtle raster with the same integer Bresenham as
// screen_gfx_line, clipped to the raster bounds.
static void raster_line(uint8_t *raster, int size, float fx1, float fy1,
                        float fx2, float fy2)
{
    int x1 = (int)(fx1 + 0.5f);
    int y1 = (int)(fy1 + 0.5f);
    int x2 = (int)(fx2 + 0.5f);
    int y2 = (int)(fy2 + 0.5f);

    int dx = x2 - x1;
    int dy = y2 - y1;
    int sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    int sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
    dx = (dx < 0) ? -dx : dx;
    dy = (dy < 0) ? -dy : dy;

    int x = x1, y = y1;

    #define RASTER_PLOT(px, py) do { \
        if ((px) >= 0 && (px) < size && (py) >= 0 && (py) < size) \
            raster[(py) * size + (px)] = 1; \
    } while (0)

    if (dx >= dy)
    {
        int err = 2 * dy - dx;
        for (int i = 0; i <= dx; ++i)
        {
            RASTER_PLOT(x, y);
            if (err > 0) { y += sy; err -= 2 * dx; }
            err += 2 * dy;
            x += sx;
        }
    }
    else
    {
        int err = 2 * dx - dy;
        for (int i = 0; i <= dy; ++i)
        {
            RASTER_PLOT(x, y);
            if (err > 0) { x += sx; err -= 2 * dy; }
            err += 2 * dx;
            y += sy;
        }
    }

    #undef RASTER_PLOT
}

// Rebuild the turtle raster if the shape, shape data, or (for shape 0)
// the heading changed since it was last built.
static void turtle_update_raster(void)
{
    if (cur->raster_valid && cur->raster_shape == cur->shape &&
        (cur->shape != 0 || cur->raster_angle == cur->angle))
    {
        return;
    }

    if (cur->shape == 0)
    {
        // Shape 0: the line-drawn turtle triangle, rotated to the heading.
        // Base width 6 (half_base 3), height 7, position at centre of base;
        // fits in 24x24 with the base centre at the raster centre.
        memset(cur->raster, 0, SHAPE0_RASTER_SIZE * SHAPE0_RASTER_SIZE);

        float radians = cur->angle * (float)(M_PI / 180.0);
        float sin_a = sinf(radians);
        float cos_a = cosf(radians);
        float half_base = 3.0f;
        float height = 7.0f;
        float cx = SHAPE0_RASTER_SIZE / 2.0f;
        float cy = SHAPE0_RASTER_SIZE / 2.0f;

        // Base vertices perpendicular to the heading; tip along the heading
        float x1 = cx + half_base * cos_a, y1 = cy + half_base * sin_a;
        float x2 = cx - half_base * cos_a, y2 = cy - half_base * sin_a;
        float x3 = cx + height * sin_a,    y3 = cy - height * cos_a;

        raster_line(cur->raster, SHAPE0_RASTER_SIZE, x1, y1, x2, y2);
        raster_line(cur->raster, SHAPE0_RASTER_SIZE, x2, y2, x3, y3);
        raster_line(cur->raster, SHAPE0_RASTER_SIZE, x3, y3, x1, y1);
    }
    else
    {
        // Shapes 1-15: 16x16 bitmap (rows doubled from the user's 8 bits)
        memset(cur->raster, 0, BITMAP_RASTER_SIZE * BITMAP_RASTER_SIZE);

        const uint16_t *shape = turtle_shapes[cur->shape - 1];
        for (int row = 0; row < 16; row++)
        {
            uint16_t row_bits = shape[row];
            if (row_bits == 0) continue;
            for (int col = 0; col < 16; col++)
            {
                if (row_bits & (0x8000 >> col))
                {
                    cur->raster[row * BITMAP_RASTER_SIZE + col] = 1;
                }
            }
        }
    }

    cur->raster_valid = true;
    cur->raster_shape = cur->shape;
    cur->raster_angle = cur->angle;
}

// Show the turtle sprite at the current position (or hide it when the
// turtle is hidden / off-screen in window mode). The compositor takes it
// from here — nothing is written into the canvas.
static void turtle_draw(void)
{
    if (!turtle_should_draw())
    {
        screen_sprite_hide(current_turtle);
        return;
    }

    turtle_update_raster();

    ScreenSprite sprite = {
        .visible = true,
        .colour = cur->colour,
        .mask = cur->raster,
    };

    if (cur->shape == 0)
    {
        // Centred on the turtle position
        sprite.w = SHAPE0_RASTER_SIZE;
        sprite.h = SHAPE0_RASTER_SIZE;
        sprite.x = (int16_t)((int)(cur->x + 0.5f) - SHAPE0_RASTER_SIZE / 2);
        sprite.y = (int16_t)((int)(cur->y + 0.5f) - SHAPE0_RASTER_SIZE / 2);
    }
    else
    {
        // Centred horizontally, bottom row at the turtle position
        sprite.w = BITMAP_RASTER_SIZE;
        sprite.h = BITMAP_RASTER_SIZE;
        sprite.x = (int16_t)((int)cur->x - BITMAP_RASTER_SIZE / 2);
        sprite.y = (int16_t)((int)cur->y - (BITMAP_RASTER_SIZE - 1));
    }

    screen_sprite_set(current_turtle, &sprite);
}

// Clear the graphics buffer and reset the turtle to the home position
static void turtle_clearscreen(void)
{
    screen_show_field();

    // Clear the graphics buffer
    screen_gfx_clear();

    // Draw the turtle at its current position
    turtle_draw();

    // Update the graphics display
    screen_gfx_update();
}

// Move the turtle forward or backward by the specified distance
// Returns true on success, false if boundary error in fence mode
static bool turtle_move(float distance)
{
    screen_show_field();

    float old_x = cur->x;
    float old_y = cur->y;

    // Calculate new position in screen coordinates
    float new_x = cur->x + distance * sinf(cur->angle * (M_PI / 180.0f));
    float new_y = cur->y - distance * cosf(cur->angle * (M_PI / 180.0f));

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
        cur->x = new_x;
        cur->y = new_y;
        break;

    case BOUNDARY_MODE_WINDOW:
        // Allow any position, no restrictions
        cur->x = new_x;
        cur->y = new_y;
        break;

    case BOUNDARY_MODE_WRAP:
        // Don't wrap yet - we need unwrapped coordinates for line drawing
        // so that pixel-by-pixel wrapping happens correctly in screen_gfx_line
        cur->x = new_x;
        cur->y = new_y;
        break;
    }

    // Draw line if pen is down
    if (cur->pen_state == LOGO_PEN_DOWN)
    {
        screen_gfx_line(old_x, old_y, cur->x, cur->y, cur->colour, false);
    }
    else if (cur->pen_state == LOGO_PEN_ERASE)
    {
        screen_gfx_line(old_x, old_y, cur->x, cur->y, GFX_DEFAULT_BACKGROUND, false);
    }
    else if (cur->pen_state == LOGO_PEN_REVERSE)
    {
        screen_gfx_line(old_x, old_y, cur->x, cur->y, cur->colour, true);
    }
    // else if LOGO_PEN_UP: do not draw anything

    // Now wrap the turtle position after drawing (for wrap mode only)
    if (turtle_boundary_mode == BOUNDARY_MODE_WRAP)
    {
        // Robust wrapping formula that handles any value (positive or negative)
        // Uses the same approach as wrap_and_round() in screen.c
        cur->x = cur->x - floorf(cur->x / SCREEN_WIDTH) * SCREEN_WIDTH;
        cur->y = cur->y - floorf(cur->y / SCREEN_HEIGHT) * SCREEN_HEIGHT;
    }

    turtle_draw();
    screen_gfx_update();
    return true;  // Success
}

// Reset the turtle to the home position
static void turtle_home(void)
{
    screen_show_field();

    float old_x = cur->x;
    float old_y = cur->y;

    // Reset the turtle to the home position
    cur->x = TURTLE_HOME_X;
    cur->y = TURTLE_HOME_Y;
    cur->angle = TURTLE_DEFAULT_ANGLE;

    // Draw line if pen is down
    if (cur->pen_state == LOGO_PEN_DOWN)
    {
        screen_gfx_line(old_x, old_y, cur->x, cur->y, cur->colour, false);
    }
    else if (cur->pen_state == LOGO_PEN_ERASE)
    {
        screen_gfx_line(old_x, old_y, cur->x, cur->y, GFX_DEFAULT_BACKGROUND, false);
    }
    else if (cur->pen_state == LOGO_PEN_REVERSE)
    {
        screen_gfx_line(old_x, old_y, cur->x, cur->y, cur->colour, true);
    }
    // else if LOGO_PEN_UP: do not draw anything

    // Draw the turtle at the home position
    turtle_draw();

    screen_gfx_update();
}

// Set the turtle position to the specified coordinates
// Logo coordinates: origin at center, Y increases upward (north)
// Screen coordinates: origin at top-left, Y increases downward
// Returns true on success, false if boundary error (fence mode)
static bool turtle_set_position(float x, float y)
{
    screen_show_field();

    float old_x = cur->x;
    float old_y = cur->y;

    // Convert Logo coordinates to screen coordinates:
    // - X: Logo 0 -> Screen center (SCREEN_WIDTH/2)
    // - Y: Logo 0 -> Screen center, but Y axis is flipped (Logo Y up, Screen Y down)
    float new_x = x + SCREEN_WIDTH / 2;
    float new_y = -y + SCREEN_HEIGHT / 2;

    // Handle boundary modes
    switch (turtle_boundary_mode)
    {
    case BOUNDARY_MODE_FENCE:
        // Check if new position is out of bounds (in Logo coordinates)
        if (x < TURTLE_MIN_X - 0.5f || x >= TURTLE_MAX_X + 0.5f ||
            y < TURTLE_MIN_Y - 0.5f || y >= TURTLE_MAX_Y + 0.5f)
        {
            // Redraw turtle at original position and don't move
            turtle_draw();
            return false;  // Boundary error
        }
        cur->x = new_x;
        cur->y = new_y;
        break;

    case BOUNDARY_MODE_WINDOW:
        // Allow any position, no restrictions
        cur->x = new_x;
        cur->y = new_y;
        break;

    case BOUNDARY_MODE_WRAP:
        // Don't wrap yet - we need unwrapped coordinates for line drawing
        // so that pixel-by-pixel wrapping happens correctly in screen_gfx_line
        cur->x = new_x;
        cur->y = new_y;
        break;
    }

    // Draw line if pen is down
    if (cur->pen_state == LOGO_PEN_DOWN)
    {
        screen_gfx_line(old_x, old_y, cur->x, cur->y, cur->colour, false);
    }
    else if (cur->pen_state == LOGO_PEN_ERASE)
    {
        screen_gfx_line(old_x, old_y, cur->x, cur->y, GFX_DEFAULT_BACKGROUND, false);
    }
    else if (cur->pen_state == LOGO_PEN_REVERSE)
    {
        screen_gfx_line(old_x, old_y, cur->x, cur->y, cur->colour, true);
    }
    // else if LOGO_PEN_UP: do not draw anything

    // Now wrap the turtle position after drawing (for wrap mode only)
    if (turtle_boundary_mode == BOUNDARY_MODE_WRAP)
    {
        cur->x = cur->x - floorf(cur->x / SCREEN_WIDTH) * SCREEN_WIDTH;
        cur->y = cur->y - floorf(cur->y / SCREEN_HEIGHT) * SCREEN_HEIGHT;
    }

    // Draw the turtle at the new position
    turtle_draw();
    screen_gfx_update();
    return true;  // Success
}

// Get the current turtle position
// Returns Logo coordinates (origin at center, Y increases upward)
static void turtle_get_position(float *x, float *y)
{
    if (x)
    {
        *x = cur->x - SCREEN_WIDTH / 2;
    }
    if (y)
    {
        // Convert screen Y to Logo Y (flip the axis)
        *y = -(cur->y - SCREEN_HEIGHT / 2);
    }
}

// Set the turtle angle to the specified value
static void turtle_set_angle(float angle)
{
    screen_show_field();

    // Normalize the angle to [0, 360)
    cur->angle = fmodf(angle, 360.0f);
    if (cur->angle < 0.0f)
    {
        cur->angle += 360.0f;
    }
    turtle_draw();

    screen_gfx_update();
}

// Get the current turtle angle
static float turtle_get_angle(void)
{
    return cur->angle; // Return the current angle
}

// Set the turtle color to the specified value
static void turtle_set_colour(uint8_t colour)
{
    screen_show_field();

    // Set the new turtle color
    cur->colour = colour;

    // Draw the turtle at the current position with the new color
    turtle_draw();

    screen_gfx_update();
}

// Get the current turtle color
static uint8_t turtle_get_colour(void)
{
    return cur->colour; // Return the current turtle color
}

static void turtle_set_bg_colour(uint8_t slot)
{
    // Set the background color
    uint16_t palette_value = lcd_get_palette_value(slot);
    lcd_set_palette_value(PALETTE_BG, palette_value);
    background_colour = slot;

    screen_gfx_mark_all_dirty();  // Palette changed — entire buffer needs re-blit
    screen_gfx_update();
}

static uint8_t turtle_get_bg_colour(void)
{
    return background_colour;
}

// Set the pen state (down or up)
static void turtle_set_pen_state(LogoPen state)
{
    screen_show_field();
    cur->pen_state = state; // Set the pen state
}

// Get the current pen state (down or up)
static LogoPen turtle_get_pen_state(void)
{
    return cur->pen_state; // Return the current pen state
}

// Set the turtle visibility (visible or hidden)
static void turtle_set_visibility(bool visible)
{
    if (cur->visible == visible)
    {
        return; // No change in visibility
    }

    screen_show_field();

    cur->visible = visible;

    // turtle_draw shows or hides the sprite to match the new visibility
    turtle_draw();

    screen_gfx_update();
}

// Draw or erase the turtle based on visibility
static bool turtle_get_visibility(void)
{
    return cur->visible;
}

static void turtle_dot(float x, float y)
{
    screen_show_field();
    // Convert Logo coordinates to screen coordinates:
    // - X: Logo 0 -> Screen center (SCREEN_WIDTH/2)
    // - Y: Logo 0 -> Screen center, but Y axis is flipped (Logo Y up, Screen Y down)
    float screen_x = x + SCREEN_WIDTH / 2;
    float screen_y = -y + SCREEN_HEIGHT / 2;
    screen_gfx_set_point(screen_x, screen_y, cur->colour);
    screen_gfx_update();
}

static bool turtle_dot_at(float x, float y)
{
    // Convert Logo coordinates to screen coordinates:
    // - X: Logo 0 -> Screen center (SCREEN_WIDTH/2)
    // - Y: Logo 0 -> Screen center, but Y axis is flipped (Logo Y up, Screen Y down)
    float screen_x = x + SCREEN_WIDTH / 2;
    float screen_y = -y + SCREEN_HEIGHT / 2;
    return screen_gfx_get_point(screen_x, screen_y) != GFX_DEFAULT_BACKGROUND;
}

// Fill enclosed area starting from turtle position
// Respects current pen state:
// - PEN_DOWN: fill with pen colour
// - PEN_ERASE: fill with background colour  
// - PEN_UP/PEN_REVERSE: do nothing
static void turtle_fill(void)
{
    uint8_t fill_colour;

    switch (cur->pen_state)
    {
        case LOGO_PEN_DOWN:
            fill_colour = cur->colour;
            break;
        case LOGO_PEN_ERASE:
            fill_colour = background_colour;
            break;
        default:
            // Pen up or reverse - don't fill
            return;
    }

    screen_show_field();

    // The turtle is composited over the canvas, never written into it,
    // so the fill sees a clean canvas and needs no hide/restore dance.
    screen_gfx_fill(cur->x, cur->y, fill_colour);

    screen_gfx_update();
}

static int turtle_gfx_save(LogoStream *out)
{
    return screen_gfx_save(out);
}

static int turtle_gfx_load(LogoStream *in)
{
    int result = screen_gfx_load(in);
    screen_gfx_update();
    return result;
}

static void turtle_set_palette(uint8_t slot, uint8_t r, uint8_t g, uint8_t b)
{
    lcd_set_palette_rgb(slot, r, g, b);
    screen_gfx_mark_all_dirty();  // Palette changed — entire buffer needs re-blit
    screen_gfx_update();
    screen_txt_mark_all_dirty();  // Palette changed — text colours need refresh
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
    
    screen_gfx_mark_all_dirty();  // Palette changed — entire buffer needs re-blit
    screen_gfx_update();
    screen_txt_mark_all_dirty();  // Palette changed — text colours need refresh
    screen_txt_update();
}

// Helper to check if turtle is within visible bounds (in screen coordinates)
static bool turtle_is_on_screen(void)
{
    return cur->x >= 0 && cur->x < SCREEN_WIDTH &&
           cur->y >= 0 && cur->y < SCREEN_HEIGHT;
}

// Set fence boundary mode - turtle stops at boundary with error
static void turtle_set_fence(void)
{
    // If switching from window mode and turtle is off-screen, send to home
    if (turtle_boundary_mode == BOUNDARY_MODE_WINDOW && !turtle_is_on_screen())
    {
        cur->x = TURTLE_HOME_X;
        cur->y = TURTLE_HOME_Y;
        cur->angle = TURTLE_DEFAULT_ANGLE;
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
        cur->x = TURTLE_HOME_X;
        cur->y = TURTLE_HOME_Y;
        cur->angle = TURTLE_DEFAULT_ANGLE;
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
    
    if (shape_num == cur->shape)
        return;
    
    screen_show_field();

    // Change shape; the raster cache notices the shape mismatch
    cur->shape = shape_num;

    // Draw turtle with new shape
    turtle_draw();
    screen_gfx_update();
}

// Get the current turtle shape number
static uint8_t turtle_get_shape_num(void)
{
    return cur->shape;
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
    
    // Rebuild the raster of every turtle wearing this shape and redraw
    // the visible ones
    uint8_t saved_turtle = current_turtle;
    for (uint8_t i = 0; i < MAX_TURTLES; i++)
    {
        if (picoturtles[i].shape != shape_num)
        {
            continue;
        }
        picoturtles[i].raster_valid = false;
        if (picoturtles[i].visible)
        {
            turtle_op_select(i);
            screen_show_field();
            turtle_draw();
        }
    }
    turtle_op_select(saved_turtle);
    screen_gfx_update();

    return true;
}

static const LogoConsoleTurtle picocalc_turtle_ops = {
    .select = turtle_op_select,
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
    PicoCalcConsoleContext *ctx = (PicoCalcConsoleContext *)malloc(sizeof(PicoCalcConsoleContext));
    LogoConsole *console = (LogoConsole *)malloc(sizeof(LogoConsole));

    if (!ctx || !console)
    {
        free(ctx);
        free(console);
        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));
    logo_console_init(console, &picocalc_input_ops, &picocalc_output_ops, ctx);
    ctx->console = console;
    logo_stream_init(&console->error_output,
                     LOGO_STREAM_CONSOLE_OUTPUT,
                     &picocalc_error_output_ops,
                     ctx,
                     "error");
    console->screen = &picocalc_screen_ops;
    console->text = &picocalc_text_ops;
    console->turtle = &picocalc_turtle_ops;
    console->editor = picocalc_editor_get_ops();

    turtles_init();
    turtle_set_bg_colour(0); // Set default background color (black)
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
