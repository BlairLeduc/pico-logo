//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//

#include "test_scaffold.h"
#include "core/parse_list.h"
#include <string.h>
#include <stdio.h>

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

void test_item_is_one_indexed(void)
{
    // The reference defines `item` as 1-indexed: `item 1 [a b c]` -> a.
    // Pin this with a list and a word.
    Result r1 = eval_string("item 1 [a b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r1.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r1.value.type);
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(r1.value.as.node));

    Result r2 = eval_string("item 1 \"abc");
    TEST_ASSERT_EQUAL(RESULT_OK, r2.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r2.value.type);
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(r2.value.as.node));
}

void test_item_zero_index_errors(void)
{
    // 0 is below the valid range; should error per 1-indexed semantics.
    Result r = eval_string("item 0 [a b c]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
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
// .setfirst (destructive in-place car) Tests
//==========================================================================

void test_dsetfirst_mutates_in_place(void)
{
    // .setfirst mutates the same cell the variable holds, so :l changes
    // without rebuilding the list.
    run_string("make \"l [a b c]\n.setfirst :l \"x\nprint :l");
    TEST_ASSERT_EQUAL_STRING("x b c\n", output_buffer);
}

void test_dsetfirst_number_value(void)
{
    // A numeric value is stored in word form, so it compares equal to a number.
    run_string("make \"l [1 1 1]\n.setfirst :l 0\nprint first :l");
    TEST_ASSERT_EQUAL_STRING("0\n", output_buffer);
}

void test_dsetfirst_affects_shared_tail(void)
{
    // A cursor obtained via butfirst shares structure; mutating through it
    // is visible in the original list. This is the property invaders relies
    // on to flip one alien's liveness flag with no allocation.
    run_string("make \"l [1 1 1 1]\nmake \"c butfirst butfirst :l\n"
               ".setfirst :c 0\nprint :l");
    TEST_ASSERT_EQUAL_STRING("1 1 0 1\n", output_buffer);
}

void test_dsetfirst_empty_list_error(void)
{
    Result r = eval_string(".setfirst [] \"x");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_dsetfirst_non_list_error(void)
{
    Result r = eval_string(".setfirst \"abc \"x");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_dsetbf_replaces_tail(void)
{
    // .setbf swaps the whole tail in place, keeping the first member.
    run_string("make \"l [a b c]\n.setbf :l [x y]\nprint :l");
    TEST_ASSERT_EQUAL_STRING("a x y\n", output_buffer);
}

void test_dsetbf_truncates_with_empty_tail(void)
{
    // Setting the butfirst to [] turns the list into a single-element list.
    run_string("make \"l [a b c]\n.setbf :l []\nprint :l");
    TEST_ASSERT_EQUAL_STRING("a\n", output_buffer);
}

void test_dsetbf_requires_list_value(void)
{
    Result r = eval_string(".setbf [a b] \"x");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_dsetbf_empty_list_error(void)
{
    Result r = eval_string(".setbf [] [x]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, r.error_code);
}

void test_dsetitem_mutates_nth(void)
{
    // .setitem sets the index-th member in place (1-based).
    run_string("make \"l [a b c d]\n.setitem 3 :l \"x\nprint :l");
    TEST_ASSERT_EQUAL_STRING("a b x d\n", output_buffer);
}

void test_dsetitem_number_value(void)
{
    run_string("make \"l [1 1 1]\n.setitem 2 :l 0\nprint item 2 :l");
    TEST_ASSERT_EQUAL_STRING("0\n", output_buffer);
}

void test_dsetitem_index_out_of_bounds(void)
{
    Result r = eval_string(".setitem 5 [a b c] \"x");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_TOO_FEW_ITEMS, r.error_code);
}

void test_dsetitem_index_zero_error(void)
{
    Result r = eval_string(".setitem 0 [a b c] \"x");
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

void test_word_overflow_returns_error(void)
{
    // Without a PSRAM aux region (the default test setup, and the Pico 2 /
    // Pico 2 W boards), a result beyond the 255-char atom cap cannot be
    // blobbed, so it must raise ERR_OUT_OF_SPACE rather than truncate.
    // Build a 200-char word and concatenate it with itself => 400 chars.
    char big[256];
    memset(big, 'a', 200);
    big[200] = '\0';

    char src[600];
    snprintf(src, sizeof(src), "(word \"%s \"%s)", big, big);

    Result r = eval_string(src);
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_word_at_atom_limit_succeeds(void)
{
    // Exactly 255 chars total must still succeed (the cap is inclusive).
    char a[200];
    memset(a, 'a', 199); a[199] = '\0';
    char b[60];
    memset(b, 'b', 56); b[56] = '\0'; // 199 + 56 = 255

    char src[600];
    snprintf(src, sizeof(src), "(word \"%s \"%s)", a, b);
    Result r = eval_string(src);
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_size_t(255, mem_word_len(r.value.as.node));
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

// Direct exercise of the shared parser used by both `parse` and `readlist`.
// This is the cleanest regression for the previous "parse drops brackets"
// bug because Logo's quoted-word syntax stops at `[` and `]`, so the
// `parse` primitive itself can't easily be handed a bracket-containing
// word from a Logo source line.
void test_parse_list_from_string_nested(void)
{
    ParseListResult r = parse_list_from_string("a [b c] d");
    TEST_ASSERT_TRUE(r.success);

    Node first = mem_car(r.node);
    TEST_ASSERT_TRUE(mem_is_word(first));
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(first));

    Node second_cell = mem_cdr(r.node);
    Node sub = mem_car(second_cell);
    TEST_ASSERT_TRUE(mem_is_list(sub));
    TEST_ASSERT_EQUAL_STRING("b", mem_word_ptr(mem_car(sub)));
    TEST_ASSERT_EQUAL_STRING("c", mem_word_ptr(mem_car(mem_cdr(sub))));
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(mem_cdr(sub))));

    Node third_cell = mem_cdr(second_cell);
    TEST_ASSERT_EQUAL_STRING("d", mem_word_ptr(mem_car(third_cell)));
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(third_cell)));
}

void test_parse_list_from_string_deeply_nested(void)
{
    ParseListResult r = parse_list_from_string("[a [b [c]]]");
    TEST_ASSERT_TRUE(r.success);

    // One outer element: the [a [b [c]]] sublist.
    Node outer = mem_car(r.node);
    TEST_ASSERT_TRUE(mem_is_list(outer));
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(r.node)));

    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(mem_car(outer)));
    Node level1 = mem_car(mem_cdr(outer));
    TEST_ASSERT_TRUE(mem_is_list(level1));

    TEST_ASSERT_EQUAL_STRING("b", mem_word_ptr(mem_car(level1)));
    Node level2 = mem_car(mem_cdr(level1));
    TEST_ASSERT_TRUE(mem_is_list(level2));

    TEST_ASSERT_EQUAL_STRING("c", mem_word_ptr(mem_car(level2)));
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(level2)));
}

// Empty bracketed group must be encoded so it survives mem_is_list checks.
void test_parse_list_from_string_empty_sublist(void)
{
    ParseListResult r = parse_list_from_string("x [] y");
    TEST_ASSERT_TRUE(r.success);

    TEST_ASSERT_EQUAL_STRING("x", mem_word_ptr(mem_car(r.node)));

    Node empty_cell = mem_cdr(r.node);
    Node empty_sub = mem_car(empty_cell);
    TEST_ASSERT_TRUE(mem_is_list(empty_sub));
    TEST_ASSERT_TRUE(mem_is_nil(empty_sub));

    Node y_cell = mem_cdr(empty_cell);
    TEST_ASSERT_EQUAL_STRING("y", mem_word_ptr(mem_car(y_cell)));
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(y_cell)));
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
// pick / reverse / shuffle Tests
//==========================================================================
// The mock device's random() returns a fixed 42, so pick is deterministic:
// the chosen index is 42 % count.

void test_pick_list(void)
{
    // 42 % 3 = 0 -> first element
    Result r = eval_string("pick [a b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(r.value.as.node));

    // 42 % 4 = 2 -> third element
    r = eval_string("pick [a b c d]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("c", mem_word_ptr(r.value.as.node));
}

void test_pick_list_element_can_be_list(void)
{
    // 42 % 4 = 2 -> third element, a sublist
    Result r = eval_string("pick [a b [c d] e]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
}

void test_pick_word(void)
{
    // 42 % 3 = 0 -> first character
    Result r = eval_string("pick \"xyz");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("x", mem_word_ptr(r.value.as.node));
}

void test_pick_number(void)
{
    // Numbers are words: 42 % 3 = 0 -> first digit
    Result r = eval_string("pick 987");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("9", mem_word_ptr(r.value.as.node));
}

void test_pick_empty_errors(void)
{
    Result r = eval_string("pick []");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_TOO_FEW_ITEMS, r.error_code);

    r = eval_string("pick \"");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_TOO_FEW_ITEMS, r.error_code);
}

void test_reverse_list(void)
{
    reset_output();
    run_string("show reverse [a b c d]");
    TEST_ASSERT_EQUAL_STRING("[d c b a]\n", output_buffer);
}

void test_reverse_list_keeps_sublists_intact(void)
{
    reset_output();
    run_string("show reverse [a [b c] d]");
    TEST_ASSERT_EQUAL_STRING("[d [b c] a]\n", output_buffer);
}

void test_reverse_word(void)
{
    Result r = eval_string("reverse \"stressed");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("desserts", mem_word_ptr(r.value.as.node));
}

void test_reverse_number(void)
{
    Result r = eval_string("reverse 123");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("321", mem_word_ptr(r.value.as.node));
}

void test_reverse_empty(void)
{
    Result r = eval_string("reverse []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));

    r = eval_string("reverse \"");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(0, mem_word_len(r.value.as.node));
}

void test_shuffle_list_is_permutation(void)
{
    reset_output();
    run_string("make \"s shuffle [a b c d e]");
    run_string("print count :s");
    TEST_ASSERT_EQUAL_STRING("5\n", output_buffer);

    // Every original element is still present
    reset_output();
    run_string("print member? \"a :s");
    run_string("print member? \"b :s");
    run_string("print member? \"c :s");
    run_string("print member? \"d :s");
    run_string("print member? \"e :s");
    TEST_ASSERT_EQUAL_STRING("true\ntrue\ntrue\ntrue\ntrue\n", output_buffer);
}

void test_shuffle_word_is_permutation(void)
{
    Result r = eval_string("shuffle \"abc");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(3, mem_word_len(r.value.as.node));

    reset_output();
    run_string("make \"w shuffle \"abc");
    run_string("print member? \"a :w");
    run_string("print member? \"b :w");
    run_string("print member? \"c :w");
    TEST_ASSERT_EQUAL_STRING("true\ntrue\ntrue\n", output_buffer);
}

void test_shuffle_empty(void)
{
    Result r = eval_string("shuffle []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

//==========================================================================
// remove / remdup
//==========================================================================

void test_remove_list(void)
{
    reset_output();
    run_string("show remove \"b [a b c b d]");
    TEST_ASSERT_EQUAL_STRING("[a c d]\n", output_buffer);
}

void test_remove_list_no_match_copies(void)
{
    reset_output();
    run_string("show remove \"z [a b c]");
    TEST_ASSERT_EQUAL_STRING("[a b c]\n", output_buffer);
}

void test_remove_list_numbers(void)
{
    reset_output();
    run_string("show remove 5 [1 5 3 5 2]");
    TEST_ASSERT_EQUAL_STRING("[1 3 2]\n", output_buffer);
}

void test_remove_list_sublist_element(void)
{
    reset_output();
    run_string("show remove [b c] [a [b c] d [b c]]");
    TEST_ASSERT_EQUAL_STRING("[a d]\n", output_buffer);
}

void test_remove_word(void)
{
    Result r = eval_string("remove \"l \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("heo", mem_word_ptr(r.value.as.node));
}

void test_remove_word_is_case_insensitive(void)
{
    // equal? ignores case, so removing "l also removes "L.
    Result r = eval_string("remove \"l \"heLlo");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("heo", mem_word_ptr(r.value.as.node));
}

void test_remove_word_multichar_thing_never_matches(void)
{
    // A word's members are single characters, so a two-character thing
    // matches nothing and the word is copied unchanged.
    Result r = eval_string("remove \"ll \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(r.value.as.node));
}

void test_remove_empty(void)
{
    Result r = eval_string("remove \"x []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_remdup_list_keeps_last(void)
{
    // When members are equal only the last is kept, so survivors appear in
    // the order of their final occurrence.
    reset_output();
    run_string("show remdup [a b a c b]");
    TEST_ASSERT_EQUAL_STRING("[a c b]\n", output_buffer);
}

void test_remdup_list_no_duplicates(void)
{
    reset_output();
    run_string("show remdup [a b c]");
    TEST_ASSERT_EQUAL_STRING("[a b c]\n", output_buffer);
}

void test_remdup_word(void)
{
    // Keeping the last of each character: m@0, s@6, p@9, i@10 -> "mspi".
    Result r = eval_string("remdup \"mississippi");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("mspi", mem_word_ptr(r.value.as.node));
}

void test_remdup_empty(void)
{
    Result r = eval_string("remdup []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

// A PSRAM-backed region so blob words (longer than the 255-byte atom cap)
// can be created; reverse/shuffle must handle them without overflowing
// their word buffers.
static _Alignas(8) uint8_t words_blob_region[64 * 1024];

static void bind_long_blob_word(const char *varname, char *content, size_t len)
{
    logo_mem_set_aux_region(words_blob_region, sizeof(words_blob_region));
    for (size_t i = 0; i < len; i++)
    {
        content[i] = (char)('a' + (i % 26));
    }
    Node blob = mem_word(content, len);
    TEST_ASSERT_TRUE(mem_is_blob(blob));
    TEST_ASSERT_TRUE(var_set(mem_word_ptr(mem_atom_cstr(varname)),
                             value_word(blob)));
}

void test_reverse_long_blob_word(void)
{
    char content[300];
    bind_long_blob_word("b", content, sizeof(content));

    Result r = eval_string("reverse :b");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(sizeof(content), mem_word_len(r.value.as.node));
    const char *rev = mem_word_ptr(r.value.as.node);
    for (size_t i = 0; i < sizeof(content); i++)
    {
        TEST_ASSERT_EQUAL(content[sizeof(content) - 1 - i], rev[i]);
    }
}

void test_shuffle_long_blob_word(void)
{
    char content[300];
    bind_long_blob_word("b", content, sizeof(content));

    Result r = eval_string("shuffle :b");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL(sizeof(content), mem_word_len(r.value.as.node));

    // Permutation check: same character multiset as the input.
    int want[26] = {0};
    int got[26] = {0};
    const char *out = mem_word_ptr(r.value.as.node);
    for (size_t i = 0; i < sizeof(content); i++)
    {
        want[content[i] - 'a']++;
        got[out[i] - 'a']++;
    }
    for (int c = 0; c < 26; c++)
    {
        TEST_ASSERT_EQUAL(want[c], got[c]);
    }
}

void test_word_long_blob_on_psram(void)
{
    // With a PSRAM aux region, `word` may exceed the 255-char atom cap by
    // blobbing the result instead of erroring.
    logo_mem_set_aux_region(words_blob_region, sizeof(words_blob_region));

    char big[256];
    memset(big, 'a', 200);
    big[200] = '\0';

    char src[600];
    snprintf(src, sizeof(src), "(word \"%s \"%s)", big, big); // 400 chars

    Result r = eval_string(src);
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_TRUE(mem_is_blob(r.value.as.node));
    TEST_ASSERT_EQUAL_size_t(400, mem_word_len(r.value.as.node));

    const char *out = mem_word_ptr(r.value.as.node);
    for (size_t i = 0; i < 400; i++)
    {
        TEST_ASSERT_EQUAL('a', out[i]);
    }
}

//==========================================================================
// Out-of-Nodes Error Tests
//==========================================================================

// Exhaust the node pool so any further mem_cons fails. The garbage chain
// is reclaimed when the scaffold re-inits memory in tearDown.
static void exhaust_node_pool(void)
{
    Node chain = NODE_NIL;
    for (;;)
    {
        Node c = mem_cons(NODE_NIL, chain);
        if (mem_is_nil(c))
        {
            return;
        }
        chain = c;
    }
}

void test_fput_out_of_nodes_errors(void)
{
    run_string("make \"l [2 3]");
    exhaust_node_pool();

    Result r = eval_string("fput 1 :l");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
}

void test_butlast_out_of_nodes_errors(void)
{
    // The copy loop must report the failure, not return a truncated list.
    run_string("make \"l [1 2 3 4]");
    exhaust_node_pool();

    Result r = eval_string("butlast :l");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
}

void test_list_literal_out_of_nodes_errors(void)
{
    exhaust_node_pool();

    Result r = eval_string("print [a b c]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
}

void test_reverse_out_of_nodes_errors(void)
{
    run_string("make \"l [1 2 3 4]");
    exhaust_node_pool();

    Result r = eval_string("reverse :l");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
}

void test_remove_out_of_nodes_errors(void)
{
    run_string("make \"l [1 2 3 4]");
    exhaust_node_pool();

    Result r = eval_string("remove 9 :l");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
}

void test_remdup_out_of_nodes_errors(void)
{
    run_string("make \"l [1 2 3 4]");
    exhaust_node_pool();

    Result r = eval_string("remdup :l");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
}

// Exhaust the atom table so further interning fails. Atoms needed by the
// test itself must be interned before calling this.
static void exhaust_atom_table(void)
{
    char buf[16];
    for (int i = 0;; i++)
    {
        int len = snprintf(buf, sizeof(buf), "a%d", i);
        if (mem_is_nil(mem_atom(buf, (size_t)len)))
        {
            return;
        }
    }
}

void test_word_of_numbers_out_of_atoms_errors(void)
{
    // number_to_word interns the formatted number; on atom exhaustion it
    // returns NIL and word must error, not crash on a NULL string.
    exhaust_atom_table();

    Result r = eval_string("word 12345 678");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
}

void test_word_result_out_of_atoms_errors(void)
{
    // Inputs are interned ahead of time; only the concatenated result is
    // new, so its interning is the failure point.
    mem_atom_cstr("qq");
    mem_atom_cstr("rr");
    exhaust_atom_table();

    Result r = eval_string("word \"qq \"rr");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
}

void test_remove_numeric_thing_out_of_atoms_no_crash(void)
{
    // Regression: a numeric thing routed through value_as_word_str must not
    // dereference NULL when the atom table is exhausted (number_to_word
    // returns NIL). The word obj is interned before exhaustion so the copy
    // succeeds; under exhaustion the thing cannot be resolved, so nothing is
    // removed and the word is returned unchanged.
    run_string("make \"w \"12345");
    exhaust_atom_table();

    Result r = eval_string("remove 5 :w");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("12345", mem_word_ptr(r.value.as.node));
}

void test_count_of_number_out_of_atoms_errors(void)
{
    exhaust_atom_table();

    Result r = eval_string("count 98765");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
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

void test_equalp_words_case_insensitive(void)
{
    // Word comparison ignores case (classic Logo semantics)
    Result r = eval_string("equal? \"Hello \"hello");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));

    r = eval_string("equal? \"TRUE \"true");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_equals_operator_case_insensitive(void)
{
    Result r = eval_string("\"A = \"a");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_equalp_list_elements_case_insensitive(void)
{
    Result r = eval_string("equal? [Alpha Beta] [alpha beta]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_beforep_stays_case_sensitive(void)
{
    // before? compares ASCII codes per the reference: all uppercase letters
    // come before all lowercase letters. Case-insensitive equal? must not
    // leak into it.
    Result r = eval_string("before? \"ZEBRA \"apple");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
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

//---- member/memberp edge cases ----

void test_member_empty_needle_in_word_returns_empty(void)
{
    // Empty needle in word context yields empty word ("not found"
    // sentinel for the word-overload), not the whole haystack.
    Result r = eval_string("member \"|| \"abc");  // || is empty word literal
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("", mem_word_ptr(r.value.as.node));
}

void test_member_multichar_substring_in_word(void)
{
    // Multi-character needle should be found as substring.
    Result r = eval_string("member \"bc \"abcd");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("bcd", mem_word_ptr(r.value.as.node));
}

void test_member_word_is_case_insensitive(void)
{
    Result r = eval_string("member \"B \"abc");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    // Tail starts at position of 'b' in haystack
    TEST_ASSERT_EQUAL_STRING("bc", mem_word_ptr(r.value.as.node));
}

void test_member_list_in_word_returns_empty(void)
{
    // List argument cannot be a member of a word; expect empty word.
    Result r = eval_string("member [a] \"abc");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("", mem_word_ptr(r.value.as.node));
}

void test_member_number_in_list(void)
{
    Result r = eval_string("member 2 [1 2 3]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    Node first = mem_car(r.value.as.node);
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(first));
}

void test_memberp_empty_needle_word(void)
{
    // The predicate form should be `false` when member returns the
    // empty-word sentinel.
    Result r = eval_string("member? \"|| \"abc");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_member_whitespace_needle_in_word(void)
{
    // Whitespace inside a word should match verbatim as a substring.
    // Build needle "  " (two spaces) and haystack "a  b" using `char`.
    Result r = eval_string(
        "member (word char 32 char 32) (word \"a char 32 char 32 \"b)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("  b", mem_word_ptr(r.value.as.node));
}

void test_memberp_whitespace_needle_in_word_agrees_with_member(void)
{
    // member returning non-empty implies memberp is true.
    Result r = eval_string(
        "member? (word char 32 char 32) (word \"a char 32 char 32 \"b)");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

//============================================================================
// Vertical-bar quoting (end-to-end)
//
// `|...|` lets otherwise-delimiting characters appear in a word. The bars are
// stripped when the word is interned, so a quoted word, a list literal, and
// `parse` all agree.
//============================================================================

void test_bar_quoted_word_strips_bars(void)
{
    // "|San Francisco| is the single 13-character word "San Francisco".
    Result r = eval_string("count \"|San Francisco|");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(13.0f, r.value.as.number);
}

void test_bar_list_literal_is_one_word(void)
{
    // [|a b| c] is a two-element list whose first element is the word "a b".
    Result r = eval_string("first [|a b| c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("a b", mem_word_ptr(r.value.as.node));
}

void test_bar_list_literal_count(void)
{
    Result r = eval_string("count [|a b| c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, r.value.as.number);
}

void test_bar_protects_bracket_in_list_literal(void)
{
    // A `]` inside bars is an ordinary character, so `[|a]b| c]` is a
    // two-element list whose first element is the three-char word "a]b".
    Result r = eval_string("first [|a]b| c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("a]b", mem_word_ptr(r.value.as.node));
}

void test_bar_parse_of_word_containing_bars(void)
{
    // Per the reference: when `parse` tokenizes a word that CONTAINS vertical
    // bars, the bars do not appear in the result but every character between
    // them (here a space) is tokenized as though it were a letter. The word
    // "\|a\ b\| is the 5-character word |a b| (literal bars and space); parsing
    // it yields the one-element list [a b].
    Result r = eval_string("count parse \"\\|a\\ b\\|");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, r.value.as.number);
}

void test_bar_parse_first_is_whole_word(void)
{
    Result r = eval_string("first parse \"\\|a\\ b\\|");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("a b", mem_word_ptr(r.value.as.node));
}

void test_bar_variable_name_with_space(void)
{
    // make/thing round-trips a variable whose name contains a space.
    Result r = run_string("make \"|a b| 42\nthing \"|a b|");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, r.value.as.number);
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
    RUN_TEST(test_item_is_one_indexed);
    RUN_TEST(test_item_zero_index_errors);
    
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
    RUN_TEST(test_dsetfirst_mutates_in_place);
    RUN_TEST(test_dsetfirst_number_value);
    RUN_TEST(test_dsetfirst_affects_shared_tail);
    RUN_TEST(test_dsetfirst_empty_list_error);
    RUN_TEST(test_dsetfirst_non_list_error);
    RUN_TEST(test_dsetbf_replaces_tail);
    RUN_TEST(test_dsetbf_truncates_with_empty_tail);
    RUN_TEST(test_dsetbf_requires_list_value);
    RUN_TEST(test_dsetbf_empty_list_error);
    RUN_TEST(test_dsetitem_mutates_nth);
    RUN_TEST(test_dsetitem_number_value);
    RUN_TEST(test_dsetitem_index_out_of_bounds);
    RUN_TEST(test_dsetitem_index_zero_error);
    
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
    RUN_TEST(test_word_overflow_returns_error);
    RUN_TEST(test_word_at_atom_limit_succeeds);
    RUN_TEST(test_parse);
    RUN_TEST(test_parse_list_from_string_nested);
    RUN_TEST(test_parse_list_from_string_deeply_nested);
    RUN_TEST(test_parse_list_from_string_empty_sublist);
    
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
    RUN_TEST(test_equalp_words_case_insensitive);
    RUN_TEST(test_equals_operator_case_insensitive);
    RUN_TEST(test_equalp_list_elements_case_insensitive);
    RUN_TEST(test_beforep_stays_case_sensitive);
    RUN_TEST(test_word_of_numbers_out_of_atoms_errors);
    RUN_TEST(test_word_result_out_of_atoms_errors);
    RUN_TEST(test_remove_numeric_thing_out_of_atoms_no_crash);
    RUN_TEST(test_count_of_number_out_of_atoms_errors);
    RUN_TEST(test_pick_list);
    RUN_TEST(test_pick_list_element_can_be_list);
    RUN_TEST(test_pick_word);
    RUN_TEST(test_pick_number);
    RUN_TEST(test_pick_empty_errors);
    RUN_TEST(test_reverse_list);
    RUN_TEST(test_reverse_list_keeps_sublists_intact);
    RUN_TEST(test_reverse_word);
    RUN_TEST(test_reverse_number);
    RUN_TEST(test_reverse_empty);
    RUN_TEST(test_reverse_out_of_nodes_errors);
    RUN_TEST(test_shuffle_list_is_permutation);
    RUN_TEST(test_shuffle_word_is_permutation);
    RUN_TEST(test_shuffle_empty);
    RUN_TEST(test_reverse_long_blob_word);
    RUN_TEST(test_shuffle_long_blob_word);
    RUN_TEST(test_word_long_blob_on_psram);
    RUN_TEST(test_remove_list);
    RUN_TEST(test_remove_list_no_match_copies);
    RUN_TEST(test_remove_list_numbers);
    RUN_TEST(test_remove_list_sublist_element);
    RUN_TEST(test_remove_word);
    RUN_TEST(test_remove_word_is_case_insensitive);
    RUN_TEST(test_remove_word_multichar_thing_never_matches);
    RUN_TEST(test_remove_empty);
    RUN_TEST(test_remove_out_of_nodes_errors);
    RUN_TEST(test_remdup_list_keeps_last);
    RUN_TEST(test_remdup_list_no_duplicates);
    RUN_TEST(test_remdup_word);
    RUN_TEST(test_remdup_empty);
    RUN_TEST(test_remdup_out_of_nodes_errors);
    RUN_TEST(test_fput_out_of_nodes_errors);
    RUN_TEST(test_butlast_out_of_nodes_errors);
    RUN_TEST(test_list_literal_out_of_nodes_errors);
    
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
    RUN_TEST(test_member_empty_needle_in_word_returns_empty);
    RUN_TEST(test_member_multichar_substring_in_word);
    RUN_TEST(test_member_word_is_case_insensitive);
    RUN_TEST(test_member_list_in_word_returns_empty);
    RUN_TEST(test_member_number_in_list);
    RUN_TEST(test_memberp_empty_needle_word);

    RUN_TEST(test_member_whitespace_needle_in_word);
    RUN_TEST(test_memberp_whitespace_needle_in_word_agrees_with_member);

    // Vertical-bar quoting (end-to-end)
    RUN_TEST(test_bar_quoted_word_strips_bars);
    RUN_TEST(test_bar_list_literal_is_one_word);
    RUN_TEST(test_bar_list_literal_count);
    RUN_TEST(test_bar_protects_bracket_in_list_literal);
    RUN_TEST(test_bar_parse_of_word_containing_bars);
    RUN_TEST(test_bar_parse_first_is_whole_word);
    RUN_TEST(test_bar_variable_name_with_space);

    return UNITY_END();
}
