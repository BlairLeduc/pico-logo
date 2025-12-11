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

void test_test_local_to_procedure(void)
{
    // Test state set in a procedure should NOT affect the outer scope
    // after the procedure returns
    
    // Define a procedure that sets test to true using define primitive
    run_string("define \"testproc [[] [test true]]");
    
    // Set test to false at top level
    run_string("test false");
    
    // Call procedure that sets test to true inside it
    run_string("testproc");
    
    // Test state should still be false at top level (procedure's test is local)
    reset_output();
    run_string("iffalse [print \"stillfalse]");
    TEST_ASSERT_EQUAL_STRING("stillfalse\n", output_buffer);
    
    // Clean up
    run_string("erase \"testproc");
}

void test_test_inherited_by_subprocedure(void)
{
    // Test state should be inherited by called procedures
    // (they can see test from caller)
    
    // Define a procedure that checks test state using define primitive
    run_string("define \"checktest [[] [iftrue [print \"yes]] [iffalse [print \"no]]]");
    
    // Set test to true at top level, then call procedure
    run_string("test true");
    reset_output();
    run_string("checktest");
    TEST_ASSERT_EQUAL_STRING("yes\n", output_buffer);
    
    // Set test to false at top level, then call procedure
    run_string("test false");
    reset_output();
    run_string("checktest");
    TEST_ASSERT_EQUAL_STRING("no\n", output_buffer);
    
    // Clean up
    run_string("erase \"checktest");
}

void test_test_nested_procedures(void)
{
    // More complex test: nested procedure calls with different test states
    
    // Define inner procedure that also sets test (to a different value)
    run_string("define \"inner [[] [test false] [iffalse [print \"innerfalse]]]");
    
    // Define outer procedure that sets test and calls inner
    run_string("define \"outer [[] [test true] [inner] [iftrue [print \"outertrue]]]");
    
    // Run outer - outer sets true, calls inner which sets false locally
    // When inner returns, outer should still see its own test=true
    reset_output();
    run_string("outer");
    TEST_ASSERT_EQUAL_STRING("innerfalse\noutertrue\n", output_buffer);
    
    // Clean up
    run_string("erase \"outer");
    run_string("erase \"inner");
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

void test_catch_throw_match(void)
{
    // Catch with matching throw
    Result r = run_string("catch \"mytag [throw \"mytag]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_catch_throw_nomatch(void)
{
    // Catch with non-matching throw should propagate
    Result r = run_string("catch \"othertag [throw \"mytag]");
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
    TEST_ASSERT_EQUAL_STRING("mytag", r.throw_tag);
}

void test_throw_no_catch(void)
{
    // Throw without matching catch should return RESULT_THROW
    Result r = run_string("throw \"mytag");
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
    TEST_ASSERT_EQUAL_STRING("mytag", r.throw_tag);
}

void test_throw_toplevel(void)
{
    // throw "toplevel should work
    Result r = run_string("throw \"toplevel");
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
    TEST_ASSERT_EQUAL_STRING("toplevel", r.throw_tag);
}

void test_catch_error(void)
{
    // catch "error should catch errors
    // Test that an error is caught
    Result r = run_string("catch \"error [sum 1 \"notanumber]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // After catching, error primitive should return error info
    Result err = eval_string("error");
    TEST_ASSERT_EQUAL(RESULT_OK, err.status);
    TEST_ASSERT_TRUE(value_is_list(err.value));
    TEST_ASSERT_FALSE(mem_is_nil(err.value.as.node));
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
// Go/Label Tests
//==========================================================================

void test_label_basic(void)
{
    // label should do nothing
    Result r = run_string("label \"start");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_go_at_toplevel(void)
{
    // go at top level (not in a procedure) should return error
    Result r = run_string("go \"nowhere");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_ONLY_IN_PROCEDURE, r.error_code);
}

void test_go_label_basic_loop(void)
{
    // Basic countdown loop using go/label with global variable
    run_string("make \"n 3");
    run_string("define \"countdown [[] [label \"loop] [if :n < 1 [stop]] [print :n] [make \"n :n - 1] [go \"loop]]");
    reset_output();
    run_string("countdown");
    TEST_ASSERT_EQUAL_STRING("3\n2\n1\n", output_buffer);
    run_string("erase \"countdown");
}

void test_go_label_forward(void)
{
    // Go forward to skip instructions
    run_string("define \"skipper [[] [print \"before] [go \"after] [print \"skipped] [label \"after] [print \"after]]");
    reset_output();
    run_string("skipper");
    TEST_ASSERT_EQUAL_STRING("before\nafter\n", output_buffer);
    run_string("erase \"skipper");
}

void test_go_label_backward(void)
{
    // Go backward to repeat instructions
    run_string("make \"n 3");
    run_string("make \"i 0");
    run_string("define \"counter [[] [label \"loop] [print :i] [make \"i :i + 1] [if :i < :n [go \"loop]]]");
    reset_output();
    run_string("counter");
    TEST_ASSERT_EQUAL_STRING("0\n1\n2\n", output_buffer);
    run_string("erase \"counter");
}

void test_go_label_not_found(void)
{
    // go to non-existent label should return error
    run_string("define \"nolabel [[] [go \"nowhere]]");
    Result r = run_string("nolabel");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_CANT_FIND_LABEL, r.error_code);
    run_string("erase \"nolabel");
}

void test_go_label_multiple_labels(void)
{
    // Test with multiple labels using global variable
    run_string("make \"which 0");
    run_string("define \"multi [[] [if :which = 1 [go \"one]] [if :which = 2 [go \"two]] [print \"zero] [stop] [label \"one] [print \"one] [stop] [label \"two] [print \"two]]");
    
    run_string("make \"which 0");
    reset_output();
    run_string("multi");
    TEST_ASSERT_EQUAL_STRING("zero\n", output_buffer);
    
    run_string("make \"which 1");
    reset_output();
    run_string("multi");
    TEST_ASSERT_EQUAL_STRING("one\n", output_buffer);
    
    run_string("make \"which 2");
    reset_output();
    run_string("multi");
    TEST_ASSERT_EQUAL_STRING("two\n", output_buffer);
    
    run_string("erase \"multi");
}

void test_go_label_with_repeat(void)
{
    // Test go/label inside repeat
    run_string("define \"repeatgo [[] [repeat 2 [print \"before]] [label \"skip] [print \"after]]");
    reset_output();
    run_string("repeatgo");
    // Should print: before, before, after (label is in the outer procedure, not inside repeat)
    TEST_ASSERT_EQUAL_STRING("before\nbefore\nafter\n", output_buffer);
    run_string("erase \"repeatgo");
}

void test_go_label_case_insensitive(void)
{
    // Test that label matching is case-insensitive
    run_string("define \"casetest [[] [go \"LOOP] [label \"loop] [print \"found]]");
    reset_output();
    run_string("casetest");
    TEST_ASSERT_EQUAL_STRING("found\n", output_buffer);
    run_string("erase \"casetest");
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
    RUN_TEST(test_test_local_to_procedure);
    RUN_TEST(test_test_inherited_by_subprocedure);
    RUN_TEST(test_test_nested_procedures);
    
    // Wait
    RUN_TEST(test_wait);
    
    // Catch/throw/error
    RUN_TEST(test_catch_basic);
    RUN_TEST(test_catch_throw_match);
    RUN_TEST(test_catch_throw_nomatch);
    RUN_TEST(test_throw_no_catch);
    RUN_TEST(test_throw_toplevel);
    RUN_TEST(test_catch_error);
    RUN_TEST(test_error_no_error);
    
    // Go/label
    RUN_TEST(test_label_basic);
    RUN_TEST(test_go_at_toplevel);
    RUN_TEST(test_go_label_basic_loop);
    RUN_TEST(test_go_label_forward);
    RUN_TEST(test_go_label_backward);
    RUN_TEST(test_go_label_not_found);
    RUN_TEST(test_go_label_multiple_labels);
    RUN_TEST(test_go_label_with_repeat);
    RUN_TEST(test_go_label_case_insensitive);

    return UNITY_END();
}
