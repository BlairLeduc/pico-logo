//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Debugging primitives: step, unstep, trace, untrace
//
//  These primitives help debug Logo procedures by:
//  
//  trace "name / trace [name1 name2 ...]
//    - Prints procedure calls with arguments (indented by depth)
//    - Prints return values or "stopped" on exit
//    - Useful for understanding recursion and call flow
//  
//  untrace "name / untrace [name1 name2 ...]
//    - Disables tracing for the specified procedure(s)
//  
//  step "name / step [name1 name2 ...]
//    - Marks procedure for stepped execution
//    - Note: Currently simplified - flag is set but execution is normal
//    - TODO: Implement pause-before-each-instruction behavior
//  
//  unstep "name / unstep [name1 name2 ...]
//    - Disables stepping for the specified procedure(s)
//

#include "primitives.h"
#include "procedures.h"
#include "memory.h"
#include "error.h"
#include "eval.h"

// step "name or step [name1 name2 ...]
// Set stepped flag on procedure(s)
static Result prim_step(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "step", NULL);
    }
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        if (!proc_exists(name))
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        proc_step(name);
    }
    else if (value_is_list(args[0]))
    {
        // List of procedure names
        Node curr = args[0].as.node;
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                if (!proc_exists(name))
                {
                    return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
                }
                proc_step(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "step", value_to_string(args[0]));
    }
    
    return result_none();
}

// unstep "name or unstep [name1 name2 ...]
// Clear stepped flag on procedure(s)
static Result prim_unstep(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "unstep", NULL);
    }
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        if (!proc_exists(name))
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        proc_unstep(name);
    }
    else if (value_is_list(args[0]))
    {
        // List of procedure names
        Node curr = args[0].as.node;
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                if (!proc_exists(name))
                {
                    return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
                }
                proc_unstep(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "unstep", value_to_string(args[0]));
    }
    
    return result_none();
}

// trace "name or trace [name1 name2 ...]
// Set traced flag on procedure(s)
static Result prim_trace(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "trace", NULL);
    }
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        if (!proc_exists(name))
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        proc_trace(name);
    }
    else if (value_is_list(args[0]))
    {
        // List of procedure names
        Node curr = args[0].as.node;
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                if (!proc_exists(name))
                {
                    return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
                }
                proc_trace(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "trace", value_to_string(args[0]));
    }
    
    return result_none();
}

// untrace "name or untrace [name1 name2 ...]
// Clear traced flag on procedure(s)
static Result prim_untrace(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "untrace", NULL);
    }
    
    if (value_is_word(args[0]))
    {
        // Single procedure name
        const char *name = mem_word_ptr(args[0].as.node);
        if (!proc_exists(name))
        {
            return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
        }
        proc_untrace(name);
    }
    else if (value_is_list(args[0]))
    {
        // List of procedure names
        Node curr = args[0].as.node;
        while (!mem_is_nil(curr))
        {
            Node elem = mem_car(curr);
            if (mem_is_word(elem))
            {
                const char *name = mem_word_ptr(elem);
                if (!proc_exists(name))
                {
                    return result_error_arg(ERR_DONT_KNOW_HOW, name, NULL);
                }
                proc_untrace(name);
            }
            curr = mem_cdr(curr);
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "untrace", value_to_string(args[0]));
    }
    
    return result_none();
}

void primitives_debug_init(void)
{
    primitive_register("step", 1, prim_step);
    primitive_register("unstep", 1, prim_unstep);
    primitive_register("trace", 1, prim_trace);
    primitive_register("untrace", 1, prim_untrace);
}
