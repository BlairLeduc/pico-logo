//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#ifndef TEST_SCAFFOLD_H
#define TEST_SCAFFOLD_H

#include "unity.h"
#include "core/memory.h"
#include "core/lexer.h"
#include "core/eval.h"
#include "core/error.h"
#include "core/primitives.h"
#include "core/procedures.h"
#include "core/variables.h"
#include "devices/device.h"
#include <string.h>

// Buffer for capturing print output
static char output_buffer[1024];
static int output_pos;

// Mock device for capturing print output
static void mock_write(void *context, const char *text)
{
    (void)context;
    size_t len = strlen(text);
    if (output_pos + (int)len < (int)sizeof(output_buffer))
    {
        memcpy(output_buffer + output_pos, text, len);
        output_pos += len;
    }
    output_buffer[output_pos] = '\0';
}

static int mock_read_line(void *context, char *buffer, size_t buffer_size)
{
    (void)context;
    (void)buffer;
    (void)buffer_size;
    return -1;
}

static void mock_flush(void *context)
{
    (void)context;
}

static LogoDeviceOps mock_ops = {
    .read_line = mock_read_line,
    .write = mock_write,
    .flush = mock_flush,
    .fullscreen = NULL,
    .splitscreen = NULL,
    .textscreen = NULL
};

static LogoDevice mock_device;

// Declare the function to set device in primitives_output.c
extern void primitives_set_device(LogoDevice *device);
// Declare the function to set device in primitives_workspace.c
extern void primitives_workspace_set_device(LogoDevice *device);

// Common setUp function
static void test_scaffold_setUp(void)
{
    mem_init();
    primitives_init();
    procedures_init();
    variables_init();
    output_buffer[0] = '\0';
    output_pos = 0;

    // Set up mock device
    logo_device_init(&mock_device, &mock_ops, NULL);
    primitives_set_device(&mock_device);
    primitives_workspace_set_device(&mock_device);
}

// Common tearDown function (currently empty but available for extension)
static void test_scaffold_tearDown(void)
{
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

#endif // TEST_SCAFFOLD_H
