//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Arithmetic primitives: sum, difference, product, quotient
//

#include "primitives.h"
#include "eval.h"

// Error codes from Error_Messages.md
#define ERR_DIVIDE_BY_ZERO 13
#define ERR_DOESNT_LIKE_INPUT 41

// Helper: require numeric argument
static bool require_number(Value v, float *out, int *err_code)
{
    if (!value_to_number(v, out))
    {
        *err_code = ERR_DOESNT_LIKE_INPUT;
        return false;
    }
    return true;
}

static Result prim_sum(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    float total = 0;
    for (int i = 0; i < argc; i++)
    {
        float n;
        int err;
        if (!require_number(args[i], &n, &err))
        {
            return result_error(err);
        }
        total += n;
    }
    return result_ok(value_number(total));
}

static Result prim_difference(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float a, b;
    int err;
    if (!require_number(args[0], &a, &err))
        return result_error(err);
    if (!require_number(args[1], &b, &err))
        return result_error(err);
    return result_ok(value_number(a - b));
}

static Result prim_product(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    float total = 1;
    for (int i = 0; i < argc; i++)
    {
        float n;
        int err;
        if (!require_number(args[i], &n, &err))
        {
            return result_error(err);
        }
        total *= n;
    }
    return result_ok(value_number(total));
}

static Result prim_quotient(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float a, b;
    int err;
    if (!require_number(args[0], &a, &err))
        return result_error(err);
    if (!require_number(args[1], &b, &err))
        return result_error(err);
    if (b == 0)
        return result_error(ERR_DIVIDE_BY_ZERO);
    return result_ok(value_number(a / b));
}

void primitives_arithmetic_init(void)
{
    primitive_register("sum", 2, prim_sum);
    primitive_register("difference", 2, prim_difference);
    primitive_register("product", 2, prim_product);
    primitive_register("quotient", 2, prim_quotient);
}
