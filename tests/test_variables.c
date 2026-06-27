//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the variable storage system (core/variables.c).
//

#include "unity.h"
#include "test_scaffold.h"
#include "core/variables.h"
#include "core/memory.h"

#include <string.h>

void setUp(void)
{
    test_scaffold_setUp();
    variables_init();
}

void tearDown(void)
{
    // Pop any remaining scopes
    while (test_scope_depth() > 0)
    {
        test_pop_scope();
    }
    test_scaffold_tearDown();
}

//============================================================================
// Basic Global Variable Tests
//============================================================================

void test_set_and_get_global(void)
{
    TEST_ASSERT_TRUE(var_set("x", value_number(42)));
    
    Value v;
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL(VALUE_NUMBER, v.type);
    TEST_ASSERT_EQUAL_FLOAT(42, v.as.number);
}

void test_var_exists_true(void)
{
    var_set("x", value_number(10));
    TEST_ASSERT_TRUE(var_exists("x"));
}

void test_var_exists_false(void)
{
    TEST_ASSERT_FALSE(var_exists("nonexistent"));
}

void test_var_get_nonexistent(void)
{
    Value v;
    TEST_ASSERT_FALSE(var_get("nonexistent", &v));
}

void test_var_set_overwrites(void)
{
    var_set("x", value_number(1));
    var_set("x", value_number(2));
    
    Value v;
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL_FLOAT(2, v.as.number);
}

void test_var_erase(void)
{
    var_set("x", value_number(42));
    TEST_ASSERT_TRUE(var_exists("x"));
    
    var_erase("x");
    TEST_ASSERT_FALSE(var_exists("x"));
}

void test_var_erase_nonexistent(void)
{
    // Should not crash
    var_erase("nonexistent");
}

void test_var_erase_all(void)
{
    var_set("a", value_number(1));
    var_set("b", value_number(2));
    var_set("c", value_number(3));
    
    var_erase_all();
    
    TEST_ASSERT_FALSE(var_exists("a"));
    TEST_ASSERT_FALSE(var_exists("b"));
    TEST_ASSERT_FALSE(var_exists("c"));
}

//============================================================================
// Local Variable Declaration Tests
//============================================================================

void test_declare_local_at_top_level(void)
{
    // At top level, local behaves like unbound global
    TEST_ASSERT_TRUE(var_declare_local("x"));
    
    // Should not have a value
    Value v;
    TEST_ASSERT_FALSE(var_get("x", &v));
}

void test_declare_local_in_scope(void)
{
    test_push_scope();
    
    TEST_ASSERT_TRUE(var_declare_local("x"));
    
    // Declared local creates a binding with VALUE_NONE
    Value v;
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL(VALUE_NONE, v.type);
    
    test_pop_scope();
}

void test_set_local_in_scope(void)
{
    test_push_scope();
    
    TEST_ASSERT_TRUE(var_set_local("x", value_number(42)));
    
    Value v;
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL_FLOAT(42, v.as.number);
    
    test_pop_scope();
}

void test_set_local_at_top_level_creates_global(void)
{
    // At top level, set_local behaves like set
    var_set_local("x", value_number(42));
    
    Value v;
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL_FLOAT(42, v.as.number);
}

void test_local_shadows_global(void)
{
    var_set("x", value_number(1));
    
    test_push_scope();
    var_set_local("x", value_number(2));
    
    Value v;
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL_FLOAT(2, v.as.number);
    
    test_pop_scope();
    
    // Global should be restored
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL_FLOAT(1, v.as.number);
}

void test_make_updates_local_not_global(void)
{
    var_set("x", value_number(1));
    
    test_push_scope();
    var_set_local("x", value_number(2));
    
    // MAKE should update the local, not create a new global
    var_set("x", value_number(3));
    
    Value v;
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL_FLOAT(3, v.as.number);
    
    test_pop_scope();
    
    // Global should still be 1
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL_FLOAT(1, v.as.number);
}

//============================================================================
// Erase All Globals Tests
//============================================================================

void test_erase_all_globals(void)
{
    var_set("a", value_number(1));
    var_set("b", value_number(2));
    
    var_erase_all_globals(false);
    
    TEST_ASSERT_FALSE(var_exists("a"));
    TEST_ASSERT_FALSE(var_exists("b"));
}

void test_erase_all_globals_respects_buried(void)
{
    var_set("a", value_number(1));
    var_set("b", value_number(2));
    var_bury("a");
    
    var_erase_all_globals(true);  // check_buried = true
    
    // Buried variable should survive
    TEST_ASSERT_TRUE(var_exists("a"));
    // Non-buried variable should be erased
    TEST_ASSERT_FALSE(var_exists("b"));
}

//============================================================================
// Bury/Unbury Tests
//============================================================================

void test_bury_and_unbury(void)
{
    var_set("x", value_number(42));
    
    // Count without buried filter
    TEST_ASSERT_EQUAL(1, var_global_count(true));
    TEST_ASSERT_EQUAL(1, var_global_count(false));
    
    // Bury
    var_bury("x");
    TEST_ASSERT_EQUAL(1, var_global_count(true));   // include_buried
    TEST_ASSERT_EQUAL(0, var_global_count(false));  // exclude_buried
    
    // Unbury
    var_unbury("x");
    TEST_ASSERT_EQUAL(1, var_global_count(false));
}

void test_bury_all_and_unbury_all(void)
{
    var_set("a", value_number(1));
    var_set("b", value_number(2));
    
    var_bury_all();
    TEST_ASSERT_EQUAL(0, var_global_count(false));
    TEST_ASSERT_EQUAL(2, var_global_count(true));
    
    var_unbury_all();
    TEST_ASSERT_EQUAL(2, var_global_count(false));
}

void test_bury_nonexistent(void)
{
    // Should not crash
    var_bury("nonexistent");
    var_unbury("nonexistent");
}

//============================================================================
// Global Count and Iteration Tests
//============================================================================

void test_global_count_empty(void)
{
    TEST_ASSERT_EQUAL(0, var_global_count(true));
    TEST_ASSERT_EQUAL(0, var_global_count(false));
}

void test_global_count_with_variables(void)
{
    var_set("a", value_number(1));
    var_set("b", value_number(2));
    var_set("c", value_number(3));
    
    TEST_ASSERT_EQUAL(3, var_global_count(true));
}

void test_get_global_by_index(void)
{
    var_set("alpha", value_number(10));
    var_set("beta", value_number(20));
    
    const char *name;
    Value v;
    
    TEST_ASSERT_TRUE(var_get_global_by_index(0, true, &name, &v));
    TEST_ASSERT_NOT_NULL(name);
    
    TEST_ASSERT_TRUE(var_get_global_by_index(1, true, &name, &v));
    TEST_ASSERT_NOT_NULL(name);
    
    // Out of range
    TEST_ASSERT_FALSE(var_get_global_by_index(2, true, &name, &v));
}

void test_get_global_by_index_excludes_buried(void)
{
    var_set("a", value_number(1));
    var_set("b", value_number(2));
    var_bury("a");
    
    const char *name;
    Value v;
    
    // Only one non-buried variable
    TEST_ASSERT_TRUE(var_get_global_by_index(0, false, &name, &v));
    TEST_ASSERT_EQUAL_STRING("b", name);
    
    TEST_ASSERT_FALSE(var_get_global_by_index(1, false, &name, &v));
}

//============================================================================
// Local Count and Iteration Tests
//============================================================================

void test_local_count_no_scope(void)
{
    TEST_ASSERT_EQUAL(0, var_local_count());
}

void test_local_count_with_locals(void)
{
    test_push_scope();
    var_set_local("x", value_number(1));
    var_set_local("y", value_number(2));
    
    TEST_ASSERT_EQUAL(2, var_local_count());
    
    test_pop_scope();
}

void test_get_local_by_index(void)
{
    test_push_scope();
    var_set_local("x", value_number(10));
    var_set_local("y", value_number(20));
    
    const char *name;
    Value v;
    
    TEST_ASSERT_TRUE(var_get_local_by_index(0, &name, &v));
    TEST_ASSERT_NOT_NULL(name);
    
    TEST_ASSERT_TRUE(var_get_local_by_index(1, &name, &v));
    TEST_ASSERT_NOT_NULL(name);
    
    // Out of range
    TEST_ASSERT_FALSE(var_get_local_by_index(2, &name, &v));
    
    test_pop_scope();
}

void test_get_local_by_index_no_scope(void)
{
    const char *name;
    Value v;
    TEST_ASSERT_FALSE(var_get_local_by_index(0, &name, &v));
}

//============================================================================
// Shadowing Detection Tests
//============================================================================

void test_is_shadowed_by_local_true(void)
{
    var_set("x", value_number(1));
    
    test_push_scope();
    var_set_local("x", value_number(2));
    
    TEST_ASSERT_TRUE(var_is_shadowed_by_local("x"));
    
    test_pop_scope();
}

void test_is_shadowed_by_local_false(void)
{
    var_set("x", value_number(1));
    
    test_push_scope();
    // Don't create a local 'x'
    
    TEST_ASSERT_FALSE(var_is_shadowed_by_local("x"));
    
    test_pop_scope();
}

void test_is_shadowed_by_local_no_scope(void)
{
    var_set("x", value_number(1));
    TEST_ASSERT_FALSE(var_is_shadowed_by_local("x"));
}

//============================================================================
// Test State Tests
//============================================================================

void test_set_and_get_test_state(void)
{
    var_set_test(true);
    
    bool val;
    TEST_ASSERT_TRUE(var_get_test(&val));
    TEST_ASSERT_TRUE(val);
    
    var_set_test(false);
    TEST_ASSERT_TRUE(var_get_test(&val));
    TEST_ASSERT_FALSE(val);
}

void test_test_is_valid_initially_false(void)
{
    TEST_ASSERT_FALSE(var_test_is_valid());
}

void test_test_is_valid_after_set(void)
{
    var_set_test(true);
    TEST_ASSERT_TRUE(var_test_is_valid());
}

void test_reset_test_state(void)
{
    var_set_test(true);
    TEST_ASSERT_TRUE(var_test_is_valid());
    
    var_reset_test_state();
    TEST_ASSERT_FALSE(var_test_is_valid());
}

void test_test_state_in_scope(void)
{
    test_push_scope();
    
    var_set_test(true);
    
    bool val;
    TEST_ASSERT_TRUE(var_get_test(&val));
    TEST_ASSERT_TRUE(val);
    
    test_pop_scope();
}

void test_get_test_no_valid_test(void)
{
    bool val;
    TEST_ASSERT_FALSE(var_get_test(&val));
}

//============================================================================
// Case Insensitivity Tests
//============================================================================

void test_case_insensitive_get(void)
{
    var_set("MyVar", value_number(42));
    
    Value v;
    TEST_ASSERT_TRUE(var_get("myvar", &v));
    TEST_ASSERT_EQUAL_FLOAT(42, v.as.number);
    
    TEST_ASSERT_TRUE(var_get("MYVAR", &v));
    TEST_ASSERT_EQUAL_FLOAT(42, v.as.number);
}

void test_case_insensitive_exists(void)
{
    var_set("Test", value_number(1));
    TEST_ASSERT_TRUE(var_exists("test"));
    TEST_ASSERT_TRUE(var_exists("TEST"));
}

//============================================================================
// GC Mark Tests
//============================================================================

void test_gc_mark_all_no_crash(void)
{
    var_set("x", value_number(42));
    
    Node w = mem_atom("hello", 5);
    var_set("y", value_word(w));
    
    var_set("z", value_list(NODE_NIL));
    
    // Should not crash
    var_gc_mark_all();
}

//============================================================================
// Declared But Unbound Variable Tests
//============================================================================

void test_declared_local_unbound(void)
{
    test_push_scope();
    
    var_declare_local("x");
    
    // Declared local with no explicit value returns VALUE_NONE
    Value v;
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL(VALUE_NONE, v.type);
    
    test_pop_scope();
}

void test_set_after_declare_local(void)
{
    test_push_scope();
    
    var_declare_local("x");
    var_set("x", value_number(42));
    
    Value v;
    TEST_ASSERT_TRUE(var_get("x", &v));
    TEST_ASSERT_EQUAL_FLOAT(42, v.as.number);
    
    test_pop_scope();
}

//============================================================================
// Main
//============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Basic globals
    RUN_TEST(test_set_and_get_global);
    RUN_TEST(test_var_exists_true);
    RUN_TEST(test_var_exists_false);
    RUN_TEST(test_var_get_nonexistent);
    RUN_TEST(test_var_set_overwrites);
    RUN_TEST(test_var_erase);
    RUN_TEST(test_var_erase_nonexistent);
    RUN_TEST(test_var_erase_all);

    // Local variables
    RUN_TEST(test_declare_local_at_top_level);
    RUN_TEST(test_declare_local_in_scope);
    RUN_TEST(test_set_local_in_scope);
    RUN_TEST(test_set_local_at_top_level_creates_global);
    RUN_TEST(test_local_shadows_global);
    RUN_TEST(test_make_updates_local_not_global);

    // Erase all globals
    RUN_TEST(test_erase_all_globals);
    RUN_TEST(test_erase_all_globals_respects_buried);

    // Bury/unbury
    RUN_TEST(test_bury_and_unbury);
    RUN_TEST(test_bury_all_and_unbury_all);
    RUN_TEST(test_bury_nonexistent);

    // Global count and iteration
    RUN_TEST(test_global_count_empty);
    RUN_TEST(test_global_count_with_variables);
    RUN_TEST(test_get_global_by_index);
    RUN_TEST(test_get_global_by_index_excludes_buried);

    // Local count and iteration
    RUN_TEST(test_local_count_no_scope);
    RUN_TEST(test_local_count_with_locals);
    RUN_TEST(test_get_local_by_index);
    RUN_TEST(test_get_local_by_index_no_scope);

    // Shadowing
    RUN_TEST(test_is_shadowed_by_local_true);
    RUN_TEST(test_is_shadowed_by_local_false);
    RUN_TEST(test_is_shadowed_by_local_no_scope);

    // Test state
    RUN_TEST(test_set_and_get_test_state);
    RUN_TEST(test_test_is_valid_initially_false);
    RUN_TEST(test_test_is_valid_after_set);
    RUN_TEST(test_reset_test_state);
    RUN_TEST(test_test_state_in_scope);
    RUN_TEST(test_get_test_no_valid_test);

    // Case insensitivity
    RUN_TEST(test_case_insensitive_get);
    RUN_TEST(test_case_insensitive_exists);

    // GC mark
    RUN_TEST(test_gc_mark_all_no_crash);

    // Declared but unbound
    RUN_TEST(test_declared_local_unbound);
    RUN_TEST(test_set_after_declare_local);

    return UNITY_END();
}
