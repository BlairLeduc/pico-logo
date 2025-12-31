//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Lexer implementation for tokenizing Logo input.
//

#include "core/lexer.h"

#include <ctype.h>
#include <string.h>

// Delimiter characters that separate tokens in code mode
// [ ] ( ) + - * / = < >
static bool is_code_delimiter(char c)
{
    return c == '[' || c == ']' || c == '(' || c == ')' ||
           c == '+' || c == '-' || c == '*' || c == '/' ||
           c == '=' || c == '<' || c == '>';
}

// In data mode, only brackets are delimiters (for list structure)
static bool is_data_delimiter(char c)
{
    return c == '[' || c == ']';
}

// Check if character is a delimiter based on lexer mode
static bool is_delimiter(const Lexer *lexer, char c)
{
    if (lexer->data_mode)
    {
        return is_data_delimiter(c);
    }
    return is_code_delimiter(c);
}

// Check if character is whitespace (space or tab)
static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Check if character can start a number
static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

// Check if this looks like a number (including negative numbers, decimals, E/N notation)
static bool is_number_char(char c)
{
    return is_digit(c) || c == '.' || c == 'e' || c == 'E' || c == 'n' || c == 'N';
}

// Check if a word is a valid number
static bool is_valid_number(const char *start, size_t length)
{
    if (length == 0)
        return false;

    const char *p = start;
    const char *end = start + length;

    // Optional leading minus
    if (*p == '-')
    {
        p++;
        if (p == end)
            return false; // Just a minus sign
    }

    bool has_digit = false;
    bool has_dot = false;
    bool has_exp = false;

    while (p < end)
    {
        if (is_digit(*p))
        {
            has_digit = true;
        }
        else if (*p == '.')
        {
            if (has_dot || has_exp)
                return false; // Multiple dots or dot after exponent
            has_dot = true;
        }
        else if (*p == 'e' || *p == 'E' || *p == 'n' || *p == 'N')
        {
            if (has_exp || !has_digit)
                return false; // Multiple exponents or exponent without digits
            has_exp = true;
            // Exponent can be followed by optional sign and digits
            p++;
            if (p < end && (*p == '+' || *p == '-'))
            {
                p++;
            }
            // Must have at least one digit after exponent
            if (p >= end || !is_digit(*p))
                return false;
            while (p < end && is_digit(*p))
            {
                p++;
            }
            return p == end;
        }
        else
        {
            return false; // Invalid character
        }
        p++;
    }

    return has_digit;
}

void lexer_init(Lexer *lexer, const char *source)
{
    lexer->source = source;
    lexer->current = source;
    lexer->previous.type = TOKEN_EOF;
    lexer->previous.start = NULL;
    lexer->previous.length = 0;
    lexer->had_whitespace = true; // Start of input acts like whitespace
    lexer->data_mode = false;     // Code mode by default
}

void lexer_init_data(Lexer *lexer, const char *source)
{
    lexer_init(lexer, source);
    lexer->data_mode = true;      // Data mode - only brackets/whitespace delimit
}

// Skip whitespace and track if any was found
static void skip_whitespace(Lexer *lexer)
{
    lexer->had_whitespace = false;
    while (*lexer->current && is_space(*lexer->current))
    {
        lexer->had_whitespace = true;
        lexer->current++;
    }
}

// Create a token
static Token make_token(Lexer *lexer, TokenType type, const char *start, size_t length)
{
    Token token;
    token.type = type;
    token.start = start;
    token.length = length;
    lexer->previous = token;
    return token;
}

// Create an error token
static Token make_error_token(Lexer *lexer, const char *start, size_t length)
{
    return make_token(lexer, TOKEN_ERROR, start, length);
}

// Read a word (alphanumeric sequence, possibly with escaped characters)
static Token read_word(Lexer *lexer)
{
    const char *start = lexer->current;

    while (*lexer->current)
    {
        if (*lexer->current == '\\')
        {
            // Escape: skip the backslash and the next character
            lexer->current++;
            if (*lexer->current)
            {
                lexer->current++;
            }
        }
        else if (is_space(*lexer->current) || is_delimiter(lexer, *lexer->current))
        {
            break;
        }
        else
        {
            lexer->current++;
        }
    }

    size_t length = lexer->current - start;

    // Check if this word is a number (self-quoting)
    if (is_valid_number(start, length))
    {
        return make_token(lexer, TOKEN_NUMBER, start, length);
    }

    return make_token(lexer, TOKEN_WORD, start, length);
}

// Read a quoted word (starts with ")
static Token read_quoted(Lexer *lexer)
{
    // Skip the quote character
    const char *start = lexer->current;
    lexer->current++;

    // The first character after quote doesn't need backslash for special chars
    // (except for brackets which always need backslash)
    bool first_char = true;

    while (*lexer->current)
    {
        if (*lexer->current == '\\')
        {
            // Escape: skip backslash and next character
            lexer->current++;
            if (*lexer->current)
            {
                lexer->current++;
            }
            first_char = false;
        }
        else if (*lexer->current == '[' || *lexer->current == ']')
        {
            // Brackets always end a quoted word (unless escaped)
            break;
        }
        else if (is_space(*lexer->current))
        {
            // Space ends the quoted word
            break;
        }
        else if (!first_char && is_delimiter(lexer, *lexer->current))
        {
            // Non-first delimiter ends the word
            break;
        }
        else
        {
            lexer->current++;
            first_char = false;
        }
    }

    size_t length = lexer->current - start;
    return make_token(lexer, TOKEN_QUOTED, start, length);
}

// Read a variable reference (starts with :)
static Token read_colon(Lexer *lexer)
{
    const char *start = lexer->current;
    lexer->current++; // Skip the colon

    // Read the variable name
    while (*lexer->current)
    {
        if (*lexer->current == '\\')
        {
            lexer->current++;
            if (*lexer->current)
            {
                lexer->current++;
            }
        }
        else if (is_space(*lexer->current) || is_delimiter(lexer, *lexer->current))
        {
            break;
        }
        else
        {
            lexer->current++;
        }
    }

    size_t length = lexer->current - start;
    return make_token(lexer, TOKEN_COLON, start, length);
}

// Read a number (including negative numbers)
static Token read_number(Lexer *lexer)
{
    const char *start = lexer->current;

    // Optional leading minus (already validated by caller)
    if (*lexer->current == '-')
    {
        lexer->current++;
    }

    // Read digits and number characters
    while (*lexer->current && (is_number_char(*lexer->current) || is_digit(*lexer->current)))
    {
        // Handle exponent sign
        if ((*lexer->current == 'e' || *lexer->current == 'E' ||
             *lexer->current == 'n' || *lexer->current == 'N'))
        {
            lexer->current++;
            if (*lexer->current == '+' || *lexer->current == '-')
            {
                lexer->current++;
            }
        }
        else
        {
            lexer->current++;
        }
    }

    size_t length = lexer->current - start;
    return make_token(lexer, TOKEN_NUMBER, start, length);
}

// Check if the next characters form a pure number (no escapes or non-number chars following)
static bool looks_like_number(const char *p)
{
    // Skip leading minus
    if (*p == '-')
    {
        p++;
    }
    
    if (!is_digit(*p))
    {
        return false;
    }
    
    // Scan ahead to see if this is a pure number
    while (*p)
    {
        if (*p == '\\')
        {
            // Escape sequence - not a pure number
            return false;
        }
        if (is_space(*p) || is_code_delimiter(*p))
        {
            // Hit a delimiter - it's a number
            return true;
        }
        if (!is_digit(*p) && *p != '.' && 
            *p != 'e' && *p != 'E' && *p != 'n' && *p != 'N' &&
            *p != '+' && *p != '-')
        {
            // Non-number character without being a delimiter - it's a word
            return false;
        }
        // Handle exponent
        if (*p == 'e' || *p == 'E' || *p == 'n' || *p == 'N')
        {
            p++;
            if (*p == '+' || *p == '-')
            {
                p++;
            }
            continue;
        }
        p++;
    }
    return true;  // End of string - it's a number
}

// Determine if minus should be unary based on context
// According to the reference:
// - If "-" immediately precedes a word/variable/paren and follows delimiter except ), it's unary
// - If "-" immediately precedes a number and follows delimiter except ), it's a negative number
// - Space counts as a delimiter
// Key: "immediately precedes" means no space between - and what follows
static bool should_be_unary_minus(const Lexer *lexer)
{
    TokenType prev = lexer->previous.type;
    char after = lexer->current[1];
    
    // Check: does minus immediately precede something (no space after)?
    bool immediately_precedes = !is_space(after) && after != '\0';
    
    // If there was whitespace before the minus, it follows a delimiter
    if (lexer->had_whitespace)
    {
        // After ), even with whitespace, it's still binary minus
        if (prev == TOKEN_RIGHT_PAREN)
            return false;
        // Whitespace before AND immediately precedes something = unary/negative
        // e.g., "print 3 * -4" or "[-1 -2 -3]"
        if (immediately_precedes)
            return true;
        // Whitespace both before and after = binary
        // e.g., "7 - 3"
        return false;
    }
    
    // No whitespace before: binary minus after ), NUMBER, or QUOTED
    if (prev == TOKEN_RIGHT_PAREN || prev == TOKEN_NUMBER || prev == TOKEN_QUOTED)
        return false;
    
    // No whitespace before: After WORD or COLON (value-producing tokens)
    // e.g., "xcor-ycor" parses as xcor minus ycor (binary)
    if (prev == TOKEN_WORD || prev == TOKEN_COLON)
        return false;
    
    // For operators, opening brackets, and start of input: unary minus
    return prev == TOKEN_EOF ||
           prev == TOKEN_LEFT_BRACKET ||
           prev == TOKEN_LEFT_PAREN ||
           prev == TOKEN_PLUS ||
           prev == TOKEN_MINUS ||
           prev == TOKEN_UNARY_MINUS ||
           prev == TOKEN_MULTIPLY ||
           prev == TOKEN_DIVIDE ||
           prev == TOKEN_EQUALS ||
           prev == TOKEN_LESS_THAN ||
           prev == TOKEN_GREATER_THAN;
}

Token lexer_next_token(Lexer *lexer)
{
    skip_whitespace(lexer);

    if (*lexer->current == '\0')
    {
        return make_token(lexer, TOKEN_EOF, lexer->current, 0);
    }

    char c = *lexer->current;
    const char *start = lexer->current;

    // Brackets are always delimiters in both modes
    switch (c)
    {
    case '[':
        lexer->current++;
        return make_token(lexer, TOKEN_LEFT_BRACKET, start, 1);

    case ']':
        lexer->current++;
        return make_token(lexer, TOKEN_RIGHT_BRACKET, start, 1);

    case '"':
        return read_quoted(lexer);

    case ':':
        return read_colon(lexer);
    
    default:
        break;
    }

    // In data mode, everything else (except brackets) is part of words
    if (lexer->data_mode)
    {
        return read_word(lexer);
    }

    // Code mode: operators and parens are separate tokens
    switch (c)
    {
    case '(':
        lexer->current++;
        return make_token(lexer, TOKEN_LEFT_PAREN, start, 1);

    case ')':
        lexer->current++;
        return make_token(lexer, TOKEN_RIGHT_PAREN, start, 1);

    case '+':
        lexer->current++;
        return make_token(lexer, TOKEN_PLUS, start, 1);

    case '*':
        lexer->current++;
        return make_token(lexer, TOKEN_MULTIPLY, start, 1);

    case '/':
        lexer->current++;
        return make_token(lexer, TOKEN_DIVIDE, start, 1);

    case '=':
        lexer->current++;
        return make_token(lexer, TOKEN_EQUALS, start, 1);

    case '<':
        lexer->current++;
        return make_token(lexer, TOKEN_LESS_THAN, start, 1);

    case '>':
        lexer->current++;
        return make_token(lexer, TOKEN_GREATER_THAN, start, 1);

    case '-':
        // Minus is special - context-dependent
        if (should_be_unary_minus(lexer))
        {
            // Check if followed by a number (negative literal)
            if (is_digit(lexer->current[1]))
            {
                return read_number(lexer);
            }
            // Unary minus operator
            lexer->current++;
            return make_token(lexer, TOKEN_UNARY_MINUS, start, 1);
        }
        // Binary minus
        lexer->current++;
        return make_token(lexer, TOKEN_MINUS, start, 1);

    default:
        // Check for number - but only if it looks like a pure number
        if (is_digit(c) && looks_like_number(lexer->current))
        {
            return read_number(lexer);
        }
        // Otherwise it's a word
        return read_word(lexer);
    }
}

Token lexer_peek_token(Lexer *lexer)
{
    // Save state
    const char *saved_current = lexer->current;
    Token saved_previous = lexer->previous;

    // Get next token
    Token token = lexer_next_token(lexer);

    // Restore state
    lexer->current = saved_current;
    lexer->previous = saved_previous;

    return token;
}

bool lexer_is_at_end(const Lexer *lexer)
{
    // Skip whitespace to check for actual end
    const char *p = lexer->current;
    while (*p && is_space(*p))
    {
        p++;
    }
    return *p == '\0';
}

size_t lexer_token_text(const Token *token, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0)
    {
        return token->length;
    }

    size_t copy_len = token->length;
    if (copy_len >= buffer_size)
    {
        copy_len = buffer_size - 1;
    }

    if (copy_len > 0)
    {
        memcpy(buffer, token->start, copy_len);
    }
    buffer[copy_len] = '\0';

    return copy_len;
}

const char *lexer_token_type_name(TokenType type)
{
    switch (type)
    {
    case TOKEN_EOF:
        return "EOF";
    case TOKEN_WORD:
        return "WORD";
    case TOKEN_QUOTED:
        return "QUOTED";
    case TOKEN_NUMBER:
        return "NUMBER";
    case TOKEN_COLON:
        return "COLON";
    case TOKEN_LEFT_BRACKET:
        return "LEFT_BRACKET";
    case TOKEN_RIGHT_BRACKET:
        return "RIGHT_BRACKET";
    case TOKEN_LEFT_PAREN:
        return "LEFT_PAREN";
    case TOKEN_RIGHT_PAREN:
        return "RIGHT_PAREN";
    case TOKEN_PLUS:
        return "PLUS";
    case TOKEN_MINUS:
        return "MINUS";
    case TOKEN_UNARY_MINUS:
        return "UNARY_MINUS";
    case TOKEN_MULTIPLY:
        return "MULTIPLY";
    case TOKEN_DIVIDE:
        return "DIVIDE";
    case TOKEN_EQUALS:
        return "EQUALS";
    case TOKEN_LESS_THAN:
        return "LESS_THAN";
    case TOKEN_GREATER_THAN:
        return "GREATER_THAN";
    case TOKEN_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}
