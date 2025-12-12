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
#include "devices/io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//==========================================================================
// Output helpers
//==========================================================================

static void print_to_writer(const char *str)
{
    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_write(io, str);
    }
}

static void flush_writer(void)
{
    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_flush(io);
    }
}

static void print_list_contents(Node node)
{
    bool first = true;
    while (!mem_is_nil(node))
    {
        if (!first)
            print_to_writer(" ");
        first = false;

        Node element = mem_car(node);
        if (mem_is_word(element))
        {
            print_to_writer(mem_word_ptr(element));
        }
        else if (mem_is_list(element))
        {
            print_to_writer("[");
            print_list_contents(element);
            print_to_writer("]");
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
        print_to_writer(buf);
        break;
    case VALUE_WORD:
        print_to_writer(mem_word_ptr(v.as.node));
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
        print_to_writer(buf);
        break;
    case VALUE_WORD:
        print_to_writer(mem_word_ptr(v.as.node));
        break;
    case VALUE_LIST:
        print_to_writer("[");
        print_list_contents(v.as.node);
        print_to_writer("]");
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
    (void)eval;
    (void)argc;
    (void)args;

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL)); // Empty list on EOF
    }

    int ch = logo_io_read_char(io);
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

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_word(mem_atom("", 0))); // Empty word on error
    }

    char buffer[1024];
    int len = logo_io_read_line(io, buffer, sizeof(buffer));
    if (len < 0)
    {
        // EOF - return empty word
        return result_ok(value_word(mem_atom("", 0)));
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

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL)); // Empty list on error/EOF
    }

    char buffer[1024];
    int len = logo_io_read_line(io, buffer, sizeof(buffer));
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
// File management primitives
//==========================================================================

// open file - opens file for read/write, creates if doesn't exist
static Result prim_open(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "open", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        // Check if we hit the file limit
        if (logo_io_open_count(io) >= LOGO_MAX_OPEN_FILES)
        {
            return result_error(ERR_NO_FILE_BUFFERS);
        }
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// openread file - opens file for reading only
static Result prim_openread(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "openread", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    LogoStream *stream = logo_io_open_read(io, pathname);
    if (!stream)
    {
        if (logo_io_open_count(io) >= LOGO_MAX_OPEN_FILES)
        {
            return result_error(ERR_NO_FILE_BUFFERS);
        }
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    return result_none();
}

// openwrite file - opens file for writing, creates/truncates
static Result prim_openwrite(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "openwrite", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    LogoStream *stream = logo_io_open_write(io, pathname);
    if (!stream)
    {
        if (logo_io_open_count(io) >= LOGO_MAX_OPEN_FILES)
        {
            return result_error(ERR_NO_FILE_BUFFERS);
        }
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// openappend file - opens file for appending
static Result prim_openappend(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "openappend", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    LogoStream *stream = logo_io_open_append(io, pathname);
    if (!stream)
    {
        if (logo_io_open_count(io) >= LOGO_MAX_OPEN_FILES)
        {
            return result_error(ERR_NO_FILE_BUFFERS);
        }
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// openupdate file - opens file for reading and writing
static Result prim_openupdate(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "openupdate", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    LogoStream *stream = logo_io_open_update(io, pathname);
    if (!stream)
    {
        if (logo_io_open_count(io) >= LOGO_MAX_OPEN_FILES)
        {
            return result_error(ERR_NO_FILE_BUFFERS);
        }
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    return result_none();
}

// close file - closes the named file
static Result prim_close(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "close", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Check if file is open
    if (!logo_io_is_open(io, pathname))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    logo_io_close(io, pathname);
    return result_none();
}

// closeall - closes all open files (not dribble)
static Result prim_closeall(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_close_all(io);
    }

    return result_none();
}

// setread file - sets current reader to file (empty list for keyboard)
static Result prim_setread(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Empty list means reset to keyboard
    if (args[0].type == VALUE_LIST && mem_is_nil(args[0].as.node))
    {
        logo_io_set_reader(io, NULL);
        return result_none();
    }

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setread", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    
    // Find the open stream
    LogoStream *stream = logo_io_find_open(io, pathname);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    logo_io_set_reader(io, stream);
    return result_none();
}

// setwrite file - sets current writer to file (empty list for screen)
static Result prim_setwrite(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Empty list means reset to screen
    if (args[0].type == VALUE_LIST && mem_is_nil(args[0].as.node))
    {
        logo_io_set_writer(io, NULL);
        return result_none();
    }

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setwrite", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    
    // Find the open stream
    LogoStream *stream = logo_io_find_open(io, pathname);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    logo_io_set_writer(io, stream);
    return result_none();
}

// reader - outputs the current reader name (empty list for keyboard)
static Result prim_reader(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    if (logo_io_reader_is_keyboard(io))
    {
        return result_ok(value_list(NODE_NIL));
    }

    const char *name = logo_io_get_reader_name(io);
    return result_ok(value_word(mem_atom_cstr(name)));
}

// writer - outputs the current writer name (empty list for screen)
static Result prim_writer(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    if (logo_io_writer_is_screen(io))
    {
        return result_ok(value_list(NODE_NIL));
    }

    const char *name = logo_io_get_writer_name(io);
    return result_ok(value_word(mem_atom_cstr(name)));
}

// allopen - outputs a list of all open files
static Result prim_allopen(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_ok(value_list(NODE_NIL));
    }

    // Build list of open file names
    Node list = NODE_NIL;
    int count = logo_io_open_count(io);
    
    // Build in reverse order so first file ends up first in list
    for (int i = count - 1; i >= 0; i--)
    {
        LogoStream *stream = logo_io_get_open(io, i);
        if (stream)
        {
            Node name_node = mem_atom_cstr(stream->name);
            list = mem_cons(name_node, list);
        }
    }

    return result_ok(value_list(list));
}

// readpos - outputs the current read position in the current file
static Result prim_readpos(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    LogoIO *io = primitives_get_io();
    if (!io || !io->reader)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Error if reader is keyboard
    if (logo_io_reader_is_keyboard(io))
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    long pos = logo_stream_get_read_pos(io->reader);
    if (pos < 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_ok(value_number((float)pos));
}

// setreadpos integer - sets the read position in the current file
static Result prim_setreadpos(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    float pos_f;
    if (!value_to_number(args[0], &pos_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setreadpos", value_to_string(args[0]));
    }
    long pos = (long)pos_f;
    if (pos < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setreadpos", value_to_string(args[0]));
    }

    LogoIO *io = primitives_get_io();
    if (!io || !io->reader)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Error if reader is keyboard
    if (logo_io_reader_is_keyboard(io))
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    if (!logo_stream_set_read_pos(io->reader, pos))
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// writepos - outputs the current write position in the current file
static Result prim_writepos(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    LogoIO *io = primitives_get_io();
    if (!io || !io->writer)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Error if writer is screen
    if (logo_io_writer_is_screen(io))
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    long pos = logo_stream_get_write_pos(io->writer);
    if (pos < 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_ok(value_number((float)pos));
}

// setwritepos integer - sets the write position in the current file
static Result prim_setwritepos(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    float pos_f;
    if (!value_to_number(args[0], &pos_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setwritepos", value_to_string(args[0]));
    }
    long pos = (long)pos_f;
    if (pos < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setwritepos", value_to_string(args[0]));
    }

    LogoIO *io = primitives_get_io();
    if (!io || !io->writer)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Error if writer is screen
    if (logo_io_writer_is_screen(io))
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    if (!logo_stream_set_write_pos(io->writer, pos))
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// filelen pathname - outputs the length in bytes of the file
// The file must be open
static Result prim_filelen(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "filelen", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Find the open stream
    LogoStream *stream = logo_io_find_open(io, pathname);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    long len = logo_stream_get_length(stream);
    if (len < 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_ok(value_number((float)len));
}

// dribble file - starts dribbling output to file
static Result prim_dribble(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    if (args[0].type != VALUE_WORD)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "dribble", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    if (!logo_io_start_dribble(io, pathname))
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// nodribble - stops dribbling
static Result prim_nodribble(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_stop_dribble(io);
    }

    return result_none();
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
            print_to_writer(" ");
        print_value(args[i]);
    }
    print_to_writer("\n");
    flush_writer();
    return result_none();
}

// show object - prints object followed by newline
// Lists keep their brackets.
static Result prim_show(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    show_value(args[0]);
    print_to_writer("\n");
    flush_writer();
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
            print_to_writer(" ");
        print_value(args[i]);
    }
    flush_writer();
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

    // File management
    primitive_register("open", 1, prim_open);
    primitive_register("openread", 1, prim_openread);
    primitive_register("openwrite", 1, prim_openwrite);
    primitive_register("openappend", 1, prim_openappend);
    primitive_register("openupdate", 1, prim_openupdate);
    primitive_register("close", 1, prim_close);
    primitive_register("closeall", 0, prim_closeall);
    primitive_register("setread", 1, prim_setread);
    primitive_register("setwrite", 1, prim_setwrite);
    primitive_register("reader", 0, prim_reader);
    primitive_register("writer", 0, prim_writer);
    primitive_register("allopen", 0, prim_allopen);
    primitive_register("readpos", 0, prim_readpos);
    primitive_register("setreadpos", 1, prim_setreadpos);
    primitive_register("writepos", 0, prim_writepos);
    primitive_register("setwritepos", 1, prim_setwritepos);
    primitive_register("filelen", 1, prim_filelen);
    primitive_register("dribble", 1, prim_dribble);
    primitive_register("nodribble", 0, prim_nodribble);
}
