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
// Simplified buffer-based formatting wrappers
//==========================================================================

bool format_procedure_to_buffer(FormatBufferContext *ctx, UserProcedure *proc)
{
    return format_procedure_definition(format_buffer_output, ctx, proc);
}

bool format_variable_to_buffer(FormatBufferContext *ctx, const char *name, Value value)
{
    return format_variable(format_buffer_output, ctx, name, value);
}

bool format_property_to_buffer(FormatBufferContext *ctx, const char *name,
                               const char *property, Node val_node)
{
    return format_property(format_buffer_output, ctx, name, property, val_node);
}

bool format_property_list_to_buffer(FormatBufferContext *ctx, const char *name, Node list)
{
    return format_property_list(format_buffer_output, ctx, name, list);
}

bool format_value_to_buffer(FormatBufferContext *ctx, Value value)
{
    return format_value(format_buffer_output, ctx, value);
}

bool format_value_show_to_buffer(FormatBufferContext *ctx, Value value)
{
    return format_value_show(format_buffer_output, ctx, value);
}

//==========================================================================
// Number formatting
//==========================================================================

// Format a number to a buffer using Logo conventions:
// - Remove trailing zeros after decimal point
// - Use 'e' for positive exponents (1e7), 'n' for negative exponents (1n6)
// Uses up to 6 significant digits for single-precision floats.
// Returns the number of characters written (excluding null terminator).
//
// This custom implementation avoids snprintf for efficiency on embedded systems.
int format_number(char *buf, size_t size, float n)
{
    if (size == 0)
        return 0;

    char *p = buf;
    char *end = buf + size - 1;  // Leave room for null terminator

    // Handle special cases
    if (n != n)  // NaN check
    {
        if (end - p >= 3)
        {
            *p++ = 'n';
            *p++ = 'a';
            *p++ = 'n';
        }
        *p = '\0';
        return (int)(p - buf);
    }

    // Handle sign
    if (n < 0)
    {
        if (p < end)
            *p++ = '-';
        n = -n;
    }

    // Handle infinity
    if (n > 3.4e38f)
    {
        if (end - p >= 3)
        {
            *p++ = 'i';
            *p++ = 'n';
            *p++ = 'f';
        }
        *p = '\0';
        return (int)(p - buf);
    }

    // Handle zero
    if (n == 0.0f)
    {
        if (p < end)
            *p++ = '0';
        *p = '\0';
        return (int)(p - buf);
    }

    // Determine the decimal exponent
    // We want to find exp10 such that 1 <= n * 10^(-exp10) < 10
    int exp10 = 0;
    float scaled = n;

    // Scale down large numbers
    if (scaled >= 10.0f)
    {
        // Use larger steps for efficiency
        while (scaled >= 1e32f) { scaled *= 1e-32f; exp10 += 32; }
        while (scaled >= 1e16f) { scaled *= 1e-16f; exp10 += 16; }
        while (scaled >= 1e8f)  { scaled *= 1e-8f;  exp10 += 8; }
        while (scaled >= 1e4f)  { scaled *= 1e-4f;  exp10 += 4; }
        while (scaled >= 10.0f) { scaled *= 0.1f;   exp10 += 1; }
    }
    // Scale up small numbers
    else if (scaled < 1.0f)
    {
        while (scaled < 1e-31f) { scaled *= 1e32f; exp10 -= 32; }
        while (scaled < 1e-15f) { scaled *= 1e16f; exp10 -= 16; }
        while (scaled < 1e-7f)  { scaled *= 1e8f;  exp10 -= 8; }
        while (scaled < 1e-3f)  { scaled *= 1e4f;  exp10 -= 4; }
        while (scaled < 1.0f)   { scaled *= 10.0f; exp10 -= 1; }
    }

    // Now scaled is in [1.0, 10.0) and exp10 is the exponent
    // Decide between fixed-point and scientific notation
    // Use fixed-point for exponents -4 to 5 (like %g behavior)
    bool use_scientific = (exp10 < -4 || exp10 > 5);

    // Extract up to 6 significant digits
    // Add small rounding factor
    scaled += 0.0000005f;
    if (scaled >= 10.0f)
    {
        scaled *= 0.1f;
        exp10++;
    }

    // Extract digits
    char digits[8];
    int num_digits = 0;
    float temp = scaled;
    for (int i = 0; i < 6 && temp > 0.000001f; i++)
    {
        int d = (int)temp;
        if (d > 9) d = 9;  // Clamp for safety
        digits[num_digits++] = '0' + d;
        temp = (temp - d) * 10.0f;
    }

    // Remove trailing zeros from significant digits
    while (num_digits > 1 && digits[num_digits - 1] == '0')
    {
        num_digits--;
    }

    if (use_scientific)
    {
        // Scientific notation: d.dddde±exp or d.ddddn±exp
        if (p < end)
            *p++ = digits[0];

        if (num_digits > 1)
        {
            if (p < end)
                *p++ = '.';
            for (int i = 1; i < num_digits && p < end; i++)
            {
                *p++ = digits[i];
            }
        }

        // Write exponent: 'e' for positive, 'n' for negative
        if (p < end)
            *p++ = (exp10 >= 0) ? 'e' : 'n';

        // Write absolute exponent value
        int abs_exp = (exp10 >= 0) ? exp10 : -exp10;
        if (abs_exp >= 100)
        {
            if (p < end) *p++ = '0' + (abs_exp / 100);
            abs_exp %= 100;
            if (p < end) *p++ = '0' + (abs_exp / 10);
            if (p < end) *p++ = '0' + (abs_exp % 10);
        }
        else if (abs_exp >= 10)
        {
            if (p < end) *p++ = '0' + (abs_exp / 10);
            if (p < end) *p++ = '0' + (abs_exp % 10);
        }
        else
        {
            if (p < end) *p++ = '0' + abs_exp;
        }
    }
    else
    {
        // Fixed-point notation
        // Position of decimal point: after digit at index exp10
        // e.g., exp10=2 means ###.### (3 digits before decimal)
        //       exp10=-1 means 0.0### (decimal before first digit)

        if (exp10 >= 0)
        {
            // Digits before decimal point
            int before_decimal = exp10 + 1;

            for (int i = 0; i < before_decimal && p < end; i++)
            {
                if (i < num_digits)
                    *p++ = digits[i];
                else
                    *p++ = '0';
            }

            // Digits after decimal point (if any significant ones remain)
            if (num_digits > before_decimal)
            {
                if (p < end)
                    *p++ = '.';
                for (int i = before_decimal; i < num_digits && p < end; i++)
                {
                    *p++ = digits[i];
                }
            }
        }
        else
        {
            // exp10 is negative: need leading "0." and possibly zeros
            if (p < end)
                *p++ = '0';
            if (p < end)
                *p++ = '.';

            // Leading zeros after decimal point
            int leading_zeros = -exp10 - 1;
            for (int i = 0; i < leading_zeros && p < end; i++)
            {
                *p++ = '0';
            }

            // Significant digits
            for (int i = 0; i < num_digits && p < end; i++)
            {
                *p++ = digits[i];
            }
        }
    }

    *p = '\0';
    return (int)(p - buf);
}

//==========================================================================
// Newline-aware list formatting helpers
//==========================================================================

// Check if a list contains any newline markers
static bool list_has_newlines(Node list)
{
    Node curr = list;
    while (!mem_is_nil(curr))
    {
        Node elem = mem_car(curr);
        if (mem_is_newline(elem))
        {
            return true;
        }
        // Check nested lists too
        if (mem_is_list(elem) && list_has_newlines(elem))
        {
            return true;
        }
        curr = mem_cdr(curr);
    }
    return false;
}

// Check if only newline markers remain in the list (for closing bracket indent)
static bool only_newlines_remain(Node list)
{
    Node curr = list;
    while (!mem_is_nil(curr))
    {
        Node elem = mem_car(curr);
        if (!mem_is_newline(elem))
        {
            return false;
        }
        curr = mem_cdr(curr);
    }
    return true;
}

// Forward declaration for mutual recursion
static bool format_list_with_newlines(FormatOutputFunc out, void *ctx, Node list, int depth);

// Format a body element, handling nested lists with newlines
static bool format_body_element_multiline(FormatOutputFunc out, void *ctx, Node elem, int depth)
{
    if (mem_is_newline(elem))
    {
        return true;  // Skip newline markers - handled at list level
    }
    else if (mem_is_word(elem))
    {
        return out(ctx, mem_word_ptr(elem));
    }
    else if (mem_is_list(elem))
    {
        // Check if this nested list has newlines
        if (list_has_newlines(elem))
        {
            return format_list_with_newlines(out, ctx, elem, depth);
        }
        else
        {
            // Format without newlines (compact form)
            if (!out(ctx, "["))
                return false;
            bool first = true;
            Node curr = elem;
            while (!mem_is_nil(curr))
            {
                Node e = mem_car(curr);
                if (!mem_is_newline(e))
                {
                    if (!first)
                    {
                        if (!out(ctx, " "))
                            return false;
                    }
                    first = false;
                    if (!format_body_element_multiline(out, ctx, e, depth))
                        return false;
                }
                curr = mem_cdr(curr);
            }
            return out(ctx, "]");
        }
    }
    return true;
}

// Format a list that contains newline markers, with proper indentation
static bool format_list_with_newlines(FormatOutputFunc out, void *ctx, Node list, int depth)
{
    if (!out(ctx, "["))
        return false;
    
    bool first = true;
    bool at_line_start = false;
    Node curr = list;
    
    while (!mem_is_nil(curr))
    {
        Node elem = mem_car(curr);
        Node next = mem_cdr(curr);
        
        if (mem_is_newline(elem))
        {
            if (!out(ctx, "\n"))
                return false;
            at_line_start = true;
            first = true;
        }
        else
        {
            if (at_line_start)
            {
                // Indent: 2 spaces base + 2 spaces per depth level
                // But reduce by 1 level if only newlines remain (for closing bracket)
                int indent_depth = depth + 1;
                if (only_newlines_remain(next))
                {
                    indent_depth = depth;
                }
                for (int i = 0; i < indent_depth; i++)
                {
                    if (!out(ctx, "  "))
                        return false;
                }
                at_line_start = false;
            }
            else if (!first)
            {
                if (!out(ctx, " "))
                    return false;
            }
            first = false;
            
            if (!format_body_element_multiline(out, ctx, elem, depth + 1))
                return false;
        }
        
        curr = next;
    }
    
    // If we ended with a newline, indent the closing bracket to match the opening
    if (at_line_start)
    {
        for (int i = 0; i < depth; i++)
        {
            if (!out(ctx, "  "))
                return false;
        }
    }
    
    return out(ctx, "]");
}

//==========================================================================
// Core formatting functions
//==========================================================================

// Format a procedure body element (handles nested lists)
bool format_body_element(FormatOutputFunc out, void *ctx, Node elem)
{
    // Check for newlines in lists and use appropriate formatter
    if (mem_is_list(elem) && list_has_newlines(elem))
    {
        // Use depth 1 since we're at procedure body level
        return format_list_with_newlines(out, ctx, elem, 1);
    }
    
    if (mem_is_newline(elem))
    {
        return true;  // Skip newline markers
    }
    else if (mem_is_word(elem))
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

//==========================================================================
// Value output functions (for print/show/type primitives)
//==========================================================================

// Format list contents without outer brackets (recursive)
bool format_list_contents(FormatOutputFunc out, void *ctx, Node node)
{
    bool first = true;
    while (!mem_is_nil(node))
    {
        Node element = mem_car(node);
        
        // Skip newline markers - they are for formatting only
        if (mem_is_newline(element))
        {
            node = mem_cdr(node);
            continue;
        }
        
        if (!first)
        {
            if (!out(ctx, " "))
                return false;
        }
        first = false;

        if (mem_is_word(element))
        {
            if (!out(ctx, mem_word_ptr(element)))
                return false;
        }
        else if (mem_is_list(element))
        {
            if (!out(ctx, "["))
                return false;
            if (!format_list_contents(out, ctx, element))
                return false;
            if (!out(ctx, "]"))
                return false;
        }
        else if (mem_is_nil(element))
        {
            // Empty list as element
            if (!out(ctx, "[]"))
                return false;
        }
        node = mem_cdr(node);
    }
    return true;
}

// Format a value without outer brackets on lists (for print/type)
bool format_value(FormatOutputFunc out, void *ctx, Value value)
{
    char buf[32];
    switch (value.type)
    {
    case VALUE_NONE:
    case VALUE_NEWLINE:
        break;
    case VALUE_NUMBER:
        format_number(buf, sizeof(buf), value.as.number);
        if (!out(ctx, buf))
            return false;
        break;
    case VALUE_WORD:
        if (!out(ctx, mem_word_ptr(value.as.node)))
            return false;
        break;
    case VALUE_LIST:
        if (!format_list_contents(out, ctx, value.as.node))
            return false;
        break;
    }
    return true;
}

// Format a value with brackets around lists (for show)
bool format_value_show(FormatOutputFunc out, void *ctx, Value value)
{
    char buf[32];
    switch (value.type)
    {
    case VALUE_NONE:
    case VALUE_NEWLINE:
        break;
    case VALUE_NUMBER:
        format_number(buf, sizeof(buf), value.as.number);
        if (!out(ctx, buf))
            return false;
        break;
    case VALUE_WORD:
        if (!out(ctx, mem_word_ptr(value.as.node)))
            return false;
        break;
    case VALUE_LIST:
        if (!out(ctx, "["))
            return false;
        if (!format_list_contents(out, ctx, value.as.node))
            return false;
        if (!out(ctx, "]"))
            return false;
        break;
    }
    return true;
}
