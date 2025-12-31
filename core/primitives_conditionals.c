//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Conditional primitives: if, true, false, test, iftrue, iffalse
//

#include "primitives.h"
#include "eval.h"
#include "memory.h"
#include "value.h"
#include "variables.h"
#include <strings.h>  // for strcasecmp

//==========================================================================
// IF Command/Operation
//==========================================================================

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
            return result_error_arg(ERR_NOT_BOOL, NULL, str);
    }
    else
    {
        return result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(pred));
    }
    
    // Check that list1 is a list
    if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    
    if (condition)
    {
        // Use eval_run_list_expr so if can act as an operation
        return eval_run_list_expr(eval, args[1].as.node);
    }
    else if (argc >= 3)
    {
        // Check that list2 is a list
        if (!value_is_list(args[2]))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[2]));
        }
        // Use eval_run_list_expr so if can act as an operation
        return eval_run_list_expr(eval, args[2].as.node);
    }
    
    return result_none();
}

//==========================================================================
// Boolean Operations
//==========================================================================

static Result prim_true(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    Node true_word = mem_atom_cstr("true");
    return result_ok(value_word(true_word));
}

static Result prim_false(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    Node false_word = mem_atom_cstr("false");
    return result_ok(value_word(false_word));
}

//==========================================================================
// Test/Conditional Flow
//==========================================================================

static Result prim_test(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    Value pred = args[0];
    
    if (value_is_word(pred))
    {
        const char *str = value_to_string(pred);
        if (strcasecmp(str, "true") == 0)
        {
            var_set_test(true);
        }
        else if (strcasecmp(str, "false") == 0)
        {
            var_set_test(false);
        }
        else
        {
            return result_error_arg(ERR_NOT_BOOL, NULL, str);
        }
    }
    else
    {
        return result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(pred));
    }
    
    return result_none();
}

static Result prim_iftrue(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    
    bool test_value;
    if (var_get_test(&test_value) && test_value)
    {
        return eval_run_list(eval, args[0].as.node);
    }
    
    return result_none();
}

static Result prim_iffalse(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_LIST(args[0]);
    
    bool test_value;
    if (var_get_test(&test_value) && !test_value)
    {
        return eval_run_list(eval, args[0].as.node);
    }
    
    return result_none();
}

void primitives_conditionals_init(void)
{
    primitive_register("if", 2, prim_if);
    
    // Boolean operations
    primitive_register("true", 0, prim_true);
    primitive_register("false", 0, prim_false);
    
    // Test/conditional flow
    primitive_register("test", 1, prim_test);
    primitive_register("iftrue", 1, prim_iftrue);
    primitive_register("ift", 1, prim_iftrue); // Abbreviation
    primitive_register("iffalse", 1, prim_iffalse);
    primitive_register("iff", 1, prim_iffalse); // Abbreviation
}
