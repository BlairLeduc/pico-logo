//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Arithmetic primitives: sum, difference, product, quotient, random,
//                         arctan, cos, sin, int, intquotient, remainder, round, sqrt, form
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "devices/io.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Conversion factor from degrees to radians
#define DEG_TO_RAD (3.14159265358979323846f / 180.0f)
// Conversion factor from radians to degrees
#define RAD_TO_DEG (180.0f / 3.14159265358979323846f)

// abs - outputs absolute value of number
static Result prim_abs(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    REQUIRE_NUMBER(args[0], n);

    return result_ok(value_number(fabsf(n)));
}

static Result prim_sum(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    float total = 0;

    for (int i = 0; i < argc; i++)
    {
        REQUIRE_NUMBER(args[i], n);

        total += n;
    }

    return result_ok(value_number(total));
}

static Result prim_difference(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], a);
    REQUIRE_NUMBER(args[1], b);

    return result_ok(value_number(a - b));
}

static Result prim_product(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    float total = 1;
    
    for (int i = 0; i < argc; i++)
    {
        REQUIRE_NUMBER(args[i], n);
        
        total *= n;
    }

    return result_ok(value_number(total));
}

static Result prim_quotient(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], a);
    REQUIRE_NUMBER(args[1], b);

    if (b == 0)
        return result_error_arg(ERR_DIVIDE_BY_ZERO, NULL, NULL);
    
    return result_ok(value_number(a / b));
}

// random integer - outputs a random non-negative integer less than integer
static Result prim_random(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], n);
    
    int limit = (int)n;

    if (limit <= 0)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    
    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    int result = logo_io_random(io) % limit;
    return result_ok(value_number((float)result));
}

// arctan - outputs arctangent of number in degrees
static Result prim_arctan(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], n);
    
    return result_ok(value_number(atanf(n) * RAD_TO_DEG));
}

// cos - outputs cosine of number (in degrees)
static Result prim_cos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], n);
    
    return result_ok(value_number(cosf(n * DEG_TO_RAD)));
}

// sin - outputs sine of number (in degrees)
static Result prim_sin(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], n);
    
    return result_ok(value_number(sinf(n * DEG_TO_RAD)));
}

// int - returns integer part of number (truncates toward zero)
static Result prim_int(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], n);
    
    return result_ok(value_number(truncf(n)));
}

// intquotient - outputs integer1 / integer2, truncated to integer
static Result prim_intquotient(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], a);
    REQUIRE_NUMBER(args[1], b);
    
    int ia = (int)a;
    int ib = (int)b;
    if (ib == 0)
        return result_error_arg(ERR_DIVIDE_BY_ZERO, NULL, NULL);
    
    return result_ok(value_number((float)(ia / ib)));
}

// remainder - outputs remainder of integer1 / integer2
static Result prim_remainder(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], a);
    REQUIRE_NUMBER(args[1], b);
    
    int ia = (int)a;
    int ib = (int)b;
    if (ib == 0)
        return result_error_arg(ERR_DIVIDE_BY_ZERO, NULL, NULL);
    
    return result_ok(value_number((float)(ia % ib)));
}

// round - rounds number to nearest integer
static Result prim_round(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], n);
    
    return result_ok(value_number(roundf(n)));
}

// sqrt - outputs square root of number
static Result prim_sqrt(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], n);
    
    if (n < 0)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    
    return result_ok(value_number(sqrtf(n)));
}

// log - Outputs base-10 logarithm of number
static Result prim_log(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], n);

    if (n <= 0)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));

    return result_ok(value_number(log10f(n)));
}

// ln - Outputs natural logarithm of number
static Result prim_ln(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], n);

    if (n <= 0)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));

    return result_ok(value_number(logf(n)));
}

// pwr - outputs number1 raised to the power of number2
static Result prim_pwr(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], base);
    REQUIRE_NUMBER(args[1], exponent);

    return result_ok(value_number(powf(base, exponent)));
}

// exp - outputs e raised to the power of number
static Result prim_exp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], exponent);

    return result_ok(value_number(expf(exponent)));
}

// form - outputs a word representing number formatted to fit in a field of
// width characters with decimalplaces digits to the right of the decimal point.
// If decimalplaces is zero, no decimal point is included. The number is rounded
// to the specified number of decimal places.
// If number is too large to fit in the specified width, form outputs a string
// using the minimum length required for number with decimalplaces.
// Error if width <= 0 or decimalplaces < 0.
static Result prim_form(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], number);
    REQUIRE_NUMBER(args[1], width_f);
    REQUIRE_NUMBER(args[2], decimalplaces_f);

    int width = (int)width_f;
    int decimalplaces = (int)decimalplaces_f;

    // Validate inputs
    if (width <= 0)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    if (decimalplaces < 0)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[2]));

    // Buffer for formatting - enough for any reasonable number
    char buf[64];

    // Round the number to the specified decimal places
    float multiplier = 1.0f;
    for (int i = 0; i < decimalplaces; i++)
        multiplier *= 10.0f;
    float rounded = roundf(number * multiplier) / multiplier;

    // Format the number with the specified decimal places
    char fmt_buf[48];
    int fmt_len;

    if (decimalplaces == 0)
    {
        // No decimal point - format as integer
        int int_val = (int)roundf(rounded);
        fmt_len = snprintf(fmt_buf, sizeof(fmt_buf), "%d", int_val);
    }
    else
    {
        // Format with specified decimal places
        fmt_len = snprintf(fmt_buf, sizeof(fmt_buf), "%.*f", decimalplaces, (double)rounded);
    }

    // If the formatted number fits in width, pad with leading spaces
    // If it doesn't fit, just use the formatted number as-is
    if (fmt_len < width)
    {
        // Pad with leading spaces
        int padding = width - fmt_len;
        for (int i = 0; i < padding; i++)
            buf[i] = ' ';
        memcpy(buf + padding, fmt_buf, fmt_len + 1);  // +1 for null terminator
    }
    else
    {
        // Number doesn't fit, use as-is
        memcpy(buf, fmt_buf, fmt_len + 1);
    }

    // Create a word from the formatted string
    Node result = mem_atom_cstr(buf);
    return result_ok(value_word(result));
}

void primitives_arithmetic_init(void)
{
    primitive_register("abs", 1, prim_abs);
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
    primitive_register("log", 1, prim_log);
    primitive_register("ln", 1, prim_ln);
    primitive_register("pwr", 2, prim_pwr);
    primitive_register("exp", 1, prim_exp);
    primitive_register("form", 3, prim_form);
}
