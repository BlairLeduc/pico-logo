//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for hardware primitives (battery)
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
// Battery Primitive Tests
//==========================================================================

void test_battery_returns_list(void)
{
    // Battery should return a list [level charging_status]
    Result r = eval_string("battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
}

void test_battery_returns_two_element_list(void)
{
    Result r = eval_string("battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    // Check that it has exactly 2 elements
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));  // First element exists
    Node rest = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(rest));  // Second element exists
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(rest)));  // No third element
}

void test_battery_level_full(void)
{
    set_mock_battery(100, false);
    
    Result r = eval_string("first battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("100", mem_word_ptr(r.value.as.node));
}

void test_battery_level_partial(void)
{
    set_mock_battery(42, false);
    
    Result r = eval_string("first battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("42", mem_word_ptr(r.value.as.node));
}

void test_battery_level_empty(void)
{
    set_mock_battery(0, false);
    
    Result r = eval_string("first battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(r.value.as.node));
}

void test_battery_level_unavailable(void)
{
    set_mock_battery(-1, false);
    
    Result r = eval_string("first battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("-1", mem_word_ptr(r.value.as.node));
}

void test_battery_not_charging(void)
{
    set_mock_battery(50, false);
    
    Result r = eval_string("last battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_battery_charging(void)
{
    set_mock_battery(75, true);
    
    Result r = eval_string("last battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_battery_in_procedure(void)
{
    // Test using battery within a procedure
    set_mock_battery(88, true);
    
    const char *params[] = {};
    define_proc("getlevel", params, 0, "output first battery");
    
    Result r = eval_string("getlevel");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("88", mem_word_ptr(r.value.as.node));
}

void test_battery_charging_in_procedure(void)
{
    set_mock_battery(60, true);
    
    const char *params[] = {};
    define_proc("ischarging", params, 0, "output last battery");
    
    Result r = eval_string("ischarging");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_battery_print_output(void)
{
    set_mock_battery(50, false);
    
    run_string("print battery");
    
    TEST_ASSERT_EQUAL_STRING("50 false\n", output_buffer);
}

void test_battery_show_output(void)
{
    set_mock_battery(75, true);
    
    run_string("show battery");
    
    TEST_ASSERT_EQUAL_STRING("[75 true]\n", output_buffer);
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Battery tests
    RUN_TEST(test_battery_returns_list);
    RUN_TEST(test_battery_returns_two_element_list);
    RUN_TEST(test_battery_level_full);
    RUN_TEST(test_battery_level_partial);
    RUN_TEST(test_battery_level_empty);
    RUN_TEST(test_battery_level_unavailable);
    RUN_TEST(test_battery_not_charging);
    RUN_TEST(test_battery_charging);
    RUN_TEST(test_battery_in_procedure);
    RUN_TEST(test_battery_charging_in_procedure);
    RUN_TEST(test_battery_print_output);
    RUN_TEST(test_battery_show_output);
    
    return UNITY_END();
}
