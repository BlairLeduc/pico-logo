//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the lexer.
//

#include "unity.h"
#include "core/lexer.h"

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
// Data Mode Tests
//============================================================================

void test_data_mode_phone_number(void)
{
    // In data mode, [Bob 555-1212] should be 2 tokens, not 4
    Lexer lexer;
    lexer_init_data(&lexer, "Bob 555-1212");
    assert_token(&lexer, TOKEN_WORD, "Bob");
    assert_token(&lexer, TOKEN_WORD, "555-1212");
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_data_mode_operators_in_words(void)
{
    // In data mode, operators are part of words
    Lexer lexer;
    lexer_init_data(&lexer, "a+b c*d e/f x=y");
    assert_token(&lexer, TOKEN_WORD, "a+b");
    assert_token(&lexer, TOKEN_WORD, "c*d");
    assert_token(&lexer, TOKEN_WORD, "e/f");
    assert_token(&lexer, TOKEN_WORD, "x=y");
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_data_mode_brackets_still_delimit(void)
{
    // Brackets should still work as delimiters in data mode
    Lexer lexer;
    lexer_init_data(&lexer, "hello [world] there");
    assert_token(&lexer, TOKEN_WORD, "hello");
    assert_token(&lexer, TOKEN_LEFT_BRACKET, "[");
    assert_token(&lexer, TOKEN_WORD, "world");
    assert_token(&lexer, TOKEN_RIGHT_BRACKET, "]");
    assert_token(&lexer, TOKEN_WORD, "there");
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_data_mode_parens_in_words(void)
{
    // In data mode, parentheses are part of words
    Lexer lexer;
    lexer_init_data(&lexer, "hello(world) foo(bar)baz");
    assert_token(&lexer, TOKEN_WORD, "hello(world)");
    assert_token(&lexer, TOKEN_WORD, "foo(bar)baz");
    assert_token_type(&lexer, TOKEN_EOF);
}

void test_code_mode_still_splits_operators(void)
{
    // Verify code mode still works as before
    Lexer lexer;
    lexer_init(&lexer, "100-20");
    assert_token(&lexer, TOKEN_NUMBER, "100");
    assert_token(&lexer, TOKEN_MINUS, "-");
    assert_token(&lexer, TOKEN_NUMBER, "20");
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

    // Data mode tests
    RUN_TEST(test_data_mode_phone_number);
    RUN_TEST(test_data_mode_operators_in_words);
    RUN_TEST(test_data_mode_brackets_still_delimit);
    RUN_TEST(test_data_mode_parens_in_words);
    RUN_TEST(test_code_mode_still_splits_operators);

    return UNITY_END();
}
