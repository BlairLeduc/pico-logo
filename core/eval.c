//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "eval.h"
#include "error.h"
#include "primitives.h"
#include "variables.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Binding powers for Pratt parser
#define BP_NONE 0
#define BP_COMPARISON 10      // = < >
#define BP_ADDITIVE 20        // + -
#define BP_MULTIPLICATIVE 30  // * /

// Forward declarations
static Result eval_expr_bp(Evaluator *eval, int min_bp);
static Result eval_primary(Evaluator *eval);

void eval_init(Evaluator *eval, Lexer *lexer)
{
    eval->lexer = lexer;
    eval->has_current = false;
    eval->paren_depth = 0;
    eval->error_code = 0;
    eval->error_context = NULL;
}

// Get current token, fetching if needed
static Token peek(Evaluator *eval)
{
    if (!eval->has_current)
    {
        eval->current = lexer_next_token(eval->lexer);
        eval->has_current = true;
    }
    return eval->current;
}

// Consume current token
static void advance(Evaluator *eval)
{
    eval->has_current = false;
}

// Check if at end of input
bool eval_at_end(Evaluator *eval)
{
    Token t = peek(eval);
    return t.type == TOKEN_EOF;
}

// Check if token is an infix operator
static int get_infix_bp(TokenType type)
{
    switch (type)
    {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
        return BP_ADDITIVE;
    case TOKEN_MULTIPLY:
    case TOKEN_DIVIDE:
        return BP_MULTIPLICATIVE;
    case TOKEN_EQUALS:
    case TOKEN_LESS_THAN:
    case TOKEN_GREATER_THAN:
        return BP_COMPARISON;
    default:
        return BP_NONE;
    }
}

// Try to parse a number from a string
static bool is_number_string(const char *str, size_t len)
{
    if (len == 0)
        return false;
    size_t i = 0;

    if (str[i] == '-' || str[i] == '+')
        i++;
    if (i >= len)
        return false;

    bool has_digit = false;
    while (i < len && isdigit((unsigned char)str[i]))
    {
        has_digit = true;
        i++;
    }
    if (i < len && str[i] == '.')
    {
        i++;
        while (i < len && isdigit((unsigned char)str[i]))
        {
            has_digit = true;
            i++;
        }
    }
    if (i < len && (str[i] == 'e' || str[i] == 'E' || str[i] == 'n' || str[i] == 'N'))
    {
        i++;
        if (i < len && (str[i] == '-' || str[i] == '+'))
            i++;
        while (i < len && isdigit((unsigned char)str[i]))
            i++;
    }
    return has_digit && i == len;
}

static float parse_number(const char *str, size_t len)
{
    // Create null-terminated copy for strtof
    char buf[64];
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, str, len);
    buf[len] = '\0';

    // Handle 'n' notation: 1n4 = 0.0001
    char *n_pos = strchr(buf, 'n');
    if (!n_pos)
        n_pos = strchr(buf, 'N');

    if (n_pos)
    {
        *n_pos = '\0';
        float mantissa = strtof(buf, NULL);
        float exp = strtof(n_pos + 1, NULL);
        float result = mantissa;
        for (int i = 0; i < (int)exp; i++)
        {
            result /= 10.0f;
        }
        return result;
    }
    return strtof(buf, NULL);
}

// Parse a list from tokens until ]
static Node parse_list(Evaluator *eval)
{
    Node list = NODE_NIL;
    Node tail = NODE_NIL;

    while (true)
    {
        Token t = peek(eval);
        if (t.type == TOKEN_EOF || t.type == TOKEN_RIGHT_BRACKET)
        {
            if (t.type == TOKEN_RIGHT_BRACKET)
                advance(eval);
            break;
        }

        Node item = NODE_NIL;

        if (t.type == TOKEN_LEFT_BRACKET)
        {
            advance(eval);
            item = parse_list(eval);
            // Wrap in list marker for later
            item = NODE_MAKE_LIST(NODE_GET_INDEX(item));
        }
        else if (t.type == TOKEN_WORD || t.type == TOKEN_NUMBER)
        {
            item = mem_atom(t.start, t.length);
            advance(eval);
        }
        else if (t.type == TOKEN_QUOTED)
        {
            // Skip the quote character
            item = mem_atom(t.start + 1, t.length - 1);
            advance(eval);
        }
        else if (t.type == TOKEN_COLON)
        {
            // Store :var as-is in list (will be evaluated when run)
            // Create a word with the colon
            item = mem_atom(t.start, t.length);
            advance(eval);
        }
        else
        {
            advance(eval);
            continue;
        }

        Node new_cons = mem_cons(item, NODE_NIL);
        if (mem_is_nil(list))
        {
            list = new_cons;
            tail = new_cons;
        }
        else
        {
            mem_set_cdr(tail, new_cons);
            tail = new_cons;
        }
    }
    return list;
}

// Evaluate a primary expression
static Result eval_primary(Evaluator *eval)
{
    Token t = peek(eval);

    switch (t.type)
    {
    case TOKEN_NUMBER:
    {
        advance(eval);
        return result_ok(value_number(parse_number(t.start, t.length)));
    }

    case TOKEN_QUOTED:
    {
        advance(eval);
        // Skip the quote character
        Node atom = mem_atom(t.start + 1, t.length - 1);
        return result_ok(value_word(atom));
    }

    case TOKEN_COLON:
    {
        advance(eval);
        // :var is shorthand for thing "var
        // The token includes the colon, so skip it
        // Intern the name so the pointer persists for error messages
        Node name_atom = mem_atom(t.start + 1, t.length - 1);
        const char *name = mem_word_ptr(name_atom);

        Value v;
        if (!var_get(name, &v))
        {
            return result_error_arg(ERR_NO_VALUE, NULL, name);
        }
        return result_ok(v);
    }

    case TOKEN_LEFT_BRACKET:
    {
        advance(eval);
        Node list = parse_list(eval);
        return result_ok(value_list(list));
    }

    case TOKEN_LEFT_PAREN:
    {
        advance(eval);
        eval->paren_depth++;
        
        // Check if this is a procedure call with variable args: (proc arg1 arg2 ...)
        Token next = peek(eval);
        if (next.type == TOKEN_WORD && !is_number_string(next.start, next.length))
        {
            // Check if it's a primitive
            char name_buf[64];
            size_t name_len = next.length;
            if (name_len >= sizeof(name_buf))
                name_len = sizeof(name_buf) - 1;
            memcpy(name_buf, next.start, name_len);
            name_buf[name_len] = '\0';
            
            const Primitive *prim = primitive_find(name_buf);
            if (prim)
            {
                advance(eval); // consume procedure name
                
                // Greedily collect all arguments until )
                Value args[16];
                int argc = 0;
                
                while (argc < 16)
                {
                    Token t = peek(eval);
                    if (t.type == TOKEN_RIGHT_PAREN || t.type == TOKEN_EOF)
                        break;
                    
                    Result arg = eval_expression(eval);
                    if (arg.status == RESULT_ERROR)
                    {
                        eval->paren_depth--;
                        return arg;
                    }
                    if (arg.status != RESULT_OK)
                        break;
                    args[argc++] = arg.value;
                }
                
                // Consume closing paren
                Token closing = peek(eval);
                if (closing.type == TOKEN_RIGHT_PAREN)
                {
                    advance(eval);
                }
                eval->paren_depth--;
                
                return prim->func(eval, argc, args);
            }
        }
        
        // Not a procedure call, just grouping
        Result r = eval_expr_bp(eval, BP_NONE);
        if (r.status == RESULT_ERROR)
        {
            eval->paren_depth--;
            return r;
        }

        Token closing = peek(eval);
        if (closing.type == TOKEN_RIGHT_PAREN)
        {
            advance(eval);
        }
        eval->paren_depth--;
        return r;
    }

    case TOKEN_MINUS:
    case TOKEN_UNARY_MINUS:
    {
        // Unary minus
        advance(eval);
        Result r = eval_primary(eval);
        if (r.status != RESULT_OK)
            return r;

        float n;
        if (!value_to_number(r.value, &n))
        {
            return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(r.value));
        }
        return result_ok(value_number(-n));
    }

    case TOKEN_WORD:
    {
        // Check if it's a number (self-quoting)
        if (is_number_string(t.start, t.length))
        {
            advance(eval);
            return result_ok(value_number(parse_number(t.start, t.length)));
        }

        // Look up primitive
        // Need null-terminated name for lookup
        char name_buf[64];
        size_t name_len = t.length;
        if (name_len >= sizeof(name_buf))
            name_len = sizeof(name_buf) - 1;
        memcpy(name_buf, t.start, name_len);
        name_buf[name_len] = '\0';

        const Primitive *prim = primitive_find(name_buf);
        if (prim)
        {
            advance(eval);
            // Collect default number of arguments
            Value args[16]; // Max args
            int argc = 0;

            for (int i = 0; i < prim->default_args && !eval_at_end(eval); i++)
            {
                // Check for tokens that would end args
                Token next = peek(eval);
                if (next.type == TOKEN_RIGHT_PAREN || next.type == TOKEN_RIGHT_BRACKET)
                {
                    break;
                }

                Result arg = eval_expression(eval);
                if (arg.status == RESULT_ERROR)
                    return arg;
                if (arg.status != RESULT_OK)
                {
                    // Use prim->name since it's a static string
                    return result_error_arg(ERR_NOT_ENOUGH_INPUTS, prim->name, NULL);
                }
                args[argc++] = arg.value;
            }

            if (argc < prim->default_args)
            {
                return result_error_arg(ERR_NOT_ENOUGH_INPUTS, prim->name, NULL);
            }

            return prim->func(eval, argc, args);
        }

        // Unknown procedure - intern the name so pointer persists
        Node name_atom = mem_atom(t.start, t.length);
        return result_error_arg(ERR_DONT_KNOW_HOW, mem_word_ptr(name_atom), NULL);
    }

    case TOKEN_RIGHT_PAREN:
        return result_error(ERR_PAREN_MISMATCH);

    case TOKEN_EOF:
        return result_error(ERR_NOT_ENOUGH_INPUTS);

    default:
        return result_error(ERR_DONT_KNOW_WHAT);
    }
}

// Pratt parser for expressions with infix operators
static Result eval_expr_bp(Evaluator *eval, int min_bp)
{
    Result lhs = eval_primary(eval);
    if (lhs.status != RESULT_OK)
        return lhs;

    while (true)
    {
        Token op = peek(eval);
        int bp = get_infix_bp(op.type);

        if (bp == BP_NONE || bp < min_bp)
            break;

        advance(eval);

        Result rhs = eval_expr_bp(eval, bp + 1);
        if (rhs.status != RESULT_OK)
            return rhs;

        float left_n, right_n;
        bool left_ok = value_to_number(lhs.value, &left_n);
        bool right_ok = value_to_number(rhs.value, &right_n);
        
        // Get operator name for error messages
        const char *op_name;
        switch (op.type)
        {
        case TOKEN_PLUS: op_name = "+"; break;
        case TOKEN_MINUS: op_name = "-"; break;
        case TOKEN_MULTIPLY: op_name = "*"; break;
        case TOKEN_DIVIDE: op_name = "/"; break;
        case TOKEN_EQUALS: op_name = "="; break;
        case TOKEN_LESS_THAN: op_name = "<"; break;
        case TOKEN_GREATER_THAN: op_name = ">"; break;
        default: op_name = "?"; break;
        }
        
        if (!left_ok)
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, op_name, value_to_string(lhs.value));
        if (!right_ok)
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, op_name, value_to_string(rhs.value));

        float result;
        switch (op.type)
        {
        case TOKEN_PLUS:
            result = left_n + right_n;
            break;
        case TOKEN_MINUS:
            result = left_n - right_n;
            break;
        case TOKEN_MULTIPLY:
            result = left_n * right_n;
            break;
        case TOKEN_DIVIDE:
            if (right_n == 0)
                return result_error(ERR_DIVIDE_BY_ZERO);
            result = left_n / right_n;
            break;
        case TOKEN_EQUALS:
            result = (left_n == right_n) ? 1 : 0;
            break;
        case TOKEN_LESS_THAN:
            result = (left_n < right_n) ? 1 : 0;
            break;
        case TOKEN_GREATER_THAN:
            result = (left_n > right_n) ? 1 : 0;
            break;
        default:
            return result_error(ERR_DONT_KNOW_WHAT);
        }

        lhs = result_ok(value_number(result));
    }

    return lhs;
}

Result eval_expression(Evaluator *eval)
{
    return eval_expr_bp(eval, BP_NONE);
}

Result eval_instruction(Evaluator *eval)
{
    if (eval_at_end(eval))
    {
        return result_none();
    }

    Result r = eval_expression(eval);

    // If we got a value but this was at top level, it's an error
    // (unless it's from inside a run/repeat which handles this)
    return r;
}

Result eval_run_list(Evaluator *eval, Node list)
{
    // Save current lexer state
    Lexer *old_lexer = eval->lexer;
    Token old_current = eval->current;
    bool old_has_current = eval->has_current;

    // Build string from list for lexing
    char buffer[256];
    int pos = 0;

    Node node = list;
    while (!mem_is_nil(node) && pos < 250)
    {
        Node element = mem_car(node);
        if (mem_is_word(element))
        {
            const char *str = mem_word_ptr(element);
            size_t len = mem_word_len(element);
            if (pos + (int)len + 1 < 256)
            {
                if (pos > 0)
                    buffer[pos++] = ' ';
                memcpy(buffer + pos, str, len);
                pos += len;
            }
        }
        else if (mem_is_list(element))
        {
            // For nested lists, we need to serialize them with brackets
            // Simplified for now: just mark with brackets
            if (pos > 0)
                buffer[pos++] = ' ';
            buffer[pos++] = '[';
            // Recursively would be better, but keep it simple
            buffer[pos++] = ']';
        }
        node = mem_cdr(node);
    }
    buffer[pos] = '\0';

    // Create new lexer for list content
    Lexer list_lexer;
    lexer_init(&list_lexer, buffer);
    eval->lexer = &list_lexer;
    eval->has_current = false;

    Result r = result_none();

    while (!eval_at_end(eval))
    {
        r = eval_instruction(eval);

        // Propagate stop/output/error immediately
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            break;
        }

        // If we got a value at top level of list, it's an error
        if (r.status == RESULT_OK)
        {
            r = result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(r.value));
            break;
        }
    }

    // Restore lexer state
    eval->lexer = old_lexer;
    eval->current = old_current;
    eval->has_current = old_has_current;

    return r;
}
