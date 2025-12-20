//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the value system.
//

#include "unity.h"
#include "core/value.h"
#include "core/memory.h"

#include <string.h>
#include <math.h>

void setUp(void)
{
    mem_init();
}

void tearDown(void)
{
    // Nothing to tear down
}

//============================================================================
// Value Constructor Tests
//============================================================================

void test_value_none_type(void)
{
    Value v = value_none();
    TEST_ASSERT_EQUAL(VALUE_NONE, v.type);
}

void test_value_number_type(void)
{
    Value v = value_number(42.5f);
    TEST_ASSERT_EQUAL(VALUE_NUMBER, v.type);
}

void test_value_number_content(void)
{
    Value v = value_number(42.5f);
    TEST_ASSERT_EQUAL_FLOAT(42.5f, v.as.number);
}

void test_value_number_negative(void)
{
    Value v = value_number(-123.456f);
    TEST_ASSERT_EQUAL_FLOAT(-123.456f, v.as.number);
}

void test_value_number_zero(void)
{
    Value v = value_number(0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, v.as.number);
}

void test_value_word_type(void)
{
    Node word = mem_atom("hello", 5);
    Value v = value_word(word);
    TEST_ASSERT_EQUAL(VALUE_WORD, v.type);
}

void test_value_word_content(void)
{
    Node word = mem_atom("hello", 5);
    Value v = value_word(word);
    TEST_ASSERT_EQUAL(word, v.as.node);
}

void test_value_list_type(void)
{
    Node word = mem_atom("item", 4);
    Node list = mem_cons(word, NODE_NIL);
    Value v = value_list(list);
    TEST_ASSERT_EQUAL(VALUE_LIST, v.type);
}

void test_value_list_content(void)
{
    Node word = mem_atom("item", 4);
    Node list = mem_cons(word, NODE_NIL);
    Value v = value_list(list);
    TEST_ASSERT_EQUAL(list, v.as.node);
}

void test_value_list_empty(void)
{
    Value v = value_list(NODE_NIL);
    TEST_ASSERT_EQUAL(VALUE_LIST, v.type);
    TEST_ASSERT_EQUAL(NODE_NIL, v.as.node);
}

//============================================================================
// Value Predicate Tests
//============================================================================

void test_value_is_none_true(void)
{
    Value v = value_none();
    TEST_ASSERT_TRUE(value_is_none(v));
}

void test_value_is_none_false_for_number(void)
{
    Value v = value_number(42.0f);
    TEST_ASSERT_FALSE(value_is_none(v));
}

void test_value_is_none_false_for_word(void)
{
    Node word = mem_atom("test", 4);
    Value v = value_word(word);
    TEST_ASSERT_FALSE(value_is_none(v));
}

void test_value_is_none_false_for_list(void)
{
    Value v = value_list(NODE_NIL);
    TEST_ASSERT_FALSE(value_is_none(v));
}

void test_value_is_number_true(void)
{
    Value v = value_number(3.14f);
    TEST_ASSERT_TRUE(value_is_number(v));
}

void test_value_is_number_false_for_none(void)
{
    Value v = value_none();
    TEST_ASSERT_FALSE(value_is_number(v));
}

void test_value_is_number_false_for_word(void)
{
    Node word = mem_atom("123", 3);
    Value v = value_word(word);
    TEST_ASSERT_FALSE(value_is_number(v));
}

void test_value_is_number_false_for_list(void)
{
    Value v = value_list(NODE_NIL);
    TEST_ASSERT_FALSE(value_is_number(v));
}

void test_value_is_word_true(void)
{
    Node word = mem_atom("hello", 5);
    Value v = value_word(word);
    TEST_ASSERT_TRUE(value_is_word(v));
}

void test_value_is_word_false_for_none(void)
{
    Value v = value_none();
    TEST_ASSERT_FALSE(value_is_word(v));
}

void test_value_is_word_false_for_number(void)
{
    Value v = value_number(42.0f);
    TEST_ASSERT_FALSE(value_is_word(v));
}

void test_value_is_word_false_for_list(void)
{
    Value v = value_list(NODE_NIL);
    TEST_ASSERT_FALSE(value_is_word(v));
}

void test_value_is_list_true(void)
{
    Value v = value_list(NODE_NIL);
    TEST_ASSERT_TRUE(value_is_list(v));
}

void test_value_is_list_true_with_items(void)
{
    Node word = mem_atom("item", 4);
    Node list = mem_cons(word, NODE_NIL);
    Value v = value_list(list);
    TEST_ASSERT_TRUE(value_is_list(v));
}

void test_value_is_list_false_for_none(void)
{
    Value v = value_none();
    TEST_ASSERT_FALSE(value_is_list(v));
}

void test_value_is_list_false_for_number(void)
{
    Value v = value_number(42.0f);
    TEST_ASSERT_FALSE(value_is_list(v));
}

void test_value_is_list_false_for_word(void)
{
    Node word = mem_atom("test", 4);
    Value v = value_word(word);
    TEST_ASSERT_FALSE(value_is_list(v));
}

//============================================================================
// Value Conversion Tests
//============================================================================

void test_value_to_number_from_number(void)
{
    Value v = value_number(42.5f);
    float out;
    TEST_ASSERT_TRUE(value_to_number(v, &out));
    TEST_ASSERT_EQUAL_FLOAT(42.5f, out);
}

void test_value_to_number_from_word_integer(void)
{
    Node word = mem_atom("123", 3);
    Value v = value_word(word);
    float out;
    TEST_ASSERT_TRUE(value_to_number(v, &out));
    TEST_ASSERT_EQUAL_FLOAT(123.0f, out);
}

void test_value_to_number_from_word_float(void)
{
    Node word = mem_atom("3.14", 4);
    Value v = value_word(word);
    float out;
    TEST_ASSERT_TRUE(value_to_number(v, &out));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.14f, out);
}

void test_value_to_number_from_word_negative(void)
{
    Node word = mem_atom("-42", 3);
    Value v = value_word(word);
    float out;
    TEST_ASSERT_TRUE(value_to_number(v, &out));
    TEST_ASSERT_EQUAL_FLOAT(-42.0f, out);
}

void test_value_to_number_from_word_scientific(void)
{
    Node word = mem_atom("1e4", 3);
    Value v = value_word(word);
    float out;
    TEST_ASSERT_TRUE(value_to_number(v, &out));
    TEST_ASSERT_EQUAL_FLOAT(10000.0f, out);
}

void test_value_to_number_from_word_invalid(void)
{
    Node word = mem_atom("hello", 5);
    Value v = value_word(word);
    float out;
    TEST_ASSERT_FALSE(value_to_number(v, &out));
}

void test_value_to_number_from_word_partial(void)
{
    // "42abc" should fail because not entire string is consumed
    Node word = mem_atom("42abc", 5);
    Value v = value_word(word);
    float out;
    TEST_ASSERT_FALSE(value_to_number(v, &out));
}

void test_value_to_number_from_none(void)
{
    Value v = value_none();
    float out;
    TEST_ASSERT_FALSE(value_to_number(v, &out));
}

void test_value_to_number_from_list(void)
{
    Value v = value_list(NODE_NIL);
    float out;
    TEST_ASSERT_FALSE(value_to_number(v, &out));
}

void test_value_to_node_from_word(void)
{
    Node word = mem_atom("test", 4);
    Value v = value_word(word);
    TEST_ASSERT_EQUAL(word, value_to_node(v));
}

void test_value_to_node_from_list(void)
{
    Node word = mem_atom("item", 4);
    Node list = mem_cons(word, NODE_NIL);
    Value v = value_list(list);
    TEST_ASSERT_EQUAL(list, value_to_node(v));
}

void test_value_to_node_from_empty_list(void)
{
    Value v = value_list(NODE_NIL);
    TEST_ASSERT_EQUAL(NODE_NIL, value_to_node(v));
}

void test_value_to_node_from_number(void)
{
    Value v = value_number(42.0f);
    TEST_ASSERT_EQUAL(NODE_NIL, value_to_node(v));
}

void test_value_to_node_from_none(void)
{
    Value v = value_none();
    TEST_ASSERT_EQUAL(NODE_NIL, value_to_node(v));
}

void test_value_to_string_none(void)
{
    Value v = value_none();
    TEST_ASSERT_EQUAL_STRING("", value_to_string(v));
}

void test_value_to_string_number_integer(void)
{
    Value v = value_number(42.0f);
    TEST_ASSERT_EQUAL_STRING("42", value_to_string(v));
}

void test_value_to_string_number_float(void)
{
    Value v = value_number(3.5f);
    TEST_ASSERT_EQUAL_STRING("3.5", value_to_string(v));
}

void test_value_to_string_number_negative(void)
{
    Value v = value_number(-7.0f);
    TEST_ASSERT_EQUAL_STRING("-7", value_to_string(v));
}

void test_value_to_string_word(void)
{
    Node word = mem_atom("hello", 5);
    Value v = value_word(word);
    TEST_ASSERT_EQUAL_STRING("hello", value_to_string(v));
}

void test_value_to_string_empty_list(void)
{
    Value v = value_list(NODE_NIL);
    TEST_ASSERT_EQUAL_STRING("[]", value_to_string(v));
}

void test_value_to_string_single_item_list(void)
{
    Node word = mem_atom("hello", 5);
    Node list = mem_cons(word, NODE_NIL);
    Value v = value_list(list);
    TEST_ASSERT_EQUAL_STRING("[hello]", value_to_string(v));
}

void test_value_to_string_multi_item_list(void)
{
    Node word1 = mem_atom("hello", 5);
    Node word2 = mem_atom("world", 5);
    Node list = mem_cons(word1, mem_cons(word2, NODE_NIL));
    Value v = value_list(list);
    TEST_ASSERT_EQUAL_STRING("[hello world]", value_to_string(v));
}

void test_value_to_string_nested_list(void)
{
    Node word1 = mem_atom("a", 1);
    Node word2 = mem_atom("b", 1);
    Node inner = mem_cons(word2, NODE_NIL);
    Node list = mem_cons(word1, mem_cons(inner, NODE_NIL));
    Value v = value_list(list);
    TEST_ASSERT_EQUAL_STRING("[a [b]]", value_to_string(v));
}

//============================================================================
// Result Constructor Tests
//============================================================================

void test_result_ok_status(void)
{
    Value v = value_number(42.0f);
    Result r = result_ok(v);
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
}

void test_result_ok_value(void)
{
    Value v = value_number(42.0f);
    Result r = result_ok(v);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
}

void test_result_ok_no_throw_tag(void)
{
    Value v = value_number(42.0f);
    Result r = result_ok(v);
    TEST_ASSERT_NULL(r.throw_tag);
}

void test_result_none_status(void)
{
    Result r = result_none();
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_result_none_value(void)
{
    Result r = result_none();
    TEST_ASSERT_TRUE(value_is_none(r.value));
}

void test_result_none_no_throw_tag(void)
{
    Result r = result_none();
    TEST_ASSERT_NULL(r.throw_tag);
}

void test_result_stop_status(void)
{
    Result r = result_stop();
    TEST_ASSERT_EQUAL(RESULT_STOP, r.status);
}

void test_result_stop_value(void)
{
    Result r = result_stop();
    TEST_ASSERT_TRUE(value_is_none(r.value));
}

void test_result_stop_no_throw_tag(void)
{
    Result r = result_stop();
    TEST_ASSERT_NULL(r.throw_tag);
}

void test_result_output_status(void)
{
    Value v = value_number(99.0f);
    Result r = result_output(v);
    TEST_ASSERT_EQUAL(RESULT_OUTPUT, r.status);
}

void test_result_output_value(void)
{
    Value v = value_number(99.0f);
    Result r = result_output(v);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(99.0f, r.value.as.number);
}

void test_result_output_no_throw_tag(void)
{
    Value v = value_number(99.0f);
    Result r = result_output(v);
    TEST_ASSERT_NULL(r.throw_tag);
}

void test_result_error_status(void)
{
    Result r = result_error(42);
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_result_error_code(void)
{
    Result r = result_error(42);
    TEST_ASSERT_EQUAL(42, r.error_code);
}

void test_result_error_nulls(void)
{
    Result r = result_error(42);
    TEST_ASSERT_NULL(r.error_proc);
    TEST_ASSERT_NULL(r.error_arg);
    TEST_ASSERT_NULL(r.error_caller);
    TEST_ASSERT_NULL(r.throw_tag);
}

void test_result_throw_status(void)
{
    Result r = result_throw("toplevel");
    TEST_ASSERT_EQUAL(RESULT_THROW, r.status);
}

void test_result_throw_tag(void)
{
    Result r = result_throw("toplevel");
    TEST_ASSERT_EQUAL_STRING("toplevel", r.throw_tag);
}

void test_result_throw_value(void)
{
    Result r = result_throw("error");
    TEST_ASSERT_TRUE(value_is_none(r.value));
}

void test_result_error_arg_status(void)
{
    Result r = result_error_arg(41, "sum", "hello");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_result_error_arg_code(void)
{
    Result r = result_error_arg(41, "sum", "hello");
    TEST_ASSERT_EQUAL(41, r.error_code);
}

void test_result_error_arg_proc(void)
{
    Result r = result_error_arg(41, "sum", "hello");
    TEST_ASSERT_EQUAL_STRING("sum", r.error_proc);
}

void test_result_error_arg_arg(void)
{
    Result r = result_error_arg(41, "sum", "hello");
    TEST_ASSERT_EQUAL_STRING("hello", r.error_arg);
}

void test_result_error_arg_caller_null(void)
{
    Result r = result_error_arg(41, "sum", "hello");
    TEST_ASSERT_NULL(r.error_caller);
}

void test_result_error_in_sets_caller(void)
{
    Result r = result_error(42);
    r = result_error_in(r, "myproc");
    TEST_ASSERT_EQUAL_STRING("myproc", r.error_caller);
}

void test_result_error_in_preserves_existing_caller(void)
{
    Result r = result_error_arg(41, "sum", "hello");
    r.error_caller = "firstproc";
    r = result_error_in(r, "secondproc");
    // Should preserve the first caller
    TEST_ASSERT_EQUAL_STRING("firstproc", r.error_caller);
}

void test_result_error_in_non_error_unchanged(void)
{
    Result r = result_ok(value_number(42.0f));
    r = result_error_in(r, "myproc");
    // Non-error results should be unchanged
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
}

//============================================================================
// Result Predicate Tests
//============================================================================

void test_result_is_ok_true(void)
{
    Result r = result_ok(value_number(42.0f));
    TEST_ASSERT_TRUE(result_is_ok(r));
}

void test_result_is_ok_false_for_none(void)
{
    Result r = result_none();
    TEST_ASSERT_FALSE(result_is_ok(r));
}

void test_result_is_ok_false_for_stop(void)
{
    Result r = result_stop();
    TEST_ASSERT_FALSE(result_is_ok(r));
}

void test_result_is_ok_false_for_output(void)
{
    Result r = result_output(value_number(42.0f));
    TEST_ASSERT_FALSE(result_is_ok(r));
}

void test_result_is_ok_false_for_error(void)
{
    Result r = result_error(42);
    TEST_ASSERT_FALSE(result_is_ok(r));
}

void test_result_is_ok_false_for_throw(void)
{
    Result r = result_throw("toplevel");
    TEST_ASSERT_FALSE(result_is_ok(r));
}

void test_result_is_returnable_true_for_ok(void)
{
    Result r = result_ok(value_number(42.0f));
    TEST_ASSERT_TRUE(result_is_returnable(r));
}

void test_result_is_returnable_true_for_output(void)
{
    Result r = result_output(value_number(42.0f));
    TEST_ASSERT_TRUE(result_is_returnable(r));
}

void test_result_is_returnable_false_for_none(void)
{
    Result r = result_none();
    TEST_ASSERT_FALSE(result_is_returnable(r));
}

void test_result_is_returnable_false_for_stop(void)
{
    Result r = result_stop();
    TEST_ASSERT_FALSE(result_is_returnable(r));
}

void test_result_is_returnable_false_for_error(void)
{
    Result r = result_error(42);
    TEST_ASSERT_FALSE(result_is_returnable(r));
}

void test_result_is_returnable_false_for_throw(void)
{
    Result r = result_throw("toplevel");
    TEST_ASSERT_FALSE(result_is_returnable(r));
}

//============================================================================
// Main
//============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Value Constructor Tests
    RUN_TEST(test_value_none_type);
    RUN_TEST(test_value_number_type);
    RUN_TEST(test_value_number_content);
    RUN_TEST(test_value_number_negative);
    RUN_TEST(test_value_number_zero);
    RUN_TEST(test_value_word_type);
    RUN_TEST(test_value_word_content);
    RUN_TEST(test_value_list_type);
    RUN_TEST(test_value_list_content);
    RUN_TEST(test_value_list_empty);

    // Value Predicate Tests
    RUN_TEST(test_value_is_none_true);
    RUN_TEST(test_value_is_none_false_for_number);
    RUN_TEST(test_value_is_none_false_for_word);
    RUN_TEST(test_value_is_none_false_for_list);
    RUN_TEST(test_value_is_number_true);
    RUN_TEST(test_value_is_number_false_for_none);
    RUN_TEST(test_value_is_number_false_for_word);
    RUN_TEST(test_value_is_number_false_for_list);
    RUN_TEST(test_value_is_word_true);
    RUN_TEST(test_value_is_word_false_for_none);
    RUN_TEST(test_value_is_word_false_for_number);
    RUN_TEST(test_value_is_word_false_for_list);
    RUN_TEST(test_value_is_list_true);
    RUN_TEST(test_value_is_list_true_with_items);
    RUN_TEST(test_value_is_list_false_for_none);
    RUN_TEST(test_value_is_list_false_for_number);
    RUN_TEST(test_value_is_list_false_for_word);

    // Value Conversion Tests
    RUN_TEST(test_value_to_number_from_number);
    RUN_TEST(test_value_to_number_from_word_integer);
    RUN_TEST(test_value_to_number_from_word_float);
    RUN_TEST(test_value_to_number_from_word_negative);
    RUN_TEST(test_value_to_number_from_word_scientific);
    RUN_TEST(test_value_to_number_from_word_invalid);
    RUN_TEST(test_value_to_number_from_word_partial);
    RUN_TEST(test_value_to_number_from_none);
    RUN_TEST(test_value_to_number_from_list);
    RUN_TEST(test_value_to_node_from_word);
    RUN_TEST(test_value_to_node_from_list);
    RUN_TEST(test_value_to_node_from_empty_list);
    RUN_TEST(test_value_to_node_from_number);
    RUN_TEST(test_value_to_node_from_none);
    RUN_TEST(test_value_to_string_none);
    RUN_TEST(test_value_to_string_number_integer);
    RUN_TEST(test_value_to_string_number_float);
    RUN_TEST(test_value_to_string_number_negative);
    RUN_TEST(test_value_to_string_word);
    RUN_TEST(test_value_to_string_empty_list);
    RUN_TEST(test_value_to_string_single_item_list);
    RUN_TEST(test_value_to_string_multi_item_list);
    RUN_TEST(test_value_to_string_nested_list);

    // Result Constructor Tests
    RUN_TEST(test_result_ok_status);
    RUN_TEST(test_result_ok_value);
    RUN_TEST(test_result_ok_no_throw_tag);
    RUN_TEST(test_result_none_status);
    RUN_TEST(test_result_none_value);
    RUN_TEST(test_result_none_no_throw_tag);
    RUN_TEST(test_result_stop_status);
    RUN_TEST(test_result_stop_value);
    RUN_TEST(test_result_stop_no_throw_tag);
    RUN_TEST(test_result_output_status);
    RUN_TEST(test_result_output_value);
    RUN_TEST(test_result_output_no_throw_tag);
    RUN_TEST(test_result_error_status);
    RUN_TEST(test_result_error_code);
    RUN_TEST(test_result_error_nulls);
    RUN_TEST(test_result_throw_status);
    RUN_TEST(test_result_throw_tag);
    RUN_TEST(test_result_throw_value);
    RUN_TEST(test_result_error_arg_status);
    RUN_TEST(test_result_error_arg_code);
    RUN_TEST(test_result_error_arg_proc);
    RUN_TEST(test_result_error_arg_arg);
    RUN_TEST(test_result_error_arg_caller_null);
    RUN_TEST(test_result_error_in_sets_caller);
    RUN_TEST(test_result_error_in_preserves_existing_caller);
    RUN_TEST(test_result_error_in_non_error_unchanged);

    // Result Predicate Tests
    RUN_TEST(test_result_is_ok_true);
    RUN_TEST(test_result_is_ok_false_for_none);
    RUN_TEST(test_result_is_ok_false_for_stop);
    RUN_TEST(test_result_is_ok_false_for_output);
    RUN_TEST(test_result_is_ok_false_for_error);
    RUN_TEST(test_result_is_ok_false_for_throw);
    RUN_TEST(test_result_is_returnable_true_for_ok);
    RUN_TEST(test_result_is_returnable_true_for_output);
    RUN_TEST(test_result_is_returnable_false_for_none);
    RUN_TEST(test_result_is_returnable_false_for_stop);
    RUN_TEST(test_result_is_returnable_false_for_error);
    RUN_TEST(test_result_is_returnable_false_for_throw);

    return UNITY_END();
}
