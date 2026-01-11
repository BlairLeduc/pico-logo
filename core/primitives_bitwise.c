//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Bitwise primitives: bitand, bitor, bitxor, bitnot, ashift, lshift
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include <stdint.h>

// Helper macro to extract integer from argument
#define REQUIRE_INTEGER(arg, var) \
    int32_t var; \
    do { \
        float f; \
        if (!value_to_number(arg, &f)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(arg)); \
        var = (int32_t)f; \
    } while(0)

// bitand - outputs bitwise AND of its inputs (integers)
static Result prim_bitand(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    REQUIRE_INTEGER(args[0], result);

    for (int i = 1; i < argc; i++)
    {
        REQUIRE_INTEGER(args[i], n);
        result = result & n;
    }

    return result_ok(value_number((float)result));
}

// bitor - outputs bitwise OR of its inputs (integers)
static Result prim_bitor(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    REQUIRE_INTEGER(args[0], result);

    for (int i = 1; i < argc; i++)
    {
        REQUIRE_INTEGER(args[i], n);
        result = result | n;
    }

    return result_ok(value_number((float)result));
}

// bitxor - outputs bitwise exclusive OR of its inputs (integers)
static Result prim_bitxor(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    REQUIRE_INTEGER(args[0], result);

    for (int i = 1; i < argc; i++)
    {
        REQUIRE_INTEGER(args[i], n);
        result = result ^ n;
    }

    return result_ok(value_number((float)result));
}

// bitnot - outputs bitwise NOT of its input (integer)
static Result prim_bitnot(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    REQUIRE_INTEGER(args[0], n);

    return result_ok(value_number((float)(~n)));
}

// ashift - arithmetic shift left by num2 bits (right with sign extension if negative)
static Result prim_ashift(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    REQUIRE_INTEGER(args[0], num);
    REQUIRE_INTEGER(args[1], shift);

    int32_t result;
    if (shift >= 0)
    {
        // Left shift
        result = num << shift;
    }
    else
    {
        // Right arithmetic shift (sign extension)
        // C guarantees sign extension for signed integers
        result = num >> (-shift);
    }

    return result_ok(value_number((float)result));
}

// lshift - logical shift left by num2 bits (right with zero fill if negative)
static Result prim_lshift(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    REQUIRE_INTEGER(args[0], num);
    REQUIRE_INTEGER(args[1], shift);

    uint32_t unum = (uint32_t)num;
    uint32_t result;
    if (shift >= 0)
    {
        // Left shift
        result = unum << shift;
    }
    else
    {
        // Right logical shift (zero fill)
        result = unum >> (-shift);
    }

    return result_ok(value_number((float)(int32_t)result));
}

void primitives_bitwise_init(void)
{
    primitive_register("bitand", 2, prim_bitand);
    primitive_register("bitor", 2, prim_bitor);
    primitive_register("bitxor", 2, prim_bitxor);
    primitive_register("bitnot", 1, prim_bitnot);
    primitive_register("ashift", 2, prim_ashift);
    primitive_register("lshift", 2, prim_lshift);
}
