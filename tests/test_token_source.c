//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the token source abstraction.
//

#include "unity.h"
#include "core/token_source.h"
#include "core/memory.h"
#include "core/lexer.h"

#include <string.h>

void setUp(void)
{
    logo_mem_init();
}

void tearDown(void)
{
    // Nothing to tear down
}

// Helper to assert token type and text
static void assert_token(Token token, TokenType expected_type, const char *expected_text)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected_type, token.type, "Token type mismatch");
    if (expected_text)
    {
        char text[256];
        size_t len = token.length < sizeof(text) - 1 ? token.length : sizeof(text) - 1;
        if (token.start)
        {
            memcpy(text, token.start, len);
            text[len] = '\0';
        }
        else
        {
            text[0] = '\0';
        }
        TEST_ASSERT_EQUAL_STRING_MESSAGE(expected_text, text, "Token text mismatch");
    }
}

// Helper to assert just token type
static void assert_token_type(Token token, TokenType expected_type)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected_type, token.type, "Token type mismatch");
}

//============================================================================
// Lexer-based TokenSource Tests
//============================================================================

void test_lexer_source_init(void)
{
    Lexer lexer;
    lexer_init(&lexer, "forward 100");
    
    TokenSource ts;
    token_source_init_lexer(&ts, &lexer);
    
    TEST_ASSERT_EQUAL(TOKEN_SOURCE_LEXER, ts.type);
    TEST_ASSERT_EQUAL_PTR(&lexer, ts.lexer);
    TEST_ASSERT_FALSE(ts.has_current);
}

void test_lexer_source_next(void)
{
    Lexer lexer;
    lexer_init(&lexer, "forward 100");
    
    TokenSource ts;
    token_source_init_lexer(&ts, &lexer);
    
    Token t1 = token_source_next(&ts);
    assert_token(t1, TOKEN_WORD, "forward");
    
    Token t2 = token_source_next(&ts);
    assert_token(t2, TOKEN_NUMBER, "100");
    
    Token t3 = token_source_next(&ts);
    assert_token_type(t3, TOKEN_EOF);
}

void test_lexer_source_peek(void)
{
    Lexer lexer;
    lexer_init(&lexer, "hello world");
    
    TokenSource ts;
    token_source_init_lexer(&ts, &lexer);
    
    // Peek should return next token without consuming
    Token t1 = token_source_peek(&ts);
    assert_token(t1, TOKEN_WORD, "hello");
    
    // Peek again should return same token
    Token t2 = token_source_peek(&ts);
    assert_token(t2, TOKEN_WORD, "hello");
    
    // Now consume with next
    Token t3 = token_source_next(&ts);
    assert_token(t3, TOKEN_WORD, "hello");
    
    // Next token should be world
    Token t4 = token_source_next(&ts);
    assert_token(t4, TOKEN_WORD, "world");
}

void test_lexer_source_at_end_empty(void)
{
    Lexer lexer;
    lexer_init(&lexer, "");
    
    TokenSource ts;
    token_source_init_lexer(&ts, &lexer);
    
    TEST_ASSERT_TRUE(token_source_at_end(&ts));
}

void test_lexer_source_at_end_with_content(void)
{
    Lexer lexer;
    lexer_init(&lexer, "test");
    
    TokenSource ts;
    token_source_init_lexer(&ts, &lexer);
    
    TEST_ASSERT_FALSE(token_source_at_end(&ts));
    
    token_source_next(&ts);  // Consume "test"
    
    TEST_ASSERT_TRUE(token_source_at_end(&ts));
}

void test_lexer_source_brackets(void)
{
    Lexer lexer;
    lexer_init(&lexer, "[a b c]");
    
    TokenSource ts;
    token_source_init_lexer(&ts, &lexer);
    
    assert_token_type(token_source_next(&ts), TOKEN_LEFT_BRACKET);
    assert_token(token_source_next(&ts), TOKEN_WORD, "a");
    assert_token(token_source_next(&ts), TOKEN_WORD, "b");
    assert_token(token_source_next(&ts), TOKEN_WORD, "c");
    assert_token_type(token_source_next(&ts), TOKEN_RIGHT_BRACKET);
    assert_token_type(token_source_next(&ts), TOKEN_EOF);
}

void test_lexer_source_operators(void)
{
    Lexer lexer;
    lexer_init(&lexer, "3 + 4 * 5");
    
    TokenSource ts;
    token_source_init_lexer(&ts, &lexer);
    
    assert_token(token_source_next(&ts), TOKEN_NUMBER, "3");
    assert_token_type(token_source_next(&ts), TOKEN_PLUS);
    assert_token(token_source_next(&ts), TOKEN_NUMBER, "4");
    assert_token_type(token_source_next(&ts), TOKEN_MULTIPLY);
    assert_token(token_source_next(&ts), TOKEN_NUMBER, "5");
}

void test_lexer_source_get_sublist_returns_nil(void)
{
    Lexer lexer;
    lexer_init(&lexer, "[a b]");
    
    TokenSource ts;
    token_source_init_lexer(&ts, &lexer);
    
    token_source_next(&ts);  // TOKEN_LEFT_BRACKET
    
    // Lexer-based source should return NIL for get_sublist
    Node sublist = token_source_get_sublist(&ts);
    TEST_ASSERT_TRUE(mem_is_nil(sublist));
}

//============================================================================
// Node Iterator-based TokenSource Tests
//============================================================================

void test_node_iter_source_init(void)
{
    // Create a simple list: [forward 100]
    Node word1 = mem_atom("forward", 7);
    Node word2 = mem_atom("100", 3);
    Node list = mem_cons(word1, mem_cons(word2, NODE_NIL));
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    TEST_ASSERT_EQUAL(TOKEN_SOURCE_NODE_ITERATOR, ts.type);
    TEST_ASSERT_FALSE(ts.has_current);
}

void test_node_iter_source_empty_list(void)
{
    TokenSource ts;
    token_source_init_list(&ts, NODE_NIL);
    
    TEST_ASSERT_TRUE(token_source_at_end(&ts));
    
    Token t = token_source_next(&ts);
    assert_token_type(t, TOKEN_EOF);
}

void test_node_iter_source_word(void)
{
    // Create list: [hello]
    Node word = mem_atom("hello", 5);
    Node list = mem_cons(word, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_WORD, "hello");
    
    assert_token_type(token_source_next(&ts), TOKEN_EOF);
}

void test_node_iter_source_multiple_words(void)
{
    // Create list: [forward right repeat]
    Node w1 = mem_atom("forward", 7);
    Node w2 = mem_atom("right", 5);
    Node w3 = mem_atom("repeat", 6);
    Node list = mem_cons(w1, mem_cons(w2, mem_cons(w3, NODE_NIL)));
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    assert_token(token_source_next(&ts), TOKEN_WORD, "forward");
    assert_token(token_source_next(&ts), TOKEN_WORD, "right");
    assert_token(token_source_next(&ts), TOKEN_WORD, "repeat");
    assert_token_type(token_source_next(&ts), TOKEN_EOF);
}

void test_node_iter_source_number(void)
{
    // Create list: [100]
    Node num = mem_atom("100", 3);
    Node list = mem_cons(num, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_NUMBER, "100");
}

void test_node_iter_source_decimal_number(void)
{
    // Create list: [3.14]
    Node num = mem_atom("3.14", 4);
    Node list = mem_cons(num, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_NUMBER, "3.14");
}

void test_node_iter_source_negative_number(void)
{
    // Create list: [-42]
    Node num = mem_atom("-42", 3);
    Node list = mem_cons(num, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    // At start of list (delimiter context), negative number should be NUMBER
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_NUMBER, "-42");
}

void test_node_iter_source_quoted_word(void)
{
    // Create list: ["hello] - quoted word stored with quote
    Node word = mem_atom("\"hello", 6);
    Node list = mem_cons(word, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_QUOTED, "\"hello");
}

void test_node_iter_source_variable(void)
{
    // Create list: [:var] - variable stored with colon
    Node word = mem_atom(":var", 4);
    Node list = mem_cons(word, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_COLON, ":var");
}

void test_node_iter_source_operators(void)
{
    // Create list: [+ - * / = < >]
    Node op1 = mem_atom("+", 1);
    Node op2 = mem_atom("-", 1);
    Node op3 = mem_atom("*", 1);
    Node op4 = mem_atom("/", 1);
    Node op5 = mem_atom("=", 1);
    Node op6 = mem_atom("<", 1);
    Node op7 = mem_atom(">", 1);
    Node list = mem_cons(op1, mem_cons(op2, mem_cons(op3, 
                mem_cons(op4, mem_cons(op5, mem_cons(op6, 
                mem_cons(op7, NODE_NIL)))))));
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    assert_token_type(token_source_next(&ts), TOKEN_PLUS);
    assert_token_type(token_source_next(&ts), TOKEN_UNARY_MINUS);  // After +, delimiter context
    assert_token_type(token_source_next(&ts), TOKEN_MULTIPLY);
    assert_token_type(token_source_next(&ts), TOKEN_DIVIDE);
    assert_token_type(token_source_next(&ts), TOKEN_EQUALS);
    assert_token_type(token_source_next(&ts), TOKEN_LESS_THAN);
    assert_token_type(token_source_next(&ts), TOKEN_GREATER_THAN);
}

void test_node_iter_source_minus_after_word(void)
{
    // After a word, minus should be binary
    // Create list: [x -] 
    Node w1 = mem_atom("x", 1);
    Node op = mem_atom("-", 1);
    Node list = mem_cons(w1, mem_cons(op, NODE_NIL));
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    token_source_next(&ts);  // x
    Token t = token_source_next(&ts);  // -
    assert_token_type(t, TOKEN_MINUS);  // Binary minus after word
}

void test_node_iter_source_minus_after_number(void)
{
    // After a number, minus should be binary
    // Create list: [5 -] 
    Node num = mem_atom("5", 1);
    Node op = mem_atom("-", 1);
    Node list = mem_cons(num, mem_cons(op, NODE_NIL));
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    token_source_next(&ts);  // 5
    Token t = token_source_next(&ts);  // -
    assert_token_type(t, TOKEN_MINUS);  // Binary minus after number
}

void test_node_iter_source_nested_list(void)
{
    // Create list: [[a b] c]
    // Inner list is [a b]
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node inner = mem_cons(a, mem_cons(b, NODE_NIL));
    
    // Outer list is [inner c]
    Node c = mem_atom("c", 1);
    Node outer = mem_cons(inner, mem_cons(c, NODE_NIL));
    
    TokenSource ts;
    token_source_init_list(&ts, outer);
    
    // First element is a list, should return TOKEN_LEFT_BRACKET
    Token t1 = token_source_next(&ts);
    assert_token_type(t1, TOKEN_LEFT_BRACKET);
    
    // Should be able to get the sublist
    Node sublist = token_source_get_sublist(&ts);
    TEST_ASSERT_FALSE(mem_is_nil(sublist));
    TEST_ASSERT_TRUE(mem_is_list(sublist));
    
    // Verify sublist is [a b]
    Node first = mem_car(sublist);
    TEST_ASSERT_TRUE(mem_is_word(first));
    TEST_ASSERT_EQUAL_STRING("a", mem_word_ptr(first));
    
    // Consume the sublist
    token_source_consume_sublist(&ts);
    
    // Next should be 'c'
    Token t2 = token_source_next(&ts);
    assert_token(t2, TOKEN_WORD, "c");
    
    // Then EOF
    assert_token_type(token_source_next(&ts), TOKEN_EOF);
}

void test_node_iter_source_empty_nested_list(void)
{
    // Create list: [[] x] - contains empty list
    Node inner = NODE_NIL;  // Empty list
    Node x = mem_atom("x", 1);
    Node outer = mem_cons(inner, mem_cons(x, NODE_NIL));
    
    TokenSource ts;
    token_source_init_list(&ts, outer);
    
    // First element is empty list
    Token t1 = token_source_next(&ts);
    assert_token_type(t1, TOKEN_LEFT_BRACKET);
    
    // Get the sublist (should be NIL)
    Node sublist = token_source_get_sublist(&ts);
    TEST_ASSERT_TRUE(mem_is_nil(sublist));
    
    // Consume it
    token_source_consume_sublist(&ts);
    
    // Next should be 'x'
    assert_token(token_source_next(&ts), TOKEN_WORD, "x");
}

void test_node_iter_source_peek(void)
{
    // Create list: [a b]
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node list = mem_cons(a, mem_cons(b, NODE_NIL));
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    // Peek should return 'a'
    Token t1 = token_source_peek(&ts);
    assert_token(t1, TOKEN_WORD, "a");
    
    // Peek again should still be 'a'
    Token t2 = token_source_peek(&ts);
    assert_token(t2, TOKEN_WORD, "a");
    
    // Consume with next
    Token t3 = token_source_next(&ts);
    assert_token(t3, TOKEN_WORD, "a");
    
    // Now should be 'b'
    assert_token(token_source_next(&ts), TOKEN_WORD, "b");
}

//============================================================================
// Copy State Tests
//============================================================================

void test_copy_lexer_source(void)
{
    Lexer lexer;
    lexer_init(&lexer, "a b c");
    
    TokenSource ts;
    token_source_init_lexer(&ts, &lexer);
    
    token_source_next(&ts);  // Consume 'a'
    
    // Copy the state
    TokenSource copy;
    token_source_copy(&copy, &ts);
    
    // Continue with original
    Token t1 = token_source_next(&ts);
    assert_token(t1, TOKEN_WORD, "b");
    
    // Copy should also be at 'b' (shallow copy shares lexer)
    Token t2 = token_source_next(&copy);
    // Note: lexer state is shared, so this advances the same lexer
    assert_token(t2, TOKEN_WORD, "c");
}

void test_copy_node_iter_source(void)
{
    // Create list: [a b c]
    Node a = mem_atom("a", 1);
    Node b = mem_atom("b", 1);
    Node c = mem_atom("c", 1);
    Node list = mem_cons(a, mem_cons(b, mem_cons(c, NODE_NIL)));
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    token_source_next(&ts);  // Consume 'a'
    
    // Copy the state
    TokenSource copy;
    token_source_copy(&copy, &ts);
    
    // Continue with original - should get 'b'
    Token t1 = token_source_next(&ts);
    assert_token(t1, TOKEN_WORD, "b");
    
    // Copy is independent - should also be at 'b'
    Token t2 = token_source_next(&copy);
    assert_token(t2, TOKEN_WORD, "b");
    
    // Original continues to 'c'
    assert_token(token_source_next(&ts), TOKEN_WORD, "c");
    
    // Copy also continues independently to 'c'
    assert_token(token_source_next(&copy), TOKEN_WORD, "c");
}

//============================================================================
// Word Classification Tests
//============================================================================

void test_classify_exponent_e(void)
{
    // Create list: [1e4]
    Node num = mem_atom("1e4", 3);
    Node list = mem_cons(num, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_NUMBER, "1e4");
}

void test_classify_exponent_E(void)
{
    // Create list: [1E4]
    Node num = mem_atom("1E4", 3);
    Node list = mem_cons(num, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_NUMBER, "1E4");
}

void test_classify_exponent_n(void)
{
    // Create list: [1n4] (Logo notation for 1e-4)
    Node num = mem_atom("1n4", 3);
    Node list = mem_cons(num, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_NUMBER, "1n4");
}

void test_classify_exponent_with_sign(void)
{
    // Create list: [1e+4]
    Node num = mem_atom("1e+4", 4);
    Node list = mem_cons(num, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_NUMBER, "1e+4");
}

void test_classify_positive_number(void)
{
    // Create list: [+42]
    Node num = mem_atom("+42", 3);
    Node list = mem_cons(num, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_NUMBER, "+42");
}

void test_classify_decimal_only(void)
{
    // Create list: [.5]
    Node num = mem_atom(".5", 2);
    Node list = mem_cons(num, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_NUMBER, ".5");
}

void test_classify_empty_word(void)
{
    // Create list with empty word: [""]
    // Empty word stored as just the quote
    Node word = mem_atom("\"", 1);
    Node list = mem_cons(word, NODE_NIL);
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    Token t = token_source_next(&ts);
    assert_token(t, TOKEN_QUOTED, "\"");
}

void test_classify_bracket_chars(void)
{
    // Single bracket characters as words
    Node lb = mem_atom("[", 1);
    Node rb = mem_atom("]", 1);
    Node list = mem_cons(lb, mem_cons(rb, NODE_NIL));
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    assert_token_type(token_source_next(&ts), TOKEN_LEFT_BRACKET);
    assert_token_type(token_source_next(&ts), TOKEN_RIGHT_BRACKET);
}

void test_classify_paren_chars(void)
{
    // Single paren characters as words
    Node lp = mem_atom("(", 1);
    Node rp = mem_atom(")", 1);
    Node list = mem_cons(lp, mem_cons(rp, NODE_NIL));
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    assert_token_type(token_source_next(&ts), TOKEN_LEFT_PAREN);
    assert_token_type(token_source_next(&ts), TOKEN_RIGHT_PAREN);
}

//============================================================================
// Edge Cases
//============================================================================

void test_consume_sublist_on_lexer_source(void)
{
    // consume_sublist should be a no-op on lexer source
    Lexer lexer;
    lexer_init(&lexer, "[a b]");
    
    TokenSource ts;
    token_source_init_lexer(&ts, &lexer);
    
    token_source_next(&ts);  // [
    token_source_consume_sublist(&ts);  // Should do nothing
    
    // Should still be able to continue
    assert_token(token_source_next(&ts), TOKEN_WORD, "a");
}

void test_deeply_nested_lists(void)
{
    // Create: [[[x]]]
    Node x = mem_atom("x", 1);
    Node inner1 = mem_cons(x, NODE_NIL);        // [x]
    Node inner2 = mem_cons(inner1, NODE_NIL);   // [[x]]
    Node outer = mem_cons(inner2, NODE_NIL);    // [[[x]]]
    
    TokenSource ts;
    token_source_init_list(&ts, outer);
    
    // First: TOKEN_LEFT_BRACKET for inner2
    assert_token_type(token_source_next(&ts), TOKEN_LEFT_BRACKET);
    
    Node sub1 = token_source_get_sublist(&ts);
    TEST_ASSERT_TRUE(mem_is_list(sub1));
    
    // Create a new token source for the sublist
    TokenSource ts2;
    token_source_init_list(&ts2, sub1);
    
    // Should get TOKEN_LEFT_BRACKET for inner1
    assert_token_type(token_source_next(&ts2), TOKEN_LEFT_BRACKET);
    
    Node sub2 = token_source_get_sublist(&ts2);
    TEST_ASSERT_TRUE(mem_is_list(sub2));
    
    // Create another token source
    TokenSource ts3;
    token_source_init_list(&ts3, sub2);
    
    // Finally get 'x'
    assert_token(token_source_next(&ts3), TOKEN_WORD, "x");
}

void test_mixed_content_list(void)
{
    // Create: [forward 100 [fd 50] rt 90]
    Node fwd = mem_atom("forward", 7);
    Node n100 = mem_atom("100", 3);
    
    Node fd = mem_atom("fd", 2);
    Node n50 = mem_atom("50", 2);
    Node inner = mem_cons(fd, mem_cons(n50, NODE_NIL));  // [fd 50]
    
    Node rt = mem_atom("rt", 2);
    Node n90 = mem_atom("90", 2);
    
    Node list = mem_cons(fwd, mem_cons(n100, mem_cons(inner, 
                mem_cons(rt, mem_cons(n90, NODE_NIL)))));
    
    TokenSource ts;
    token_source_init_list(&ts, list);
    
    assert_token(token_source_next(&ts), TOKEN_WORD, "forward");
    assert_token(token_source_next(&ts), TOKEN_NUMBER, "100");
    
    // Nested list
    assert_token_type(token_source_next(&ts), TOKEN_LEFT_BRACKET);
    Node sub = token_source_get_sublist(&ts);
    TEST_ASSERT_TRUE(mem_is_list(sub));
    token_source_consume_sublist(&ts);
    
    assert_token(token_source_next(&ts), TOKEN_WORD, "rt");
    assert_token(token_source_next(&ts), TOKEN_NUMBER, "90");
    assert_token_type(token_source_next(&ts), TOKEN_EOF);
}

//============================================================================
// Main
//============================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Lexer-based TokenSource
    RUN_TEST(test_lexer_source_init);
    RUN_TEST(test_lexer_source_next);
    RUN_TEST(test_lexer_source_peek);
    RUN_TEST(test_lexer_source_at_end_empty);
    RUN_TEST(test_lexer_source_at_end_with_content);
    RUN_TEST(test_lexer_source_brackets);
    RUN_TEST(test_lexer_source_operators);
    RUN_TEST(test_lexer_source_get_sublist_returns_nil);
    
    // Node Iterator-based TokenSource
    RUN_TEST(test_node_iter_source_init);
    RUN_TEST(test_node_iter_source_empty_list);
    RUN_TEST(test_node_iter_source_word);
    RUN_TEST(test_node_iter_source_multiple_words);
    RUN_TEST(test_node_iter_source_number);
    RUN_TEST(test_node_iter_source_decimal_number);
    RUN_TEST(test_node_iter_source_negative_number);
    RUN_TEST(test_node_iter_source_quoted_word);
    RUN_TEST(test_node_iter_source_variable);
    RUN_TEST(test_node_iter_source_operators);
    RUN_TEST(test_node_iter_source_minus_after_word);
    RUN_TEST(test_node_iter_source_minus_after_number);
    RUN_TEST(test_node_iter_source_nested_list);
    RUN_TEST(test_node_iter_source_empty_nested_list);
    RUN_TEST(test_node_iter_source_peek);
    
    // Copy state
    RUN_TEST(test_copy_lexer_source);
    RUN_TEST(test_copy_node_iter_source);
    
    // Word classification
    RUN_TEST(test_classify_exponent_e);
    RUN_TEST(test_classify_exponent_E);
    RUN_TEST(test_classify_exponent_n);
    RUN_TEST(test_classify_exponent_with_sign);
    RUN_TEST(test_classify_positive_number);
    RUN_TEST(test_classify_decimal_only);
    RUN_TEST(test_classify_empty_word);
    RUN_TEST(test_classify_bracket_chars);
    RUN_TEST(test_classify_paren_chars);
    
    // Edge cases
    RUN_TEST(test_consume_sublist_on_lexer_source);
    RUN_TEST(test_deeply_nested_lists);
    RUN_TEST(test_mixed_content_list);
    
    return UNITY_END();
}
