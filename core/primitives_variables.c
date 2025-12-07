//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Variable primitives: make, thing
//

#include "primitives.h"
#include "error.h"
#include "variables.h"
#include "eval.h"

static Result prim_make(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "make", value_to_string(args[0]));
    }
    const char *name = mem_word_ptr(args[0].as.node);
    var_set(name, args[1]);
    return result_none();
}

static Result prim_thing(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "thing", value_to_string(args[0]));
    }
    const char *name = mem_word_ptr(args[0].as.node);
    Value v;
    if (!var_get(name, &v))
    {
        // Use error_arg to store the variable name
        return result_error_arg(ERR_NO_VALUE, NULL, name);
    }
    return result_ok(v);
}

void primitives_variables_init(void)
{
    primitive_register("make", 2, prim_make);
    primitive_register("thing", 1, prim_thing);
}
