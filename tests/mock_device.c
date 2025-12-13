//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Mock device implementation for testing turtle graphics and text screen.
//

#include "mock_device.h"
#include <string.h>
#include <math.h>

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

static void mock_turtle_move(float distance)
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
        if (new_x < -SCREEN_HALF_WIDTH || new_x >= SCREEN_HALF_WIDTH ||
            new_y < -SCREEN_HALF_HEIGHT || new_y >= SCREEN_HALF_HEIGHT)
        {
            mock_state.boundary_error = true;
            // Don't move
            return;
        }
        break;
        
    case MOCK_BOUNDARY_WINDOW:
        // Allow any position, no restrictions
        break;
        
    case MOCK_BOUNDARY_WRAP:
        // Wrap around edges
        while (new_x < -SCREEN_HALF_WIDTH) new_x += SCREEN_WIDTH;
        while (new_x >= SCREEN_HALF_WIDTH) new_x -= SCREEN_WIDTH;
        while (new_y < -SCREEN_HALF_HEIGHT) new_y += SCREEN_HEIGHT;
        while (new_y >= SCREEN_HALF_HEIGHT) new_y -= SCREEN_HEIGHT;
        break;
    }
    
    // Draw line if pen is down
    if (mock_state.turtle.pen_down && mock_state.turtle.pen_mode == MOCK_PEN_DOWN)
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
}

static void mock_turtle_home(void)
{
    // Draw line to home if pen is down
    if (mock_state.turtle.pen_down && mock_state.turtle.pen_mode == MOCK_PEN_DOWN)
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

static void mock_turtle_set_position(float x, float y)
{
    // Draw line to new position if pen is down
    if (mock_state.turtle.pen_down && mock_state.turtle.pen_mode == MOCK_PEN_DOWN)
    {
        if (mock_state.graphics.line_count < MOCK_MAX_LINES)
        {
            MockLine *line = &mock_state.graphics.lines[mock_state.graphics.line_count++];
            line->x1 = mock_state.turtle.x;
            line->y1 = mock_state.turtle.y;
            line->x2 = x;
            line->y2 = y;
            line->colour = mock_state.turtle.pen_colour;
        }
    }
    
    mock_state.turtle.x = x;
    mock_state.turtle.y = y;
    record_command_position(MOCK_CMD_SET_POSITION, x, y);
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

static void mock_turtle_set_pen_colour(uint16_t colour)
{
    mock_state.turtle.pen_colour = colour;
    record_command_colour(MOCK_CMD_SET_PEN_COLOUR, colour);
}

static uint16_t mock_turtle_get_pen_colour(void)
{
    return mock_state.turtle.pen_colour;
}

static void mock_turtle_set_bg_colour(uint16_t colour)
{
    mock_state.turtle.bg_colour = colour;
    record_command_colour(MOCK_CMD_SET_BG_COLOUR, colour);
}

static uint16_t mock_turtle_get_bg_colour(void)
{
    return mock_state.turtle.bg_colour;
}

static void mock_turtle_set_pen_down(bool down)
{
    mock_state.turtle.pen_down = down;
    if (down)
    {
        mock_state.turtle.pen_mode = MOCK_PEN_DOWN;
    }
    record_command_bool(MOCK_CMD_SET_PEN_DOWN, down);
}

static bool mock_turtle_get_pen_down(void)
{
    return mock_state.turtle.pen_down;
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

// Turtle operations structure
static const LogoConsoleTurtle mock_turtle_ops = {
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
    .set_pen_down = mock_turtle_set_pen_down,
    .get_pen_down = mock_turtle_get_pen_down,
    .set_visible = mock_turtle_set_visible,
    .get_visible = mock_turtle_get_visible,
    .dot = mock_turtle_dot,
    .dot_at = mock_turtle_dot_at,
    .fill = mock_turtle_fill,
    .set_fence = mock_turtle_set_fence,
    .set_window = mock_turtle_set_window,
    .set_wrap = mock_turtle_set_wrap
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

// Text operations structure
static const LogoConsoleText mock_text_ops = {
    .clear = mock_text_clear,
    .set_cursor = mock_text_set_cursor,
    .get_cursor = mock_text_get_cursor,
    .set_width = mock_text_set_width,
    .get_width = mock_text_get_width
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

// Screen mode operations structure
static const LogoConsoleScreen mock_screen_ops = {
    .fullscreen = mock_screen_fullscreen,
    .splitscreen = mock_screen_splitscreen,
    .textscreen = mock_screen_textscreen
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
    
    // Attach turtle, text, and screen operations
    mock_console.turtle = &mock_turtle_ops;
    mock_console.text = &mock_text_ops;
    mock_console.screen = &mock_screen_ops;
}

void mock_device_reset(void)
{
    memset(&mock_state, 0, sizeof(mock_state));
    
    // Initialize turtle to default state
    mock_state.turtle.x = 0.0f;
    mock_state.turtle.y = 0.0f;
    mock_state.turtle.heading = 0.0f;  // North
    mock_state.turtle.pen_down = true;
    mock_state.turtle.pen_mode = MOCK_PEN_DOWN;
    mock_state.turtle.pen_colour = 1;  // Default color (not black)
    mock_state.turtle.bg_colour = 0;   // Black background
    mock_state.turtle.visible = true;
    mock_state.turtle.boundary_mode = MOCK_BOUNDARY_WRAP;  // Default is wrap
    
    // Initialize text screen to default state
    mock_state.text.cursor_col = 0;
    mock_state.text.cursor_row = 0;
    mock_state.text.width = 40;
    mock_state.text.cleared = false;
    
    // Initialize screen mode
    mock_state.screen_mode = MOCK_SCREEN_TEXT;
    
    // Clear graphics state
    mock_state.graphics.cleared = false;
    mock_state.graphics.dot_count = 0;
    mock_state.graphics.line_count = 0;
    
    // Clear command history
    mock_state.command_count = 0;
    
    // Clear boundary error
    mock_state.boundary_error = false;
    
    // Clear I/O buffers
    mock_output_buffer[0] = '\0';
    mock_output_pos = 0;
    mock_input_buffer = NULL;
    mock_input_pos = 0;
}

const MockDeviceState *mock_device_get_state(void)
{
    return &mock_state;
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
