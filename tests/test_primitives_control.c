//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include "core/error.h"

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
    // if with stop inside should propagate stop
    Result r = run_string("if true [stop]");
    TEST_ASSERT_EQUAL(RESULT_STOP, r.status);
}

void test_if_list_with_output(void)
{
    // if with output inside should propagate output
    Result r = eval_string("if true [output 42]");
    TEST_ASSERT_EQUAL(RESULT_OUTPUT, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
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
// User Interrupt Tests
//==========================================================================

void test_user_interrupt_stops_evaluation(void)
{
    // Set the user interrupt flag before evaluating
    mock_user_interrupt = true;
    
    // Try to run something - should be stopped immediately
    // Use run_string which calls eval_instruction where the check happens
    Result r = run_string("print 42");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_STOPPED, r.error_code);
    
    // Output should be empty since we stopped before executing
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
    
    // Flag should be cleared after check
    TEST_ASSERT_FALSE(mock_user_interrupt);
}

void test_user_interrupt_stops_repeat(void)
{
    // This tests that user interrupt stops a repeat loop
    // We can't easily test mid-loop interruption without threading,
    // but we can test that checking happens
    
    // Run a repeat without interrupt first - should complete
    run_string("repeat 3 [print 1]");
    TEST_ASSERT_EQUAL_STRING("1\n1\n1\n", output_buffer);
}

void test_pause_request_triggers_pause_in_procedure(void)
{
    // Define a procedure that will be paused by F9
    proc_define_from_text("to pauseme\nprint 1\nprint 2\nend");
    
    // Set mock input to simulate user typing "co" in the pause REPL
    set_mock_input("co\n");
    
    // Set the pause request flag before evaluating
    mock_pause_requested = true;
    
    // Run the procedure - should pause then continue after co
    run_string("pauseme");
    
    // Should see "Pausing..." then continue after co
    TEST_ASSERT_TRUE(strstr(output_buffer, "Pausing...") != NULL);
    // Should complete after co
    TEST_ASSERT_TRUE(strstr(output_buffer, "1") != NULL);
    
    // Flag should be cleared after check
    TEST_ASSERT_FALSE(mock_pause_requested);
}

void test_pause_request_ignored_at_toplevel(void)
{
    // Set the pause request flag at top level (no procedure running)
    mock_pause_requested = true;
    
    // Run something at top level - pause should be ignored
    run_string("print 42");
    
    // Should execute normally (F9 only works inside procedures)
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
    
    // Flag should still be set since we didn't enter a procedure
    // (it will be consumed next time we're inside a procedure)
    TEST_ASSERT_TRUE(mock_pause_requested);
    
    // Clean up
    mock_pause_requested = false;
}

void test_freeze_request_waits_for_key(void)
{
    // Define a procedure
    proc_define_from_text("to freezeme\nprint 1\nprint 2\nend");
    
    // Set mock input to provide a key to continue after freeze
    set_mock_input("x");
    
    // Set the freeze request flag
    mock_freeze_requested = true;
    
    // Run the procedure - should freeze briefly then continue after key
    run_string("freezeme");
    
    // Should complete normally after key was pressed
    TEST_ASSERT_TRUE(strstr(output_buffer, "1") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "2") != NULL);
    
    // Flag should be cleared
    TEST_ASSERT_FALSE(mock_freeze_requested);
}

void test_freeze_request_break_stops_execution(void)
{
    // Define a procedure
    proc_define_from_text("to freezeme2\nprint 1\nprint 2\nend");
    
    // Set the freeze request flag
    mock_freeze_requested = true;
    
    // Set user interrupt to simulate Brk during freeze
    mock_user_interrupt = true;
    
    // Run the procedure - should stop due to Brk
    Result r = run_string("freezeme2");
    
    // Should have stopped
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_STOPPED, r.error_code);
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

void test_throw_toplevel_in_run_inside_catch(void)
{
    // throw "toplevel inside a catch should propagate to top level
    // even if there's a catch with a different tag
    run_string("define \"inner [[] [run [throw \"toplevel]]");
    run_string("define \"outer [[] [catch \"error [inner]]");

    Result r = run_string("outer");
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

    // The error list should be:
    // [41 <formatted-error-message> sum []]
    // Where <formatted-error-message> is the error message with arguments filled in
    Node list = err.value.as.node;
    
    // First element: error code (41 = ERR_DOESNT_LIKE_INPUT)
    Node first = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_word(first));
    float error_code;
    TEST_ASSERT_TRUE(value_to_number(value_word(first), &error_code));
    TEST_ASSERT_EQUAL_FLOAT(ERR_DOESNT_LIKE_INPUT, error_code);
    
    // Second element: formatted error message (word)
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));
    Node second = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_word(second));
    const char *message = mem_word_ptr(second);
    TEST_ASSERT_NOT_NULL(message);
    // The message is a template like "%s doesn't like %s as input"
    TEST_ASSERT_EQUAL_STRING("sum doesn't like notanumber as input", message);
    
    // Third element: primitive name ("sum")
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));
    Node third = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_word(third));
    TEST_ASSERT_EQUAL_STRING("sum", mem_word_ptr(third));
    
    // Fourth element: caller procedure (empty list since at top level)
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));
    Node fourth = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_nil(fourth));  // Empty list (NODE_NIL)
    
    // Should be end of list
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(list)));
}

void test_error_no_error(void)
{
    // error should return empty list if no error occurred
    Result r = eval_string("error");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_catch_through_calls_good(void)
{
    // Test that catch works through nested procedure calls
    run_string("define \"tc [[in] [catch \"oops [trythis :in]]]");
    run_string("define \"trythis [[n] [pr check :n pr \"good]]");
    run_string("define \"check [[num] [if :num = 0 [throw \"oops] op :num]]");

    // Run catch around outerproc
    Result r = run_string("tc 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "1\ngood\n"));
    
    // Clean up
    run_string("erase \"tc");
    run_string("erase \"trythis");
    run_string("erase \"check");
}

void test_catch_through_calls_catch(void)
{
    // Test that catch works through nested procedure calls
    run_string("define \"tc [[in] [catch \"oops [trythis :in]]]");
    run_string("define \"trythis [[n] [pr check :n pr \"good]]");
    run_string("define \"check [[num] [if :num = 0 [throw \"oops] op :num]]");

    // Run catch around outerproc
    Result r = run_string("tc 0");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Clean up
    run_string("erase \"tc");
    run_string("erase \"trythis");
    run_string("erase \"check");
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
    // go outside a procedure should return error
    Result r = run_string("go \"nowhere");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_ONLY_IN_PROCEDURE, r.error_code);
}

void test_go_with_label(void)
{
    Result def = proc_define_from_text(
        "to countdown :n\n"
        "label \"loop\n"
        "if :n < 0 [stop]\n"
        "print :n\n"
        "make \"n :n - 1\n"
        "go \"loop\n"
        "end\n");
    TEST_ASSERT_EQUAL(RESULT_OK, def.status);
    reset_output();

    Result r = run_string("countdown 3");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("3\n2\n1\n0\n", output_buffer);
}

void test_go_label_not_found_in_procedure(void)
{
    // go to a label that doesn't exist inside a procedure
    Result def = proc_define_from_text(
        "to missinglabel\n"
        "go \"nothere\n"
        "end\n");
    TEST_ASSERT_EQUAL(RESULT_OK, def.status);

    Result r = run_string("missinglabel");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_CANT_FIND_LABEL, r.error_code);
    // Verify the error message includes the label name
    TEST_ASSERT_EQUAL_STRING("nothere", r.error_arg);
}

//==========================================================================
// Pause/Continue Tests
//==========================================================================

void test_pause_at_toplevel_error(void)
{
    // pause at top level should return error
    Result r = run_string("pause");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_AT_TOPLEVEL, r.error_code);
}

void test_co_at_toplevel(void)
{
    // co at top level should do nothing (no pause to continue)
    Result r = run_string("co");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_pause_in_procedure_with_co(void)
{
    // Define a procedure that pauses using proc_define_from_text
    Result def = proc_define_from_text("to testproc :x \\n print :x \\n pause \\n print :x + 1 \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, def.status);
    reset_output();
    
    // Set up input: "co\n" to continue immediately after pause
    set_mock_input("co\n");
    
    // Run the procedure
    Result r = run_string("testproc 5");
    
    // Should complete normally (RESULT_NONE)
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Should print "5" then "Pausing..." then "6"
    // The Pausing... message and prompt are also written to output
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "5\n"));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "Pausing..."));
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "6\n"));
}

void test_pause_can_inspect_local_variables(void)
{
    // Define a procedure that pauses
    Result def = proc_define_from_text("to testproc :val \\n pause \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, def.status);
    reset_output();
    
    // Set up input: print the local variable, then continue
    set_mock_input("print :val\nco\n");
    
    // Run the procedure
    Result r = run_string("testproc 42");
    
    // Should complete normally
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Output should contain "42" (the value of :val)
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "42"));
}

void test_pause_prompt_shows_procedure_name(void)
{
    // Define a procedure that pauses
    Result def = proc_define_from_text("to myproc \\n pause \\n end");
    TEST_ASSERT_EQUAL(RESULT_OK, def.status);
    reset_output();
    
    // Set up input: continue
    set_mock_input("co\n");
    
    // Run the procedure
    Result r = run_string("myproc");
    
    // Should complete normally
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Prompt should contain "myproc?"
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "myproc?"));
}

void test_pause_throw_toplevel_exits(void)
{
    // Define a procedure that pauses
    Result def = proc_define_from_text("to testpause print \"before pause print \"after end");
    TEST_ASSERT_EQUAL(RESULT_OK, def.status);
    reset_output();
    
    // Set up input: throw "toplevel to exit pause
    set_mock_input("throw \"toplevel\n");
    
    // Run the procedure
    Result r = run_string("testpause");
    
    // Should return throw result
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
    TEST_ASSERT_EQUAL_STRING("toplevel", r.throw_tag);
    
    // Should have printed "before" but not "after"
    TEST_ASSERT_NOT_NULL(strstr(output_buffer, "before"));
    TEST_ASSERT_NULL(strstr(output_buffer, "after\n"));
}

int main(void)
{
    UNITY_BEGIN();

    // Existing tests
    RUN_TEST(test_repeat);
    RUN_TEST(test_repcount_basic);
    RUN_TEST(test_repcount_no_repeat);
    RUN_TEST(test_repcount_nested);
    RUN_TEST(test_repcount_used_in_expression);
    RUN_TEST(test_stop);
    RUN_TEST(test_output);
    RUN_TEST(test_run_list);
    RUN_TEST(test_infix_minus_in_list);
    
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
    RUN_TEST(test_if_number_predicate_error);
    RUN_TEST(test_if_list_predicate_error);
    RUN_TEST(test_if_non_list_body_error);
    RUN_TEST(test_if_non_list_else_body_error);
    RUN_TEST(test_if_true_case_insensitive);
    RUN_TEST(test_if_false_case_insensitive);
    
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
    
    // User interrupt
    RUN_TEST(test_user_interrupt_stops_evaluation);
    RUN_TEST(test_user_interrupt_stops_repeat);
    
    // F9 pause request
    RUN_TEST(test_pause_request_triggers_pause_in_procedure);
    RUN_TEST(test_pause_request_ignored_at_toplevel);
    
    // F4 freeze request
    RUN_TEST(test_freeze_request_waits_for_key);
    RUN_TEST(test_freeze_request_break_stops_execution);
    
    // Catch/throw/error
    RUN_TEST(test_catch_basic);
    RUN_TEST(test_catch_throw_match);
    RUN_TEST(test_catch_throw_nomatch);
    RUN_TEST(test_throw_no_catch);
    RUN_TEST(test_throw_toplevel);
    RUN_TEST(test_throw_toplevel_in_run_inside_catch);
    RUN_TEST(test_catch_error);
    RUN_TEST(test_error_no_error);
    RUN_TEST(test_catch_through_calls_good);
    RUN_TEST(test_catch_through_calls_catch);
    
    // Go/label
    RUN_TEST(test_label_basic);
    RUN_TEST(test_go_no_label);
    RUN_TEST(test_go_with_label);
    RUN_TEST(test_go_label_not_found_in_procedure);
    
    // Pause/continue
    RUN_TEST(test_pause_at_toplevel_error);
    RUN_TEST(test_co_at_toplevel);
    RUN_TEST(test_pause_in_procedure_with_co);
    RUN_TEST(test_pause_can_inspect_local_variables);
    RUN_TEST(test_pause_prompt_shows_procedure_name);
    RUN_TEST(test_pause_throw_toplevel_exits);

    return UNITY_END();
}
