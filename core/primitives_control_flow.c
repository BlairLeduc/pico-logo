//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Control flow primitives: run, repeat, repcount, stop, output, while, do.while, for
//

#include "primitives.h"
#include "eval.h"
#include "value.h"
#include "variables.h"
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

// Helper to evaluate a word or list to get a number
// If the value is a word that looks like a number, use it directly
// If the value is a list, run it and expect a number output
static Result eval_to_number(Evaluator *eval, Value v, float *out)
{
    // First, try to interpret directly as a number
    if (value_to_number(v, out))
    {
        return result_none();
    }
    
    // If it's a list, run it to get a number
    if (value_is_list(v))
    {
        Result r = eval_run_list_expr(eval, v.as.node);
        if (r.status == RESULT_ERROR)
        {
            return r;
        }
        if (r.status != RESULT_OK)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
        }
        if (!value_to_number(r.value, out))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(r.value));
        }
        return result_none();
    }
    
    // Not a number and not a list
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
}

// for forcontrol instructionlist
// forcontrol is [varname start limit] or [varname start limit step]
// Runs instructionlist repeatedly with varname set to start, start+step, ...
// Loop ends when (current - limit) has same sign as step
static Result prim_for(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    Node forcontrol = args[0].as.node;
    Node body = args[1].as.node;
    
    // Parse forcontrol: [varname start limit] or [varname start limit step]
    // Count elements
    int count = 0;
    Node temp = forcontrol;
    while (!mem_is_nil(temp))
    {
        count++;
        temp = mem_cdr(temp);
    }
    
    if (count < 3 || count > 4)
    {
        return result_error_arg(ERR_TOO_FEW_ITEMS_LIST, NULL, value_to_string(args[0]));
    }
    
    // Extract elements
    Node elem1 = mem_car(forcontrol);
    Node rest1 = mem_cdr(forcontrol);
    Node elem2 = mem_car(rest1);
    Node rest2 = mem_cdr(rest1);
    Node elem3 = mem_car(rest2);
    Node rest3 = mem_cdr(rest2);
    
    // First element must be a word (variable name)
    if (!mem_is_word(elem1))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    const char *varname = mem_word_ptr(elem1);
    
    // Evaluate start value
    Value start_val = mem_is_word(elem2) ? value_word(elem2) : value_list(elem2);
    float start;
    Result r = eval_to_number(eval, start_val, &start);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }
    
    // Evaluate limit value
    Value limit_val = mem_is_word(elem3) ? value_word(elem3) : value_list(elem3);
    float limit;
    r = eval_to_number(eval, limit_val, &limit);
    if (r.status == RESULT_ERROR)
    {
        return r;
    }
    
    // Evaluate step (optional)
    float step;
    bool has_explicit_step = !mem_is_nil(rest3);
    if (has_explicit_step)
    {
        Node elem4 = mem_car(rest3);
        Value step_val = mem_is_word(elem4) ? value_word(elem4) : value_list(elem4);
        r = eval_to_number(eval, step_val, &step);
        if (r.status == RESULT_ERROR)
        {
            return r;
        }
    }
    else
    {
        // Default step: 1 or -1 depending on direction
        step = (limit >= start) ? 1.0f : -1.0f;
    }
    
    // Save original variable state for restoration after the loop
    // This implements the "local to the for loop" semantics
    Value saved_value;
    bool had_value = var_get(varname, &saved_value);
    
    // Declare the variable as local if we're in a procedure scope
    // This allows var_set to update the local binding
    if (eval_in_procedure(eval))
    {
        if (!var_declare_local(varname))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
    }
    
    // Loop
    float current = start;
    Result loop_result = result_none();
    while (true)
    {
        // Check termination condition: (current - limit) has same sign as step
        // This means we've gone past the limit
        float diff = current - limit;
        
        // If step is positive, we stop when current > limit (diff > 0)
        // If step is negative, we stop when current < limit (diff < 0)
        // So we stop when diff * step > 0 (same sign)
        if (diff * step > 0)
        {
            break;
        }
        
        // Set the control variable
        if (!var_set(varname, value_number(current)))
        {
            loop_result = result_error(ERR_OUT_OF_SPACE);
            break;
        }
        
        // Run the body
        r = eval_run_list(eval, body);
        // Propagate stop/output/error/throw
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            loop_result = r;
            break;
        }
        
        // Increment
        current += step;
    }
    
    // Restore the original variable state (only if not in procedure scope)
    // In procedure scope, the frame system handles cleanup
    if (!eval_in_procedure(eval))
    {
        if (had_value)
        {
            var_set(varname, saved_value);
        }
        else
        {
            var_erase(varname);
        }
    }
    
    return loop_result;
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
    primitive_register("for", 2, prim_for);
}
