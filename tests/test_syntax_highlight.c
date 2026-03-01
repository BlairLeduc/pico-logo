//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the syntax highlighter.
//

#include "unity.h"
#include "core/syntax_highlight.h"

#include <stdio.h>
#include <string.h>

void setUp(void)  {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Assert that every character in categories[from..to) equals `expected`.
static void assert_range(const uint8_t *cat, int from, int to,
                         SyntaxCategory expected, const char *msg)
{
    for (int i = from; i < to; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s at index %d", msg, i);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)expected, cat[i], buf);
    }
}

// Assert a single position
static void assert_cat(const uint8_t *cat, int pos,
                       SyntaxCategory expected, const char *msg)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%s at index %d", msg, pos);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)expected, cat[pos], buf);
}

// ---------------------------------------------------------------------------
// Empty / whitespace
// ---------------------------------------------------------------------------

void test_empty_line(void)
{
    uint8_t cat[1];
    int depth = syntax_highlight_line("", 0, cat, 0);
    TEST_ASSERT_EQUAL_INT(0, depth);
}

void test_whitespace_only(void)
{
    const char *line = "   \t  ";
    int len = (int)strlen(line);
    uint8_t cat[16];
    int depth = syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_DEFAULT, "whitespace");
    TEST_ASSERT_EQUAL_INT(0, depth);
}

// ---------------------------------------------------------------------------
// TO / END keywords
// ---------------------------------------------------------------------------

void test_to_simple(void)
{
    //  "to myproc :x :y"
    //   01234567890123456
    const char *line = "to myproc :x :y";
    int len = (int)strlen(line);
    uint8_t cat[32];
    int depth = syntax_highlight_line(line, len, cat, 5);

    // "to" → keyword
    assert_range(cat, 0, 2, SYNTAX_KEYWORD, "to keyword");
    // space
    assert_cat(cat, 2, SYNTAX_DEFAULT, "space after to");
    // "myproc" → function
    assert_range(cat, 3, 9, SYNTAX_FUNCTION, "function name");
    // space
    assert_cat(cat, 9, SYNTAX_DEFAULT, "space after name");
    // ":x" → variable
    assert_range(cat, 10, 12, SYNTAX_VARIABLE, "param :x");
    // space
    assert_cat(cat, 12, SYNTAX_DEFAULT, "space between params");
    // ":y" → variable
    assert_range(cat, 13, 15, SYNTAX_VARIABLE, "param :y");

    // TO resets depth to 0
    TEST_ASSERT_EQUAL_INT(0, depth);
}

void test_to_case_insensitive(void)
{
    const char *line = "TO myproc";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 2, SYNTAX_KEYWORD, "TO uppercase");
    assert_range(cat, 3, 9, SYNTAX_FUNCTION, "function name");
}

void test_to_with_leading_whitespace(void)
{
    const char *line = "  to myproc";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 2, SYNTAX_DEFAULT, "leading spaces");
    assert_range(cat, 2, 4, SYNTAX_KEYWORD, "to keyword");
    assert_range(cat, 5, 11, SYNTAX_FUNCTION, "function name");
}

void test_end_simple(void)
{
    const char *line = "end";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 3, SYNTAX_KEYWORD, "end keyword");
}

void test_end_case_insensitive(void)
{
    const char *line = "End";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 3, SYNTAX_KEYWORD, "End keyword");
}

void test_end_with_trailing_whitespace(void)
{
    const char *line = "end   ";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 3, SYNTAX_KEYWORD, "end keyword");
    assert_range(cat, 3, len, SYNTAX_DEFAULT, "trailing spaces");
}

void test_end_not_at_start(void)
{
    // "end" with text before it is not a keyword
    const char *line = "print end";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    // "print" is a command, "end" is a command (not keyword)
    assert_range(cat, 0, 5, SYNTAX_COMMAND, "print");
    assert_range(cat, 6, 9, SYNTAX_COMMAND, "end as command");
}

void test_end_with_extra_text_is_not_keyword(void)
{
    // "end foo" — END must be followed only by whitespace or EOL
    const char *line = "endstuff";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    // Not a keyword END — treated as a command
    assert_range(cat, 0, len, SYNTAX_COMMAND, "endstuff as command");
}

// ---------------------------------------------------------------------------
// Variables
// ---------------------------------------------------------------------------

void test_variable(void)
{
    const char *line = ":myvar";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_VARIABLE, "variable");
}

void test_variable_in_context(void)
{
    const char *line = "print :x";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 5, SYNTAX_COMMAND, "print");
    assert_cat(cat, 5, SYNTAX_DEFAULT, "space");
    assert_range(cat, 6, 8, SYNTAX_VARIABLE, ":x");
}

// ---------------------------------------------------------------------------
// Quoted words (strings)
// ---------------------------------------------------------------------------

void test_quoted_word(void)
{
    const char *line = "\"hello";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_STRING, "quoted word");
}

void test_quoted_word_to_not_keyword(void)
{
    // "to is a string, not a keyword
    const char *line = "print \"to";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 5, SYNTAX_COMMAND, "print");
    assert_cat(cat, 5, SYNTAX_DEFAULT, "space");
    assert_range(cat, 6, 9, SYNTAX_STRING, "quoted to");
}

// ---------------------------------------------------------------------------
// Numbers
// ---------------------------------------------------------------------------

void test_number_integer(void)
{
    const char *line = "123";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 3, SYNTAX_NUMBER, "integer");
}

void test_number_decimal(void)
{
    const char *line = "3.14";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 4, SYNTAX_NUMBER, "decimal");
}

void test_number_negative(void)
{
    const char *line = "-5";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 2, SYNTAX_NUMBER, "negative");
}

void test_number_exponent(void)
{
    const char *line = "1e10";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 4, SYNTAX_NUMBER, "exponent");
}

void test_number_exponent_negative(void)
{
    const char *line = "2.5E-3";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 6, SYNTAX_NUMBER, "exponent neg");
}

void test_number_exponent_no_digits(void)
{
    // "1e" has no digit after exponent — should NOT highlight as number
    const char *line = "1e";
    int len = (int)strlen(line);
    uint8_t cat[4];
    syntax_highlight_line(line, len, cat, 0);
    // scan_number fails, "1" stays SYNTAX_DEFAULT, "e" becomes SYNTAX_COMMAND
    assert_cat(cat, 0, SYNTAX_DEFAULT, "1e: digit");
    assert_cat(cat, 1, SYNTAX_COMMAND, "1e: letter");
}

void test_number_exponent_sign_no_digits(void)
{
    // "1e+" has a sign but no digit after — should NOT highlight as number
    const char *line = "1e+";
    int len = (int)strlen(line);
    uint8_t cat[4];
    syntax_highlight_line(line, len, cat, 0);
    // scan_number fails => "1" default, "e" command, "+" default (operator)
    assert_cat(cat, 0, SYNTAX_DEFAULT, "1e+: digit");
    assert_cat(cat, 1, SYNTAX_COMMAND, "1e+: letter");
    assert_cat(cat, 2, SYNTAX_DEFAULT, "1e+: sign");
}

void test_number_exponent_n_no_sign(void)
{
    // "1n5" is valid (n exponent, no sign allowed)
    const char *line = "1n5";
    int len = (int)strlen(line);
    uint8_t cat[4];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 3, SYNTAX_NUMBER, "1n5");
}

void test_number_exponent_n_with_sign(void)
{
    // "1n+5" — n/N does not allow sign, should NOT highlight as number
    const char *line = "1n+5";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);
    assert_cat(cat, 0, SYNTAX_DEFAULT, "1n+5[0]");
}

void test_number_in_context(void)
{
    const char *line = "forward 100";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 7, SYNTAX_COMMAND, "forward");
    assert_cat(cat, 7, SYNTAX_DEFAULT, "space");
    assert_range(cat, 8, 11, SYNTAX_NUMBER, "100");
}

// ---------------------------------------------------------------------------
// Operators
// ---------------------------------------------------------------------------

void test_operators(void)
{
    const char *line = "+ - * / = < >";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    // All characters should be SYNTAX_DEFAULT (operators + spaces)
    assert_range(cat, 0, len, SYNTAX_DEFAULT, "operators");
}

// ---------------------------------------------------------------------------
// Comments
// ---------------------------------------------------------------------------

void test_comment_block(void)
{
    const char *line = "; [this is a comment]";
    int len = (int)strlen(line);
    uint8_t cat[32];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_COMMENT, "block comment");
}

void test_comment_nested_brackets(void)
{
    const char *line = "; [a [b] c]";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_COMMENT, "nested comment");
}

void test_comment_line(void)
{
    // ; without [ is a line comment
    const char *line = "; rest of line";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_COMMENT, "line comment");
}

void test_comment_after_code(void)
{
    const char *line = "fd 100 ; [go forward]";
    int len = (int)strlen(line);
    uint8_t cat[32];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, 2, SYNTAX_COMMAND, "fd");
    assert_cat(cat, 2, SYNTAX_DEFAULT, "space");
    assert_range(cat, 3, 6, SYNTAX_NUMBER, "100");
    assert_cat(cat, 6, SYNTAX_DEFAULT, "space");
    assert_range(cat, 7, len, SYNTAX_COMMENT, "comment");
}

// ---------------------------------------------------------------------------
// Brackets — nesting depth and colorization
// ---------------------------------------------------------------------------

void test_bracket_single_level(void)
{
    const char *line = "[fd 100]";
    int len = (int)strlen(line);
    uint8_t cat[16];
    int depth = syntax_highlight_line(line, len, cat, 0);
    assert_cat(cat, 0, SYNTAX_BRACKET_1, "opening [");
    assert_range(cat, 1, 3, SYNTAX_COMMAND, "fd");
    assert_cat(cat, 3, SYNTAX_DEFAULT, "space");
    assert_range(cat, 4, 7, SYNTAX_NUMBER, "100");
    assert_cat(cat, 7, SYNTAX_BRACKET_1, "closing ]");
    TEST_ASSERT_EQUAL_INT(0, depth);
}

void test_bracket_nested(void)
{
    const char *line = "[[a] [b]]";
    int len = (int)strlen(line);
    uint8_t cat[16];
    int depth = syntax_highlight_line(line, len, cat, 0);

    // Outer [ = depth 1 → BRACKET_1
    assert_cat(cat, 0, SYNTAX_BRACKET_1, "outer [");
    // Inner [ = depth 2 → BRACKET_2
    assert_cat(cat, 1, SYNTAX_BRACKET_2, "inner [1");
    assert_cat(cat, 2, SYNTAX_COMMAND, "a");
    // Inner ] = depth 2 → BRACKET_2
    assert_cat(cat, 3, SYNTAX_BRACKET_2, "inner ]1");
    assert_cat(cat, 4, SYNTAX_DEFAULT, "space");
    // Second inner [ = depth 2 → BRACKET_2
    assert_cat(cat, 5, SYNTAX_BRACKET_2, "inner [2");
    assert_cat(cat, 6, SYNTAX_COMMAND, "b");
    // Second inner ] = depth 2 → BRACKET_2
    assert_cat(cat, 7, SYNTAX_BRACKET_2, "inner ]2");
    // Outer ] = depth 1 → BRACKET_1
    assert_cat(cat, 8, SYNTAX_BRACKET_1, "outer ]");
    TEST_ASSERT_EQUAL_INT(0, depth);
}

void test_bracket_triple_nested(void)
{
    const char *line = "[[[x]]]";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);

    assert_cat(cat, 0, SYNTAX_BRACKET_1, "depth 1 [");
    assert_cat(cat, 1, SYNTAX_BRACKET_2, "depth 2 [");
    assert_cat(cat, 2, SYNTAX_BRACKET_3, "depth 3 [");
    assert_cat(cat, 3, SYNTAX_COMMAND, "x");
    assert_cat(cat, 4, SYNTAX_BRACKET_3, "depth 3 ]");
    assert_cat(cat, 5, SYNTAX_BRACKET_2, "depth 2 ]");
    assert_cat(cat, 6, SYNTAX_BRACKET_1, "depth 1 ]");
}

void test_bracket_depth_wraps(void)
{
    // 4 levels: depths 1,2,3 then 4 wraps to BRACKET_1
    const char *line = "[[[[y]]]]";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);

    assert_cat(cat, 0, SYNTAX_BRACKET_1, "depth 1");
    assert_cat(cat, 1, SYNTAX_BRACKET_2, "depth 2");
    assert_cat(cat, 2, SYNTAX_BRACKET_3, "depth 3");
    assert_cat(cat, 3, SYNTAX_BRACKET_1, "depth 4 wraps");
    assert_cat(cat, 4, SYNTAX_COMMAND, "y");
    assert_cat(cat, 5, SYNTAX_BRACKET_1, "close depth 4");
    assert_cat(cat, 6, SYNTAX_BRACKET_3, "close depth 3");
    assert_cat(cat, 7, SYNTAX_BRACKET_2, "close depth 2");
    assert_cat(cat, 8, SYNTAX_BRACKET_1, "close depth 1");
}

void test_parentheses_nesting(void)
{
    const char *line = "(sum 1 (sum 2 3))";
    int len = (int)strlen(line);
    uint8_t cat[32];
    syntax_highlight_line(line, len, cat, 0);

    assert_cat(cat, 0, SYNTAX_BRACKET_1, "outer (");
    assert_range(cat, 1, 4, SYNTAX_COMMAND, "sum");
    assert_cat(cat, 4, SYNTAX_DEFAULT, "space");
    assert_cat(cat, 5, SYNTAX_NUMBER, "1");
    assert_cat(cat, 6, SYNTAX_DEFAULT, "space");
    assert_cat(cat, 7, SYNTAX_BRACKET_2, "inner (");
    assert_range(cat, 8, 11, SYNTAX_COMMAND, "sum");
    assert_cat(cat, 11, SYNTAX_DEFAULT, "space");
    assert_cat(cat, 12, SYNTAX_NUMBER, "2");
    assert_cat(cat, 13, SYNTAX_DEFAULT, "space");
    assert_cat(cat, 14, SYNTAX_NUMBER, "3");
    assert_cat(cat, 15, SYNTAX_BRACKET_2, "inner )");
    assert_cat(cat, 16, SYNTAX_BRACKET_1, "outer )");
}

void test_mixed_brackets_and_parens_share_depth(void)
{
    const char *line = "[(a)]";
    int len = (int)strlen(line);
    uint8_t cat[8];
    syntax_highlight_line(line, len, cat, 0);

    assert_cat(cat, 0, SYNTAX_BRACKET_1, "[ depth 1");
    assert_cat(cat, 1, SYNTAX_BRACKET_2, "( depth 2");
    assert_cat(cat, 2, SYNTAX_COMMAND, "a");
    assert_cat(cat, 3, SYNTAX_BRACKET_2, ") depth 2");
    assert_cat(cat, 4, SYNTAX_BRACKET_1, "] depth 1");
}

// ---------------------------------------------------------------------------
// Multi-line bracket depth tracking
// ---------------------------------------------------------------------------

void test_initial_depth_carries(void)
{
    // Line opens a bracket — returned depth should be 1
    const char *line1 = "repeat 4 [";
    int len1 = (int)strlen(line1);
    uint8_t cat1[16];
    int depth = syntax_highlight_line(line1, len1, cat1, 0);
    TEST_ASSERT_EQUAL_INT(1, depth);

    // Next line inherits depth 1 — brackets inside start at depth 2
    const char *line2 = "fd 100";
    int len2 = (int)strlen(line2);
    uint8_t cat2[8];
    depth = syntax_highlight_line(line2, len2, cat2, depth);
    // No brackets on this line, depth stays 1
    TEST_ASSERT_EQUAL_INT(1, depth);

    // Closing bracket — returns to depth 0
    const char *line3 = "]";
    int len3 = (int)strlen(line3);
    uint8_t cat3[4];
    depth = syntax_highlight_line(line3, len3, cat3, depth);
    assert_cat(cat3, 0, SYNTAX_BRACKET_1, "closing ] at depth 1");
    TEST_ASSERT_EQUAL_INT(0, depth);
}

void test_to_resets_depth(void)
{
    // Even if initial depth is 3, a TO line resets to 0
    const char *line = "to myproc :x";
    int len = (int)strlen(line);
    uint8_t cat[16];
    int depth = syntax_highlight_line(line, len, cat, 3);
    TEST_ASSERT_EQUAL_INT(0, depth);
}

void test_null_categories_depth_only(void)
{
    // With NULL categories, only depth tracking should work
    const char *line = "repeat 4 [fd [100]]";
    int len = (int)strlen(line);
    int depth = syntax_highlight_line(line, len, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, depth);

    // Open bracket, no close
    const char *line2 = "[";
    depth = syntax_highlight_line(line2, 1, NULL, 0);
    TEST_ASSERT_EQUAL_INT(1, depth);
}

// ---------------------------------------------------------------------------
// Unbalanced brackets
// ---------------------------------------------------------------------------

void test_unmatched_close_bracket(void)
{
    const char *line = "]";
    int len = (int)strlen(line);
    uint8_t cat[4];
    int depth = syntax_highlight_line(line, len, cat, 0);
    // Depth stays at 0, bracket gets BRACKET_1 coloring
    assert_cat(cat, 0, SYNTAX_BRACKET_1, "unmatched ]");
    TEST_ASSERT_EQUAL_INT(0, depth);
}

// ---------------------------------------------------------------------------
// Mixed / complex lines
// ---------------------------------------------------------------------------

void test_repeat_command(void)
{
    //  "repeat 4 [fd 100 rt 90]"
    //   0123456789012345678901234
    const char *line = "repeat 4 [fd 100 rt 90]";
    int len = (int)strlen(line);
    uint8_t cat[32];
    syntax_highlight_line(line, len, cat, 0);

    assert_range(cat, 0, 6, SYNTAX_COMMAND, "repeat");
    assert_cat(cat, 6, SYNTAX_DEFAULT, "space");
    assert_cat(cat, 7, SYNTAX_NUMBER, "4");
    assert_cat(cat, 8, SYNTAX_DEFAULT, "space");
    assert_cat(cat, 9, SYNTAX_BRACKET_1, "[");
    assert_range(cat, 10, 12, SYNTAX_COMMAND, "fd");
    assert_cat(cat, 12, SYNTAX_DEFAULT, "space");
    assert_range(cat, 13, 16, SYNTAX_NUMBER, "100");
    assert_cat(cat, 16, SYNTAX_DEFAULT, "space");
    assert_range(cat, 17, 19, SYNTAX_COMMAND, "rt");
    assert_cat(cat, 19, SYNTAX_DEFAULT, "space");
    assert_range(cat, 20, 22, SYNTAX_NUMBER, "90");
    assert_cat(cat, 22, SYNTAX_BRACKET_1, "]");
}

void test_make_variable(void)
{
    const char *line = "make \"x 42";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);

    assert_range(cat, 0, 4, SYNTAX_COMMAND, "make");
    assert_cat(cat, 4, SYNTAX_DEFAULT, "space");
    assert_range(cat, 5, 7, SYNTAX_STRING, "\"x");
    assert_cat(cat, 7, SYNTAX_DEFAULT, "space");
    assert_range(cat, 8, 10, SYNTAX_NUMBER, "42");
}

void test_comment_brackets_not_colored_as_brackets(void)
{
    // Brackets inside comments should not get bracket coloring
    const char *line = "; [a [b] c]";
    int len = (int)strlen(line);
    uint8_t cat[16];
    int depth = syntax_highlight_line(line, len, cat, 0);
    // Everything should be SYNTAX_COMMENT
    assert_range(cat, 0, len, SYNTAX_COMMENT, "comment bracketed");
    // Brackets in the comment do NOT affect depth
    TEST_ASSERT_EQUAL_INT(0, depth);
}

// ---------------------------------------------------------------------------
// Backslash escape handling
// ---------------------------------------------------------------------------

void test_quoted_word_with_escaped_spaces(void)
{
    // "g\ o\ \"\ f  is a single quoted word with escaped spaces and quote
    const char *line = "\"g\\ o\\ \\\"\\ f";
    //                   "  g  \  _  o  \  _  \  "  \  _  f
    //                   0  1  2  3  4  5  6  7  8  9  10 11
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    // The entire token should be SYNTAX_STRING
    assert_range(cat, 0, len, SYNTAX_STRING, "quoted word with escapes");
}

void test_quoted_word_with_escaped_bracket(void)
{
    // "hello\[world  — escaped bracket doesn't end the word
    const char *line = "\"hello\\[world";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_STRING, "quoted word escaped bracket");
}

void test_variable_with_escaped_space(void)
{
    // :my\ var  — variable name with escaped space
    const char *line = ":my\\ var";
    //                  :  m  y  \  _  v  a  r
    //                  0  1  2  3  4  5  6  7
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_VARIABLE, "variable with escaped space");
}

void test_word_with_escaped_space(void)
{
    // go\ home  — bare word with escaped space
    const char *line = "go\\ home";
    //                  g  o  \  _  h  o  m  e
    //                  0  1  2  3  4  5  6  7
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_COMMAND, "word with escaped space");
}

void test_word_with_escaped_bracket(void)
{
    // go\[home  — bare word with escaped bracket
    const char *line = "go\\[home";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_COMMAND, "word with escaped bracket");
}

void test_word_with_colon_mid_word(void)
{
    // go:there — colon mid-word should be a single command, not a variable
    const char *line = "go:there";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_COMMAND, "word with colon mid-word");
}

void test_word_with_colon_in_brackets(void)
{
    // [go:there] — colon mid-word inside brackets
    const char *line = "[go:there]";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    TEST_ASSERT_EQUAL_UINT8(SYNTAX_BRACKET_1, cat[0]);
    assert_range(cat, 1, 9, SYNTAX_COMMAND, "word with colon in brackets");
    TEST_ASSERT_EQUAL_UINT8(SYNTAX_BRACKET_1, cat[9]);
}

void test_quoted_word_with_colon(void)
{
    // "go:there — colon mid-word in quoted word
    const char *line = "\"go:there";
    int len = (int)strlen(line);
    uint8_t cat[16];
    syntax_highlight_line(line, len, cat, 0);
    assert_range(cat, 0, len, SYNTAX_STRING, "quoted word with colon");
}

void test_to_body_highlighting(void)
{
    // After TO name :params, the remaining body should be highlighted
    const char *line = "to box :size fd :size rt 90";
    int len = (int)strlen(line);
    uint8_t cat[32];
    syntax_highlight_line(line, len, cat, 0);

    assert_range(cat, 0, 2, SYNTAX_KEYWORD, "to");
    assert_range(cat, 3, 6, SYNTAX_FUNCTION, "box");
    assert_range(cat, 7, 12, SYNTAX_VARIABLE, ":size (param)");
    assert_range(cat, 13, 15, SYNTAX_COMMAND, "fd");
    assert_range(cat, 16, 21, SYNTAX_VARIABLE, ":size (usage)");
    assert_range(cat, 22, 24, SYNTAX_COMMAND, "rt");
    assert_range(cat, 25, 27, SYNTAX_NUMBER, "90");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void)
{
    UNITY_BEGIN();

    // Empty / whitespace
    RUN_TEST(test_empty_line);
    RUN_TEST(test_whitespace_only);

    // TO / END
    RUN_TEST(test_to_simple);
    RUN_TEST(test_to_case_insensitive);
    RUN_TEST(test_to_with_leading_whitespace);
    RUN_TEST(test_end_simple);
    RUN_TEST(test_end_case_insensitive);
    RUN_TEST(test_end_with_trailing_whitespace);
    RUN_TEST(test_end_not_at_start);
    RUN_TEST(test_end_with_extra_text_is_not_keyword);

    // Variables
    RUN_TEST(test_variable);
    RUN_TEST(test_variable_in_context);

    // Quoted words
    RUN_TEST(test_quoted_word);
    RUN_TEST(test_quoted_word_to_not_keyword);

    // Numbers
    RUN_TEST(test_number_integer);
    RUN_TEST(test_number_decimal);
    RUN_TEST(test_number_negative);
    RUN_TEST(test_number_exponent);
    RUN_TEST(test_number_exponent_negative);
    RUN_TEST(test_number_exponent_no_digits);
    RUN_TEST(test_number_exponent_sign_no_digits);
    RUN_TEST(test_number_exponent_n_no_sign);
    RUN_TEST(test_number_exponent_n_with_sign);
    RUN_TEST(test_number_in_context);

    // Operators
    RUN_TEST(test_operators);

    // Comments
    RUN_TEST(test_comment_block);
    RUN_TEST(test_comment_nested_brackets);
    RUN_TEST(test_comment_line);
    RUN_TEST(test_comment_after_code);

    // Brackets
    RUN_TEST(test_bracket_single_level);
    RUN_TEST(test_bracket_nested);
    RUN_TEST(test_bracket_triple_nested);
    RUN_TEST(test_bracket_depth_wraps);
    RUN_TEST(test_parentheses_nesting);
    RUN_TEST(test_mixed_brackets_and_parens_share_depth);

    // Multi-line depth
    RUN_TEST(test_initial_depth_carries);
    RUN_TEST(test_to_resets_depth);
    RUN_TEST(test_null_categories_depth_only);

    // Unbalanced
    RUN_TEST(test_unmatched_close_bracket);

    // Backslash escapes
    RUN_TEST(test_quoted_word_with_escaped_spaces);
    RUN_TEST(test_quoted_word_with_escaped_bracket);
    RUN_TEST(test_variable_with_escaped_space);
    RUN_TEST(test_word_with_escaped_space);
    RUN_TEST(test_word_with_escaped_bracket);

    // Colon mid-word (not a variable)
    RUN_TEST(test_word_with_colon_mid_word);
    RUN_TEST(test_word_with_colon_in_brackets);
    RUN_TEST(test_quoted_word_with_colon);

    // Complex lines
    RUN_TEST(test_repeat_command);
    RUN_TEST(test_make_variable);
    RUN_TEST(test_comment_brackets_not_colored_as_brackets);
    RUN_TEST(test_to_body_highlighting);

    return UNITY_END();
}
