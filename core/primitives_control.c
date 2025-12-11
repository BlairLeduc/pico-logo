//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Control flow primitives: run, repeat, stop, output, if, true, false,
//  test, iftrue, iffalse, wait, pause, co, catch, throw, error, go, label
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "value.h"
#include <strings.h>  // for strcasecmp
#include <unistd.h>   // for usleep
#include <stdio.h>    // for snprintf

//==========================================================================
// Test state management (local to procedure scope)
//==========================================================================

// NOTE: Current implementation uses module-level state rather than
// procedure-local state. Per the Logo spec, test state should be local
// to the procedure in which it occurs and inherited by called procedures.
// A full implementation would require integration with the procedure call
// stack to maintain separate test state per procedure context.
// For now, this simpler global state works for most use cases.
static bool test_result_valid = false;
static bool test_result_value = false;

// Function to reset test state (for testing purposes)
void primitives_control_reset_test_state(void)
{
    test_result_valid = false;
    test_result_value = false;
}

//==========================================================================
// Error info storage for the error primitive
//==========================================================================

// NOTE: This structure is defined but not currently populated by the
// error handling system. A full implementation would require integration
// with catch/throw exception handling and would populate this when errors
// are caught. For now, the error primitive always returns an empty list.
typedef struct
{
    bool has_error;
    int error_code;
    const char *error_message;
    const char *error_proc;
    const char *error_caller;
} ErrorInfo;

static ErrorInfo last_error = {false, 0, NULL, NULL, NULL};

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

//==========================================================================
// Boolean Operations
//==========================================================================

static Result prim_true(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    Node true_word = mem_atom_cstr("true");
    return result_ok(value_word(true_word));
}

static Result prim_false(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    Node false_word = mem_atom_cstr("false");
    return result_ok(value_word(false_word));
}

//==========================================================================
// Test/Conditional Flow
//==========================================================================

static Result prim_test(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value pred = args[0];
    
    if (value_is_word(pred))
    {
        const char *str = value_to_string(pred);
        if (strcasecmp(str, "true") == 0)
        {
            test_result_valid = true;
            test_result_value = true;
        }
        else if (strcasecmp(str, "false") == 0)
        {
            test_result_valid = true;
            test_result_value = false;
        }
        else
        {
            return result_error_arg(ERR_NOT_BOOL, "test", str);
        }
    }
    else
    {
        return result_error_arg(ERR_NOT_BOOL, "test", value_to_string(pred));
    }
    
    return result_none();
}

static Result prim_iftrue(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
    
    if (!value_is_list(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "iftrue", value_to_string(args[0]));
    }
    
    if (test_result_valid && test_result_value)
    {
        return eval_run_list(eval, args[0].as.node);
    }
    
    return result_none();
}

static Result prim_iffalse(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
    
    if (!value_is_list(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "iffalse", value_to_string(args[0]));
    }
    
    if (test_result_valid && !test_result_value)
    {
        return eval_run_list(eval, args[0].as.node);
    }
    
    return result_none();
}

//==========================================================================
// Timing
//==========================================================================

static Result prim_wait(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    float tenths_f;
    if (!value_to_number(args[0], &tenths_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "wait", value_to_string(args[0]));
    }
    
    int tenths = (int)tenths_f;
    if (tenths < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "wait", value_to_string(args[0]));
    }
    
    // Wait for tenths of a second (each tenth is 100,000 microseconds)
    usleep(tenths * 100000);
    
    return result_none();
}

//==========================================================================
// Pause/Continue (stubs for now)
//==========================================================================

static Result prim_pause(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    // TODO: Implement pause functionality
    // For now, just return an error indicating it's not implemented
    return result_error(ERR_PAUSING);
}

static Result prim_co(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    // TODO: Implement continue functionality
    // For now, just do nothing
    return result_none();
}

//==========================================================================
// Exception Handling (stubs for now)
//==========================================================================

static Result prim_catch(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    // TODO: Implement catch functionality
    // For now, just run the list normally
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "catch", value_to_string(args[0]));
    }
    if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "catch", value_to_string(args[1]));
    }
    
    return eval_run_list(eval, args[1].as.node);
}

static Result prim_throw(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "throw", value_to_string(args[0]));
    }
    
    // TODO: Implement throw functionality
    // For now, return an error with the tag
    return result_error_arg(ERR_NO_CATCH, "throw", value_to_string(args[0]));
}

static Result prim_error(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    if (!last_error.has_error)
    {
        // Return empty list if no error
        return result_ok(value_list(NODE_NIL));
    }
    
    // Build a four-element list: [error-number message primitive-name procedure-name]
    // Convert error code to string
    char error_num_buf[16];
    snprintf(error_num_buf, sizeof(error_num_buf), "%d", last_error.error_code);
    Node error_num_word = mem_atom_cstr(error_num_buf);
    
    Node message_word = last_error.error_message ? mem_atom_cstr(last_error.error_message) : mem_atom_cstr("");
    Node proc_word = last_error.error_proc ? mem_atom_cstr(last_error.error_proc) : mem_atom_cstr("");
    Node caller_word = last_error.error_caller ? mem_atom_cstr(last_error.error_caller) : NODE_NIL;
    
    Node list = NODE_NIL;
    if (!mem_is_nil(caller_word))
    {
        list = mem_cons(caller_word, list);
    }
    else
    {
        list = mem_cons(NODE_NIL, list);
    }
    list = mem_cons(proc_word, list);
    list = mem_cons(message_word, list);
    list = mem_cons(error_num_word, list);
    
    return result_ok(value_list(list));
}

//==========================================================================
// Control Transfer (stubs for now)
//==========================================================================

static Result prim_go(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "go", value_to_string(args[0]));
    }
    
    // TODO: Implement go functionality
    // For now, just return an error
    return result_error(ERR_CANT_FIND_LABEL);
}

static Result prim_label(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "label", value_to_string(args[0]));
    }
    
    // label does nothing itself
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
    
    // Boolean operations
    primitive_register("true", 0, prim_true);
    primitive_register("false", 0, prim_false);
    
    // Test/conditional flow
    primitive_register("test", 1, prim_test);
    primitive_register("iftrue", 1, prim_iftrue);
    primitive_register("ift", 1, prim_iftrue); // Abbreviation
    primitive_register("iffalse", 1, prim_iffalse);
    primitive_register("iff", 1, prim_iffalse); // Abbreviation
    
    // Timing
    primitive_register("wait", 1, prim_wait);
    
    // Pause/continue
    primitive_register("pause", 0, prim_pause);
    primitive_register("co", 0, prim_co);
    
    // Exception handling
    primitive_register("catch", 2, prim_catch);
    primitive_register("throw", 1, prim_throw);
    primitive_register("error", 0, prim_error);
    
    // Control transfer
    primitive_register("go", 1, prim_go);
    primitive_register("label", 1, prim_label);
}
