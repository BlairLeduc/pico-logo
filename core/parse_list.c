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

#include <string.h>

#include "lexer.h"
#include "memory.h"

// Append a freshly built element cell (a single-cell list whose car is the
// element) to the list under construction. Updates *head and *tail as
// needed. Returns false if cell allocation failed.
static bool append_element(Node element, Node *head, Node *tail)
{
    Node new_cell = mem_cons(element, NODE_NIL);
    if (mem_is_nil(new_cell))
    {
        return false;
    }
    if (mem_is_nil(*head))
    {
        *head = new_cell;
    }
    else
    {
        mem_set_cdr(*tail, new_cell);
    }
    *tail = new_cell;
    return true;
}

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
            char buf[256];
            size_t len = tok.length;
            if (len >= sizeof(buf))
                len = sizeof(buf) - 1;
            memcpy(buf, tok.start, len);
            buf[len] = '\0';
            element = mem_atom(buf, len);
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

        if (!append_element(element, &head, &tail))
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
