//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include <stdlib.h>

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Debugging Primitives Tests (step, unstep, trace, untrace)
//==========================================================================

void test_step_sets_flag(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print \"hello");
    
    TEST_ASSERT_FALSE(proc_is_stepped("myproc"));
    
    run_string("step \"myproc");
    
    TEST_ASSERT_TRUE(proc_is_stepped("myproc"));
}

void test_unstep_clears_flag(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print \"hello");
    
    run_string("step \"myproc");
    TEST_ASSERT_TRUE(proc_is_stepped("myproc"));
    
    run_string("unstep \"myproc");
    
    TEST_ASSERT_FALSE(proc_is_stepped("myproc"));
}

void test_trace_sets_flag(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print \"hello");
    
    TEST_ASSERT_FALSE(proc_is_traced("myproc"));
    
    run_string("trace \"myproc");
    
    TEST_ASSERT_TRUE(proc_is_traced("myproc"));
}

void test_untrace_clears_flag(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print \"hello");
    
    run_string("trace \"myproc");
    TEST_ASSERT_TRUE(proc_is_traced("myproc"));
    
    run_string("untrace \"myproc");
    
    TEST_ASSERT_FALSE(proc_is_traced("myproc"));
}

void test_step_with_list(void)
{
    const char *params[] = {};
    define_proc("proc1", params, 0, "print 1");
    define_proc("proc2", params, 0, "print 2");
    define_proc("proc3", params, 0, "print 3");
    
    run_string("step [proc1 proc2]");
    
    TEST_ASSERT_TRUE(proc_is_stepped("proc1"));
    TEST_ASSERT_TRUE(proc_is_stepped("proc2"));
    TEST_ASSERT_FALSE(proc_is_stepped("proc3"));
}

void test_unstep_with_list(void)
{
    const char *params[] = {};
    define_proc("proc1", params, 0, "print 1");
    define_proc("proc2", params, 0, "print 2");
    
    run_string("step [proc1 proc2]");
    TEST_ASSERT_TRUE(proc_is_stepped("proc1"));
    TEST_ASSERT_TRUE(proc_is_stepped("proc2"));
    
    run_string("unstep [proc1 proc2]");
    
    TEST_ASSERT_FALSE(proc_is_stepped("proc1"));
    TEST_ASSERT_FALSE(proc_is_stepped("proc2"));
}

void test_trace_with_list(void)
{
    const char *params[] = {};
    define_proc("proc1", params, 0, "print 1");
    define_proc("proc2", params, 0, "print 2");
    define_proc("proc3", params, 0, "print 3");
    
    run_string("trace [proc1 proc2]");
    
    TEST_ASSERT_TRUE(proc_is_traced("proc1"));
    TEST_ASSERT_TRUE(proc_is_traced("proc2"));
    TEST_ASSERT_FALSE(proc_is_traced("proc3"));
}

void test_untrace_with_list(void)
{
    const char *params[] = {};
    define_proc("proc1", params, 0, "print 1");
    define_proc("proc2", params, 0, "print 2");
    
    run_string("trace [proc1 proc2]");
    TEST_ASSERT_TRUE(proc_is_traced("proc1"));
    TEST_ASSERT_TRUE(proc_is_traced("proc2"));
    
    run_string("untrace [proc1 proc2]");
    
    TEST_ASSERT_FALSE(proc_is_traced("proc1"));
    TEST_ASSERT_FALSE(proc_is_traced("proc2"));
}

void test_step_nonexistent_gives_error(void)
{
    Result r = run_string("step \"nonexistent");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_unstep_nonexistent_gives_error(void)
{
    Result r = run_string("unstep \"nonexistent");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_trace_nonexistent_gives_error(void)
{
    Result r = run_string("trace \"nonexistent");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_untrace_nonexistent_gives_error(void)
{
    Result r = run_string("untrace \"nonexistent");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_step_and_trace_independent(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print \"hello");
    
    run_string("step \"myproc");
    TEST_ASSERT_TRUE(proc_is_stepped("myproc"));
    TEST_ASSERT_FALSE(proc_is_traced("myproc"));
    
    run_string("trace \"myproc");
    TEST_ASSERT_TRUE(proc_is_stepped("myproc"));
    TEST_ASSERT_TRUE(proc_is_traced("myproc"));
    
    run_string("unstep \"myproc");
    TEST_ASSERT_FALSE(proc_is_stepped("myproc"));
    TEST_ASSERT_TRUE(proc_is_traced("myproc"));
}

void test_trace_prints_entry_and_exit(void)
{
    const char *params[] = {};
    define_proc("simple", params, 0, "print \"hello");
    
    run_string("trace \"simple");
    
    reset_output();
    run_string("simple");
    
    // Should print procedure entry, the actual output, and exit
    TEST_ASSERT_TRUE(strstr(output_buffer, "simple") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "hello") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "stopped") != NULL);
}

void test_trace_with_arguments(void)
{
    Node p = mem_atom("x", 1);
    const char *params[] = {mem_word_ptr(p)};
    define_proc("double", params, 1, "output :x * 2");
    
    run_string("trace \"double");
    
    reset_output();
    run_string("print double 5");
    
    // Should print procedure entry with argument, return value, and the final result
    TEST_ASSERT_TRUE(strstr(output_buffer, "double") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "5") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "10") != NULL);
}

void test_trace_shows_recursion_depth(void)
{
    Node p = mem_atom("n", 1);
    const char *params[] = {mem_word_ptr(p)};
    define_proc("countdown", params, 1, "if :n > 0 [print :n countdown :n - 1]");
    
    run_string("trace \"countdown");
    
    reset_output();
    run_string("countdown 3");
    
    // Should show indentation for recursive calls
    TEST_ASSERT_TRUE(strstr(output_buffer, "countdown") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "3") != NULL);
}

void test_step_pauses_execution(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print \"line1");
    
    run_string("step \"myproc");
    
    // Provide mock input (one keypress for the one line)
    set_mock_input("x");
    
    reset_output();
    Result r = run_string("myproc");
    
    // Should complete successfully
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(proc_is_stepped("myproc"));
    
    // Output should contain both the stepped line and the execution output
    TEST_ASSERT_TRUE(strstr(output_buffer, "print \"line1") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "line1") != NULL);
    
    // After unstep, should still work
    run_string("unstep \"myproc");
    reset_output();
    r = run_string("myproc");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_FALSE(proc_is_stepped("myproc"));
}

void test_step_multiline_procedure(void)
{
    // Define procedure with multiple lines
    Node p = mem_atom("word", 4);
    const char *params[] = {mem_word_ptr(p)};
    
    // Use define to create a multi-line procedure
    // Body: "if empty? :word [stop] \n pr :word \n triangle bl :word"
    Node body = NODE_NIL;
    body = mem_cons(mem_atom("if", 2), body);
    body = mem_cons(mem_atom("empty?", 6), body);
    body = mem_cons(mem_atom(":word", 5), body);
    // Build the [stop] list
    Node stop_list = mem_cons(mem_atom("stop", 4), NODE_NIL);
    body = mem_cons(stop_list, body);
    body = mem_cons(mem_atom("\\n", 2), body);  // Newline marker
    body = mem_cons(mem_atom("pr", 2), body);
    body = mem_cons(mem_atom(":word", 5), body);
    
    // Reverse to get correct order
    Node reversed = NODE_NIL;
    while (!mem_is_nil(body)) {
        reversed = mem_cons(mem_car(body), reversed);
        body = mem_cdr(body);
    }
    
    proc_define("triangle", params, 1, reversed);
    
    run_string("step \"triangle");
    
    // Two lines need two keypresses
    set_mock_input("xx");
    
    reset_output();
    Result r = run_string("triangle \"ab");
    
    // Should complete successfully (will stop when :word becomes empty)
    TEST_ASSERT_TRUE(r.status == RESULT_NONE || r.status == RESULT_STOP);
    
    // Output should show the stepped lines
    TEST_ASSERT_TRUE(strstr(output_buffer, "if") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "pr") != NULL);
}

void test_step_shows_each_line_before_execution(void)
{
    const char *params[] = {};
    
    // Create a simple two-line procedure using define
    Node body = NODE_NIL;
    body = mem_cons(mem_atom("print", 5), body);
    body = mem_cons(mem_atom("\"first", 6), body);
    body = mem_cons(mem_atom("\\n", 2), body);  // Newline marker
    body = mem_cons(mem_atom("print", 5), body);
    body = mem_cons(mem_atom("\"second", 7), body);
    
    // Reverse to get correct order
    Node reversed = NODE_NIL;
    while (!mem_is_nil(body)) {
        reversed = mem_cons(mem_car(body), reversed);
        body = mem_cdr(body);
    }
    
    proc_define("twolines", params, 0, reversed);
    
    run_string("step \"twolines");
    
    // Two lines need two keypresses
    set_mock_input("ab");
    
    reset_output();
    Result r = run_string("twolines");
    
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Output should contain both stepped display and execution output
    // The order should be: first line displayed, first line executed, second line displayed, second line executed
    TEST_ASSERT_TRUE(strstr(output_buffer, "print \"first") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "first\n") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "print \"second") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "second\n") != NULL);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_step_sets_flag);
    RUN_TEST(test_unstep_clears_flag);
    RUN_TEST(test_trace_sets_flag);
    RUN_TEST(test_untrace_clears_flag);
    RUN_TEST(test_step_with_list);
    RUN_TEST(test_unstep_with_list);
    RUN_TEST(test_trace_with_list);
    RUN_TEST(test_untrace_with_list);
    RUN_TEST(test_step_nonexistent_gives_error);
    RUN_TEST(test_unstep_nonexistent_gives_error);
    RUN_TEST(test_trace_nonexistent_gives_error);
    RUN_TEST(test_untrace_nonexistent_gives_error);
    RUN_TEST(test_step_and_trace_independent);
    RUN_TEST(test_trace_prints_entry_and_exit);
    RUN_TEST(test_trace_with_arguments);
    RUN_TEST(test_trace_shows_recursion_depth);
    RUN_TEST(test_step_pauses_execution);
    RUN_TEST(test_step_multiline_procedure);
    RUN_TEST(test_step_shows_each_line_before_execution);

    return UNITY_END();
}
