//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Defines the LogoConsole interface for physical keyboard/screen devices.
//  A console provides streams for keyboard input and screen output,
//  plus optional turtle graphics, text cursor, and screen mode support.
//

#pragma once

#include "stream.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum LogoPen
    {
        LOGO_PEN_UP,
        LOGO_PEN_DOWN,
        LOGO_PEN_ERASE,
        LOGO_PEN_REVERSE,
    } LogoPen;

    //
    // Turtle graphics operations (optional)
    // These are available on devices with graphics capability.
    //
    typedef struct LogoConsoleTurtle
    {
        // Clear the graphics screen
        void (*clear)(void);

        // Redraw the turtle
        void (*draw)(void);

        // Move turtle forward/backward by distance (draws if pen is down)
        // Returns true on success, false if boundary error (fence mode)
        bool (*move)(float distance);

        // Move turtle to home position (0,0) heading north
        void (*home)(void);

        // Absolute position control
        void (*set_position)(float x, float y);
        void (*get_position)(float *x, float *y);

        // Heading control (0 = north, 90 = east)
        void (*set_heading)(float angle);
        float (*get_heading)(void);

        // Pen color
        void (*set_pen_colour)(uint8_t colour);
        uint8_t (*get_pen_colour)(void);

        // Background color
        void (*set_bg_colour)(uint8_t colour);
        uint8_t (*get_bg_colour)(void);

        // Pen state
        void (*set_pen_state)(LogoPen state);
        LogoPen (*get_pen_state)(void);

        // Turtle visibility
        void (*set_visible)(bool visible);
        bool (*get_visible)(void);

        // Draw a dot at position without moving turtle
        void (*dot)(float x, float y);

        // Check if there's a dot at position
        bool (*dot_at)(float x, float y);

        // Fill enclosed area with current pen color
        void (*fill)(void);

        // Boundary modes
        void (*set_fence)(void);    // Turtle stops at boundary
        void (*set_window)(void);   // Turtle can go off-screen
        void (*set_wrap)(void);     // Turtle wraps around

        // Save graphics screen to file
        // Returns 0 on success, errno on failure
        int (*gfx_save)(const char *filename);

        // Load graphics screen from file
        // Returns 0 on success, errno on failure
        int (*gfx_load)(const char *filename);

        // Palette functions
        // Set palette slot to RGB565 value (from 24-bit RGB components)
        void (*set_palette)(uint8_t slot, uint8_t r, uint8_t g, uint8_t b);

        // Get palette slot RGB components (from RGB565 value)
        void (*get_palette)(uint8_t slot, uint8_t *r, uint8_t *g, uint8_t *b);

        // Restore default palette (slots 0-127)
        void (*restore_palette)(void);
    } LogoConsoleTurtle;

    //
    // Text cursor operations (optional)
    // These are available on devices with text display capability.
    //
    typedef struct LogoConsoleText
    {
        // Clear the text display
        void (*clear)(void);

        // Cursor position (column 0 = left, row 0 = top)
        void (*set_cursor)(uint8_t column, uint8_t row);
        void (*get_cursor)(uint8_t *column, uint8_t *row);
    } LogoConsoleText;

    //
    // Screen mode operations (optional)
    // These are available on devices that support mixed graphics/text modes.
    //
    typedef struct LogoConsoleScreen
    {
        // Full-screen graphics mode
        void (*fullscreen)(void);

        // Split screen: graphics on top, text on bottom
        void (*splitscreen)(void);

        // Full-screen text mode
        void (*textscreen)(void);
    } LogoConsoleScreen;

    //
    // Console structure
    // Represents a physical device with keyboard input and screen output.
    //
    typedef struct LogoConsole
    {
        // Keyboard input as a stream (always available)
        LogoStream input;

        // Screen output as a stream (always available)
        LogoStream output;

        // Optional turtle graphics (NULL if no graphics support)
        const LogoConsoleTurtle *turtle;

        // Optional text cursor operations (NULL if no cursor control)
        const LogoConsoleText *text;

        // Optional screen mode operations (NULL if single mode only)
        const LogoConsoleScreen *screen;

        // Private context for the console implementation
        void *context;
    } LogoConsole;

    //
    // Console lifecycle functions
    // Implementations should provide these for their specific device.
    //

    // Initialize a console with streams and optional capabilities
    void logo_console_init(LogoConsole *console,
                           const LogoStreamOps *input_ops,
                           const LogoStreamOps *output_ops,
                           void *context);

    // Check if console has various capabilities
    bool logo_console_has_turtle(const LogoConsole *console);
    bool logo_console_has_text(const LogoConsole *console);
    bool logo_console_has_screen_modes(const LogoConsole *console);

#ifdef __cplusplus
}
#endif
