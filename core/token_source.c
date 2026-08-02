//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
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
    
    // Exponent part (e/E for standard, n/N for negative exponent).
    // Must mirror the lexer's `is_valid_number` rules so the same word is
    // classified the same whether it comes from raw source or from a list:
    //   - exponent only allowed when the mantissa contained a digit
    //   - n/N may not be followed by a sign (the n itself implies negative)
    //   - at least one digit is required after the exponent
    if (i < len && (str[i] == 'e' || str[i] == 'E' || str[i] == 'n' || str[i] == 'N'))
    {
        if (!has_digit)
            return false;

        bool is_n_notation = (str[i] == 'n' || str[i] == 'N');
        i++;
        if (!is_n_notation && i < len && (str[i] == '-' || str[i] == '+'))
            i++;

        // Require at least one digit after the exponent marker.
        if (i >= len || !isdigit((unsigned char)str[i]))
            return false;

        while (i < len && isdigit((unsigned char)str[i]))
            i++;
    }

    return has_digit && i == len;
}

// Word class cached on the interned atom (P10 M1; design section 4).
// A word's characters never change, so its class is a pure function of the
// atom and is derived once instead of on every evaluation. The byte lives in
// the atom's memo slot, where 0 means "not computed yet".
#define ATOM_CLASS_NONE    0u  // nothing cached yet
#define ATOM_CLASS_CONTEXT 1u  // leading '-': class depends on the previous token
#define ATOM_CLASS_COMMENT 2u  // leading ';': starts a comment, never a token
#define ATOM_CLASS_BASE    3u  // classes from here up are ATOM_CLASS_BASE + TokenType
#define ATOM_CLASS_OF(tok) ((uint8_t)(ATOM_CLASS_BASE + (tok)))

// Derive the class of a word from its characters.
// Words in lists keep their prefix characters, so:
// - Quoted words: stored as "hello (with leading quote)
// - Variables: stored as :var (with leading colon)
// - Numbers: stored as 123 (just the number)
// - Procedure names: stored as forward (just the name)
static uint8_t compute_word_class(const char *str, size_t len)
{
    if (len == 0)
    {
        // Empty word - treated as quoted empty string
        return ATOM_CLASS_OF(TOKEN_QUOTED);
    }

    char first = str[0];

    if (first == ';')
        return ATOM_CLASS_COMMENT;

    // Quoted word: start includes the quote, evaluator will skip it
    if (first == '"')
        return ATOM_CLASS_OF(TOKEN_QUOTED);

    // Variable reference
    if (first == ':')
        return ATOM_CLASS_OF(TOKEN_COLON);

    // The one context-dependent shape. A signed number such as -5 is NOT one
    // of them: is_number_word accepts the leading sign, so it is a number in
    // every position. Everything else starting with '-' is unary or binary
    // depending on what came before, and is resolved at token time.
    if (first == '-')
    {
        return is_number_word(str, len)
            ? ATOM_CLASS_OF(TOKEN_NUMBER) : ATOM_CLASS_CONTEXT;
    }

    // Operators (single character words)
    if (len == 1)
    {
        switch (first)
        {
        case '+':
            return ATOM_CLASS_OF(TOKEN_PLUS);
        case '*':
            return ATOM_CLASS_OF(TOKEN_MULTIPLY);
        case '/':
            return ATOM_CLASS_OF(TOKEN_DIVIDE);
        case '=':
            return ATOM_CLASS_OF(TOKEN_EQUALS);
        case '<':
            return ATOM_CLASS_OF(TOKEN_LESS_THAN);
        case '>':
            return ATOM_CLASS_OF(TOKEN_GREATER_THAN);
        case '[':
            return ATOM_CLASS_OF(TOKEN_LEFT_BRACKET);
        case ']':
            return ATOM_CLASS_OF(TOKEN_RIGHT_BRACKET);
        case '(':
            return ATOM_CLASS_OF(TOKEN_LEFT_PAREN);
        case ')':
            return ATOM_CLASS_OF(TOKEN_RIGHT_PAREN);
        }
    }

    if (is_number_word(str, len))
        return ATOM_CLASS_OF(TOKEN_NUMBER);

    // Default: it's a word (procedure name or keyword)
    return ATOM_CLASS_OF(TOKEN_WORD);
}

// Read a word element in one entry walk: its characters, its length, and its
// class, the last computed once per atom and memoised on it. A word with no
// memo slot (a blob) or an unreadable node is classified afresh every time;
// an unreadable one reads as the empty word, as it did before the memo.
static uint8_t word_view(Node element, const char **str, size_t *len)
{
    uint8_t *memo = NULL;
    *str = NULL;
    *len = 0;
    mem_word_view(element, str, len, &memo);

    if (memo && *memo != ATOM_CLASS_NONE)
        return *memo;

    uint8_t cls = compute_word_class(*str, *len);
    if (memo)
        *memo = cls;
    return cls;
}

// Turn a cached class into the token for this position in the list.
static Token token_from_class(uint8_t cls, const char *str, size_t len,
                              bool prev_was_delimiter)
{
    Token t = {TOKEN_WORD, str, len};

    if (cls != ATOM_CLASS_CONTEXT)
    {
        t.type = (TokenType)(cls - ATOM_CLASS_BASE);
        return t;
    }

    // Minus is binary unless at the start or after a delimiter.
    if (len == 1)
    {
        t.type = prev_was_delimiter ? TOKEN_UNARY_MINUS : TOKEN_MINUS;
        return t;
    }

    if (prev_was_delimiter)
    {
        // Minus followed by a number (the word itself is not one, or it would
        // have classified as TOKEN_NUMBER above).
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
    ts->node_iter.has_pending_sublist = false;
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
    
    // Skip newline markers - they are for formatting only - and comment runs,
    // then take the element that follows. A word is looked up once here and
    // the result carries through to the token below.
    Node element = NODE_NIL;
    const char *str = NULL;
    size_t len = 0;
    uint8_t cls = ATOM_CLASS_NONE;

    for (;;)
    {
        // Check for end of list
        if (mem_is_nil(iter->current))
        {
            return (Token){TOKEN_EOF, NULL, 0};
        }

        element = mem_car(iter->current);

        if (mem_is_newline(element))
        {
            iter->current = mem_cdr(iter->current);
            continue;
        }
        if (!mem_is_word(element))
        {
            break;
        }

        cls = word_view(element, &str, &len);
        if (cls != ATOM_CLASS_COMMENT)
        {
            break;
        }

        // Drop the comment and the rest of its line.
        iter->current = mem_cdr(iter->current);
        while (!mem_is_nil(iter->current))
        {
            Node skipped = mem_car(iter->current);
            iter->current = mem_cdr(iter->current);
            if (mem_is_newline(skipped))
            {
                break;
            }
        }
    }

    iter->current = mem_cdr(iter->current);

    // Handle word elements
    if (mem_is_word(element))
    {
        Token t = token_from_class(cls, str, len, iter->previous_was_delimiter);
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
        iter->has_pending_sublist = true;
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

// Check if a sublist is pending (including empty lists)
bool token_source_has_sublist(TokenSource *ts)
{
    if (ts->type == TOKEN_SOURCE_NODE_ITERATOR)
    {
        return ts->node_iter.has_pending_sublist;
    }
    return false;
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
        ts->node_iter.has_pending_sublist = false;
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

// Mark the list position this source will resume from (GC root support).
// Lexer sources read raw text and hold no nodes.
void token_source_gc_mark(const TokenSource *ts)
{
    if (ts->type == TOKEN_SOURCE_NODE_ITERATOR)
    {
        mem_gc_mark(ts->node_iter.current);
        if (ts->node_iter.has_pending_sublist)
        {
            mem_gc_mark(ts->node_iter.pending_sublist);
        }
    }
}

// Restore position from saved Node (for CPS continuation)
void token_source_set_position(TokenSource *ts, Node position)
{
    if (ts->type == TOKEN_SOURCE_NODE_ITERATOR)
    {
        ts->node_iter.current = position;
        ts->node_iter.pending_sublist = NODE_NIL;
        ts->node_iter.has_pending_sublist = false;
        ts->node_iter.has_peeked = false;
        ts->node_iter.previous_was_delimiter = true;
        ts->has_current = false;
    }
}
