//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Variable primitives: make, thing, local, name, namep
//

#include "primitives.h"
#include "error.h"
#include "variables.h"
#include "eval.h"

static Result prim_make(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD_STR(args[0], name);
    var_set(name, args[1]);
    return result_none();
}

static Result prim_thing(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD_STR(args[0], name);
    Value v;
    if (!var_get(name, &v))
    {
        // Use error_arg to store the variable name
        return result_error_arg(ERR_NO_VALUE, NULL, name);
    }
    return result_ok(v);
}

// local "name or local [name1 name2 ...]
// Declares variable(s) as local to the current procedure
static Result prim_local(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    if (value_is_word(args[0]))
    {
        // Single name
        const char *name = mem_word_ptr(args[0].as.node);
        var_declare_local(name);
    }
    else if (value_is_list(args[0]))
    {
        // List of names
        Node node = args[0].as.node;
        while (!mem_is_nil(node))
        {
            Node element = mem_car(node);
            if (mem_is_word(element))
            {
                const char *name = mem_word_ptr(element);
                var_declare_local(name);
            }
            node = mem_cdr(node);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    return result_none();
}

// name "value "varname - same as make but with reversed arguments
static Result prim_name(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD_STR(args[1], name);
    var_set(name, args[0]);
    return result_none();
}

// namep "name - outputs true if name has a value
static Result prim_namep(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD_STR(args[0], name);
    
    if (var_exists(name))
    {
        return result_ok(value_word(mem_atom("true", 4)));
    }
    return result_ok(value_word(mem_atom("false", 5)));
}

void primitives_variables_init(void)
{
    primitive_register("make", 2, prim_make);
    primitive_register("thing", 1, prim_thing);
    primitive_register("local", 1, prim_local);
    primitive_register("name", 2, prim_name);
    primitive_register("name?", 1, prim_namep);
    primitive_register("namep", 1, prim_namep);
}
