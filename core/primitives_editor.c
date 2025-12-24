//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Editor primitives: edit, edn, edns
//

#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "memory.h"
#include "error.h"
#include "eval.h"
#include "devices/io.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

// Static editor buffer (8KB as specified in reference)
#define EDITOR_BUFFER_SIZE 8192
static char editor_buffer[EDITOR_BUFFER_SIZE];

// Helper to append string to buffer with bounds checking
static bool buffer_append(char *buffer, size_t buffer_size, size_t *pos, const char *str)
{
    size_t len = strlen(str);
    if (*pos + len >= buffer_size - 1)
    {
        return false;
    }
    memcpy(buffer + *pos, str, len);
    *pos += len;
    buffer[*pos] = '\0';
    return true;
}

// Format a procedure body element to buffer (handles quoted words, etc.)
static bool format_body_element(char *buffer, size_t buffer_size, size_t *pos, Node elem)
{
    if (mem_is_word(elem))
    {
        return buffer_append(buffer, buffer_size, pos, mem_word_ptr(elem));
    }
    else if (mem_is_list(elem))
    {
        if (!buffer_append(buffer, buffer_size, pos, "["))
            return false;
        bool first = true;
        Node curr = elem;
        while (!mem_is_nil(curr))
        {
            if (!first)
            {
                if (!buffer_append(buffer, buffer_size, pos, " "))
                    return false;
            }
            first = false;
            if (!format_body_element(buffer, buffer_size, pos, mem_car(curr)))
                return false;
            curr = mem_cdr(curr);
        }
        return buffer_append(buffer, buffer_size, pos, "]");
    }
    return true;
}

// Format procedure definition to buffer (po format)
static bool format_procedure_definition(char *buffer, size_t buffer_size, size_t *pos, 
                                        UserProcedure *proc)
{
    // Print "to name"
    if (!buffer_append(buffer, buffer_size, pos, "to "))
        return false;
    if (!buffer_append(buffer, buffer_size, pos, proc->name))
        return false;
    
    // Print parameters
    for (int i = 0; i < proc->param_count; i++)
    {
        if (!buffer_append(buffer, buffer_size, pos, " :"))
            return false;
        if (!buffer_append(buffer, buffer_size, pos, proc->params[i]))
            return false;
    }
    if (!buffer_append(buffer, buffer_size, pos, "\n"))
        return false;
    
    // Print body with newline detection and indentation
    int bracket_depth = 0;
    Node curr = proc->body;
    bool need_indent = true;
    
    // Skip leading newline marker
    if (!mem_is_nil(curr))
    {
        Node first_elem = mem_car(curr);
        if (mem_is_word(first_elem) && proc_is_newline_marker(mem_word_ptr(first_elem)))
        {
            curr = mem_cdr(curr);
        }
    }
    
    while (!mem_is_nil(curr))
    {
        Node elem = mem_car(curr);
        
        // Check if this is a newline marker
        if (mem_is_word(elem))
        {
            const char *word = mem_word_ptr(elem);
            if (proc_is_newline_marker(word))
            {
                if (!buffer_append(buffer, buffer_size, pos, "\n"))
                    return false;
                need_indent = true;
                curr = mem_cdr(curr);
                continue;
            }
        }
        
        // Check if this is a closing bracket - decrease depth before indenting
        if (mem_is_word(elem))
        {
            const char *word = mem_word_ptr(elem);
            if (strcmp(word, "]") == 0 && bracket_depth > 0)
            {
                bracket_depth--;
            }
        }
        
        // Output indentation if needed (base indent of 1 for procedure body)
        if (need_indent)
        {
            for (int i = 0; i < bracket_depth + 1; i++)
            {
                if (!buffer_append(buffer, buffer_size, pos, "  "))
                    return false;
            }
            need_indent = false;
        }
        
        if (!format_body_element(buffer, buffer_size, pos, elem))
            return false;
        
        // Track bracket depth after printing (for opening brackets)
        if (mem_is_word(elem))
        {
            const char *word = mem_word_ptr(elem);
            if (strcmp(word, "[") == 0)
            {
                bracket_depth++;
            }
        }
        
        // Add space between elements
        Node next = mem_cdr(curr);
        if (!mem_is_nil(next))
        {
            // Don't add space before newline marker
            Node next_elem = mem_car(next);
            if (mem_is_word(next_elem))
            {
                const char *next_word = mem_word_ptr(next_elem);
                if (!proc_is_newline_marker(next_word))
                {
                    if (!buffer_append(buffer, buffer_size, pos, " "))
                        return false;
                }
            }
            else
            {
                if (!buffer_append(buffer, buffer_size, pos, " "))
                    return false;
            }
        }
        curr = next;
    }
    
    if (!buffer_append(buffer, buffer_size, pos, "end\n"))
        return false;
    
    return true;
}

// Format a variable and its value to buffer
static bool format_variable(char *buffer, size_t buffer_size, size_t *pos,
                           const char *name, Value value)
{
    char num_buf[64];
    
    if (!buffer_append(buffer, buffer_size, pos, "make \""))
        return false;
    if (!buffer_append(buffer, buffer_size, pos, name))
        return false;
    if (!buffer_append(buffer, buffer_size, pos, " "))
        return false;
    
    switch (value.type)
    {
    case VALUE_NUMBER:
        snprintf(num_buf, sizeof(num_buf), "%g", value.as.number);
        if (!buffer_append(buffer, buffer_size, pos, num_buf))
            return false;
        break;
    case VALUE_WORD:
        if (!buffer_append(buffer, buffer_size, pos, "\""))
            return false;
        if (!buffer_append(buffer, buffer_size, pos, mem_word_ptr(value.as.node)))
            return false;
        break;
    case VALUE_LIST:
        if (!buffer_append(buffer, buffer_size, pos, "["))
            return false;
        {
            bool first = true;
            Node curr = value.as.node;
            while (!mem_is_nil(curr))
            {
                if (!first)
                {
                    if (!buffer_append(buffer, buffer_size, pos, " "))
                        return false;
                }
                first = false;
                if (!format_body_element(buffer, buffer_size, pos, mem_car(curr)))
                    return false;
                curr = mem_cdr(curr);
            }
        }
        if (!buffer_append(buffer, buffer_size, pos, "]"))
            return false;
        break;
    default:
        break;
    }
    
    if (!buffer_append(buffer, buffer_size, pos, "\n"))
        return false;
    
    return true;
}

// Helper to check if a line starts with "to " (case-insensitive)
static bool line_starts_with_to(const char *line)
{
    // Skip leading whitespace
    while (*line && (*line == ' ' || *line == '\t'))
        line++;
    
    if (strncasecmp(line, "to", 2) != 0)
        return false;
    
    // Must be followed by whitespace or end of line
    char c = line[2];
    return c == '\0' || c == ' ' || c == '\t' || c == '\n';
}

// Helper to check if a line is just "end" (case-insensitive)
static bool line_is_end(const char *line)
{
    // Skip leading whitespace
    while (*line && (*line == ' ' || *line == '\t'))
        line++;
    
    if (strncasecmp(line, "end", 3) != 0)
        return false;
    
    // Must be followed by whitespace, newline, or end of string
    char c = line[3];
    return c == '\0' || c == ' ' || c == '\t' || c == '\n';
}

// Run editor and process results
// Processes buffer as if each line were typed at top level
static Result run_editor_and_process(Evaluator *eval, char *buffer)
{
    (void)eval;  // Not used directly
    
    LogoIO *io = primitives_get_io();
    if (!io || !io->console)
    {
        return result_error_arg(ERR_UNDEFINED, "edit", NULL);
    }
    
    // Check if editor is available
    if (!logo_console_has_editor(io->console))
    {
        // No editor support - print message and return
        logo_io_write(io, "Editor not available on this device\n");
        return result_none();
    }
    
    // Call the editor
    LogoEditorResult editor_result = io->console->editor->edit(buffer, EDITOR_BUFFER_SIZE);
    
    if (editor_result == LOGO_EDITOR_CANCEL)
    {
        // User cancelled - do nothing
        return result_none();
    }
    
    if (editor_result == LOGO_EDITOR_ERROR)
    {
        return result_error_arg(ERR_OUT_OF_SPACE, "edit", NULL);
    }
    
    // Process the buffer content - each line is executed as if typed at top level
    // We need to handle procedure definitions (to...end) specially
    
    // Procedure definition buffer
    char proc_buffer[EDITOR_BUFFER_SIZE];
    size_t proc_len = 0;
    bool in_procedure_def = false;
    
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
        
        // Skip empty lines
        bool is_empty = true;
        for (size_t i = 0; i < line_len; i++)
        {
            if (line_start[i] != ' ' && line_start[i] != '\t')
            {
                is_empty = false;
                break;
            }
        }
        
        if (!is_empty)
        {
            // Temporarily null-terminate the line for processing
            char saved = *line_end;
            *line_end = '\0';
            
            if (!in_procedure_def && line_starts_with_to(line_start))
            {
                // Start collecting procedure definition
                in_procedure_def = true;
                proc_len = 0;
                
                // Copy the "to" line to buffer with newline marker
                if (line_len + 4 < EDITOR_BUFFER_SIZE - 10)
                {
                    memcpy(proc_buffer, line_start, line_len);
                    proc_buffer[line_len] = ' ';
                    proc_buffer[line_len + 1] = '\\';
                    proc_buffer[line_len + 2] = 'n';
                    proc_buffer[line_len + 3] = ' ';
                    proc_len = line_len + 4;
                }
                else
                {
                    logo_io_write(io, "Procedure too long\n");
                    in_procedure_def = false;
                }
            }
            else if (in_procedure_def)
            {
                if (line_is_end(line_start))
                {
                    // Complete the procedure definition
                    if (proc_len + 4 < EDITOR_BUFFER_SIZE)
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
                else
                {
                    // Append line to procedure buffer with newline marker
                    if (proc_len + line_len + 4 < EDITOR_BUFFER_SIZE - 10)
                    {
                        memcpy(proc_buffer + proc_len, line_start, line_len);
                        proc_buffer[proc_len + line_len] = ' ';
                        proc_buffer[proc_len + line_len + 1] = '\\';
                        proc_buffer[proc_len + line_len + 2] = 'n';
                        proc_buffer[proc_len + line_len + 3] = ' ';
                        proc_len += line_len + 4;
                    }
                    else
                    {
                        logo_io_write(io, "Procedure too long\n");
                        in_procedure_def = false;
                        proc_len = 0;
                    }
                }
            }
            else
            {
                // Regular instruction - evaluate it
                Lexer lexer;
                Evaluator line_eval;
                lexer_init(&lexer, line_start);
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
                        if (strcasecmp(r.throw_tag, "toplevel") == 0)
                        {
                            // throw "toplevel returns to top level silently
                            break;
                        }
                        else
                        {
                            // Other uncaught throws are errors
                            char msg[128];
                            snprintf(msg, sizeof(msg), "No one caught %s\n", r.throw_tag);
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
        if (proc_len + 4 < EDITOR_BUFFER_SIZE)
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
    size_t pos = 0;
    editor_buffer[0] = '\0';
    bool first_proc = true;
    
    if (argc == 0)
    {
        // (edit) with no args - use current buffer content
        // For now, just open empty editor
        return run_editor_and_process(eval, editor_buffer);
    }
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        UserProcedure *proc = proc_find(name);
        if (proc == NULL)
        {
            // Procedure doesn't exist - create template: "to name\n"
            // Cursor will be on line below, ready for user to define body
            if (!buffer_append(editor_buffer, EDITOR_BUFFER_SIZE, &pos, "to "))
                return result_error_arg(ERR_OUT_OF_SPACE, "edit", NULL);
            if (!buffer_append(editor_buffer, EDITOR_BUFFER_SIZE, &pos, name))
                return result_error_arg(ERR_OUT_OF_SPACE, "edit", NULL);
            if (!buffer_append(editor_buffer, EDITOR_BUFFER_SIZE, &pos, "\n"))
                return result_error_arg(ERR_OUT_OF_SPACE, "edit", NULL);
        }
        else
        {
            if (!format_procedure_definition(editor_buffer, EDITOR_BUFFER_SIZE, &pos, proc))
            {
                return result_error_arg(ERR_OUT_OF_SPACE, "edit", NULL);
            }
        }
    }
    else if (value_is_list(args[0]))
    {
        // List of procedure names
        Node curr = args[0].as.node;
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                UserProcedure *proc = proc_find(name);
                if (proc == NULL)
                {
                    return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
                }
                
                // Add blank line between definitions
                if (!first_proc)
                {
                    if (!buffer_append(editor_buffer, EDITOR_BUFFER_SIZE, &pos, "\n"))
                    {
                        return result_error_arg(ERR_OUT_OF_SPACE, "edit", NULL);
                    }
                }
                first_proc = false;
                
                if (!format_procedure_definition(editor_buffer, EDITOR_BUFFER_SIZE, &pos, proc))
                {
                    return result_error_arg(ERR_OUT_OF_SPACE, "edit", NULL);
                }
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "edit", value_to_string(args[0]));
    }
    
    return run_editor_and_process(eval, editor_buffer);
}

// edn "name or edn [name1 name2 ...]
// Edit variable name(s) and value(s)
static Result prim_edn(Evaluator *eval, int argc, Value *args)
{
    size_t pos = 0;
    editor_buffer[0] = '\0';
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "edn", NULL);
    }
    
    if (value_is_word(args[0]))
    {
        // Single variable name
        const char *name = mem_word_ptr(args[0].as.node);
        Value value;
        if (!var_get(name, &value))
        {
            return result_error_arg(ERR_NO_VALUE, name, NULL);
        }
        if (!format_variable(editor_buffer, EDITOR_BUFFER_SIZE, &pos, name, value))
        {
            return result_error_arg(ERR_OUT_OF_SPACE, "edn", NULL);
        }
    }
    else if (value_is_list(args[0]))
    {
        // List of variable names
        Node curr = args[0].as.node;
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                Value value;
                if (!var_get(name, &value))
                {
                    return result_error_arg(ERR_NO_VALUE, name, NULL);
                }
                
                if (!format_variable(editor_buffer, EDITOR_BUFFER_SIZE, &pos, name, value))
                {
                    return result_error_arg(ERR_OUT_OF_SPACE, "edn", NULL);
                }
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "edn", value_to_string(args[0]));
    }
    
    return run_editor_and_process(eval, editor_buffer);
}

// edns - edit all variable names and values (not buried)
static Result prim_edns(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
    (void)args;
    
    size_t pos = 0;
    editor_buffer[0] = '\0';
    
    int count = var_global_count(false);  // Exclude buried
    for (int i = 0; i < count; i++)
    {
        const char *name;
        Value value;
        if (var_get_global_by_index(i, false, &name, &value))
        {
            if (!format_variable(editor_buffer, EDITOR_BUFFER_SIZE, &pos, name, value))
            {
                return result_error_arg(ERR_OUT_OF_SPACE, "edns", NULL);
            }
        }
    }
    
    return run_editor_and_process(eval, editor_buffer);
}

void primitives_editor_init(void)
{
    primitive_register("edit", 1, prim_edit);  // 1 argument, (edit) for none
    primitive_register("ed", 1, prim_edit);    // Abbreviation
    primitive_register("edn", 1, prim_edn);
    primitive_register("edns", 0, prim_edns);
}
