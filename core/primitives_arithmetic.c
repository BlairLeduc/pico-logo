//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Arithmetic primitives: sum, difference, product, quotient
//

#include "primitives.h"
#include "error.h"
#include "eval.h"

static Result prim_sum(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    float total = 0;
    for (int i = 0; i < argc; i++)
    {
        float n;
        if (!value_to_number(args[i], &n))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, "sum", value_to_string(args[i]));
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
    if (!value_to_number(args[0], &a))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "difference", value_to_string(args[0]));
    if (!value_to_number(args[1], &b))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "difference", value_to_string(args[1]));
    return result_ok(value_number(a - b));
}

static Result prim_product(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    float total = 1;
    for (int i = 0; i < argc; i++)
    {
        float n;
        if (!value_to_number(args[i], &n))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, "product", value_to_string(args[i]));
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
    if (!value_to_number(args[0], &a))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "quotient", value_to_string(args[0]));
    if (!value_to_number(args[1], &b))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "quotient", value_to_string(args[1]));
    if (b == 0)
        return result_error_arg(ERR_DIVIDE_BY_ZERO, "quotient", NULL);
    return result_ok(value_number(a / b));
}

void primitives_arithmetic_init(void)
{
    primitive_register("sum", 2, prim_sum);
    primitive_register("difference", 2, prim_difference);
    primitive_register("product", 2, prim_product);
    primitive_register("quotient", 2, prim_quotient);
}
