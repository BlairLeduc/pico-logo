//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Token source abstraction for the evaluator.
//
//  This provides a common interface for producing tokens from different sources:
//  - Lexer: tokenizes character input (REPL, file loading)
//  - NodeIterator: tokenizes Node lists directly (list evaluation)
//
//  The evaluator works with TokenSource, allowing the same evaluation code
//  to work with both text input and stored procedure bodies without
//  serializing lists back to strings.
//

#pragma once

#include "lexer.h"
#include "memory.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Token source type
    typedef enum
    {
        TOKEN_SOURCE_LEXER,         // From character stream (Lexer)
        TOKEN_SOURCE_NODE_ITERATOR  // From Node list
    } TokenSourceType;

    // Forward declaration
    typedef struct TokenSource TokenSource;

    // Node iterator state for traversing a Logo list as tokens
    typedef struct
    {
        Node current;           // Current position in list
        Node pending_sublist;   // Sublist element (when TOKEN_LEFT_BRACKET returned)
        bool has_peeked;        // True if we have a peeked token
        Token peeked_token;     // The peeked token
        bool previous_was_delimiter; // For unary minus detection
    } NodeIterator;

    // Unified token source - can be either a Lexer or NodeIterator
    struct TokenSource
    {
        TokenSourceType type;
        union
        {
            Lexer *lexer;
            NodeIterator node_iter;
        };
        Token current;      // Current token (for peek)
        bool has_current;   // True if current is valid
    };

    // Initialize a token source from a Lexer
    void token_source_init_lexer(TokenSource *ts, Lexer *lexer);

    // Initialize a token source from a Node list
    void token_source_init_list(TokenSource *ts, Node list);

    // Get the next token (advances position)
    Token token_source_next(TokenSource *ts);

    // Peek at the next token without consuming it
    Token token_source_peek(TokenSource *ts);

    // Check if at end of input
    bool token_source_at_end(TokenSource *ts);

    // Copy the current state for lookahead
    void token_source_copy(TokenSource *dest, const TokenSource *src);

    // For NodeIterator: get the pending sublist after TOKEN_LEFT_BRACKET
    // Returns NODE_NIL if not a node iterator or no pending sublist
    Node token_source_get_sublist(TokenSource *ts);

    // For NodeIterator: consume the pending sublist (call after get_sublist)
    void token_source_consume_sublist(TokenSource *ts);

    // For CPS continuation: get current position as a Node (for NodeIterator)
    // Returns NODE_NIL for Lexer sources (not supported for continuation)
    Node token_source_get_position(TokenSource *ts);

    // For CPS continuation: restore position from saved Node
    void token_source_set_position(TokenSource *ts, Node position);

#ifdef __cplusplus
}
#endif

