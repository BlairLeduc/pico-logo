//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#pragma once

#include "unity.h"
#include "core/memory.h"
#include "core/lexer.h"
#include "core/eval.h"
#include "core/error.h"
#include "core/primitives.h"
#include "core/procedures.h"
#include "core/variables.h"
#include "core/properties.h"
#include "devices/stream.h"
#include "devices/console.h"
#include "devices/io.h"
#include "devices/hardware.h"
#include "devices/storage.h"
#include "mock_device.h"
#include <string.h>

// Buffer for capturing print output
static char output_buffer[1024];
static int output_pos;

// Buffer for simulated input
static const char *mock_input_buffer = NULL;
static size_t mock_input_pos = 0;

// ============================================================================
// Mock Stream Operations (for LogoStream interface)
// ============================================================================

// Forward declarations (streams need to be declared before ops use them)
// Note: LogoStreamOps callbacks take LogoStream* as first parameter

static int mock_stream_read_char(LogoStream *stream)
{
    (void)stream;
    if (!mock_input_buffer || mock_input_buffer[mock_input_pos] == '\0')
    {
        return -1; // EOF
    }
    return (unsigned char)mock_input_buffer[mock_input_pos++];
}

static int mock_stream_read_chars(LogoStream *stream, char *buffer, int count)
{
    (void)stream;
    if (!mock_input_buffer)
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
        return -1; // EOF
    }
    
    // Read until newline or end of input
    size_t i = 0;
    while (i < buffer_size - 1 && mock_input_buffer[mock_input_pos] != '\0')
    {
        char c = mock_input_buffer[mock_input_pos++];
        buffer[i++] = c;
        if (c == '\n')
            break;
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
    size_t len = strlen(text);
    if (output_pos + (int)len < (int)sizeof(output_buffer))
    {
        memcpy(output_buffer + output_pos, text, len);
        output_pos += len;
    }
    output_buffer[output_pos] = '\0';
}

static void mock_stream_flush(LogoStream *stream)
{
    (void)stream;
}

static void mock_stream_close(LogoStream *stream)
{
    (void)stream;
}

// Stream operations for mock input stream
static LogoStreamOps mock_input_stream_ops = {
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

// Stream operations for mock output stream
static LogoStreamOps mock_output_stream_ops = {
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

// Mock hardware operation implementations
static void mock_sleep(int milliseconds)
{
    (void)milliseconds;
    // No-op for testing
}

static uint32_t mock_random(void)
{
    return 42; // Fixed value for testing
}

// Mock battery state for testing
static int mock_battery_level = 100;
static bool mock_battery_charging = false;

static void mock_get_battery_level(int *level, bool *charging)
{
    *level = mock_battery_level;
    *charging = mock_battery_charging;
}

// Helper to set mock battery level for testing
static void set_mock_battery(int level, bool charging)
{
    mock_battery_level = level;
    mock_battery_charging = charging;
}

// User interrupt flag for testing
static bool mock_user_interrupt = false;

static bool mock_check_user_interrupt(void)
{
    return mock_user_interrupt;
}

static void mock_clear_user_interrupt(void)
{
    mock_user_interrupt = false;
}

// Pause request flag for testing (F9 key)
static bool mock_pause_requested = false;

static bool mock_check_pause_request(void)
{
    return mock_pause_requested;
}

static void mock_clear_pause_request(void)
{
    mock_pause_requested = false;
}

// Mock power_off state for testing
static bool mock_power_off_available = false;
static bool mock_power_off_result = false;
static bool mock_power_off_called = false;

static bool mock_power_off(void)
{
    mock_power_off_called = true;
    return mock_power_off_result;
}

static LogoHardwareOps mock_hardware_ops = {
    .sleep = mock_sleep,
    .random = mock_random,
    .get_battery_level = mock_get_battery_level,
    .power_off = NULL,  // Default: not available, use set_mock_power_off() to enable
    .check_user_interrupt = mock_check_user_interrupt,
    .clear_user_interrupt = mock_clear_user_interrupt,
    .check_pause_request = mock_check_pause_request,
    .clear_pause_request = mock_clear_pause_request,
    .toot = NULL,  // Mock: no audio
};

// Helper to configure mock power_off for testing
static void set_mock_power_off(bool available, bool result)
{
    mock_power_off_available = available;
    mock_power_off_result = result;
    mock_power_off_called = false;
    mock_hardware_ops.power_off = mock_power_off_available ? mock_power_off : NULL;
}

static bool was_mock_power_off_called(void)
{
    return mock_power_off_called;
}

// Mock console (contains embedded streams)
static LogoConsole mock_console;

// Mock hardware
static LogoHardware mock_hardware;

// Mock I/O manager
static LogoIO mock_io;

// Flag to track if we're using mock_device for turtle/text testing
static bool use_mock_device = false;

// Common setUp function
static void test_scaffold_setUp(void)
{
    mem_init();
    primitives_init();
    procedures_init();
    variables_init();
    properties_init();
    output_buffer[0] = '\0';
    output_pos = 0;
    mock_input_buffer = NULL;
    mock_input_pos = 0;
    use_mock_device = false;
    mock_user_interrupt = false;  // Reset user interrupt flag
    mock_pause_requested = false; // Reset pause request flag
    mock_battery_level = 100;     // Reset mock battery state
    mock_battery_charging = false;
    
    // Reset mock power_off state (default: not available)
    mock_power_off_available = false;
    mock_power_off_result = false;
    mock_power_off_called = false;
    mock_hardware_ops.power_off = NULL;

    // Set up mock console (embeds streams internally)
    logo_console_init(&mock_console, &mock_input_stream_ops, &mock_output_stream_ops, NULL);
    
    // Set up mock hardware
    logo_hardware_init(&mock_hardware, &mock_hardware_ops);
    
    // Set up mock I/O manager
    logo_io_init(&mock_io, &mock_console, NULL, &mock_hardware);
    
    // Register I/O with primitives
    primitives_set_io(&mock_io);
}

// Setup function that enables turtle/text mock device
static void test_scaffold_setUp_with_device(void)
{
    mem_init();
    primitives_init();
    procedures_init();
    variables_init();
    properties_init();
    output_buffer[0] = '\0';
    output_pos = 0;
    use_mock_device = true;

    // Initialize the mock device with turtle, text, and screen capabilities
    mock_device_init();
    
    // Set up mock I/O manager with the mock device's console
    logo_io_init(&mock_io, mock_device_get_console(), NULL, NULL);
    
    // Register I/O with primitives
    primitives_set_io(&mock_io);
}

// Common tearDown function (currently empty but available for extension)
static void test_scaffold_tearDown(void)
{
}

// Helper to set mock input for testing input primitives
static void set_mock_input(const char *input)
{
    if (use_mock_device)
    {
        mock_device_set_input(input);
    }
    else
    {
        mock_input_buffer = input;
        mock_input_pos = 0;
    }
}

// Helper to evaluate and return result
static Result eval_string(const char *input)
{
    Lexer lexer;
    Evaluator eval;
    lexer_init(&lexer, input);
    eval_init(&eval, &lexer);
    return eval_expression(&eval);
}

// Helper to run instructions
static Result run_string(const char *input)
{
    Lexer lexer;
    Evaluator eval;
    lexer_init(&lexer, input);
    eval_init(&eval, &lexer);

    Result r = result_none();
    while (!eval_at_end(&eval))
    {
        r = eval_instruction(&eval);
        if (r.status == RESULT_ERROR)
            break;
    }
    return r;
}

// Helper to reset output buffer
static void reset_output(void)
{
    output_buffer[0] = '\0';
    output_pos = 0;
}

// Helper to define a procedure using define primitive
static void define_proc(const char *name, const char **params, int param_count, const char *body_str)
{
    // Build body list from string
    Lexer lexer;
    lexer_init(&lexer, body_str);
    
    Node body = NODE_NIL;
    Node body_tail = NODE_NIL;
    
    while (true)
    {
        Token t = lexer_next_token(&lexer);
        if (t.type == TOKEN_EOF)
            break;
        
        Node item = NODE_NIL;
        if (t.type == TOKEN_WORD || t.type == TOKEN_NUMBER)
        {
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_QUOTED)
        {
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_COLON)
        {
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_PLUS)
        {
            item = mem_atom("+", 1);
        }
        else if (t.type == TOKEN_MINUS)
        {
            item = mem_atom("-", 1);
        }
        else if (t.type == TOKEN_MULTIPLY)
        {
            item = mem_atom("*", 1);
        }
        else if (t.type == TOKEN_DIVIDE)
        {
            item = mem_atom("/", 1);
        }
        else if (t.type == TOKEN_EQUALS)
        {
            item = mem_atom("=", 1);
        }
        else if (t.type == TOKEN_LESS_THAN)
        {
            item = mem_atom("<", 1);
        }
        else if (t.type == TOKEN_GREATER_THAN)
        {
            item = mem_atom(">", 1);
        }
        else if (t.type == TOKEN_LEFT_BRACKET)
        {
            // Parse nested list
            item = mem_atom("[", 1);
        }
        else if (t.type == TOKEN_RIGHT_BRACKET)
        {
            item = mem_atom("]", 1);
        }
        
        if (!mem_is_nil(item))
        {
            Node new_cons = mem_cons(item, NODE_NIL);
            if (mem_is_nil(body))
            {
                body = new_cons;
                body_tail = new_cons;
            }
            else
            {
                mem_set_cdr(body_tail, new_cons);
                body_tail = new_cons;
            }
        }
    }
    
    // Intern the name
    Node name_atom = mem_atom(name, strlen(name));
    const char *interned_name = mem_word_ptr(name_atom);
    
    proc_define(interned_name, params, param_count, body);
}
