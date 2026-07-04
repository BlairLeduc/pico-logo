//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  File load/save primitives: load, save, savel, savepic, loadpic, pofile
//

#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "properties.h"
#include "error.h"
#include "eval.h"
#include "format.h"
#include "lexer.h"
#include "repl.h"
#include "devices/io.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>

// Flag to prevent recursive loading
static bool loading_in_progress = false;

//==========================================================================
// Load helpers
//==========================================================================

// `to`-line and `end`-line detection are shared with the REPL; see
// repl_line_starts_with_to / repl_line_is_end in core/repl.h.

// Maximum line length for load
#define LOAD_MAX_LINE 256

// Maximum procedure buffer for load
#define LOAD_MAX_PROC 4096

// Static buffer for assembling procedure definitions during load.
// Safe as static because loading_in_progress prevents reentrancy.
static char load_proc_buffer[LOAD_MAX_PROC];

// load pathname - loads and executes file contents
static Result prim_load(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_WORD(args[0]);

    // Prevent recursive loading
    if (loading_in_progress)
    {
        return result_error(ERR_NO_FILE_BUFFERS);
    }

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file exists (logo_io_file_exists resolves path internally)
    if (!logo_io_file_exists(io, pathname))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Remember startup variable state before loading
    // We run startup if the loaded file sets it (changes the value or creates it)
    Value startup_before = {0};
    bool startup_existed_before = var_get("startup", &startup_before);

    // Open the file for reading (logo_io_open resolves path internally)
    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Set the loading flag to prevent recursive loads
    loading_in_progress = true;

    // Read and execute the file line by line
    char line[LOAD_MAX_LINE];
    char *proc_buffer = load_proc_buffer;
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
        if (!in_procedure_def && repl_line_starts_with_to(line))
        {
            // Start collecting procedure definition
            in_procedure_def = true;
            proc_len = 0;

            // Copy the "to" line to buffer with newline
            if (len + 2 <= LOAD_MAX_PROC - 10)
            {
                memcpy(proc_buffer, line, len);
                proc_buffer[len] = '\n';
                proc_len = len + 1;
            }
            else
            {
                // Procedure header is larger than the in-memory buffer.
                // Returning ERR_OUT_OF_SPACE is preferable to silently
                // dropping the entire procedure: the user needs to know
                // their code did not load.
                in_procedure_def = false;
                proc_len = 0;
                result = result_error_arg(ERR_OUT_OF_SPACE, NULL, pathname);
                break;
            }
            continue;
        }

        if (in_procedure_def)
        {
            if (repl_line_is_end(line))
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
                // Append line to procedure buffer with newline
                if (proc_len + len + 2 <= LOAD_MAX_PROC - 10)
                {
                    memcpy(proc_buffer + proc_len, line, len);
                    proc_buffer[proc_len + len] = '\n';
                    proc_len += len + 1;
                }
                else
                {
                    // Body line would overflow the procedure buffer. Report
                    // ERR_OUT_OF_SPACE rather than silently truncating the
                    // definition (which would leave a half-defined procedure
                    // that may not even parse).
                    in_procedure_def = false;
                    proc_len = 0;
                    result = result_error_arg(ERR_OUT_OF_SPACE, NULL, pathname);
                    break;
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

    // Close the file (logo_io_close resolves path internally)
    logo_io_close(io, pathname);

    // Clear the loading flag
    loading_in_progress = false;

    // If load was successful, check if the file set the startup variable
    // Run startup only if the loaded file sets it (value changed or newly created)
    if (result.status == RESULT_NONE)
    {
        Value startup_after;
        if (var_get("startup", &startup_after))
        {
            // Check if startup was set by the loaded file:
            // - It didn't exist before, or
            // - It existed but the value (node pointer) changed.
            //
            // IMPORTANT: this is a *node-identity* (pointer) comparison,
            // not a structural deep-equal. It works because:
            //   1. List literals parsed from the loaded file produce a
            //      freshly cons'd list with a new node index, so any
            //      `make "startup [...]` in the file changes the pointer
            //      even if the printed contents look identical to the
            //      previous binding.
            //   2. The interpreter never mutates an existing list in
            //      place to "replace" startup; `make` always rebinds.
            // If either invariant ever changes (e.g. structural sharing
            // of literal lists, or in-place edits), this comparison
            // could spuriously skip a re-run; switch to value equality
            // (deep compare via `values_equal`) at that point.
            bool startup_set_by_file = !startup_existed_before ||
                                       (startup_after.as.node != startup_before.as.node);
            
            if (startup_set_by_file && value_is_list(startup_after))
            {
                // Execute the startup list
                result = eval_run_list(eval, startup_after.as.node);
            }
        }
    }

    return result;
}

//==========================================================================
// Save helper functions
//==========================================================================

// Output callback for file saving (always succeeds since logo_io_write handles errors)
static bool save_output(void *ctx, const char *str)
{
    (void)ctx;
    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_write(io, str);
    }
    return true;
}

// save pathname - saves all unburied procedures, variables, and properties
static Result prim_save(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file already exists (logo_io_file_exists resolves path internally)
    if (logo_io_file_exists(io, pathname))
    {
        return result_error_arg(ERR_FILE_EXISTS, "", pathname);
    }

    // Open the file for writing (logo_io_open resolves path internally)
    LogoStream *stream = logo_io_open(io, pathname);
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
            format_procedure_definition(save_output, NULL, proc);
            save_output(NULL, "\n");
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
            format_variable(save_output, NULL, name, value);
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
            format_property_list(save_output, NULL, name, list);
        }
    }

    // Restore writer and close file (logo_io_close resolves path internally)
    logo_io_set_writer(io, old_writer == &io->console->output ? NULL : old_writer);
    logo_io_close(io, pathname);

    return result_none();
}

// savel name pathname or savel [name1 name2 ...] pathname
// Save specified procedure(s) and all unburied variables and properties
static Result prim_savel(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[1]);

    const char *pathname = mem_word_ptr(args[1].as.node);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file already exists
    if (logo_io_file_exists(io, pathname))
    {
        return result_error_arg(ERR_FILE_EXISTS, "", pathname);
    }

    // First argument is name or list of names - validate procedures exist first
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!proc_find(name))
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
    }
    else if (value_is_list(args[0]))
    {
        Node curr = args[0].as.node;
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                if (!proc_find(name))
                {
                    return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
                }
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    // Open the file for writing
    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    // Save current writer and set to file
    LogoStream *old_writer = io->writer;
    logo_io_set_writer(io, stream);

    // Save the specified procedure(s)
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        UserProcedure *proc = proc_find(name);
        if (proc)
        {
            format_procedure_definition(save_output, NULL, proc);
            save_output(NULL, "\n");
        }
    }
    else if (value_is_list(args[0]))
    {
        Node curr = args[0].as.node;
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                UserProcedure *proc = proc_find(name);
                if (proc)
                {
                    format_procedure_definition(save_output, NULL, proc);
                    save_output(NULL, "\n");
                }
            }
            curr = mem_cdr(curr);
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
            format_variable(save_output, NULL, name, value);
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
            format_property_list(save_output, NULL, name, list);
        }
    }

    // Restore writer and close file
    logo_io_set_writer(io, old_writer == &io->console->output ? NULL : old_writer);
    logo_io_close(io, pathname);

    return result_none();
}

// savepic pathname - saves the graphics screen as a BMP file
static Result prim_savepic(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io || !io->console || !io->console->turtle)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    const LogoConsoleTurtle *turtle = io->console->turtle;
    if (!turtle->gfx_save)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file already exists (logo_io_file_exists resolves path internally)
    if (logo_io_file_exists(io, pathname))
    {
        return result_error_arg(ERR_FILE_EXISTS, "", pathname);
    }

    // Open through the I/O manager so the storage router picks the right
    // filesystem (LittleFS root or /sd), then stream the BMP to it
    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    int err = turtle->gfx_save(stream);
    logo_io_close(io, pathname);
    if (err != 0)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

// loadpic pathname - loads an 8-bit indexed color BMP file to the graphics screen
static Result prim_loadpic(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io || !io->console || !io->console->turtle)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    const LogoConsoleTurtle *turtle = io->console->turtle;
    if (!turtle->gfx_load)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file exists (logo_io_file_exists resolves path internally)
    if (!logo_io_file_exists(io, pathname))
    {
        return result_error(ERR_FILE_NOT_FOUND);
    }

    // Open through the I/O manager so the storage router picks the right
    // filesystem (LittleFS root or /sd), then read the BMP from it
    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        return result_error(ERR_DISK_TROUBLE);
    }

    int err = turtle->gfx_load(stream);
    logo_io_close(io, pathname);
    if (err != 0)
    {
        // Check for specific error codes
        if (err == EINVAL)
        {
            return result_error(ERR_FILE_WRONG_TYPE);
        }
        return result_error(ERR_DISK_TROUBLE);
    }

    return result_none();
}

//==========================================================================
// pofile
//==========================================================================

// Maximum line size for pofile
#define POFILE_MAX_LINE 256

// pofile pathname - prints the contents of a file to the screen
// Always prints to the display, not to the current writer.
// Errors if the file is already open.
static Result prim_pofile(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);

    const char *pathname = mem_word_ptr(args[0].as.node);

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    // Check if file is already open - this is an error per the spec
    // (logo_io_is_open resolves path internally)
    if (logo_io_is_open(io, pathname))
    {
        return result_error_arg(ERR_FILE_ALREADY_OPEN, NULL, pathname);
    }

    // Check if file exists (logo_io_file_exists resolves path internally)
    if (!logo_io_file_exists(io, pathname))
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Open the file for reading (logo_io_open resolves path internally)
    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
    }

    // Read and print each line to the console (not the writer)
    char line[POFILE_MAX_LINE];
    int len;
    while ((len = logo_stream_read_line(stream, line, sizeof(line))) >= 0)
    {
        // Strip trailing newline/carriage return
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[--len] = '\0';
        }

        // Print directly to console (bypasses writer)
        logo_io_console_write_line(io, line);
    }

    // Close the file (logo_io_close resolves path internally)
    logo_io_close(io, pathname);

    return result_none();
}

//==========================================================================
// Registration
//==========================================================================

void primitives_files_load_save_init(void)
{
    // Load and save
    primitive_register("load", 1, prim_load);
    primitive_register("save", 1, prim_save);
    primitive_register("savel", 2, prim_savel);
    primitive_register("savepic", 1, prim_savepic);
    primitive_register("loadpic", 1, prim_loadpic);
    primitive_register("pofile", 1, prim_pofile);
}
