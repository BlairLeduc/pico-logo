//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Control flow primitives: run, repeat, stop, output, if, true, false,
//  test, iftrue, iffalse, wait, pause, co, catch, throw, error, go, label
//

#include "primitives.h"
#include "procedures.h"
#include "error.h"
#include "eval.h"
#include "lexer.h"
#include "repl.h"
#include "value.h"
#include "variables.h"
#include "devices/io.h"
#include <strings.h>  // for strcasecmp
#include <unistd.h>   // for usleep
#include <stdio.h>    // for snprintf

//==========================================================================
// Error info storage for the error primitive
//==========================================================================

// NOTE: This structure stores the most recent caught error for the error
// primitive to retrieve. It is populated when catch handles an error.
typedef struct
{
    bool has_error;
    int error_code;
    const char *error_message;
    const char *error_proc;
    const char *error_caller;
} ErrorInfo;

static ErrorInfo last_error = {false, 0, NULL, NULL, NULL};

// Function to reset test state (for testing purposes)
void primitives_control_reset_test_state(void)
{
    var_reset_test_state();
    last_error.has_error = false;
    last_error.error_code = 0;
    last_error.error_message = NULL;
    last_error.error_proc = NULL;
    last_error.error_caller = NULL;
}

// Helper to populate error info from a Result
static void set_last_error(Result r)
{
    if (r.status == RESULT_ERROR)
    {
        last_error.has_error = true;
        last_error.error_code = r.error_code;
        last_error.error_message = error_message(r.error_code);
        last_error.error_proc = r.error_proc;
        last_error.error_caller = r.error_caller;
    }
}

static Result prim_run(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
    if (!value_is_list(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "run", value_to_string(args[0]));
    }
    // Use eval_run_list_expr so run can act as an operation
    return eval_run_list_expr(eval, args[0].as.node);
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
    (void)eval;
    (void)argc;
    (void)args;
    
    // Return the innermost repeat count (1-based)
    return result_ok(value_number(eval->repcount));
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
            return result_error_arg(ERR_NOT_BOOL, NULL, str);
    }
    else
    {
        return result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(pred));
    }
    
    // Check that list1 is a list
    if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "if", value_to_string(args[1]));
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
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, "if", value_to_string(args[2]));
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
    (void)argc;
    
    if (!value_is_list(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "iftrue", value_to_string(args[0]));
    }
    
    bool test_value;
    if (var_get_test(&test_value) && test_value)
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
    
    bool test_value;
    if (var_get_test(&test_value) && !test_value)
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

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error(ERR_UNDEFINED);
    }
    
    // Wait for tenths of a second (each tenth is 100 milliseconds)
    // Check for user interrupt every 100ms to make the wait interruptible
    for (int i = 0; i < tenths; i++)
    {
        if (logo_io_check_user_interrupt(io))
        {
            return result_error(ERR_STOPPED);
        }
        logo_io_sleep(io, 100);
    }
    
    return result_none();
}

//==========================================================================
// Pause/Continue
//==========================================================================

// Flag to signal continue from pause
static bool pause_continue_requested = false;

// Check if continue has been requested and reset the flag
bool pause_check_continue(void)
{
    bool result = pause_continue_requested;
    pause_continue_requested = false;
    return result;
}

// Request continue from pause
void pause_request_continue(void)
{
    pause_continue_requested = true;
}

// Reset pause state (for testing)
void pause_reset_state(void)
{
    pause_continue_requested = false;
}

static Result prim_pause(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    // Get the current procedure name
    const char *proc_name = proc_get_current();
    if (proc_name == NULL)
    {
        // pause at top level is an error
        return result_error(ERR_AT_TOPLEVEL);
    }
    
    // Run the pause REPL - this blocks until co is called or throw "toplevel
    LogoIO *io = primitives_get_io();
    if (!io || !io->console)
    {
        return result_none();
    }
    
    logo_io_write_line(io, "Pausing...");
    
    ReplState state;
    repl_init(&state, io, REPL_FLAGS_PAUSE, proc_name);
    return repl_run(&state);
}

static Result prim_co(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;
    
    // Signal to exit the pause REPL
    pause_request_continue();
    return result_none();
}

//==========================================================================
// Exception Handling
//==========================================================================

static Result prim_catch(Evaluator *eval, int argc, Value *args)
{
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "catch", value_to_string(args[0]));
    }
    if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "catch", value_to_string(args[1]));
    }
    
    const char *tag = value_to_string(args[0]);
    
    // Run the list
    Result r = eval_run_list(eval, args[1].as.node);
    
    // Check if a throw or error occurred
    if (r.status == RESULT_THROW)
    {
        // Check if the tag matches
        if (strcasecmp(r.throw_tag, tag) == 0)
        {
            // Caught the throw, return normally
            return result_none();
        }
        // Tag doesn't match, propagate the throw
        return r;
    }
    else if (r.status == RESULT_ERROR && strcasecmp(tag, "error") == 0)
    {
        // Special case: catch "error catches errors
        // Save the error info for the error primitive
        set_last_error(r);
        // Return normally (error was caught)
        return result_none();
    }
    
    // No throw/error or didn't match, return the result as-is
    return r;
}

static Result prim_throw(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    if (!value_is_word(args[0]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "throw", value_to_string(args[0]));
    }
    
    const char *tag = value_to_string(args[0]);
    
    // Return a RESULT_THROW with the tag
    // The tag will be checked by catch primitives up the call stack
    return result_throw(tag);
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
    list = mem_cons(caller_word, list);
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
    primitive_register("repcount", 0, prim_repcount);
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
