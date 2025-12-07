//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Output primitives: print
//

#include "primitives.h"
#include "eval.h"
#include "devices/device.h"
#include <stdio.h>
#include <string.h>

// Forward declaration for the device - will be set by REPL
static LogoDevice *current_device = NULL;

void primitives_set_device(LogoDevice *device)
{
    current_device = device;
}

static void print_to_device(const char *str)
{
    if (current_device)
    {
        logo_device_write(current_device, str);
    }
}

static void print_list_contents(Node node)
{
    bool first = true;
    while (!mem_is_nil(node))
    {
        if (!first)
            print_to_device(" ");
        first = false;

        Node element = mem_car(node);
        if (mem_is_word(element))
        {
            print_to_device(mem_word_ptr(element));
        }
        else if (mem_is_list(element))
        {
            print_to_device("[");
            print_list_contents(element);
            print_to_device("]");
        }
        node = mem_cdr(node);
    }
}

static void print_value(Value v)
{
    char buf[32];
    switch (v.type)
    {
    case VALUE_NONE:
        break;
    case VALUE_NUMBER:
        snprintf(buf, sizeof(buf), "%g", v.as.number);
        print_to_device(buf);
        break;
    case VALUE_WORD:
        print_to_device(mem_word_ptr(v.as.node));
        break;
    case VALUE_LIST:
        print_list_contents(v.as.node);
        break;
    }
}

static Result prim_print(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    for (int i = 0; i < argc; i++)
    {
        if (i > 0)
            print_to_device(" ");
        print_value(args[i]);
    }
    print_to_device("\n");
    return result_none();
}

void primitives_output_init(void)
{
    primitive_register("print", 1, prim_print);
    primitive_register("pr", 1, prim_print); // Abbreviation
}
