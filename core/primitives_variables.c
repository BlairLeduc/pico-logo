//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Variable primitives: make, thing
//

#include "primitives.h"
#include "variables.h"
#include "eval.h"

#define ERR_HAS_NO_VALUE 36
#define ERR_DOESNT_LIKE_INPUT 41

static Result prim_make(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    if (!value_is_word(args[0]))
    {
        return result_error(ERR_DOESNT_LIKE_INPUT);
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
        return result_error(ERR_DOESNT_LIKE_INPUT);
    }
    const char *name = mem_word_ptr(args[0].as.node);
    Value v;
    if (!var_get(name, &v))
    {
        return result_error(ERR_HAS_NO_VALUE);
    }
    return result_ok(v);
}

void primitives_variables_init(void)
{
    primitive_register("make", 2, prim_make);
    primitive_register("thing", 1, prim_thing);
}
