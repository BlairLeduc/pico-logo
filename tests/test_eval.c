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
// Core Evaluation Tests
//==========================================================================

void test_eval_number(void)
{
    Result r = eval_string("42");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_NUMBER, r.value.type);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_eval_negative_number(void)
{
    Result r = eval_string("-5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(-5.0f, r.value.as.number);
}

void test_eval_infix_add(void)
{
    Result r = eval_string("3 + 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
}

void test_eval_infix_precedence(void)
{
    Result r = eval_string("3 + 4 * 2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(11.0f, r.value.as.number);
}

void test_eval_parentheses(void)
{
    Result r = eval_string("(3 + 4) * 2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(14.0f, r.value.as.number);
}

void test_eval_quoted_word(void)
{
    Result r = eval_string("\"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// Print Tests (output primitives)
//==========================================================================

void test_print(void)
{
    run_string("print 42");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
}

void test_print_word(void)
{
    run_string("print \"hello");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_print_variadic_parens(void)
{
    run_string("(print 1 2 3)");
    TEST_ASSERT_EQUAL_STRING("1 2 3\n", output_buffer);
}

//==========================================================================
// Error Message Tests
//==========================================================================

void test_error_dont_know_how(void)
{
    // I don't know how to foobar
    Result r = eval_string("foobar");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DONT_KNOW_HOW, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("I don't know how to foobar", msg);
}

void test_error_not_enough_inputs(void)
{
    // Not enough inputs to sum
    Result r = eval_string("sum 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_ENOUGH_INPUTS, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("Not enough inputs to sum", msg);
}

void test_error_infix_doesnt_like(void)
{
    // + doesn't like hello as input
    Result r = eval_string("1 + \"hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("+ doesn't like hello as input", msg);
}

int main(void)
{
    UNITY_BEGIN();

    // Core evaluation tests
    RUN_TEST(test_eval_number);
    RUN_TEST(test_eval_negative_number);
    RUN_TEST(test_eval_infix_add);
    RUN_TEST(test_eval_infix_precedence);
    RUN_TEST(test_eval_parentheses);
    RUN_TEST(test_eval_quoted_word);

    // Print tests
    RUN_TEST(test_print);
    RUN_TEST(test_print_word);
    RUN_TEST(test_print_variadic_parens);

    // Error message tests
    RUN_TEST(test_error_dont_know_how);
    RUN_TEST(test_error_not_enough_inputs);
    RUN_TEST(test_error_infix_doesnt_like);

    return UNITY_END();
}

