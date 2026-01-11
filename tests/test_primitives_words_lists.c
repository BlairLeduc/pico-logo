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
// First, Last, Butfirst, Butlast Tests
//==========================================================================

void test_first_number(void)
{
    // print first 12.345 outputs "1"
    run_string("print first 12.345");
    TEST_ASSERT_EQUAL_STRING("1\n", output_buffer);
}

void test_first_word(void)
{
    Result r = eval_string("first \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("H", mem_word_ptr(r.value.as.node));
}

void test_first_list(void)
{
    Result r = eval_string("first [apple banana cherry]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("apple", mem_word_ptr(r.value.as.node));
}

void test_last_word(void)
{
    Result r = eval_string("last \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("E", mem_word_ptr(r.value.as.node));
}

void test_butfirst_word(void)
{
    Result r = eval_string("bf \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("OUSE", mem_word_ptr(r.value.as.node));
}

void test_butfirst_empty_word_error(void)
{
    Result r = eval_string("bf \"");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_butfirst_empty_list_error(void)
{
    Result r = eval_string("bf []");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_butlast_word(void)
{
    Result r = eval_string("bl \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("HOUS", mem_word_ptr(r.value.as.node));
}

void test_butlast_empty_word_error(void)
{
    Result r = eval_string("bl \"");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_butlast_empty_list_error(void)
{
    Result r = eval_string("bl []");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// Item Tests
//==========================================================================

void test_item_word(void)
{
    Result r = eval_string("item 3 \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("U", mem_word_ptr(r.value.as.node));
}

void test_item_list(void)
{
    Result r = eval_string("item 2 [apple banana cherry]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("banana", mem_word_ptr(r.value.as.node));
}

void test_item_number(void)
{
    Result r = eval_string("item 2 123");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// Replace Tests
//==========================================================================

void test_replace_word(void)
{
    // pr replace 2 "dig "u outputs "dug"
    run_string("print replace 2 \"dig \"u");
    TEST_ASSERT_EQUAL_STRING("dug\n", output_buffer);
}

void test_replace_list(void)
{
    // show replace 4 [a b c d] "x outputs [a b c x]
    Result r = eval_string("replace 4 [a b c d] \"x");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    // Verify [a b c x]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("b", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("c", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("x", mem_word_ptr(mem_car(list)));
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(list)));
}

void test_replace_capitalize_first_char(void)
{
    // make "greet "hello
    // pr replace 1 :greet uppercase item 1 :greet outputs "Hello"
    run_string("make \"greet \"hello\nprint replace 1 :greet uppercase item 1 :greet");
    TEST_ASSERT_EQUAL_STRING("Hello\n", output_buffer);
}

void test_replace_number(void)
{
    // replace 2 123 "x outputs "1x3"
    run_string("print replace 2 123 \"x");
    TEST_ASSERT_EQUAL_STRING("1x3\n", output_buffer);
}

void test_replace_index_out_of_bounds(void)
{
    // Index greater than length
    Result r = eval_string("replace 5 \"abc \"x");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_TOO_FEW_ITEMS, r.error_code);
}

void test_replace_empty_word_error(void)
{
    Result r = eval_string("replace 1 \" \"x");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_TOO_FEW_ITEMS, r.error_code);
}

void test_replace_empty_list_error(void)
{
    Result r = eval_string("replace 1 [] \"x");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_TOO_FEW_ITEMS, r.error_code);
}

void test_replace_invalid_index_zero(void)
{
    Result r = eval_string("replace 0 \"abc \"x");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_replace_invalid_index_negative(void)
{
    Result r = eval_string("replace -1 \"abc \"x");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

//==========================================================================
// Count and Empty Tests
//==========================================================================

void test_count_word(void)
{
    Result r = eval_string("count \"HOUSE");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, r.value.as.number);
}

void test_count_list(void)
{
    Result r = eval_string("count [a b c d]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, r.value.as.number);
}

void test_emptyp_empty_list(void)
{
    Result r = eval_string("emptyp []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_emptyp_nonempty_list(void)
{
    Result r = eval_string("emptyp [a]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_empty_list(void)
{
    Result r = eval_string("[]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_list_with_words(void)
{
    Result r = eval_string("[hello world]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_FALSE(mem_is_nil(r.value.as.node));
}

//==========================================================================
// List Construction Tests (fput, list, lput, sentence)
//==========================================================================

void test_fput(void)
{
    Result r = eval_string("fput \"a [b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    // Check first element is "a"
    Node first = mem_car(r.value.as.node);
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(first));
}

void test_lput(void)
{
    Result r = eval_string("lput \"c [a b]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    // Check last element is "c"
    Node list = r.value.as.node;
    while (!mem_is_nil(mem_cdr(list)))
    {
        list = mem_cdr(list);
    }
    Node last = mem_car(list);
    TEST_ASSERT_EQUAL_STRING("c", mem_word_ptr(last));
}

void test_list_operation(void)
{
    Result r = eval_string("list \"a \"b");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    // Check first element is "a"
    Node first = mem_car(r.value.as.node);
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(first));
    // Check second element is "b"
    Node second = mem_car(mem_cdr(r.value.as.node));
    TEST_ASSERT_EQUAL_STRING("b", mem_word_ptr(second));
}

void test_sentence(void)
{
    Result r = eval_string("sentence \"a [b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    // Should be [a b c]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("b", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("c", mem_word_ptr(mem_car(list)));
}

void test_se_alias(void)
{
    Result r = eval_string("se \"a \"b");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
}

//==========================================================================
// Word Operation Tests
//==========================================================================

void test_word_operation(void)
{
    Result r = eval_string("word \"hello \"world");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("helloworld", mem_word_ptr(r.value.as.node));
}

void test_parse(void)
{
    // parse "hello world" should give [hello world]
    Result r = eval_string("parse \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    // Should be [hello]
    Node first = mem_car(r.value.as.node);
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(first));
}

//==========================================================================
// Character Operation Tests (ascii, char)
//==========================================================================

void test_ascii(void)
{
    Result r = eval_string("ascii \"A");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_NUMBER, r.value.type);
    TEST_ASSERT_EQUAL_FLOAT(65.0f, r.value.as.number);
}

void test_char(void)
{
    Result r = eval_string("char 65");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("A", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// Case Conversion Tests
//==========================================================================

void test_lowercase(void)
{
    Result r = eval_string("lowercase \"HELLO");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(r.value.as.node));
}

void test_uppercase(void)
{
    Result r = eval_string("uppercase \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("HELLO", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// Comparison Tests (before?, equal?)
//==========================================================================

void test_beforep_true(void)
{
    Result r = eval_string("before? \"apple \"banana");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_beforep_false(void)
{
    Result r = eval_string("before? \"banana \"apple");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_equalp_words_true(void)
{
    Result r = eval_string("equal? \"hello \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_equalp_words_false(void)
{
    Result r = eval_string("equal? \"hello \"world");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_equalp_numbers(void)
{
    Result r = eval_string("equal? 42 42");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_equalp_lists(void)
{
    Result r = eval_string("equal? [a b] [a b]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// Type Predicate Tests
//==========================================================================

void test_listp_true(void)
{
    Result r = eval_string("list? [a b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_listp_false(void)
{
    Result r = eval_string("list? \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_wordp_true(void)
{
    Result r = eval_string("word? \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_wordp_number(void)
{
    // Numbers are also words (self-quoting)
    Result r = eval_string("word? 42");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_wordp_false(void)
{
    Result r = eval_string("word? [a b]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_numberp_true(void)
{
    Result r = eval_string("number? 42");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_numberp_word_number(void)
{
    // A word that can be parsed as number
    Result r = eval_string("number? \"42");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_numberp_false(void)
{
    Result r = eval_string("number? \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

//==========================================================================
// Member Tests
//==========================================================================

void test_member_word(void)
{
    Result r = eval_string("member \"b \"abc");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("bc", mem_word_ptr(r.value.as.node));
}

void test_member_list(void)
{
    Result r = eval_string("member \"b [a b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    // Should return [b c]
    Node first = mem_car(r.value.as.node);
    TEST_ASSERT_EQUAL_STRING("b", mem_word_ptr(first));
}

void test_member_not_found(void)
{
    Result r = eval_string("member \"x [a b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_memberp_word_true(void)
{
    Result r = eval_string("member? \"b \"abc");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_memberp_list_true(void)
{
    Result r = eval_string("member? \"b [a b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_memberp_list_false(void)
{
    Result r = eval_string("member? \"x [a b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

int main(void)
{
    UNITY_BEGIN();

    // First, last, butfirst, butlast
    RUN_TEST(test_first_number);
    RUN_TEST(test_first_word);
    RUN_TEST(test_first_list);
    RUN_TEST(test_last_word);
    RUN_TEST(test_butfirst_word);
    RUN_TEST(test_butfirst_empty_word_error);
    RUN_TEST(test_butfirst_empty_list_error);
    RUN_TEST(test_butlast_word);
    RUN_TEST(test_butlast_empty_word_error);
    RUN_TEST(test_butlast_empty_list_error);
    
    // Item
    RUN_TEST(test_item_word);
    RUN_TEST(test_item_list);
    RUN_TEST(test_item_number);
    
    // Replace
    RUN_TEST(test_replace_word);
    RUN_TEST(test_replace_list);
    RUN_TEST(test_replace_capitalize_first_char);
    RUN_TEST(test_replace_number);
    RUN_TEST(test_replace_index_out_of_bounds);
    RUN_TEST(test_replace_empty_word_error);
    RUN_TEST(test_replace_empty_list_error);
    RUN_TEST(test_replace_invalid_index_zero);
    RUN_TEST(test_replace_invalid_index_negative);
    
    // Count and empty
    RUN_TEST(test_count_word);
    RUN_TEST(test_count_list);
    RUN_TEST(test_emptyp_empty_list);
    RUN_TEST(test_emptyp_nonempty_list);
    RUN_TEST(test_empty_list);
    RUN_TEST(test_list_with_words);
    
    // List construction
    RUN_TEST(test_fput);
    RUN_TEST(test_lput);
    RUN_TEST(test_list_operation);
    RUN_TEST(test_sentence);
    RUN_TEST(test_se_alias);
    
    // Word operations
    RUN_TEST(test_word_operation);
    RUN_TEST(test_parse);
    
    // Character operations
    RUN_TEST(test_ascii);
    RUN_TEST(test_char);
    
    // Case conversion
    RUN_TEST(test_lowercase);
    RUN_TEST(test_uppercase);
    
    // Comparison
    RUN_TEST(test_beforep_true);
    RUN_TEST(test_beforep_false);
    RUN_TEST(test_equalp_words_true);
    RUN_TEST(test_equalp_words_false);
    RUN_TEST(test_equalp_numbers);
    RUN_TEST(test_equalp_lists);
    
    // Type predicates
    RUN_TEST(test_listp_true);
    RUN_TEST(test_listp_false);
    RUN_TEST(test_wordp_true);
    RUN_TEST(test_wordp_number);
    RUN_TEST(test_wordp_false);
    RUN_TEST(test_numberp_true);
    RUN_TEST(test_numberp_word_number);
    RUN_TEST(test_numberp_false);
    
    // Member
    RUN_TEST(test_member_word);
    RUN_TEST(test_member_list);
    RUN_TEST(test_member_not_found);
    RUN_TEST(test_memberp_word_true);
    RUN_TEST(test_memberp_list_true);
    RUN_TEST(test_memberp_list_false);

    return UNITY_END();
}
