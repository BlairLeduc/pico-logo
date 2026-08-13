//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Expression parser: Pratt parser, eval_primary, eval_expr_bp.
//

#include "eval_internal.h"
#include "core/atom_memo.h"
#include "error.h"
#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "frame.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include "hot.h"

// Record a resolved binding on the atom, so the next lookup of this name is a
// single read. An index that will not fit the memo field is simply not cached
// (the tables are sized to fit, so this is belt and braces).
static void remember_binding(uint8_t *memo, uint16_t kind, int index)
{
    if (!memo || index < 0 || (unsigned)index >= ATOM_MEMO_INDEX_LIMIT)
        return;
    mem_atom_memo_set(memo,
        atom_memo_set_binding(mem_atom_memo_get(memo), kind, (unsigned)index));
}

// See WordBinding in eval_internal.h.
//
// Two paths. A token from a list carries its atom, so one mem_word_view gives
// the characters, the name for error messages, and the memo that answers the
// lookup outright. A token from raw text has no atom, so it interns the name
// as it always did and resolves by string; the REPL line is not the hot path.
WordBinding resolve_word(Token t)
{
    WordBinding b = {NULL, NULL, NODE_NIL, NULL};
    const char *str = t.start;
    size_t len = t.length;
    uint8_t *memo = NULL;

    if (mem_is_nil(t.atom))
    {
        b.atom = mem_atom(t.start, t.length);
        b.name = mem_word_ptr(b.atom);
    }
    else
    {
        b.atom = t.atom;
        mem_word_view(t.atom, &str, &len, &memo);
        b.name = str;

        uint16_t word = mem_atom_memo_get(memo);
        switch (atom_memo_bind_kind(word))
        {
        case ATOM_BIND_PRIMITIVE:
            b.prim = primitive_by_index((int)atom_memo_bind_index(word));
            if (b.prim)
                return b;
            break;
        case ATOM_BIND_PROCEDURE:
            b.proc = proc_by_index((int)atom_memo_bind_index(word));
            if (b.proc)
                return b;
            break;
        case ATOM_BIND_NONE:
            return b;
        default:
            break;
        }
    }

    b.prim = primitive_find_n(str, len);
    if (b.prim)
    {
        remember_binding(memo, ATOM_BIND_PRIMITIVE, primitive_index_of(b.prim));
        return b;
    }

    b.proc = proc_find_n(str, len);
    if (b.proc)
    {
        remember_binding(memo, ATOM_BIND_PROCEDURE, proc_index_of(b.proc));
        return b;
    }

    remember_binding(memo, ATOM_BIND_NONE, 0);
    return b;
}

// Apply a binary infix operator to two values.
// Returns RESULT_OK with the computed value, or RESULT_ERROR on type/divide errors.
Result apply_binary_op(TokenType op_type, Value left, Value right)
{
    // Handle = separately since it works with all value types
    if (op_type == TOKEN_EQUALS)
    {
        bool equal = values_equal(left, right);
        return result_ok(value_bool(equal));
    }

    float left_n, right_n;
    bool left_ok = value_to_number(left, &left_n);
    bool right_ok = value_to_number(right, &right_n);

    // Get operator name for error messages
    const char *op_name;
    switch (op_type)
    {
    case TOKEN_PLUS: op_name = "+"; break;
    case TOKEN_MINUS: op_name = "-"; break;
    case TOKEN_MULTIPLY: op_name = "*"; break;
    case TOKEN_DIVIDE: op_name = "/"; break;
    case TOKEN_LESS_THAN: op_name = "<"; break;
    case TOKEN_GREATER_THAN: op_name = ">"; break;
    default: op_name = "?"; break;
    }

    if (!left_ok)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, op_name, value_to_string(left));
    if (!right_ok)
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, op_name, value_to_string(right));

    switch (op_type)
    {
    case TOKEN_PLUS:
        return result_ok(value_number(left_n + right_n));
    case TOKEN_MINUS:
        return result_ok(value_number(left_n - right_n));
    case TOKEN_MULTIPLY:
        return result_ok(value_number(left_n * right_n));
    case TOKEN_DIVIDE:
        if (right_n == 0)
            return result_error(ERR_DIVIDE_BY_ZERO);
        return result_ok(value_number(left_n / right_n));
    case TOKEN_LESS_THAN:
        return result_ok(value_bool(left_n < right_n));
    case TOKEN_GREATER_THAN:
        return result_ok(value_bool(left_n > right_n));
    default:
        return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, op_name);
    }
}

// Try to parse a number from a string
bool is_number_string(const char *str, size_t len)
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
        bool is_n_notation = (str[i] == 'n' || str[i] == 'N');
        i++;
        // Only allow signs after e/E, not after n/N
        if (!is_n_notation && i < len && (str[i] == '-' || str[i] == '+'))
            i++;
        // Require at least one digit after the exponent marker, matching
        // the lexer's is_valid_number and token_source's is_number_word —
        // otherwise a bare `1e` silently evaluates as 1 (B10).
        if (i >= len || !isdigit((unsigned char)str[i]))
            return false;
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
        // Parse exponent as digits-only non-negative integer
        int exp = 0;
        const char *p = n_pos + 1;
        while (*p != '\0')
        {
            exp = exp * 10 + (*p - '0');
            p++;
        }
        float result = mantissa;
        for (int i = 0; i < exp; i++)
        {
            result /= 10.0f;
        }
        return result;
    }
    return strtof(buf, NULL);
}

// Parse a list from tokens until ].
// Returns 0 on success (with *out receiving the parsed list), otherwise an
// error code: ERR_OUT_OF_SPACE (node pool exhausted), ERR_UNCLOSED_BRACKET
// (input ran out before the `]`), or ERR_DONT_KNOW_WHAT (a token kind that
// cannot appear in a list literal). Nothing is dropped silently — a `[...]`
// that does not parse must be reported, not quietly truncated.
static int parse_list(Evaluator *eval, Node *out)
{
    Node list = NODE_NIL;
    Node tail = NODE_NIL;

    while (true)
    {
        Token t = peek(eval);
        if (t.type == TOKEN_EOF)
            return ERR_UNCLOSED_BRACKET;
        if (t.type == TOKEN_RIGHT_BRACKET)
        {
            advance(eval);
            break;
        }

        Node item = NODE_NIL;

        if (t.type == TOKEN_LEFT_BRACKET)
        {
            advance(eval);
            int err = parse_list(eval, &item);
            if (err != 0)
                return err;
            // Wrap in list marker for later
            item = NODE_MAKE_LIST(NODE_GET_INDEX(item));
        }
        else if (t.type == TOKEN_WORD || t.type == TOKEN_NUMBER ||
                 t.type == TOKEN_QUOTED || t.type == TOKEN_COLON)
        {
            // Resolve backslash escapes and strip vertical-bar quoting so a
            // list literal holds the same words the reader would produce for
            // `parse`/`readlist` (e.g. `[|a b|]` holds the word "a b"). A
            // leading `"` (quoted word) or `:` (variable) is neither a bar nor
            // a backslash, so it survives and the word runs correctly later.
            // A NIL result means the word exceeded the 255-char atom limit;
            // fail the parse rather than store an invalid node.
            item = mem_atom_unescape(t.start, t.length);
            if (mem_is_nil(item))
                return ERR_OUT_OF_SPACE;
            advance(eval);
        }
        else if (t.type == TOKEN_PLUS || t.type == TOKEN_MINUS || 
                 t.type == TOKEN_UNARY_MINUS ||
                 t.type == TOKEN_MULTIPLY || t.type == TOKEN_DIVIDE ||
                 t.type == TOKEN_EQUALS || t.type == TOKEN_LESS_THAN ||
                 t.type == TOKEN_GREATER_THAN)
        {
            // Store operator tokens as words in lists
            item = mem_atom(t.start, t.length);
            advance(eval);
        }
        else if (t.type == TOKEN_LEFT_PAREN || t.type == TOKEN_RIGHT_PAREN)
        {
            // Store parentheses as words in lists  
            item = mem_atom(t.start, t.length);
            advance(eval);
        }
        else
        {
            // Every token kind a list literal can hold is handled above, so
            // anything left is something we cannot represent. Report it
            // rather than dropping it, which would silently change the list.
            return ERR_DONT_KNOW_WHAT;
        }

        if (!mem_list_append(&list, &tail, item))
        {
            return ERR_OUT_OF_SPACE;
        }
    }
    *out = list;
    return 0;
}

// Evaluate a primary expression
Result LOGO_HOT(eval_primary)(Evaluator *eval)
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
        // Skip the quote character and process escape sequences
        Node atom = mem_atom_unescape(t.start + 1, t.length - 1);
        // A full atom region interns nothing, and `value_word(NODE_NIL)` is a
        // word with no characters behind it: `mem_word_ptr` gives NULL and the
        // first primitive to read the name dereferences it. Report the space
        // we could not find instead. The empty word `"` still interns, so a
        // nil node here can only be the failure.
        if (mem_is_nil(atom))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
        return result_ok(value_word(atom));
    }

    case TOKEN_COLON:
    {
        advance(eval);
        // :var is shorthand for thing "var
        // The token includes the colon, so skip it
        // Intern the name so the pointer persists for error messages
        // Process escape sequences in variable names
        Node name_atom = mem_atom_unescape(t.start + 1, t.length - 1);
        const char *name = mem_word_ptr(name_atom);
        // The same failure the quoted case reports above, and the same crash
        // if it is not: on a full atom region the name interns to nothing,
        // `mem_word_ptr` gives NULL, and `var_get` hashes it. `:x` reaches
        // `var_get` directly rather than through `REQUIRE_WORD_STR`, so it
        // needs its own check -- found by the test suite running a workspace
        // to exhaustion and then reading a variable.
        if (name == NULL)
        {
            return result_error(ERR_OUT_OF_SPACE);
        }

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
        
        // Check if this is a pre-parsed sublist from NodeIterator
        // This happens when the source list has nested list nodes, not flat [ ] tokens
        // token_source_has_sublist returns true even for empty sublists (NODE_NIL)
        if (token_source_has_sublist(&eval->token_source))
        {
            // For NodeIterator with nested list: sublist is already parsed, just use it
            Node sublist = token_source_get_sublist(&eval->token_source);
            token_source_consume_sublist(&eval->token_source);
            // Handle the list marker wrapping
            if (NODE_GET_TYPE(sublist) == NODE_TYPE_LIST)
            {
                sublist = NODE_MAKE_LIST(NODE_GET_INDEX(sublist));
            }
            return result_ok(value_list(sublist));
        }
        
        // For Lexer OR NodeIterator with flat [ ] tokens: parse tokens until ]
        Node list;
        int err = parse_list(eval, &list);
        if (err != 0)
        {
            return result_error(err);
        }
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
            // Resolve the name once, from the atom's memo where there is one.
            // A paren-form user procedure call is not handled here; it falls
            // through to the grouping path and re-enters eval_primary, where
            // this same lookup is answered from the memo just written.
            WordBinding paren_binding = resolve_word(next);
            const Primitive *prim = paren_binding.prim;
            if (prim)
            {
                Node user_name_atom = paren_binding.atom;   // for error messages
                const char *user_name = paren_binding.name;
                
                advance(eval); // consume procedure name
                
                // For 0-arg primitives, check if followed by infix operator or )
                // This allows (xcor+3) to work like xcor+3, while (files "ext") still works
                if (prim->default_args == 0)
                {
                    Token after = peek(eval);
                    
                    // Check if next token is an infix operator
                    int bp = get_infix_bp(after.type);
                    if (bp != BP_NONE)
                    {
                        // Call the primitive with no args first
                        Result r = eval_call_primitive(eval, prim, 0, NULL);
                        if (r.status != RESULT_OK)
                        {
                            eval->paren_depth--;
                            return result_set_error_proc(r, user_name);
                        }
                        
                        // Continue with infix expression parsing
                        Result lhs = r;
                        while (true)
                        {
                            Token op = peek(eval);
                            int op_bp = get_infix_bp(op.type);

                            if (op_bp == BP_NONE)
                                break;

                            advance(eval);

                            Result rhs = eval_expr_bp(eval, op_bp + 1);
                            if (rhs.status != RESULT_OK)
                            {
                                eval->paren_depth--;
                                return rhs;
                            }

                            lhs = apply_binary_op(op.type, lhs.value, rhs.value);
                            if (lhs.status != RESULT_OK)
                            {
                                eval->paren_depth--;
                                return lhs;
                            }
                        }

                        // Consume closing paren
                        Token closing = peek(eval);
                        if (closing.type == TOKEN_RIGHT_PAREN)
                        {
                            advance(eval);
                        }
                        eval->paren_depth--;
                        return lhs;
                    }
                    // Otherwise fall through to normal greedy arg collection
                }
                
                // Greedily collect all arguments until )
                Value args[MAX_PRIM_ARGS];
                int argc = 0;

                // Speculative OP_PRIM_CALL for deferred expression handling
                EvalOp *prim_staging_paren = NULL;
                int depth_before_prim_paren = op_stack_depth(eval->op_stack);
                if (eval->proc_depth > 0)
                {
                    prim_staging_paren = op_stack_push(eval->op_stack);
                    if (!prim_staging_paren)
                    {
                        eval->paren_depth--;
                        return result_error(ERR_STACK_OVERFLOW);
                    }
                    prim_staging_paren->kind = OP_PRIM_CALL;
                    prim_staging_paren->flags = OP_FLAG_NONE;
                    prim_staging_paren->result = result_none();
                    prim_staging_paren->prim_call.prim = prim;
                    prim_staging_paren->prim_call.user_name = user_name;
                    prim_staging_paren->prim_call.arg_base = op_stack_alloc_prim_args(eval->op_stack, MAX_PRIM_ARGS);
                    prim_staging_paren->prim_call.arg_capacity = MAX_PRIM_ARGS;
                    if (prim_staging_paren->prim_call.arg_base < 0)
                    {
                        op_stack_pop(eval->op_stack);
                        eval->paren_depth--;
                        return result_error(ERR_STACK_OVERFLOW);
                    }
                    prim_staging_paren->prim_call.argc = 0;
                    prim_staging_paren->prim_call.total_args = -1; // varargs
                    prim_staging_paren->prim_call.current_arg = 0;
                    // Paren-form calls are never the output/op tail-position
                    // exception; arg evaluation is not in tail position.
                    prim_staging_paren->prim_call.saved_in_tail_position = false;
                }
                
                // Track that we're collecting primitive args
                eval->primitive_arg_depth++;
                
                while (argc < MAX_PRIM_ARGS)
                {
                    Token t = peek(eval);
                    if (t.type == TOKEN_RIGHT_PAREN || t.type == TOKEN_EOF)
                        break;
                    
                    // Arguments to primitives are not in tail position
                    bool old_tail = eval->in_tail_position;
                    eval->in_tail_position = false;
                    Result arg = eval_expression(eval);
                    eval->in_tail_position = old_tail;

                    // Check if expression was deferred
                    if (arg.status == RESULT_NONE &&
                        prim_staging_paren &&
                        op_stack_depth(eval->op_stack) > depth_before_prim_paren + 1)
                    {
                        Value *staged_args = op_stack_get_prim_args(eval->op_stack,
                            prim_staging_paren->prim_call.arg_base);
                        if (!staged_args)
                        {
                            eval->primitive_arg_depth--;
                            return result_error(ERR_STACK_OVERFLOW);
                        }
                        for (int j = 0; j < argc; j++)
                            staged_args[j] = args[j];
                        prim_staging_paren->prim_call.argc = argc;
                        prim_staging_paren->prim_call.current_arg = argc;
                        eval->primitive_arg_depth--;
                        // Don't decrement paren_depth — the closing ) hasn't been consumed
                        // and will be handled by step_prim_call
                        return result_none();
                    }
                    
                    if (arg.status == RESULT_ERROR)
                    {
                        if (prim_staging_paren) op_stack_pop(eval->op_stack);
                        eval->primitive_arg_depth--;
                        eval->paren_depth--;
                        return result_set_error_proc(arg, user_name);
                    }
                    if (arg.status != RESULT_OK)
                        break;
                    args[argc++] = arg.value;
                }
                
                eval->primitive_arg_depth--;
                
                // All args collected synchronously — remove speculative OP_PRIM_CALL
                if (prim_staging_paren)
                    op_stack_pop(eval->op_stack);
                
                // Consume closing paren
                Token closing = peek(eval);
                if (closing.type == TOKEN_RIGHT_PAREN)
                {
                    advance(eval);
                }
                eval->paren_depth--;
                
                // Call primitive and set error_proc if needed
                Result r = eval_call_primitive(eval, prim, argc, args);
                return result_set_error_proc(r, user_name);
            }
        }
        
        // Not a procedure call, just grouping.
        // A parenthesized subexpression is not in tail position — its
        // result may feed into an outer infix expression.
        bool old_tail = eval->in_tail_position;
        eval->in_tail_position = false;
        int depth_before_group = op_stack_depth(eval->op_stack);
        Result r = eval_expr_bp(eval, BP_NONE);
        eval->in_tail_position = old_tail;

        // The group deferred a user procedure call to the trampoline, so its
        // value is not known yet and the tokens up to the closing ) have not
        // been consumed. Park the closing ) on the op stack, below everything
        // the deferral pushed, and let it run once the group has its value.
        if (r.status == RESULT_NONE &&
            op_stack_depth(eval->op_stack) > depth_before_group)
        {
            EvalOp *group_op = op_stack_insert(eval->op_stack, depth_before_group);
            if (!group_op)
                return result_error(ERR_STACK_OVERFLOW);
            group_op->kind = OP_PAREN_GROUP;
            group_op->flags = OP_FLAG_NONE;
            group_op->saved_source = eval->token_source;
            return result_none();
        }

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

        // Resolve the name once, from the atom's memo where there is one
        WordBinding binding = resolve_word(t);
        const Primitive *prim = binding.prim;
        if (prim)
        {
            Node user_name_atom = binding.atom;      // for error messages
            const char *user_name = binding.name;

            advance(eval);
            // Collect default number of arguments
            Value args[MAX_PRIM_ARGS];
            int argc = 0;
            Node gc_roots[MAX_PRIM_ARGS + 1];
            size_t gc_root_count = 1;
            gc_roots[0] = user_name_atom;
            MemGcRootScope gc_scope;
            mem_gc_roots_push(&gc_scope, gc_roots, gc_root_count);

            // When inside a procedure, speculatively push OP_PRIM_CALL.
            // If an arg expression defers (user proc call pushed OP_PROC_CALL),
            // OP_PRIM_CALL is already in the correct stack position below.
            // If all args are collected synchronously, we pop it and call directly.
            EvalOp *prim_staging = NULL;
            int depth_before_prim = op_stack_depth(eval->op_stack);
            if (eval->proc_depth > 0 && prim->default_args > 0 &&
                prim->default_args <= MAX_PRIM_STAGED_ARGS)
            {
                prim_staging = op_stack_push(eval->op_stack);
                if (!prim_staging)
                {
                    mem_gc_roots_pop(&gc_scope);
                    return result_error(ERR_STACK_OVERFLOW);
                }
                prim_staging->kind = OP_PRIM_CALL;
                prim_staging->flags = OP_FLAG_NONE;
                prim_staging->result = result_none();
                prim_staging->prim_call.prim = prim;
                prim_staging->prim_call.user_name = user_name;
                prim_staging->prim_call.arg_base = -1;
                prim_staging->prim_call.arg_capacity = MAX_PRIM_STAGED_ARGS;
                prim_staging->prim_call.argc = 0;
                prim_staging->prim_call.total_args = prim->default_args;
                prim_staging->prim_call.current_arg = 0;
                // Default to false; the deferral check below overwrites this
                // with the per-arg value (true for output/op, false otherwise)
                // so step_prim_call can preserve the output/op tail-position
                // exception when collecting subsequent args.
                prim_staging->prim_call.saved_in_tail_position = false;
            }

            // Track that we're collecting primitive args
            eval->primitive_arg_depth++;

            for (int i = 0; i < prim->default_args && !eval_at_end(eval); i++)
            {
                // Check for tokens that would end args
                Token next = peek(eval);
                if (next.type == TOKEN_RIGHT_PAREN || next.type == TOKEN_RIGHT_BRACKET)
                {
                    break;
                }

                // Arguments to primitives are not in tail position.
                // EXCEPTION: output/op ALWAYS puts its argument in tail
                // position when inside a procedure, because output terminates
                // the procedure regardless of where it appears (e.g. inside
                // an if branch that isn't on the last body line).
                bool is_output_prim = primitive_is_output(prim);
                bool old_tail = eval->in_tail_position;
                if (is_output_prim && eval->proc_depth > 0)
                    eval->in_tail_position = true;
                else
                    eval->in_tail_position = false;
                Result arg = eval_expression(eval);
                eval->in_tail_position = old_tail;

                // Check if expression was deferred (user proc call on op stack)
                if (arg.status == RESULT_NONE &&
                    op_stack_depth(eval->op_stack) > depth_before_prim + 1 &&
                    prim_staging)
                {
                    // Save collected args so far into OP_PRIM_CALL
                    for (int j = 0; j < argc; j++)
                        prim_staging->prim_call.args[j] = args[j];
                    prim_staging->prim_call.argc = argc;
                    prim_staging->prim_call.current_arg = i;
                    // Capture per-arg tail position so step_prim_call can
                    // restore it when collecting subsequent args. For output/op
                    // this preserves the tail-position exception across the
                    // deferred resume; for everything else it stays false.
                    prim_staging->prim_call.saved_in_tail_position =
                        (is_output_prim && eval->proc_depth > 0);
                    eval->primitive_arg_depth--;
                    mem_gc_roots_pop(&gc_scope);
                    return result_none();
                }
                
                // Propagate errors and control flow (throw, stop, output)
                if (arg.status == RESULT_ERROR || arg.status == RESULT_THROW ||
                    arg.status == RESULT_STOP || arg.status == RESULT_OUTPUT)
                {
                    if (prim_staging) op_stack_pop(eval->op_stack);
                    eval->primitive_arg_depth--;
                    mem_gc_roots_pop(&gc_scope);
                    return result_set_error_proc(arg, user_name);
                }
                if (arg.status != RESULT_OK)
                {
                    if (prim_staging) op_stack_pop(eval->op_stack);
                    eval->primitive_arg_depth--;
                    mem_gc_roots_pop(&gc_scope);
                    return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_name, NULL);
                }
                args[argc++] = arg.value;
                if (arg.value.type == VALUE_WORD || arg.value.type == VALUE_LIST)
                {
                    gc_roots[gc_root_count++] = arg.value.as.node;
                    gc_scope.count = gc_root_count;
                }
            }

            eval->primitive_arg_depth--;

            // All args collected synchronously — remove speculative OP_PRIM_CALL
            if (prim_staging)
                op_stack_pop(eval->op_stack);

            if (argc < prim->default_args)
            {
                mem_gc_roots_pop(&gc_scope);
                return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_name, NULL);
            }

            // Call primitive and set error_proc if needed
            Result r = eval_call_primitive(eval, prim, argc, args);
            mem_gc_roots_pop(&gc_scope);
            return result_set_error_proc(r, user_name);
        }

        // Check for user-defined procedure
        UserProcedure *user_proc = binding.proc;
        if (user_proc)
        {
            advance(eval);
            // Collect arguments for user procedure
            Value args[MAX_PROC_PARAMS];
            int argc = 0;
            Node gc_roots[MAX_PROC_PARAMS];
            size_t gc_root_count = 0;
            MemGcRootScope gc_scope;
            mem_gc_roots_push(&gc_scope, gc_roots, gc_root_count);

            // Track that we're collecting user proc args (blocks deferral
            // for nested proc calls — only OP_PRIM_CALL can handle deferrals)
            eval->user_arg_depth++;

            for (int i = 0; i < user_proc->param_count && !eval_at_end(eval); i++)
            {
                // Check for tokens that would end args
                Token next = peek(eval);
                if (next.type == TOKEN_RIGHT_PAREN || next.type == TOKEN_RIGHT_BRACKET)
                {
                    break;
                }

                // Arguments are not in tail position
                bool old_tail = eval->in_tail_position;
                eval->in_tail_position = false;
                Result arg = eval_expression(eval);
                eval->in_tail_position = old_tail;
                
                // Propagate errors and control flow (throw, stop, output)
                if (arg.status == RESULT_ERROR || arg.status == RESULT_THROW ||
                    arg.status == RESULT_STOP || arg.status == RESULT_OUTPUT)
                {
                    eval->user_arg_depth--;
                    mem_gc_roots_pop(&gc_scope);
                    return arg;
                }
                if (arg.status != RESULT_OK)
                {
                    eval->user_arg_depth--;
                    mem_gc_roots_pop(&gc_scope);
                    return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_proc->name, NULL);
                }
                args[argc++] = arg.value;
                if (arg.value.type == VALUE_WORD || arg.value.type == VALUE_LIST)
                {
                    gc_roots[gc_root_count++] = arg.value.as.node;
                    gc_scope.count = gc_root_count;
                }
            }

            eval->user_arg_depth--;

            if (argc < user_proc->param_count)
            {
                mem_gc_roots_pop(&gc_scope);
                return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_proc->name, NULL);
            }

            // Tail call optimization: if we're in tail position inside a procedure,
            // and this is a SELF-RECURSIVE call, set up a tail call for frame reuse.
            if (eval->in_tail_position && eval->proc_depth > 0)
            {
                // Check if this is a self-recursive call
                FrameStack *frames = eval->frames;
                if (frames && !frame_stack_is_empty(frames))
                {
                    FrameHeader *current_frame = frame_current(frames);
                    if (current_frame && current_frame->proc == user_proc)
                    {
                        // Self-recursive tail call - set up TCO.
                        // TailCall.args is fixed-size (MAX_PROC_PARAMS); a
                        // mismatch between the call's argc and the procedure's
                        // declared param_count would silently corrupt the
                        // tail-call buffer, so refuse TCO if the invariant
                        // does not hold and fall through to a regular call.
                        if (argc == user_proc->param_count &&
                            argc >= 0 && argc <= MAX_PROC_PARAMS)
                        {
                            TailCall *tc = proc_get_tail_call();
                            tc->is_tail_call = true;
                            tc->is_output_call = (eval->primitive_arg_depth > 0);
                            tc->proc_name = user_proc->name;
                            tc->arg_count = argc;
                            for (int i = 0; i < argc; i++)
                            {
                                tc->args[i] = args[i];
                            }
                            // Return RESULT_STOP — step_proc_call handles TCO
                            mem_gc_roots_pop(&gc_scope);
                            return result_stop();
                        }
                        // Invariant violated: fall through to non-TCO path.
                    }
                }
                // Non-self-recursive tail call - fall through to op stack
            }

            // Inside a procedure and not collecting user proc args: push
            // OP_PROC_CALL on the op stack. The trampoline handles it
            // asynchronously.  When inside a primitive's arg expression,
            // OP_PRIM_CALL (speculatively pushed) catches the deferral.
            // When inside a user proc's arg collection (user_arg_depth > 0)
            // we fall through to the synchronous sub-trampoline since user
            // proc arg collection doesn't have OP_PRIM_CALL support.
            if (eval->proc_depth > 0 && eval->user_arg_depth == 0)
            {
                // Any following ')' stays in the stream: it belongs to the
                // paren that opened this group, and OP_PAREN_GROUP consumes
                // it once the deferred call has produced its value.

                // Push frame
                word_offset_t frame_offset = frame_push(eval->frames, user_proc, args, argc);
                if (frame_offset == OFFSET_NONE)
                {
                    mem_gc_roots_pop(&gc_scope);
                    return result_error(ERR_OUT_OF_SPACE);
                }
                eval->proc_depth++;
                proc_push_current(user_proc->name);
                proc_clear_tail_call();

                // Push OP_PROC_CALL
                EvalOp *call_op = op_stack_push(eval->op_stack);
                if (!call_op)
                {
                    eval->proc_depth--;
                    proc_pop_current();
                    frame_pop(eval->frames);
                    mem_gc_roots_pop(&gc_scope);
                    return result_error(ERR_STACK_OVERFLOW);
                }
                call_op->kind = OP_PROC_CALL;
                call_op->flags = OP_FLAG_NONE;
                call_op->proc_call.proc = user_proc;
                call_op->proc_call.current_line = user_proc->body;
                call_op->proc_call.phase = 0;
                call_op->proc_call.tco_mode = TCO_MODE_NONE;
                mem_gc_roots_pop(&gc_scope);
                return result_none();
            }

            // At top-level or inside primitive args: synchronous call via sub-trampoline
            Result r = eval_push_proc_call(eval, user_proc, argc, args);
            mem_gc_roots_pop(&gc_scope);
            return r;
        }

        // Unknown procedure - intern the name so pointer persists
        Node name_atom = mem_atom(t.start, t.length);
        return result_error_arg(ERR_DONT_KNOW_HOW, mem_word_ptr(name_atom), NULL);
    }

    case TOKEN_RIGHT_PAREN:
        return result_error(ERR_PAREN_MISMATCH);

    case TOKEN_RIGHT_BRACKET:
        return result_error(ERR_BRACKET_MISMATCH);

    case TOKEN_EOF:
        return result_error(ERR_NOT_ENOUGH_INPUTS);

    default:
    {
        // Intern the token text so the pointer persists
        Node token_atom = mem_atom(t.start, t.length);
        return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, mem_word_ptr(token_atom));
    }
    }
}

// Pratt parser for expressions with infix operators
// Iterative Pratt parser for infix expressions.
// Uses an explicit operator stack instead of C recursion, so that when
// eval_primary defers a user procedure call (pushes OP_PROC_CALL and returns
// result_none()), we can save our state to OP_EXPR_EVAL on the op stack and
// yield to the trampoline instead of blocking on the C stack.
Result LOGO_HOT(eval_expr_bp)(Evaluator *eval, int min_bp)
{
    PendingBinOp op_stack[MAX_EXPR_OPS];
    int depth = 0;
    int depth_before_primary = op_stack_depth(eval->op_stack);

    Result lhs = eval_primary(eval);

    // If eval_primary pushed a deferred proc call, save expression state.
    // Always push OP_EXPR_EVAL so the infix loop can resume after the
    // deferred call completes — there may be infix operators following
    // the primary expression (e.g. (f :x) + (g :y)).
    if (lhs.status == RESULT_NONE && op_stack_depth(eval->op_stack) > depth_before_primary)
    {
        // Insert OP_EXPR_EVAL below every op the primary pushed, so the
        // value of the primary flows back into this expression.
        // Stack order: ... → OP_EXPR_EVAL → ... → OP_PROC_CALL (top)
        EvalOp *expr_op = op_stack_insert(eval->op_stack, depth_before_primary);
        if (!expr_op)
            return result_error(ERR_STACK_OVERFLOW);
        expr_op->kind = OP_EXPR_EVAL;
        expr_op->flags = OP_FLAG_NONE;
        expr_op->saved_source = eval->token_source;
        expr_op->expr_eval.depth = depth;
        expr_op->expr_eval.min_bp = min_bp;
        expr_op->expr_eval.phase = 0;
        return result_none();
    }

    if (lhs.status != RESULT_OK)
        return lhs;

    for (;;)
    {
        Token op_tok = peek(eval);
        int bp = get_infix_bp(op_tok.type);

        // Reduce: apply pending operators with binding power > current
        while ((bp == BP_NONE || bp < min_bp) && depth > 0)
        {
            depth--;
            Result r = apply_binary_op(op_stack[depth].op_type, op_stack[depth].left, lhs.value);
            if (r.status != RESULT_OK)
                return r;
            lhs = r;
            min_bp = op_stack[depth].min_bp;

            // Re-check with restored binding power
            op_tok = peek(eval);
            bp = get_infix_bp(op_tok.type);
        }

        if (bp == BP_NONE || bp < min_bp)
            break;

        // In a binary expression: operands are not in tail position.
        // This prevents TCO from firing for patterns like
        // "output :n * factorial :n - 1" where the self-call result
        // is needed for the multiplication.
        eval->in_tail_position = false;

        // Shift: save current state and parse right operand
        advance(eval);
        if (depth >= MAX_EXPR_OPS)
            return result_error(ERR_STACK_OVERFLOW);
        op_stack[depth].left = lhs.value;
        op_stack[depth].op_type = (uint8_t)op_tok.type;
        op_stack[depth].min_bp = min_bp;
        depth++;
        min_bp = bp + 1;

        depth_before_primary = op_stack_depth(eval->op_stack);
        lhs = eval_primary(eval);

        // If eval_primary pushed a deferred proc call, save expression state
        if (lhs.status == RESULT_NONE && op_stack_depth(eval->op_stack) > depth_before_primary)
        {
            // Insert OP_EXPR_EVAL below every op the primary pushed.
            EvalOp *expr_op = op_stack_insert(eval->op_stack, depth_before_primary);
            if (!expr_op)
                return result_error(ERR_STACK_OVERFLOW);
            expr_op->kind = OP_EXPR_EVAL;
            expr_op->flags = OP_FLAG_NONE;
            expr_op->saved_source = eval->token_source;
            expr_op->expr_eval.depth = depth;
            expr_op->expr_eval.min_bp = min_bp;
            expr_op->expr_eval.phase = 0;
            for (int i = 0; i < depth; i++)
                expr_op->expr_eval.ops[i] = op_stack[i];
            return result_none();
        }

        if (lhs.status != RESULT_OK)
            return lhs;
    }

    return lhs;
}

Result eval_expression(Evaluator *eval)
{
    return eval_expr_bp(eval, BP_NONE);
}
