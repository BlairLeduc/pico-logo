//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for conditional primitives: if, true, false, test, iftrue, iffalse
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
// IF Command/Operation Tests
//==========================================================================

// --- IF as a command (one list) ---

void test_if_true_one_list_command(void)
{
    // if true [print "yes] - should print "yes"
    run_string("if true [print \"yes]");
    TEST_ASSERT_EQUAL_STRING("yes\n", output_buffer);
}

void test_if_false_one_list_command(void)
{
    // if false [print "yes] - should do nothing
    run_string("if false [print \"yes]");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

void test_if_with_expression_predicate(void)
{
    // if 5 > 3 [print "greater]
    run_string("if 5 > 3 [print \"greater]");
    TEST_ASSERT_EQUAL_STRING("greater\n", output_buffer);
}

void test_if_with_equal_expression(void)
{
    // if 5 = 5 [print "equal]
    run_string("if 5 = 5 [print \"equal]");
    TEST_ASSERT_EQUAL_STRING("equal\n", output_buffer);
}

void test_if_with_less_than_expression(void)
{
    // if 3 < 5 [print "less]
    run_string("if 3 < 5 [print \"less]");
    TEST_ASSERT_EQUAL_STRING("less\n", output_buffer);
}

// --- IF as a command (two lists using parentheses) ---

void test_if_true_two_lists_command(void)
{
    // (if true [print "yes] [print "no]) - should print "yes"
    run_string("(if true [print \"yes] [print \"no])");
    TEST_ASSERT_EQUAL_STRING("yes\n", output_buffer);
}

void test_if_false_two_lists_command(void)
{
    // (if false [print "yes] [print "no]) - should print "no"
    run_string("(if false [print \"yes] [print \"no])");
    TEST_ASSERT_EQUAL_STRING("no\n", output_buffer);
}

void test_if_two_lists_with_expression(void)
{
    // (if 2 > 5 [print "greater] [print "notgreater]) - should print "notgreater"
    run_string("(if 2 > 5 [print \"greater] [print \"notgreater])");
    TEST_ASSERT_EQUAL_STRING("notgreater\n", output_buffer);
}

// --- IF as an operation ---

void test_if_true_operation_returns_value(void)
{
    // (if true ["yes] ["no]) - should output "yes"
    Result r = eval_string("(if true [\"yes] [\"no])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("yes", value_to_string(r.value));
}

void test_if_false_operation_returns_value(void)
{
    // (if false ["yes] ["no]) - should output "no"
    Result r = eval_string("(if false [\"yes] [\"no])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("no", value_to_string(r.value));
}

void test_if_operation_with_arithmetic(void)
{
    // (if true [sum 1 2] [sum 3 4]) - should output 3
    Result r = eval_string("(if true [sum 1 2] [sum 3 4])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, r.value.as.number);
}

void test_if_operation_false_with_arithmetic(void)
{
    // (if false [sum 1 2] [sum 3 4]) - should output 7
    Result r = eval_string("(if false [sum 1 2] [sum 3 4])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
}

void test_if_operation_used_in_print(void)
{
    // print (if true ["hello] ["goodbye])
    run_string("print (if true [\"hello] [\"goodbye])");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_if_operation_used_in_expression(void)
{
    // sum 10 (if true [5] [0]) - should output 15
    Result r = eval_string("sum 10 (if true [5] [0])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, r.value.as.number);
}

void test_if_operation_nested(void)
{
    // (if true [(if false ["inner_yes] ["inner_no])] ["outer_no])
    Result r = eval_string("(if true [(if false [\"inner_yes] [\"inner_no])] [\"outer_no])");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("inner_no", value_to_string(r.value));
}

// --- IF with stop/output in lists ---

void test_if_list_with_stop(void)
{
    // if with stop inside a procedure should propagate stop
    run_string("define \"teststop [[] [if true [stop] print \"should_not_print]]");
    Result r = run_string("teststop");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
    run_string("erase \"teststop");
}

void test_if_list_with_output(void)
{
    // if with output inside a procedure should propagate output
    run_string("define \"testoutput [[] [if true [output 42]]]");
    Result r = run_string("print testoutput");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
    run_string("erase \"testoutput");
}

void test_output_with_recursive_call_in_if(void)
{
    // Test run list inside procedure - verifies variables are accessible
    // when executing a nested list in a procedure body
    Result r = run_string("define \"myproc2 [[:x] [run [print :x]]]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    r = run_string("myproc2 \"hello");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
    reset_output();
    
    // Clean up
    run_string("erase \"myproc2");
}

void test_output_in_recursive_procedure(void)
{
    // This test mimics the pig latin case: output inside if inside recursive procedure
    // to countdown :n
    //   if :n = 0 [output "done]
    //   print :n
    //   output countdown :n - 1
    // end
    Result r = run_string("define \"countdown [[n] [(if :n = 0 [output \"done]) print :n output countdown :n - 1]]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    r = run_string("print countdown 3");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("3\n2\n1\ndone\n", output_buffer);
    reset_output();
    
    // Clean up
    run_string("erase \"countdown");
}

void test_output_in_pig_latin_procedure(void)
{
    // Test output inside pig latin procedure
    Result r = run_string(
        "define \"pig [[word] [\n"
        "  if member? first :word [a e i o u y] [op word :word \"ay]\n"
        "  op pig word bf :word first :word\n"
        "]]\n\n"
        "define \"latin [[sent] [\n"
        "  if empty? :sent [ op [ ] ]\n"
        "  op se pig first :sent latin bf :sent\n"
        "]]"
    );
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    r = run_string("print latin [no pigs]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("onay igspay\n", output_buffer);
    reset_output();
    
    // Clean up
    run_string("erase \"pig");
    run_string("erase \"latin");
}

// --- IF error cases ---

void test_if_list_with_print_empty_then_stop(void)
{
    // Bug: if empty? :thing [pr [] stop] was printing "stop" instead of
    // printing an empty list and then stopping. The empty list [] in the
    // instruction list was not being handled as a pre-parsed sublist,
    // causing parse_list to consume "stop" as part of the list.
    // Must be tested inside a procedure since stop requires procedure context.
    run_string("make \"thing []");
    run_string("define \"teststop2 [[] [if empty? :thing [pr [] stop] print \"after]]");
    Result r = run_string("teststop2");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("\n", output_buffer);
    run_string("erase \"teststop2");
}

void test_if_list_with_empty_list_arg(void)
{
    // Verify that an empty list literal works as a primitive argument
    // inside an instruction list evaluated by if
    run_string("if true [print count []]");
    TEST_ASSERT_EQUAL_STRING("0\n", output_buffer);
}

void test_if_non_boolean_predicate_error(void)
{
    // if with non-boolean predicate should error
    Result r = run_string("if \"notabool [print \"test]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, r.error_code);
}

void test_if_number_predicate_error(void)
{
    // if with number predicate should error
    Result r = run_string("if 42 [print \"test]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, r.error_code);
}

void test_if_list_predicate_error(void)
{
    // if with list predicate should error
    Result r = run_string("if [a b c] [print \"test]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, r.error_code);
}

void test_if_non_list_body_error(void)
{
    // if with non-list body should error
    Result r = run_string("if true \"notalist");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_if_non_list_else_body_error(void)
{
    // (if predicate list1 non-list) should error when else branch is taken
    Result r = run_string("(if false [print \"test] \"notalist)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// IFELSE Command/Operation Tests
//==========================================================================

void test_ifelse_true_command(void)
{
    run_string("ifelse true [print \"yes] [print \"no]");
    TEST_ASSERT_EQUAL_STRING("yes\n", output_buffer);
}

void test_ifelse_false_command(void)
{
    run_string("ifelse false [print \"yes] [print \"no]");
    TEST_ASSERT_EQUAL_STRING("no\n", output_buffer);
}

void test_ifelse_with_expression_predicate(void)
{
    run_string("ifelse 5 > 3 [print \"greater] [print \"notgreater]");
    TEST_ASSERT_EQUAL_STRING("greater\n", output_buffer);
}

void test_ifelse_false_with_expression_predicate(void)
{
    run_string("ifelse 2 > 5 [print \"greater] [print \"notgreater]");
    TEST_ASSERT_EQUAL_STRING("notgreater\n", output_buffer);
}

void test_ifelse_true_operation_returns_value(void)
{
    Result r = eval_string("ifelse true [\"yes] [\"no]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("yes", value_to_string(r.value));
}

void test_ifelse_false_operation_returns_value(void)
{
    Result r = eval_string("ifelse false [\"yes] [\"no]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("no", value_to_string(r.value));
}

void test_ifelse_operation_with_arithmetic(void)
{
    Result r = eval_string("ifelse true [sum 1 2] [sum 3 4]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, r.value.as.number);
}

void test_ifelse_operation_false_with_arithmetic(void)
{
    Result r = eval_string("ifelse false [sum 1 2] [sum 3 4]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, r.value.as.number);
}

void test_ifelse_used_in_print(void)
{
    run_string("print ifelse true [\"hello] [\"goodbye]");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_ifelse_nested(void)
{
    Result r = eval_string("ifelse true [ifelse false [\"inner_yes] [\"inner_no]] [\"outer_no]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("inner_no", value_to_string(r.value));
}

void test_ifelse_non_boolean_predicate_error(void)
{
    Result r = run_string("ifelse \"notabool [print \"yes] [print \"no]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, r.error_code);
}

void test_ifelse_non_list_true_body_error(void)
{
    Result r = run_string("ifelse true \"notalist [print \"no]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_ifelse_non_list_false_body_error(void)
{
    Result r = run_string("ifelse false [print \"yes] \"notalist");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_ifelse_in_procedure_with_output(void)
{
    run_string("define \"decide [[] [output ifelse 0 = random 1 [\"yes] [\"no]]]");
    Result r = eval_string("decide");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    // random 1 always returns 0, so 0 = 0 is true
    TEST_ASSERT_EQUAL_STRING("yes", value_to_string(r.value));
    run_string("erase \"decide");
}

// --- IF case insensitivity ---

void test_if_true_case_insensitive(void)
{
    // TRUE, True, true should all work
    run_string("if \"TRUE [print \"yes]");
    TEST_ASSERT_EQUAL_STRING("yes\n", output_buffer);
    
    reset_output();
    run_string("if \"True [print \"yes]");
    TEST_ASSERT_EQUAL_STRING("yes\n", output_buffer);
}

void test_if_false_case_insensitive(void)
{
    // FALSE, False, false should all work
    run_string("(if \"FALSE [print \"yes] [print \"no])");
    TEST_ASSERT_EQUAL_STRING("no\n", output_buffer);
    
    reset_output();
    run_string("(if \"False [print \"yes] [print \"no])");
    TEST_ASSERT_EQUAL_STRING("no\n", output_buffer);
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

int main(void)
{
    UNITY_BEGIN();

    // Boolean operations
    RUN_TEST(test_true);
    RUN_TEST(test_false);
    
    // IF command/operation tests
    RUN_TEST(test_if_true_one_list_command);
    RUN_TEST(test_if_false_one_list_command);
    RUN_TEST(test_if_with_expression_predicate);
    RUN_TEST(test_if_with_equal_expression);
    RUN_TEST(test_if_with_less_than_expression);
    RUN_TEST(test_if_true_two_lists_command);
    RUN_TEST(test_if_false_two_lists_command);
    RUN_TEST(test_if_two_lists_with_expression);
    RUN_TEST(test_if_true_operation_returns_value);
    RUN_TEST(test_if_false_operation_returns_value);
    RUN_TEST(test_if_operation_with_arithmetic);
    RUN_TEST(test_if_operation_false_with_arithmetic);
    RUN_TEST(test_if_operation_used_in_print);
    RUN_TEST(test_if_operation_used_in_expression);
    RUN_TEST(test_if_operation_nested);
    RUN_TEST(test_if_list_with_stop);
    RUN_TEST(test_if_list_with_output);
    RUN_TEST(test_output_with_recursive_call_in_if);
    RUN_TEST(test_output_in_recursive_procedure);
    RUN_TEST(test_output_in_pig_latin_procedure);
    RUN_TEST(test_if_list_with_print_empty_then_stop);
    RUN_TEST(test_if_list_with_empty_list_arg);
    RUN_TEST(test_if_non_boolean_predicate_error);
    RUN_TEST(test_if_number_predicate_error);
    RUN_TEST(test_if_list_predicate_error);
    RUN_TEST(test_if_non_list_body_error);
    RUN_TEST(test_if_non_list_else_body_error);
    RUN_TEST(test_if_true_case_insensitive);
    RUN_TEST(test_if_false_case_insensitive);
    
    // IFELSE command/operation tests
    RUN_TEST(test_ifelse_true_command);
    RUN_TEST(test_ifelse_false_command);
    RUN_TEST(test_ifelse_with_expression_predicate);
    RUN_TEST(test_ifelse_false_with_expression_predicate);
    RUN_TEST(test_ifelse_true_operation_returns_value);
    RUN_TEST(test_ifelse_false_operation_returns_value);
    RUN_TEST(test_ifelse_operation_with_arithmetic);
    RUN_TEST(test_ifelse_operation_false_with_arithmetic);
    RUN_TEST(test_ifelse_used_in_print);
    RUN_TEST(test_ifelse_nested);
    RUN_TEST(test_ifelse_non_boolean_predicate_error);
    RUN_TEST(test_ifelse_non_list_true_body_error);
    RUN_TEST(test_ifelse_non_list_false_body_error);
    RUN_TEST(test_ifelse_in_procedure_with_output);

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

    return UNITY_END();
}
