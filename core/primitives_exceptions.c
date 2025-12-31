//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Exception handling primitives: catch, throw, error
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "memory.h"
#include "value.h"
#include "variables.h"
#include <string.h>   // for strncpy
#include <strings.h>  // for strcasecmp
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
    char error_message[256];  // Buffer to store formatted error message
    const char *error_proc;
    const char *error_caller;
} ErrorInfo;

static ErrorInfo last_error = {false, 0, "", NULL, NULL};

// Function to reset error state (for testing purposes)
void primitives_exceptions_reset_state(void)
{
    last_error.has_error = false;
    last_error.error_code = 0;
    last_error.error_message[0] = '\0';
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
        // Use error_format to get the fully formatted error message
        const char *formatted = error_format(r);
        strncpy(last_error.error_message, formatted, sizeof(last_error.error_message) - 1);
        last_error.error_message[sizeof(last_error.error_message) - 1] = '\0';
        last_error.error_proc = r.error_proc;
        last_error.error_caller = r.error_caller;
    }
}

//==========================================================================
// Exception Handling
//==========================================================================

static Result prim_catch(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_WORD("catch", args[0]);
    REQUIRE_LIST("catch", args[1]);
    
    const char *tag = value_to_string(args[0]);
    
    // Run the list
    Result r = eval_run_list(eval, args[1].as.node);
    
    // Check if a throw or error occurred
    if (r.status == RESULT_THROW)
    {
        // Special case: throw "toplevel always propagates to top level
        // and is never caught by any catch (per Logo reference)
        if (strcasecmp(r.throw_tag, "toplevel") == 0)
        {
            return r;
        }
        
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
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD("throw", args[0]);
    
    const char *tag = value_to_string(args[0]);
    
    // Return a RESULT_THROW with the tag
    // The tag will be checked by catch primitives up the call stack
    return result_throw(tag);
}

static Result prim_error(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
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
    
    Node message_word = last_error.error_message[0] ? mem_atom_cstr(last_error.error_message) : mem_atom_cstr("");
    Node proc_word = last_error.error_proc ? mem_atom_cstr(last_error.error_proc) : mem_atom_cstr("");
    Node caller_word = last_error.error_caller ? mem_atom_cstr(last_error.error_caller) : NODE_NIL;
    
    Node list = NODE_NIL;
    list = mem_cons(caller_word, list);
    list = mem_cons(proc_word, list);
    list = mem_cons(message_word, list);
    list = mem_cons(error_num_word, list);
    
    return result_ok(value_list(list));
}

void primitives_exceptions_init(void)
{
    primitive_register("catch", 2, prim_catch);
    primitive_register("throw", 1, prim_throw);
    primitive_register("error", 0, prim_error);
}
