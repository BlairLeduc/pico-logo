//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
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

//==========================================================================
// Boolean Operations Tests
//==========================================================================

void test_true(void)
{
    Result r = eval_string("true");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_false(void)
{
    Result r = eval_string("false");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(r.value));
}

//==========================================================================
// Test/Conditional Flow Tests
//==========================================================================

void test_test_iftrue(void)
{
    run_string("test true");
    run_string("iftrue [print \"yes]");
    TEST_ASSERT_EQUAL_STRING("yes\n", output_buffer);
}

void test_test_iffalse(void)
{
    run_string("test false");
    run_string("iffalse [print \"no]");
    TEST_ASSERT_EQUAL_STRING("no\n", output_buffer);
}

void test_iftrue_without_test(void)
{
    // iftrue should do nothing if test hasn't been run
    run_string("iftrue [print \"yes]");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

void test_iffalse_without_test(void)
{
    // iffalse should do nothing if test hasn't been run
    run_string("iffalse [print \"no]");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

void test_ift_abbreviation(void)
{
    run_string("test true");
    run_string("ift [print \"yes]");
    TEST_ASSERT_EQUAL_STRING("yes\n", output_buffer);
}

void test_iff_abbreviation(void)
{
    run_string("test false");
    run_string("iff [print \"no]");
    TEST_ASSERT_EQUAL_STRING("no\n", output_buffer);
}

void test_test_with_expression(void)
{
    // Test with a comparison expression
    run_string("test 5 > 3");
    run_string("iftrue [print \"greater]");
    TEST_ASSERT_EQUAL_STRING("greater\n", output_buffer);
}

//==========================================================================
// Wait Test
//==========================================================================

void test_wait(void)
{
    // Just test that wait doesn't crash and returns normally
    // We don't test the actual timing since that would make tests slow
    Result r = run_string("wait 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

//==========================================================================
// Catch/Throw Tests (basic stubs for now)
//==========================================================================

void test_catch_basic(void)
{
    // Basic catch that just runs the list
    run_string("catch \"error [print \"hello]");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_throw_no_catch(void)
{
    // Throw without matching catch should return error
    Result r = run_string("throw \"mytag");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_CATCH, r.error_code);
}

void test_error_no_error(void)
{
    // error should return empty list if no error occurred
    Result r = eval_string("error");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

//==========================================================================
// Go/Label Tests (stubs for now)
//==========================================================================

void test_label_basic(void)
{
    // label should do nothing
    Result r = run_string("label \"start");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_go_no_label(void)
{
    // go without matching label should return error
    Result r = run_string("go \"nowhere");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_CANT_FIND_LABEL, r.error_code);
}

int main(void)
{
    UNITY_BEGIN();

    // Existing tests
    RUN_TEST(test_repeat);
    RUN_TEST(test_stop);
    RUN_TEST(test_output);
    RUN_TEST(test_run_list);
    RUN_TEST(test_infix_minus_in_list);
    
    // Boolean operations
    RUN_TEST(test_true);
    RUN_TEST(test_false);
    
    // Test/conditional flow
    RUN_TEST(test_test_iftrue);
    RUN_TEST(test_test_iffalse);
    RUN_TEST(test_iftrue_without_test);
    RUN_TEST(test_iffalse_without_test);
    RUN_TEST(test_ift_abbreviation);
    RUN_TEST(test_iff_abbreviation);
    RUN_TEST(test_test_with_expression);
    
    // Wait
    RUN_TEST(test_wait);
    
    // Catch/throw/error
    RUN_TEST(test_catch_basic);
    RUN_TEST(test_throw_no_catch);
    RUN_TEST(test_error_no_error);
    
    // Go/label
    RUN_TEST(test_label_basic);
    RUN_TEST(test_go_no_label);

    return UNITY_END();
}
