//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Control flow primitives: run, repeat, stop, output, if, test, iftrue, iffalse
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "value.h"
#include "variables.h"
#include <strings.h>  // for strcasecmp

static Result prim_run(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
    if (!value_is_list(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "run", value_to_string(args[0]));
    }
    return eval_run_list(eval, args[0].as.node);
}

static Result prim_repeat(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
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

// if predicate list1 [list2]
// If predicate is true, run list1
// If predicate is false and list2 provided, run list2
static Result prim_if(Evaluator *eval, int argc, Value *args)
{
    // Check predicate is a boolean word
    Value pred = args[0];
    bool condition;
    
    if (value_is_word(pred))
    {
        const char *str = value_to_string(pred);
        if (strcasecmp(str, "true") == 0)
            condition = true;
        else if (strcasecmp(str, "false") == 0)
            condition = false;
        else
            return result_error_arg(ERR_NOT_BOOL, "if", str);
    }
    else
    {
        return result_error_arg(ERR_NOT_BOOL, "if", value_to_string(pred));
    }
    
    // Check that list1 is a list
    if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "if", value_to_string(args[1]));
    }
    
    if (condition)
    {
        return eval_run_list(eval, args[1].as.node);
    }
    else if (argc >= 3)
    {
        // Check that list2 is a list
        if (!value_is_list(args[2]))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, "if", value_to_string(args[2]));
        }
        return eval_run_list(eval, args[2].as.node);
    }
    
    return result_none();
}

// test predicate
// Evaluates predicate and remembers the result for iftrue/iffalse
static Result prim_test(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value pred = args[0];
    bool condition;
    
    if (value_is_word(pred))
    {
        const char *str = value_to_string(pred);
        if (strcasecmp(str, "true") == 0)
            condition = true;
        else if (strcasecmp(str, "false") == 0)
            condition = false;
        else
            return result_error_arg(ERR_NOT_BOOL, "test", str);
    }
    else
    {
        return result_error_arg(ERR_NOT_BOOL, "test", value_to_string(pred));
    }
    
    // Store the test result in the current scope
    var_set_test_result(condition);
    return result_none();
}

// iftrue list (ift list)
// Runs list if the most recent test was true
static Result prim_iftrue(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
    
    // Check that argument is a list
    if (!value_is_list(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "iftrue", value_to_string(args[0]));
    }
    
    // Get test result from scope
    bool test_result;
    if (!var_get_test_result(&test_result))
    {
        // No test has been run - do nothing (per spec)
        return result_none();
    }
    
    // Run the list if test was true
    if (test_result)
    {
        return eval_run_list(eval, args[0].as.node);
    }
    
    return result_none();
}

// iffalse list (iff list)
// Runs list if the most recent test was false
static Result prim_iffalse(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
    
    // Check that argument is a list
    if (!value_is_list(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "iffalse", value_to_string(args[0]));
    }
    
    // Get test result from scope
    bool test_result;
    if (!var_get_test_result(&test_result))
    {
        // No test has been run - do nothing (per spec)
        return result_none();
    }
    
    // Run the list if test was false
    if (!test_result)
    {
        return eval_run_list(eval, args[0].as.node);
    }
    
    return result_none();
}

void primitives_control_init(void)
{
    primitive_register("run", 1, prim_run);
    primitive_register("repeat", 2, prim_repeat);
    primitive_register("stop", 0, prim_stop);
    primitive_register("output", 1, prim_output);
    primitive_register("op", 1, prim_output); // Abbreviation
    primitive_register("if", 2, prim_if);
    primitive_register("test", 1, prim_test);
    primitive_register("iftrue", 1, prim_iftrue);
    primitive_register("ift", 1, prim_iftrue); // Abbreviation
    primitive_register("iffalse", 1, prim_iffalse);
    primitive_register("iff", 1, prim_iffalse); // Abbreviation
}
