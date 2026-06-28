//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include "core/variables.h"
#include <string.h>

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

// A JSON document, as text. Bound to the Logo variable :doc for each test.
// Raw JSON cannot be written as a Logo word literal (it contains brackets,
// quotes and spaces), so it is interned in C. "\\t" is a real backslash-t in
// the bytes, i.e. a JSON \t escape to exercise unescaping.
static const char DOC[] =
    "{"
        "\"name\":\"Blair\","
        "\"age\":42,"
        "\"address\":{\"city\":\"Ottawa\",\"zip\":\"K1A\"},"
        "\"tags\":[\"logo\",\"c\",\"rp2350\"],"
        "\"users\":[{\"name\":\"Ann\",\"age\":30},{\"name\":\"Bob\",\"age\":25}],"
        "\"active\":true,"
        "\"nickname\":null,"
        "\"greeting\":\"hi\\tthere\""
    "}";

static void make_doc(void)
{
    Node n = mem_word(DOC, sizeof(DOC) - 1);
    TEST_ASSERT_FALSE(mem_is_nil(n));
    TEST_ASSERT_TRUE(var_set("doc", value_word(n)));
}

static void assert_word(Result r, const char *expected)
{
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_TRUE(mem_word_eq(r.value.as.node, expected, strlen(expected)));
}

static void assert_empty(Result r)
{
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

//==========================================================================
// Scalar extraction
//==========================================================================

void test_top_level_string(void)
{
    make_doc();
    assert_word(eval_string("json.get :doc [name]"), "Blair");
}

void test_top_level_number(void)
{
    make_doc();
    assert_word(eval_string("json.get :doc [age]"), "42");
}

void test_number_is_usable_as_a_number(void)
{
    make_doc();
    // The numeric word coerces in arithmetic.
    Result r = eval_string("sum json.get :doc [age] 8");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(50.0, r.value.as.number);
}

void test_boolean_true(void)
{
    make_doc();
    assert_word(eval_string("json.get :doc [active]"), "true");
}

void test_null_is_empty_list(void)
{
    make_doc();
    assert_empty(eval_string("json.get :doc [nickname]"));
}

void test_string_is_unescaped(void)
{
    make_doc();
    // JSON "hi\tthere" -> Logo word with a real tab.
    assert_word(eval_string("json.get :doc [greeting]"), "hi\tthere");
}

//==========================================================================
// Object / array navigation
//==========================================================================

void test_nested_object_key(void)
{
    make_doc();
    assert_word(eval_string("json.get :doc [address city]"), "Ottawa");
}

void test_array_index_is_one_based(void)
{
    make_doc();
    assert_word(eval_string("json.get :doc [tags 1]"), "logo");
    assert_word(eval_string("json.get :doc [tags 3]"), "rp2350");
}

void test_array_of_objects(void)
{
    make_doc();
    assert_word(eval_string("json.get :doc [users 1 name]"), "Ann");
    assert_word(eval_string("json.get :doc [users 2 age]"), "25");
}

//==========================================================================
// Nested containers come back as raw, re-queryable JSON text
//==========================================================================

void test_object_returned_as_raw_text(void)
{
    make_doc();
    assert_word(eval_string("json.get :doc [address]"),
                "{\"city\":\"Ottawa\",\"zip\":\"K1A\"}");
}

void test_extracted_object_is_requeryable(void)
{
    make_doc();
    // json.get the [address] sub-text, then json.get [zip] from it.
    assert_word(eval_string("json.get json.get :doc [address] [zip]"), "K1A");
}

void test_extracted_array_is_requeryable(void)
{
    make_doc();
    assert_word(eval_string("json.get json.get :doc [tags] [2]"), "c");
}

//==========================================================================
// Keys are case-sensitive (JSON spec)
//==========================================================================

void test_key_match_is_case_sensitive(void)
{
    make_doc();
    assert_word(eval_string("json.get :doc [name]"), "Blair");
    assert_empty(eval_string("json.get :doc [Name]"));
    assert_empty(eval_string("json.get :doc [NAME]"));
}

//==========================================================================
// Missing paths yield the empty list
//==========================================================================

void test_missing_top_level_key(void)
{
    make_doc();
    assert_empty(eval_string("json.get :doc [missing]"));
}

void test_missing_nested_key(void)
{
    make_doc();
    assert_empty(eval_string("json.get :doc [address country]"));
}

void test_index_out_of_range(void)
{
    make_doc();
    assert_empty(eval_string("json.get :doc [tags 9]"));
}

void test_index_below_one(void)
{
    make_doc();
    assert_empty(eval_string("json.get :doc [tags 0]"));
}

void test_descending_into_scalar(void)
{
    make_doc();
    // name is a string; the path cannot continue into it.
    assert_empty(eval_string("json.get :doc [name first]"));
}

void test_key_lookup_on_array_fails(void)
{
    make_doc();
    // tags is an array; a word step does not apply.
    assert_empty(eval_string("json.get :doc [tags name]"));
}

//==========================================================================
// Edge cases
//==========================================================================

void test_empty_path_returns_whole_document(void)
{
    make_doc();
    Result r = eval_string("json.get :doc []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_TRUE(mem_word_eq(r.value.as.node, DOC, sizeof(DOC) - 1));
}

//==========================================================================
// Error handling
//==========================================================================

void test_document_must_be_a_word(void)
{
    Result r = run_string("print json.get [a b] [x]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_path_must_be_a_list(void)
{
    make_doc();
    Result r = run_string("print json.get :doc \"name");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_path_step_must_be_a_word(void)
{
    make_doc();
    Result r = run_string("print json.get :doc [[a b]]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_top_level_string);
    RUN_TEST(test_top_level_number);
    RUN_TEST(test_number_is_usable_as_a_number);
    RUN_TEST(test_boolean_true);
    RUN_TEST(test_null_is_empty_list);
    RUN_TEST(test_string_is_unescaped);

    RUN_TEST(test_nested_object_key);
    RUN_TEST(test_array_index_is_one_based);
    RUN_TEST(test_array_of_objects);

    RUN_TEST(test_object_returned_as_raw_text);
    RUN_TEST(test_extracted_object_is_requeryable);
    RUN_TEST(test_extracted_array_is_requeryable);

    RUN_TEST(test_key_match_is_case_sensitive);

    RUN_TEST(test_missing_top_level_key);
    RUN_TEST(test_missing_nested_key);
    RUN_TEST(test_index_out_of_range);
    RUN_TEST(test_index_below_one);
    RUN_TEST(test_descending_into_scalar);
    RUN_TEST(test_key_lookup_on_array_fails);

    RUN_TEST(test_empty_path_returns_whole_document);

    RUN_TEST(test_document_must_be_a_word);
    RUN_TEST(test_path_must_be_a_list);
    RUN_TEST(test_path_step_must_be_a_word);

    return UNITY_END();
}
