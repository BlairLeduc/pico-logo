//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements a PicoCalc device that uses standard input and output.
//

#include "picocalc_console.h"
#include "devices/console.h"
#include "devices/stream.h"
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
static LogoPen turtle_pen_state = LOGO_PEN_DOWN;   // Pen state for graphics
static bool turtle_visible = TURTLE_DEFAULT_VISIBILITY; // Turtle visibility state for graphics

//
//  Stream operations for keyboard input
//

static int input_read_char(LogoStream *stream)
{
    (void)stream;
    return keyboard_get_key();
}

static int input_read_chars(LogoStream *stream, char *buffer, int count)
{
    (void)stream;
    return picocalc_read_line(buffer, count);
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

//  Draw the turtle at the current position
static void turtle_draw()
{
    // Convert the angle to radians
    float radians = turtle_angle * (M_PI / 180.0f);

    // Calculate the vertices of the turtle triangle
    float half_base = 4.0f; // Half the base width of the turtle triangle
    float height = 12.0f;   // Height of the turtle triangle

    // Calculate the three vertices of the triangle
    float x1 = turtle_x + half_base * cosf(radians);
    float y1 = turtle_y + half_base * sinf(radians);
    float x2 = turtle_x - half_base * cosf(radians);
    float y2 = turtle_y - half_base * sinf(radians);
    float x3 = turtle_x + height * sinf(radians);
    float y3 = turtle_y - height * cosf(radians);

    // Draw the triangle using lines
    screen_gfx_line(x1, y1, x2, y2, turtle_colour, true);
    screen_gfx_line(x2, y2, x3, y3, turtle_colour, true);
    screen_gfx_line(x3, y3, x1, y1, turtle_colour, true);
}

// Clear the graphics buffer and reset the turtle to the home position
static void turtle_clearscreen(void)
{
    screen_show_field();

    // Clear the graphics buffer
    screen_gfx_clear();

    // Reset the turtle to the home position
    turtle_x = TURTLE_HOME_X;
    turtle_y = TURTLE_HOME_Y;
    turtle_angle = TURTLE_DEFAULT_ANGLE;

    // Draw the turtle at the home position
    turtle_draw();

    // Update the graphics display
    screen_gfx_update();
}

// Move the turtle forward or backward by the specified distance
static void turtle_move(float distance)
{
    screen_show_field();

    float x = turtle_x;
    float y = turtle_y;

    turtle_draw();

    // Move the turtle forward by the specified distance
    turtle_x += distance * sinf(turtle_angle * (M_PI / 180.0f));
    turtle_y -= distance * cosf(turtle_angle * (M_PI / 180.0f));

    // Draw the turtle at the new position

    if (turtle_pen_state == LOGO_PEN_DOWN)
    {
        // Draw a line from the old position to the new position
        screen_gfx_line(x, y, turtle_x, turtle_y, turtle_colour, false);
    }
    else if (turtle_pen_state == LOGO_PEN_ERASE)
    {
        // Erase a line from the old position to the new position
        screen_gfx_line(x, y, turtle_x, turtle_y, GFX_DEFAULT_BACKGROUND, false);
    }
    else if (turtle_pen_state == LOGO_PEN_REVERSE)
    {
        // Reverse the pixels along the line from the old position to the new position
        screen_gfx_line(x, y, turtle_x, turtle_y, turtle_colour, true);
    }
    // else if LOGO_PEN_UP: do not draw anything

    turtle_draw();

    // Ensure the turtle stays within bounds
    turtle_x = fmodf(turtle_x + SCREEN_WIDTH, SCREEN_WIDTH);
    turtle_y = fmodf(turtle_y + SCREEN_HEIGHT, SCREEN_HEIGHT);

    screen_gfx_update();
}

// Reset the turtle to the home position
static void turtle_home(void)
{
    screen_show_field();

    // Erase the turtle at the current position
    turtle_draw();

    // Reset the turtle to the home position
    turtle_x = TURTLE_HOME_X;
    turtle_y = TURTLE_HOME_Y;
    turtle_angle = TURTLE_DEFAULT_ANGLE;

    // Draw the turtle at the home position
    turtle_draw();

    screen_gfx_update();
}

// Set the turtle position to the specified coordinates
static void turtle_set_position(float x, float y)
{
    screen_show_field();

    // Draw the current turtle position before moving
    turtle_draw();

    // Set the new position
    turtle_x = fmodf((x + SCREEN_WIDTH / 2) + SCREEN_WIDTH, SCREEN_WIDTH);
    turtle_y = fmodf((y + SCREEN_HEIGHT / 2) + SCREEN_HEIGHT, SCREEN_HEIGHT);

    // Draw the turtle at the new position
    turtle_draw();
    screen_gfx_update();
}

// Get the current turtle position
static void turtle_get_position(float *x, float *y)
{
    if (x)
    {
        *x = turtle_x - SCREEN_WIDTH / 2;
    }
    if (y)
    {
        *y = turtle_y - SCREEN_HEIGHT / 2;
    }
}

// Set the turtle angle to the specified value
static void turtle_set_angle(float angle)
{
    screen_show_field();

    // Draw the current turtle position before changing angle
    turtle_draw();

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
    // Draw the current turtle position before changing color
    turtle_draw(turtle_x, turtle_y, turtle_angle);

    // Set the new turtle color
    turtle_colour = colour;

    // Draw the turtle at the current position with the new color
    turtle_draw(turtle_x, turtle_y, turtle_angle);

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
    turtle_draw(); // XOR the current turtle to draw/erase it
    turtle_visible = visible;

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
    .fill = NULL,            // Not implemented
    .set_fence = NULL,       // Not implemented
    .set_window = NULL,      // Not implemented
    .set_wrap = NULL,        // Not implemented
    .gfx_save = turtle_gfx_save,
    .gfx_load = turtle_gfx_load,
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
