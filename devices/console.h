//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
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

    // How a turtle's bitmap/colour costume follows its heading (setrot).
    // Shape 0 (the line-drawn triangle) always rotates regardless.
    typedef enum LogoRotationStyle
    {
        LOGO_ROT_FIXED, // Costume never rotates (period behaviour; default)
        LOGO_ROT_FULL,  // Costume rotates to the heading
        LOGO_ROT_FLIP,  // Costume mirrors left/right when facing west
    } LogoRotationStyle;

    // A turtle's rendered raster, exactly as the compositor would overlay
    // it (backs the sensing primitives touching?/over?/colourunder). All
    // coordinates are in screen pixels — the same space canvas_point reads.
    // The mask is w*h bytes, row-major; a pixel is solid where the byte is
    // nonzero (mono mask) or not the transparent index (indexed colour).
    // The pointer is owned by the device. Each turtle's mask must be backed
    // independently and stay valid until that same turtle's raster is next
    // rebuilt (another get_raster for it, or a change to its shape/pose).
    // Reading or selecting a *different* turtle must not invalidate it —
    // touching? holds two turtles' masks at once, and over?/colourunder
    // read the canvas while holding one.
    #define LOGO_RASTER_TRANSPARENT 255

    typedef struct LogoTurtleRaster
    {
        int16_t x, y;        // Mask top-left corner
        int16_t cx, cy;      // Turtle anchor point (its position on screen)
        uint8_t w, h;        // Mask dimensions
        bool indexed;        // Mask bytes are palette slots (255 transparent);
                             // otherwise mono (solid where nonzero)
        bool visible;        // Currently rendered (shown and on-screen).
                             // touching? requires it; over?/colourunder
                             // sense the canvas under the turtle regardless.
        const uint8_t *mask;
    } LogoTurtleRaster;

    //
    // Turtle graphics operations (optional)
    // These are available on devices with graphics capability.
    //
    typedef struct LogoConsoleTurtle
    {
        // Route the stateful turtle ops below to turtle n (0-based).
        // Devices without multi-turtle support may leave this NULL; core
        // then addresses the single built-in turtle regardless of n.
        void (*select)(uint8_t n);

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
        // Returns true on success, false if boundary error (fence mode)
        bool (*set_position)(float x, float y);
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

        // Draw text on the graphics screen at the selected turtle's current
        // position, in the current pen colour, upright and left-to-right.
        // The turtle does not move and its heading is ignored (this is the
        // horizontal-only cousin of UCB `label`). Optional.
        void (*draw_text)(const char *text);

        // Boundary modes
        void (*set_fence)(void);    // Turtle stops at boundary
        void (*set_window)(void);   // Turtle can go off-screen
        void (*set_wrap)(void);     // Turtle wraps around

        // Save graphics screen as a BMP to an open stream (caller closes)
        // Returns 0 on success, errno on failure
        int (*gfx_save)(LogoStream *out);

        // Load graphics screen from a BMP read from an open stream (caller closes)
        // Returns 0 on success, errno on failure (EINVAL = not a usable BMP)
        int (*gfx_load)(LogoStream *in);

        // Palette functions
        // Set palette slot to RGB565 value (from 24-bit RGB components)
        void (*set_palette)(uint8_t slot, uint8_t r, uint8_t g, uint8_t b);

        // Get palette slot RGB components (from RGB565 value)
        void (*get_palette)(uint8_t slot, uint8_t *r, uint8_t *g, uint8_t *b);

        // Restore default palette (slots 0-127)
        void (*restore_palette)(void);

        // Shape management (shapes 0-15)
        // Shape 0 is the default line-drawn turtle that rotates with heading
        // Shapes 1-15 are 8x16 bitmaps that do not rotate
        void (*set_shape)(uint8_t shape_num);
        uint8_t (*get_shape)(void);

        // Get shape data for shapes 1-15 (8 columns x 16 rows as 16 uint8_t values)
        // Each byte represents one row, MSB = leftmost column
        // Returns false if shape_num is 0 or > 15
        bool (*get_shape_data)(uint8_t shape_num, uint8_t *data);

        // Put shape data for shapes 1-15 (8 columns x 16 rows as 16 uint8_t values)
        // Each byte represents one row, MSB = leftmost column
        // Returns false if shape_num is 0 or > 15
        bool (*put_shape_data)(uint8_t shape_num, const uint8_t *data);

        // Rotation style for the turtle's costume (setrot). Optional.
        void (*set_rotation_style)(LogoRotationStyle style);

        // Integer magnification 1 or 2 for the rendered costume (setmag).
        // Costumes larger than 16x16 always render at 1 (32x32 raster cap).
        // Optional.
        void (*set_scale)(uint8_t mag);

        //
        // Autonomous motion and animation (backs setspeed/speed/setanim).
        // The device owns the moving state and advances it on turtle_tick;
        // core only sets it and drives the tick from the demon poll. All
        // optional; absent ops make the turtles stationary.
        //

        // Set the selected turtle's autonomous speed in turtle steps per
        // second (0 stops it). The turtle glides along its heading, drawing
        // and honouring the boundary mode exactly as forward would.
        void (*set_speed)(float steps_per_second);

        // Output the selected turtle's autonomous speed.
        float (*get_speed)(void);

        // Animate the selected turtle's costume: cycle its shape from
        // `first` to `last` (inclusive), advancing one frame every
        // `interval_ms`. interval_ms 0 stops animating and leaves the shape
        // as it is.
        void (*set_anim)(uint8_t first, uint8_t last, uint16_t interval_ms);

        // Advance every turtle's autonomous motion and animation by dt
        // milliseconds. Driven from the demon poll (during execution and at
        // the prompt); the device owns all positions.
        void (*turtle_tick)(uint32_t dt_ms);

        // Composite the turtle's current costume into the canvas at the
        // turtle's position (backs the stamp primitive). Optional.
        void (*stamp)(void);

        // Capture the w x h canvas region centred on the turtle into
        // colour costume slot (1-15); canvas pixels matching the current
        // background slot become transparent (backs snapsh). Returns
        // false when the costume pool cannot hold the capture. Optional.
        bool (*snap_costume)(uint8_t slot, uint8_t w, uint8_t h);

        //
        // Sensing support (backs touching?/over?/colourunder). Core owns
        // the geometry; the device only exposes the rendered rasters and
        // the canvas beneath them, so the logic is fully testable on the
        // mock. All optional; sensing degrades to false/0 when absent.
        //

        // Fill `out` with the selected turtle's rendered raster (see
        // LogoTurtleRaster), including whether it is currently visible.
        // Returns false only when the device cannot produce a raster.
        bool (*get_raster)(LogoTurtleRaster *out);

        // Canvas palette index the compositor would show at screen pixel
        // (x, y), honouring the boundary mode: wrapped in wrap mode, the
        // background slot outside the canvas otherwise. Reads the canvas
        // only — never sprite pixels — so it senses drawings.
        uint8_t (*canvas_point)(int x, int y);

        // Screen pixel dimensions and whether wrap boundary mode is
        // active, so core can fold edge-straddling contacts round the
        // screen. When absent, core treats the surface as non-wrapping.
        void (*sense_metrics)(int *width, int *height, bool *wrap);
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

        // Text color indices (0-15, referencing text palette slots)
        void (*set_foreground)(uint8_t index);
        uint8_t (*get_foreground)(void);
        void (*set_background)(uint8_t index);
        uint8_t (*get_background)(void);
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

        // Refresh policy. In automatic mode (the default) graphics drawing
        // is presented to the display as it happens; in manual mode drawing
        // accumulates off-screen until refresh_now() presents it.
        // Switching back to automatic mode presents any pending drawing.
        void (*set_refresh_auto)(bool auto_mode);
        bool (*get_refresh_auto)(void);

        // Present pending drawing immediately, regardless of the refresh
        // policy or any frame-rate limiting.
        void (*refresh_now)(void);
    } LogoConsoleScreen;

    //
    // Editor result codes
    //
    typedef enum LogoEditorResult
    {
        LOGO_EDITOR_ACCEPT,     // User pressed ESC to accept changes
        LOGO_EDITOR_CANCEL,     // User pressed BRK to cancel
        LOGO_EDITOR_ERROR       // Editor error (buffer too small, etc.)
    } LogoEditorResult;

    //
    // Editor operations (optional)
    // These are available on devices with full-screen text editing capability.
    //
    typedef struct LogoConsoleEditor
    {
        // Edit text in a full-screen editor
        // buffer: the text to edit (in/out), must be pre-filled with initial content
        // buffer_size: maximum size of the buffer
        // Returns: LOGO_EDITOR_ACCEPT if user accepted, LOGO_EDITOR_CANCEL if cancelled
        LogoEditorResult (*edit)(char *buffer, size_t buffer_size);
    } LogoConsoleEditor;

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

        // Error output as a stream (optional — falls back to output if NULL ops)
        LogoStream error_output;

        // Optional turtle graphics (NULL if no graphics support)
        const LogoConsoleTurtle *turtle;

        // Optional text cursor operations (NULL if no cursor control)
        const LogoConsoleText *text;

        // Optional screen mode operations (NULL if single mode only)
        const LogoConsoleScreen *screen;

        // Optional editor operations (NULL if no editor support)
        const LogoConsoleEditor *editor;

        // Carried syntax depth for line-oriented console input.
        // Devices that support live input highlighting can use this to render
        // a continued line relative to already-open brackets from prior lines.
        int input_syntax_depth;

        // True when a person is at the console (default). The host device
        // clears this when stdin is not a terminal (pipe/file), which
        // suppresses REPL prompts so scripted output is clean and diffable.
        bool interactive;

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
    bool logo_console_has_editor(const LogoConsole *console);

#ifdef __cplusplus
}
#endif
