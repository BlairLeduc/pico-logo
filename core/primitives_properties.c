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
#include "devices/io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void prop_print(const char *str)
{
    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_write(io, str);
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
        format_number(buf, sizeof(buf), value.as.number);
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
    UNUSED(eval);
    REQUIRE_ARGC("pprop", 3);
    REQUIRE_WORD("pprop", args[0]);
    REQUIRE_WORD("pprop", args[1]);
    
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
    UNUSED(eval);
    REQUIRE_ARGC("gprop", 2);
    REQUIRE_WORD("gprop", args[0]);
    REQUIRE_WORD("gprop", args[1]);
    
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
    UNUSED(eval);
    REQUIRE_ARGC("plist", 1);
    REQUIRE_WORD("plist", args[0]);
    
    const char *name = mem_word_ptr(args[0].as.node);
    
    Node list = prop_get_list(name);
    
    return result_ok(value_list(list));
}

// remprop name property
// Removes a property from a name's property list
static Result prim_remprop(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("remprop", 2);
    REQUIRE_WORD("remprop", args[0]);
    REQUIRE_WORD("remprop", args[1]);
    
    const char *name = mem_word_ptr(args[0].as.node);
    const char *property = mem_word_ptr(args[1].as.node);
    
    prop_remove(name, property);
    
    return result_none();
}

// pps - print all property lists as pprop commands
static Result prim_pps(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    int count = prop_name_count();
    for (int i = 0; i < count; i++)
    {
        const char *name;
        if (prop_get_name_by_index(i, &name))
        {
            // Get property list: [prop1 val1 prop2 val2 ...]
            Node list = prop_get_list(name);
            Node curr = list;
            
            // Iterate through pairs
            while (!mem_is_nil(curr))
            {
                Node prop_node = mem_car(curr);
                curr = mem_cdr(curr);
                if (mem_is_nil(curr))
                    break;
                Node val_node = mem_car(curr);
                curr = mem_cdr(curr);
                
                // Print: pprop "name "property value
                prop_print("pprop \"");
                prop_print(name);
                prop_print(" \"");
                if (mem_is_word(prop_node))
                {
                    prop_print(mem_word_ptr(prop_node));
                }
                prop_print(" ");
                
                // Print value (could be word, number, or list)
                if (mem_is_word(val_node))
                {
                    // Check if it's a number
                    const char *str = mem_word_ptr(val_node);
                    char *endptr;
                    strtof(str, &endptr);
                    if (*endptr == '\0' && str[0] != '\0')
                    {
                        // It's a number, print without quote
                        prop_print(str);
                    }
                    else
                    {
                        // It's a word, print with quote if needed
                        prop_print(str);
                    }
                }
                else if (mem_is_list(val_node))
                {
                    // Print list with brackets
                    prop_print("[");
                    bool first = true;
                    Node inner = val_node;
                    while (!mem_is_nil(inner))
                    {
                        if (!first)
                            prop_print(" ");
                        first = false;
                        Node elem = mem_car(inner);
                        if (mem_is_word(elem))
                        {
                            prop_print(mem_word_ptr(elem));
                        }
                        else if (mem_is_list(elem))
                        {
                            // Nested list - recursive print would be better
                            // but keep it simple for now
                            prop_print("[...]");
                        }
                        inner = mem_cdr(inner);
                    }
                    prop_print("]");
                }
                prop_newline();
            }
        }
    }
    
    return result_none();
}

// erprops - erase all properties
static Result prim_erprops(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
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
