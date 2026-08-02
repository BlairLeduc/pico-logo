//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Lexer for tokenizing Logo input.
//

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/memory.h"  // Node, carried by tokens sourced from a list

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
        TOKEN_COMMENT,      // ; through end of line (when preserving comments)
        TOKEN_ERROR,        // Lexer error
    } TokenType;

    // A token produced by the lexer or by the node-list iterator.
    //
    // Size matters here out of proportion to the struct: a Token sits inside
    // every TokenSource, which sits inside every EvalOp, which lives in a
    // 768-deep static op stack -- so one word added here is 3 KB of bss on a
    // board that was over 95 % full. Adding `atom` was paid for by deleting
    // NodeIterator's unused second Token, not by narrowing the fields below;
    // narrowing was measured and cost 16 % of the interpreter's throughput.
    // Weigh both before growing this.
    typedef struct
    {
        const char *start; // Pointer to start of token in source
        // The interned word this token came from, or NODE_NIL when it came
        // from raw text. A token sourced from a list carries its atom so the
        // evaluator can reach the per-atom memo (core/atom_memo.h) instead of
        // re-deriving the name's binding, and can use the atom directly where
        // it would otherwise re-intern the same characters. Lexer tokens point
        // into the input line buffer, not at an atom, so they keep the string
        // path.
        Node atom;
        size_t length;     // Length of token text
        TokenType type;
    } Token;

    // Lexer state
    typedef struct
    {
        const char *source;  // Source input string
        const char *current; // Current position in source
        Token previous;      // Previous token (for context)
        bool had_whitespace; // Whitespace before current token
        bool had_newline;    // Newline in whitespace before current token
        int newline_count;   // Number of newlines in whitespace (for empty line detection)
        bool preserve_comments; // If true, return comments instead of discarding them
    } Lexer;

    // Initialize the lexer with source input
    void lexer_init(Lexer *lexer, const char *source);

    // Get the next token from the input
    // Tokens are produced on demand (pull-based)
    Token lexer_next_token(Lexer *lexer);

    // Control whether semicolon comments are returned as TOKEN_COMMENT.
    // The default is false, so comments are ignored during normal evaluation.
    void lexer_set_preserve_comments(Lexer *lexer, bool preserve);

    // Peek at the next token without consuming it
    Token lexer_peek_token(Lexer *lexer);

    // Check if we've reached the end of input
    bool lexer_is_at_end(const Lexer *lexer);

    // Copy token text to a caller-provided buffer.
    // Returns the number of characters copied (excluding null terminator).
    // If buffer is NULL or buffer_size is 0, returns the required size.
    size_t lexer_token_text(const Token *token, char *buffer, size_t buffer_size);

    // Get a string name for a token type (for debugging)
    const char *lexer_token_type_name(TokenType type);

#ifdef __cplusplus
}
#endif
