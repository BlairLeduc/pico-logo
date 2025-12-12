//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Files primitives: open, openread, openwrite, openappend, openupdate, close, closeall,
//                    setread, setwrite, reader, writer, allopen, readpos, setreadpos,
//                    writepos, setwritepos, filelen, dribble, nodribble, load, save
//

#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "properties.h"
#include "error.h"
#include "eval.h"
#include "lexer.h"
#include "devices/io.h"
#include "devices/host/host_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
// File I/O: load and save
//==========================================================================

// Helper to check if a line starts with "to " (case-insensitive)
static bool line_starts_with_to(const char *line)
{
    // Skip leading whitespace
    while (*line && isspace((unsigned char)*line))
        line++;

    if (strncasecmp(line, "to", 2) != 0)
        return false;

    // Must be followed by whitespace or end of line
    char c = line[2];
    return c == '\0' || isspace((unsigned char)c);
}

// Helper to check if a line is just "end" (case-insensitive)
static bool line_is_end(const char *line)
{
    // Skip leading whitespace
    while (*line && isspace((unsigned char)*line))
        line++;

    if (strncasecmp(line, "end", 3) != 0)
        return false;

    // Must be followed by whitespace, newline, or end of string
    char c = line[3];
    return c == '\0' || isspace((unsigned char)c);
}

// Maximum line length for load
#define LOAD_MAX_LINE 256

// Maximum procedure buffer for load
#define LOAD_MAX_PROC 4096

// load pathname - loads and executes file contents
static Result prim_load(Evaluator *eval, int argc, Value *args)
{
    (void)argc;

    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "load", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Resolve path with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Check if file exists
    if (!logo_host_file_exists(full_path))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Open the file for reading
    LogoStream *stream = logo_io_open_read(io, full_path);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Read and execute the file line by line
    char line[LOAD_MAX_LINE];
    char proc_buffer[LOAD_MAX_PROC];
    size_t proc_len = 0;
    bool in_procedure_def = false;
    Result result = result_none();

    while (logo_stream_read_line(stream, line, sizeof(line)) >= 0)
    {
        // Strip trailing newline/carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[--len] = '\0';
        }

        // Skip empty lines
        if (len == 0)
        {
            continue;
        }

        // Handle multi-line procedure definitions
        if (!in_procedure_def && line_starts_with_to(line))
        {
            // Start collecting procedure definition
            in_procedure_def = true;
            proc_len = 0;

            // Copy the "to" line to buffer
            if (len < LOAD_MAX_PROC - 1)
            {
                memcpy(proc_buffer, line, len);
                proc_buffer[len] = ' '; // Space separator instead of newline
                proc_len = len + 1;
            }
            continue;
        }

        if (in_procedure_def)
        {
            if (line_is_end(line))
            {
                // Complete the procedure definition
                if (proc_len + 4 < LOAD_MAX_PROC)
                {
                    memcpy(proc_buffer + proc_len, "end", 3);
                    proc_len += 3;
                    proc_buffer[proc_len] = '\0';
                }

                in_procedure_def = false;

                // Parse and define the procedure
                Result r = proc_define_from_text(proc_buffer);
                if (r.status == RESULT_ERROR)
                {
                    result = r;
                    break;
                }

                proc_len = 0;
            }
            else
            {
                // Append line to procedure buffer with space separator
                if (proc_len + len + 1 < LOAD_MAX_PROC)
                {
                    memcpy(proc_buffer + proc_len, line, len);
                    proc_buffer[proc_len + len] = ' ';
                    proc_len += len + 1;
                }
            }
            continue;
        }

        // Regular instruction - evaluate it
        Lexer lexer;
        Evaluator load_eval;
        lexer_init(&lexer, line);
        eval_init(&load_eval, &lexer);

        // Evaluate all instructions on the line
        while (!eval_at_end(&load_eval))
        {
            Result r = eval_instruction(&load_eval);

            if (r.status == RESULT_ERROR)
            {
                result = r;
                break;
            }
            else if (r.status == RESULT_THROW)
            {
                // Propagate throw
                result = r;
                break;
            }
            else if (r.status == RESULT_OK)
            {
                // Expression returned a value - ignore in load
                // (Unlike REPL, we don't report "I don't know what to do with")
            }
            // RESULT_NONE means command completed successfully - continue
        }

        if (result.status != RESULT_NONE)
        {
            break;
        }
    }

    // Close the file
    logo_io_close(io, full_path);

    // If load was successful, check for startup variable
    if (result.status == RESULT_NONE)
    {
        Value startup_value;
        if (var_get("startup", &startup_value))
        {
            if (value_is_list(startup_value))
            {
                // Execute the startup list
                result = eval_run_list(eval, startup_value.as.node);
            }
        }
    }

    return result;
}

//==========================================================================
// Save helper functions
//==========================================================================

// Write a string to the current writer
static void save_write(const char *str)
{
    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_write(io, str);
    }
}

// Write a newline
static void save_newline(void)
{
    save_write("\n");
}

// Print a procedure body element (handles quoted words, etc.)
static void save_body_element(Node elem)
{
    if (mem_is_word(elem))
    {
        save_write(mem_word_ptr(elem));
    }
    else if (mem_is_list(elem))
    {
        save_write("[");
        bool first = true;
        Node curr = elem;
        while (!mem_is_nil(curr))
        {
            if (!first)
                save_write(" ");
            first = false;
            save_body_element(mem_car(curr));
            curr = mem_cdr(curr);
        }
        save_write("]");
    }
}

// Save procedure title line: to name :param1 :param2 ...
static void save_procedure_title(UserProcedure *proc)
{
    save_write("to ");
    save_write(proc->name);
    for (int i = 0; i < proc->param_count; i++)
    {
        save_write(" :");
        save_write(proc->params[i]);
    }
    save_newline();
}

// Save full procedure definition
static void save_procedure_definition(UserProcedure *proc)
{
    save_procedure_title(proc);

    // Print body
    Node curr = proc->body;
    while (!mem_is_nil(curr))
    {
        Node elem = mem_car(curr);
        save_body_element(elem);

        // Add space between elements
        Node next = mem_cdr(curr);
        if (!mem_is_nil(next))
        {
            save_write(" ");
        }
        curr = next;
    }
    save_newline();
    save_write("end");
    save_newline();
}

// Save a variable and its value
static void save_variable(const char *name, Value value)
{
    char buf[64];
    save_write("make \"");
    save_write(name);
    save_write(" ");

    switch (value.type)
    {
    case VALUE_NUMBER:
        snprintf(buf, sizeof(buf), "%g", value.as.number);
        save_write(buf);
        break;
    case VALUE_WORD:
        save_write("\"");
        save_write(mem_word_ptr(value.as.node));
        break;
    case VALUE_LIST:
        save_write("[");
        {
            bool first = true;
            Node curr = value.as.node;
            while (!mem_is_nil(curr))
            {
                if (!first)
                    save_write(" ");
                first = false;
                save_body_element(mem_car(curr));
                curr = mem_cdr(curr);
            }
        }
        save_write("]");
        break;
    default:
        break;
    }
    save_newline();
}

// Save a property as pprop command
static void save_property(const char *name, const char *property, Value value)
{
    char buf[64];
    save_write("pprop \"");
    save_write(name);
    save_write(" \"");
    save_write(property);
    save_write(" ");

    switch (value.type)
    {
    case VALUE_NUMBER:
        snprintf(buf, sizeof(buf), "%g", value.as.number);
        save_write(buf);
        break;
    case VALUE_WORD:
        save_write("\"");
        save_write(mem_word_ptr(value.as.node));
        break;
    case VALUE_LIST:
        save_write("[");
        {
            bool first = true;
            Node curr = value.as.node;
            while (!mem_is_nil(curr))
            {
                if (!first)
                    save_write(" ");
                first = false;
                save_body_element(mem_car(curr));
                curr = mem_cdr(curr);
            }
        }
        save_write("]");
        break;
    default:
        break;
    }
    save_newline();
}

// save pathname - saves all unburied procedures, variables, and properties
static Result prim_save(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "save", value_to_string(args[0]));
    }

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Resolve path with prefix
    char resolved[LOGO_STREAM_NAME_MAX];
    char *full_path = logo_io_resolve_path(io, pathname, resolved, sizeof(resolved));
    if (!full_path)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Check if file already exists
    if (logo_host_file_exists(full_path))
    {
        return result_error_arg(ERR_FILE_EXISTS, "", pathname);
    }

    // Open the file for writing
    LogoStream *stream = logo_io_open_write(io, full_path);
    if (!stream)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Save current writer and set to file
    LogoStream *old_writer = io->writer;
    logo_io_set_writer(io, stream);

    // Save all procedures (not buried)
    int proc_cnt = proc_count(true); // Get ALL, filter by buried in loop
    for (int i = 0; i < proc_cnt; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            save_procedure_definition(proc);
            save_newline();
        }
    }

    // Save all variables (not buried)
    int var_cnt = var_global_count(false);
    for (int i = 0; i < var_cnt; i++)
    {
        const char *name;
        Value value;
        if (var_get_global_by_index(i, false, &name, &value))
        {
            save_variable(name, value);
        }
    }

    // Save all property lists
    int prop_cnt = prop_name_count();
    for (int i = 0; i < prop_cnt; i++)
    {
        const char *name;
        if (prop_get_name_by_index(i, &name))
        {
            Node list = prop_get_list(name);
            // Property list is [prop1 val1 prop2 val2 ...]
            Node curr = list;
            while (!mem_is_nil(curr) && !mem_is_nil(mem_cdr(curr)))
            {
                Node prop_node = mem_car(curr);
                Node val_node = mem_car(mem_cdr(curr));

                if (mem_is_word(prop_node))
                {
                    const char *prop = mem_word_ptr(prop_node);
                    Value val;
                    if (mem_is_word(val_node))
                    {
                        val = value_word(val_node);
                    }
                    else if (mem_is_list(val_node))
                    {
                        val = value_list(val_node);
                    }
                    else
                    {
                        val = value_list(NODE_NIL);
                    }
                    save_property(name, prop, val);
                }

                curr = mem_cdr(mem_cdr(curr));
            }
        }
    }

    // Restore writer and close file
    logo_io_set_writer(io, old_writer == &io->console->output ? NULL : old_writer);
    logo_io_close(io, full_path);

    return result_none();
}

//==========================================================================
// Registration
//==========================================================================

void primitives_files_init(void)
{
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

    // Load and save
    primitive_register("load", 1, prim_load);
    primitive_register("save", 1, prim_save);
}
