//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Syntax highlighting for the Logo editor.
//
//  Produces per-character category tags from a line of Logo source text,
//  following the grammar defined in grammar/syntaxes/logo.tmLanguage.json
//  and using VS Code Dark+ theme semantics.
//
//  The highlighter is line-local with one exception: bracket/parenthesis
//  nesting depth carries across lines but resets to 0 at any line that
//  begins a procedure definition (starts with TO).
//

#pragma once

#include <stdint.h>

// Syntax categories — each character in a line is tagged with one of these.
// The editor maps these to palette slots for rendering.
typedef enum {
    SYNTAX_DEFAULT = 0,   // Plain text / whitespace / operators
    SYNTAX_COMMENT,       // ; [...] block comments
    SYNTAX_KEYWORD,       // TO, END
    SYNTAX_FUNCTION,      // Procedure name after TO
    SYNTAX_VARIABLE,      // :varname
    SYNTAX_STRING,        // "word (quoted words)
    SYNTAX_NUMBER,        // Numeric literals
    SYNTAX_COMMAND,       // Bare words (primitive/procedure calls)
    SYNTAX_BRACKET_1,     // Bracket/paren at nesting depth 1, 4, 7, ...
    SYNTAX_BRACKET_2,     // Bracket/paren at nesting depth 2, 5, 8, ...
    SYNTAX_BRACKET_3,     // Bracket/paren at nesting depth 3, 6, 9, ...
    SYNTAX_CATEGORY_COUNT
} SyntaxCategory;

// Highlight a single line of Logo source text.
//
// line:           pointer to the line text (not necessarily NUL-terminated)
// length:         number of characters in the line
// categories:     output array of length `length` (or NULL for depth-only mode)
//                 each byte receives a SyntaxCategory value
// initial_depth:  bracket nesting depth inherited from the previous line
//
// Returns the bracket nesting depth at the end of this line.
// If the line starts with TO, initial_depth is forced to 0 internally.
int syntax_highlight_line(const char *line, int length,
                          uint8_t *categories, int initial_depth);
