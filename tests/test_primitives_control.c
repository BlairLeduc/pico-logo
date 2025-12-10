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

void test_test_iftrue_iffalse(void)
{
    // Test with true predicate
    run_string("test \"true");
    run_string("iftrue [print \"yes]");
    TEST_ASSERT_EQUAL_STRING("yes\n", output_buffer);
    
    reset_output();
    run_string("test \"true");
    run_string("iffalse [print \"no]");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
    
    // Test with false predicate
    reset_output();
    run_string("test \"false");
    run_string("iftrue [print \"yes]");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
    
    reset_output();
    run_string("test \"false");
    run_string("iffalse [print \"no]");
    TEST_ASSERT_EQUAL_STRING("no\n", output_buffer);
}

void test_test_abbreviations(void)
{
    // Test ift abbreviation
    run_string("test \"true");
    run_string("ift [print \"ift_works]");
    TEST_ASSERT_EQUAL_STRING("ift_works\n", output_buffer);
    
    // Test iff abbreviation
    reset_output();
    run_string("test \"false");
    run_string("iff [print \"iff_works]");
    TEST_ASSERT_EQUAL_STRING("iff_works\n", output_buffer);
}

void test_test_in_procedure(void)
{
    // Test that test state is local to procedure scope
    const char *params[] = {"val"};
    define_proc("checktest", params, 1, 
                "test :val iftrue [print \"true] iffalse [print \"false]");
    
    run_string("checktest \"true");
    TEST_ASSERT_EQUAL_STRING("true\n", output_buffer);
    
    reset_output();
    run_string("checktest \"false");
    TEST_ASSERT_EQUAL_STRING("false\n", output_buffer);
}

void test_test_persists_in_scope(void)
{
    // Test result should persist across multiple iftrue/iffalse calls
    run_string("test \"true");
    run_string("iftrue [print \"first]");
    TEST_ASSERT_EQUAL_STRING("first\n", output_buffer);
    
    reset_output();
    run_string("iftrue [print \"second]");
    TEST_ASSERT_EQUAL_STRING("second\n", output_buffer);
}

void test_catch_throw_basic(void)
{
    // Test basic catch/throw
    run_string("catch \"mytag [print \"before throw \"mytag print \"after]");
    TEST_ASSERT_EQUAL_STRING("before\n", output_buffer);
}

void test_catch_throw_nested(void)
{
    // Test that throws propagate through procedures
    const char *params[] = {};
    define_proc("thrower", params, 0, "print \"throwing throw \"test");
    
    reset_output();
    run_string("catch \"test [print \"before thrower print \"after]");
    TEST_ASSERT_EQUAL_STRING("before\nthrowing\n", output_buffer);
}

void test_catch_different_tags(void)
{
    // Test that only matching tags are caught
    reset_output();
    Result r = run_string("catch \"tag1 [throw \"tag2]");
    // Should propagate the throw since tags don't match
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
}

void test_catch_error(void)
{
    // Test that catch "error catches errors
    reset_output();
    run_string("catch \"error [print sum \"a \"b print \"done]");
    // Error should be caught, "done" should not print
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

void test_throw_toplevel(void)
{
    // Test throw "toplevel returns to top level
    const char *params[] = {};
    define_proc("deepproc", params, 0, "print \"deep throw \"toplevel print \"never");
    
    reset_output();
    Result r = run_string("catch \"toplevel [print \"start deepproc print \"after]");
    TEST_ASSERT_EQUAL_STRING("start\ndeep\n", output_buffer);
}

void test_wait(void)
{
    // Test wait primitive - just verify it doesn't error
    // We can't easily test the timing without making tests slow
    Result r = run_string("wait 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_repeat);
    RUN_TEST(test_stop);
    RUN_TEST(test_output);
    RUN_TEST(test_run_list);
    RUN_TEST(test_infix_minus_in_list);
    RUN_TEST(test_test_iftrue_iffalse);
    RUN_TEST(test_test_abbreviations);
    RUN_TEST(test_test_in_procedure);
    RUN_TEST(test_test_persists_in_scope);
    RUN_TEST(test_catch_throw_basic);
    RUN_TEST(test_catch_throw_nested);
    RUN_TEST(test_catch_different_tags);
    RUN_TEST(test_catch_error);
    RUN_TEST(test_throw_toplevel);
    RUN_TEST(test_wait);

    return UNITY_END();
}
