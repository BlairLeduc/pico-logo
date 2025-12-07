//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Control flow primitives: run, repeat, stop, output
//

#include "primitives.h"
#include "eval.h"

#define ERR_DOESNT_LIKE_INPUT 41

static Result prim_run(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
    if (!value_is_list(args[0]))
    {
        return result_error(ERR_DOESNT_LIKE_INPUT);
    }
    return eval_run_list(eval, args[0].as.node);
}

static Result prim_repeat(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
    float count_f;
    if (!value_to_number(args[0], &count_f))
    {
        return result_error(ERR_DOESNT_LIKE_INPUT);
    }
    if (!value_is_list(args[1]))
    {
        return result_error(ERR_DOESNT_LIKE_INPUT);
    }

    int count = (int)count_f;
    Node body = args[1].as.node;

    for (int i = 0; i < count; i++)
    {
        Result r = eval_run_list(eval, body);
        // Propagate stop/output/error
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            return r;
        }
    }
    return result_none();
}

static Result prim_stop(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    return result_stop();
}

static Result prim_output(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    return result_output(args[0]);
}

void primitives_control_init(void)
{
    primitive_register("run", 1, prim_run);
    primitive_register("repeat", 2, prim_repeat);
    primitive_register("stop", 0, prim_stop);
    primitive_register("output", 1, prim_output);
    primitive_register("op", 1, prim_output); // Abbreviation
}
