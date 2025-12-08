//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Workspace management primitives: po, poall, pon, pons, pops, pot, pots
//

#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "error.h"
#include "eval.h"
#include "devices/device.h"
#include <stdio.h>
#include <string.h>

// External device access (set by primitives_output.c)
extern void primitives_set_device(LogoDevice *device);

// Device write helper - uses the same device as print
static LogoDevice *ws_device = NULL;

void primitives_workspace_set_device(LogoDevice *device)
{
    ws_device = device;
}

static void ws_print(const char *str)
{
    if (ws_device)
    {
        logo_device_write(ws_device, str);
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

// poall - print all procedures and variables (not buried)
static Result prim_poall(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    // Print all procedures (not buried)
    int count = proc_count(false);
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
    
    int count = proc_count(false);
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
    
    int count = proc_count(false);
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

void primitives_workspace_init(void)
{
    primitive_register("po", 1, prim_po);
    primitive_register("poall", 0, prim_poall);
    primitive_register("pon", 1, prim_pon);
    primitive_register("pons", 0, prim_pons);
    primitive_register("pops", 0, prim_pops);
    primitive_register("pot", 1, prim_pot);
    primitive_register("pots", 0, prim_pots);
}
