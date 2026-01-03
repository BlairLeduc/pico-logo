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
// Variable Primitive Tests
//==========================================================================

void test_make_and_thing(void)
{
    run_string("make \"x 42");
    Result r = eval_string("thing \"x");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_dots_variable(void)
{
    run_string("make \"y 100");
    Result r = eval_string(":y");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, r.value.as.number);
}

void test_global_variable(void)
{
    // Basic global variable test
    run_string("make \"x 42");
    Result r = eval_string(":x");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_local_declaration(void)
{
    // At top level, local behaves like creating an unbound variable
    // We can then make it and access it
    run_string("local \"myvar");
    run_string("make \"myvar 100");
    Result r = eval_string(":myvar");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, r.value.as.number);
}

void test_scope_push_pop(void)
{
    // Test that frame-based scope management works
    TEST_ASSERT_EQUAL(0, test_scope_depth());
    test_push_scope();
    TEST_ASSERT_EQUAL(1, test_scope_depth());
    test_push_scope();
    TEST_ASSERT_EQUAL(2, test_scope_depth());
    test_pop_scope();
    TEST_ASSERT_EQUAL(1, test_scope_depth());
    test_pop_scope();
    TEST_ASSERT_EQUAL(0, test_scope_depth());
}

void test_local_variable_shadowing(void)
{
    // Create global variable
    run_string("make \"sound \"crash");
    Result r1 = eval_string(":sound");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL_STRING("crash", mem_word_ptr(r1.value.as.node));
    
    // Push a scope and create local with same name
    test_push_scope();
    test_set_local("sound", value_word(mem_atom("woof", 4)));
    
    // Local should shadow global
    Result r2 = eval_string(":sound");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("woof", mem_word_ptr(r2.value.as.node));
    
    // Pop scope
    test_pop_scope();
    
    // Global should be restored
    Result r3 = eval_string(":sound");
    TEST_ASSERT_EQUAL(RESULT_OK, r3.status);
    TEST_ASSERT_EQUAL_STRING("crash", mem_word_ptr(r3.value.as.node));
}

void test_local_variable_not_visible_after_scope(void)
{
    // Push scope and create a local-only variable
    test_push_scope();
    test_set_local("tempvar", value_number(999));
    
    // Should be accessible
    Result r1 = eval_string(":tempvar");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL_FLOAT(999.0f, r1.value.as.number);
    
    // Pop scope
    test_pop_scope();
    
    // Should no longer be accessible
    Result r2 = eval_string(":tempvar");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r2.status);
    TEST_ASSERT_EQUAL(ERR_NO_VALUE, r2.error_code);
}

void test_make_updates_local_in_scope(void)
{
    // Create global
    run_string("make \"x 10");
    
    // Push scope and create local
    test_push_scope();
    test_set_local("x", value_number(20));
    
    // Make should update the local, not global
    run_string("make \"x 30");
    Result r1 = eval_string(":x");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, r1.value.as.number);
    
    // Pop scope
    test_pop_scope();
    
    // Global should still be 10
    Result r2 = eval_string(":x");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, r2.value.as.number);
}

void test_make_creates_global_when_no_local(void)
{
    // Push scope
    test_push_scope();
    
    // Make creates global since no local declared
    run_string("make \"newglobal 42");
    
    // Pop scope
    test_pop_scope();
    
    // Should still be accessible as global
    Result r = eval_string(":newglobal");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_local_with_list(void)
{
    // local with list of names
    run_string("local [a b c]");
    run_string("make \"a 1");
    run_string("make \"b 2");
    run_string("make \"c 3");
    
    Result ra = eval_string(":a");
    TEST_ASSERT_EQUAL(RESULT_OK, ra.status);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, ra.value.as.number);
    
    Result rb = eval_string(":b");
    TEST_ASSERT_EQUAL(RESULT_OK, rb.status);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, rb.value.as.number);
    
    Result rc = eval_string(":c");
    TEST_ASSERT_EQUAL(RESULT_OK, rc.status);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, rc.value.as.number);
}

void test_name_primitive(void)
{
    // name is make with reversed arguments
    run_string("name \"pigeon \"bird");
    Result r = eval_string(":bird");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("pigeon", mem_word_ptr(r.value.as.node));
}

void test_namep_true(void)
{
    run_string("make \"testvar 123");
    Result r = eval_string("namep \"testvar");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_namep_false(void)
{
    Result r = eval_string("namep \"nonexistent");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_name_question_alias(void)
{
    // name? is the canonical name for namep
    run_string("make \"exists 42");
    
    Result r1 = eval_string("name? \"exists");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r1.value.as.node));
    
    Result r2 = eval_string("name? \"doesnotexist");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r2.value.as.node));
}

void test_nested_scopes(void)
{
    // Test multiple levels of nesting
    run_string("make \"level \"global");
    
    test_push_scope();
    test_set_local("level", value_word(mem_atom("scope1", 6)));
    Result r1 = eval_string(":level");
    TEST_ASSERT_EQUAL_STRING("scope1", mem_word_ptr(r1.value.as.node));
    
    test_push_scope();
    test_set_local("level", value_word(mem_atom("scope2", 6)));
    Result r2 = eval_string(":level");
    TEST_ASSERT_EQUAL_STRING("scope2", mem_word_ptr(r2.value.as.node));
    
    test_pop_scope();
    Result r3 = eval_string(":level");
    TEST_ASSERT_EQUAL_STRING("scope1", mem_word_ptr(r3.value.as.node));
    
    test_pop_scope();
    Result r4 = eval_string(":level");
    TEST_ASSERT_EQUAL_STRING("global", mem_word_ptr(r4.value.as.node));
}

void test_error_no_value(void)
{
    // x has no value
    Result r = eval_string(":undefined_var");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NO_VALUE, r.error_code);
    
    const char *msg = error_format(r);
    TEST_ASSERT_EQUAL_STRING("undefined_var has no value", msg);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_make_and_thing);
    RUN_TEST(test_dots_variable);
    RUN_TEST(test_global_variable);
    RUN_TEST(test_local_declaration);
    RUN_TEST(test_scope_push_pop);
    RUN_TEST(test_local_variable_shadowing);
    RUN_TEST(test_local_variable_not_visible_after_scope);
    RUN_TEST(test_make_updates_local_in_scope);
    RUN_TEST(test_make_creates_global_when_no_local);
    RUN_TEST(test_local_with_list);
    RUN_TEST(test_name_primitive);
    RUN_TEST(test_namep_true);
    RUN_TEST(test_namep_false);
    RUN_TEST(test_name_question_alias);
    RUN_TEST(test_nested_scopes);
    RUN_TEST(test_error_no_value);

    return UNITY_END();
}
