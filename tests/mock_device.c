//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Mock device implementation for testing turtle graphics and text screen.
//

#include "mock_device.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

// Screen dimensions (matches Pico Logo reference)
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 320
#define SCREEN_HALF_WIDTH  160
#define SCREEN_HALF_HEIGHT 160

//
// Static state
//
static MockDeviceState mock_state;
static LogoConsole mock_console;

// Input/output buffers for stream operations
static char mock_output_buffer[4096];
static int mock_output_pos = 0;
static const char *mock_input_buffer = NULL;
static size_t mock_input_pos = 0;

// Editor state for testing
static char mock_editor_input[8192];      // Content passed to editor
static char mock_editor_content[8192];    // Content editor returns
static LogoEditorResult mock_editor_result = LOGO_EDITOR_ACCEPT;
static bool mock_editor_called = false;

//
// Forward declarations for console operations
//
static void record_command(MockCommandType type);
static void record_command_float(MockCommandType type, float value);
static void record_command_position(MockCommandType type, float x, float y);
static void record_command_bool(MockCommandType type, bool value);
static void record_command_colour(MockCommandType type, uint16_t colour);
static void record_command_cursor(uint8_t col, uint8_t row);
static void record_command_width(uint8_t width);

// Normalize angle to [0, 360)
static float normalize_angle(float angle)
{
    while (angle < 0.0f) angle += 360.0f;
    while (angle >= 360.0f) angle -= 360.0f;
    return angle;
}

// Convert heading to radians (0 = north, clockwise)
static float heading_to_radians(float heading)
{
    // Convert from compass heading (0=N, 90=E) to math angle (0=E, 90=N)
    // heading 0 (N) -> radians PI/2
    // heading 90 (E) -> radians 0
    return (90.0f - heading) * (float)M_PI / 180.0f;
}

//
// Turtle graphics operations
//

static void mock_turtle_clear(void)
{
    mock_state.graphics.cleared = true;
    mock_state.graphics.dot_count = 0;
    mock_state.graphics.line_count = 0;
    record_command(MOCK_CMD_CLEAR_GRAPHICS);
}

static void mock_turtle_draw(void)
{
    record_command(MOCK_CMD_DRAW);
}

static bool mock_turtle_move(float distance)
{
    float heading_rad = heading_to_radians(mock_state.turtle.heading);
    float dx = distance * cosf(heading_rad);
    float dy = distance * sinf(heading_rad);
    
    float old_x = mock_state.turtle.x;
    float old_y = mock_state.turtle.y;
    float new_x = old_x + dx;
    float new_y = old_y + dy;
    
    // Handle boundary modes
    switch (mock_state.turtle.boundary_mode)
    {
    case MOCK_BOUNDARY_FENCE:
        // Check if new position is out of bounds
        // Valid Logo coordinates are [-160, 159], but we use 0.5 threshold to align
        // with pixel rounding (coordinates that round to valid pixels are accepted)
        if (new_x < -SCREEN_HALF_WIDTH - 0.5f || new_x >= SCREEN_HALF_WIDTH - 0.5f ||
            new_y < -SCREEN_HALF_HEIGHT - 0.5f || new_y >= SCREEN_HALF_HEIGHT - 0.5f)
        {
            mock_state.boundary_error = true;
            // Don't move
            return false;  // Boundary error
        }
        break;
        
    case MOCK_BOUNDARY_WINDOW:
        // Allow any position, no restrictions
        break;
        
    case MOCK_BOUNDARY_WRAP:
        // Wrap around edges using robust formula
        // First normalize to [0, SCREEN_WIDTH), then shift back to [-HALF, HALF)
        {
            float norm_x = new_x + SCREEN_HALF_WIDTH;  // Shift to [0, 320) range
            float norm_y = new_y + SCREEN_HALF_HEIGHT;
            norm_x = norm_x - floorf(norm_x / SCREEN_WIDTH) * SCREEN_WIDTH;
            norm_y = norm_y - floorf(norm_y / SCREEN_HEIGHT) * SCREEN_HEIGHT;
            new_x = norm_x - SCREEN_HALF_WIDTH;  // Shift back to [-160, 160)
            new_y = norm_y - SCREEN_HALF_HEIGHT;
        }
        break;
    }
    
    // Draw line if pen is down
    if (mock_state.turtle.pen_state == LOGO_PEN_DOWN)
    {
        if (mock_state.graphics.line_count < MOCK_MAX_LINES)
        {
            MockLine *line = &mock_state.graphics.lines[mock_state.graphics.line_count++];
            line->x1 = old_x;
            line->y1 = old_y;
            line->x2 = new_x;
            line->y2 = new_y;
            line->colour = mock_state.turtle.pen_colour;
        }
    }
    
    mock_state.turtle.x = new_x;
    mock_state.turtle.y = new_y;
    
    record_command_float(MOCK_CMD_MOVE, distance);
    return true;  // Success
}

// Copy the working (selected) turtle state into its turtles[] slot
static void turtle_slot_sync(void)
{
    MockTurtleState *t = &mock_state.turtles[mock_state.current_turtle];
    t->x = mock_state.turtle.x;
    t->y = mock_state.turtle.y;
    t->heading = mock_state.turtle.heading;
    t->pen_state = mock_state.turtle.pen_state;
    t->pen_colour = mock_state.turtle.pen_colour;
    t->visible = mock_state.turtle.visible;
    t->shape = mock_state.shape.current_shape;
}

static void mock_turtle_select(uint8_t n)
{
    if (n >= MOCK_MAX_TURTLES || n == mock_state.current_turtle)
    {
        return;  // No-op selects aren't recorded (keeps history noise-free)
    }
    record_command_float(MOCK_CMD_SELECT, (float)n);

    turtle_slot_sync();
    mock_state.current_turtle = n;

    const MockTurtleState *t = &mock_state.turtles[n];
    mock_state.turtle.x = t->x;
    mock_state.turtle.y = t->y;
    mock_state.turtle.heading = t->heading;
    mock_state.turtle.pen_state = t->pen_state;
    mock_state.turtle.pen_colour = t->pen_colour;
    mock_state.turtle.visible = t->visible;
    mock_state.shape.current_shape = t->shape;
}

static void mock_turtle_home(void)
{
    // Draw line to home if pen is down
    if (mock_state.turtle.pen_state == LOGO_PEN_DOWN)
    {
        if (mock_state.graphics.line_count < MOCK_MAX_LINES)
        {
            MockLine *line = &mock_state.graphics.lines[mock_state.graphics.line_count++];
            line->x1 = mock_state.turtle.x;
            line->y1 = mock_state.turtle.y;
            line->x2 = 0.0f;
            line->y2 = 0.0f;
            line->colour = mock_state.turtle.pen_colour;
        }
    }
    
    mock_state.turtle.x = 0.0f;
    mock_state.turtle.y = 0.0f;
    mock_state.turtle.heading = 0.0f;
    record_command(MOCK_CMD_HOME);
}

static bool mock_turtle_set_position(float x, float y)
{
    float new_x = x;
    float new_y = y;

    // Handle boundary modes
    switch (mock_state.turtle.boundary_mode)
    {
    case MOCK_BOUNDARY_FENCE:
        // Check if new position is out of bounds
        if (new_x < -SCREEN_HALF_WIDTH - 0.5f || new_x >= SCREEN_HALF_WIDTH - 0.5f ||
            new_y < -SCREEN_HALF_HEIGHT - 0.5f || new_y >= SCREEN_HALF_HEIGHT - 0.5f)
        {
            mock_state.boundary_error = true;
            return false;  // Boundary error
        }
        break;

    case MOCK_BOUNDARY_WINDOW:
        // Allow any position, no restrictions
        break;

    case MOCK_BOUNDARY_WRAP:
        // Don't wrap yet - draw line with unwrapped coordinates first
        break;
    }

    // Draw line if pen is down (use unwrapped coordinates for correct wrap rendering)
    if (mock_state.turtle.pen_state == LOGO_PEN_DOWN)
    {
        if (mock_state.graphics.line_count < MOCK_MAX_LINES)
        {
            MockLine *line = &mock_state.graphics.lines[mock_state.graphics.line_count++];
            line->x1 = mock_state.turtle.x;
            line->y1 = mock_state.turtle.y;
            line->x2 = new_x;
            line->y2 = new_y;
            line->colour = mock_state.turtle.pen_colour;
        }
    }

    mock_state.turtle.x = new_x;
    mock_state.turtle.y = new_y;

    // Now wrap the position after drawing (for wrap mode only)
    if (mock_state.turtle.boundary_mode == MOCK_BOUNDARY_WRAP)
    {
        float norm_x = mock_state.turtle.x + SCREEN_HALF_WIDTH;
        float norm_y = mock_state.turtle.y + SCREEN_HALF_HEIGHT;
        norm_x = norm_x - floorf(norm_x / SCREEN_WIDTH) * SCREEN_WIDTH;
        norm_y = norm_y - floorf(norm_y / SCREEN_HEIGHT) * SCREEN_HEIGHT;
        mock_state.turtle.x = norm_x - SCREEN_HALF_WIDTH;
        mock_state.turtle.y = norm_y - SCREEN_HALF_HEIGHT;
    }

    record_command_position(MOCK_CMD_SET_POSITION, mock_state.turtle.x, mock_state.turtle.y);
    return true;  // Success
}

static void mock_turtle_get_position(float *x, float *y)
{
    if (x) *x = mock_state.turtle.x;
    if (y) *y = mock_state.turtle.y;
}

static void mock_turtle_set_heading(float heading)
{
    mock_state.turtle.heading = normalize_angle(heading);
    record_command_float(MOCK_CMD_SET_HEADING, heading);
}

static float mock_turtle_get_heading(void)
{
    return mock_state.turtle.heading;
}

static void mock_turtle_set_pen_colour(uint8_t colour)
{
    mock_state.turtle.pen_colour = colour;
    record_command_colour(MOCK_CMD_SET_PEN_COLOUR, colour);
}

static uint8_t mock_turtle_get_pen_colour(void)
{
    return mock_state.turtle.pen_colour;
}

static void mock_turtle_set_bg_colour(uint8_t colour)
{
    mock_state.turtle.bg_colour = colour;
    record_command_colour(MOCK_CMD_SET_BG_COLOUR, colour);
}

static uint8_t mock_turtle_get_bg_colour(void)
{
    return mock_state.turtle.bg_colour;
}

static void mock_turtle_set_pen_state(LogoPen pen)
{
    mock_state.turtle.pen_state = pen;
    record_command_bool(MOCK_CMD_SET_PEN_STATE, pen);
}

static LogoPen mock_turtle_get_pen_state(void)
{
    return mock_state.turtle.pen_state;
}

static void mock_turtle_set_visible(bool visible)
{
    mock_state.turtle.visible = visible;
    record_command_bool(MOCK_CMD_SET_VISIBLE, visible);
}

static bool mock_turtle_get_visible(void)
{
    return mock_state.turtle.visible;
}

static void mock_turtle_dot(float x, float y)
{
    if (mock_state.graphics.dot_count < MOCK_MAX_DOTS)
    {
        MockDot *dot = &mock_state.graphics.dots[mock_state.graphics.dot_count++];
        dot->x = x;
        dot->y = y;
        dot->colour = mock_state.turtle.pen_colour;
    }
    record_command_position(MOCK_CMD_DOT, x, y);
}

static bool mock_turtle_dot_at(float x, float y)
{
    // Check if there's a dot at this position (with small tolerance)
    const float tolerance = 0.5f;
    for (int i = 0; i < mock_state.graphics.dot_count; i++)
    {
        if (fabsf(mock_state.graphics.dots[i].x - x) < tolerance &&
            fabsf(mock_state.graphics.dots[i].y - y) < tolerance)
        {
            return true;
        }
    }
    return false;
}

static void mock_turtle_fill(void)
{
    record_command(MOCK_CMD_FILL);
}

static void mock_turtle_set_fence(void)
{
    mock_state.turtle.boundary_mode = MOCK_BOUNDARY_FENCE;
    record_command(MOCK_CMD_SET_FENCE);
}

static void mock_turtle_set_window(void)
{
    mock_state.turtle.boundary_mode = MOCK_BOUNDARY_WINDOW;
    record_command(MOCK_CMD_SET_WINDOW);
}

static void mock_turtle_set_wrap(void)
{
    mock_state.turtle.boundary_mode = MOCK_BOUNDARY_WRAP;
    record_command(MOCK_CMD_SET_WRAP);
}

static int mock_turtle_gfx_save(LogoStream *out)
{
    // Track the call
    mock_state.gfx_io.gfx_save_call_count++;
    if (out)
    {
        strncpy(mock_state.gfx_io.last_save_filename, out->name,
                sizeof(mock_state.gfx_io.last_save_filename) - 1);
        mock_state.gfx_io.last_save_filename[sizeof(mock_state.gfx_io.last_save_filename) - 1] = '\0';
    }
    return mock_state.gfx_io.gfx_save_result;
}

static int mock_turtle_gfx_load(LogoStream *in)
{
    // Track the call
    mock_state.gfx_io.gfx_load_call_count++;
    if (in)
    {
        strncpy(mock_state.gfx_io.last_load_filename, in->name,
                sizeof(mock_state.gfx_io.last_load_filename) - 1);
        mock_state.gfx_io.last_load_filename[sizeof(mock_state.gfx_io.last_load_filename) - 1] = '\0';
    }
    return mock_state.gfx_io.gfx_load_result;
}

static void mock_turtle_set_palette(uint8_t slot, uint8_t r, uint8_t g, uint8_t b)
{
    mock_state.palette.r[slot] = r;
    mock_state.palette.g[slot] = g;
    mock_state.palette.b[slot] = b;
}

static void mock_turtle_get_palette(uint8_t slot, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (r) *r = mock_state.palette.r[slot];
    if (g) *g = mock_state.palette.g[slot];
    if (b) *b = mock_state.palette.b[slot];
}

static void mock_turtle_restore_palette(void)
{
    mock_state.palette.restore_palette_called = true;
    // Reset to some default values (black) for first 128 slots
    for (int i = 0; i < 128; i++)
    {
        mock_state.palette.r[i] = 0;
        mock_state.palette.g[i] = 0;
        mock_state.palette.b[i] = 0;
    }
}

static void mock_turtle_set_shape(uint8_t shape_num)
{
    if (shape_num > 15)
        return;
    mock_state.shape.current_shape = shape_num;
}

static uint8_t mock_turtle_get_shape(void)
{
    return mock_state.shape.current_shape;
}

static bool mock_turtle_get_shape_data(uint8_t shape_num, uint8_t *data)
{
    if (shape_num == 0 || shape_num > 15 || data == NULL)
        return false;
    
    memcpy(data, mock_state.shape.shapes[shape_num - 1], 16);
    return true;
}

static bool mock_turtle_put_shape_data(uint8_t shape_num, const uint8_t *data)
{
    if (shape_num == 0 || shape_num > 15 || data == NULL)
        return false;
    
    memcpy(mock_state.shape.shapes[shape_num - 1], data, 16);
    return true;
}

static void mock_turtle_stamp(void)
{
    record_command(MOCK_CMD_STAMP);
}

static void mock_turtle_draw_text(const char *text)
{
    mock_state.label.count++;
    mock_state.label.last_x = mock_state.turtle.x;
    mock_state.label.last_y = mock_state.turtle.y;
    mock_state.label.last_colour = mock_state.turtle.pen_colour;
    mock_state.label.last_turtle = mock_state.current_turtle;
    snprintf(mock_state.label.last_text, sizeof(mock_state.label.last_text), "%s", text ? text : "");
    record_command(MOCK_CMD_WRITE);
}

// rot/mag live only in the turtles[] slots (turtle_slot_sync doesn't
// touch them), so write straight to the selected slot
static void mock_turtle_set_rotation_style(LogoRotationStyle style)
{
    mock_state.turtles[mock_state.current_turtle].rot_style = (uint8_t)style;
    record_command_float(MOCK_CMD_SET_ROT, (float)style);
}

static void mock_turtle_set_scale(uint8_t mag)
{
    mock_state.turtles[mock_state.current_turtle].mag = mag;
    record_command_float(MOCK_CMD_SET_MAG, (float)mag);
}

static bool mock_turtle_snap_costume(uint8_t slot, uint8_t w, uint8_t h)
{
    mock_state.costume.snap_count++;
    mock_state.costume.last_snap_slot = slot;
    mock_state.costume.last_snap_w = w;
    mock_state.costume.last_snap_h = h;
    mock_state.costume.last_snap_turtle = mock_state.current_turtle;
    return mock_state.costume.snap_result;
}

//
// Sensing ops. Tests stage rasters and the canvas; these just report
// them so the core sensing logic (touching?/over?/colourunder) runs
// against real data.
//
static bool mock_turtle_get_raster(LogoTurtleRaster *out)
{
    const MockRaster *r = &mock_state.sensing.rasters[mock_state.current_turtle];
    if (!r->set)
    {
        return false;
    }
    out->x = r->x;
    out->y = r->y;
    out->cx = r->cx;
    out->cy = r->cy;
    out->w = r->w;
    out->h = r->h;
    out->indexed = r->indexed;
    out->visible = r->visible;
    out->mask = r->mask;
    return true;
}

static uint8_t mock_turtle_canvas_point(int x, int y)
{
    if (mock_state.turtle.boundary_mode == MOCK_BOUNDARY_WRAP)
    {
        x = ((x % SCREEN_WIDTH) + SCREEN_WIDTH) % SCREEN_WIDTH;
        y = ((y % SCREEN_HEIGHT) + SCREEN_HEIGHT) % SCREEN_HEIGHT;
    }
    else if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
    {
        return 0;  // Background outside the canvas
    }
    return mock_state.sensing.canvas[y * SCREEN_WIDTH + x];
}

static void mock_turtle_sense_metrics(int *width, int *height, bool *wrap)
{
    if (width) *width = SCREEN_WIDTH;
    if (height) *height = SCREEN_HEIGHT;
    if (wrap) *wrap = (mock_state.turtle.boundary_mode == MOCK_BOUNDARY_WRAP);
}

//
// Autonomous motion and animation. speed/anim live in the per-turtle slots
// (not the working state), so they survive select(). turtle_tick drives
// each turtle through the real move op, so gliding honours boundary mode
// and draws exactly like forward.
//
static void mock_turtle_set_speed(float steps_per_second)
{
    mock_state.turtles[mock_state.current_turtle].speed = steps_per_second;
}

static float mock_turtle_get_speed(void)
{
    return mock_state.turtles[mock_state.current_turtle].speed;
}

static void mock_turtle_set_anim(uint8_t first, uint8_t last, uint16_t interval_ms)
{
    MockTurtleState *t = &mock_state.turtles[mock_state.current_turtle];
    t->anim_first = first;
    t->anim_last = last;
    t->anim_interval = interval_ms;
    t->anim_accum = 0;
}

static void mock_turtle_tick(uint32_t dt_ms)
{
    uint8_t saved = mock_state.current_turtle;

    for (uint8_t n = 0; n < MOCK_MAX_TURTLES; n++)
    {
        MockTurtleState *t = &mock_state.turtles[n];

        // Advance autonomous motion through the real move op (needs the
        // turtle selected so it operates on the working state).
        if (t->speed > 0.0f)
        {
            mock_turtle_select(n);
            mock_turtle_move(t->speed * (float)dt_ms / 1000.0f);
        }

        // Advance the animation frame(s) due this interval. Accumulate in a
        // wide local so a large dt_ms can't wrap the uint16_t field, and
        // step a local shape var (not t->shape, which mock_turtle_select
        // only refreshes on an actual turtle change) so multiple frames due
        // in one tick all advance instead of recomputing the same frame.
        if (t->anim_interval > 0)
        {
            uint32_t accum = (uint32_t)t->anim_accum + dt_ms;
            uint8_t shape = t->shape;
            while (accum >= t->anim_interval)
            {
                accum -= t->anim_interval;
                if (shape >= t->anim_last || shape < t->anim_first)
                {
                    shape = t->anim_first;
                }
                else
                {
                    shape = (uint8_t)(shape + 1);
                }
            }
            t->anim_accum = (uint16_t)accum;
            if (shape != t->shape)
            {
                mock_turtle_select(n);
                mock_turtle_set_shape(shape);  // keeps working shape in sync
                t->shape = shape;
            }
        }
    }

    mock_turtle_select(saved);
}

// Turtle operations structure
static const LogoConsoleTurtle mock_turtle_ops = {
    .select = mock_turtle_select,
    .clear = mock_turtle_clear,
    .draw = mock_turtle_draw,
    .move = mock_turtle_move,
    .home = mock_turtle_home,
    .set_position = mock_turtle_set_position,
    .get_position = mock_turtle_get_position,
    .set_heading = mock_turtle_set_heading,
    .get_heading = mock_turtle_get_heading,
    .set_pen_colour = mock_turtle_set_pen_colour,
    .get_pen_colour = mock_turtle_get_pen_colour,
    .set_bg_colour = mock_turtle_set_bg_colour,
    .get_bg_colour = mock_turtle_get_bg_colour,
    .set_pen_state = mock_turtle_set_pen_state,
    .get_pen_state = mock_turtle_get_pen_state,
    .set_visible = mock_turtle_set_visible,
    .get_visible = mock_turtle_get_visible,
    .dot = mock_turtle_dot,
    .dot_at = mock_turtle_dot_at,
    .fill = mock_turtle_fill,
    .draw_text = mock_turtle_draw_text,
    .set_fence = mock_turtle_set_fence,
    .set_window = mock_turtle_set_window,
    .set_wrap = mock_turtle_set_wrap,
    .gfx_save = mock_turtle_gfx_save,
    .gfx_load = mock_turtle_gfx_load,
    .set_palette = mock_turtle_set_palette,
    .get_palette = mock_turtle_get_palette,
    .restore_palette = mock_turtle_restore_palette,
    .set_shape = mock_turtle_set_shape,
    .get_shape = mock_turtle_get_shape,
    .get_shape_data = mock_turtle_get_shape_data,
    .put_shape_data = mock_turtle_put_shape_data,
    .set_rotation_style = mock_turtle_set_rotation_style,
    .set_scale = mock_turtle_set_scale,
    .stamp = mock_turtle_stamp,
    .snap_costume = mock_turtle_snap_costume,
    .get_raster = mock_turtle_get_raster,
    .canvas_point = mock_turtle_canvas_point,
    .sense_metrics = mock_turtle_sense_metrics,
    .set_speed = mock_turtle_set_speed,
    .get_speed = mock_turtle_get_speed,
    .set_anim = mock_turtle_set_anim,
    .turtle_tick = mock_turtle_tick
};

//
// Text screen operations
//

static void mock_text_clear(void)
{
    mock_state.text.cleared = true;
    mock_state.text.cursor_col = 0;
    mock_state.text.cursor_row = 0;
    record_command(MOCK_CMD_CLEAR_TEXT);
}

static void mock_text_set_cursor(uint8_t column, uint8_t row)
{
    mock_state.text.cursor_col = column;
    mock_state.text.cursor_row = row;
    record_command_cursor(column, row);
}

static void mock_text_get_cursor(uint8_t *column, uint8_t *row)
{
    if (column) *column = mock_state.text.cursor_col;
    if (row) *row = mock_state.text.cursor_row;
}

static void mock_text_set_width(uint8_t width)
{
    mock_state.text.width = width;
    record_command_width(width);
}

static uint8_t mock_text_get_width(void)
{
    return mock_state.text.width;
}

static void mock_text_set_foreground(uint8_t index)
{
    mock_state.text.foreground = index;
    MockCommand cmd = { .type = MOCK_CMD_SET_TEXT_FOREGROUND };
    cmd.params.text_index = index;
    mock_state.commands[mock_state.command_count % MOCK_COMMAND_HISTORY_SIZE] = cmd;
    mock_state.command_count++;
}

static uint8_t mock_text_get_foreground(void)
{
    return mock_state.text.foreground;
}

static void mock_text_set_background(uint8_t index)
{
    mock_state.text.background = index;
    MockCommand cmd = { .type = MOCK_CMD_SET_TEXT_BACKGROUND };
    cmd.params.text_index = index;
    mock_state.commands[mock_state.command_count % MOCK_COMMAND_HISTORY_SIZE] = cmd;
    mock_state.command_count++;
}

static uint8_t mock_text_get_background(void)
{
    return mock_state.text.background;
}

// Text operations structure
static const LogoConsoleText mock_text_ops = {
    .clear = mock_text_clear,
    .set_cursor = mock_text_set_cursor,
    .get_cursor = mock_text_get_cursor,
    .set_foreground = mock_text_set_foreground,
    .get_foreground = mock_text_get_foreground,
    .set_background = mock_text_set_background,
    .get_background = mock_text_get_background,
};

//
// Screen mode operations
//

static void mock_screen_fullscreen(void)
{
    mock_state.screen_mode = MOCK_SCREEN_FULLSCREEN;
    record_command(MOCK_CMD_FULLSCREEN);
}

static void mock_screen_splitscreen(void)
{
    mock_state.screen_mode = MOCK_SCREEN_SPLIT;
    record_command(MOCK_CMD_SPLITSCREEN);
}

static void mock_screen_textscreen(void)
{
    mock_state.screen_mode = MOCK_SCREEN_TEXT;
    record_command(MOCK_CMD_TEXTSCREEN);
}

static void mock_screen_set_refresh_auto(bool auto_mode)
{
    mock_state.refresh_auto = auto_mode;
    record_command(MOCK_CMD_SET_REFRESH);
}

static bool mock_screen_get_refresh_auto(void)
{
    return mock_state.refresh_auto;
}

static void mock_screen_refresh_now(void)
{
    mock_state.refresh_now_count++;
    record_command(MOCK_CMD_REFRESH_NOW);
}

// Screen mode operations structure
static const LogoConsoleScreen mock_screen_ops = {
    .fullscreen = mock_screen_fullscreen,
    .splitscreen = mock_screen_splitscreen,
    .textscreen = mock_screen_textscreen,
    .set_refresh_auto = mock_screen_set_refresh_auto,
    .get_refresh_auto = mock_screen_get_refresh_auto,
    .refresh_now = mock_screen_refresh_now
};

//
// Stream operations for I/O
//

static int mock_stream_read_char(LogoStream *stream)
{
    (void)stream;
    if (!mock_input_buffer || mock_input_buffer[mock_input_pos] == '\0')
    {
        return -1;
    }
    return (unsigned char)mock_input_buffer[mock_input_pos++];
}

static int mock_stream_read_chars(LogoStream *stream, char *buffer, int count)
{
    (void)stream;
    if (!mock_input_buffer || !buffer || count <= 0)
    {
        return 0;
    }
    
    int i;
    for (i = 0; i < count && mock_input_buffer[mock_input_pos] != '\0'; i++)
    {
        buffer[i] = mock_input_buffer[mock_input_pos++];
    }
    return i;
}

static int mock_stream_read_line(LogoStream *stream, char *buffer, size_t buffer_size)
{
    (void)stream;
    if (!mock_input_buffer || mock_input_buffer[mock_input_pos] == '\0')
    {
        return -1;
    }
    
    size_t i = 0;
    while (i < buffer_size - 1 && mock_input_buffer[mock_input_pos] != '\0')
    {
        char c = mock_input_buffer[mock_input_pos++];
        if (c == '\n')
            break;
        buffer[i++] = c;
    }
    buffer[i] = '\0';
    return (int)i;
}

static bool mock_stream_can_read(LogoStream *stream)
{
    (void)stream;
    return mock_input_buffer && mock_input_buffer[mock_input_pos] != '\0';
}

static void mock_stream_write(LogoStream *stream, const char *text)
{
    (void)stream;
    if (!text) return;
    
    size_t len = strlen(text);
    if (mock_output_pos + (int)len < (int)sizeof(mock_output_buffer) - 1)
    {
        memcpy(mock_output_buffer + mock_output_pos, text, len);
        mock_output_pos += (int)len;
    }
    mock_output_buffer[mock_output_pos] = '\0';
}

static void mock_stream_flush(LogoStream *stream)
{
    (void)stream;
}

static void mock_stream_close(LogoStream *stream)
{
    (void)stream;
}

// Stream operations structures
static const LogoStreamOps mock_input_ops = {
    .read_char = mock_stream_read_char,
    .read_chars = mock_stream_read_chars,
    .read_line = mock_stream_read_line,
    .can_read = mock_stream_can_read,
    .write = NULL,
    .flush = NULL,
    .get_read_pos = NULL,
    .set_read_pos = NULL,
    .get_write_pos = NULL,
    .set_write_pos = NULL,
    .get_length = NULL,
    .close = mock_stream_close
};

static const LogoStreamOps mock_output_ops = {
    .read_char = NULL,
    .read_chars = NULL,
    .read_line = NULL,
    .can_read = NULL,
    .write = mock_stream_write,
    .flush = mock_stream_flush,
    .get_read_pos = NULL,
    .set_read_pos = NULL,
    .get_write_pos = NULL,
    .set_write_pos = NULL,
    .get_length = NULL,
    .close = mock_stream_close
};

//
// Editor operations
//

static LogoEditorResult mock_editor_edit(char *buffer, size_t buffer_size)
{
    mock_editor_called = true;
    
    // Save the input content
    if (buffer)
    {
        strncpy(mock_editor_input, buffer, sizeof(mock_editor_input) - 1);
        mock_editor_input[sizeof(mock_editor_input) - 1] = '\0';
    }
    
    // If accepting, replace buffer with mock content
    if (mock_editor_result == LOGO_EDITOR_ACCEPT && mock_editor_content[0] != '\0')
    {
        size_t len = strlen(mock_editor_content);
        if (len < buffer_size)
        {
            strcpy(buffer, mock_editor_content);
        }
        else
        {
            return LOGO_EDITOR_ERROR;
        }
    }
    
    return mock_editor_result;
}

// Editor operations structure
static const LogoConsoleEditor mock_editor_ops = {
    .edit = mock_editor_edit
};

//
// Command recording helpers
//

static void record_command(MockCommandType type)
{
    if (mock_state.command_count < MOCK_COMMAND_HISTORY_SIZE)
    {
        MockCommand *cmd = &mock_state.commands[mock_state.command_count++];
        cmd->type = type;
    }
}

static void record_command_float(MockCommandType type, float value)
{
    if (mock_state.command_count < MOCK_COMMAND_HISTORY_SIZE)
    {
        MockCommand *cmd = &mock_state.commands[mock_state.command_count++];
        cmd->type = type;
        if (type == MOCK_CMD_MOVE)
        {
            cmd->params.distance = value;
        }
        else if (type == MOCK_CMD_SET_HEADING)
        {
            cmd->params.heading = value;
        }
    }
}

static void record_command_position(MockCommandType type, float x, float y)
{
    if (mock_state.command_count < MOCK_COMMAND_HISTORY_SIZE)
    {
        MockCommand *cmd = &mock_state.commands[mock_state.command_count++];
        cmd->type = type;
        cmd->params.position.x = x;
        cmd->params.position.y = y;
    }
}

static void record_command_bool(MockCommandType type, bool value)
{
    if (mock_state.command_count < MOCK_COMMAND_HISTORY_SIZE)
    {
        MockCommand *cmd = &mock_state.commands[mock_state.command_count++];
        cmd->type = type;
        cmd->params.flag = value;
    }
}

static void record_command_colour(MockCommandType type, uint16_t colour)
{
    if (mock_state.command_count < MOCK_COMMAND_HISTORY_SIZE)
    {
        MockCommand *cmd = &mock_state.commands[mock_state.command_count++];
        cmd->type = type;
        cmd->params.colour = colour;
    }
}

static void record_command_cursor(uint8_t col, uint8_t row)
{
    if (mock_state.command_count < MOCK_COMMAND_HISTORY_SIZE)
    {
        MockCommand *cmd = &mock_state.commands[mock_state.command_count++];
        cmd->type = MOCK_CMD_SET_CURSOR;
        cmd->params.cursor.col = col;
        cmd->params.cursor.row = row;
    }
}

static void record_command_width(uint8_t width)
{
    if (mock_state.command_count < MOCK_COMMAND_HISTORY_SIZE)
    {
        MockCommand *cmd = &mock_state.commands[mock_state.command_count++];
        cmd->type = MOCK_CMD_SET_WIDTH;
        cmd->params.width = width;
    }
}

//
// Public API
//

void mock_device_init(void)
{
    mock_device_reset();
    
    // Initialize console with our mock operations
    logo_console_init(&mock_console, &mock_input_ops, &mock_output_ops, NULL);
    
    // Attach turtle, text, screen, and editor operations
    mock_console.turtle = &mock_turtle_ops;
    mock_console.text = &mock_text_ops;
    mock_console.screen = &mock_screen_ops;
    mock_console.editor = &mock_editor_ops;
}

void mock_device_reset(void)
{
    memset(&mock_state, 0, sizeof(mock_state));
    
    // Initialize turtle to default state
    mock_state.turtle.x = 0.0f;
    mock_state.turtle.y = 0.0f;
    mock_state.turtle.heading = 0.0f;  // North
    mock_state.turtle.pen_state = LOGO_PEN_DOWN;
    mock_state.turtle.pen_colour = 254;  // Default color (white)
    mock_state.turtle.bg_colour = 0;   // Black background
    mock_state.turtle.visible = true;
    mock_state.turtle.boundary_mode = MOCK_BOUNDARY_WRAP;  // Default is wrap

    mock_state.costume.snap_result = true;  // snap_costume succeeds by default

    // Multi-turtle slots: all boot at home, pen down; only turtle 0 visible
    mock_state.current_turtle = 0;
    for (int i = 0; i < MOCK_MAX_TURTLES; i++)
    {
        mock_state.turtles[i].x = 0.0f;
        mock_state.turtles[i].y = 0.0f;
        mock_state.turtles[i].heading = 0.0f;
        mock_state.turtles[i].pen_state = LOGO_PEN_DOWN;
        mock_state.turtles[i].pen_colour = 254;
        mock_state.turtles[i].visible = (i == 0);
        mock_state.turtles[i].shape = 0;
        mock_state.turtles[i].rot_style = 0;  // LOGO_ROT_FIXED
        mock_state.turtles[i].mag = 1;
    }
    
    // Initialize text screen to default state
    mock_state.text.cursor_col = 0;
    mock_state.text.cursor_row = 0;
    mock_state.text.width = 40;
    mock_state.text.cleared = false;
    mock_state.text.foreground = 4;  // Default text foreground (white)
    mock_state.text.background = 0;  // Default text background (black)
    
    // Initialize screen mode
    mock_state.screen_mode = MOCK_SCREEN_TEXT;

    // Refresh policy defaults to automatic
    mock_state.refresh_auto = true;
    mock_state.refresh_now_count = 0;

    // Clear graphics state
    mock_state.graphics.cleared = false;
    mock_state.graphics.dot_count = 0;
    mock_state.graphics.line_count = 0;
    
    // Clear command history
    mock_state.command_count = 0;
    
    // Clear boundary error
    mock_state.boundary_error = false;
    
    // Clear gfx I/O state
    mock_state.gfx_io.last_save_filename[0] = '\0';
    mock_state.gfx_io.last_load_filename[0] = '\0';
    mock_state.gfx_io.gfx_save_call_count = 0;
    mock_state.gfx_io.gfx_load_call_count = 0;
    mock_state.gfx_io.gfx_save_result = 0;  // Default to success
    mock_state.gfx_io.gfx_load_result = 0;  // Default to success
    
    // Clear I/O buffers
    mock_output_buffer[0] = '\0';
    mock_output_pos = 0;
    mock_input_buffer = NULL;
    mock_input_pos = 0;
    
    // Clear editor state
    mock_editor_input[0] = '\0';
    mock_editor_content[0] = '\0';
    mock_editor_result = LOGO_EDITOR_ACCEPT;
    mock_editor_called = false;

    // Clear WiFi state
    mock_state.wifi.connected = false;
    mock_state.wifi.ssid[0] = '\0';
    mock_state.wifi.ip_address[0] = '\0';
    mock_state.wifi.connect_result = 0;      // Default to success
    mock_state.wifi.disconnect_result = 0;   // Default to success
    mock_state.wifi.scan_result_count = 0;
    mock_state.wifi.scan_return_value = 0;   // Default to success
    memset(mock_state.wifi.mac, 0, 6);
    mock_state.wifi.mac_available = false;

    // Initialize network state
    mock_state.network.ping_result_ms = -1;  // Default to ping failure
    mock_state.network.last_ping_ip[0] = '\0';

    // Initialize TCP state (memset already zeroed buffers/lengths)
    mock_state.tcp.connect_success = true;   // Default to a successful connect
    mock_state.tcp.timeout_after = -1;       // Timeout trigger disabled
    mock_state.tcp.close_after = -1;         // Mid-stream close trigger disabled
    mock_state.tcp.read_chunk = 0;           // Unlimited bytes per read

    // Initialize time state to defaults
    mock_state.time.year = 2025;
    mock_state.time.month = 1;
    mock_state.time.day = 1;
    mock_state.time.hour = 0;
    mock_state.time.minute = 0;
    mock_state.time.second = 0;
    mock_state.time.get_date_enabled = true;
    mock_state.time.get_time_enabled = true;
    mock_state.time.set_date_enabled = true;
    mock_state.time.set_time_enabled = true;
}

const MockDeviceState *mock_device_get_state(void)
{
    return &mock_state;
}

const MockTurtleState *mock_device_get_turtle(uint8_t n)
{
    turtle_slot_sync();  // The selected turtle's slot may be stale
    return &mock_state.turtles[n < MOCK_MAX_TURTLES ? n : 0];
}

void mock_device_set_snap_result(bool result)
{
    mock_state.costume.snap_result = result;
}

void mock_device_set_raster(uint8_t n, const LogoTurtleRaster *raster)
{
    if (n >= MOCK_MAX_TURTLES)
    {
        return;
    }
    MockRaster *r = &mock_state.sensing.rasters[n];
    // Reject empty or oversized rasters: the device caps costumes at
    // MOCK_RASTER_MAX, and storing a larger w/h would let core index past
    // the fixed mask buffer.
    if (!raster || !raster->mask || raster->w == 0 || raster->h == 0 ||
        raster->w > MOCK_RASTER_MAX || raster->h > MOCK_RASTER_MAX)
    {
        r->set = false;
        return;
    }
    r->set = true;
    r->x = raster->x;
    r->y = raster->y;
    r->cx = raster->cx;
    r->cy = raster->cy;
    r->w = raster->w;
    r->h = raster->h;
    r->indexed = raster->indexed;
    r->visible = raster->visible;
    memcpy(r->mask, raster->mask, (size_t)raster->w * raster->h);
}

void mock_device_set_canvas_point(int x, int y, uint8_t index)
{
    if (x < 0 || x >= MOCK_SCREEN_WIDTH_PX || y < 0 || y >= MOCK_SCREEN_HEIGHT_PX)
    {
        return;
    }
    mock_state.sensing.canvas[y * MOCK_SCREEN_WIDTH_PX + x] = index;
}

void mock_device_paint_canvas(int x, int y, int w, int h, uint8_t index)
{
    for (int j = y; j < y + h; j++)
    {
        for (int i = x; i < x + w; i++)
        {
            mock_device_set_canvas_point(i, j, index);
        }
    }
}

LogoConsole *mock_device_get_console(void)
{
    return &mock_console;
}

int mock_device_command_count(void)
{
    return mock_state.command_count;
}

const MockCommand *mock_device_get_command(int index)
{
    if (index < 0 || index >= mock_state.command_count)
    {
        return NULL;
    }
    return &mock_state.commands[index];
}

const MockCommand *mock_device_last_command(void)
{
    if (mock_state.command_count == 0)
    {
        return NULL;
    }
    return &mock_state.commands[mock_state.command_count - 1];
}

void mock_device_clear_commands(void)
{
    mock_state.command_count = 0;
}

int mock_device_dot_count(void)
{
    return mock_state.graphics.dot_count;
}

const MockDot *mock_device_get_dot(int index)
{
    if (index < 0 || index >= mock_state.graphics.dot_count)
    {
        return NULL;
    }
    return &mock_state.graphics.dots[index];
}

int mock_device_line_count(void)
{
    return mock_state.graphics.line_count;
}

const MockLine *mock_device_get_line(int index)
{
    if (index < 0 || index >= mock_state.graphics.line_count)
    {
        return NULL;
    }
    return &mock_state.graphics.lines[index];
}

void mock_device_clear_graphics(void)
{
    mock_state.graphics.cleared = false;
    mock_state.graphics.dot_count = 0;
    mock_state.graphics.line_count = 0;
}

bool mock_device_verify_position(float x, float y, float tolerance)
{
    return fabsf(mock_state.turtle.x - x) < tolerance &&
           fabsf(mock_state.turtle.y - y) < tolerance;
}

bool mock_device_verify_heading(float heading, float tolerance)
{
    float normalized = normalize_angle(heading);
    return fabsf(mock_state.turtle.heading - normalized) < tolerance;
}

bool mock_device_has_line_from_to(float x1, float y1, float x2, float y2, float tolerance)
{
    for (int i = 0; i < mock_state.graphics.line_count; i++)
    {
        const MockLine *line = &mock_state.graphics.lines[i];
        // Check forward direction
        if (fabsf(line->x1 - x1) < tolerance &&
            fabsf(line->y1 - y1) < tolerance &&
            fabsf(line->x2 - x2) < tolerance &&
            fabsf(line->y2 - y2) < tolerance)
        {
            return true;
        }
        // Check reverse direction
        if (fabsf(line->x1 - x2) < tolerance &&
            fabsf(line->y1 - y2) < tolerance &&
            fabsf(line->x2 - x1) < tolerance &&
            fabsf(line->y2 - y1) < tolerance)
        {
            return true;
        }
    }
    return false;
}

bool mock_device_has_dot_at(float x, float y, float tolerance)
{
    for (int i = 0; i < mock_state.graphics.dot_count; i++)
    {
        if (fabsf(mock_state.graphics.dots[i].x - x) < tolerance &&
            fabsf(mock_state.graphics.dots[i].y - y) < tolerance)
        {
            return true;
        }
    }
    return false;
}

//
// Additional helpers for I/O testing
//

void mock_device_set_input(const char *input)
{
    mock_input_buffer = input;
    mock_input_pos = 0;
}

const char *mock_device_get_output(void)
{
    return mock_output_buffer;
}

void mock_device_clear_output(void)
{
    mock_output_buffer[0] = '\0';
    mock_output_pos = 0;
}

//
// Graphics file I/O helpers for testing
//

void mock_device_set_gfx_save_result(int result)
{
    mock_state.gfx_io.gfx_save_result = result;
}

void mock_device_set_gfx_load_result(int result)
{
    mock_state.gfx_io.gfx_load_result = result;
}

const char *mock_device_get_last_gfx_save_filename(void)
{
    return mock_state.gfx_io.last_save_filename;
}

const char *mock_device_get_last_gfx_load_filename(void)
{
    return mock_state.gfx_io.last_load_filename;
}

int mock_device_get_gfx_save_call_count(void)
{
    return mock_state.gfx_io.gfx_save_call_count;
}

int mock_device_get_gfx_load_call_count(void)
{
    return mock_state.gfx_io.gfx_load_call_count;
}

//
// Palette helpers for testing
//

bool mock_device_verify_palette(uint8_t slot, uint8_t r, uint8_t g, uint8_t b)
{
    return mock_state.palette.r[slot] == r &&
           mock_state.palette.g[slot] == g &&
           mock_state.palette.b[slot] == b;
}

bool mock_device_was_restore_palette_called(void)
{
    return mock_state.palette.restore_palette_called;
}

//
// Editor helpers for testing
//

void mock_device_set_editor_result(LogoEditorResult result)
{
    mock_editor_result = result;
}

void mock_device_set_editor_content(const char *content)
{
    if (content)
    {
        strncpy(mock_editor_content, content, sizeof(mock_editor_content) - 1);
        mock_editor_content[sizeof(mock_editor_content) - 1] = '\0';
    }
    else
    {
        mock_editor_content[0] = '\0';
    }
}

const char *mock_device_get_editor_input(void)
{
    return mock_editor_input;
}

bool mock_device_was_editor_called(void)
{
    return mock_editor_called;
}

void mock_device_clear_editor(void)
{
    mock_editor_input[0] = '\0';
    mock_editor_content[0] = '\0';
    mock_editor_result = LOGO_EDITOR_ACCEPT;
    mock_editor_called = false;
}

//
// Mock WiFi operations (exposed for test_scaffold.c to use in mock_hardware_ops)
//

bool mock_wifi_is_connected(void)
{
    return mock_state.wifi.connected;
}

bool mock_wifi_connect(const char *ssid, const char *password)
{
    (void)password;  // Not stored in mock
    if (mock_state.wifi.connect_result == 0)
    {
        mock_state.wifi.connected = true;
        strncpy(mock_state.wifi.ssid, ssid, sizeof(mock_state.wifi.ssid) - 1);
        mock_state.wifi.ssid[sizeof(mock_state.wifi.ssid) - 1] = '\0';
        // Set a default IP when connected
        if (mock_state.wifi.ip_address[0] == '\0')
        {
            strncpy(mock_state.wifi.ip_address, "192.168.1.100", sizeof(mock_state.wifi.ip_address));
        }
        return true;
    }
    return false;
}

void mock_wifi_disconnect(void)
{
    if (mock_state.wifi.disconnect_result == 0)
    {
        mock_state.wifi.connected = false;
        mock_state.wifi.ssid[0] = '\0';
        mock_state.wifi.ip_address[0] = '\0';
    }
}

bool mock_wifi_get_ip(char *ip_buffer, size_t buffer_size)
{
    if (!mock_state.wifi.connected || mock_state.wifi.ip_address[0] == '\0')
    {
        return false;
    }
    strncpy(ip_buffer, mock_state.wifi.ip_address, buffer_size - 1);
    ip_buffer[buffer_size - 1] = '\0';
    return true;
}

bool mock_wifi_get_ssid(char *ssid_buffer, size_t buffer_size)
{
    if (!mock_state.wifi.connected || mock_state.wifi.ssid[0] == '\0')
    {
        return false;
    }
    strncpy(ssid_buffer, mock_state.wifi.ssid, buffer_size - 1);
    ssid_buffer[buffer_size - 1] = '\0';
    return true;
}

int mock_wifi_scan(char ssids[][33], int8_t strengths[], int max_networks)
{
    if (mock_state.wifi.scan_return_value != 0)
    {
        return -1;  // Error
    }
    
    int count = mock_state.wifi.scan_result_count;
    if (count > max_networks)
    {
        count = max_networks;
    }
    
    for (int i = 0; i < count; i++)
    {
        strncpy(ssids[i], mock_state.wifi.scan_results[i].ssid, 32);
        ssids[i][32] = '\0';
        strengths[i] = mock_state.wifi.scan_results[i].rssi;
    }
    return count;
}

bool mock_wifi_get_mac(uint8_t mac_buffer[6])
{
    if (!mock_state.wifi.mac_available)
    {
        return false;
    }
    memcpy(mac_buffer, mock_state.wifi.mac, 6);
    return true;
}

//
// Mock network operations (exposed for test_scaffold.c to use in mock_hardware_ops)
//

float mock_network_ping(const char *ip_address)
{
    // Store the IP address for verification
    if (ip_address)
    {
        strncpy(mock_state.network.last_ping_ip, ip_address, 
                sizeof(mock_state.network.last_ping_ip) - 1);
        mock_state.network.last_ping_ip[sizeof(mock_state.network.last_ping_ip) - 1] = '\0';
    }
    else
    {
        mock_state.network.last_ping_ip[0] = '\0';
    }
    
    return mock_state.network.ping_result_ms;
}

//
// Network test helpers
//

void mock_device_set_ping_result(float result_ms)
{
    mock_state.network.ping_result_ms = result_ms;
}

const char *mock_device_get_last_ping_ip(void)
{
    return mock_state.network.last_ping_ip;
}

void mock_device_set_resolve_result(const char *ip, bool success)
{
    mock_state.network.resolve_success = success;
    if (ip)
    {
        strncpy(mock_state.network.resolve_result_ip, ip,
                sizeof(mock_state.network.resolve_result_ip) - 1);
        mock_state.network.resolve_result_ip[sizeof(mock_state.network.resolve_result_ip) - 1] = '\0';
    }
    else
    {
        mock_state.network.resolve_result_ip[0] = '\0';
    }
}

const char *mock_device_get_last_resolve_hostname(void)
{
    return mock_state.network.last_resolve_hostname;
}

bool mock_network_resolve(const char *hostname, char *ip_buffer, size_t buffer_size)
{
    // Store the hostname for verification
    if (hostname)
    {
        strncpy(mock_state.network.last_resolve_hostname, hostname,
                sizeof(mock_state.network.last_resolve_hostname) - 1);
        mock_state.network.last_resolve_hostname[sizeof(mock_state.network.last_resolve_hostname) - 1] = '\0';
    }
    else
    {
        mock_state.network.last_resolve_hostname[0] = '\0';
    }

    if (mock_state.network.resolve_success && ip_buffer && buffer_size > 0)
    {
        strncpy(ip_buffer, mock_state.network.resolve_result_ip, buffer_size - 1);
        ip_buffer[buffer_size - 1] = '\0';
        return true;
    }

    return false;
}

//
// NTP test helpers
//

void mock_device_set_ntp_result(bool success)
{
    mock_state.network.ntp_success = success;
}

const char *mock_device_get_last_ntp_server(void)
{
    return mock_state.network.last_ntp_server;
}

float mock_device_get_last_ntp_timezone(void)
{
    return mock_state.network.last_ntp_timezone;
}

bool mock_network_ntp(const char *server, float timezone_offset)
{
    // Store the server for verification
    if (server)
    {
        strncpy(mock_state.network.last_ntp_server, server,
                sizeof(mock_state.network.last_ntp_server) - 1);
        mock_state.network.last_ntp_server[sizeof(mock_state.network.last_ntp_server) - 1] = '\0';
    }
    else
    {
        mock_state.network.last_ntp_server[0] = '\0';
    }

    // Store the timezone offset for verification
    mock_state.network.last_ntp_timezone = timezone_offset;

    return mock_state.network.ntp_success;
}

//
// TCP test helpers
//

void mock_device_set_tcp_connect_result(bool success)
{
    mock_state.tcp.connect_success = success;
}

void mock_device_set_tcp_response(const char *bytes, size_t len)
{
    if (len > sizeof(mock_state.tcp.response))
    {
        len = sizeof(mock_state.tcp.response);
    }
    if (bytes && len > 0)
    {
        memcpy(mock_state.tcp.response, bytes, len);
    }
    mock_state.tcp.response_len = (int)len;
    mock_state.tcp.read_pos = 0;
}

void mock_device_set_tcp_read_chunk(int max_bytes_per_read)
{
    mock_state.tcp.read_chunk = max_bytes_per_read;
}

void mock_device_set_tcp_write_chunk(int max_bytes_per_write)
{
    mock_state.tcp.write_chunk = max_bytes_per_write;
}

void mock_device_set_tcp_timeout_after(int bytes)
{
    mock_state.tcp.timeout_after = bytes;
}

void mock_device_set_tcp_close_after(int bytes)
{
    mock_state.tcp.close_after = bytes;
}

const char *mock_device_get_tcp_request(void)
{
    return mock_state.tcp.request;
}

size_t mock_device_get_tcp_request_len(void)
{
    return (size_t)mock_state.tcp.request_len;
}

const char *mock_device_get_last_tcp_ip(void)
{
    return mock_state.tcp.last_ip;
}

const char *mock_device_get_last_tls_host(void)
{
    return mock_state.tcp.last_tls_host;
}

uint16_t mock_device_get_last_tcp_port(void)
{
    return mock_state.tcp.last_port;
}

//
// Mock TCP operations (honour the contracts in devices/hardware.h)
//

void *mock_network_tcp_connect(const char *ip_address, uint16_t port, int timeout_ms)
{
    (void)timeout_ms;

    if (ip_address)
    {
        strncpy(mock_state.tcp.last_ip, ip_address, sizeof(mock_state.tcp.last_ip) - 1);
        mock_state.tcp.last_ip[sizeof(mock_state.tcp.last_ip) - 1] = '\0';
    }
    else
    {
        mock_state.tcp.last_ip[0] = '\0';
    }
    // Clear the TLS hostname so tests can tell the most recent connect went the
    // plaintext path, not a stale value from a prior HTTPS call.
    mock_state.tcp.last_tls_host[0] = '\0';
    mock_state.tcp.last_port = port;

    if (!mock_state.tcp.connect_success)
    {
        return NULL;
    }

    mock_state.tcp.open = true;
    // Return an opaque, non-NULL handle. The address of the state suffices.
    return &mock_state.tcp;
}

void *mock_network_tls_connect(const char *hostname, uint16_t port, int timeout_ms)
{
    (void)timeout_ms;

    if (hostname)
    {
        strncpy(mock_state.tcp.last_tls_host, hostname, sizeof(mock_state.tcp.last_tls_host) - 1);
        mock_state.tcp.last_tls_host[sizeof(mock_state.tcp.last_tls_host) - 1] = '\0';
    }
    else
    {
        mock_state.tcp.last_tls_host[0] = '\0';
    }
    // Clear the plaintext IP so the most-recent-connect path is unambiguous.
    mock_state.tcp.last_ip[0] = '\0';
    mock_state.tcp.last_port = port;

    if (!mock_state.tcp.connect_success)
    {
        return NULL;
    }

    mock_state.tcp.open = true;
    // TLS connections share the same opaque handle and read/write/close path.
    return &mock_state.tcp;
}

void mock_network_tcp_close(void *connection)
{
    (void)connection;
    mock_state.tcp.open = false;
}

int mock_network_tcp_read(void *connection, char *buffer, int count, int timeout_ms)
{
    (void)timeout_ms;

    if (!connection || !mock_state.tcp.open || !buffer || count <= 0)
    {
        return -1;
    }

    int pos = mock_state.tcp.read_pos;

    // Mid-stream close trigger fires once the cursor reaches close_after.
    if (mock_state.tcp.close_after >= 0 && pos >= mock_state.tcp.close_after)
    {
        return -1;
    }
    // Timeout trigger fires once the cursor reaches timeout_after.
    if (mock_state.tcp.timeout_after >= 0 && pos >= mock_state.tcp.timeout_after)
    {
        return 0;
    }

    int available = mock_state.tcp.response_len - pos;
    if (available <= 0)
    {
        // No more scripted data: peer closed the connection.
        return -1;
    }

    int to_read = count;
    if (to_read > available)
    {
        to_read = available;
    }
    if (mock_state.tcp.read_chunk > 0 && to_read > mock_state.tcp.read_chunk)
    {
        to_read = mock_state.tcp.read_chunk;
    }
    // Don't deliver past a pending trigger boundary, so the trigger fires after
    // exactly that many bytes have been read.
    if (mock_state.tcp.timeout_after >= 0 && pos < mock_state.tcp.timeout_after &&
        pos + to_read > mock_state.tcp.timeout_after)
    {
        to_read = mock_state.tcp.timeout_after - pos;
    }
    if (mock_state.tcp.close_after >= 0 && pos < mock_state.tcp.close_after &&
        pos + to_read > mock_state.tcp.close_after)
    {
        to_read = mock_state.tcp.close_after - pos;
    }

    memcpy(buffer, mock_state.tcp.response + pos, (size_t)to_read);
    mock_state.tcp.read_pos += to_read;
    return to_read;
}

int mock_network_tcp_write(void *connection, const char *data, int count)
{
    if (!connection || !mock_state.tcp.open || !data || count < 0)
    {
        return -1;
    }

    int space = (int)sizeof(mock_state.tcp.request) - 1 - mock_state.tcp.request_len;
    int to_store = count;
    if (to_store > space)
    {
        to_store = space;
    }
    // Optionally force a short write so callers must loop (exercises the
    // client's short-write handling).
    if (mock_state.tcp.write_chunk > 0 && to_store > mock_state.tcp.write_chunk)
    {
        to_store = mock_state.tcp.write_chunk;
    }
    if (to_store <= 0)
    {
        return -1; // No room to store any bytes -> write error (per hardware.h)
    }
    memcpy(mock_state.tcp.request + mock_state.tcp.request_len, data, (size_t)to_store);
    mock_state.tcp.request_len += to_store;
    mock_state.tcp.request[mock_state.tcp.request_len] = '\0';
    // Report the actual number of bytes stored (contract: bytes written).
    return to_store;
}

bool mock_network_tcp_can_read(void *connection)
{
    if (!connection || !mock_state.tcp.open)
    {
        return false;
    }
    return mock_state.tcp.read_pos < mock_state.tcp.response_len;
}

//
// WiFi test helpers
//

void mock_device_set_wifi_connected(bool connected)
{
    mock_state.wifi.connected = connected;
}

void mock_device_set_wifi_ssid(const char *ssid)
{
    if (ssid)
    {
        strncpy(mock_state.wifi.ssid, ssid, sizeof(mock_state.wifi.ssid) - 1);
        mock_state.wifi.ssid[sizeof(mock_state.wifi.ssid) - 1] = '\0';
    }
    else
    {
        mock_state.wifi.ssid[0] = '\0';
    }
}

void mock_device_set_wifi_ip(const char *ip)
{
    if (ip)
    {
        strncpy(mock_state.wifi.ip_address, ip, sizeof(mock_state.wifi.ip_address) - 1);
        mock_state.wifi.ip_address[sizeof(mock_state.wifi.ip_address) - 1] = '\0';
    }
    else
    {
        mock_state.wifi.ip_address[0] = '\0';
    }
}

void mock_device_set_wifi_connect_result(int result)
{
    mock_state.wifi.connect_result = result;
}

void mock_device_set_wifi_disconnect_result(int result)
{
    mock_state.wifi.disconnect_result = result;
}

void mock_device_add_wifi_scan_result(const char *ssid, int8_t rssi)
{
    if (mock_state.wifi.scan_result_count >= 16)
    {
        return;  // Full
    }
    int idx = mock_state.wifi.scan_result_count++;
    strncpy(mock_state.wifi.scan_results[idx].ssid, ssid,
            sizeof(mock_state.wifi.scan_results[idx].ssid) - 1);
    mock_state.wifi.scan_results[idx].ssid[sizeof(mock_state.wifi.scan_results[idx].ssid) - 1] = '\0';
    mock_state.wifi.scan_results[idx].rssi = rssi;
}

void mock_device_clear_wifi_scan_results(void)
{
    mock_state.wifi.scan_result_count = 0;
}

void mock_device_set_wifi_scan_result(int result)
{
    mock_state.wifi.scan_return_value = result;
}

void mock_device_set_wifi_mac(const uint8_t mac[6])
{
    memcpy(mock_state.wifi.mac, mac, 6);
    mock_state.wifi.mac_available = true;
}

// ============================================================================
// Mock time helpers
// ============================================================================

void mock_device_set_time(int year, int month, int day, int hour, int minute, int second)
{
    mock_state.time.year = year;
    mock_state.time.month = month;
    mock_state.time.day = day;
    mock_state.time.hour = hour;
    mock_state.time.minute = minute;
    mock_state.time.second = second;
}

void mock_device_set_time_enabled(bool get_date, bool get_time, bool set_date, bool set_time_flag)
{
    mock_state.time.get_date_enabled = get_date;
    mock_state.time.get_time_enabled = get_time;
    mock_state.time.set_date_enabled = set_date;
    mock_state.time.set_time_enabled = set_time_flag;
}

// ============================================================================
// Mock time operations (for use by test_scaffold in mock_hardware_ops)
// ============================================================================

bool mock_get_date(int *year, int *month, int *day)
{
    if (!mock_state.time.get_date_enabled)
    {
        return false;
    }
    if (year) *year = mock_state.time.year;
    if (month) *month = mock_state.time.month;
    if (day) *day = mock_state.time.day;
    return true;
}

bool mock_get_time(int *hour, int *minute, int *second)
{
    if (!mock_state.time.get_time_enabled)
    {
        return false;
    }
    if (hour) *hour = mock_state.time.hour;
    if (minute) *minute = mock_state.time.minute;
    if (second) *second = mock_state.time.second;
    return true;
}

bool mock_set_date(int year, int month, int day)
{
    if (!mock_state.time.set_date_enabled)
    {
        return false;
    }
    // Validate inputs
    if (month < 1 || month > 12 || day < 1 || day > 31)
    {
        return false;
    }
    mock_state.time.year = year;
    mock_state.time.month = month;
    mock_state.time.day = day;
    return true;
}

bool mock_set_time(int hour, int minute, int second)
{
    if (!mock_state.time.set_time_enabled)
    {
        return false;
    }
    // Validate inputs
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
    {
        return false;
    }
    mock_state.time.hour = hour;
    mock_state.time.minute = minute;
    mock_state.time.second = second;
    return true;
}
