//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Arithmetic Primitive Tests
//==========================================================================

void test_sum(void)
{
    Result r = eval_string("sum 3 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
}

void test_sum_variadic_parens(void)
{
    // (sum 1 2 3 4 5) should add all arguments
    Result r = eval_string("(sum 1 2 3 4 5)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, r.value.as.number);
}

void test_sum_single_arg_parens(void)
{
    // (sum 5) with just one arg
    Result r = eval_string("(sum 5)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, r.value.as.number);
}

void test_difference(void)
{
    Result r = eval_string("difference 10 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
}

void test_product(void)
{
    Result r = eval_string("product 3 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(12.0f, r.value.as.number);
}

void test_product_variadic_parens(void)
{
    // (product 2 3 4) should multiply all arguments
    Result r = eval_string("(product 2 3 4)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(24.0f, r.value.as.number);
}

void test_quotient(void)
{
    Result r = eval_string("quotient 20 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, r.value.as.number);
}

void test_divide_by_zero(void)
{
    Result r = eval_string("quotient 10 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(13, r.error_code); // ERR_DIVIDE_BY_ZERO
}

void test_error_divide_by_zero_msg(void)
{
    // Can't divide by zero
    Result r = eval_string("quotient 5 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DIVIDE_BY_ZERO, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("Can't divide by zero", msg);
}

void test_error_sum_doesnt_like(void)
{
    // sum doesn't like hello as input
    Result r = eval_string("sum 1 \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    TEST_ASSERT_EQUAL_STRING("sum", r.error_proc);
    TEST_ASSERT_EQUAL_STRING("hello", r.error_arg);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("sum doesn't like hello as input", msg);
}

void test_random(void)
{
    // random should return a non-negative integer less than the input
    Result r = eval_string("random 10");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(r.value.as.number >= 0);
    TEST_ASSERT_TRUE(r.value.as.number < 10);
}

void test_random_error_negative(void)
{
    Result r = eval_string("random -5");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_arctan(void)
{
    // arctan 1 should be 45 degrees
    Result r = eval_string("arctan 1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 45.0f, r.value.as.number);
}

void test_arctan_zero(void)
{
    // arctan 0 should be 0 degrees
    Result r = eval_string("arctan 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, r.value.as.number);
}

void test_cos(void)
{
    // cos 0 should be 1
    Result r = eval_string("cos 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, r.value.as.number);
}

void test_cos_90(void)
{
    // cos 90 should be 0
    Result r = eval_string("cos 90");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, r.value.as.number);
}

void test_cos_60(void)
{
    // cos 60 should be 0.5
    Result r = eval_string("cos 60");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, r.value.as.number);
}

void test_sin(void)
{
    // sin 0 should be 0
    Result r = eval_string("sin 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, r.value.as.number);
}

void test_sin_90(void)
{
    // sin 90 should be 1
    Result r = eval_string("sin 90");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, r.value.as.number);
}

void test_sin_30(void)
{
    // sin 30 should be 0.5
    Result r = eval_string("sin 30");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, r.value.as.number);
}

void test_int(void)
{
    // int 3.7 should be 3
    Result r = eval_string("int 3.7");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, r.value.as.number);
}

void test_int_negative(void)
{
    // int -3.7 should be -3 (truncate toward zero)
    Result r = eval_string("int -3.7");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(-3.0f, r.value.as.number);
}

void test_int_whole(void)
{
    // int 5 should be 5
    Result r = eval_string("int 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, r.value.as.number);
}

void test_intquotient(void)
{
    // intquotient 17 5 should be 3
    Result r = eval_string("intquotient 17 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, r.value.as.number);
}

void test_intquotient_truncates_inputs(void)
{
    // intquotient 17.9 5.9 should truncate inputs to 17 and 5, giving 3
    Result r = eval_string("intquotient 17.9 5.9");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, r.value.as.number);
}

void test_intquotient_divide_by_zero(void)
{
    Result r = eval_string("intquotient 10 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DIVIDE_BY_ZERO, r.error_code);
}

void test_remainder(void)
{
    // remainder 17 5 should be 2
    Result r = eval_string("remainder 17 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, r.value.as.number);
}

void test_remainder_truncates_inputs(void)
{
    // remainder 17.9 5.9 should truncate inputs to 17 and 5, giving 2
    Result r = eval_string("remainder 17.9 5.9");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, r.value.as.number);
}

void test_remainder_divide_by_zero(void)
{
    Result r = eval_string("remainder 10 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DIVIDE_BY_ZERO, r.error_code);
}

void test_round(void)
{
    // round 3.4 should be 3
    Result r = eval_string("round 3.4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, r.value.as.number);
}

void test_round_up(void)
{
    // round 3.6 should be 4
    Result r = eval_string("round 3.6");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, r.value.as.number);
}

void test_round_half(void)
{
    // round 3.5 should be 4 (round away from zero)
    Result r = eval_string("round 3.5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, r.value.as.number);
}

void test_round_negative(void)
{
    // round -3.6 should be -4
    Result r = eval_string("round -3.6");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(-4.0f, r.value.as.number);
}

void test_sqrt(void)
{
    // sqrt 16 should be 4
    Result r = eval_string("sqrt 16");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, r.value.as.number);
}

void test_sqrt_decimal(void)
{
    // sqrt 2 should be approximately 1.414
    Result r = eval_string("sqrt 2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.41421356f, r.value.as.number);
}

void test_sqrt_zero(void)
{
    // sqrt 0 should be 0
    Result r = eval_string("sqrt 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.value.as.number);
}

void test_sqrt_negative_error(void)
{
    // sqrt of negative number should error
    Result r = eval_string("sqrt -4");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// abs Tests
//==========================================================================

void test_abs_positive(void)
{
    // abs 5 should be 5
    Result r = eval_string("abs 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, r.value.as.number);
}

void test_abs_negative(void)
{
    // abs -5 should be 5
    Result r = eval_string("abs -5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, r.value.as.number);
}

void test_abs_zero(void)
{
    // abs 0 should be 0
    Result r = eval_string("abs 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.value.as.number);
}

void test_abs_decimal(void)
{
    // abs -3.14 should be 3.14
    Result r = eval_string("abs -3.14");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.14f, r.value.as.number);
}

//==========================================================================
// ln Tests (natural logarithm)
//==========================================================================

void test_ln_e(void)
{
    // ln of e should be 1
    Result r = eval_string("ln 2.718281828");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, r.value.as.number);
}

void test_ln_one(void)
{
    // ln 1 should be 0
    Result r = eval_string("ln 1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, r.value.as.number);
}

void test_ln_positive(void)
{
    // ln 10 should be approximately 2.3026
    Result r = eval_string("ln 10");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.302585f, r.value.as.number);
}

void test_ln_zero_error(void)
{
    // ln 0 should error
    Result r = eval_string("ln 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_ln_negative_error(void)
{
    // ln of negative number should error
    Result r = eval_string("ln -5");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// log Tests (base-10 logarithm)
//==========================================================================

void test_log_ten(void)
{
    // log 10 should be 1
    Result r = eval_string("log 10");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, r.value.as.number);
}

void test_log_hundred(void)
{
    // log 100 should be 2
    Result r = eval_string("log 100");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, r.value.as.number);
}

void test_log_one(void)
{
    // log 1 should be 0
    Result r = eval_string("log 1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, r.value.as.number);
}

void test_log_zero_error(void)
{
    // log 0 should error
    Result r = eval_string("log 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_log_negative_error(void)
{
    // log of negative number should error
    Result r = eval_string("log -5");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// pwr Tests (power function)
//==========================================================================

void test_pwr_basic(void)
{
    // pwr 2 3 should be 8
    Result r = eval_string("pwr 2 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(8.0f, r.value.as.number);
}

void test_pwr_square(void)
{
    // pwr 5 2 should be 25
    Result r = eval_string("pwr 5 2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(25.0f, r.value.as.number);
}

void test_pwr_zero_exponent(void)
{
    // pwr 5 0 should be 1
    Result r = eval_string("pwr 5 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, r.value.as.number);
}

void test_pwr_one_exponent(void)
{
    // pwr 7 1 should be 7
    Result r = eval_string("pwr 7 1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
}

void test_pwr_fractional_exponent(void)
{
    // pwr 9 0.5 should be 3 (square root)
    Result r = eval_string("pwr 9 0.5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.0f, r.value.as.number);
}

void test_pwr_negative_exponent(void)
{
    // pwr 2 -1 should be 0.5
    Result r = eval_string("pwr 2 -1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, r.value.as.number);
}

//==========================================================================
// exp Tests (e^x)
//==========================================================================

void test_exp_zero(void)
{
    // exp 0 should be 1
    Result r = eval_string("exp 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, r.value.as.number);
}

void test_exp_one(void)
{
    // exp 1 should be e (approximately 2.718)
    Result r = eval_string("exp 1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.718281828f, r.value.as.number);
}

void test_exp_two(void)
{
    // exp 2 should be e^2 (approximately 7.389)
    Result r = eval_string("exp 2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.389056f, r.value.as.number);
}

void test_exp_negative(void)
{
    // exp -1 should be 1/e (approximately 0.368)
    Result r = eval_string("exp -1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.367879f, r.value.as.number);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_sum);
    RUN_TEST(test_sum_variadic_parens);
    RUN_TEST(test_sum_single_arg_parens);
    RUN_TEST(test_difference);
    RUN_TEST(test_product);
    RUN_TEST(test_product_variadic_parens);
    RUN_TEST(test_quotient);
    RUN_TEST(test_divide_by_zero);
    RUN_TEST(test_error_divide_by_zero_msg);
    RUN_TEST(test_error_sum_doesnt_like);
    RUN_TEST(test_random);
    RUN_TEST(test_random_error_negative);
    RUN_TEST(test_arctan);
    RUN_TEST(test_arctan_zero);
    RUN_TEST(test_cos);
    RUN_TEST(test_cos_90);
    RUN_TEST(test_cos_60);
    RUN_TEST(test_sin);
    RUN_TEST(test_sin_90);
    RUN_TEST(test_sin_30);
    RUN_TEST(test_int);
    RUN_TEST(test_int_negative);
    RUN_TEST(test_int_whole);
    RUN_TEST(test_intquotient);
    RUN_TEST(test_intquotient_truncates_inputs);
    RUN_TEST(test_intquotient_divide_by_zero);
    RUN_TEST(test_remainder);
    RUN_TEST(test_remainder_truncates_inputs);
    RUN_TEST(test_remainder_divide_by_zero);
    RUN_TEST(test_round);
    RUN_TEST(test_round_up);
    RUN_TEST(test_round_half);
    RUN_TEST(test_round_negative);
    RUN_TEST(test_sqrt);
    RUN_TEST(test_sqrt_decimal);
    RUN_TEST(test_sqrt_zero);
    RUN_TEST(test_sqrt_negative_error);
    RUN_TEST(test_abs_positive);
    RUN_TEST(test_abs_negative);
    RUN_TEST(test_abs_zero);
    RUN_TEST(test_abs_decimal);
    RUN_TEST(test_ln_e);
    RUN_TEST(test_ln_one);
    RUN_TEST(test_ln_positive);
    RUN_TEST(test_ln_zero_error);
    RUN_TEST(test_ln_negative_error);
    RUN_TEST(test_log_ten);
    RUN_TEST(test_log_hundred);
    RUN_TEST(test_log_one);
    RUN_TEST(test_log_zero_error);
    RUN_TEST(test_log_negative_error);
    RUN_TEST(test_pwr_basic);
    RUN_TEST(test_pwr_square);
    RUN_TEST(test_pwr_zero_exponent);
    RUN_TEST(test_pwr_one_exponent);
    RUN_TEST(test_pwr_fractional_exponent);
    RUN_TEST(test_pwr_negative_exponent);
    RUN_TEST(test_exp_zero);
    RUN_TEST(test_exp_one);
    RUN_TEST(test_exp_two);
    RUN_TEST(test_exp_negative);

    return UNITY_END();
}
