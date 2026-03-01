//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Syntax highlighting for the Logo editor.
//
//  Scans a single line of Logo source left-to-right, tagging each character
//  with a SyntaxCategory.  Bracket/paren nesting depth is carried across
//  lines via the initial_depth parameter, with a TO line resetting to 0.
//

#include "core/syntax_highlight.h"

#include <stdbool.h>
#include <ctype.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Is `c` a Logo delimiter (whitespace or structural character)?
static inline bool is_delimiter(char c)
{
    return c == ' '  || c == '\t' || c == '\n' || c == '\r' ||
           c == '['  || c == ']'  || c == '('  || c == ')' ||
           c == '+'  || c == '-'  || c == '*'  || c == '/' ||
           c == '='  || c == '<'  || c == '>'  || c == '\0';
}

// Case-insensitive character comparison
static inline bool ci_eq(char a, char b)
{
    return tolower((unsigned char)a) == tolower((unsigned char)b);
}

// Case-insensitive prefix match of `prefix` (NUL-terminated) against
// line[pos..pos+len).  Returns the length of the prefix if matched, else 0.
static int match_keyword(const char *line, int pos, int length,
                         const char *prefix)
{
    int i = 0;
    while (prefix[i] && pos + i < length) {
        if (!ci_eq(line[pos + i], prefix[i]))
            return 0;
        i++;
    }
    if (prefix[i] != '\0')
        return 0;  // prefix longer than remaining line
    return i;
}

// Map a 1-based bracket depth to a SYNTAX_BRACKET_* category.
static inline SyntaxCategory bracket_category(int depth)
{
    // depth 1,4,7,... -> BRACKET_1;  2,5,8,... -> BRACKET_2;  3,6,9,... -> BRACKET_3
    int idx = ((depth - 1) % 3);
    return (SyntaxCategory)(SYNTAX_BRACKET_1 + idx);
}

// ---------------------------------------------------------------------------
// Tag helpers — write a category for a range, respecting NULL categories
// ---------------------------------------------------------------------------

static inline void tag_range(uint8_t *categories, int from, int to,
                             SyntaxCategory cat)
{
    if (!categories) return;
    for (int i = from; i < to; i++)
        categories[i] = (uint8_t)cat;
}

static inline void tag_one(uint8_t *categories, int pos, SyntaxCategory cat)
{
    if (!categories) return;
    categories[pos] = (uint8_t)cat;
}

// ---------------------------------------------------------------------------
// Scan helpers
// ---------------------------------------------------------------------------

// Skip whitespace starting at *pos.  Tags skipped chars as SYNTAX_DEFAULT.
static void skip_ws(const char *line, int length,
                    uint8_t *categories, int *pos)
{
    while (*pos < length && (line[*pos] == ' ' || line[*pos] == '\t')) {
        tag_one(categories, *pos, SYNTAX_DEFAULT);
        (*pos)++;
    }
}

// Read a "word" — contiguous non-delimiter, non-bracket characters.
// Handles backslash escapes: \X skips the backslash and next character.
// Returns start position; *pos is advanced to one past the end.
static int read_word_span(const char *line, int length, int *pos)
{
    int start = *pos;
    while (*pos < length) {
        if (line[*pos] == '\\') {
            (*pos)++;  // skip backslash
            if (*pos < length)
                (*pos)++;  // skip escaped character
        } else if (is_delimiter(line[*pos])) {
            break;
        } else {
            (*pos)++;
        }
    }
    return start;
}

// ---------------------------------------------------------------------------
// Comment scanner
// ---------------------------------------------------------------------------

// Scan a `;` comment.  Expects line[pos] == ';'.
// Handles `; [...]` block comments with nested brackets and also
// `; rest-of-line` comments where there is no opening bracket.
// Tags everything from `;` to the end of the comment as SYNTAX_COMMENT.
// Returns true if a comment was recognised, advancing *pos.
static bool scan_comment(const char *line, int length,
                         uint8_t *categories, int *pos)
{
    if (*pos >= length || line[*pos] != ';')
        return false;

    int start = *pos;
    (*pos)++;  // skip ';'

    // Skip optional whitespace between ';' and '['
    while (*pos < length && (line[*pos] == ' ' || line[*pos] == '\t'))
        (*pos)++;

    if (*pos < length && line[*pos] == '[') {
        // Block comment: ; [...]  — scan to matching ']'
        int depth = 0;
        while (*pos < length) {
            if (line[*pos] == '[') depth++;
            else if (line[*pos] == ']') {
                depth--;
                if (depth == 0) {
                    (*pos)++;  // consume the closing ']'
                    break;
                }
            }
            (*pos)++;
        }
    } else {
        // Line comment: ; rest-of-line
        *pos = length;
    }

    tag_range(categories, start, *pos, SYNTAX_COMMENT);
    return true;
}

// ---------------------------------------------------------------------------
// Number scanner
// ---------------------------------------------------------------------------

// Try to scan a number at the current position.
// Pattern: -?[0-9]+(.[0-9]*)?([eEnN][+-]?[0-9]+)?
// A leading '-' is only treated as part of the number if the previous
// character is a delimiter or the start of line (to avoid matching the
// minus in `sum 3-2` as a negative number).
static bool scan_number(const char *line, int length,
                        uint8_t *categories, int *pos)
{
    int p = *pos;

    // Optional leading minus
    if (p < length && line[p] == '-') {
        // Only treat as sign if preceded by a delimiter or at SOL
        if (p == 0 || is_delimiter(line[p - 1])) {
            p++;
        }
    }

    // Must have at least one digit
    if (p >= length || !isdigit((unsigned char)line[p])) {
        return false;
    }

    // Integer part
    while (p < length && isdigit((unsigned char)line[p]))
        p++;

    // Optional decimal part
    if (p < length && line[p] == '.') {
        p++;
        while (p < length && isdigit((unsigned char)line[p]))
            p++;
    }

    // Optional exponent (e/E/n/N)
    if (p < length && (line[p] == 'e' || line[p] == 'E' ||
                       line[p] == 'n' || line[p] == 'N')) {
        bool is_n = (line[p] == 'n' || line[p] == 'N');
        p++;
        // Only e/E allows optional sign, not n/N (matching lexer)
        if (!is_n && p < length && (line[p] == '+' || line[p] == '-'))
            p++;
        // Must have at least one digit after exponent
        if (p >= length || !isdigit((unsigned char)line[p]))
            return false;
        while (p < length && isdigit((unsigned char)line[p]))
            p++;
    }

    // Check that the number is followed by a delimiter (or end of line)
    if (p < length && !is_delimiter(line[p]))
        return false;

    tag_range(categories, *pos, p, SYNTAX_NUMBER);
    *pos = p;
    return true;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

int syntax_highlight_line(const char *line, int length,
                          uint8_t *categories, int initial_depth)
{
    if (length <= 0)
        return initial_depth;

    // Initialise output to SYNTAX_DEFAULT
    tag_range(categories, 0, length, SYNTAX_DEFAULT);

    int pos = 0;
    int depth = initial_depth;

    // --- Check for TO / END at the start of the line ---
    // Skip leading whitespace
    int ws_end = 0;
    while (ws_end < length && (line[ws_end] == ' ' || line[ws_end] == '\t'))
        ws_end++;

    // Check for TO keyword at line start
    int kw_len = match_keyword(line, ws_end, length, "to");
    if (kw_len > 0 && (ws_end + kw_len >= length ||
                        line[ws_end + kw_len] == ' ' ||
                        line[ws_end + kw_len] == '\t')) {
        // This is a TO line — reset bracket depth
        depth = 0;

        // Tag leading whitespace
        tag_range(categories, 0, ws_end, SYNTAX_DEFAULT);

        // Tag "to" keyword
        tag_range(categories, ws_end, ws_end + kw_len, SYNTAX_KEYWORD);
        pos = ws_end + kw_len;

        // Skip whitespace after TO
        skip_ws(line, length, categories, &pos);

        // Tag procedure name (next word)
        if (pos < length && !is_delimiter(line[pos])) {
            int name_start = pos;
            read_word_span(line, length, &pos);
            tag_range(categories, name_start, pos, SYNTAX_FUNCTION);
        }

        // Remaining tokens on the TO line: parameters and body via $self
        // Fall through to the main scanning loop below
    }

    // Check for END keyword at line start (only if we haven't matched TO)
    if (pos == 0) {
        kw_len = match_keyword(line, ws_end, length, "end");
        if (kw_len > 0) {
            // END must be followed only by whitespace or end of line
            int after = ws_end + kw_len;
            bool valid_end = true;
            while (after < length) {
                if (line[after] != ' ' && line[after] != '\t') {
                    valid_end = false;
                    break;
                }
                after++;
            }
            if (valid_end) {
                tag_range(categories, 0, ws_end, SYNTAX_DEFAULT);
                tag_range(categories, ws_end, ws_end + kw_len, SYNTAX_KEYWORD);
                tag_range(categories, ws_end + kw_len, length, SYNTAX_DEFAULT);
                return depth;
            }
        }
    }

    // --- Main scanning loop for the rest of the line ---
    while (pos < length) {
        char c = line[pos];

        // Whitespace
        if (c == ' ' || c == '\t') {
            tag_one(categories, pos, SYNTAX_DEFAULT);
            pos++;
            continue;
        }

        // Comment: ; [...]  or ; rest-of-line
        if (c == ';') {
            scan_comment(line, length, categories, &pos);
            continue;
        }

        // Variable: :word (with backslash escape support)
        if (c == ':') {
            int start = pos;
            pos++;  // skip ':'
            while (pos < length) {
                if (line[pos] == '\\') {
                    pos++;  // skip backslash
                    if (pos < length)
                        pos++;  // skip escaped character
                } else if (is_delimiter(line[pos])) {
                    break;
                } else {
                    pos++;
                }
            }
            tag_range(categories, start, pos, SYNTAX_VARIABLE);
            continue;
        }

        // Quoted word: "word (with backslash escape support)
        if (c == '"') {
            int start = pos;
            pos++;  // skip '"'
            while (pos < length) {
                if (line[pos] == '\\') {
                    pos++;  // skip backslash
                    if (pos < length)
                        pos++;  // skip escaped character
                } else if (line[pos] == ' '  || line[pos] == '\t' ||
                           line[pos] == '['  || line[pos] == ']' ||
                           line[pos] == '('  || line[pos] == ')') {
                    break;
                } else {
                    pos++;
                }
            }
            tag_range(categories, start, pos, SYNTAX_STRING);
            continue;
        }

        // Opening bracket/paren
        if (c == '[' || c == '(') {
            depth++;
            tag_one(categories, pos, bracket_category(depth));
            pos++;
            continue;
        }

        // Closing bracket/paren
        if (c == ']' || c == ')') {
            if (depth > 0) {
                tag_one(categories, pos, bracket_category(depth));
                depth--;
            } else {
                tag_one(categories, pos, bracket_category(1));
            }
            pos++;
            continue;
        }

        // Operators: + - * / = < >
        if (c == '+' || c == '*' || c == '/' ||
            c == '=' || c == '<' || c == '>') {
            tag_one(categories, pos, SYNTAX_DEFAULT);
            pos++;
            continue;
        }

        // Minus: could be negative number or operator
        if (c == '-') {
            // Try as number first (only if preceded by delimiter/SOL)
            if (scan_number(line, length, categories, &pos))
                continue;
            // Otherwise it's an operator
            tag_one(categories, pos, SYNTAX_DEFAULT);
            pos++;
            continue;
        }

        // Number (starts with digit)
        if (isdigit((unsigned char)c)) {
            if (scan_number(line, length, categories, &pos))
                continue;
            // Shouldn't reach here, but tag as default if it does
            tag_one(categories, pos, SYNTAX_DEFAULT);
            pos++;
            continue;
        }

        // Bare word: [a-zA-Z][a-zA-Z0-9_.?]*  → SYNTAX_COMMAND
        // Also handles backslash escapes within words.
        if (isalpha((unsigned char)c) || c == '\\') {
            int start = pos;
            pos++;
            while (pos < length) {
                if (line[pos] == '\\') {
                    pos++;  // skip backslash
                    if (pos < length)
                        pos++;  // skip escaped character
                } else if (isalnum((unsigned char)line[pos]) ||
                           line[pos] == '_' || line[pos] == '.' || line[pos] == '?' ||
                           line[pos] == ':') {
                    pos++;
                } else {
                    break;
                }
            }
            tag_range(categories, start, pos, SYNTAX_COMMAND);
            continue;
        }

        // Anything else — tag as default
        tag_one(categories, pos, SYNTAX_DEFAULT);
        pos++;
    }

    return depth;
}
