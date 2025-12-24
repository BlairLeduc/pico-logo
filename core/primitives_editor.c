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

// Static editor buffer (8KB as specified in reference)
#define EDITOR_BUFFER_SIZE 8192
static char editor_buffer[EDITOR_BUFFER_SIZE];

// Secondary buffer for newline conversion
static char converted_buffer[EDITOR_BUFFER_SIZE];

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

// Convert actual newlines to newline markers (\n) in the buffer
// This is needed because proc_define_from_text expects markers, not actual newlines
static bool convert_newlines_to_markers(const char *src, char *dst, size_t dst_size)
{
    size_t src_pos = 0;
    size_t dst_pos = 0;
    size_t src_len = strlen(src);
    
    while (src_pos < src_len)
    {
        if (src[src_pos] == '\n')
        {
            // Insert " \n " (space + marker + space)
            if (dst_pos + 4 >= dst_size - 1)
            {
                return false;  // Buffer too small
            }
            dst[dst_pos++] = ' ';
            dst[dst_pos++] = '\\';
            dst[dst_pos++] = 'n';
            dst[dst_pos++] = ' ';
            src_pos++;
        }
        else
        {
            if (dst_pos >= dst_size - 1)
            {
                return false;  // Buffer too small
            }
            dst[dst_pos++] = src[src_pos++];
        }
    }
    
    dst[dst_pos] = '\0';
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

// Run editor and process results
static Result run_editor_and_process(Evaluator *eval, char *buffer)
{
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
    
    // Convert newlines to markers before processing
    // proc_define_from_text expects \n markers, not actual newlines
    if (!convert_newlines_to_markers(buffer, converted_buffer, EDITOR_BUFFER_SIZE))
    {
        return result_error_arg(ERR_OUT_OF_SPACE, "edit", NULL);
    }
    
    // Process the buffer content - run through evaluator
    // Each line is executed as if typed at top level
    Result result = proc_define_from_text(converted_buffer);
    
    // If there was an error, show it at top level
    if (result.status == RESULT_ERROR)
    {
        logo_io_write(io, error_format(result));
        logo_io_write(io, "\n");
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
    bool first_var = true;
    
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
                
                // Add blank line between definitions
                if (!first_var)
                {
                    if (!buffer_append(editor_buffer, EDITOR_BUFFER_SIZE, &pos, "\n"))
                    {
                        return result_error_arg(ERR_OUT_OF_SPACE, "edn", NULL);
                    }
                }
                first_var = false;
                
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
    bool first_var = true;
    
    int count = var_global_count(false);  // Exclude buried
    for (int i = 0; i < count; i++)
    {
        const char *name;
        Value value;
        if (var_get_global_by_index(i, false, &name, &value))
        {
            // Add blank line between definitions
            if (!first_var)
            {
                if (!buffer_append(editor_buffer, EDITOR_BUFFER_SIZE, &pos, "\n"))
                {
                    return result_error_arg(ERR_OUT_OF_SPACE, "edns", NULL);
                }
            }
            first_var = false;
            
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
