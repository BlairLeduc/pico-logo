//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Control flow primitives: run, repeat, repcount, stop, output
//

#include "primitives.h"
#include "eval.h"
#include "value.h"

static Result prim_run(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST("run", args[0]);
    // Use eval_run_list_expr so run can act as an operation
    return eval_run_list_expr(eval, args[0].as.node);
}

static Result prim_repeat(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    float count_f;
    if (!value_to_number(args[0], &count_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "repeat", value_to_string(args[0]));
    }
    if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "repeat", value_to_string(args[1]));
    }

    int count = (int)count_f;
    Node body = args[1].as.node;

    int previous_repcount = eval->repcount;
    for (int i = 0; i < count; i++)
    {
        eval->repcount = i + 1;  // repcount is 1-based
        Result r = eval_run_list(eval, body);
        // Propagate stop/output/error/throw
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            eval->repcount = previous_repcount;  // Restore previous repcount
            return r;
        }
    }
    eval->repcount = previous_repcount;  // Restore previous repcount
    return result_none();
}

static Result prim_repcount(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc); UNUSED(args);
    
    // Return the innermost repeat count (1-based)
    return result_ok(value_number(eval->repcount));
}

static Result prim_stop(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    return result_stop();
}

static Result prim_output(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    return result_output(args[0]);
}

void primitives_control_flow_init(void)
{
    primitive_register("run", 1, prim_run);
    primitive_register("repeat", 2, prim_repeat);
    primitive_register("repcount", 0, prim_repcount);
    primitive_register("stop", 0, prim_stop);
    primitive_register("output", 1, prim_output);
    primitive_register("op", 1, prim_output); // Abbreviation
}
