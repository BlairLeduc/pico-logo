//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unified formatting functions for procedures, variables, and properties.
//  Used by po*, ed*, and save commands.
//

#include "format.h"
#include "memory.h"
#include "value.h"
#include <stdlib.h>
#include <string.h>

//==========================================================================
// Buffer context implementation
//==========================================================================

void format_buffer_init(FormatBufferContext *ctx, char *buffer, size_t buffer_size)
{
    ctx->buffer = buffer;
    ctx->buffer_size = buffer_size;
    ctx->pos = 0;
    if (buffer && buffer_size > 0)
    {
        buffer[0] = '\0';
    }
}

bool format_buffer_output(void *ctx, const char *str)
{
    FormatBufferContext *buf_ctx = (FormatBufferContext *)ctx;
    size_t len = strlen(str);
    if (buf_ctx->pos + len >= buf_ctx->buffer_size - 1)
    {
        return false;
    }
    memcpy(buf_ctx->buffer + buf_ctx->pos, str, len);
    buf_ctx->pos += len;
    buf_ctx->buffer[buf_ctx->pos] = '\0';
    return true;
}

size_t format_buffer_pos(FormatBufferContext *ctx)
{
    return ctx->pos;
}

//==========================================================================
// Core formatting functions
//==========================================================================

// Format a procedure body element (handles nested lists)
bool format_body_element(FormatOutputFunc out, void *ctx, Node elem)
{
    if (mem_is_word(elem))
    {
        return out(ctx, mem_word_ptr(elem));
    }
    else if (mem_is_list(elem))
    {
        if (!out(ctx, "["))
            return false;
        bool first = true;
        Node curr = elem;
        while (!mem_is_nil(curr))
        {
            if (!first)
            {
                if (!out(ctx, " "))
                    return false;
            }
            first = false;
            if (!format_body_element(out, ctx, mem_car(curr)))
                return false;
            curr = mem_cdr(curr);
        }
        return out(ctx, "]");
    }
    return true;
}

// Format a procedure title line only (to name :param1 :param2 ...)
bool format_procedure_title(FormatOutputFunc out, void *ctx, UserProcedure *proc)
{
    if (!out(ctx, "to "))
        return false;
    if (!out(ctx, proc->name))
        return false;
    
    for (int i = 0; i < proc->param_count; i++)
    {
        if (!out(ctx, " :"))
            return false;
        if (!out(ctx, proc->params[i]))
            return false;
    }
    return out(ctx, "\n");
}

// Format a complete procedure definition (to...end)
bool format_procedure_definition(FormatOutputFunc out, void *ctx, UserProcedure *proc)
{
    if (!format_procedure_title(out, ctx, proc))
        return false;
    
    // Body is a list of line-lists: [[line1-tokens] [line2-tokens] ...]
    Node curr_line = proc->body;
    
    // Track cumulative bracket depth across lines for indentation
    int bracket_depth = 0;
    
    while (!mem_is_nil(curr_line))
    {
        Node line = mem_car(curr_line);
        Node tokens = line;
        
        // Handle the list type marker
        if (NODE_GET_TYPE(line) == NODE_TYPE_LIST)
        {
            tokens = NODE_MAKE_LIST(NODE_GET_INDEX(line));
        }
        
        // Count leading closing brackets to reduce indent for this line
        Node peek = tokens;
        while (!mem_is_nil(peek))
        {
            Node elem = mem_car(peek);
            if (mem_is_word(elem) && strcmp(mem_word_ptr(elem), "]") == 0 && bracket_depth > 0)
            {
                bracket_depth--;
            }
            else
            {
                break;  // Stop at first non-] token
            }
            peek = mem_cdr(peek);
        }
        
        // Print base indent (2 spaces for procedure body)
        if (!out(ctx, "  "))
            return false;
        
        // Add extra indentation based on cumulative bracket depth
        for (int i = 0; i < bracket_depth; i++)
        {
            if (!out(ctx, "  "))
                return false;
        }
        
        while (!mem_is_nil(tokens))
        {
            Node elem = mem_car(tokens);
            
            if (!format_body_element(out, ctx, elem))
                return false;
            
            // Track brackets
            if (mem_is_word(elem))
            {
                const char *word = mem_word_ptr(elem);
                if (strcmp(word, "[") == 0)
                {
                    bracket_depth++;
                }
                else if (strcmp(word, "]") == 0 && bracket_depth > 0)
                {
                    bracket_depth--;
                }
            }
            
            // Add space between elements on same line
            Node next = mem_cdr(tokens);
            if (!mem_is_nil(next))
            {
                if (!out(ctx, " "))
                    return false;
            }
            tokens = next;
        }
        
        if (!out(ctx, "\n"))
            return false;
        curr_line = mem_cdr(curr_line);
    }
    
    return out(ctx, "end\n");
}

// Format a variable as a make command
bool format_variable(FormatOutputFunc out, void *ctx, const char *name, Value value)
{
    char num_buf[64];
    
    if (!out(ctx, "make \""))
        return false;
    if (!out(ctx, name))
        return false;
    if (!out(ctx, " "))
        return false;
    
    switch (value.type)
    {
    case VALUE_NUMBER:
        format_number(num_buf, sizeof(num_buf), value.as.number);
        if (!out(ctx, num_buf))
            return false;
        break;
    case VALUE_WORD:
        if (!out(ctx, "\""))
            return false;
        if (!out(ctx, mem_word_ptr(value.as.node)))
            return false;
        break;
    case VALUE_LIST:
        if (!out(ctx, "["))
            return false;
        {
            bool first = true;
            Node curr = value.as.node;
            while (!mem_is_nil(curr))
            {
                if (!first)
                {
                    if (!out(ctx, " "))
                        return false;
                }
                first = false;
                if (!format_body_element(out, ctx, mem_car(curr)))
                    return false;
                curr = mem_cdr(curr);
            }
        }
        if (!out(ctx, "]"))
            return false;
        break;
    default:
        break;
    }
    
    return out(ctx, "\n");
}

// Format a single property as a pprop command
bool format_property(FormatOutputFunc out, void *ctx, const char *name, 
                     const char *property, Node val_node)
{
    char num_buf[64];
    
    if (!out(ctx, "pprop \""))
        return false;
    if (!out(ctx, name))
        return false;
    if (!out(ctx, " \""))
        return false;
    if (!out(ctx, property))
        return false;
    if (!out(ctx, " "))
        return false;
    
    // Format the value
    if (mem_is_word(val_node))
    {
        // Check if it's a number (numbers are stored as words)
        const char *str = mem_word_ptr(val_node);
        char *endptr;
        strtof(str, &endptr);
        if (*endptr == '\0' && str[0] != '\0')
        {
            // It's a number, output without quote
            if (!out(ctx, str))
                return false;
        }
        else
        {
            // It's a word, output with quote
            if (!out(ctx, "\""))
                return false;
            if (!out(ctx, str))
                return false;
        }
    }
    else if (mem_is_list(val_node))
    {
        if (!format_body_element(out, ctx, val_node))
            return false;
    }
    
    return out(ctx, "\n");
}

// Format an entire property list (outputs multiple pprop commands)
bool format_property_list(FormatOutputFunc out, void *ctx, const char *name, Node list)
{
    // Property lists are stored as [prop1 val1 prop2 val2 ...]
    Node curr = list;
    while (!mem_is_nil(curr) && !mem_is_nil(mem_cdr(curr)))
    {
        Node prop_node = mem_car(curr);
        Node val_node = mem_car(mem_cdr(curr));
        
        if (mem_is_word(prop_node))
        {
            const char *property = mem_word_ptr(prop_node);
            if (!format_property(out, ctx, name, property, val_node))
                return false;
        }
        
        curr = mem_cdr(mem_cdr(curr));
    }
    return true;
}
