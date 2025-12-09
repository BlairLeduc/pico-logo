//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Outside World primitives: keyp, readchar, readchars, readlist, readword,
//                            print, show, type
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "lexer.h"
#include "devices/device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration for the device - set by REPL
static LogoDevice *current_device = NULL;

void primitives_set_device(LogoDevice *device)
{
    current_device = device;
}

//==========================================================================
// Output helpers
//==========================================================================

static void print_to_device(const char *str)
{
    if (current_device)
    {
        logo_device_write(current_device, str);
    }
}

static void flush_device(void)
{
    if (current_device)
    {
        logo_device_flush(current_device);
    }
}

static void print_list_contents(Node node)
{
    bool first = true;
    while (!mem_is_nil(node))
    {
        if (!first)
            print_to_device(" ");
        first = false;

        Node element = mem_car(node);
        if (mem_is_word(element))
        {
            print_to_device(mem_word_ptr(element));
        }
        else if (mem_is_list(element))
        {
            print_to_device("[");
            print_list_contents(element);
            print_to_device("]");
        }
        node = mem_cdr(node);
    }
}

// Print value without outer brackets on lists
static void print_value(Value v)
{
    char buf[32];
    switch (v.type)
    {
    case VALUE_NONE:
        break;
    case VALUE_NUMBER:
        snprintf(buf, sizeof(buf), "%g", v.as.number);
        print_to_device(buf);
        break;
    case VALUE_WORD:
        print_to_device(mem_word_ptr(v.as.node));
        break;
    case VALUE_LIST:
        print_list_contents(v.as.node);
        break;
    }
}

// Print value with brackets around lists (for show)
static void show_value(Value v)
{
    char buf[32];
    switch (v.type)
    {
    case VALUE_NONE:
        break;
    case VALUE_NUMBER:
        snprintf(buf, sizeof(buf), "%g", v.as.number);
        print_to_device(buf);
        break;
    case VALUE_WORD:
        print_to_device(mem_word_ptr(v.as.node));
        break;
    case VALUE_LIST:
        print_to_device("[");
        print_list_contents(v.as.node);
        print_to_device("]");
        break;
    }
}

//==========================================================================
// Input primitives
//==========================================================================

// keyp - outputs true if a character is waiting to be read
static Result prim_keyp(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    if (!current_device)
    {
        return result_ok(value_word(mem_atom_cstr("false")));
    }

    bool available = logo_device_key_available(current_device);
    if (available)
    {
        return result_ok(value_word(mem_atom_cstr("true")));
    }
    return result_ok(value_word(mem_atom_cstr("false")));
}

// readchar (rc) - outputs the first character typed at the keyboard
// Does not echo the character.
// Returns empty list if reading from file and at EOF.
static Result prim_readchar(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    if (!current_device)
    {
        return result_ok(value_list(NODE_NIL)); // Empty list on EOF
    }

    int ch = logo_device_read_char(current_device);
    if (ch < 0)
    {
        // EOF - return empty list
        return result_ok(value_list(NODE_NIL));
    }

    // Return single character as a word
    char buf[2] = {(char)ch, '\0'};
    Node word = mem_atom(buf, 1);
    return result_ok(value_word(word));
}

// readchars integer (rcs integer) - outputs the first integer characters
// Does not echo the characters.
// Returns empty list if at EOF before reading any characters.
static Result prim_readchars(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    float count_f;
    if (!value_to_number(args[0], &count_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "readchars", value_to_string(args[0]));
    }
    int count = (int)count_f;
    if (count <= 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "readchars", value_to_string(args[0]));
    }

    if (!current_device)
    {
        return result_ok(value_list(NODE_NIL)); // Empty list on EOF
    }

    // Allocate buffer for characters
    char *buffer = (char *)malloc(count + 1);
    if (!buffer)
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    int read_count = logo_device_read_chars(current_device, buffer, count);
    if (read_count == 0)
    {
        free(buffer);
        return result_ok(value_list(NODE_NIL)); // Empty list on EOF
    }

    buffer[read_count] = '\0';
    Node word = mem_atom(buffer, read_count);
    free(buffer);
    return result_ok(value_word(word));
}

// Helper: Parse a line into a list of words
// This is similar to what the lexer does but returns a list structure
static Node parse_line_to_list(const char *line)
{
    Lexer lexer;
    lexer_init(&lexer, line);

    // Build list in reverse, then reverse it at the end
    Node result = NODE_NIL;
    Node *tail = &result;

    for (;;)
    {
        Token tok = lexer_next_token(&lexer);
        if (tok.type == TOKEN_EOF || tok.type == TOKEN_ERROR)
        {
            break;
        }

        Node element = NODE_NIL;

        switch (tok.type)
        {
        case TOKEN_WORD:
        case TOKEN_QUOTED:
        case TOKEN_NUMBER:
        case TOKEN_COLON:
        {
            // Copy token text
            char buf[256];
            size_t len = tok.length;
            if (len >= sizeof(buf))
                len = sizeof(buf) - 1;
            memcpy(buf, tok.start, len);
            buf[len] = '\0';

            // For quoted words, include the quote
            if (tok.type == TOKEN_QUOTED)
            {
                // Prepend quote mark
                char quoted[258];
                quoted[0] = '"';
                memcpy(quoted + 1, buf, len + 1);
                element = mem_atom_cstr(quoted);
            }
            else if (tok.type == TOKEN_COLON)
            {
                // Prepend colon
                char with_colon[258];
                with_colon[0] = ':';
                memcpy(with_colon + 1, buf, len + 1);
                element = mem_atom_cstr(with_colon);
            }
            else
            {
                element = mem_atom(buf, len);
            }
            break;
        }

        case TOKEN_LEFT_BRACKET:
        {
            // Parse nested list recursively
            // Find matching right bracket
            int depth = 1;
            const char *list_start = lexer.current;
            while (depth > 0 && !lexer_is_at_end(&lexer))
            {
                Token inner = lexer_next_token(&lexer);
                if (inner.type == TOKEN_LEFT_BRACKET)
                    depth++;
                else if (inner.type == TOKEN_RIGHT_BRACKET)
                    depth--;
                else if (inner.type == TOKEN_EOF)
                    break;
            }
            // list_start to current position (minus the ] we just consumed) forms the inner list
            // For simplicity, recursively parse
            size_t inner_len = lexer.current - list_start - 1; // Exclude the ]
            if (inner_len > 0)
            {
                char *inner_str = (char *)malloc(inner_len + 1);
                if (inner_str)
                {
                    memcpy(inner_str, list_start, inner_len);
                    inner_str[inner_len] = '\0';
                    element = parse_line_to_list(inner_str);
                    free(inner_str);
                    // Wrap in list type
                    element = NODE_MAKE_LIST(NODE_GET_INDEX(element)) | (NODE_TYPE_LIST << NODE_TYPE_SHIFT);
                }
            }
            else
            {
                element = NODE_NIL; // Empty list
            }
            // Mark as list value
            Node new_cell = mem_cons(element, NODE_NIL);
            *tail = new_cell;
            tail = &((Node *)&new_cell)[0]; // This won't work directly, need different approach
            continue; // Skip the normal append below
        }

        case TOKEN_RIGHT_BRACKET:
            // Shouldn't happen at top level, ignore
            continue;

        case TOKEN_PLUS:
            element = mem_atom_cstr("+");
            break;
        case TOKEN_MINUS:
        case TOKEN_UNARY_MINUS:
            element = mem_atom_cstr("-");
            break;
        case TOKEN_MULTIPLY:
            element = mem_atom_cstr("*");
            break;
        case TOKEN_DIVIDE:
            element = mem_atom_cstr("/");
            break;
        case TOKEN_EQUALS:
            element = mem_atom_cstr("=");
            break;
        case TOKEN_LESS_THAN:
            element = mem_atom_cstr("<");
            break;
        case TOKEN_GREATER_THAN:
            element = mem_atom_cstr(">");
            break;
        case TOKEN_LEFT_PAREN:
            element = mem_atom_cstr("(");
            break;
        case TOKEN_RIGHT_PAREN:
            element = mem_atom_cstr(")");
            break;

        default:
            continue;
        }

        // Append element to list
        Node new_cell = mem_cons(element, NODE_NIL);
        if (mem_is_nil(result))
        {
            result = new_cell;
        }
        else
        {
            // Find end of list and append
            Node current = result;
            while (!mem_is_nil(mem_cdr(current)))
            {
                current = mem_cdr(current);
            }
            mem_set_cdr(current, new_cell);
        }
    }

    return result;
}

// readlist (rl) - reads a line of input and outputs it as a list
// Echoes the input. Returns empty word if at EOF.
static Result prim_readlist(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    if (!current_device)
    {
        return result_ok(value_word(mem_atom("", 0))); // Empty word on error
    }

    char buffer[1024];
    if (!logo_device_read_line(current_device, buffer, sizeof(buffer)))
    {
        // EOF - return empty word
        return result_ok(value_word(mem_atom("", 0)));
    }

    // Remove trailing newline if present
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
    {
        buffer[--len] = '\0';
    }
    if (len > 0 && buffer[len - 1] == '\r')
    {
        buffer[--len] = '\0';
    }

    // Parse the line into a list
    Node list = parse_line_to_list(buffer);
    return result_ok(value_list(list));
}

// readword (rw) - reads a line of input and outputs it as a word
// Echoes the input. Returns empty word if press Enter without typing, empty list at EOF.
static Result prim_readword(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    if (!current_device)
    {
        return result_ok(value_list(NODE_NIL)); // Empty list on error/EOF
    }

    char buffer[1024];
    if (!logo_device_read_line(current_device, buffer, sizeof(buffer)))
    {
        // EOF - return empty list
        return result_ok(value_list(NODE_NIL));
    }

    // Remove trailing newline if present
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n')
    {
        buffer[--len] = '\0';
    }
    if (len > 0 && buffer[len - 1] == '\r')
    {
        buffer[--len] = '\0';
    }

    // Return as a single word (even if empty)
    Node word = mem_atom(buffer, len);
    return result_ok(value_word(word));
}

//==========================================================================
// Output primitives
//==========================================================================

// print object (pr object) - prints object followed by newline
// Outermost brackets of lists are not printed.
static Result prim_print(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    for (int i = 0; i < argc; i++)
    {
        if (i > 0)
            print_to_device(" ");
        print_value(args[i]);
    }
    print_to_device("\n");
    flush_device();
    return result_none();
}

// show object - prints object followed by newline
// Lists keep their brackets.
static Result prim_show(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    show_value(args[0]);
    print_to_device("\n");
    flush_device();
    return result_none();
}

// type object - prints object without newline
// Outermost brackets of lists are not printed.
static Result prim_type(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    for (int i = 0; i < argc; i++)
    {
        if (i > 0)
            print_to_device(" ");
        print_value(args[i]);
    }
    flush_device();
    return result_none();
}

//==========================================================================
// Registration
//==========================================================================

void primitives_outside_world_init(void)
{
    // Input
    primitive_register("keyp", 0, prim_keyp);
    primitive_register("readchar", 0, prim_readchar);
    primitive_register("rc", 0, prim_readchar);
    primitive_register("readchars", 1, prim_readchars);
    primitive_register("rcs", 1, prim_readchars);
    primitive_register("readlist", 0, prim_readlist);
    primitive_register("rl", 0, prim_readlist);
    primitive_register("readword", 0, prim_readword);
    primitive_register("rw", 0, prim_readword);

    // Output
    primitive_register("print", 1, prim_print);
    primitive_register("pr", 1, prim_print);
    primitive_register("show", 1, prim_show);
    primitive_register("type", 1, prim_type);
}
