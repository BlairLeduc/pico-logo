//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Workspace management primitives: po, poall, pon, pons, pops, pot, pots,
//                                   nodes, recycle
//

#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "properties.h"
#include "memory.h"
#include "error.h"
#include "eval.h"
#include "format.h"
#include "devices/io.h"
#include <stdio.h>
#include <string.h>

// Output callback for workspace printing (always succeeds)
static bool ws_output(void *ctx, const char *str)
{
    (void)ctx;
    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_write(io, str);
    }
    return true;
}

static void ws_newline(void)
{
    ws_output(NULL, "\n");
}

// po "name or po [name1 name2 ...]
// Print out procedure definition(s)
static Result prim_po(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        UserProcedure *proc = proc_find(name);
        if (proc == NULL)
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        format_procedure_definition(ws_output, NULL, proc);
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
                format_procedure_definition(ws_output, NULL, proc);
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
    
    // Print all procedures (not buried)
    int count = proc_count(true);  // Get ALL, filter by buried in loop
    for (int i = 0; i < count; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            format_procedure_definition(ws_output, NULL, proc);
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
            format_variable(ws_output, NULL, name, value);
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
            format_property_list(ws_output, NULL, name, list);
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
    
    if (value_is_word(args[0]))
    {
        // Single variable name
        const char *name = mem_word_ptr(args[0].as.node);
        Value value;
        if (!var_get(name, &value))
        {
            return result_error_arg(ERR_NO_VALUE, NULL, name);
        }
        format_variable(ws_output, NULL, name, value);
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
                format_variable(ws_output, NULL, name, value);
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
                format_variable(ws_output, NULL, name, value);
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
                format_variable(ws_output, NULL, name, value);
            }
        }
    }
    
    return result_none();
}

// pops - print out all procedure definitions (not buried)
static Result prim_pops(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    int count = proc_count(true);  // Get ALL, filter by buried in loop
    for (int i = 0; i < count; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            format_procedure_definition(ws_output, NULL, proc);
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
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        UserProcedure *proc = proc_find(name);
        if (proc == NULL)
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        format_procedure_title(ws_output, NULL, proc);
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
                format_procedure_title(ws_output, NULL, proc);
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
    
    int count = proc_count(true);  // Get ALL procedures, filter by buried in loop
    for (int i = 0; i < count; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            format_procedure_title(ws_output, NULL, proc);
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
    proc_reset_execution_state();
    
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
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    // Mark all roots: variables, procedure bodies, and property lists
    var_gc_mark_all();
    proc_gc_mark_all();
    prop_gc_mark_all();
    
    // Sweep unmarked nodes
    mem_gc_sweep();
    
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
    primitive_register("recycle", 0, prim_recycle);
}
