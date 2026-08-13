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
#include "core/limits.h"

#include <stdio.h>
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

void test_bury_all_does_not_mark_atom_hash_links(void)
{
    char name[16];
    for (int i = 0; i < 128; i++)
    {
        snprintf(name, sizeof(name), "variable%d", i);
        TEST_ASSERT_TRUE(var_set(name, value_number((float)i)));
    }

    var_bury_all();

    for (int i = 0; i < 128; i++)
    {
        snprintf(name, sizeof(name), "variable%d", i);
        TEST_ASSERT_TRUE(mem_is_word(mem_atom(name, strlen(name))));
    }
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

// The table stores the name POINTER it is given, so a test that creates many
// variables has to hand it a stable string -- which is exactly what the
// interpreter does for every name it parses.
static const char *interned(const char *name)
{
    return mem_word_ptr(mem_atom(name, (int)strlen(name)));
}

//============================================================================
// The global hash index
//
// `find_global` keeps a hash index beside the table so a lookup does not
// depend on where the variable was defined. The table itself is unchanged --
// same order, same slots, same listings -- so what these hold is that the
// index cannot disagree with it. The failure modes of an open-addressed index
// are all about erasure: a probe chain broken by a removed entry hides every
// name behind it, and a slot reused by a new name resurrects the old one.
//============================================================================

// A full table, looked up in reverse. Before the index this was a linear scan,
// so the last name defined was the most expensive to read; now nothing about
// the order should matter -- but what a *test* can hold is that every one of
// them is still findable, which is what a broken probe chain would break.
void test_every_variable_is_found_however_full_the_table_is(void)
{
    char name[32];
    const int n = MAX_GLOBAL_VARIABLES - 8;   // room to spare in the table
    for (int i = 0; i < n; i++)
    {
        snprintf(name, sizeof(name), "v%d", i);
        // Interned, because the table stores the caller's POINTER: a reused
        // stack buffer would alias every entry onto one string. This is what
        // the interpreter itself always passes (the NAMING POLICY in frame.h).
        TEST_ASSERT_TRUE_MESSAGE(var_set(interned(name), value_number(i)), name);
    }

    for (int i = n - 1; i >= 0; i--)
    {
        Value v;
        snprintf(name, sizeof(name), "v%d", i);
        TEST_ASSERT_TRUE_MESSAGE(var_get(interned(name), &v), name);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(i, v.as.number, name);
    }
}

// Erasing one name must not hide the others. With linear probing an entry
// removed from the middle of a chain cuts off everything behind it, so this
// erases from the middle of a run of names that were created together.
void test_erasing_one_variable_leaves_the_others_findable(void)
{
    char name[32];
    for (int i = 0; i < 40; i++)
    {
        snprintf(name, sizeof(name), "e%d", i);
        var_set(interned(name), value_number(i));
    }

    for (int i = 10; i < 20; i++)
    {
        snprintf(name, sizeof(name), "e%d", i);
        var_erase(interned(name));
    }

    for (int i = 0; i < 40; i++)
    {
        Value v;
        snprintf(name, sizeof(name), "e%d", i);
        if (i >= 10 && i < 20)
        {
            TEST_ASSERT_FALSE_MESSAGE(var_exists(interned(name)), name);
        }
        else
        {
            TEST_ASSERT_TRUE_MESSAGE(var_get(interned(name), &v), name);
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(i, v.as.number, name);
        }
    }
}

// An erased slot is reused by the next variable created. The index must follow
// it: a stale entry would either resurrect the erased name or answer with the
// new one under the old name.
void test_a_reused_slot_answers_to_its_new_name_only(void)
{
    var_set("gone", value_number(1));
    var_set("kept", value_number(2));
    var_erase("gone");

    var_set("fresh", value_number(3));

    Value v;
    TEST_ASSERT_FALSE_MESSAGE(var_exists("gone"), "an erased name came back");
    TEST_ASSERT_TRUE(var_get("fresh", &v));
    TEST_ASSERT_EQUAL_FLOAT(3, v.as.number);
    TEST_ASSERT_TRUE(var_get("kept", &v));
    TEST_ASSERT_EQUAL_FLOAT(2, v.as.number);

    // And re-creating the erased name gives a fresh variable, not the old value.
    var_set("gone", value_number(9));
    TEST_ASSERT_TRUE(var_get("gone", &v));
    TEST_ASSERT_EQUAL_FLOAT(9, v.as.number);
}

// B20: a slot carries its `buried` flag past the variable that set it. Erasing a
// buried global leaves the flag standing, so the next variable to land in that
// slot is born buried — invisible to `pons` and immune to `erall`.
void test_a_reused_slot_does_not_inherit_burial(void)
{
    var_set("hidden", value_number(1));
    var_bury("hidden");
    var_erase("hidden");

    var_set("plain", value_number(2));

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, var_global_count(false),
                                  "a new variable was born buried");

    // And `erall`, which spares buried names, must still take this one.
    var_erase_all_globals(true);
    TEST_ASSERT_FALSE_MESSAGE(var_exists("plain"),
                              "a new variable survived erall as if buried");
}

// `erall` clears the table wholesale, and the index has to be cleared with it
// or every name would still appear to exist.
void test_erasing_everything_leaves_nothing_findable(void)
{
    var_set("a", value_number(1));
    var_set("b", value_number(2));
    var_erase_all_globals(false);

    TEST_ASSERT_FALSE(var_exists("a"));
    TEST_ASSERT_FALSE(var_exists("b"));

    // The table still works afterwards -- a cleared index must not be a broken
    // one.
    var_set("a", value_number(7));
    Value v;
    TEST_ASSERT_TRUE(var_get("a", &v));
    TEST_ASSERT_EQUAL_FLOAT(7, v.as.number);
}

// The index hashes a folded name, so it has to agree with the case-insensitive
// comparison it is short-cutting: `FOO` and `foo` are one variable, and if they
// hashed apart they would quietly become two.
void test_case_folding_agrees_with_the_index(void)
{
    var_set("MixedCase", value_number(1));
    var_set("MIXEDCASE", value_number(2));

    Value v;
    TEST_ASSERT_TRUE(var_get("mixedcase", &v));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, v.as.number,
                                    "the second `make` did not find the first variable");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, var_global_count(true),
                                  "a differently-cased name became a second variable");
}

// The table's ORDER is what `pons`, `poall` and the workspace listings print,
// and the index is a side table precisely so that order is untouched.
void test_the_index_does_not_reorder_the_table(void)
{
    var_set("first", value_number(1));
    var_set("second", value_number(2));
    var_set("third", value_number(3));

    // Read them back -- a lookup must not move anything.
    Value v;
    var_get("third", &v);
    var_get("third", &v);
    var_get("first", &v);

    const char *name;
    TEST_ASSERT_TRUE(var_get_global_by_index(0, true, &name, &v));
    TEST_ASSERT_EQUAL_STRING("first", name);
    TEST_ASSERT_TRUE(var_get_global_by_index(1, true, &name, &v));
    TEST_ASSERT_EQUAL_STRING("second", name);
    TEST_ASSERT_TRUE(var_get_global_by_index(2, true, &name, &v));
    TEST_ASSERT_EQUAL_STRING("third", name);
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
    RUN_TEST(test_bury_all_does_not_mark_atom_hash_links);
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
    RUN_TEST(test_every_variable_is_found_however_full_the_table_is);
    RUN_TEST(test_erasing_one_variable_leaves_the_others_findable);
    RUN_TEST(test_a_reused_slot_answers_to_its_new_name_only);
    RUN_TEST(test_a_reused_slot_does_not_inherit_burial);
    RUN_TEST(test_erasing_everything_leaves_nothing_findable);
    RUN_TEST(test_case_folding_agrees_with_the_index);
    RUN_TEST(test_the_index_does_not_reorder_the_table);
    RUN_TEST(test_gc_mark_all_no_crash);

    // Declared but unbound
    RUN_TEST(test_declared_local_unbound);
    RUN_TEST(test_set_after_declare_local);

    return UNITY_END();
}
