//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Exception handling primitives: catch, throw, error
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "memory.h"
#include "value.h"
#include "variables.h"
#include <stdio.h>    // for snprintf
#include <strings.h>  // for strcasecmp

//==========================================================================
// Exception Handling
//==========================================================================

static Result prim_catch(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_WORD(args[0]);
    REQUIRE_LIST(args[1]);
    
    const char *tag = value_to_string(args[0]);
    
    return eval_push_catch(eval, tag, args[1].as.node);
}

static Result prim_throw(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);
    
    const char *tag = value_to_string(args[0]);
    
    // Return a RESULT_THROW with the tag
    // The tag will be checked by catch primitives up the call stack
    return result_throw(tag);
}

static Result prim_toplevel(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    return result_ok(value_word(mem_atom_cstr("toplevel")));
}

static Result prim_error(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    const CaughtError *err = error_get_caught();
    if (!err)
    {
        // Return empty list if no error
        return result_ok(value_list(NODE_NIL));
    }
    
    // Build a four-element list: [error-number message primitive-name procedure-name]
    // Convert error code to string
    char error_num_buf[16];
    snprintf(error_num_buf, sizeof(error_num_buf), "%d", err->code);
    Node error_num_word = mem_atom_cstr(error_num_buf);
    
    Node message_word = err->message[0] ? mem_atom_cstr(err->message) : mem_atom_cstr("");
    Node proc_word = err->proc ? mem_atom_cstr(err->proc) : mem_atom_cstr("");
    Node caller_word = err->caller ? mem_atom_cstr(err->caller) : NODE_NIL;
    
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
    primitive_register("toplevel", 0, prim_toplevel);
    primitive_register("error", 0, prim_error);
}
