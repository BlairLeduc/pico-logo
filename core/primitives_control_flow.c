//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Control flow primitives: run, repeat, repcount, stop, output, while, do.while
//

#include "primitives.h"
#include "eval.h"
#include "value.h"
#include <strings.h>  // for strcasecmp

// run list - runs the provided list as Logo code
static Result prim_run(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);

    // Use eval_run_list_expr so run can act as an operation
    return eval_run_list_expr(eval, args[0].as.node);
}

// forever - repeats the provided list indefinitely
static Result prim_forever(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);

    Node body = args[0].as.node;
    int previous_repcount = eval->repcount;
    int iteration = 0;
    while (true)
    {
        eval->repcount = iteration + 1;  // repcount is 1-based
        Result r = eval_run_list(eval, body);
        iteration++;
        // Propagate stop/output/error/throw
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            eval->repcount = previous_repcount;  // Restore previous repcount
            return r;
        }
    }

    // Unreachable, but for completeness
    eval->repcount = previous_repcount;  // Restore previous repcount
    return result_none();
}

// repeat count list - repeats the provided list count times
static Result prim_repeat(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_NUMBER(args[0], count_f);
    REQUIRE_LIST(args[1]);

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

// ignore - does nothing
static Result prim_ignore(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    return result_none();
}

// ; (comment) - ignores its input (a word or list)
static Result prim_comment(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    return result_none();
}

// do.while list predicate_list - runs list repeatedly as long as predicate_list evaluates to true
// list is always run at least once
static Result prim_do_while(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    Node body = args[0].as.node;
    Node predicate_list = args[1].as.node;
    
    do
    {
        // Run the body
        Result r = eval_run_list(eval, body);
        // Propagate stop/output/error/throw
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            return r;
        }
        
        // Evaluate the predicate list to get a boolean
        Result pred_result = eval_run_list_expr(eval, predicate_list);
        if (pred_result.status == RESULT_ERROR)
        {
            return pred_result;
        }
        if (pred_result.status != RESULT_OK)
        {
            return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
        }
        
        REQUIRE_BOOL(pred_result.value, condition);
        if (!condition)
        {
            break;
        }
    } while (true);
    
    return result_none();
}

// while predicate_list list - tests predicate_list and runs list if true, repeats until false
// list may not be run at all if predicate_list is initially false
static Result prim_while(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    Node predicate_list = args[0].as.node;
    Node body = args[1].as.node;
    
    while (true)
    {
        // Evaluate the predicate list to get a boolean
        Result pred_result = eval_run_list_expr(eval, predicate_list);
        if (pred_result.status == RESULT_ERROR)
        {
            return pred_result;
        }
        if (pred_result.status != RESULT_OK)
        {
            return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
        }
        
        REQUIRE_BOOL(pred_result.value, condition);
        if (!condition)
        {
            break;
        }
        
        // Run the body
        Result r = eval_run_list(eval, body);
        // Propagate stop/output/error/throw
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            return r;
        }
    }
    
    return result_none();
}

// do.until list predicate_list - runs list repeatedly until predicate_list evaluates to true
// list is always run at least once
static Result prim_do_until(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    Node body = args[0].as.node;
    Node predicate_list = args[1].as.node;
    
    do
    {
        // Run the body
        Result r = eval_run_list(eval, body);
        // Propagate stop/output/error/throw
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            return r;
        }
        
        // Evaluate the predicate list to get a boolean
        Result pred_result = eval_run_list_expr(eval, predicate_list);
        if (pred_result.status == RESULT_ERROR)
        {
            return pred_result;
        }
        if (pred_result.status != RESULT_OK)
        {
            return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
        }
        
        REQUIRE_BOOL(pred_result.value, condition);
        if (condition)
        {
            break;
        }
    } while (true);
    
    return result_none();
}

// until predicate_list list - tests predicate_list and runs list if false, repeats until true
// list may not be run at all if predicate_list is initially true
static Result prim_until(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    Node predicate_list = args[0].as.node;
    Node body = args[1].as.node;
    
    while (true)
    {
        // Evaluate the predicate list to get a boolean
        Result pred_result = eval_run_list_expr(eval, predicate_list);
        if (pred_result.status == RESULT_ERROR)
        {
            return pred_result;
        }
        if (pred_result.status != RESULT_OK)
        {
            return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
        }
        
        REQUIRE_BOOL(pred_result.value, condition);
        if (condition)
        {
            break;
        }
        
        // Run the body
        Result r = eval_run_list(eval, body);
        // Propagate stop/output/error/throw
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            return r;
        }
    }
    
    return result_none();
}


void primitives_control_flow_init(void)
{
    primitive_register("run", 1, prim_run);
    primitive_register("forever", 1, prim_forever);
    primitive_register("repeat", 2, prim_repeat);
    primitive_register("repcount", 0, prim_repcount);
    primitive_register("stop", 0, prim_stop);
    primitive_register("output", 1, prim_output);
    primitive_register("op", 1, prim_output); // Abbreviation
    primitive_register("ignore", 1, prim_ignore);
    primitive_register(";", 1, prim_comment);
    primitive_register("do.while", 2, prim_do_while);
    primitive_register("while", 2, prim_while);
    primitive_register("do.until", 2, prim_do_until);
    primitive_register("until", 2, prim_until);
}
