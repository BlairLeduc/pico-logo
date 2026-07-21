//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Lexer-driven helper that turns a string into a Logo list, recursing
//  into nested `[...]` sublists. Shared by the `parse` and `readlist`
//  primitives so both behave identically with respect to brackets,
//  operators, and quoted/colon-prefixed words.
//

#include "parse_list.h"

#include "lexer.h"
#include "memory.h"

// Parse a list body from the lexer up to a matching TOKEN_RIGHT_BRACKET,
// TOKEN_EOF, or TOKEN_ERROR. Nested lists are parsed recursively against
// the same lexer so bracket matching mirrors the main parser exactly.
static ParseListResult parse_list_body(Lexer *lexer)
{
    Node head = NODE_NIL;
    Node tail = NODE_NIL;

    for (;;)
    {
        Token tok = lexer_next_token(lexer);
        if (tok.type == TOKEN_EOF || tok.type == TOKEN_ERROR ||
            tok.type == TOKEN_RIGHT_BRACKET)
        {
            break;
        }

        Node element = NODE_NIL;
        bool have_element = true;

        switch (tok.type)
        {
        case TOKEN_WORD:
        case TOKEN_QUOTED:
        case TOKEN_NUMBER:
        case TOKEN_COLON:
        {
            // Resolve backslash escapes and strip vertical-bar quoting, so
            // that `parse`/`readlist` see the same words the main parser
            // would (e.g. `|San Francisco|` becomes one word "San Francisco").
            element = mem_atom_unescape(tok.start, tok.length);
            if (mem_is_nil(element))
            {
                // Word exceeds the 255-char atom limit.
                return (ParseListResult){.node = NODE_NIL, .success = false};
            }
            break;
        }

        case TOKEN_LEFT_BRACKET:
        {
            ParseListResult inner = parse_list_body(lexer);
            if (!inner.success)
            {
                return inner;
            }
            // An empty inner list must be encoded as NODE_MAKE_LIST(0), not
            // NODE_NIL, so it round-trips through cell encoding correctly.
            element = mem_is_nil(inner.node) ? NODE_MAKE_LIST(0) : inner.node;
            break;
        }

        case TOKEN_PLUS:
            element = mem_atom_cstr("+");
            break;
        case TOKEN_MINUS:
        case TOKEN_UNARY_MINUS:
            element = mem_atom_cstr("-");
            break;
        case TOKEN_MULTIPLY:
            element = mem_atom_cstr("*");
            break;
        case TOKEN_DIVIDE:
            element = mem_atom_cstr("/");
            break;
        case TOKEN_EQUALS:
            element = mem_atom_cstr("=");
            break;
        case TOKEN_LESS_THAN:
            element = mem_atom_cstr("<");
            break;
        case TOKEN_GREATER_THAN:
            element = mem_atom_cstr(">");
            break;
        case TOKEN_LEFT_PAREN:
            element = mem_atom_cstr("(");
            break;
        case TOKEN_RIGHT_PAREN:
            element = mem_atom_cstr(")");
            break;

        default:
            have_element = false;
            break;
        }

        if (!have_element)
            continue;

        if (!mem_list_append(&head, &tail, element))
        {
            return (ParseListResult){.node = NODE_NIL, .success = false};
        }
    }

    return (ParseListResult){.node = head, .success = true};
}

ParseListResult parse_list_from_string(const char *line)
{
    Lexer lexer;
    lexer_init(&lexer, line);
    return parse_list_body(&lexer);
}
