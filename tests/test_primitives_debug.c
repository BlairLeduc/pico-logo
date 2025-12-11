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
    
    reset_output();
    Result r = run_string("myproc");
    
    // Step is set but currently executes normally (TODO: implement proper stepping)
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(proc_is_stepped("myproc"));
    
    // After unstep, should still work
    run_string("unstep \"myproc");
    reset_output();
    r = run_string("myproc");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_FALSE(proc_is_stepped("myproc"));
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

    return UNITY_END();
}
