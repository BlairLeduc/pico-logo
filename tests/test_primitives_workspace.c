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
// Workspace Management Tests (po, pots, pons, bury, etc.)
//==========================================================================

void test_pots_shows_procedure_titles(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    reset_output();
    
    run_string("pots");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to myproc") != NULL);
}

void test_pots_shows_multiple_procedures(void)
{
    const char *params[] = {};
    define_proc("proca", params, 0, "print 1");
    define_proc("procb", params, 0, "print 2");
    
    reset_output();
    
    run_string("pots");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to proca") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "to procb") != NULL);
}

void test_pot_shows_single_procedure(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    reset_output();
    
    run_string("pot \"myproc");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to myproc") != NULL);
}

void test_pot_with_params(void)
{
    Node p = mem_atom("x", 1);
    const char *params[] = {mem_word_ptr(p)};
    define_proc("double", params, 1, "output :x * 2");
    
    reset_output();
    
    run_string("pot \"double");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to double :x") != NULL);
}

void test_po_shows_full_procedure(void)
{
    const char *params[] = {};
    define_proc("hello", params, 0, "print \"world");
    
    reset_output();
    
    run_string("po \"hello");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to hello") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "print") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "end") != NULL);
}

void test_pons_shows_variables(void)
{
    run_string("make \"x 42");
    run_string("make \"name \"John");
    
    reset_output();
    
    run_string("pons");
    TEST_ASSERT_TRUE(strstr(output_buffer, "make \"x 42") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "make \"name \"John") != NULL);
}

void test_pon_shows_single_variable(void)
{
    run_string("make \"myvar 123");
    
    reset_output();
    
    run_string("pon \"myvar");
    TEST_ASSERT_TRUE(strstr(output_buffer, "make \"myvar 123") != NULL);
}

void test_bury_hides_procedure_from_pots(void)
{
    const char *params[] = {};
    define_proc("visible", params, 0, "print 1");
    define_proc("hidden", params, 0, "print 2");
    
    run_string("bury \"hidden");
    
    reset_output();
    
    run_string("pots");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to visible") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "to hidden") == NULL);
}

void test_unbury_shows_procedure_in_pots(void)
{
    const char *params[] = {};
    define_proc("myproc", params, 0, "print 1");
    
    run_string("bury \"myproc");
    run_string("unbury \"myproc");
    
    reset_output();
    
    run_string("pots");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to myproc") != NULL);
}

void test_buryname_hides_variable_from_pons(void)
{
    run_string("make \"visible 1");
    run_string("make \"hidden 2");
    run_string("buryname \"hidden");
    
    reset_output();
    
    run_string("pons");
    TEST_ASSERT_TRUE(strstr(output_buffer, "make \"visible 1") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "hidden") == NULL);
}

void test_unburyname_shows_variable_in_pons(void)
{
    run_string("make \"myvar 99");
    run_string("buryname \"myvar");
    run_string("unburyname \"myvar");
    
    reset_output();
    
    run_string("pons");
    TEST_ASSERT_TRUE(strstr(output_buffer, "make \"myvar 99") != NULL);
}

void test_buryall_hides_all(void)
{
    const char *params[] = {};
    define_proc("proc1", params, 0, "print 1");
    run_string("make \"var1 100");
    
    run_string("buryall");
    
    reset_output();
    
    run_string("pots");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
    
    run_string("pons");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

void test_unburyall_shows_all(void)
{
    const char *params[] = {};
    define_proc("proc1", params, 0, "print 1");
    run_string("make \"var1 100");
    
    run_string("buryall");
    run_string("unburyall");
    
    reset_output();
    
    run_string("pots");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to proc1") != NULL);
    
    reset_output();
    
    run_string("pons");
    TEST_ASSERT_TRUE(strstr(output_buffer, "make \"var1 100") != NULL);
}

void test_bury_with_list(void)
{
    const char *params[] = {};
    define_proc("a", params, 0, "print 1");
    define_proc("b", params, 0, "print 2");
    define_proc("c", params, 0, "print 3");
    
    run_string("bury [a b]");
    
    reset_output();
    
    run_string("pots");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to a") == NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "to b") == NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "to c") != NULL);
}

void test_po_with_list(void)
{
    const char *params[] = {};
    define_proc("proca", params, 0, "print 1");
    define_proc("procb", params, 0, "print 2");
    
    reset_output();
    
    run_string("po [proca procb]");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to proca") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "to procb") != NULL);
}

void test_pot_with_list(void)
{
    const char *params[] = {};
    define_proc("x", params, 0, "print 1");
    define_proc("y", params, 0, "print 2");
    
    reset_output();
    
    run_string("pot [x y]");
    TEST_ASSERT_TRUE(strstr(output_buffer, "to x") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "to y") != NULL);
}

void test_pon_with_list(void)
{
    run_string("make \"a 1");
    run_string("make \"b 2");
    
    reset_output();
    
    run_string("pon [a b]");
    TEST_ASSERT_TRUE(strstr(output_buffer, "make \"a 1") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "make \"b 2") != NULL);
}

void test_buried_procedure_still_callable(void)
{
    const char *params[] = {};
    define_proc("buried", params, 0, "print \"works");
    
    run_string("bury \"buried");
    
    reset_output();
    
    run_string("buried");
    TEST_ASSERT_EQUAL_STRING("works\n", output_buffer);
}

void test_buried_variable_still_accessible(void)
{
    run_string("make \"secret 42");
    run_string("buryname \"secret");
    
    reset_output();
    
    run_string("print :secret");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
}

//==========================================================================
// Memory Management Tests (nodes, recycle)
//==========================================================================

void test_nodes_returns_number(void)
{
    reset_output();
    
    run_string("print nodes");
    
    // nodes should output a positive number
    int free_nodes = atoi(output_buffer);
    TEST_ASSERT_TRUE(free_nodes > 0);
}

void test_nodes_returns_correct_type(void)
{
    reset_output();
    
    // Use nodes in an arithmetic expression to verify it returns a number
    run_string("print nodes > 0");
    TEST_ASSERT_EQUAL_STRING("true\n", output_buffer);
}

void test_recycle_runs_without_error(void)
{
    // Create some garbage
    run_string("make \"x [a b c d e f]");
    run_string("ern \"x");
    
    // recycle should run without error
    run_string("recycle");
    
    // Test passed if we got here without crashing
    TEST_PASS();
}

void test_recycle_frees_memory(void)
{
    // recycle should run and free garbage nodes
    // This is a basic smoke test - detailed memory tests are in test_memory.c
    
    // Create some lists (uses memory)
    run_string("make \"x [a b c d e f g h i j]");
    run_string("make \"y [1 2 3 4 5 6 7 8 9 10]");
    
    // Erase variables (makes lists garbage)
    run_string("ern \"x");
    run_string("ern \"y");
    
    // Run garbage collection - should not crash
    run_string("recycle");
    
    // Verify nodes still returns a valid number after recycle
    reset_output();
    run_string("print nodes > 0");
    TEST_ASSERT_EQUAL_STRING("true\n", output_buffer);
}

void test_recycle_preserves_live_data(void)
{
    // Create data that should be preserved
    run_string("make \"keepme [important data]");
    
    const char *params[] = {};
    define_proc("myproc", params, 0, "print \"hello");
    
    // Run garbage collection
    run_string("recycle");
    
    reset_output();
    
    // Verify data is still accessible (print outputs list contents without brackets)
    run_string("print :keepme");
    TEST_ASSERT_EQUAL_STRING("important data\n", output_buffer);
    
    reset_output();
    
    // Verify procedure still works
    run_string("myproc");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_pots_shows_procedure_titles);
    RUN_TEST(test_pots_shows_multiple_procedures);
    RUN_TEST(test_pot_shows_single_procedure);
    RUN_TEST(test_pot_with_params);
    RUN_TEST(test_po_shows_full_procedure);
    RUN_TEST(test_pons_shows_variables);
    RUN_TEST(test_pon_shows_single_variable);
    RUN_TEST(test_bury_hides_procedure_from_pots);
    RUN_TEST(test_unbury_shows_procedure_in_pots);
    RUN_TEST(test_buryname_hides_variable_from_pons);
    RUN_TEST(test_unburyname_shows_variable_in_pons);
    RUN_TEST(test_buryall_hides_all);
    RUN_TEST(test_unburyall_shows_all);
    RUN_TEST(test_bury_with_list);
    RUN_TEST(test_po_with_list);
    RUN_TEST(test_pot_with_list);
    RUN_TEST(test_pon_with_list);
    RUN_TEST(test_buried_procedure_still_callable);
    RUN_TEST(test_buried_variable_still_accessible);
    
    // Memory management tests
    RUN_TEST(test_nodes_returns_number);
    RUN_TEST(test_nodes_returns_correct_type);
    RUN_TEST(test_recycle_runs_without_error);
    RUN_TEST(test_recycle_frees_memory);
    RUN_TEST(test_recycle_preserves_live_data);

    return UNITY_END();
}
