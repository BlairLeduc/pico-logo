//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the lexer.
//

#include "unity.h"
#include "core/lexer.h"

#include <stdlib.h>
#include <string.h>

void setUp(void)
{
    // Nothing to set up
}

void tearDown(void)
{
    // Nothing to tear down
}

// Helper to assert token type and text
static void assert_token(Lexer *lexer, TokenType expected_type, const char *expected_text)
{
    Token token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected_type, token.type,
                                  "Token type mismatch");
    if (expected_text)
    {
        char text[256];
        lexer_token_text(&token, text, sizeof(text));
        TEST_ASSERT_EQUAL_STRING_MESSAGE(expected_text, text,
                                         "Token text mismatch");
    }
}

// Helper to assert just token type
static void assert_token_type(Lexer *lexer, TokenType expected_type)
{
    assert_token(lexer, expected_type, NULL);
}

//============================================================================
// Basic Token Tests
//============================================================================

void test_empty_input(void)
{
    Lexer lexer;
    lexer_init(&lexer, "");
    assert_token(&lexer, TOKEN_EOF, "");
}

void test_whitespace_only(void)
{
    Lexer lexer;
    lexer_init(&lexer, "   \t\n  ");
    assert_token(&lexer, TOKEN_EOF, "");
}

void test_single_word(void)
{
    Lexer lexer;
    lexer_init(&lexer, "forward");
    assert_token(&lexer, TOKEN_WORD, "forward");
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_multiple_words(void)
{
    Lexer lexer;
    lexer_init(&lexer, "forward right repeat");
    assert_token(&lexer, TOKEN_WORD, "forward");
    assert_token(&lexer, TOKEN_WORD, "right");
    assert_token(&lexer, TOKEN_WORD, "repeat");
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_mixed_case_word(void)
{
    Lexer lexer;
    lexer_init(&lexer, "ForWard HELLO hello");
    assert_token(&lexer, TOKEN_WORD, "ForWard");
    assert_token(&lexer, TOKEN_WORD, "HELLO");
    assert_token(&lexer, TOKEN_WORD, "hello");
}

//============================================================================
// Number Tests
//============================================================================

void test_integer_number(void)
{
    Lexer lexer;
    lexer_init(&lexer, "100");
    assert_token(&lexer, TOKEN_NUMBER, "100");
}

void test_decimal_number(void)
{
    Lexer lexer;
    lexer_init(&lexer, "3.14");
    assert_token(&lexer, TOKEN_NUMBER, "3.14");
}

void test_negative_number(void)
{
    Lexer lexer;
    lexer_init(&lexer, "-42");
    assert_token(&lexer, TOKEN_NUMBER, "-42");
}

void test_negative_decimal(void)
{
    Lexer lexer;
    lexer_init(&lexer, "-3.14159");
    assert_token(&lexer, TOKEN_NUMBER, "-3.14159");
}

void test_scientific_notation_e(void)
{
    Lexer lexer;
    // 1e4 = 10000
    lexer_init(&lexer, "1e4");
    assert_token(&lexer, TOKEN_NUMBER, "1e4");
}

void test_scientific_notation_E(void)
{
    Lexer lexer;
    lexer_init(&lexer, "2.5E10");
    assert_token(&lexer, TOKEN_NUMBER, "2.5E10");
}

void test_scientific_notation_n(void)
{
    Lexer lexer;
    // 1n4 = 0.0001 (per reference)
    lexer_init(&lexer, "1n4");
    assert_token(&lexer, TOKEN_NUMBER, "1n4");
}

void test_scientific_notation_N(void)
{
    Lexer lexer;
    lexer_init(&lexer, "5N3");
    assert_token(&lexer, TOKEN_NUMBER, "5N3");
}

void test_n_notation_no_sign(void)
{
    // "1n" without exponent digits should be a word, not a number
    Lexer lexer;
    lexer_init(&lexer, "1n");
    assert_token(&lexer, TOKEN_WORD, "1n");
}

void test_e_notation_with_sign(void)
{
    // e/E notation still allows signs: "1e-5" is a valid number
    Lexer lexer;
    lexer_init(&lexer, "1e-5");
    assert_token(&lexer, TOKEN_NUMBER, "1e-5");
}

void test_numbers_self_quoting(void)
{
    // Numbers are self-quoting, so "100" without quotes parses as NUMBER
    Lexer lexer;
    lexer_init(&lexer, "print 100");
    assert_token(&lexer, TOKEN_WORD, "print");
    assert_token(&lexer, TOKEN_NUMBER, "100");
}

//============================================================================
// Quoted Word Tests
//============================================================================

void test_quoted_word(void)
{
    Lexer lexer;
    lexer_init(&lexer, "\"hello");
    assert_token(&lexer, TOKEN_QUOTED, "\"hello");
}

void test_quoted_number(void)
{
    // In formal Logo, numbers are quoted. This tests that quoted numbers work.
    Lexer lexer;
    lexer_init(&lexer, "\"100");
    assert_token(&lexer, TOKEN_QUOTED, "\"100");
}

void test_quoted_word_with_special_first_char(void)
{
    // First char after quote doesn't need backslash (except brackets)
    Lexer lexer;
    lexer_init(&lexer, "\"*");
    assert_token(&lexer, TOKEN_QUOTED, "\"*");
}

void test_quoted_word_special_in_middle(void)
{
    // Special chars after first position need backslash
    Lexer lexer;
    lexer_init(&lexer, "\"a\\*b");
    assert_token(&lexer, TOKEN_QUOTED, "\"a\\*b");
}

void test_quoted_bracket_needs_backslash(void)
{
    // Brackets always need backslash
    Lexer lexer;
    lexer_init(&lexer, "\"\\[");
    assert_token(&lexer, TOKEN_QUOTED, "\"\\[");
}

void test_quoted_space_with_backslash(void)
{
    // Creating "San Francisco" requires backslash before space
    Lexer lexer;
    lexer_init(&lexer, "\"San\\ Francisco");
    assert_token(&lexer, TOKEN_QUOTED, "\"San\\ Francisco");
}

void test_quoted_empty_word(void)
{
    // Empty word: quote followed by space
    Lexer lexer;
    lexer_init(&lexer, "\" next");
    assert_token(&lexer, TOKEN_QUOTED, "\"");
    assert_token(&lexer, TOKEN_WORD, "next");
}

void test_print_heading_example(void)
{
    // From reference: print "heading outputs "heading"
    Lexer lexer;
    lexer_init(&lexer, "print \"heading");
    assert_token(&lexer, TOKEN_WORD, "print");
    assert_token(&lexer, TOKEN_QUOTED, "\"heading");
}

//============================================================================
// Variable Reference Tests (Colon/Dots)
//============================================================================

void test_variable_reference(void)
{
    Lexer lexer;
    lexer_init(&lexer, ":x");
    assert_token(&lexer, TOKEN_COLON, ":x");
}

void test_variable_in_expression(void)
{
    Lexer lexer;
    lexer_init(&lexer, "forward :distance");
    assert_token(&lexer, TOKEN_WORD, "forward");
    assert_token(&lexer, TOKEN_COLON, ":distance");
}

void test_multiple_variables(void)
{
    Lexer lexer;
    lexer_init(&lexer, ":step :angle");
    assert_token(&lexer, TOKEN_COLON, ":step");
    assert_token(&lexer, TOKEN_COLON, ":angle");
}

void test_variable_in_poly_procedure(void)
{
    // From the poly example in reference
    Lexer lexer;
    lexer_init(&lexer, "forward :step");
    assert_token(&lexer, TOKEN_WORD, "forward");
    assert_token(&lexer, TOKEN_COLON, ":step");
}

//============================================================================
// Bracket Tests
//============================================================================

void test_simple_list(void)
{
    Lexer lexer;
    lexer_init(&lexer, "[a b c]");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_WORD, "a");
    assert_token(&lexer, TOKEN_WORD, "b");
    assert_token(&lexer, TOKEN_WORD, "c");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
}

void test_nested_lists(void)
{
    Lexer lexer;
    lexer_init(&lexer, "[[a b] [c d]]");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_WORD, "a");
    assert_token(&lexer, TOKEN_WORD, "b");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_WORD, "c");
    assert_token(&lexer, TOKEN_WORD, "d");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
}

void test_empty_list(void)
{
    Lexer lexer;
    lexer_init(&lexer, "[]");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
}

void test_list_with_numbers(void)
{
    Lexer lexer;
    lexer_init(&lexer, "[1 2 3]");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_NUMBER, "1");
    assert_token(&lexer, TOKEN_NUMBER, "2");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
}

void test_list_with_negative_numbers(void)
{
    Lexer lexer;
    lexer_init(&lexer, "[-1 -2 -3]");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_NUMBER, "-1");
    assert_token(&lexer, TOKEN_NUMBER, "-2");
    assert_token(&lexer, TOKEN_NUMBER, "-3");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
}

void test_repeat_with_list(void)
{
    // repeat 4 [fd 100 rt 90]
    Lexer lexer;
    lexer_init(&lexer, "repeat 4 [fd 100 rt 90]");
    assert_token(&lexer, TOKEN_WORD, "repeat");
    assert_token(&lexer, TOKEN_NUMBER, "4");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_WORD, "fd");
    assert_token(&lexer, TOKEN_NUMBER, "100");
    assert_token(&lexer, TOKEN_WORD, "rt");
    assert_token(&lexer, TOKEN_NUMBER, "90");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
}

//============================================================================
// Parentheses Tests
//============================================================================

void test_parentheses_grouping(void)
{
    Lexer lexer;
    lexer_init(&lexer, "(3 + 4)");
    assert_token(&lexer, TOKEN_LEFT_PAREN, "(");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_PLUS, "+");
    assert_token(&lexer, TOKEN_NUMBER, "4");
    assert_token(&lexer, TOKEN_RIGHT_PAREN, ")");
}

void test_variable_inputs(void)
{
    // (sum 3 4 5 6 7 8)
    Lexer lexer;
    lexer_init(&lexer, "(sum 3 4 5 6 7 8)");
    assert_token(&lexer, TOKEN_LEFT_PAREN, "(");
    assert_token(&lexer, TOKEN_WORD, "sum");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_NUMBER, "4");
    assert_token(&lexer, TOKEN_NUMBER, "5");
    assert_token(&lexer, TOKEN_NUMBER, "6");
    assert_token(&lexer, TOKEN_NUMBER, "7");
    assert_token(&lexer, TOKEN_NUMBER, "8");
    assert_token(&lexer, TOKEN_RIGHT_PAREN, ")");
}

//============================================================================
// Infix Operator Tests
//============================================================================

void test_plus_operator(void)
{
    Lexer lexer;
    lexer_init(&lexer, "3 + 4");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_PLUS, "+");
    assert_token(&lexer, TOKEN_NUMBER, "4");
}

void test_minus_operator_binary(void)
{
    Lexer lexer;
    lexer_init(&lexer, "7 - 3");
    assert_token(&lexer, TOKEN_NUMBER, "7");
    assert_token(&lexer, TOKEN_MINUS, "-");
    assert_token(&lexer, TOKEN_NUMBER, "3");
}

void test_multiply_operator(void)
{
    Lexer lexer;
    lexer_init(&lexer, "3 * 4");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_MULTIPLY, "*");
    assert_token(&lexer, TOKEN_NUMBER, "4");
}

void test_divide_operator(void)
{
    Lexer lexer;
    lexer_init(&lexer, "20 / 5");
    assert_token(&lexer, TOKEN_NUMBER, "20");
    assert_token(&lexer, TOKEN_DIVIDE, "/");
    assert_token(&lexer, TOKEN_NUMBER, "5");
}

void test_equals_operator(void)
{
    Lexer lexer;
    lexer_init(&lexer, ":x = 5");
    assert_token(&lexer, TOKEN_COLON, ":x");
    assert_token(&lexer, TOKEN_EQUALS, "=");
    assert_token(&lexer, TOKEN_NUMBER, "5");
}

void test_less_than_operator(void)
{
    Lexer lexer;
    lexer_init(&lexer, "1 < 2");
    assert_token(&lexer, TOKEN_NUMBER, "1");
    assert_token(&lexer, TOKEN_LESS_THAN, "<");
    assert_token(&lexer, TOKEN_NUMBER, "2");
}

void test_greater_than_operator(void)
{
    Lexer lexer;
    lexer_init(&lexer, "5 > 3");
    assert_token(&lexer, TOKEN_NUMBER, "5");
    assert_token(&lexer, TOKEN_GREATER_THAN, ">");
    assert_token(&lexer, TOKEN_NUMBER, "3");
}

void test_operators_no_spaces(void)
{
    // Delimiters don't need spaces around them
    Lexer lexer;
    lexer_init(&lexer, "3+4*5");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_PLUS, "+");
    assert_token(&lexer, TOKEN_NUMBER, "4");
    assert_token(&lexer, TOKEN_MULTIPLY, "*");
    assert_token(&lexer, TOKEN_NUMBER, "5");
}

void test_complex_expression(void)
{
    // (25 + 20) / 5
    Lexer lexer;
    lexer_init(&lexer, "(25 + 20) / 5");
    assert_token(&lexer, TOKEN_LEFT_PAREN, "(");
    assert_token(&lexer, TOKEN_NUMBER, "25");
    assert_token(&lexer, TOKEN_PLUS, "+");
    assert_token(&lexer, TOKEN_NUMBER, "20");
    assert_token(&lexer, TOKEN_RIGHT_PAREN, ")");
    assert_token(&lexer, TOKEN_DIVIDE, "/");
    assert_token(&lexer, TOKEN_NUMBER, "5");
}

//============================================================================
// Minus Sign Context Tests (from reference)
//============================================================================

void test_minus_in_expression(void)
{
    // print sum 20-20 (parses as 20 minus 20)
    Lexer lexer;
    lexer_init(&lexer, "sum 20-20");
    assert_token(&lexer, TOKEN_WORD, "sum");
    assert_token(&lexer, TOKEN_NUMBER, "20");
    assert_token(&lexer, TOKEN_MINUS, "-");
    assert_token(&lexer, TOKEN_NUMBER, "20");
}

void test_minus_negative_number(void)
{
    // print 3*-4 (parses as 3 times negative 4)
    Lexer lexer;
    lexer_init(&lexer, "3*-4");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_MULTIPLY, "*");
    assert_token(&lexer, TOKEN_NUMBER, "-4");
}

void test_minus_after_paren(void)
{
    // print (3+4)-5 (parses as 3 plus 4 minus 5)
    Lexer lexer;
    lexer_init(&lexer, "(3+4)-5");
    assert_token(&lexer, TOKEN_LEFT_PAREN, "(");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_PLUS, "+");
    assert_token(&lexer, TOKEN_NUMBER, "4");
    assert_token(&lexer, TOKEN_RIGHT_PAREN, ")");
    assert_token(&lexer, TOKEN_MINUS, "-");
    assert_token(&lexer, TOKEN_NUMBER, "5");
}

void test_minus_after_paren_space_before_no_space_after(void)
{
    // (pr (5+3) -2) should tokenize -2 as negative number, not binary minus
    // Space before '-' and no space after means unary/negative
    Lexer lexer;
    lexer_init(&lexer, "(5+3) -2");
    assert_token(&lexer, TOKEN_LEFT_PAREN, "(");
    assert_token(&lexer, TOKEN_NUMBER, "5");
    assert_token(&lexer, TOKEN_PLUS, "+");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_RIGHT_PAREN, ")");
    assert_token(&lexer, TOKEN_NUMBER, "-2");
}

void test_minus_in_list(void)
{
    // first [-3 4] (outputs -3)
    Lexer lexer;
    lexer_init(&lexer, "[-3 4]");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_NUMBER, "-3");
    assert_token(&lexer, TOKEN_NUMBER, "4");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
}

void test_unary_minus_variable(void)
{
    // setpos list :x -:y
    Lexer lexer;
    lexer_init(&lexer, "setpos list :x -:y");
    assert_token(&lexer, TOKEN_WORD, "setpos");
    assert_token(&lexer, TOKEN_WORD, "list");
    assert_token(&lexer, TOKEN_COLON, ":x");
    assert_token(&lexer, TOKEN_UNARY_MINUS, "-");
    assert_token(&lexer, TOKEN_COLON, ":y");
}

void test_unary_minus_word(void)
{
    // setpos list ycor -xcor
    Lexer lexer;
    lexer_init(&lexer, "setpos list ycor -xcor");
    assert_token(&lexer, TOKEN_WORD, "setpos");
    assert_token(&lexer, TOKEN_WORD, "list");
    assert_token(&lexer, TOKEN_WORD, "ycor");
    assert_token(&lexer, TOKEN_UNARY_MINUS, "-");
    assert_token(&lexer, TOKEN_WORD, "xcor");
}

void test_binary_minus_spacing(void)
{
    // print 3-4 (parses as 3 minus 4)
    // print 3 - 4 (parses exactly like the previous)
    Lexer lexer;
    
    lexer_init(&lexer, "3-4");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_MINUS, "-");
    assert_token(&lexer, TOKEN_NUMBER, "4");

    lexer_init(&lexer, "3 - 4");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_MINUS, "-");
    assert_token(&lexer, TOKEN_NUMBER, "4");
}

void test_prefix_minus_spacing(void)
{
    // print - 3 4 (procedurally same as 3 minus 4, prefix form)
    Lexer lexer;
    lexer_init(&lexer, "- 3 4");
    // At start, minus is unary
    assert_token(&lexer, TOKEN_UNARY_MINUS, "-");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_NUMBER, "4");
}

//============================================================================
// Escaped Character Tests
//============================================================================

void test_escaped_delimiter_in_word(void)
{
    // St*rs typed as St\*rs
    Lexer lexer;
    lexer_init(&lexer, "St\\*rs");
    assert_token(&lexer, TOKEN_WORD, "St\\*rs");
}

void test_escaped_minus_in_word(void)
{
    // Pig-latin typed as Pig\-latin
    Lexer lexer;
    lexer_init(&lexer, "Pig\\-latin");
    assert_token(&lexer, TOKEN_WORD, "Pig\\-latin");
}

void test_escaped_space_in_word(void)
{
    // "Hi there" typed as Hi\ there
    Lexer lexer;
    lexer_init(&lexer, "Hi\\ there");
    assert_token(&lexer, TOKEN_WORD, "Hi\\ there");
}

void test_escaped_backslash(void)
{
    Lexer lexer;
    lexer_init(&lexer, "path\\\\name");
    assert_token(&lexer, TOKEN_WORD, "path\\\\name");
}

void test_escaped_brackets_in_word(void)
{
    // 3[a]b typed as 3\[a\]b
    Lexer lexer;
    lexer_init(&lexer, "3\\[a\\]b");
    assert_token(&lexer, TOKEN_WORD, "3\\[a\\]b");
}

//============================================================================
// Complex Expression Tests (from reference)
//============================================================================

void test_if_expression_no_spaces(void)
{
    // if 1<2[print(3+4)/5][print :x+6]
    Lexer lexer;
    lexer_init(&lexer, "if 1<2[print(3+4)/5][print :x+6]");
    
    assert_token(&lexer, TOKEN_WORD, "if");
    assert_token(&lexer, TOKEN_NUMBER, "1");
    assert_token(&lexer, TOKEN_LESS_THAN, "<");
    assert_token(&lexer, TOKEN_NUMBER, "2");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_WORD, "print");
    assert_token(&lexer, TOKEN_LEFT_PAREN, "(");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token(&lexer, TOKEN_PLUS, "+");
    assert_token(&lexer, TOKEN_NUMBER, "4");
    assert_token(&lexer, TOKEN_RIGHT_PAREN, ")");
    assert_token(&lexer, TOKEN_DIVIDE, "/");
    assert_token(&lexer, TOKEN_NUMBER, "5");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_WORD, "print");
    assert_token(&lexer, TOKEN_COLON, ":x");
    assert_token(&lexer, TOKEN_PLUS, "+");
    assert_token(&lexer, TOKEN_NUMBER, "6");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_procedure_definition(void)
{
    // to poly :step :angle
    Lexer lexer;
    lexer_init(&lexer, "to poly :step :angle");
    assert_token(&lexer, TOKEN_WORD, "to");
    assert_token(&lexer, TOKEN_WORD, "poly");
    assert_token(&lexer, TOKEN_COLON, ":step");
    assert_token(&lexer, TOKEN_COLON, ":angle");
}

void test_make_command(void)
{
    // make "bird "pigeon
    Lexer lexer;
    lexer_init(&lexer, "make \"bird \"pigeon");
    assert_token(&lexer, TOKEN_WORD, "make");
    assert_token(&lexer, TOKEN_QUOTED, "\"bird");
    assert_token(&lexer, TOKEN_QUOTED, "\"pigeon");
}

void test_if_equals_expression(void)
{
    // if :sound = "meow [pr "Cat stop]
    Lexer lexer;
    lexer_init(&lexer, "if :sound = \"meow [pr \"Cat stop]");
    assert_token(&lexer, TOKEN_WORD, "if");
    assert_token(&lexer, TOKEN_COLON, ":sound");
    assert_token(&lexer, TOKEN_EQUALS, "=");
    assert_token(&lexer, TOKEN_QUOTED, "\"meow");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_WORD, "pr");
    assert_token(&lexer, TOKEN_QUOTED, "\"Cat");
    assert_token(&lexer, TOKEN_WORD, "stop");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
}

void test_define_command(void)
{
    // define "square [[ ] [repeat 4 [fd 100 rt 90]]]
    Lexer lexer;
    lexer_init(&lexer, "define \"square [[ ] [repeat 4 [fd 100 rt 90]]]");
    assert_token(&lexer, TOKEN_WORD, "define");
    assert_token(&lexer, TOKEN_QUOTED, "\"square");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_WORD, "repeat");
    assert_token(&lexer, TOKEN_NUMBER, "4");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_WORD, "fd");
    assert_token(&lexer, TOKEN_NUMBER, "100");
    assert_token(&lexer, TOKEN_WORD, "rt");
    assert_token(&lexer, TOKEN_NUMBER, "90");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
}

//============================================================================
// Peek Tests
//============================================================================

void test_peek_token(void)
{
    Lexer lexer;
    lexer_init(&lexer, "forward 100");
    
    Token peeked = lexer_peek_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, peeked.type);
    
    // Next token should still be the same
    Token token = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, token.type);
    
    // Now peek should show 100
    peeked = lexer_peek_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_NUMBER, peeked.type);
}

void test_is_at_end(void)
{
    Lexer lexer;
    lexer_init(&lexer, "a");
    
    TEST_ASSERT_FALSE(lexer_is_at_end(&lexer));
    lexer_next_token(&lexer);
    TEST_ASSERT_TRUE(lexer_is_at_end(&lexer));
}

void test_is_at_end_with_whitespace(void)
{
    Lexer lexer;
    lexer_init(&lexer, "a   ");
    
    lexer_next_token(&lexer);
    TEST_ASSERT_TRUE(lexer_is_at_end(&lexer));
}

//============================================================================
// Token Type Name Tests
//============================================================================

void test_token_type_names(void)
{
    TEST_ASSERT_EQUAL_STRING("EOF", lexer_token_type_name(TOKEN_EOF));
    TEST_ASSERT_EQUAL_STRING("WORD", lexer_token_type_name(TOKEN_WORD));
    TEST_ASSERT_EQUAL_STRING("QUOTED", lexer_token_type_name(TOKEN_QUOTED));
    TEST_ASSERT_EQUAL_STRING("NUMBER", lexer_token_type_name(TOKEN_NUMBER));
    TEST_ASSERT_EQUAL_STRING("COLON", lexer_token_type_name(TOKEN_COLON));
    TEST_ASSERT_EQUAL_STRING("LEFT_BRACKET", lexer_token_type_name(TOKEN_LEFT_BRACKET));
    TEST_ASSERT_EQUAL_STRING("RIGHT_BRACKET", lexer_token_type_name(TOKEN_RIGHT_BRACKET));
    TEST_ASSERT_EQUAL_STRING("LEFT_PAREN", lexer_token_type_name(TOKEN_LEFT_PAREN));
    TEST_ASSERT_EQUAL_STRING("RIGHT_PAREN", lexer_token_type_name(TOKEN_RIGHT_PAREN));
    TEST_ASSERT_EQUAL_STRING("PLUS", lexer_token_type_name(TOKEN_PLUS));
    TEST_ASSERT_EQUAL_STRING("MINUS", lexer_token_type_name(TOKEN_MINUS));
    TEST_ASSERT_EQUAL_STRING("UNARY_MINUS", lexer_token_type_name(TOKEN_UNARY_MINUS));
    TEST_ASSERT_EQUAL_STRING("MULTIPLY", lexer_token_type_name(TOKEN_MULTIPLY));
    TEST_ASSERT_EQUAL_STRING("DIVIDE", lexer_token_type_name(TOKEN_DIVIDE));
    TEST_ASSERT_EQUAL_STRING("EQUALS", lexer_token_type_name(TOKEN_EQUALS));
    TEST_ASSERT_EQUAL_STRING("LESS_THAN", lexer_token_type_name(TOKEN_LESS_THAN));
    TEST_ASSERT_EQUAL_STRING("GREATER_THAN", lexer_token_type_name(TOKEN_GREATER_THAN));
    TEST_ASSERT_EQUAL_STRING("ERROR", lexer_token_type_name(TOKEN_ERROR));
}

//============================================================================
// Edge Case Tests
//============================================================================

void test_word_with_question_mark(void)
{
    Lexer lexer;
    lexer_init(&lexer, "Who?");
    assert_token(&lexer, TOKEN_WORD, "Who?");
}

void test_word_with_exclamation(void)
{
    Lexer lexer;
    lexer_init(&lexer, "!NOW!");
    assert_token(&lexer, TOKEN_WORD, "!NOW!");
}

void test_alphanumeric_word(void)
{
    Lexer lexer;
    lexer_init(&lexer, "R2D2");
    assert_token(&lexer, TOKEN_WORD, "R2D2");
}

void test_digit_starting_word(void)
{
    // 3a is a word, not a number followed by a word
    Lexer lexer;
    lexer_init(&lexer, "3a");
    assert_token(&lexer, TOKEN_WORD, "3a");
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_word_with_dot(void)
{
    Lexer lexer;
    lexer_init(&lexer, "Pig.latin");
    assert_token(&lexer, TOKEN_WORD, "Pig.latin");
}

void test_true_false_words(void)
{
    Lexer lexer;
    lexer_init(&lexer, "TRUE FALSE");
    assert_token(&lexer, TOKEN_WORD, "TRUE");
    assert_token(&lexer, TOKEN_WORD, "FALSE");
}

void test_quoted_empty_before_bracket(void)
{
    // "] outputs empty word then closing bracket
    Lexer lexer;
    lexer_init(&lexer, "\"]");
    assert_token(&lexer, TOKEN_QUOTED, "\"");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
}

//============================================================================
// Fuzz / Adversarial Input Tests
//============================================================================

// Helper: drain all tokens from lexer, return count (excluding EOF)
static int drain_tokens(Lexer *lexer)
{
    int count = 0;
    Token t;
    do
    {
        t = lexer_next_token(lexer);
        if (t.type != TOKEN_EOF)
            count++;
    } while (t.type != TOKEN_EOF);
    return count;
}

// --- Very long tokens ---

void test_fuzz_very_long_word(void)
{
    // A word of 10000 characters should not crash
    char *input = malloc(10001);
    TEST_ASSERT_NOT_NULL(input);
    memset(input, 'a', 10000);
    input[10000] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(10000, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);

    free(input);
}

void test_fuzz_very_long_number(void)
{
    // A number with 5000 digits
    char *input = malloc(5001);
    TEST_ASSERT_NOT_NULL(input);
    input[0] = '1';
    memset(input + 1, '0', 4999);
    input[5000] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_NUMBER, t.type);
    TEST_ASSERT_EQUAL_INT(5000, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);

    free(input);
}

void test_fuzz_very_long_quoted_word(void)
{
    // A quoted word of 8000 characters: "aaaa...
    char *input = malloc(8002);
    TEST_ASSERT_NOT_NULL(input);
    input[0] = '"';
    memset(input + 1, 'x', 8000);
    input[8001] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_QUOTED, t.type);
    TEST_ASSERT_EQUAL_INT(8001, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);

    free(input);
}

void test_fuzz_very_long_variable(void)
{
    // :aaa... with 6000 chars after colon
    char *input = malloc(6002);
    TEST_ASSERT_NOT_NULL(input);
    input[0] = ':';
    memset(input + 1, 'v', 6000);
    input[6001] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_COLON, t.type);
    TEST_ASSERT_EQUAL_INT(6001, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);

    free(input);
}

// --- Binary data and control characters ---

void test_fuzz_control_characters(void)
{
    // Control chars 0x01-0x08, 0x0B, 0x0C, 0x0E-0x1F are not whitespace
    // and not delimiters, so they should be consumed as word characters
    char input[] = {0x01, 0x02, 0x03, 0x7F, 0x00};
    Lexer lexer;
    lexer_init(&lexer, input);
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(4, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_high_bytes(void)
{
    // Characters 0x80-0xFF (high bytes, e.g. UTF-8 continuations)
    // should be treated as word characters
    char input[] = {(char)0x80, (char)0xC0, (char)0xFF, (char)0xFE, 0x00};
    Lexer lexer;
    lexer_init(&lexer, input);
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(4, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_binary_mixed_with_delimiters(void)
{
    // Binary chars mixed with operators: 0x01+0x02*0x03
    char input[] = {0x01, '+', 0x02, '*', 0x03, 0x00};
    Lexer lexer;
    lexer_init(&lexer, input);

    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(1, (int)t.length);

    assert_token_type(&lexer, TOKEN_PLUS);

    t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(1, (int)t.length);

    assert_token_type(&lexer, TOKEN_MULTIPLY);

    t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(1, (int)t.length);

    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_null_byte_terminates(void)
{
    // Embedded null byte should terminate tokenization
    // (C string semantics)
    char input[] = {'h', 'e', 'l', '\0', 'l', 'o'};
    Lexer lexer;
    lexer_init(&lexer, input);
    assert_token(&lexer, TOKEN_WORD, "hel");
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_control_chars_as_word_content(void)
{
    // Tab and newline ARE whitespace, but form feed, vertical tab are not
    // according to our is_space: only ' ', '\t', '\n', '\r'
    char input[] = {'a', '\x0B', 'b', '\x0C', 'c', 0x00};
    Lexer lexer;
    lexer_init(&lexer, input);
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    // VT (0x0B) and FF (0x0C) are not whitespace, so the whole thing is one word
    TEST_ASSERT_EQUAL_INT(5, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

// --- Backslash edge cases ---

void test_fuzz_trailing_backslash_in_word(void)
{
    // Backslash at very end of input inside a word
    Lexer lexer;
    lexer_init(&lexer, "hello\\");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    // Backslash plus nothing after it; lexer skips backslash, no next char
    TEST_ASSERT_EQUAL_INT(6, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_trailing_backslash_in_quoted(void)
{
    // Backslash at end in a quoted word: "hello\<EOF>
    Lexer lexer;
    lexer_init(&lexer, "\"hello\\");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_QUOTED, t.type);
    TEST_ASSERT_EQUAL_INT(7, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_trailing_backslash_in_colon(void)
{
    // Backslash at end in a variable ref: :var\<EOF>
    Lexer lexer;
    lexer_init(&lexer, ":var\\");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_COLON, t.type);
    TEST_ASSERT_EQUAL_INT(5, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_only_backslash(void)
{
    // Just a single backslash
    Lexer lexer;
    lexer_init(&lexer, "\\");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(1, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_many_backslashes(void)
{
    // Many consecutive backslashes (each escapes the next)
    Lexer lexer;
    lexer_init(&lexer, "\\\\\\\\\\\\\\\\");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(8, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_odd_backslashes(void)
{
    // Odd number of backslashes: last one has nothing to escape
    Lexer lexer;
    lexer_init(&lexer, "\\\\\\");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(3, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

// --- Lone special characters ---

void test_fuzz_lone_quote(void)
{
    // Just a quote character with nothing following
    Lexer lexer;
    lexer_init(&lexer, "\"");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_QUOTED, t.type);
    TEST_ASSERT_EQUAL_INT(1, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_lone_colon(void)
{
    // Just a colon with nothing following
    Lexer lexer;
    lexer_init(&lexer, ":");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_COLON, t.type);
    TEST_ASSERT_EQUAL_INT(1, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_lone_minus(void)
{
    // Just a minus sign
    Lexer lexer;
    lexer_init(&lexer, "-");
    Token t = lexer_next_token(&lexer);
    // At start of input with nothing after: unary minus (no digit follows)
    TEST_ASSERT_EQUAL_INT(TOKEN_UNARY_MINUS, t.type);
    assert_token_type(&lexer, TOKEN_EOF);
}

// --- All operators in sequence ---

void test_fuzz_all_operators_consecutive(void)
{
    // Every delimiter character adjacent: +-*/<=>[]()
    Lexer lexer;
    lexer_init(&lexer, "+-*/<=>[]()");
    assert_token_type(&lexer, TOKEN_PLUS);
    assert_token_type(&lexer, TOKEN_UNARY_MINUS); // unary: follows operator
    assert_token_type(&lexer, TOKEN_MULTIPLY);
    assert_token_type(&lexer, TOKEN_DIVIDE);
    assert_token_type(&lexer, TOKEN_LESS_THAN);
    assert_token_type(&lexer, TOKEN_EQUALS);
    assert_token_type(&lexer, TOKEN_GREATER_THAN);
    assert_token_type(&lexer, TOKEN_LEFT_BRACKET);
    assert_token_type(&lexer, TOKEN_RIGHT_BRACKET);
    assert_token_type(&lexer, TOKEN_LEFT_PAREN);
    assert_token_type(&lexer, TOKEN_RIGHT_PAREN);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_many_consecutive_minus(void)
{
    // 20 minus signs in a row — context-sensitive minus handling
    Lexer lexer;
    lexer_init(&lexer, "--------------------");
    int count = drain_tokens(&lexer);
    TEST_ASSERT_EQUAL_INT(20, count);
}

// --- Deeply nested brackets ---

void test_fuzz_deeply_nested_brackets(void)
{
    // 500 levels of nested brackets: [[[...]]]
    const int depth = 500;
    char *input = malloc(depth * 2 + 1);
    TEST_ASSERT_NOT_NULL(input);
    for (int i = 0; i < depth; i++)
        input[i] = '[';
    for (int i = 0; i < depth; i++)
        input[depth + i] = ']';
    input[depth * 2] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    int count = drain_tokens(&lexer);
    TEST_ASSERT_EQUAL_INT(depth * 2, count);

    free(input);
}

// --- Whitespace stress ---

void test_fuzz_very_long_whitespace(void)
{
    // 10000 spaces followed by a word
    char *input = malloc(10006);
    TEST_ASSERT_NOT_NULL(input);
    memset(input, ' ', 10000);
    memcpy(input + 10000, "hello", 5);
    input[10005] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    assert_token(&lexer, TOKEN_WORD, "hello");
    assert_token_type(&lexer, TOKEN_EOF);

    free(input);
}

void test_fuzz_only_whitespace_long(void)
{
    // 5000 mixed whitespace chars with no tokens
    char *input = malloc(5001);
    TEST_ASSERT_NOT_NULL(input);
    for (int i = 0; i < 5000; i++)
    {
        switch (i % 4)
        {
        case 0: input[i] = ' '; break;
        case 1: input[i] = '\t'; break;
        case 2: input[i] = '\n'; break;
        case 3: input[i] = '\r'; break;
        }
    }
    input[5000] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    assert_token_type(&lexer, TOKEN_EOF);
    TEST_ASSERT_TRUE(lexer_is_at_end(&lexer));

    free(input);
}

void test_fuzz_many_newlines(void)
{
    // After consuming many newlines, newline_count should be correct
    char *input = malloc(1001);
    TEST_ASSERT_NOT_NULL(input);
    memset(input, '\n', 999);
    input[999] = 'x';
    input[1000] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_TRUE(lexer.had_newline);
    TEST_ASSERT_EQUAL_INT(999, lexer.newline_count);
    assert_token_type(&lexer, TOKEN_EOF);

    free(input);
}

// --- Token text buffer edge cases ---

void test_fuzz_token_text_null_buffer(void)
{
    Lexer lexer;
    lexer_init(&lexer, "hello");
    Token t = lexer_next_token(&lexer);
    // NULL buffer: returns required size
    size_t needed = lexer_token_text(&t, NULL, 0);
    TEST_ASSERT_EQUAL_INT(5, (int)needed);
}

void test_fuzz_token_text_zero_size(void)
{
    Lexer lexer;
    lexer_init(&lexer, "hello");
    Token t = lexer_next_token(&lexer);
    char buf[10];
    // Zero size: returns required size
    size_t needed = lexer_token_text(&t, buf, 0);
    TEST_ASSERT_EQUAL_INT(5, (int)needed);
}

void test_fuzz_token_text_size_one(void)
{
    Lexer lexer;
    lexer_init(&lexer, "hello");
    Token t = lexer_next_token(&lexer);
    char buf[1];
    size_t copied = lexer_token_text(&t, buf, 1);
    TEST_ASSERT_EQUAL_INT(0, (int)copied);
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
}

void test_fuzz_token_text_truncation(void)
{
    // Token of 10000 chars, copy into small buffer
    char *input = malloc(10001);
    TEST_ASSERT_NOT_NULL(input);
    memset(input, 'z', 10000);
    input[10000] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    Token t = lexer_next_token(&lexer);

    char buf[8];
    size_t copied = lexer_token_text(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(7, (int)copied);
    TEST_ASSERT_EQUAL_STRING("zzzzzzz", buf);

    free(input);
}

// --- Invalid / malformed number patterns ---

void test_fuzz_negative_invalid_number(void)
{
    // -1.2.3 via minus path: minus sees digit after, calls read_number
    // read_number doesn't validate like looks_like_number does
    Lexer lexer;
    lexer_init(&lexer, "-1.2.3");
    Token t = lexer_next_token(&lexer);
    // This goes through the negative literal path
    // read_number consumes all number-like chars
    TEST_ASSERT_EQUAL_INT(TOKEN_NUMBER, t.type);
    // Verify it consumed the whole thing
    char buf[32];
    lexer_token_text(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-1.2.3", buf);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_negative_double_exponent(void)
{
    // -1e2e3 via minus path
    Lexer lexer;
    lexer_init(&lexer, "-1e2e3");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_NUMBER, t.type);
    char buf[32];
    lexer_token_text(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-1e2e3", buf);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_positive_double_dot_not_number(void)
{
    // 1.2.3 without leading minus: looks_like_number rejects it
    Lexer lexer;
    lexer_init(&lexer, "1.2.3");
    Token t = lexer_next_token(&lexer);
    // looks_like_number returns false for this, so it goes through read_word
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    char buf[32];
    lexer_token_text(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1.2.3", buf);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_negative_trailing_dot(void)
{
    // -1. is a valid-ish number (read_number will consume it)
    Lexer lexer;
    lexer_init(&lexer, "-1.");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_NUMBER, t.type);
    char buf[32];
    lexer_token_text(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-1.", buf);
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_fuzz_negative_trailing_e(void)
{
    // -1e via minus path — exponent with no digits after
    Lexer lexer;
    lexer_init(&lexer, "-1e");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_NUMBER, t.type);
    char buf[32];
    lexer_token_text(&t, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-1e", buf);
    assert_token_type(&lexer, TOKEN_EOF);
}

// --- Rapid alternation of token types ---

void test_fuzz_mixed_tokens_rapid(void)
{
    // Quick alternation: "a :b 3 [+] ("c)
    Lexer lexer;
    lexer_init(&lexer, "\"a :b 3 [+] (\"c)");
    assert_token(&lexer, TOKEN_QUOTED, "\"a");
    assert_token(&lexer, TOKEN_COLON, ":b");
    assert_token(&lexer, TOKEN_NUMBER, "3");
    assert_token_type(&lexer, TOKEN_LEFT_BRACKET);
    assert_token_type(&lexer, TOKEN_PLUS);
    assert_token_type(&lexer, TOKEN_RIGHT_BRACKET);
    assert_token_type(&lexer, TOKEN_LEFT_PAREN);
    assert_token(&lexer, TOKEN_QUOTED, "\"c");
    assert_token_type(&lexer, TOKEN_RIGHT_PAREN);
    assert_token_type(&lexer, TOKEN_EOF);
}

// --- Many tokens stress ---

void test_fuzz_many_small_tokens(void)
{
    // 2000 single-char words separated by spaces
    const int n = 2000;
    char *input = malloc(n * 2);
    TEST_ASSERT_NOT_NULL(input);
    for (int i = 0; i < n; i++)
    {
        input[i * 2] = 'a' + (i % 26);
        input[i * 2 + 1] = ' ';
    }
    input[n * 2 - 1] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    int count = drain_tokens(&lexer);
    TEST_ASSERT_EQUAL_INT(n, count);

    free(input);
}

void test_fuzz_many_quoted_words(void)
{
    // Hundreds of quoted words: "a "b "c ...
    const int n = 500;
    char *input = malloc(n * 3 + 1);
    TEST_ASSERT_NOT_NULL(input);
    for (int i = 0; i < n; i++)
    {
        input[i * 3] = '"';
        input[i * 3 + 1] = 'a' + (i % 26);
        input[i * 3 + 2] = ' ';
    }
    input[n * 3] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);
    int count = drain_tokens(&lexer);
    TEST_ASSERT_EQUAL_INT(n, count);

    free(input);
}

// --- Peek consistency under adversarial input ---

void test_fuzz_peek_consistency_long_input(void)
{
    // Peek should always return the same as the next actual token
    char *input = malloc(201);
    TEST_ASSERT_NOT_NULL(input);
    // Mix of operators, words, numbers
    const char pattern[] = "abc 123 + \"x :y [z] ";
    int plen = (int)strlen(pattern);
    for (int i = 0; i < 200; i++)
        input[i] = pattern[i % plen];
    input[200] = '\0';

    Lexer lexer;
    lexer_init(&lexer, input);

    Token peeked, actual;
    int count = 0;
    do
    {
        peeked = lexer_peek_token(&lexer);
        actual = lexer_next_token(&lexer);
        TEST_ASSERT_EQUAL_INT(peeked.type, actual.type);
        TEST_ASSERT_EQUAL_INT((int)peeked.length, (int)actual.length);
        TEST_ASSERT_EQUAL_PTR(peeked.start, actual.start);
        count++;
    } while (actual.type != TOKEN_EOF);

    TEST_ASSERT_TRUE(count > 1);
    free(input);
}

// --- Empty string ---

void test_fuzz_empty_string(void)
{
    Lexer lexer;
    lexer_init(&lexer, "");
    assert_token_type(&lexer, TOKEN_EOF);
    // Calling again should still return EOF
    assert_token_type(&lexer, TOKEN_EOF);
    assert_token_type(&lexer, TOKEN_EOF);
}

// --- Repeated EOF reads ---

void test_fuzz_repeated_eof(void)
{
    Lexer lexer;
    lexer_init(&lexer, "x");
    assert_token(&lexer, TOKEN_WORD, "x");
    // Read EOF many times — should never crash or change
    for (int i = 0; i < 100; i++)
    {
        Token t = lexer_next_token(&lexer);
        TEST_ASSERT_EQUAL_INT(TOKEN_EOF, t.type);
    }
}

// --- Escaped null-ish characters ---

void test_fuzz_backslash_before_delimiter(void)
{
    // Backslash should escape each delimiter in a word
    Lexer lexer;
    lexer_init(&lexer, "a\\+b\\*c\\=d");
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(10, (int)t.length);
    assert_token_type(&lexer, TOKEN_EOF);
}

// --- Mixed high byte + delimiter stress ---

void test_fuzz_high_bytes_with_brackets(void)
{
    // UTF-8-like bytes interspersed with brackets
    char input[] = {(char)0xC3, (char)0xA9, '[', (char)0xE2, (char)0x80, ']', 0x00};
    Lexer lexer;
    lexer_init(&lexer, input);
    Token t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(2, (int)t.length); // 0xC3, 0xA9
    assert_token_type(&lexer, TOKEN_LEFT_BRACKET);
    t = lexer_next_token(&lexer);
    TEST_ASSERT_EQUAL_INT(TOKEN_WORD, t.type);
    TEST_ASSERT_EQUAL_INT(2, (int)t.length); // 0xE2, 0x80
    assert_token_type(&lexer, TOKEN_RIGHT_BRACKET);
    assert_token_type(&lexer, TOKEN_EOF);
}

//============================================================================
// Main
//============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Basic tokens
    RUN_TEST(test_empty_input);
    RUN_TEST(test_whitespace_only);
    RUN_TEST(test_single_word);
    RUN_TEST(test_multiple_words);
    RUN_TEST(test_mixed_case_word);

    // Numbers
    RUN_TEST(test_integer_number);
    RUN_TEST(test_decimal_number);
    RUN_TEST(test_negative_number);
    RUN_TEST(test_negative_decimal);
    RUN_TEST(test_scientific_notation_e);
    RUN_TEST(test_scientific_notation_E);
    RUN_TEST(test_scientific_notation_n);
    RUN_TEST(test_scientific_notation_N);
    RUN_TEST(test_n_notation_no_sign);
    RUN_TEST(test_e_notation_with_sign);
    RUN_TEST(test_numbers_self_quoting);

    // Quoted words
    RUN_TEST(test_quoted_word);
    RUN_TEST(test_quoted_number);
    RUN_TEST(test_quoted_word_with_special_first_char);
    RUN_TEST(test_quoted_word_special_in_middle);
    RUN_TEST(test_quoted_bracket_needs_backslash);
    RUN_TEST(test_quoted_space_with_backslash);
    RUN_TEST(test_quoted_empty_word);
    RUN_TEST(test_print_heading_example);

    // Variable references
    RUN_TEST(test_variable_reference);
    RUN_TEST(test_variable_in_expression);
    RUN_TEST(test_multiple_variables);
    RUN_TEST(test_variable_in_poly_procedure);

    // Brackets
    RUN_TEST(test_simple_list);
    RUN_TEST(test_nested_lists);
    RUN_TEST(test_empty_list);
    RUN_TEST(test_list_with_numbers);
    RUN_TEST(test_list_with_negative_numbers);
    RUN_TEST(test_repeat_with_list);

    // Parentheses
    RUN_TEST(test_parentheses_grouping);
    RUN_TEST(test_variable_inputs);

    // Infix operators
    RUN_TEST(test_plus_operator);
    RUN_TEST(test_minus_operator_binary);
    RUN_TEST(test_multiply_operator);
    RUN_TEST(test_divide_operator);
    RUN_TEST(test_equals_operator);
    RUN_TEST(test_less_than_operator);
    RUN_TEST(test_greater_than_operator);
    RUN_TEST(test_operators_no_spaces);
    RUN_TEST(test_complex_expression);

    // Minus context
    RUN_TEST(test_minus_in_expression);
    RUN_TEST(test_minus_negative_number);
    RUN_TEST(test_minus_after_paren);
    RUN_TEST(test_minus_after_paren_space_before_no_space_after);
    RUN_TEST(test_minus_in_list);
    RUN_TEST(test_unary_minus_variable);
    RUN_TEST(test_unary_minus_word);
    RUN_TEST(test_binary_minus_spacing);
    RUN_TEST(test_prefix_minus_spacing);

    // Escaped characters
    RUN_TEST(test_escaped_delimiter_in_word);
    RUN_TEST(test_escaped_minus_in_word);
    RUN_TEST(test_escaped_space_in_word);
    RUN_TEST(test_escaped_backslash);
    RUN_TEST(test_escaped_brackets_in_word);

    // Complex expressions
    RUN_TEST(test_if_expression_no_spaces);
    RUN_TEST(test_procedure_definition);
    RUN_TEST(test_make_command);
    RUN_TEST(test_if_equals_expression);
    RUN_TEST(test_define_command);

    // Peek and utility
    RUN_TEST(test_peek_token);
    RUN_TEST(test_is_at_end);
    RUN_TEST(test_is_at_end_with_whitespace);
    RUN_TEST(test_token_type_names);

    // Edge cases
    RUN_TEST(test_word_with_question_mark);
    RUN_TEST(test_word_with_exclamation);
    RUN_TEST(test_alphanumeric_word);
    RUN_TEST(test_digit_starting_word);
    RUN_TEST(test_word_with_dot);
    RUN_TEST(test_true_false_words);
    RUN_TEST(test_quoted_empty_before_bracket);

    // Fuzz / adversarial input
    RUN_TEST(test_fuzz_very_long_word);
    RUN_TEST(test_fuzz_very_long_number);
    RUN_TEST(test_fuzz_very_long_quoted_word);
    RUN_TEST(test_fuzz_very_long_variable);
    RUN_TEST(test_fuzz_control_characters);
    RUN_TEST(test_fuzz_high_bytes);
    RUN_TEST(test_fuzz_binary_mixed_with_delimiters);
    RUN_TEST(test_fuzz_null_byte_terminates);
    RUN_TEST(test_fuzz_control_chars_as_word_content);
    RUN_TEST(test_fuzz_trailing_backslash_in_word);
    RUN_TEST(test_fuzz_trailing_backslash_in_quoted);
    RUN_TEST(test_fuzz_trailing_backslash_in_colon);
    RUN_TEST(test_fuzz_only_backslash);
    RUN_TEST(test_fuzz_many_backslashes);
    RUN_TEST(test_fuzz_odd_backslashes);
    RUN_TEST(test_fuzz_lone_quote);
    RUN_TEST(test_fuzz_lone_colon);
    RUN_TEST(test_fuzz_lone_minus);
    RUN_TEST(test_fuzz_all_operators_consecutive);
    RUN_TEST(test_fuzz_many_consecutive_minus);
    RUN_TEST(test_fuzz_deeply_nested_brackets);
    RUN_TEST(test_fuzz_very_long_whitespace);
    RUN_TEST(test_fuzz_only_whitespace_long);
    RUN_TEST(test_fuzz_many_newlines);
    RUN_TEST(test_fuzz_token_text_null_buffer);
    RUN_TEST(test_fuzz_token_text_zero_size);
    RUN_TEST(test_fuzz_token_text_size_one);
    RUN_TEST(test_fuzz_token_text_truncation);
    RUN_TEST(test_fuzz_negative_invalid_number);
    RUN_TEST(test_fuzz_negative_double_exponent);
    RUN_TEST(test_fuzz_positive_double_dot_not_number);
    RUN_TEST(test_fuzz_negative_trailing_dot);
    RUN_TEST(test_fuzz_negative_trailing_e);
    RUN_TEST(test_fuzz_mixed_tokens_rapid);
    RUN_TEST(test_fuzz_many_small_tokens);
    RUN_TEST(test_fuzz_many_quoted_words);
    RUN_TEST(test_fuzz_peek_consistency_long_input);
    RUN_TEST(test_fuzz_empty_string);
    RUN_TEST(test_fuzz_repeated_eof);
    RUN_TEST(test_fuzz_backslash_before_delimiter);
    RUN_TEST(test_fuzz_high_bytes_with_brackets);

    return UNITY_END();
}
