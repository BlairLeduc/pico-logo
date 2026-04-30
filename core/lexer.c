//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Lexer implementation for tokenizing Logo input.
//

#include "core/lexer.h"

#include <ctype.h>
#include <string.h>

// Delimiter characters that separate tokens
// [ ] ( ) + - * / = < >
static bool is_delimiter(char c)
{
    return c == '[' || c == ']' || c == '(' || c == ')' ||
           c == '+' || c == '-' || c == '*' || c == '/' ||
           c == '=' || c == '<' || c == '>';
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
            bool is_n = (*p == 'n' || *p == 'N');
            // Exponent can be followed by optional sign and digits
            // but only e/E allows signs, not n/N
            p++;
            if (!is_n && p < end && (*p == '+' || *p == '-'))
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
    lexer->had_newline = true;    // Start of input acts like start of line
    lexer->newline_count = 1;     // Start of input counts as one newline
}

// Skip whitespace and line comments (everything after `;` to end of line),
// tracking whether any whitespace and how many newlines were consumed.
// Per the language reference ("; comment"), a semicolon outside a quoted
// word marks the rest of the line as a comment that the lexer must discard.
static void skip_whitespace(Lexer *lexer)
{
    lexer->had_whitespace = false;
    lexer->had_newline = false;
    lexer->newline_count = 0;
    for (;;)
    {
        // Eat ordinary whitespace.
        while (*lexer->current && is_space(*lexer->current))
        {
            if (*lexer->current == '\n')
            {
                lexer->had_newline = true;
                lexer->newline_count++;
            }
            lexer->had_whitespace = true;
            lexer->current++;
        }

        // A semicolon at this position starts a line comment. Skip every
        // character up to (but not including) the terminating newline so
        // the next loop iteration counts the newline normally.
        if (*lexer->current == ';')
        {
            lexer->had_whitespace = true;
            while (*lexer->current && *lexer->current != '\n')
            {
                lexer->current++;
            }
            continue;
        }

        break;
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
        else if (is_space(*lexer->current) || is_delimiter(*lexer->current) || *lexer->current == ';')
        {
            // `;` terminates an unquoted word so that `foo;comment` lexes
            // as `foo` followed by the comment (consumed in skip_whitespace).
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
//
// Reference (Pico_Logo_Reference.md §3873-3895, "Quotation Marks and
// Delimiters"):
//   "Normally, you have to put a backslash (\) before the characters
//    [, ], (, ), and \ itself. But the first character after a
//    quotation mark (") does not need to have a backslash preceding it."
//   "If a delimiter occupies any position but the first one after the
//    quotation mark, it must have a backslash preceding it."
//   "The only exception to the above general rule is brackets ([ ]).
//    If you want to put a quotation mark before a bracket, you must
//    always include a backslash between the quotation mark and the
//    bracket."
// Plus the file-path exception (§3807):
//   "You can use '/' in file names without escaping it, and Logo will
//    treat it as a normal character."
// So the rules implemented here are:
//   * brackets ([, ]) ALWAYS terminate a quoted word unless escaped,
//     even at the first position;
//   * any other delimiter at the first position is taken literally;
//   * any other delimiter at a subsequent position terminates the word,
//     EXCEPT '/' which is always literal (file-path exception);
//   * a backslash escapes the following character;
//   * any whitespace terminates the word.
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
        else if (!first_char && is_delimiter(*lexer->current) && *lexer->current != '/')
        {
            // Non-first delimiter ends the word (except / for file paths)
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
        else if (is_space(*lexer->current) || is_delimiter(*lexer->current) || *lexer->current == ';')
        {
            // `;` ends a variable reference (`:foo;cmt` -> :foo + comment).
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
        // Handle exponent marker and optional sign (signs only for e/E)
        if ((*lexer->current == 'e' || *lexer->current == 'E' ||
             *lexer->current == 'n' || *lexer->current == 'N'))
        {
            bool is_n_notation = (*lexer->current == 'n' || *lexer->current == 'N');
            lexer->current++;
            if (!is_n_notation && (*lexer->current == '+' || *lexer->current == '-'))
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
    
    bool has_dot = false;
    bool has_exp = false;

    // Scan ahead to see if this is a pure number
    while (*p)
    {
        if (*p == '\\')
        {
            // Escape sequence - not a pure number
            return false;
        }
        if (is_space(*p) || is_delimiter(*p))
        {
            // Hit a delimiter - it's a number
            return true;
        }
        if (*p == '.')
        {
            if (has_dot || has_exp)
                return false; // Multiple dots or dot after exponent
            has_dot = true;
            p++;
            continue;
        }
        // Handle exponent
        if (*p == 'e' || *p == 'E' || *p == 'n' || *p == 'N')
        {
            if (has_exp)
                return false; // Multiple exponents
            has_exp = true;
            bool is_n = (*p == 'n' || *p == 'N');
            p++;
            if (!is_n && (*p == '+' || *p == '-'))
            {
                p++;
            }
            // Must have at least one digit after exponent (and optional sign)
            if (!*p || !is_digit(*p))
            {
                return false;
            }
            continue;
        }
        if (!is_digit(*p))
        {
            // Non-number character without being a delimiter - it's a word
            return false;
        }
        p++;
    }
    return true;  // End of string - it's a number
}

// Determine whether a `-` token should be parsed as unary/negative
// (TOKEN_UNARY_MINUS or a negative TOKEN_NUMBER) versus binary
// subtraction (TOKEN_MINUS).
//
// The reference (`Pico Logo Reference`, "The Minus Sign") defines
// three rules. We quote them verbatim and then document the one
// place this implementation deviates:
//
//   1. If the "-" immediately precedes a number, and follows any
//      delimiter except right parenthesis ")", the number is parsed
//      as a negative number. Examples:
//        print sum 20-20    -> 20 minus 20
//        print 3*-4         -> 3 times -4
//        print (3+4)-5      -> 3 plus 4 minus 5
//        first [-3 4]       -> -3
//
//   2. If the "-" immediately precedes a word or left parenthesis "(",
//      and follows any delimiter except right parenthesis, it is the
//      unary-minus procedure. Examples:
//        setpos list :x -:y
//        setpos list ycor -xcor
//
//   3. In all other cases, "-" is binary subtraction. Examples:
//        print 3-4          -> binary
//        print 3 - 4        -> binary
//        print - 3 4        -> binary applied prefix
//
// "Immediately precedes" means no whitespace between `-` and what
// follows.
//
// CONVENTION (deliberate deviation from a strict literal reading of
// rules 1+2): the right-paren exception is CHARACTER-adjacent, not
// token-adjacent. That is, `)-X` triggers the exception (rule 3
// applies, binary), but `) -X` does NOT — once whitespace separates
// `)` from `-`, the space itself counts as the delimiter for rules
// 1/2 and the `-X` parses as unary/negative. So `(5+3) -2` tokenises
// as `( 5 + 3 ) -2`, not `( 5 + 3 ) - 2`. This matches established
// Logo convention; changing it would silently break existing user
// programs and other Logo implementations.
//
// The previous-token kinds we treat as VALUE-PRODUCING (i.e. they
// could be the left operand of a binary `-` when no whitespace
// separates them from the `-`):
//   TOKEN_NUMBER, TOKEN_WORD, TOKEN_COLON, TOKEN_QUOTED, TOKEN_RIGHT_PAREN
// Everything else is a delimiter (operators, brackets, EOF, etc.).
static bool should_be_unary_minus(const Lexer *lexer)
{
    const TokenType prev = lexer->previous.type;
    const char after = lexer->current[1];

    // "Immediately precedes" — true iff no whitespace (and not EOF)
    // separates `-` from the next token.
    const bool immediately_precedes = !is_space(after) && after != '\0';

    // Whitespace before `-` always counts as a delimiter (per the
    // CONVENTION above, this is true even when the previous TOKEN was
    // `)`). So once we have leading whitespace, only what immediately
    // follows decides:
    //   * something immediately follows -> unary/negative (rules 1/2)
    //   * whitespace also follows         -> binary (rule 3)
    if (lexer->had_whitespace)
    {
        return immediately_precedes;
    }

    // No whitespace before `-`. Now the previous token decides:
    //   * a value-producing token to the left -> binary (rule 3)
    //   * a delimiter token to the left      -> unary (rules 1/2)
    // `]` is treated like `)` for symmetry (a closer of a value).
    // `TOKEN_ERROR` keeps the previous behaviour of defaulting to
    // binary so a single bad token doesn't cascade into surprising
    // unary parses.
    switch (prev)
    {
    case TOKEN_NUMBER:
    case TOKEN_WORD:
    case TOKEN_COLON:
    case TOKEN_QUOTED:
    case TOKEN_RIGHT_PAREN:   // The literal right-paren exception
    case TOKEN_RIGHT_BRACKET:
    case TOKEN_ERROR:
        return false;          // -> binary
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
        return true;           // -> unary/negative (rules 1/2)
    }
    // Unreachable for known token kinds; keep prior default.
    return false;
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

    // Single-character tokens
    switch (c)
    {
    case '[':
        lexer->current++;
        return make_token(lexer, TOKEN_LEFT_BRACKET, start, 1);

    case ']':
        lexer->current++;
        return make_token(lexer, TOKEN_RIGHT_BRACKET, start, 1);

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

    case '"':
        return read_quoted(lexer);

    case ':':
        return read_colon(lexer);

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
