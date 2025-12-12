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

// Mock console (contains embedded streams)
static LogoConsole mock_console;

// Mock I/O manager
static LogoIO mock_io;

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

    // Set up mock console (embeds streams internally)
    logo_console_init(&mock_console, &mock_input_stream_ops, &mock_output_stream_ops, NULL);
    
    // Set up mock I/O manager
    logo_io_init(&mock_io, &mock_console);
    
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
    mock_input_buffer = input;
    mock_input_pos = 0;
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
