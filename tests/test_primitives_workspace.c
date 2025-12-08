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

    return UNITY_END();
}
