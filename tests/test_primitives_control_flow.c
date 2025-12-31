//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for control flow primitives: run, repeat, repcount, stop, output
//

#include "test_scaffold.h"

void setUp(void)
{
    test_scaffold_setUp();
    primitives_control_reset_test_state();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Control Flow Primitive Tests
//==========================================================================

void test_repeat(void)
{
    run_string("repeat 3 [print 1]");
    TEST_ASSERT_EQUAL_STRING("1\n1\n1\n", output_buffer);
}

void test_repcount_basic(void)
{
    // repcount should output current repeat iteration (1-based)
    run_string("repeat 3 [print repcount]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_repcount_no_repeat(void)
{
    // repcount outside repeat should output -1
    Result r = eval_string("repcount");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.value.as.number);
}

void test_repcount_nested(void)
{
    // repcount should output innermost repeat count
    run_string("repeat 2 [repeat 3 [print repcount]]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n1\n2\n3\n", output_buffer);
}

void test_repcount_used_in_expression(void)
{
    // repcount can be used in arithmetic expressions
    run_string("repeat 3 [print repcount * 10]");
    TEST_ASSERT_EQUAL_STRING("10\n20\n30\n", output_buffer);
}

void test_stop(void)
{
    // stop should return RESULT_STOP
    Result r = eval_string("stop");
    TEST_ASSERT_EQUAL(RESULT_STOP, r.status);
}

void test_output(void)
{
    Result r = eval_string("output 99");
    TEST_ASSERT_EQUAL(RESULT_OUTPUT, r.status);
    TEST_ASSERT_EQUAL_FLOAT(99.0f, r.value.as.number);
}

void test_run_list(void)
{
    run_string("make \"x [print 42]");
    run_string("run :x");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
}

// Test infix subtraction inside lists - Logo evaluates infix operators when list is run
void test_infix_minus_in_list(void)
{
    // First test: basic infix minus after variable reference
    // :x - 1 should be evaluated as infix subtraction (space after -)
    run_string("make \"x 3");
    reset_output();
    run_string("print :x - 1");
    // Should print 2 (3 - 1)
    TEST_ASSERT_EQUAL_STRING("2\n", output_buffer);
    
    // Second test: inside a repeat list
    reset_output();
    run_string("repeat 2 [print sum 1 :x - 1]");
    // sum 1 (:x - 1) = sum 1 2 = 3, printed twice
    TEST_ASSERT_EQUAL_STRING("3\n3\n", output_buffer);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_repeat);
    RUN_TEST(test_repcount_basic);
    RUN_TEST(test_repcount_no_repeat);
    RUN_TEST(test_repcount_nested);
    RUN_TEST(test_repcount_used_in_expression);
    RUN_TEST(test_stop);
    RUN_TEST(test_output);
    RUN_TEST(test_run_list);
    RUN_TEST(test_infix_minus_in_list);

    return UNITY_END();
}
