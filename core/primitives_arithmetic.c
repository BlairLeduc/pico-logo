//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Arithmetic primitives: sum, difference, product, quotient, random,
//                         arctan, cos, sin, int, intquotient, remainder, round, sqrt
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "devices/io.h"
#include <stdlib.h>
#include <math.h>

// Conversion factor from degrees to radians
#define DEG_TO_RAD (3.14159265358979323846f / 180.0f)
// Conversion factor from radians to degrees
#define RAD_TO_DEG (180.0f / 3.14159265358979323846f)

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

// random integer - outputs a random non-negative integer less than integer
static Result prim_random(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float n;
    if (!value_to_number(args[0], &n))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "random", value_to_string(args[0]));
    
    int limit = (int)n;
    if (limit <= 0)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "random", value_to_string(args[0]));
    
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, "random", NULL);
    }

    int result = logo_io_random(io) % limit;
    return result_ok(value_number((float)result));
}

// arctan - outputs arctangent of number in degrees
static Result prim_arctan(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float n;
    if (!value_to_number(args[0], &n))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "arctan", value_to_string(args[0]));
    
    return result_ok(value_number(atanf(n) * RAD_TO_DEG));
}

// cos - outputs cosine of number (in degrees)
static Result prim_cos(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float n;
    if (!value_to_number(args[0], &n))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "cos", value_to_string(args[0]));
    
    return result_ok(value_number(cosf(n * DEG_TO_RAD)));
}

// sin - outputs sine of number (in degrees)
static Result prim_sin(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float n;
    if (!value_to_number(args[0], &n))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "sin", value_to_string(args[0]));
    
    return result_ok(value_number(sinf(n * DEG_TO_RAD)));
}

// int - returns integer part of number (truncates toward zero)
static Result prim_int(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float n;
    if (!value_to_number(args[0], &n))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "int", value_to_string(args[0]));
    
    return result_ok(value_number(truncf(n)));
}

// intquotient - outputs integer1 / integer2, truncated to integer
static Result prim_intquotient(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float a, b;
    if (!value_to_number(args[0], &a))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "intquotient", value_to_string(args[0]));
    if (!value_to_number(args[1], &b))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "intquotient", value_to_string(args[1]));
    
    int ia = (int)a;
    int ib = (int)b;
    if (ib == 0)
        return result_error_arg(ERR_DIVIDE_BY_ZERO, "intquotient", NULL);
    
    return result_ok(value_number((float)(ia / ib)));
}

// remainder - outputs remainder of integer1 / integer2
static Result prim_remainder(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float a, b;
    if (!value_to_number(args[0], &a))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "remainder", value_to_string(args[0]));
    if (!value_to_number(args[1], &b))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "remainder", value_to_string(args[1]));
    
    int ia = (int)a;
    int ib = (int)b;
    if (ib == 0)
        return result_error_arg(ERR_DIVIDE_BY_ZERO, "remainder", NULL);
    
    return result_ok(value_number((float)(ia % ib)));
}

// round - rounds number to nearest integer
static Result prim_round(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float n;
    if (!value_to_number(args[0], &n))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "round", value_to_string(args[0]));
    
    return result_ok(value_number(roundf(n)));
}

// sqrt - outputs square root of number
static Result prim_sqrt(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    float n;
    if (!value_to_number(args[0], &n))
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "sqrt", value_to_string(args[0]));
    
    if (n < 0)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "sqrt", value_to_string(args[0]));
    
    return result_ok(value_number(sqrtf(n)));
}

void primitives_arithmetic_init(void)
{
    primitive_register("sum", 2, prim_sum);
    primitive_register("difference", 2, prim_difference);
    primitive_register("product", 2, prim_product);
    primitive_register("quotient", 2, prim_quotient);
    primitive_register("random", 1, prim_random);
    primitive_register("arctan", 1, prim_arctan);
    primitive_register("cos", 1, prim_cos);
    primitive_register("sin", 1, prim_sin);
    primitive_register("int", 1, prim_int);
    primitive_register("intquotient", 2, prim_intquotient);
    primitive_register("remainder", 2, prim_remainder);
    primitive_register("round", 1, prim_round);
    primitive_register("sqrt", 1, prim_sqrt);
}
