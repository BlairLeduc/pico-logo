//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for control flow primitives: run, repeat, repcount, stop, output, while, do.while
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

void test_forever_with_stop(void)
{
    // forever should repeat until stop is called
    run_string("make \"x 1");
    reset_output();
    run_string("forever [print :x if :x = 3 [stop] make \"x :x + 1]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_forever_repcount(void)
{
    // repcount should work in forever loops (1-based)
    run_string("forever [print repcount if repcount = 3 [stop]]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_forever_repcount_nested_in_repeat(void)
{
    // repcount should report innermost loop count
    // Use throw/catch to exit just the forever loop, not the outer repeat
    run_string("repeat 2 [catch \"done [forever [print repcount if repcount = 2 [throw \"done]]]]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n1\n2\n", output_buffer);
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
// while Tests
//==========================================================================

void test_while_basic(void)
{
    // while tests predicate first, runs list if true
    run_string("make \"x 1");
    reset_output();
    run_string("while [:x < 4] [print :x make \"x :x + 1]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_while_never_runs(void)
{
    // while should not run list if predicate is initially false
    run_string("make \"x 10");
    reset_output();
    run_string("while [:x < 5] [print :x]");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

void test_while_with_stop(void)
{
    // stop should exit the while loop
    run_string("make \"x 1");
    reset_output();
    run_string("while [:x < 10] [print :x if :x = 3 [stop] make \"x :x + 1]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_while_invalid_predicate(void)
{
    // while should error if predicate list doesn't output true/false
    Result r = eval_string("while [\"notbool] [print 1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// do.while Tests
//==========================================================================

void test_do_while_basic(void)
{
    // do.while runs list at least once, then checks predicate
    run_string("make \"x 1");
    reset_output();
    run_string("do.while [print :x make \"x :x + 1] [:x < 4]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_do_while_runs_once(void)
{
    // do.while should run list at least once even if predicate is false
    run_string("make \"x 10");
    reset_output();
    run_string("do.while [print :x] [:x < 5]");
    TEST_ASSERT_EQUAL_STRING("10\n", output_buffer);
}

void test_do_while_with_stop(void)
{
    // stop should exit the do.while loop
    run_string("make \"x 1");
    reset_output();
    run_string("do.while [print :x if :x = 3 [stop] make \"x :x + 1] [:x < 10]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_do_while_invalid_predicate(void)
{
    // do.while should error if predicate list doesn't output true/false (after first iteration)
    reset_output();
    Result r = eval_string("do.while [print 1] [\"notbool]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    // Should have printed once before error
    TEST_ASSERT_EQUAL_STRING("1\n", output_buffer);
}

//==========================================================================
// until Tests
//==========================================================================

void test_until_basic(void)
{
    // until tests predicate first, runs list if false, until predicate becomes true
    run_string("make \"x 1");
    reset_output();
    run_string("until [:x > 3] [print :x make \"x :x + 1]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_until_never_runs(void)
{
    // until should not run list if predicate is initially true
    run_string("make \"x 10");
    reset_output();
    run_string("until [:x > 5] [print :x]");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

void test_until_with_stop(void)
{
    // stop should exit the until loop
    run_string("make \"x 1");
    reset_output();
    run_string("until [:x > 10] [print :x if :x = 3 [stop] make \"x :x + 1]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_until_invalid_predicate(void)
{
    // until should error if predicate list doesn't output true/false
    Result r = eval_string("until [\"notbool] [print 1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// do.until Tests
//==========================================================================

void test_do_until_basic(void)
{
    // do.until runs list at least once, then checks predicate, stops when true
    run_string("make \"x 1");
    reset_output();
    run_string("do.until [print :x make \"x :x + 1] [:x > 3]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_do_until_runs_once(void)
{
    // do.until should run list at least once even if predicate is true
    run_string("make \"x 10");
    reset_output();
    run_string("do.until [print :x] [:x > 5]");
    TEST_ASSERT_EQUAL_STRING("10\n", output_buffer);
}

void test_do_until_with_stop(void)
{
    // stop should exit the do.until loop
    run_string("make \"x 1");
    reset_output();
    run_string("do.until [print :x if :x = 3 [stop] make \"x :x + 1] [:x > 10]");
    TEST_ASSERT_EQUAL_STRING("1\n2\n3\n", output_buffer);
}

void test_do_until_invalid_predicate(void)
{
    // do.until should error if predicate list doesn't output true/false (after first iteration)
    reset_output();
    Result r = eval_string("do.until [print 1] [\"notbool]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    // Should have printed once before error
    TEST_ASSERT_EQUAL_STRING("1\n", output_buffer);
}

//==========================================================================
// Comment (;) Tests
//==========================================================================

void test_comment_with_list(void)
{
    // ; with a list should be ignored
    Result r = eval_string("; [This is a comment]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_comment_with_word(void)
{
    // ; with a word should be ignored
    Result r = eval_string("; \"comment");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_comment_in_procedure(void)
{
    // ; should work inside procedures
    // Use define primitive to create a procedure with a comment
    run_string("define \"test.comment [[] [; [comment] print 42]]");
    reset_output();
    run_string("test.comment");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
}

void test_comment_inline(void)
{
    // ; can be used inline with other commands
    run_string("print 1 ; [comment after print]");
    TEST_ASSERT_EQUAL_STRING("1\n", output_buffer);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_repeat);
    RUN_TEST(test_repcount_basic);
    RUN_TEST(test_repcount_no_repeat);
    RUN_TEST(test_repcount_nested);
    RUN_TEST(test_repcount_used_in_expression);
    RUN_TEST(test_forever_with_stop);
    RUN_TEST(test_forever_repcount);
    RUN_TEST(test_forever_repcount_nested_in_repeat);
    RUN_TEST(test_stop);
    RUN_TEST(test_output);
    RUN_TEST(test_run_list);
    RUN_TEST(test_infix_minus_in_list);
    RUN_TEST(test_while_basic);
    RUN_TEST(test_while_never_runs);
    RUN_TEST(test_while_with_stop);
    RUN_TEST(test_while_invalid_predicate);
    RUN_TEST(test_do_while_basic);
    RUN_TEST(test_do_while_runs_once);
    RUN_TEST(test_do_while_with_stop);
    RUN_TEST(test_do_while_invalid_predicate);
    RUN_TEST(test_until_basic);
    RUN_TEST(test_until_never_runs);
    RUN_TEST(test_until_with_stop);
    RUN_TEST(test_until_invalid_predicate);
    RUN_TEST(test_do_until_basic);
    RUN_TEST(test_do_until_runs_once);
    RUN_TEST(test_do_until_with_stop);
    RUN_TEST(test_do_until_invalid_predicate);
    RUN_TEST(test_comment_with_list);
    RUN_TEST(test_comment_with_word);
    RUN_TEST(test_comment_in_procedure);
    RUN_TEST(test_comment_inline);

    return UNITY_END();
}
