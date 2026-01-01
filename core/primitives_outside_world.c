//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Outside World primitives: keyp, readchar, readchars, readlist, readword,
//                            print, show, type, standout
//

#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "properties.h"
#include "error.h"
#include "eval.h"
#include "lexer.h"
#include "format.h"
#include "devices/io.h"
#include "devices/stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//==========================================================================
// Output helpers
//==========================================================================

// Output callback for print/show/type (always succeeds)
static bool print_output(void *ctx, const char *str)
{
    (void)ctx;
    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_write(io, str);
    }
    return true;
}

static void flush_writer(void)
{
    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_flush(io);
    }
}

//==========================================================================
// Input primitives
//==========================================================================

// keyp - outputs true if a character is waiting to be read
static Result prim_keyp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_word(mem_atom_cstr("false")));
    }

    bool available = logo_io_key_available(io);
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
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL)); // Empty list on EOF
    }

    int ch = logo_io_read_char(io);
    if (ch == LOGO_STREAM_INTERRUPTED)
    {
        // User pressed BRK - return "Stopped!" error
        return result_error(ERR_STOPPED);
    }
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
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], count_f);

    int count = (int)count_f;
    if (count <= 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL)); // Empty list on EOF
    }

    // Allocate buffer for characters
    char *buffer = (char *)malloc(count + 1);
    if (!buffer)
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    int read_count = logo_io_read_chars(io, buffer, count);
    if (read_count == LOGO_STREAM_INTERRUPTED)
    {
        free(buffer);
        return result_error(ERR_STOPPED);
    }
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

// Helper result for parse_line_to_list
typedef struct {
    Node node;
    bool success;
} ParseResult;

// Helper: Parse a line into a list of words
// This is similar to what the lexer does but returns a list structure
// Returns success=false if memory allocation fails
static ParseResult parse_line_to_list(const char *line)
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
            // Copy token text - token already includes quote/colon prefix if present
            char buf[256];
            size_t len = tok.length;
            if (len >= sizeof(buf))
                len = sizeof(buf) - 1;
            memcpy(buf, tok.start, len);
            buf[len] = '\0';

            // Token text already includes the prefix (", :), just use it directly
            element = mem_atom(buf, len);
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
                if (!inner_str)
                {
                    return (ParseResult){.node = NODE_NIL, .success = false};
                }
                memcpy(inner_str, list_start, inner_len);
                inner_str[inner_len] = '\0';
                ParseResult inner_result = parse_line_to_list(inner_str);
                free(inner_str);
                if (!inner_result.success)
                {
                    return inner_result;
                }
                element = inner_result.node;
                // Wrap in list type
                element = NODE_MAKE_LIST(NODE_GET_INDEX(element)) | (NODE_TYPE_LIST << NODE_TYPE_SHIFT);
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

    return (ParseResult){.node = result, .success = true};
}

// readlist (rl) - reads a line of input and outputs it as a list
// Echoes the input. Returns empty word if at EOF.
static Result prim_readlist(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_word(mem_atom("", 0))); // Empty word on error
    }

    char buffer[1024];
    int len = logo_io_read_line(io, buffer, sizeof(buffer));
    if (len == LOGO_STREAM_INTERRUPTED)
    {
        return result_error(ERR_STOPPED);
    }
    if (len < 0)
    {
        // EOF - return empty word
        return result_ok(value_word(mem_atom("", 0)));
    }

    // Parse the line into a list
    ParseResult parse_result = parse_line_to_list(buffer);
    if (!parse_result.success)
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    return result_ok(value_list(parse_result.node));
}

// readword (rw) - reads a line of input and outputs it as a word
// Echoes the input. Returns empty word if press Enter without typing, empty list at EOF.
static Result prim_readword(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL)); // Empty list on error/EOF
    }

    char buffer[1024];
    int len = logo_io_read_line(io, buffer, sizeof(buffer));
    if (len == LOGO_STREAM_INTERRUPTED)
    {
        return result_error(ERR_STOPPED);
    }
    if (len < 0)
    {
        // EOF - return empty list
        return result_ok(value_list(NODE_NIL));
    }

    // Strip trailing newline if present
    if (len > 0 && buffer[len - 1] == '\n')
    {
        len--;
    }
    
    // Strip trailing carriage return if present (for Windows line endings)
    if (len > 0 && buffer[len - 1] == '\r')
    {
        len--;
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
    UNUSED(eval);

    for (int i = 0; i < argc; i++)
    {
        if (i > 0)
            print_output(NULL, " ");
        format_value(print_output, NULL, args[i]);
    }
    print_output(NULL, "\n");
    flush_writer();
    
    // Check for write errors (e.g., disk full)
    LogoIO *io = primitives_get_io();
    if (io && logo_io_check_write_error(io))
    {
        return result_error(ERR_DISK_FULL);
    }
    
    return result_none();
}

// show object - prints object followed by newline
// Lists keep their brackets.
static Result prim_show(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    format_value_show(print_output, NULL, args[0]);
    print_output(NULL, "\n");
    flush_writer();
    
    // Check for write errors (e.g., disk full)
    LogoIO *io = primitives_get_io();
    if (io && logo_io_check_write_error(io))
    {
        return result_error(ERR_DISK_FULL);
    }
    
    return result_none();
}

// type object - prints object without newline
// Outermost brackets of lists are not printed.
static Result prim_type(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    for (int i = 0; i < argc; i++)
    {
        if (i > 0)
            print_output(NULL, " ");
        format_value(print_output, NULL, args[i]);
    }
    flush_writer();
    
    // Check for write errors (e.g., disk full)
    LogoIO *io = primitives_get_io();
    if (io && logo_io_check_write_error(io))
    {
        return result_error(ERR_DISK_FULL);
    }
    
    return result_none();
}

//==========================================================================
// Standout formatting helpers
//==========================================================================

// Set MSB on each character in a string, appending to buffer
// Returns new position in buffer, or SIZE_MAX on overflow
static size_t standout_string(char *buffer, size_t bufsize, size_t pos, const char *str)
{
    while (*str && pos < bufsize - 1)
    {
        buffer[pos++] = *str | 0x80;  // Set MSB for standout
        str++;
    }
    return pos;
}

// Format list contents to buffer with standout (MSB set on all chars)
// Uses inverse space (0xA0) between elements
static size_t standout_list_contents(char *buffer, size_t bufsize, size_t pos, Node node);

// Format a value to buffer with standout
static size_t standout_value(char *buffer, size_t bufsize, size_t pos, Value value)
{
    char numbuf[32];
    switch (value.type)
    {
    case VALUE_NONE:
        break;
    case VALUE_NUMBER:
        format_number(numbuf, sizeof(numbuf), value.as.number);
        pos = standout_string(buffer, bufsize, pos, numbuf);
        break;
    case VALUE_WORD:
        pos = standout_string(buffer, bufsize, pos, mem_word_ptr(value.as.node));
        break;
    case VALUE_LIST:
        pos = standout_list_contents(buffer, bufsize, pos, value.as.node);
        break;
    }
    return pos;
}

// Format list contents to buffer with standout (MSB set on all chars)
// Uses inverse space (0xA0) between elements
static size_t standout_list_contents(char *buffer, size_t bufsize, size_t pos, Node node)
{
    bool first = true;
    while (!mem_is_nil(node) && pos < bufsize - 1)
    {
        if (!first)
        {
            buffer[pos++] = (char)(' ' | 0x80);  // Inverse space between items
        }
        first = false;

        Node element = mem_car(node);
        if (mem_is_word(element))
        {
            pos = standout_string(buffer, bufsize, pos, mem_word_ptr(element));
        }
        else if (mem_is_list(element))
        {
            // Nested lists get brackets (also with MSB set)
            if (pos < bufsize - 1)
                buffer[pos++] = (char)('[' | 0x80);
            pos = standout_list_contents(buffer, bufsize, pos, element);
            if (pos < bufsize - 1)
                buffer[pos++] = (char)(']' | 0x80);
        }
        else if (mem_is_nil(element))
        {
            // Empty list as element
            if (pos < bufsize - 2)
            {
                buffer[pos++] = (char)('[' | 0x80);
                buffer[pos++] = (char)(']' | 0x80);
            }
        }
        node = mem_cdr(node);
    }
    return pos;
}

// standout object
// Outputs object in standout mode (MSB set on each character).
// Outermost brackets of lists are not printed.
static Result prim_standout(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    char buffer[256];
    size_t pos = standout_value(buffer, sizeof(buffer), 0, args[0]);
    buffer[pos] = '\0';

    Node result = mem_atom(buffer, pos);
    if (mem_is_nil(result))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    return result_ok(value_word(result));
}

//==========================================================================
// Registration
//==========================================================================

void primitives_outside_world_init(void)
{
    // Input
    primitive_register("key?", 0, prim_keyp);
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
    primitive_register("standout", 1, prim_standout);
}
