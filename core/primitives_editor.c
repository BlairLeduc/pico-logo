//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Editor primitives: edit, edall, edn, edns, editfile
//

#include "primitives.h"
#include "procedures.h"
#include "properties.h"
#include "variables.h"
#include "memory.h"
#include "error.h"
#include "eval.h"
#include "format.h"
#include "repl.h"
#include "devices/io.h"
#include "devices/stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Editor buffers. They are only touched at human-editing speed, so
// primitives_editor_init() places them in the aux/PSRAM region when available
// (relieving SRAM), falling back to a one-time heap allocation of the size
// below otherwise. They are deliberately NOT static arrays: reserving them in
// BSS would keep the SRAM even when PSRAM backs them.
//
// The fallback size every board can meet (8KB here, 24576 from the presets).
#ifndef LOGO_EDITOR_BUFFER_SIZE
#define LOGO_EDITOR_BUFFER_SIZE 8192
#endif

// Size used when the buffers land in the aux region: PSRAM has megabytes going
// spare where SRAM has kilobytes, so a board that has it edits much larger
// files. Which size a board gets is decided at run time, not by the build: a
// PSRAM board whose memory fails to verify at boot gets no aux region, and must
// still come up with an editor SRAM can hold.
#ifndef LOGO_EDITOR_PSRAM_BUFFER_SIZE
#define LOGO_EDITOR_PSRAM_BUFFER_SIZE (256 * 1024)
#endif

// Active buffers (valid after primitives_editor_init()) and their size.
static char *editor_buffer = NULL;
static char *editor_proc_buffer = NULL;
static size_t editor_buffer_size = LOGO_EDITOR_BUFFER_SIZE;

// Vi mode (docs/vi-mode-design.md). A session setting like the palette, kept
// here rather than passed to `edit`, so that one flag reaches all five entry
// points -- edit, edall, edn, edns and editfile -- without widening the
// console's editor signature.
static bool vi_mode_on = false;

// Process-lifetime heap fallbacks, allocated once and reused across re-inits so
// repeated init (e.g. across tests) never leaks.
static char *editor_buffer_heap = NULL;
static char *editor_proc_buffer_heap = NULL;

// The cached heap fallback, allocated on first use.
static char *editor_heap_buffer(char **heap_cache)
{
    if (*heap_cache == NULL)
    {
        *heap_cache = (char *)malloc(LOGO_EDITOR_BUFFER_SIZE);
    }
    return *heap_cache;
}

// Procedure-definition accumulation is shared with the REPL; see
// repl_line_starts_with_to / repl_proc_def_append in core/repl.h.

// Count bracket balance in a line (positive = more '[', negative = more ']')
static int count_bracket_balance(const char *line)
{
    int balance = 0;
    for (const char *p = line; *p; p++)
    {
        if (*p == '[') balance++;
        else if (*p == ']') balance--;
    }
    return balance;
}

// Run editor and process results
// Processes buffer as if each line were typed at top level
static Result run_editor_and_process(Evaluator *eval, char *buffer)
{
    UNUSED(eval);  // Not used directly
    
    LogoIO *io = primitives_get_io();
    if (!io || !io->console)
    {
        return result_error_arg(ERR_UNDEFINED, NULL, NULL);
    }
    
    // Check if editor is available
    if (!logo_console_has_editor(io->console))
    {
        // No editor support - print message and return
        logo_io_write(io, "Editor not available on this device\n");
        return result_none();
    }
    
    // Call the editor
    // No write-back: this buffer is the workspace, so vi's `:w` accepts it the
    // same way `ZZ` does, and the definitions below are run
    LogoEditorResult editor_result =
        io->console->editor->edit(buffer, editor_buffer_size, NULL, NULL);
    
    if (editor_result == LOGO_EDITOR_CANCEL)
    {
        // User cancelled - do nothing
        return result_none();
    }
    
    if (editor_result == LOGO_EDITOR_ERROR)
    {
        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
    }
    
    // Process the buffer content - each line is executed as if typed at top level
    // We need to handle procedure definitions (to...end) specially
    
    // Procedure definition buffer (uses static to avoid 8KB on C stack)
    char *proc_buffer = editor_proc_buffer;
    size_t proc_len = 0;
    bool in_procedure_def = false;
    
    // Bracket-balance tracking for multi-line expressions (e.g. make "x [\n...\n])
    int bracket_depth = 0;
    size_t expr_len = 0;
    
    // Process line by line
    char *line_start = buffer;
    while (*line_start)
    {
        // Find end of line
        char *line_end = line_start;
        while (*line_end && *line_end != '\n')
            line_end++;
        
        // Calculate line length
        size_t line_len = line_end - line_start;
        
        // Check if line is empty (only whitespace)
        bool is_empty = true;
        for (size_t i = 0; i < line_len; i++)
        {
            if (line_start[i] != ' ' && line_start[i] != '\t')
            {
                is_empty = false;
                break;
            }
        }
        
        // When inside a procedure definition, preserve empty lines
        if (is_empty && in_procedure_def)
        {
            // Add just a newline to preserve the empty line
            if (proc_len + 1 < editor_buffer_size - 10)
            {
                proc_buffer[proc_len] = '\n';
                proc_len += 1;
            }
        }
        else if (!is_empty)
        {
            // Temporarily null-terminate the line for processing
            char saved = *line_end;
            *line_end = '\0';
            
            if (!in_procedure_def && repl_line_starts_with_to(line_start))
            {
                // Start collecting the definition. The "to" line goes through
                // the same append as the rest, since it may close it too.
                in_procedure_def = true;
                proc_len = 0;
            }

            if (in_procedure_def)
            {
                ProcDefStatus status = repl_proc_def_append(proc_buffer, editor_buffer_size,
                                                            &proc_len, line_start);
                if (status == PROC_DEF_OVERFLOW)
                {
                    logo_io_write(io, "Procedure too long\n");
                    in_procedure_def = false;
                }
                else if (status == PROC_DEF_COMPLETE)
                {
                    in_procedure_def = false;

                    // Parse and define the procedure
                    Result r = proc_define_from_text(proc_buffer);
                    if (r.status == RESULT_ERROR)
                    {
                        logo_io_write(io, error_format(r));
                        logo_io_write(io, "\n");
                    }
                    else if (r.status == RESULT_OK)
                    {
                        // Procedure defined successfully
                        char buf[256];
                        snprintf(buf, sizeof(buf), "%s defined\n",
                                r.value.as.node ? mem_word_ptr(r.value.as.node) : "procedure");
                        logo_io_write(io, buf);
                    }

                    proc_len = 0;
                }
            }
            else
            {
                // Regular instruction - handle multi-line bracket expressions
                
                if (bracket_depth > 0)
                {
                    // Continuing a multi-line bracket expression - append this line
                    if (expr_len + line_len + 2 < editor_buffer_size - 10)
                    {
                        // Add a space separator then the line content
                        proc_buffer[expr_len] = ' ';
                        memcpy(proc_buffer + expr_len + 1, line_start, line_len);
                        expr_len += line_len + 1;
                        proc_buffer[expr_len] = '\0';
                    }
                    else
                    {
                        logo_io_write(io, "Expression too long\n");
                        bracket_depth = 0;
                        expr_len = 0;
                        // Restore and skip
                        *line_end = saved;
                        if (*line_end == '\n')
                            line_start = line_end + 1;
                        else
                            break;
                        continue;
                    }
                    
                    bracket_depth += count_bracket_balance(line_start);
                    
                    if (bracket_depth > 0)
                    {
                        // Still accumulating - move to next line
                        *line_end = saved;
                        if (*line_end == '\n')
                            line_start = line_end + 1;
                        else
                            break;
                        continue;
                    }
                    
                    // Brackets balanced - fall through to evaluate proc_buffer
                    bracket_depth = 0;
                }
                else
                {
                    // Check if this line starts a multi-line bracket expression
                    int line_balance = count_bracket_balance(line_start);
                    if (line_balance > 0)
                    {
                        // Start accumulating multi-line expression
                        bracket_depth = line_balance;
                        expr_len = 0;
                        
                        if (line_len + 1 < editor_buffer_size - 10)
                        {
                            memcpy(proc_buffer, line_start, line_len);
                            expr_len = line_len;
                            proc_buffer[expr_len] = '\0';
                        }
                        else
                        {
                            logo_io_write(io, "Expression too long\n");
                            bracket_depth = 0;
                        }
                        
                        // Move to next line to continue accumulating
                        *line_end = saved;
                        if (*line_end == '\n')
                            line_start = line_end + 1;
                        else
                            break;
                        continue;
                    }
                }
                
                // Evaluate the instruction (single line or accumulated multi-line)
                const char *eval_text = (expr_len > 0) ? proc_buffer : line_start;
                
                Lexer lexer;
                Evaluator line_eval;
                lexer_init(&lexer, eval_text);
                eval_init(&line_eval, &lexer);
                
                // Evaluate all instructions on the line
                while (!eval_at_end(&line_eval))
                {
                    Result r = eval_instruction(&line_eval);
                    
                    if (r.status == RESULT_ERROR)
                    {
                        logo_io_write(io, error_format(r));
                        logo_io_write(io, "\n");
                        break;
                    }
                    else if (r.status == RESULT_THROW)
                    {
                        // Uncaught throw - check for special cases
                        if (strcasecmp(result_get_throw_tag(r), "toplevel") == 0)
                        {
                            // throw "toplevel returns to top level silently
                            break;
                        }
                        else
                        {
                            // Other uncaught throws are errors
                            char msg[128];
                            snprintf(msg, sizeof(msg), "No one caught %s\n", result_get_throw_tag(r));
                            logo_io_write(io, msg);
                            break;
                        }
                    }
                    else if (r.status == RESULT_OK)
                    {
                        // Expression returned a value - show "I don't know what to do with" error
                        char msg[128];
                        snprintf(msg, sizeof(msg), "I don't know what to do with %s\n", 
                                 value_to_string(r.value));
                        logo_io_write(io, msg);
                        break;
                    }
                    // RESULT_NONE means command completed successfully - continue
                }
                
                // Reset multi-line expression state
                expr_len = 0;
            }
            
            // Restore the character
            *line_end = saved;
        }
        
        // Move to next line
        if (*line_end == '\n')
            line_start = line_end + 1;
        else
            break;  // End of buffer
    }
    
    // If still in procedure definition at end of buffer, auto-complete with "end"
    if (in_procedure_def && proc_len > 0)
    {
        if (proc_len + 4 < editor_buffer_size)
        {
            memcpy(proc_buffer + proc_len, "end", 3);
            proc_len += 3;
            proc_buffer[proc_len] = '\0';
            
            // Parse and define the procedure
            Result r = proc_define_from_text(proc_buffer);
            if (r.status == RESULT_ERROR)
            {
                logo_io_write(io, error_format(r));
                logo_io_write(io, "\n");
            }
            else if (r.status == RESULT_OK)
            {
                // Procedure defined successfully
                char buf[256];
                snprintf(buf, sizeof(buf), "%s defined\n", 
                        r.value.as.node ? mem_word_ptr(r.value.as.node) : "procedure");
                logo_io_write(io, buf);
            }
        }
    }
    
    return result_none();
}

// edit "name or edit [name1 name2 ...] or (edit)
// Edit procedure definition(s)
static Result prim_edit(Evaluator *eval, int argc, Value *args)
{
    if (argc == 0)
    {
        // (edit) with no args - use current buffer content as-is
        return run_editor_and_process(eval, editor_buffer);
    }
    
    // When we have arguments, start fresh
    FormatBufferContext ctx;
    format_buffer_init(&ctx, editor_buffer, editor_buffer_size);
    bool first_proc = true;
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        
        // Check if it's a primitive - can't edit primitives
        if (primitive_find(name))
        {
            return result_error_arg(ERR_IS_PRIMITIVE, name, NULL);
        }
        
        UserProcedure *proc = proc_find(name);
        if (proc == NULL)
        {
            // Procedure doesn't exist - create template: "to name\n"
            // Cursor will be on line below, ready for user to define body
            if (!format_buffer_output(&ctx, "to "))
                return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
            if (!format_buffer_output(&ctx, name))
                return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
            if (!format_buffer_output(&ctx, "\n"))
                return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
        }
        else
        {
            if (!format_procedure_definition(format_buffer_output, &ctx, proc))
            {
                return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
            }
        }
    }
    else if (value_is_list(args[0]))
    {
        // List of procedure names
        Node curr = mem_first_cell(args[0].as.node);
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                
                // Check if it's a primitive - can't edit primitives
                if (primitive_find(name))
                {
                    return result_error_arg(ERR_IS_PRIMITIVE, name, NULL);
                }
                
                UserProcedure *proc = proc_find(name);
                if (proc == NULL)
                {
                    // Procedure doesn't exist - create template: "to name\n"
                    if (!first_proc)
                    {
                        if (!format_buffer_output(&ctx, "\n"))
                        {
                            return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                        }
                    }
                    first_proc = false;
                    
                    if (!format_buffer_output(&ctx, "to "))
                        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                    if (!format_buffer_output(&ctx, name))
                        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                    if (!format_buffer_output(&ctx, "\n"))
                        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                }
                else
                {
                    // Add blank line between definitions
                    if (!first_proc)
                    {
                        if (!format_buffer_output(&ctx, "\n"))
                        {
                            return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                        }
                    }
                    first_proc = false;
                    
                    if (!format_procedure_definition(format_buffer_output, &ctx, proc))
                    {
                        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                    }
                }
            }
            curr = mem_next_cell(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return run_editor_and_process(eval, editor_buffer);
}

// edn "name or edn [name1 name2 ...]
// Edit variable name(s) and value(s)
static Result prim_edn(Evaluator *eval, int argc, Value *args)
{
    FormatBufferContext ctx;
    format_buffer_init(&ctx, editor_buffer, editor_buffer_size);
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    }
    
    if (value_is_word(args[0]))
    {
        // Single variable name
        const char *name = mem_word_ptr(args[0].as.node);
        Value value;
        if (!var_get(name, &value))
        {
            return result_error_arg(ERR_NO_VALUE, NULL, name);
        }
        if (!format_variable(format_buffer_output, &ctx, name, value))
        {
            return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
        }
    }
    else if (value_is_list(args[0]))
    {
        // List of variable names
        Node curr = mem_first_cell(args[0].as.node);
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                Value value;
                if (!var_get(name, &value))
                {
                    return result_error_arg(ERR_NO_VALUE, NULL, name);
                }
                
                if (!format_variable(format_buffer_output, &ctx, name, value))
                {
                    return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                }
            }
            curr = mem_next_cell(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return run_editor_and_process(eval, editor_buffer);
}

// edns - edit all variable names and values (not buried)
static Result prim_edns(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc); UNUSED(args);
    
    FormatBufferContext ctx;
    format_buffer_init(&ctx, editor_buffer, editor_buffer_size);
    
    int count = var_global_count(false);  // Exclude buried
    for (int i = 0; i < count; i++)
    {
        const char *name;
        Value value;
        if (var_get_global_by_index(i, false, &name, &value))
        {
            if (!format_variable(format_buffer_output, &ctx, name, value))
            {
                return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
            }
        }
    }
    
    return run_editor_and_process(eval, editor_buffer);
}

// edall - edit all procedures and variables (not buried)
static Result prim_edall(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc); UNUSED(args);
    
    FormatBufferContext ctx;
    format_buffer_init(&ctx, editor_buffer, editor_buffer_size);
    
    // Format all procedures (not buried)
    int proc_cnt = proc_count(true);  // Get ALL, filter by buried in loop
    bool first_proc = true;
    for (int i = 0; i < proc_cnt; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            // Add blank line between definitions
            if (!first_proc)
            {
                if (!format_buffer_output(&ctx, "\n"))
                {
                    return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                }
            }
            first_proc = false;
            
            if (!format_procedure_definition(format_buffer_output, &ctx, proc))
            {
                return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
            }
        }
    }
    
    // Format all variables (not buried)
    int var_cnt = var_global_count(false);
    bool first_var = true;
    for (int i = 0; i < var_cnt; i++)
    {
        const char *name;
        Value value;
        if (var_get_global_by_index(i, false, &name, &value))
        {
            // Add blank line before first variable (if there were procedures)
            if (first_var && !first_proc)
            {
                if (!format_buffer_output(&ctx, "\n"))
                {
                    return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                }
            }
            first_var = false;
            
            if (!format_variable(format_buffer_output, &ctx, name, value))
            {
                return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
            }
        }
    }
    
    // Format all property lists
    int prop_cnt = prop_name_count();
    bool first_prop = true;
    for (int i = 0; i < prop_cnt; i++)
    {
        const char *name;
        if (prop_get_name_by_index(i, &name))
        {
            Node list = prop_get_list(name);
            if (!mem_is_nil(list))
            {
                // Add blank line before first property (if there were procs or vars)
                if (first_prop && (!first_proc || !first_var))
                {
                    if (!format_buffer_output(&ctx, "\n"))
                    {
                        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                    }
                }
                first_prop = false;
                
                if (!format_property_list(format_buffer_output, &ctx, name, list))
                {
                    return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                }
            }
        }
    }
    
    return run_editor_and_process(eval, editor_buffer);
}

// Write the editor's buffer to `ctx`'s pathname, replacing whatever is there.
// It is both what `editfile` does when the editor is accepted and what vi's
// `:w` calls without leaving the editor.
static bool editfile_save(const char *buffer, void *ctx)
{
    const char *pathname = (const char *)ctx;
    LogoIO *io = primitives_get_io();
    
    // Delete the old file first: `open` keeps what is already there
    if (logo_io_file_exists(io, pathname))
    {
        logo_io_file_delete(io, pathname);
    }
    
    LogoStream *stream = logo_io_open(io, pathname);
    if (!stream)
    {
        return false;
    }
    
    logo_stream_write(stream, buffer);
    logo_io_close(io, pathname);
    return true;
}

// editfile pathname - edit a file's contents (not run as Logo code)
static Result prim_editfile(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);
    
    const char *pathname = mem_word_ptr(args[0].as.node);
    
    LogoIO *io = primitives_get_io();
    if (!io || !io->console)
    {
        return result_error_arg(ERR_UNDEFINED, NULL, NULL);
    }
    
    // Check if editor is available
    if (!logo_console_has_editor(io->console))
    {
        logo_io_write(io, "Editor not available on this device\n");
        return result_none();
    }
    
    // Check if file is already open - this is an error per the spec
    if (logo_io_is_open(io, pathname))
    {
        return result_error_arg(ERR_FILE_ALREADY_OPEN, NULL, pathname);
    }
    
    // Initialize buffer
    editor_buffer[0] = '\0';
    size_t content_len = 0;
    
    // If file exists, load its contents
    if (logo_io_file_exists(io, pathname))
    {
        // Check file size first
        long file_size = logo_io_file_size(io, pathname);
        if (file_size > (long)(editor_buffer_size - 1))
        {
            return result_error_arg(ERR_FILE_TOO_BIG, NULL, pathname);
        }
        
        // Open and read file
        LogoStream *stream = logo_io_open(io, pathname);
        if (!stream)
        {
            return result_error_arg(ERR_FILE_NOT_FOUND, "", pathname);
        }
        
        // Read file line by line into buffer
        char line[256];
        int len;
        while ((len = logo_stream_read_line(stream, line, sizeof(line))) >= 0)
        {
            // Check if line fits in buffer
            size_t line_len = strlen(line);
            if (content_len + line_len + 1 >= editor_buffer_size)
            {
                logo_io_close(io, pathname);
                return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
            }
            
            // Append line to buffer (without stripping newlines - keep as-is)
            memcpy(editor_buffer + content_len, line, line_len);
            content_len += line_len;
            
            // Add newline if line didn't end with one
            if (line_len == 0 || line[line_len - 1] != '\n')
            {
                if (content_len + 1 >= editor_buffer_size)
                {
                    logo_io_close(io, pathname);
                    return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
                }
                editor_buffer[content_len++] = '\n';
            }
        }
        
        editor_buffer[content_len] = '\0';
        logo_io_close(io, pathname);
    }
    // If file doesn't exist, start with empty buffer (will create on save)
    
    // Call the editor, giving vi's `:w` the same write it does on the way out
    LogoEditorResult editor_result =
        io->console->editor->edit(editor_buffer, editor_buffer_size,
                                  editfile_save, (void *)pathname);
    
    if (editor_result == LOGO_EDITOR_CANCEL)
    {
        // User cancelled - file remains unchanged (any `:w` already written stands)
        return result_none();
    }
    
    if (editor_result == LOGO_EDITOR_ERROR)
    {
        return result_error_arg(ERR_OUT_OF_SPACE, NULL, NULL);
    }
    
    if (!editfile_save(editor_buffer, (void *)pathname))
    {
        return result_error(ERR_DISK_TROUBLE);
    }
    
    return result_none();
}

size_t primitives_editor_buffer_size(void)
{
    return editor_buffer_size;
}

//
// setvimode true/false - select the vi key layer for the full-screen editor
//
// The console's editor takes the flag through an optional vtable entry, so a
// console without one (the mock, the host REPL) accepts the setting and
// ignores it rather than failing.
//
static Result prim_setvimode(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    UNUSED(argc);

    const char *str = value_to_string(args[0]);
    bool on;

    if (str == NULL)
    {
        return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
    }
    if (strcasecmp(str, "true") == 0)
    {
        on = true;
    }
    else if (strcasecmp(str, "false") == 0)
    {
        on = false;
    }
    else
    {
        return result_error_arg(ERR_NOT_BOOL, NULL, str);
    }

    vi_mode_on = on;

    LogoIO *io = primitives_get_io();
    if (io != NULL && io->console != NULL && logo_console_has_editor(io->console) &&
        io->console->editor->set_vi_mode != NULL)
    {
        io->console->editor->set_vi_mode(on);
    }

    return result_none();
}

//
// vimode? - outputs true when the editor opens in vi mode
//
static Result prim_vimodep(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    UNUSED(argc);
    UNUSED(args);
    return result_ok(value_bool(vi_mode_on));
}

void primitives_editor_init(void)
{
    // Place the editor buffers in the aux/PSRAM region when one is available,
    // else in a one-time heap fallback. Re-selected on each init; a fresh
    // region (logo_mem_set_aux_region) resets any prior region block.
    //
    // Both buffers are taken as one block so the pair is all or nothing: the
    // bounds above are a single size, and a region that only had room for the
    // first would leave the edit buffer large and the definition buffer small.
    char *region = (char *)mem_region_alloc(2 * LOGO_EDITOR_PSRAM_BUFFER_SIZE);
    if (region != NULL)
    {
        editor_buffer = region;
        editor_proc_buffer = region + LOGO_EDITOR_PSRAM_BUFFER_SIZE;
        editor_buffer_size = LOGO_EDITOR_PSRAM_BUFFER_SIZE;
    }
    else
    {
        editor_buffer = editor_heap_buffer(&editor_buffer_heap);
        editor_proc_buffer = editor_heap_buffer(&editor_proc_buffer_heap);
        editor_buffer_size = LOGO_EDITOR_BUFFER_SIZE;
    }

    // Region memory arrives uninitialised, and (edit) with no arguments edits
    // whatever the buffer already holds.
    if (editor_buffer != NULL)
    {
        editor_buffer[0] = '\0';
    }

    // A fresh interpreter starts in the editor's default key layer
    vi_mode_on = false;
    LogoIO *io = primitives_get_io();
    if (io != NULL && io->console != NULL && logo_console_has_editor(io->console) &&
        io->console->editor->set_vi_mode != NULL)
    {
        io->console->editor->set_vi_mode(false);
    }

    primitive_register("setvimode", 1, prim_setvimode);
    primitive_register("vimode?", 0, prim_vimodep);
    primitive_register("edit", 1, prim_edit);  // 1 argument, (edit) for none
    primitive_register("ed", 1, prim_edit);    // Abbreviation
    primitive_register("edall", 0, prim_edall);
    primitive_register("edn", 1, prim_edn);
    primitive_register("edns", 0, prim_edns);
    primitive_register("editfile", 1, prim_editfile);
}
