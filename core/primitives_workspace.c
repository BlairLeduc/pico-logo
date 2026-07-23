//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Workspace management primitives: po, poall, pon, pons, pops, pot, pots,
//                                   nodes, primitives, recycle
//

#include "primitives.h"
#include "demons.h"
#include "procedures.h"
#include "variables.h"
#include "properties.h"
#include "memory.h"
#include "error.h"
#include "eval.h"
#include "frame.h"
#include "format.h"
#include "help.h"
#include "devices/io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define WS_FORMAT_BUFFER_SIZE 8192

// Shared formatting buffer for workspace printing.
//
// Each ws_write_* helper formats one item (procedure, variable, property)
// at a time before flushing to the I/O layer. The helpers are not
// reentrant (workspace primitives don't run user code), so a single
// static buffer is safe and avoids placing 8 KB on the stack per call —
// a significant hazard on Pico, where the default per-task stack is
// only a few kilobytes.
static char ws_format_buffer[WS_FORMAT_BUFFER_SIZE];

// Output callback for workspace printing (always succeeds)
static bool ws_output(void *ctx, const char *str)
{
    LogoIO *io = (LogoIO *)ctx;
    if (!io)
    {
        io = primitives_get_io();
    }
    if (io)
    {
        logo_io_write(io, str);
    }
    return true;
}

static void ws_newline(void)
{
    LogoIO *io = primitives_get_io();
    ws_output(io, "\n");
}

static void ws_write_text(LogoIO *io, const char *text)
{
    if (io && text)
    {
        logo_io_write_syntax_highlighted(io, text, 0);
    }
}

static void ws_write_procedure_definition(LogoIO *io, UserProcedure *proc)
{
    FormatBufferContext ctx;

    format_buffer_init(&ctx, ws_format_buffer, sizeof(ws_format_buffer));
    if (format_procedure_definition(format_buffer_output, &ctx, proc))
    {
        ws_write_text(io, ws_format_buffer);
        return;
    }

    format_procedure_definition(ws_output, io, proc);
}

static void ws_write_procedure_title(LogoIO *io, UserProcedure *proc)
{
    FormatBufferContext ctx;

    format_buffer_init(&ctx, ws_format_buffer, sizeof(ws_format_buffer));
    if (format_procedure_title(format_buffer_output, &ctx, proc))
    {
        ws_write_text(io, ws_format_buffer);
        return;
    }

    format_procedure_title(ws_output, io, proc);
}

static void ws_write_variable(LogoIO *io, const char *name, Value value)
{
    FormatBufferContext ctx;

    format_buffer_init(&ctx, ws_format_buffer, sizeof(ws_format_buffer));
    if (format_variable(format_buffer_output, &ctx, name, value))
    {
        ws_write_text(io, ws_format_buffer);
        return;
    }

    format_variable(ws_output, io, name, value);
}

static void ws_write_property_list(LogoIO *io, const char *name, Node list)
{
    FormatBufferContext ctx;

    format_buffer_init(&ctx, ws_format_buffer, sizeof(ws_format_buffer));
    if (format_property_list(format_buffer_output, &ctx, name, list))
    {
        ws_write_text(io, ws_format_buffer);
        return;
    }

    format_property_list(ws_output, io, name, list);
}

// po "name or po [name1 name2 ...]
// Print out procedure definition(s)
static Result prim_po(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    LogoIO *io = primitives_get_io();
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        UserProcedure *proc = proc_find(name);
        if (proc == NULL)
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        ws_write_procedure_definition(io, proc);
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
                ws_write_procedure_definition(io, proc);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return result_none();
}

// poall - print all procedures, variables, and properties (not buried)
static Result prim_poall(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    LogoIO *io = primitives_get_io();
    
    // Print all procedures (not buried)
    int count = proc_count(true);  // Get ALL, filter by buried in loop
    for (int i = 0; i < count; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            ws_write_procedure_definition(io, proc);
            ws_newline();
        }
    }
    
    // Print all variables (not buried)
    int var_count = var_global_count(false);
    for (int i = 0; i < var_count; i++)
    {
        const char *name;
        Value value;
        if (var_get_global_by_index(i, false, &name, &value))
        {
            ws_write_variable(io, name, value);
        }
    }
    
    // Print all property lists (as pprop commands)
    int prop_count = prop_name_count();
    for (int i = 0; i < prop_count; i++)
    {
        const char *name;
        if (prop_get_name_by_index(i, &name))
        {
            Node list = prop_get_list(name);
            ws_write_property_list(io, name, list);
        }
    }
    
    return result_none();
}

// pon "name or pon [name1 name2 ...]
// Print out variable name(s) and value(s)
static Result prim_pon(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    LogoIO *io = primitives_get_io();
    
    if (value_is_word(args[0]))
    {
        // Single variable name
        const char *name = mem_word_ptr(args[0].as.node);
        Value value;
        if (!var_get(name, &value))
        {
            return result_error_arg(ERR_NO_VALUE, NULL, name);
        }
        ws_write_variable(io, name, value);
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
                    return result_error_arg(ERR_NO_VALUE, NULL, name);
                }
                ws_write_variable(io, name, value);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return result_none();
}

// pons - print out all variable names and values (not buried)
// Also prints local variables if in a procedure scope (e.g., during pause)
static Result prim_pons(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    LogoIO *io = primitives_get_io();
    
    // Print local variables first (if any)
    int local_count = var_local_count();
    if (local_count > 0)
    {
        for (int i = 0; i < local_count; i++)
        {
            const char *name;
            Value value;
            if (var_get_local_by_index(i, &name, &value))
            {
                ws_write_variable(io, name, value);
            }
        }
    }
    
    // Print global variables (skip those shadowed by locals)
    int count = var_global_count(false);
    for (int i = 0; i < count; i++)
    {
        const char *name;
        Value value;
        if (var_get_global_by_index(i, false, &name, &value))
        {
            // Skip if shadowed by a local variable
            if (!var_is_shadowed_by_local(name))
            {
                ws_write_variable(io, name, value);
            }
        }
    }
    
    return result_none();
}

// pops - print out all procedure definitions (not buried)
static Result prim_pops(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    LogoIO *io = primitives_get_io();
    
    int count = proc_count(true);  // Get ALL, filter by buried in loop
    for (int i = 0; i < count; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            ws_write_procedure_definition(io, proc);
            ws_newline();
        }
    }
    
    return result_none();
}

// pot "name or pot [name1 name2 ...]
// Print out procedure title(s)
static Result prim_pot(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    LogoIO *io = primitives_get_io();
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        UserProcedure *proc = proc_find(name);
        if (proc == NULL)
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        ws_write_procedure_title(io, proc);
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
                ws_write_procedure_title(io, proc);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return result_none();
}

// pots - print out all procedure titles (not buried)
static Result prim_pots(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    LogoIO *io = primitives_get_io();
    
    int count = proc_count(true);  // Get ALL procedures, filter by buried in loop
    for (int i = 0; i < count; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            ws_write_procedure_title(io, proc);
        }
    }
    
    return result_none();
}

// bury "name or bury [name1 name2 ...]
// Bury procedure(s) - hidden from poall, pops, pots, erall, erps
static Result prim_bury(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!proc_exists(name))
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        proc_bury(name);
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
                if (!proc_exists(name))
                {
                    return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
                }
                proc_bury(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return result_none();
}

// buryall - bury all procedures and variable names
static Result prim_buryall(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    proc_bury_all();
    var_bury_all();
    
    return result_none();
}

// buryname "name or buryname [name1 name2 ...]
// Bury variable name(s)
static Result prim_buryname(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!var_exists(name))
        {
            return result_error_arg(ERR_NO_VALUE, NULL, name);
        }
        var_bury(name);
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
                if (!var_exists(name))
                {
                    return result_error_arg(ERR_NO_VALUE, NULL, name);
                }
                var_bury(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return result_none();
}

// unbury "name or unbury [name1 name2 ...]
// Unbury procedure(s)
static Result prim_unbury(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!proc_exists(name))
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        proc_unbury(name);
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
                if (!proc_exists(name))
                {
                    return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
                }
                proc_unbury(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return result_none();
}

// unburyall - unbury all procedures and variable names
static Result prim_unburyall(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    proc_unbury_all();
    var_unbury_all();
    
    return result_none();
}

// unburyname "name or unburyname [name1 name2 ...]
// Unbury variable name(s)
static Result prim_unburyname(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!var_exists(name))
        {
            return result_error_arg(ERR_NO_VALUE, NULL, name);
        }
        var_unbury(name);
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
                if (!var_exists(name))
                {
                    return result_error_arg(ERR_NO_VALUE, NULL, name);
                }
                var_unbury(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return result_none();
}

// erall - erase all procedures, variables, and properties (respects buried)
static Result prim_erall(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    proc_erase_all(true);  // true = check buried flag
    var_erase_all_globals(true);
    prop_erase_all();
    
    return result_none();
}

// erase "name or erase [name1 name2 ...]
// Erase procedure(s) from workspace
static Result prim_erase(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!proc_exists(name))
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        proc_erase(name);
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
                if (!proc_exists(name))
                {
                    return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
                }
                proc_erase(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return result_none();
}

// ern "name or ern [name1 name2 ...]
// Erase variable name(s)
static Result prim_ern(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!var_exists(name))
        {
            return result_error_arg(ERR_NO_VALUE, NULL, name);
        }
        var_erase(name);
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
                if (!var_exists(name))
                {
                    return result_error_arg(ERR_NO_VALUE, NULL, name);
                }
                var_erase(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return result_none();
}

// erns - erase all variables (respects buried)
static Result prim_erns(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    var_erase_all_globals(true);  // true = check buried flag
    
    return result_none();
}

// erps - erase all procedures (respects buried)
static Result prim_erps(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    proc_erase_all(true);  // true = check buried flag
    
    return result_none();
}

// primitives
// Outputs a list of all primitive names in alphabetical order
static int compare_strings(const void *a, const void *b)
{
    return strcasecmp(*(const char **)a, *(const char **)b);
}

static Result prim_primitives(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    int count = primitive_get_count();

    // Collect unique names (aliases share the same func, but all names are included)
    const char **names = malloc(count * sizeof(const char *));
    if (!names)
        return result_error(ERR_OUT_OF_SPACE);

    for (int i = 0; i < count; i++)
        names[i] = primitive_get_by_index(i)->name;

    // Sort alphabetically
    qsort(names, count, sizeof(const char *), compare_strings);

    // Build list in reverse order so first name ends up at the head
    Node list = NODE_NIL;
    for (int i = count - 1; i >= 0; i--)
    {
        Node atom = mem_atom_cstr(names[i]);
        list = mem_cons(atom, list);
    }

    free(names);
    return result_ok(value_list(list));
}

// nodes
// Outputs the number of free nodes available
static Result prim_nodes(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    size_t free = mem_free_nodes();
    return result_ok(value_number((float)free));
}

// recycle
// Runs garbage collection to free up as many nodes as possible
static Result prim_recycle(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc); UNUSED(args);

    // Mark the workspace roots: variables, procedure bodies, property lists
    var_gc_mark_all();
    proc_gc_mark_all();
    prop_gc_mark_all();

    // Mark everything the live evaluation still references: procedure
    // frames (parameters and locals), in-flight operations on the op stack
    // (loop bodies, saved token positions, staged arguments), the token
    // source we are currently reading from. Pending tail calls are covered by
    // proc_gc_mark_all().
    // Suspended parent evaluators (e.g. across a pause) are covered too:
    // their state lives on the shared op stack.
    frame_gc_mark_all(proc_get_frame_stack());
    op_stack_gc_mark(eval->op_stack);
    token_source_gc_mark(&eval->token_source);
    demons_gc_mark_all();

    // Sweep unmarked nodes
    mem_gc_sweep();

    return result_none();
}

// -------------------------------------------------------------------------
// help "name   |   (help)
// -------------------------------------------------------------------------

// Emit primitive names as wrapped lines (39-column budget, 2-space indent)
static void help_list_add(LogoIO *io, char *line, size_t cap, size_t *len, const char *name)
{
    size_t nlen = strlen(name);
    if (*len > 0 && *len + 1 + nlen > 39) {
        logo_io_write_line(io, line);
        *len = 0;
    }
    if (*len == 0) {
        snprintf(line, cap, "  %s", name);
        *len = 2 + nlen;
    } else {
        snprintf(line + *len, cap - *len, " %s", name);
        *len += 1 + nlen;
    }
}

static void help_list_flush(LogoIO *io, char *line, size_t *len)
{
    if (*len > 0) {
        logo_io_write_line(io, line);
        *len = 0;
    }
}

static Result prim_help(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    LogoIO *io = primitives_get_io();

    if (argc == 0) {
        // (help) with no inputs: list every primitive by category
        if (io) {
            char line[80];
            size_t len = 0;
            for (int c = 0; c < help_category_count; c++) {
                logo_io_write_line(io, help_categories[c]);
                for (int i = 0; i < help_entry_count; i++) {
                    if (help_entries[i].category == c) {
                        help_list_add(io, line, sizeof(line), &len, help_entries[i].name);
                    }
                }
                help_list_flush(io, line, &len);
            }
        }
        return result_none();
    }

    if (argc > 1) {
        return result_error_arg(ERR_TOO_MANY_INPUTS, "help", NULL);
    }

    if (!value_is_word(args[0])) {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const char *name = mem_word_ptr(args[0].as.node);
    const char *text = help_lookup(name);
    if (text != NULL) {
        if (io) {
            logo_io_write(io, text);
        }
        return result_none();
    }

    // No exact entry: keyword search, first over names, then over help text
    int hits = 0;
    bool by_text = false;
    for (int i = 0; i < help_entry_count; i++) {
        if (help_contains_nocase(help_entries[i].name, name)) hits++;
    }
    if (hits == 0) {
        by_text = true;
        for (int i = 0; i < help_entry_count; i++) {
            if (help_contains_nocase(help_entries[i].text, name)) hits++;
        }
    }
    if (hits == 0) {
        return result_error_arg(ERR_DONT_KNOW_ABOUT, name, NULL);
    }

    if (io) {
        char header[96];
        snprintf(header, sizeof(header), "No help for %s. Related:", name);
        logo_io_write_line(io, header);
        char line[80];
        size_t len = 0;
        for (int i = 0; i < help_entry_count; i++) {
            const char *hay = by_text ? help_entries[i].text : help_entries[i].name;
            if (help_contains_nocase(hay, name)) {
                help_list_add(io, line, sizeof(line), &len, help_entries[i].name);
            }
        }
        help_list_flush(io, line, &len);
    }
    return result_none();
}

void primitives_workspace_init(void)
{
    primitive_register("po", 1, prim_po);
    primitive_register("poall", 0, prim_poall);
    primitive_register("pon", 1, prim_pon);
    primitive_register("pons", 0, prim_pons);
    primitive_register("pops", 0, prim_pops);
    primitive_register("pot", 1, prim_pot);
    primitive_register("pots", 0, prim_pots);
    
    // Bury/unbury commands
    primitive_register("bury", 1, prim_bury);
    primitive_register("buryall", 0, prim_buryall);
    primitive_register("buryname", 1, prim_buryname);
    primitive_register("unbury", 1, prim_unbury);
    primitive_register("unburyall", 0, prim_unburyall);
    primitive_register("unburyname", 1, prim_unburyname);
    
    // Erase commands
    primitive_register("erall", 0, prim_erall);
    primitive_register("erase", 1, prim_erase);
    primitive_register("er", 1, prim_erase);  // Abbreviation
    primitive_register("ern", 1, prim_ern);
    primitive_register("erns", 0, prim_erns);
    primitive_register("erps", 0, prim_erps);
    
    // Memory management
    primitive_register("nodes", 0, prim_nodes);
    primitive_register("primitives", 0, prim_primitives);
    primitive_register("recycle", 0, prim_recycle);
    
    // Help
    primitive_register("help", 1, prim_help);
}
