//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include "core/properties.h"
#include <stdlib.h>

// Declare the function to set device in primitives_properties.c
extern void primitives_properties_set_device(LogoDevice *device);

void setUp(void)
{
    test_scaffold_setUp();
    properties_init();
    primitives_properties_set_device(&mock_device);
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Property List Tests
//==========================================================================

// pprop and gprop basic usage
void test_pprop_and_gprop_word_value(void)
{
    run_string("pprop \"person \"name \"John");
    
    Result r = eval_string("gprop \"person \"name");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_TRUE(mem_word_eq(r.value.as.node, "John", 4));
}

void test_pprop_and_gprop_number_value(void)
{
    run_string("pprop \"person \"age 42");
    
    Result r = eval_string("gprop \"person \"age");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(42.0, r.value.as.number);
}

void test_pprop_and_gprop_list_value(void)
{
    run_string("pprop \"person \"hobbies [reading coding]");
    
    Result r = eval_string("gprop \"person \"hobbies");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Check list contents
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));
    TEST_ASSERT_TRUE(mem_word_eq(mem_car(list), "reading", 7));
    
    Node rest = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(rest));
    TEST_ASSERT_TRUE(mem_word_eq(mem_car(rest), "coding", 6));
}

void test_gprop_returns_empty_list_for_unknown_name(void)
{
    Result r = eval_string("gprop \"unknown \"prop");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_gprop_returns_empty_list_for_unknown_property(void)
{
    run_string("pprop \"person \"name \"John");
    
    Result r = eval_string("gprop \"person \"unknownprop");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_pprop_overwrites_existing_property(void)
{
    run_string("pprop \"person \"name \"John");
    run_string("pprop \"person \"name \"Jane");
    
    Result r = eval_string("gprop \"person \"name");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_TRUE(mem_word_eq(r.value.as.node, "Jane", 4));
}

void test_multiple_properties_on_same_name(void)
{
    run_string("pprop \"person \"name \"John");
    run_string("pprop \"person \"age 42");
    run_string("pprop \"person \"city \"NYC");
    
    Result r1 = eval_string("gprop \"person \"name");
    TEST_ASSERT_TRUE(mem_word_eq(r1.value.as.node, "John", 4));
    
    Result r2 = eval_string("gprop \"person \"age");
    TEST_ASSERT_EQUAL_FLOAT(42.0, r2.value.as.number);
    
    Result r3 = eval_string("gprop \"person \"city");
    TEST_ASSERT_TRUE(mem_word_eq(r3.value.as.node, "NYC", 3));
}

void test_properties_on_different_names(void)
{
    run_string("pprop \"person1 \"name \"John");
    run_string("pprop \"person2 \"name \"Jane");
    
    Result r1 = eval_string("gprop \"person1 \"name");
    TEST_ASSERT_TRUE(mem_word_eq(r1.value.as.node, "John", 4));
    
    Result r2 = eval_string("gprop \"person2 \"name");
    TEST_ASSERT_TRUE(mem_word_eq(r2.value.as.node, "Jane", 4));
}

// plist tests
void test_plist_returns_empty_list_for_unknown_name(void)
{
    Result r = eval_string("plist \"unknown");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_plist_returns_property_pairs(void)
{
    run_string("pprop \"person \"name \"John");
    run_string("pprop \"person \"age 42");
    
    Result r = eval_string("plist \"person");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Should have 4 elements: name John age 42
    Node list = r.value.as.node;
    int count = 0;
    while (!mem_is_nil(list))
    {
        count++;
        list = mem_cdr(list);
    }
    TEST_ASSERT_EQUAL(4, count);
}

// remprop tests
void test_remprop_removes_property(void)
{
    run_string("pprop \"person \"name \"John");
    run_string("pprop \"person \"age 42");
    run_string("remprop \"person \"name");
    
    // name should be gone
    Result r1 = eval_string("gprop \"person \"name");
    TEST_ASSERT_TRUE(mem_is_nil(r1.value.as.node));
    
    // age should still exist
    Result r2 = eval_string("gprop \"person \"age");
    TEST_ASSERT_EQUAL_FLOAT(42.0, r2.value.as.number);
}

void test_remprop_on_nonexistent_property(void)
{
    run_string("pprop \"person \"name \"John");
    
    // Should not error
    Result r = run_string("remprop \"person \"unknownprop");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
}

void test_remprop_on_nonexistent_name(void)
{
    // Should not error
    Result r = run_string("remprop \"unknown \"prop");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
}

// erprops tests
void test_erprops_clears_all_properties(void)
{
    run_string("pprop \"person1 \"name \"John");
    run_string("pprop \"person2 \"name \"Jane");
    run_string("erprops");
    
    Result r1 = eval_string("gprop \"person1 \"name");
    TEST_ASSERT_TRUE(mem_is_nil(r1.value.as.node));
    
    Result r2 = eval_string("gprop \"person2 \"name");
    TEST_ASSERT_TRUE(mem_is_nil(r2.value.as.node));
}

// pps tests
void test_pps_prints_property_lists(void)
{
    run_string("pprop \"person \"name \"John");
    
    reset_output();
    run_string("pps");
    
    TEST_ASSERT_TRUE(strstr(output_buffer, "plist") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "person") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "name") != NULL);
    TEST_ASSERT_TRUE(strstr(output_buffer, "John") != NULL);
}

void test_pps_with_no_properties(void)
{
    reset_output();
    run_string("pps");
    
    // Should output nothing (or empty)
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

// Error handling tests
void test_pprop_requires_word_for_name(void)
{
    Result r = run_string("pprop 123 \"prop \"value");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_pprop_requires_word_for_property(void)
{
    Result r = run_string("pprop \"name 123 \"value");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_gprop_requires_word_for_name(void)
{
    Result r = run_string("print gprop 123 \"prop");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_gprop_requires_word_for_property(void)
{
    Result r = run_string("print gprop \"name 123");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_plist_requires_word(void)
{
    Result r = run_string("print plist 123");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_remprop_requires_word_for_name(void)
{
    Result r = run_string("remprop 123 \"prop");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_remprop_requires_word_for_property(void)
{
    Result r = run_string("remprop \"name 123");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

// Case insensitivity tests
void test_property_names_are_case_insensitive(void)
{
    run_string("pprop \"Person \"NAME \"John");
    
    Result r = eval_string("gprop \"person \"name");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_TRUE(mem_word_eq(r.value.as.node, "John", 4));
}

int main(void)
{
    UNITY_BEGIN();
    
    // Basic pprop/gprop tests
    RUN_TEST(test_pprop_and_gprop_word_value);
    RUN_TEST(test_pprop_and_gprop_number_value);
    RUN_TEST(test_pprop_and_gprop_list_value);
    RUN_TEST(test_gprop_returns_empty_list_for_unknown_name);
    RUN_TEST(test_gprop_returns_empty_list_for_unknown_property);
    RUN_TEST(test_pprop_overwrites_existing_property);
    RUN_TEST(test_multiple_properties_on_same_name);
    RUN_TEST(test_properties_on_different_names);
    
    // plist tests
    RUN_TEST(test_plist_returns_empty_list_for_unknown_name);
    RUN_TEST(test_plist_returns_property_pairs);
    
    // remprop tests
    RUN_TEST(test_remprop_removes_property);
    RUN_TEST(test_remprop_on_nonexistent_property);
    RUN_TEST(test_remprop_on_nonexistent_name);
    
    // erprops tests
    RUN_TEST(test_erprops_clears_all_properties);
    
    // pps tests
    RUN_TEST(test_pps_prints_property_lists);
    RUN_TEST(test_pps_with_no_properties);
    
    // Error handling tests
    RUN_TEST(test_pprop_requires_word_for_name);
    RUN_TEST(test_pprop_requires_word_for_property);
    RUN_TEST(test_gprop_requires_word_for_name);
    RUN_TEST(test_gprop_requires_word_for_property);
    RUN_TEST(test_plist_requires_word);
    RUN_TEST(test_remprop_requires_word_for_name);
    RUN_TEST(test_remprop_requires_word_for_property);
    
    // Case insensitivity
    RUN_TEST(test_property_names_are_case_insensitive);
    
    return UNITY_END();
}
