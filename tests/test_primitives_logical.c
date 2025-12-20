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
// Logical Primitive Tests - AND
//==========================================================================

void test_and_true_true(void)
{
    Result r = eval_string("and \"true \"true");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_and_true_false(void)
{
    Result r = eval_string("and \"true \"false");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(r.value));
}

void test_and_false_true(void)
{
    Result r = eval_string("and \"false \"true");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(r.value));
}

void test_and_false_false(void)
{
    Result r = eval_string("and \"false \"false");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(r.value));
}

void test_and_variadic_all_true(void)
{
    Result r = eval_string("(and \"true \"true \"true \"true)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_and_variadic_one_false(void)
{
    Result r = eval_string("(and \"true \"true \"false \"true)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(r.value));
}

void test_and_with_comparison(void)
{
    Result r = eval_string("and 1 < 2 3 < 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_and_with_equal(void)
{
    Result r = eval_string("and 5 = 5 equal? \"hello \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_and_error_not_bool(void)
{
    Result r = eval_string("and \"true \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, r.error_code);
    TEST_ASSERT_EQUAL_STRING("and", r.error_proc);
    TEST_ASSERT_EQUAL_STRING("hello", r.error_arg);
}

//==========================================================================
// Logical Primitive Tests - OR
//==========================================================================

void test_or_true_true(void)
{
    Result r = eval_string("or \"true \"true");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_or_true_false(void)
{
    Result r = eval_string("or \"true \"false");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_or_false_true(void)
{
    Result r = eval_string("or \"false \"true");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_or_false_false(void)
{
    Result r = eval_string("or \"false \"false");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(r.value));
}

void test_or_variadic_all_false(void)
{
    Result r = eval_string("(or \"false \"false \"false \"false)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(r.value));
}

void test_or_variadic_one_true(void)
{
    Result r = eval_string("(or \"false \"false \"true \"false)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_or_with_comparison(void)
{
    Result r = eval_string("or 1 > 2 3 < 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_or_error_not_bool(void)
{
    Result r = eval_string("or \"false 42");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, r.error_code);
    TEST_ASSERT_EQUAL_STRING("or", r.error_proc);
    TEST_ASSERT_EQUAL_STRING("42", r.error_arg);
}

//==========================================================================
// Logical Primitive Tests - NOT
//==========================================================================

void test_not_true(void)
{
    Result r = eval_string("not \"true");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(r.value));
}

void test_not_false(void)
{
    Result r = eval_string("not \"false");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_not_with_comparison(void)
{
    Result r = eval_string("not 1 > 2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_not_double(void)
{
    Result r = eval_string("not not \"true");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_not_error_not_bool(void)
{
    Result r = eval_string("not \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, r.error_code);
    TEST_ASSERT_EQUAL_STRING("not", r.error_proc);
    TEST_ASSERT_EQUAL_STRING("hello", r.error_arg);
}

void test_not_error_number(void)
{
    Result r = eval_string("not 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, r.error_code);
}

//==========================================================================
// Combined Logical Tests
//==========================================================================

void test_and_or_combined(void)
{
    // and (or false true) true => true
    Result r = eval_string("and or \"false \"true \"true");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_not_and_combined(void)
{
    // not (and true false) => true
    Result r = eval_string("not and \"true \"false");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_complex_logical_expression(void)
{
    // or (and true true) (and false true) => true
    Result r = eval_string("or and \"true \"true and \"false \"true");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();

    // AND tests
    RUN_TEST(test_and_true_true);
    RUN_TEST(test_and_true_false);
    RUN_TEST(test_and_false_true);
    RUN_TEST(test_and_false_false);
    RUN_TEST(test_and_variadic_all_true);
    RUN_TEST(test_and_variadic_one_false);
    RUN_TEST(test_and_with_comparison);
    RUN_TEST(test_and_with_equal);
    RUN_TEST(test_and_error_not_bool);

    // OR tests
    RUN_TEST(test_or_true_true);
    RUN_TEST(test_or_true_false);
    RUN_TEST(test_or_false_true);
    RUN_TEST(test_or_false_false);
    RUN_TEST(test_or_variadic_all_false);
    RUN_TEST(test_or_variadic_one_true);
    RUN_TEST(test_or_with_comparison);
    RUN_TEST(test_or_error_not_bool);

    // NOT tests
    RUN_TEST(test_not_true);
    RUN_TEST(test_not_false);
    RUN_TEST(test_not_with_comparison);
    RUN_TEST(test_not_double);
    RUN_TEST(test_not_error_not_bool);
    RUN_TEST(test_not_error_number);

    // Combined tests
    RUN_TEST(test_and_or_combined);
    RUN_TEST(test_not_and_combined);
    RUN_TEST(test_complex_logical_expression);

    return UNITY_END();
}
