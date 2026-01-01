//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for Outside World primitives: keyp, readchar, readchars, readlist,
//  readword, print, show, type, standout
//

#include "test_scaffold.h"
#include <stdlib.h>
#include <string.h>

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Output Tests: print, show, type
//==========================================================================

void test_print_number(void)
{
    run_string("print 42");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
}

void test_print_word(void)
{
    run_string("print \"hello");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_print_list_no_outer_brackets(void)
{
    run_string("print [a b c]");
    TEST_ASSERT_EQUAL_STRING("a b c\n", output_buffer);
}

void test_print_nested_list(void)
{
    run_string("print [a [b c] d]");
    TEST_ASSERT_EQUAL_STRING("a [b c] d\n", output_buffer);
}

void test_print_multiple_args(void)
{
    run_string("(print 1 2 3)");
    TEST_ASSERT_EQUAL_STRING("1 2 3\n", output_buffer);
}

void test_pr_abbreviation(void)
{
    run_string("pr \"test");
    TEST_ASSERT_EQUAL_STRING("test\n", output_buffer);
}

void test_show_number(void)
{
    run_string("show 42");
    TEST_ASSERT_EQUAL_STRING("42\n", output_buffer);
}

void test_show_word(void)
{
    run_string("show \"hello");
    TEST_ASSERT_EQUAL_STRING("hello\n", output_buffer);
}

void test_show_list_with_brackets(void)
{
    run_string("show [a b c]");
    TEST_ASSERT_EQUAL_STRING("[a b c]\n", output_buffer);
}

void test_show_nested_list(void)
{
    run_string("show [a [b c] d]");
    TEST_ASSERT_EQUAL_STRING("[a [b c] d]\n", output_buffer);
}

void test_show_empty_list(void)
{
    run_string("show []");
    TEST_ASSERT_EQUAL_STRING("[]\n", output_buffer);
}

void test_print_empty_list(void)
{
    run_string("print []");
    TEST_ASSERT_EQUAL_STRING("\n", output_buffer);
}

void test_type_empty_list(void)
{
    run_string("type []");
    TEST_ASSERT_EQUAL_STRING("", output_buffer);
}

void test_show_list_with_empty_list(void)
{
    run_string("show [a [] b]");
    TEST_ASSERT_EQUAL_STRING("[a [] b]\n", output_buffer);
}

void test_type_number_no_newline(void)
{
    run_string("type 42");
    TEST_ASSERT_EQUAL_STRING("42", output_buffer);
}

void test_type_word_no_newline(void)
{
    run_string("type \"hello");
    TEST_ASSERT_EQUAL_STRING("hello", output_buffer);
}

void test_type_list_no_outer_brackets(void)
{
    run_string("type [a b c]");
    TEST_ASSERT_EQUAL_STRING("a b c", output_buffer);
}

void test_type_multiple_args(void)
{
    run_string("(type 1 2 3)");
    TEST_ASSERT_EQUAL_STRING("1 2 3", output_buffer);
}

void test_type_then_print(void)
{
    run_string("type \"Hello");
    reset_output();
    run_string("print \"World");
    TEST_ASSERT_EQUAL_STRING("World\n", output_buffer);
}

//==========================================================================
// Standout Tests
//==========================================================================

void test_standout_word(void)
{
    Result r = eval_string("standout \"ABC");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    
    // Each character should have MSB set
    const char *str = mem_word_ptr(r.value.as.node);
    TEST_ASSERT_EQUAL(3, strlen(str));
    TEST_ASSERT_EQUAL((char)('A' | 0x80), str[0]);
    TEST_ASSERT_EQUAL((char)('B' | 0x80), str[1]);
    TEST_ASSERT_EQUAL((char)('C' | 0x80), str[2]);
}

void test_standout_number(void)
{
    Result r = eval_string("standout 42");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    
    // "42" with MSB set on each character
    const char *str = mem_word_ptr(r.value.as.node);
    TEST_ASSERT_EQUAL(2, strlen(str));
    TEST_ASSERT_EQUAL((char)('4' | 0x80), str[0]);
    TEST_ASSERT_EQUAL((char)('2' | 0x80), str[1]);
}

void test_standout_list_no_outer_brackets(void)
{
    Result r = eval_string("standout [a b c]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    
    // "a b c" with MSB set, using inverse space (0xA0) between items
    const char *str = mem_word_ptr(r.value.as.node);
    TEST_ASSERT_EQUAL(5, strlen(str));
    TEST_ASSERT_EQUAL((char)('a' | 0x80), str[0]);
    TEST_ASSERT_EQUAL((char)(' ' | 0x80), str[1]);  // Inverse space
    TEST_ASSERT_EQUAL((char)('b' | 0x80), str[2]);
    TEST_ASSERT_EQUAL((char)(' ' | 0x80), str[3]);  // Inverse space
    TEST_ASSERT_EQUAL((char)('c' | 0x80), str[4]);
}

void test_standout_nested_list(void)
{
    Result r = eval_string("standout [a [b c] d]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    
    // "a [b c] d" with MSB set
    const char *str = mem_word_ptr(r.value.as.node);
    // Expected: a<sp>[b<sp>c]<sp>d = 9 chars
    TEST_ASSERT_EQUAL(9, strlen(str));
    TEST_ASSERT_EQUAL((char)('a' | 0x80), str[0]);
    TEST_ASSERT_EQUAL((char)(' ' | 0x80), str[1]);
    TEST_ASSERT_EQUAL((char)('[' | 0x80), str[2]);
    TEST_ASSERT_EQUAL((char)('b' | 0x80), str[3]);
    TEST_ASSERT_EQUAL((char)(' ' | 0x80), str[4]);
    TEST_ASSERT_EQUAL((char)('c' | 0x80), str[5]);
    TEST_ASSERT_EQUAL((char)(']' | 0x80), str[6]);
    TEST_ASSERT_EQUAL((char)(' ' | 0x80), str[7]);
    TEST_ASSERT_EQUAL((char)('d' | 0x80), str[8]);
}

void test_standout_empty_list(void)
{
    Result r = eval_string("standout []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    
    // Empty list should produce empty word
    const char *str = mem_word_ptr(r.value.as.node);
    TEST_ASSERT_EQUAL(0, strlen(str));
}

void test_standout_empty_word(void)
{
    Result r = eval_string("standout \"");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    
    // Empty word
    const char *str = mem_word_ptr(r.value.as.node);
    TEST_ASSERT_EQUAL(0, strlen(str));
}

void test_standout_can_be_printed(void)
{
    // standout returns a word that can be printed
    run_string("type standout \"Hi");
    // Output should contain characters with MSB set
    TEST_ASSERT_EQUAL(2, strlen(output_buffer));
    TEST_ASSERT_EQUAL((char)('H' | 0x80), output_buffer[0]);
    TEST_ASSERT_EQUAL((char)('i' | 0x80), output_buffer[1]);
}

//==========================================================================
// Input Tests: keyp, readchar, readchars, readlist, readword
//==========================================================================

void test_keyp_no_input_returns_false(void)
{
    // No input set - should return false
    Result r = eval_string("keyp");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_keyp_with_input_returns_true(void)
{
    set_mock_input("x");
    Result r = eval_string("keyp");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_readchar_returns_single_character(void)
{
    set_mock_input("abc");
    Result r = eval_string("readchar");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(r.value.as.node));
}

void test_readchar_multiple_calls(void)
{
    set_mock_input("abc");
    
    Result r1 = eval_string("readchar");
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(r1.value.as.node));
    
    Result r2 = eval_string("readchar");
    TEST_ASSERT_EQUAL_STRING("b", mem_word_ptr(r2.value.as.node));
    
    Result r3 = eval_string("readchar");
    TEST_ASSERT_EQUAL_STRING("c", mem_word_ptr(r3.value.as.node));
}

void test_rc_abbreviation(void)
{
    set_mock_input("x");
    Result r = eval_string("rc");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("x", mem_word_ptr(r.value.as.node));
}

void test_readchar_eof_returns_empty_list(void)
{
    // No input set - EOF
    Result r = eval_string("readchar");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_readchars_returns_multiple_characters(void)
{
    set_mock_input("hello world");
    Result r = eval_string("readchars 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(r.value.as.node));
}

void test_rcs_abbreviation(void)
{
    set_mock_input("test");
    Result r = eval_string("rcs 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("test", mem_word_ptr(r.value.as.node));
}

void test_readchars_partial_read(void)
{
    set_mock_input("hi");
    Result r = eval_string("readchars 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("hi", mem_word_ptr(r.value.as.node));
}

void test_readchars_eof_returns_empty_list(void)
{
    // No input set - EOF
    Result r = eval_string("readchars 5");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_readchars_invalid_count(void)
{
    set_mock_input("test");
    Result r = eval_string("readchars 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_readchars_negative_count(void)
{
    set_mock_input("test");
    Result r = eval_string("readchars -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_readword_returns_line_as_word(void)
{
    set_mock_input("hello world\n");
    Result r = eval_string("readword");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("hello world", mem_word_ptr(r.value.as.node));
}

void test_rw_abbreviation(void)
{
    set_mock_input("test line\n");
    Result r = eval_string("rw");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("test line", mem_word_ptr(r.value.as.node));
}

void test_readword_empty_line_returns_empty_word(void)
{
    set_mock_input("\n");
    Result r = eval_string("readword");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("", mem_word_ptr(r.value.as.node));
}

void test_readword_eof_returns_empty_list(void)
{
    // No input set - EOF
    Result r = eval_string("readword");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_readlist_parses_words(void)
{
    set_mock_input("hello world\n");
    Result r = eval_string("readlist");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Should be [hello world]
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));
    
    Node first = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_word(first));
    TEST_ASSERT_EQUAL_STRING("hello", mem_word_ptr(first));
    
    Node rest = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(rest));
    
    Node second = mem_car(rest);
    TEST_ASSERT_TRUE(mem_is_word(second));
    TEST_ASSERT_EQUAL_STRING("world", mem_word_ptr(second));
    
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(rest)));
}

void test_rl_abbreviation(void)
{
    set_mock_input("a b c\n");
    Result r = eval_string("rl");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
}

void test_readlist_empty_line_returns_empty_list(void)
{
    set_mock_input("\n");
    Result r = eval_string("readlist");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_readlist_eof_returns_empty_word(void)
{
    // No input set - EOF
    Result r = eval_string("readlist");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_word(r.value));
    TEST_ASSERT_EQUAL_STRING("", mem_word_ptr(r.value.as.node));
}

void test_readlist_with_numbers(void)
{
    set_mock_input("1 2 3\n");
    Result r = eval_string("readlist");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(value_is_list(r.value));
    
    // Should be [1 2 3] - numbers as words
    Node list = r.value.as.node;
    Node first = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_word(first));
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(first));
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Output tests
    RUN_TEST(test_print_number);
    RUN_TEST(test_print_word);
    RUN_TEST(test_print_list_no_outer_brackets);
    RUN_TEST(test_print_nested_list);
    RUN_TEST(test_print_multiple_args);
    RUN_TEST(test_pr_abbreviation);
    
    RUN_TEST(test_show_number);
    RUN_TEST(test_show_word);
    RUN_TEST(test_show_list_with_brackets);
    RUN_TEST(test_show_nested_list);
    RUN_TEST(test_show_empty_list);
    RUN_TEST(test_show_list_with_empty_list);
    
    RUN_TEST(test_print_empty_list);
    
    RUN_TEST(test_type_number_no_newline);
    RUN_TEST(test_type_word_no_newline);
    RUN_TEST(test_type_list_no_outer_brackets);
    RUN_TEST(test_type_multiple_args);
    RUN_TEST(test_type_then_print);
    RUN_TEST(test_type_empty_list);
    
    // Standout tests
    RUN_TEST(test_standout_word);
    RUN_TEST(test_standout_number);
    RUN_TEST(test_standout_list_no_outer_brackets);
    RUN_TEST(test_standout_nested_list);
    RUN_TEST(test_standout_empty_list);
    RUN_TEST(test_standout_empty_word);
    RUN_TEST(test_standout_can_be_printed);
    
    // Input tests
    RUN_TEST(test_keyp_no_input_returns_false);
    RUN_TEST(test_keyp_with_input_returns_true);
    
    RUN_TEST(test_readchar_returns_single_character);
    RUN_TEST(test_readchar_multiple_calls);
    RUN_TEST(test_rc_abbreviation);
    RUN_TEST(test_readchar_eof_returns_empty_list);
    
    RUN_TEST(test_readchars_returns_multiple_characters);
    RUN_TEST(test_rcs_abbreviation);
    RUN_TEST(test_readchars_partial_read);
    RUN_TEST(test_readchars_eof_returns_empty_list);
    RUN_TEST(test_readchars_invalid_count);
    RUN_TEST(test_readchars_negative_count);
    
    RUN_TEST(test_readword_returns_line_as_word);
    RUN_TEST(test_rw_abbreviation);
    RUN_TEST(test_readword_empty_line_returns_empty_word);
    RUN_TEST(test_readword_eof_returns_empty_list);
    
    RUN_TEST(test_readlist_parses_words);
    RUN_TEST(test_rl_abbreviation);
    RUN_TEST(test_readlist_empty_line_returns_empty_list);
    RUN_TEST(test_readlist_eof_returns_empty_word);
    RUN_TEST(test_readlist_with_numbers);
    
    return UNITY_END();
}
