//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Lexer for tokenizing Logo input.
//

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Token types for Pico Logo
    typedef enum
    {
        TOKEN_EOF = 0,      // End of input
        TOKEN_WORD,         // Unquoted word (procedure name, keyword)
        TOKEN_QUOTED,       // Quoted word (starts with ")
        TOKEN_NUMBER,       // Numeric literal (self-quoting)
        TOKEN_COLON,        // Variable reference (:var produces thing "var)
        TOKEN_LEFT_BRACKET, // [
        TOKEN_RIGHT_BRACKET,// ]
        TOKEN_LEFT_PAREN,   // (
        TOKEN_RIGHT_PAREN,  // )
        TOKEN_PLUS,         // +
        TOKEN_MINUS,        // - (binary infix)
        TOKEN_UNARY_MINUS,  // - (unary prefix)
        TOKEN_MULTIPLY,     // *
        TOKEN_DIVIDE,       // /
        TOKEN_EQUALS,       // =
        TOKEN_LESS_THAN,    // <
        TOKEN_GREATER_THAN, // >
        TOKEN_ERROR,        // Lexer error
    } TokenType;

    // A token produced by the lexer
    typedef struct
    {
        TokenType type;
        const char *start; // Pointer to start of token in source
        size_t length;     // Length of token text
    } Token;

    // Lexer state
    typedef struct
    {
        const char *source;  // Source input string
        const char *current; // Current position in source
        Token previous;      // Previous token (for context)
    } Lexer;

    // Initialize the lexer with source input
    void lexer_init(Lexer *lexer, const char *source);

    // Get the next token from the input
    // Tokens are produced on demand (pull-based)
    Token lexer_next_token(Lexer *lexer);

    // Peek at the next token without consuming it
    Token lexer_peek_token(Lexer *lexer);

    // Check if we've reached the end of input
    bool lexer_is_at_end(const Lexer *lexer);

    // Get a null-terminated copy of the token text
    // Caller is responsible for freeing the returned string
    char *lexer_token_text(const Token *token);

    // Get a string name for a token type (for debugging)
    const char *lexer_token_type_name(TokenType type);

#ifdef __cplusplus
}
#endif
