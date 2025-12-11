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

    return UNITY_END();
}
