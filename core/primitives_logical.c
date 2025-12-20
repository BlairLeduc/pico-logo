//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Logical primitives: and, or, not
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include <stdlib.h>
#include <string.h>

// Helper to extract boolean from a value (word "true" or "false")
static bool get_bool_arg(Value v, bool *out)
{
    const char *str = value_to_string(v);
    if (str == NULL) return false;
    
    if (strcmp(str, "true") == 0)
    {
        *out = true;
        return true;
    }
    if (strcmp(str, "false") == 0)
    {
        *out = false;
        return true;
    }
    return false;
}

// and - outputs true if all arguments are true, false otherwise
static Result prim_and(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    bool result = true;
    for (int i = 0; i < argc; i++)
    {
        bool b;
        if (!get_bool_arg(args[i], &b))
        {
            return result_error_arg(ERR_NOT_BOOL, "and", value_to_string(args[i]));
        }
        result = result && b;
    }
    return result_ok(value_word(mem_atom_cstr(result ? "true" : "false")));
}

// or - outputs true if any argument is true, false otherwise
static Result prim_or(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    bool result = false;
    for (int i = 0; i < argc; i++)
    {
        bool b;
        if (!get_bool_arg(args[i], &b))
        {
            return result_error_arg(ERR_NOT_BOOL, "or", value_to_string(args[i]));
        }
        result = result || b;
    }
    return result_ok(value_word(mem_atom_cstr(result ? "true" : "false")));
}

// not - outputs true if argument is false, false if argument is true
static Result prim_not(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    bool b;
    if (!get_bool_arg(args[0], &b))
    {
        return result_error_arg(ERR_NOT_BOOL, "not", value_to_string(args[0]));
    }
    return result_ok(value_word(mem_atom_cstr(b ? "false" : "true")));
}

void primitives_logical_init(void)
{
    primitive_register("and", 2, prim_and);
    primitive_register("or", 2, prim_or);
    primitive_register("not", 1, prim_not);
}
