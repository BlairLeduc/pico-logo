//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unified formatting functions for procedures, variables, and properties.
//  Used by po*, ed*, and save commands.
//

#ifndef LOGO_FORMAT_H
#define LOGO_FORMAT_H

#include "memory.h"
#include "procedures.h"
#include "value.h"
#include <stdbool.h>
#include <stddef.h>

// Output callback function type
// Returns true on success, false on error (e.g., buffer overflow)
typedef bool (*FormatOutputFunc)(void *ctx, const char *str);

// Format a procedure body element (handles nested lists)
bool format_body_element(FormatOutputFunc out, void *ctx, Node elem);

// Format a complete procedure definition (to...end)
bool format_procedure_definition(FormatOutputFunc out, void *ctx, UserProcedure *proc);

// Format a procedure title line only (to name :param1 :param2 ...)
bool format_procedure_title(FormatOutputFunc out, void *ctx, UserProcedure *proc);

// Format a variable as a make command
bool format_variable(FormatOutputFunc out, void *ctx, const char *name, Value value);

// Format a single property as a pprop command
bool format_property(FormatOutputFunc out, void *ctx, const char *name, 
                     const char *property, Node val_node);

// Format an entire property list (outputs multiple pprop commands)
bool format_property_list(FormatOutputFunc out, void *ctx, const char *name, Node list);

//==========================================================================
// Value output functions (for print/show/type primitives)
//==========================================================================

// Format a value without outer brackets on lists (for print/type)
bool format_value(FormatOutputFunc out, void *ctx, Value value);

// Format a value with brackets around lists (for show)
bool format_value_show(FormatOutputFunc out, void *ctx, Value value);

// Format list contents without outer brackets
bool format_list_contents(FormatOutputFunc out, void *ctx, Node node);

//==========================================================================
// Pre-built output contexts for common use cases
//==========================================================================

// Context for buffered output (used by editor)
typedef struct {
    char *buffer;
    size_t buffer_size;
    size_t pos;
} FormatBufferContext;

// Initialize a buffer context
void format_buffer_init(FormatBufferContext *ctx, char *buffer, size_t buffer_size);

// Buffer output callback
bool format_buffer_output(void *ctx, const char *str);

// Get current position in buffer
size_t format_buffer_pos(FormatBufferContext *ctx);

//==========================================================================
// Simplified buffer-based formatting (for common cases)
// These wrappers hide the callback pattern for convenience
//==========================================================================

// Format a procedure definition directly to a buffer
// Returns true on success, false if buffer too small
bool format_procedure_to_buffer(FormatBufferContext *ctx, UserProcedure *proc);

// Format a variable directly to a buffer
// Returns true on success, false if buffer too small
bool format_variable_to_buffer(FormatBufferContext *ctx, const char *name, Value value);

// Format a property directly to a buffer
// Returns true on success, false if buffer too small
bool format_property_to_buffer(FormatBufferContext *ctx, const char *name,
                               const char *property, Node val_node);

// Format a property list directly to a buffer
// Returns true on success, false if buffer too small
bool format_property_list_to_buffer(FormatBufferContext *ctx, const char *name, Node list);

// Format a value directly to a buffer (for print/type)
// Returns true on success, false if buffer too small
bool format_value_to_buffer(FormatBufferContext *ctx, Value value);

// Format a value with brackets around lists directly to a buffer (for show)
// Returns true on success, false if buffer too small
bool format_value_show_to_buffer(FormatBufferContext *ctx, Value value);

//==========================================================================
// Number formatting
//==========================================================================

// Format a number to a buffer, removing trailing zeros.
// Uses 'e' for positive exponents (1e7), 'n' for negative exponents (1n6).
// Uses up to 6 significant digits for single-precision floats.
// Returns the number of characters written (excluding null terminator).
int format_number(char *buf, size_t size, float n);

#endif // LOGO_FORMAT_H
