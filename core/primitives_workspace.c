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
#include "devices/device.h"
#include <stdio.h>
#include <string.h>

static void ws_print(const char *str)
{
    LogoDevice *device = primitives_get_device();
    if (device)
    {
        logo_device_write(device, str);
    }
}

static void ws_newline(void)
{
    ws_print("\n");
}

// Print a procedure body element (handles quoted words, etc.)
static void print_body_element(Node elem)
{
    if (mem_is_word(elem))
    {
        ws_print(mem_word_ptr(elem));
    }
    else if (mem_is_list(elem))
    {
        ws_print("[");
        bool first = true;
        Node curr = elem;
        while (!mem_is_nil(curr))
        {
            if (!first)
                ws_print(" ");
            first = false;
            print_body_element(mem_car(curr));
            curr = mem_cdr(curr);
        }
        ws_print("]");
    }
}

// Print procedure title line: to name :param1 :param2 ...
static void print_procedure_title(UserProcedure *proc)
{
    ws_print("to ");
    ws_print(proc->name);
    for (int i = 0; i < proc->param_count; i++)
    {
        ws_print(" :");
        ws_print(proc->params[i]);
    }
    ws_newline();
}

// Print full procedure definition
static void print_procedure_definition(UserProcedure *proc)
{
    print_procedure_title(proc);
    
    // Print body
    Node curr = proc->body;
    while (!mem_is_nil(curr))
    {
        Node elem = mem_car(curr);
        print_body_element(elem);
        
        // Add space between elements, newline at certain points (simple approach)
        Node next = mem_cdr(curr);
        if (!mem_is_nil(next))
        {
            ws_print(" ");
        }
        curr = next;
    }
    ws_newline();
    ws_print("end");
    ws_newline();
}

// Print a variable and its value
static void print_variable(const char *name, Value value)
{
    char buf[64];
    ws_print("make \"");
    ws_print(name);
    ws_print(" ");
    
    switch (value.type)
    {
    case VALUE_NUMBER:
        snprintf(buf, sizeof(buf), "%g", value.as.number);
        ws_print(buf);
        break;
    case VALUE_WORD:
        ws_print("\"");
        ws_print(mem_word_ptr(value.as.node));
        break;
    case VALUE_LIST:
        ws_print("[");
        {
            bool first = true;
            Node curr = value.as.node;
            while (!mem_is_nil(curr))
            {
                if (!first)
                    ws_print(" ");
                first = false;
                print_body_element(mem_car(curr));
                curr = mem_cdr(curr);
            }
        }
        ws_print("]");
        break;
    default:
        break;
    }
    ws_newline();
}

// po "name or po [name1 name2 ...]
// Print out procedure definition(s)
static Result prim_po(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "po", NULL);
    }
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        UserProcedure *proc = proc_find(name);
        if (proc == NULL)
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        print_procedure_definition(proc);
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
                print_procedure_definition(proc);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "po", value_to_string(args[0]));
    }
    
    return result_none();
}

// poall - print all procedures, variables, and properties (not buried)
static Result prim_poall(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    // Print all procedures (not buried)
    int count = proc_count(true);  // Get ALL, filter by buried in loop
    for (int i = 0; i < count; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            print_procedure_definition(proc);
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
            print_variable(name, value);
        }
    }
    
    // Print all property lists
    int prop_count = prop_name_count();
    for (int i = 0; i < prop_count; i++)
    {
        const char *name;
        if (prop_get_name_by_index(i, &name))
        {
            Node list = prop_get_list(name);
            if (!mem_is_nil(list))
            {
                ws_print("plist \"");
                ws_print(name);
                ws_print(" [");
                bool first = true;
                Node curr = list;
                while (!mem_is_nil(curr))
                {
                    if (!first)
                        ws_print(" ");
                    first = false;
                    
                    Node elem = mem_car(curr);
                    if (mem_is_word(elem))
                    {
                        ws_print(mem_word_ptr(elem));
                    }
                    else if (mem_is_list(elem))
                    {
                        ws_print("[...]");
                    }
                    curr = mem_cdr(curr);
                }
                ws_print("]");
                ws_newline();
            }
        }
    }
    
    return result_none();
}

// pon "name or pon [name1 name2 ...]
// Print out variable name(s) and value(s)
static Result prim_pon(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "pon", NULL);
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
        print_variable(name, value);
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
                print_variable(name, value);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "pon", value_to_string(args[0]));
    }
    
    return result_none();
}

// pons - print out all variable names and values (not buried)
static Result prim_pons(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    int count = var_global_count(false);
    for (int i = 0; i < count; i++)
    {
        const char *name;
        Value value;
        if (var_get_global_by_index(i, false, &name, &value))
        {
            print_variable(name, value);
        }
    }
    
    return result_none();
}

// pops - print out all procedure definitions (not buried)
static Result prim_pops(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    int count = proc_count(true);  // Get ALL, filter by buried in loop
    for (int i = 0; i < count; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            print_procedure_definition(proc);
            ws_newline();
        }
    }
    
    return result_none();
}

// pot "name or pot [name1 name2 ...]
// Print out procedure title(s)
static Result prim_pot(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "pot", NULL);
    }
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        UserProcedure *proc = proc_find(name);
        if (proc == NULL)
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        print_procedure_title(proc);
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
                print_procedure_title(proc);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "pot", value_to_string(args[0]));
    }
    
    return result_none();
}

// pots - print out all procedure titles (not buried)
static Result prim_pots(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    int count = proc_count(true);  // Get ALL procedures, filter by buried in loop
    for (int i = 0; i < count; i++)
    {
        UserProcedure *proc = proc_get_by_index(i);
        if (proc && !proc->buried)
        {
            print_procedure_title(proc);
        }
    }
    
    return result_none();
}

// bury "name or bury [name1 name2 ...]
// Bury procedure(s) - hidden from poall, pops, pots, erall, erps
static Result prim_bury(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "bury", NULL);
    }
    
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "bury", value_to_string(args[0]));
    }
    
    return result_none();
}

// buryall - bury all procedures and variable names
static Result prim_buryall(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    proc_bury_all();
    var_bury_all();
    
    return result_none();
}

// buryname "name or buryname [name1 name2 ...]
// Bury variable name(s)
static Result prim_buryname(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "buryname", NULL);
    }
    
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!var_exists(name))
        {
            return result_error_arg(ERR_NO_VALUE, name, NULL);
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
                    return result_error_arg(ERR_NO_VALUE, name, NULL);
                }
                var_bury(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "buryname", value_to_string(args[0]));
    }
    
    return result_none();
}

// unbury "name or unbury [name1 name2 ...]
// Unbury procedure(s)
static Result prim_unbury(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "unbury", NULL);
    }
    
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "unbury", value_to_string(args[0]));
    }
    
    return result_none();
}

// unburyall - unbury all procedures and variable names
static Result prim_unburyall(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    proc_unbury_all();
    var_unbury_all();
    
    return result_none();
}

// unburyname "name or unburyname [name1 name2 ...]
// Unbury variable name(s)
static Result prim_unburyname(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "unburyname", NULL);
    }
    
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!var_exists(name))
        {
            return result_error_arg(ERR_NO_VALUE, name, NULL);
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
                    return result_error_arg(ERR_NO_VALUE, name, NULL);
                }
                var_unbury(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "unburyname", value_to_string(args[0]));
    }
    
    return result_none();
}

// erall - erase all procedures, variables, and properties (respects buried)
static Result prim_erall(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    proc_erase_all(true);  // true = check buried flag
    var_erase_all_globals(true);
    prop_erase_all();
    
    return result_none();
}

// erase "name or erase [name1 name2 ...]
// Erase procedure(s) from workspace
static Result prim_erase(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "erase", NULL);
    }
    
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "erase", value_to_string(args[0]));
    }
    
    return result_none();
}

// ern "name or ern [name1 name2 ...]
// Erase variable name(s)
static Result prim_ern(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "ern", NULL);
    }
    
    if (value_is_word(args[0]))
    {
        const char *name = mem_word_ptr(args[0].as.node);
        if (!var_exists(name))
        {
            return result_error_arg(ERR_NO_VALUE, name, NULL);
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
                    return result_error_arg(ERR_NO_VALUE, name, NULL);
                }
                var_erase(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "ern", value_to_string(args[0]));
    }
    
    return result_none();
}

// erns - erase all variables (respects buried)
static Result prim_erns(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    var_erase_all_globals(true);  // true = check buried flag
    
    return result_none();
}

// erps - erase all procedures (respects buried)
static Result prim_erps(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    proc_erase_all(true);  // true = check buried flag
    
    return result_none();
}

// nodes
// Outputs the number of free nodes available
static Result prim_nodes(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    size_t free = mem_free_nodes();
    return result_ok(value_number((float)free));
}

// recycle
// Runs garbage collection to free up as many nodes as possible
static Result prim_recycle(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
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
