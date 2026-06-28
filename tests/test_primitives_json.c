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

// Aux region so blob words (values over the 255-byte atom limit) can exist.
_Alignas(8) static uint8_t json_blob_region[1u << 16];

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

static void assert_number(Result r, float expected)
{
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_number(r.value));
    TEST_ASSERT_EQUAL_FLOAT(expected, r.value.as.number);
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

//==========================================================================
// Counting: json.count
//==========================================================================

void test_count_array_from_doc(void)
{
    make_doc();
    // tags is ["logo","c","rp2350"]
    assert_number(eval_string("json.count json.get :doc [tags]"), 3);
}

void test_count_array_of_objects(void)
{
    make_doc();
    // users is two objects; commas inside the objects must not be counted.
    assert_number(eval_string("json.count json.get :doc [users]"), 2);
}

void test_count_object_members(void)
{
    make_doc();
    // address has city and zip.
    assert_number(eval_string("json.count json.get :doc [address]"), 2);
}

void test_count_empty_array(void)
{
    assert_number(eval_string("json.count json.make (json.array)"), 0);
}

void test_count_nested_arrays_counts_top_level(void)
{
    // [[1,2],[3,4,5]] -> 2 top-level elements
    assert_number(eval_string("json.count json.make (json.array (json.array 1 2) (json.array 3 4 5))"), 2);
}

void test_count_ignores_separators_inside_strings(void)
{
    // ["a,b","c"] -> 2 (the comma inside the first string must not split).
    assert_number(eval_string("json.count json.make (json.array (word \"a char 44 \"b) \"c)"), 2);
}

void test_count_scalar_is_zero(void)
{
    make_doc();
    assert_number(eval_string("json.count json.get :doc [name]"), 0);
}

void test_count_missing_is_zero(void)
{
    make_doc();
    // json.get of a missing path is the empty list; counting it is 0.
    assert_number(eval_string("json.count json.get :doc [missing]"), 0);
}

void test_count_drives_iteration(void)
{
    make_doc();
    // Use json.count as a loop bound to pull each user's name.
    reset_output();
    run_string("for [i 1 [json.count json.get :doc [users]]] "
               "[type json.get :doc (list \"users :i \"name)]");
    TEST_ASSERT_EQUAL_STRING("AnnBob", output_buffer);
}

void test_count_rejects_non_empty_list(void)
{
    Result r = run_string("print json.count [a b c]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// Building: json.object / json.array / json.make
//==========================================================================

void test_make_simple_object(void)
{
    assert_word(eval_string("json.make (json.object \"name \"Blair \"age 42)"),
                "{\"name\":\"Blair\",\"age\":42}");
}

void test_make_empty_object(void)
{
    assert_word(eval_string("json.make (json.object)"), "{}");
}

void test_make_simple_array(void)
{
    assert_word(eval_string("json.make (json.array \"logo \"c)"), "[\"logo\",\"c\"]");
}

void test_make_empty_array(void)
{
    assert_word(eval_string("json.make (json.array)"), "[]");
}

void test_make_nested_object(void)
{
    assert_word(eval_string("json.make (json.object \"addr (json.object \"city \"Ottawa))"),
                "{\"addr\":{\"city\":\"Ottawa\"}}");
}

void test_make_object_with_array(void)
{
    assert_word(eval_string("json.make (json.object \"tags (json.array \"logo \"c))"),
                "{\"tags\":[\"logo\",\"c\"]}");
}

void test_make_boolean_value(void)
{
    assert_word(eval_string("json.make (json.object \"active \"true \"hidden \"false)"),
                "{\"active\":true,\"hidden\":false}");
}

void test_make_number_value_is_unquoted(void)
{
    assert_word(eval_string("json.make (json.object \"age 42)"), "{\"age\":42}");
}

void test_make_special_chars_in_value_survive(void)
{
    // The whole point of quoted-word builders: '/' is preserved (a list literal
    // would split text/plain).
    assert_word(eval_string("json.make (json.object \"type \"text/plain)"),
                "{\"type\":\"text/plain\"}");
}

void test_make_escapes_quote_and_newline(void)
{
    // Value a"b (quote via char 34).
    assert_word(eval_string("json.make (json.object \"q (word \"a char 34 \"b))"),
                "{\"q\":\"a\\\"b\"}");
    // Value a<newline>b (char 10).
    assert_word(eval_string("json.make (json.object \"n (word \"a char 10 \"b))"),
                "{\"n\":\"a\\nb\"}");
}

void test_make_plain_list_is_array(void)
{
    assert_word(eval_string("json.make [1 2 3]"), "[1,2,3]");
}

void test_make_empty_list_is_null(void)
{
    assert_word(eval_string("json.make []"), "null");
}

void test_make_top_level_scalars(void)
{
    assert_word(eval_string("json.make \"hello"), "\"hello\"");
    assert_word(eval_string("json.make 42"), "42");
}

void test_make_then_get_round_trips(void)
{
    run_string("make \"doc json.make (json.object \"name \"Blair \"age 42)");
    assert_word(eval_string("json.get :doc [name]"), "Blair");
    assert_word(eval_string("json.get :doc [age]"), "42");
}

void test_object_requires_even_args(void)
{
    Result r = run_string("print json.make (json.object \"name)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_ENOUGH_INPUTS, r.error_code);
}

void test_object_key_must_be_word(void)
{
    Result r = run_string("print json.make (json.object 5 \"v)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// Corrections from code review
//==========================================================================

void test_get_numeric_object_key(void)
{
    // Object keys that look like numbers must still be reachable.
    run_string("make \"d json.make (json.object \"1 \"one \"2 \"two)");
    assert_word(eval_string("json.get :d [1]"), "one");
    assert_word(eval_string("json.get :d [2]"), "two");
}

void test_get_integer_index_still_works(void)
{
    run_string("make \"arr json.make (json.array 10 20 30)");
    assert_word(eval_string("json.get :arr (list 2)"), "20");
}

void test_get_fractional_index_rejected(void)
{
    // A non-integer index must not silently truncate and select an element.
    run_string("make \"arr json.make (json.array 10 20 30)");
    assert_empty(eval_string("json.get :arr (list 1.5)"));
}

void test_make_small_number_is_valid_json(void)
{
    // Negative exponents must use JSON 'e-' syntax, not format_number's 'n'.
    Result r = eval_string("json.make 0.0000015");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    const char *s = mem_word_ptr(r.value.as.node);
    TEST_ASSERT_NOT_NULL(strstr(s, "e-"));
    TEST_ASSERT_NULL(strstr(s, "n"));
}

void test_make_small_number_in_object_is_valid_json(void)
{
    Result r = eval_string("json.make (json.object \"x 0.0000015)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    const char *s = mem_word_ptr(r.value.as.node);
    TEST_ASSERT_NOT_NULL(strstr(s, "e-"));
    TEST_ASSERT_NULL(strstr(s, "n"));
}

void test_make_negative_number(void)
{
    assert_word(eval_string("json.make (json.object \"t -5)"), "{\"t\":-5}");
}

void test_make_invalid_number_word_is_string(void)
{
    // A numeric-looking word that is not valid JSON (leading zero) is emitted
    // as a string, never as a malformed bare number.
    assert_word(eval_string("json.make \"007"), "\"007\"");
}

void test_get_unescapes_bmp_escape(void)
{
    // {"c":"é"} -> é (UTF-8 C3 A9)
    static const char doc[] = "{\"c\":\"\\u00e9\"}";
    Node n = mem_word(doc, sizeof(doc) - 1);
    TEST_ASSERT_TRUE(var_set("acc", value_word(n)));
    Result r = eval_string("json.get :acc [c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("\xC3\xA9", mem_word_ptr(r.value.as.node));
}

void test_get_unescapes_surrogate_pair(void)
{
    // {"e":"😀"} -> U+1F600 emoji as 4-byte UTF-8 (F0 9F 98 80)
    static const char doc[] = "{\"e\":\"\\uD83D\\uDE00\"}";
    Node n = mem_word(doc, sizeof(doc) - 1);
    TEST_ASSERT_TRUE(var_set("emoji", value_word(n)));
    Result r = eval_string("json.get :emoji [e]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("\xF0\x9F\x98\x80", mem_word_ptr(r.value.as.node));
}

void test_get_malformed_value_is_empty(void)
{
    // A member with no value ({"a":}) must yield the empty list, not an empty
    // word interned from a non-advancing scan.
    static const char doc[] = "{\"a\":}";
    Node n = mem_word(doc, sizeof(doc) - 1);
    TEST_ASSERT_TRUE(var_set("m", value_word(n)));
    assert_empty(eval_string("json.get :m [a]"));
}

void test_make_tolerates_non_word_key(void)
{
    // Fabricate a tagged-object AST whose key is a list rather than a word, and
    // ensure json.make does not dereference a NULL key pointer.
    Node tag = mem_atom("\x01o", 2);
    Node key = mem_cons(mem_atom_cstr("x"), NODE_NIL); // [x] -- not a word
    Node ast = mem_cons(tag, mem_cons(key, mem_cons(mem_atom_cstr("v"), NODE_NIL)));
    TEST_ASSERT_TRUE(var_set("bad", value_list(ast)));
    Result r = eval_string("json.make :bad");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
}

void test_object_rejects_blob_value(void)
{
    // A value over the 255-byte atom limit is a blob, which cannot live in a
    // cons cell; the builder must error rather than silently drop it.
    logo_mem_set_aux_region(json_blob_region, sizeof(json_blob_region));
    char big[400];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    Node blob = mem_word(big, sizeof(big) - 1);
    TEST_ASSERT_FALSE(mem_is_nil(blob));
    TEST_ASSERT_TRUE(var_set("big", value_word(blob)));

    Result r = run_string("print json.make (json.object \"k :big)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
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

    RUN_TEST(test_count_array_from_doc);
    RUN_TEST(test_count_array_of_objects);
    RUN_TEST(test_count_object_members);
    RUN_TEST(test_count_empty_array);
    RUN_TEST(test_count_nested_arrays_counts_top_level);
    RUN_TEST(test_count_ignores_separators_inside_strings);
    RUN_TEST(test_count_scalar_is_zero);
    RUN_TEST(test_count_missing_is_zero);
    RUN_TEST(test_count_drives_iteration);
    RUN_TEST(test_count_rejects_non_empty_list);

    RUN_TEST(test_get_numeric_object_key);
    RUN_TEST(test_get_integer_index_still_works);
    RUN_TEST(test_get_fractional_index_rejected);
    RUN_TEST(test_make_small_number_is_valid_json);
    RUN_TEST(test_make_small_number_in_object_is_valid_json);
    RUN_TEST(test_make_negative_number);
    RUN_TEST(test_make_invalid_number_word_is_string);
    RUN_TEST(test_get_unescapes_bmp_escape);
    RUN_TEST(test_get_unescapes_surrogate_pair);
    RUN_TEST(test_get_malformed_value_is_empty);
    RUN_TEST(test_make_tolerates_non_word_key);
    RUN_TEST(test_object_rejects_blob_value);

    RUN_TEST(test_make_simple_object);
    RUN_TEST(test_make_empty_object);
    RUN_TEST(test_make_simple_array);
    RUN_TEST(test_make_empty_array);
    RUN_TEST(test_make_nested_object);
    RUN_TEST(test_make_object_with_array);
    RUN_TEST(test_make_boolean_value);
    RUN_TEST(test_make_number_value_is_unquoted);
    RUN_TEST(test_make_special_chars_in_value_survive);
    RUN_TEST(test_make_escapes_quote_and_newline);
    RUN_TEST(test_make_plain_list_is_array);
    RUN_TEST(test_make_empty_list_is_null);
    RUN_TEST(test_make_top_level_scalars);
    RUN_TEST(test_make_then_get_round_trips);
    RUN_TEST(test_object_requires_even_args);
    RUN_TEST(test_object_key_must_be_word);

    return UNITY_END();
}
