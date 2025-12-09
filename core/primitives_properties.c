//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Property list primitives: pprop, gprop, plist, remprop, pps, erprops
//

#include "primitives.h"
#include "properties.h"
#include "memory.h"
#include "error.h"
#include "eval.h"
#include "devices/device.h"
#include <stdio.h>
#include <string.h>

// Device write helper
static LogoDevice *prop_device = NULL;

void primitives_properties_set_device(LogoDevice *device)
{
    prop_device = device;
}

static void prop_print(const char *str)
{
    if (prop_device)
    {
        logo_device_write(prop_device, str);
    }
}

static void prop_newline(void)
{
    prop_print("\n");
}

// Print a value (word, list, or number)
static void print_value(Value value)
{
    char buf[64];
    
    switch (value.type)
    {
    case VALUE_NUMBER:
        snprintf(buf, sizeof(buf), "%g", value.as.number);
        prop_print(buf);
        break;
    case VALUE_WORD:
        prop_print(mem_word_ptr(value.as.node));
        break;
    case VALUE_LIST:
        prop_print("[");
        {
            bool first = true;
            Node curr = value.as.node;
            while (!mem_is_nil(curr))
            {
                if (!first)
                    prop_print(" ");
                first = false;
                
                Node elem = mem_car(curr);
                if (mem_is_word(elem))
                {
                    prop_print(mem_word_ptr(elem));
                }
                else if (mem_is_list(elem))
                {
                    // Nested list - simplified printing
                    prop_print("[...]");
                }
                curr = mem_cdr(curr);
            }
        }
        prop_print("]");
        break;
    default:
        break;
    }
}

// pprop name property object
// Puts a property value on a name's property list
static Result prim_pprop(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 3)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "pprop", NULL);
    }
    
    // First argument must be a word (the name)
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "pprop", value_to_string(args[0]));
    }
    
    // Second argument must be a word (the property name)
    if (!value_is_word(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "pprop", value_to_string(args[1]));
    }
    
    const char *name = mem_word_ptr(args[0].as.node);
    const char *property = mem_word_ptr(args[1].as.node);
    
    prop_put(name, property, args[2]);
    
    return result_none();
}

// gprop name property
// Gets a property value from a name's property list
// Returns empty list if not found
static Result prim_gprop(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 2)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "gprop", NULL);
    }
    
    // First argument must be a word (the name)
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "gprop", value_to_string(args[0]));
    }
    
    // Second argument must be a word (the property name)
    if (!value_is_word(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "gprop", value_to_string(args[1]));
    }
    
    const char *name = mem_word_ptr(args[0].as.node);
    const char *property = mem_word_ptr(args[1].as.node);
    
    Value out;
    prop_get(name, property, &out);
    
    return result_ok(out);
}

// plist name
// Returns the entire property list for a name as [prop1 val1 prop2 val2 ...]
static Result prim_plist(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "plist", NULL);
    }
    
    // Argument must be a word (the name)
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "plist", value_to_string(args[0]));
    }
    
    const char *name = mem_word_ptr(args[0].as.node);
    
    Node list = prop_get_list(name);
    
    return result_ok(value_list(list));
}

// remprop name property
// Removes a property from a name's property list
static Result prim_remprop(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 2)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "remprop", NULL);
    }
    
    // First argument must be a word (the name)
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "remprop", value_to_string(args[0]));
    }
    
    // Second argument must be a word (the property name)
    if (!value_is_word(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "remprop", value_to_string(args[1]));
    }
    
    const char *name = mem_word_ptr(args[0].as.node);
    const char *property = mem_word_ptr(args[1].as.node);
    
    prop_remove(name, property);
    
    return result_none();
}

// pps - print all property lists
static Result prim_pps(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    int count = prop_name_count();
    for (int i = 0; i < count; i++)
    {
        const char *name;
        if (prop_get_name_by_index(i, &name))
        {
            // Print: plist "name [prop1 val1 prop2 val2 ...]
            prop_print("plist \"");
            prop_print(name);
            prop_print(" ");
            
            Node list = prop_get_list(name);
            prop_print("[");
            bool first = true;
            Node curr = list;
            while (!mem_is_nil(curr))
            {
                if (!first)
                    prop_print(" ");
                first = false;
                
                Node elem = mem_car(curr);
                if (mem_is_word(elem))
                {
                    prop_print(mem_word_ptr(elem));
                }
                else if (mem_is_list(elem))
                {
                    // Print nested list
                    prop_print("[");
                    bool inner_first = true;
                    Node inner = elem;
                    while (!mem_is_nil(inner))
                    {
                        if (!inner_first)
                            prop_print(" ");
                        inner_first = false;
                        Node inner_elem = mem_car(inner);
                        if (mem_is_word(inner_elem))
                        {
                            prop_print(mem_word_ptr(inner_elem));
                        }
                        inner = mem_cdr(inner);
                    }
                    prop_print("]");
                }
                curr = mem_cdr(curr);
            }
            prop_print("]");
            prop_newline();
        }
    }
    
    return result_none();
}

// erprops - erase all properties
static Result prim_erprops(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    prop_erase_all();
    
    return result_none();
}

void primitives_properties_init(void)
{
    primitive_register("pprop", 3, prim_pprop);
    primitive_register("gprop", 2, prim_gprop);
    primitive_register("plist", 1, prim_plist);
    primitive_register("remprop", 2, prim_remprop);
    primitive_register("pps", 0, prim_pps);
    primitive_register("erprops", 0, prim_erprops);
}
