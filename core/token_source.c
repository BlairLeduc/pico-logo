//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "token_source.h"
#include <string.h>
#include <ctype.h>

// Check if a word string represents a number
static bool is_number_word(const char *str, size_t len)
{
    if (len == 0)
        return false;
    size_t i = 0;

    // Optional sign
    if (str[i] == '-' || str[i] == '+')
        i++;
    if (i >= len)
        return false;

    bool has_digit = false;
    
    // Integer part
    while (i < len && isdigit((unsigned char)str[i]))
    {
        has_digit = true;
        i++;
    }
    
    // Decimal part
    if (i < len && str[i] == '.')
    {
        i++;
        while (i < len && isdigit((unsigned char)str[i]))
        {
            has_digit = true;
            i++;
        }
    }
    
    // Exponent part (e/E for standard, n/N for negative exponent)
    if (i < len && (str[i] == 'e' || str[i] == 'E' || str[i] == 'n' || str[i] == 'N'))
    {
        i++;
        if (i < len && (str[i] == '-' || str[i] == '+'))
            i++;
        while (i < len && isdigit((unsigned char)str[i]))
            i++;
    }
    
    return has_digit && i == len;
}

// Classify a word from a Node list into a token type
// Words in lists are stored WITHOUT their prefix characters, so:
// - Quoted words: stored as "hello (with leading quote)
// - Variables: stored as :var (with leading colon)
// - Numbers: stored as 123 (just the number)
// - Procedure names: stored as forward (just the name)
static Token classify_word(const char *str, size_t len, bool prev_was_delimiter)
{
    Token t = {TOKEN_WORD, str, len};
    
    if (len == 0)
    {
        // Empty word - treated as quoted empty string
        t.type = TOKEN_QUOTED;
        return t;
    }
    
    char first = str[0];
    
    // Check for quoted word
    if (first == '"')
    {
        t.type = TOKEN_QUOTED;
        // Note: start includes the quote, evaluator will skip it
        return t;
    }
    
    // Check for variable reference
    if (first == ':')
    {
        t.type = TOKEN_COLON;
        return t;
    }
    
    // Check for operators (single character words)
    if (len == 1)
    {
        switch (first)
        {
        case '+':
            t.type = TOKEN_PLUS;
            return t;
        case '*':
            t.type = TOKEN_MULTIPLY;
            return t;
        case '/':
            t.type = TOKEN_DIVIDE;
            return t;
        case '=':
            t.type = TOKEN_EQUALS;
            return t;
        case '<':
            t.type = TOKEN_LESS_THAN;
            return t;
        case '>':
            t.type = TOKEN_GREATER_THAN;
            return t;
        case '[':
            t.type = TOKEN_LEFT_BRACKET;
            return t;
        case ']':
            t.type = TOKEN_RIGHT_BRACKET;
            return t;
        case '(':
            t.type = TOKEN_LEFT_PAREN;
            return t;
        case ')':
            t.type = TOKEN_RIGHT_PAREN;
            return t;
        case '-':
            // Minus is binary unless at start or after delimiter
            t.type = prev_was_delimiter ? TOKEN_UNARY_MINUS : TOKEN_MINUS;
            return t;
        }
    }
    
    // Check for number (possibly with leading minus)
    if (is_number_word(str, len))
    {
        t.type = TOKEN_NUMBER;
        return t;
    }
    
    // Check if minus followed by number (negative number)
    if (first == '-' && len > 1 && prev_was_delimiter)
    {
        if (is_number_word(str + 1, len - 1))
        {
            t.type = TOKEN_NUMBER;
            return t;
        }
        // Otherwise treat as unary minus followed by word
        // But we can only return one token, so return unary minus
        // Actually, in a list the whole "-foo" is stored as one word
        // This shouldn't happen in practice for well-formed lists
        t.type = TOKEN_UNARY_MINUS;
        t.length = 1;
        return t;
    }
    
    // Default: it's a word (procedure name or keyword)
    t.type = TOKEN_WORD;
    return t;
}

// Check if a token type is a delimiter for unary minus detection
static bool is_delimiter_token(TokenType type)
{
    switch (type)
    {
    case TOKEN_EOF:
    case TOKEN_LEFT_BRACKET:
    case TOKEN_LEFT_PAREN:
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_UNARY_MINUS:
    case TOKEN_MULTIPLY:
    case TOKEN_DIVIDE:
    case TOKEN_EQUALS:
    case TOKEN_LESS_THAN:
    case TOKEN_GREATER_THAN:
        return true;
    default:
        return false;
    }
}

// Initialize token source from a Lexer
void token_source_init_lexer(TokenSource *ts, Lexer *lexer)
{
    ts->type = TOKEN_SOURCE_LEXER;
    ts->lexer = lexer;
    ts->has_current = false;
}

// Initialize token source from a Node list
void token_source_init_list(TokenSource *ts, Node list)
{
    ts->type = TOKEN_SOURCE_NODE_ITERATOR;
    ts->node_iter.current = list;
    ts->node_iter.pending_sublist = NODE_NIL;
    ts->node_iter.has_peeked = false;
    ts->node_iter.previous_was_delimiter = true;  // Start of list acts like delimiter
    ts->has_current = false;
}

// Get next token from node iterator
static Token node_iter_next(NodeIterator *iter)
{
    // If we have a peeked token, return it
    if (iter->has_peeked)
    {
        iter->has_peeked = false;
        Token t = iter->peeked_token;
        iter->previous_was_delimiter = is_delimiter_token(t.type);
        return t;
    }
    
    // Check for end of list
    if (mem_is_nil(iter->current))
    {
        return (Token){TOKEN_EOF, NULL, 0};
    }
    
    Node element = mem_car(iter->current);
    iter->current = mem_cdr(iter->current);
    
    // Handle word elements
    if (mem_is_word(element))
    {
        const char *str = mem_word_ptr(element);
        size_t len = mem_word_len(element);
        Token t = classify_word(str, len, iter->previous_was_delimiter);
        iter->previous_was_delimiter = is_delimiter_token(t.type);
        return t;
    }
    
    // Handle list elements - these appear as sublists [...]
    // The evaluator will handle these specially
    if (mem_is_list(element) || mem_is_nil(element))
    {
        // Store the sublist for the evaluator to retrieve via token_source_get_sublist()
        // Return TOKEN_LEFT_BRACKET to signal a list literal
        iter->pending_sublist = element;
        iter->previous_was_delimiter = true;
        return (Token){TOKEN_LEFT_BRACKET, NULL, 0};
    }
    
    // Shouldn't reach here
    return (Token){TOKEN_EOF, NULL, 0};
}

// Get next token
Token token_source_next(TokenSource *ts)
{
    // If we have a cached token, consume and return it
    if (ts->has_current)
    {
        ts->has_current = false;
        return ts->current;
    }
    
    if (ts->type == TOKEN_SOURCE_LEXER)
    {
        return lexer_next_token(ts->lexer);
    }
    else
    {
        return node_iter_next(&ts->node_iter);
    }
}

// Peek at next token
Token token_source_peek(TokenSource *ts)
{
    if (!ts->has_current)
    {
        ts->current = token_source_next(ts);
        ts->has_current = true;
    }
    return ts->current;
}

// Check if at end
bool token_source_at_end(TokenSource *ts)
{
    Token t = token_source_peek(ts);
    return t.type == TOKEN_EOF;
}

// Copy state for lookahead
void token_source_copy(TokenSource *dest, const TokenSource *src)
{
    *dest = *src;
    // For lexer type, we just copy the pointer (shallow copy)
    // For node iterator, the struct is copied by value
}

// Get pending sublist (after TOKEN_LEFT_BRACKET from NodeIterator)
Node token_source_get_sublist(TokenSource *ts)
{
    if (ts->type == TOKEN_SOURCE_NODE_ITERATOR)
    {
        return ts->node_iter.pending_sublist;
    }
    return NODE_NIL;
}

// Consume the pending sublist
void token_source_consume_sublist(TokenSource *ts)
{
    if (ts->type == TOKEN_SOURCE_NODE_ITERATOR)
    {
        ts->node_iter.pending_sublist = NODE_NIL;
        // Also clear the cached current token since we've consumed the bracket
        ts->has_current = false;
    }
}

// Get current position for CPS continuation (NodeIterator only)
Node token_source_get_position(TokenSource *ts)
{
    if (ts->type == TOKEN_SOURCE_NODE_ITERATOR)
    {
        return ts->node_iter.current;
    }
    return NODE_NIL;
}

// Restore position from saved Node (for CPS continuation)
void token_source_set_position(TokenSource *ts, Node position)
{
    if (ts->type == TOKEN_SOURCE_NODE_ITERATOR)
    {
        ts->node_iter.current = position;
        ts->node_iter.pending_sublist = NODE_NIL;
        ts->node_iter.has_peeked = false;
        ts->node_iter.previous_was_delimiter = true;
        ts->has_current = false;
    }
}
