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

    return UNITY_END();
}
