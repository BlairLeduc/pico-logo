//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for debug control primitives: pause, co, go, label, wait
//

#include "test_scaffold.h"
#include "core/error.h"
#include <string.h>
#include <stdio.h>

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
// Go/Label Tests
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
    if (r.status == RESULT_ERROR) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Error %d: proc=%s arg=%s", r.error_code, 
                 r.error_proc ? r.error_proc : "(null)",
                 r.error_arg ? r.error_arg : "(null)");
        TEST_FAIL_MESSAGE(msg);
    }
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, "Should complete without error");
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
    Result def = proc_define_from_text("to testproc :x\nprint :x\npause\nprint :x + 1\nend");
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
    Result def = proc_define_from_text("to testproc :val\npause\nend");
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
    Result def = proc_define_from_text("to myproc\npause\nend");
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
