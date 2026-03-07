//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Expression parser: Pratt parser, eval_primary, eval_expr_bp.
//

#include "eval_internal.h"
#include "error.h"
#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "frame.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Apply a binary infix operator to two values.
// Returns RESULT_OK with the computed value, or RESULT_ERROR on type/divide errors.
Result apply_binary_op(TokenType op_type, Value left, Value right)
{
    // Handle = separately since it works with all value types
    if (op_type == TOKEN_EQUALS)
    {
        bool equal = values_equal(left, right);
        return result_ok(value_word(mem_atom_cstr(equal ? "true" : "false")));
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
        return result_ok(value_word(mem_atom_cstr((left_n < right_n) ? "true" : "false")));
    case TOKEN_GREATER_THAN:
        return result_ok(value_word(mem_atom_cstr((left_n > right_n) ? "true" : "false")));
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
            // Keep the quote - it's needed when the list is run
            item = mem_atom(t.start, t.length);
            advance(eval);
        }
        else if (t.type == TOKEN_COLON)
        {
            // Store :var as-is in list (will be evaluated when run)
            // Create a word with the colon
            item = mem_atom(t.start, t.length);
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
Result eval_primary(Evaluator *eval)
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
                // Intern the user's name for error messages
                Node user_name_atom = mem_atom(next.start, next.length);
                const char *user_name = mem_word_ptr(user_name_atom);
                
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
                        Result r = prim->func(eval, 0, NULL);
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
                            
                            // Handle = separately since it works with all value types
                            if (op.type == TOKEN_EQUALS)
                            {
                                bool equal = values_equal(lhs.value, rhs.value);
                                lhs = result_ok(value_word(mem_atom_cstr(equal ? "true" : "false")));
                                continue;
                            }
                            
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
                            case TOKEN_LESS_THAN: op_name = "<"; break;
                            case TOKEN_GREATER_THAN: op_name = ">"; break;
                            default: op_name = "?"; break;
                            }
                            
                            if (!left_ok)
                            {
                                eval->paren_depth--;
                                return result_error_arg(ERR_DOESNT_LIKE_INPUT, op_name, value_to_string(lhs.value));
                            }
                            if (!right_ok)
                            {
                                eval->paren_depth--;
                                return result_error_arg(ERR_DOESNT_LIKE_INPUT, op_name, value_to_string(rhs.value));
                            }
                            
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
                                {
                                    eval->paren_depth--;
                                    return result_error(ERR_DIVIDE_BY_ZERO);
                                }
                                result = left_n / right_n;
                                break;
                            case TOKEN_LESS_THAN:
                                lhs = result_ok(value_word(mem_atom_cstr((left_n < right_n) ? "true" : "false")));
                                continue;
                            case TOKEN_GREATER_THAN:
                                lhs = result_ok(value_word(mem_atom_cstr((left_n > right_n) ? "true" : "false")));
                                continue;
                            default:
                                eval->paren_depth--;
                                return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, op_name);
                            }
                            
                            lhs = result_ok(value_number(result));
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
                Value args[16];
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
                    prim_staging_paren->prim_call.argc = 0;
                    prim_staging_paren->prim_call.total_args = -1; // varargs
                    prim_staging_paren->prim_call.current_arg = 0;
                }
                
                // Track that we're collecting primitive args
                eval->primitive_arg_depth++;
                
                while (argc < 16)
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
                        for (int j = 0; j < argc && j < MAX_PRIM_STAGED_ARGS; j++)
                            prim_staging_paren->prim_call.args[j] = args[j];
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
                Result r = prim->func(eval, argc, args);
                return result_set_error_proc(r, user_name);
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
            // Intern the user's name for error messages
            Node user_name_atom = mem_atom(t.start, t.length);
            const char *user_name = mem_word_ptr(user_name_atom);
            
            advance(eval);
            // Collect default number of arguments
            Value args[16]; // Max args
            int argc = 0;

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
                    return result_error(ERR_STACK_OVERFLOW);
                prim_staging->kind = OP_PRIM_CALL;
                prim_staging->flags = OP_FLAG_NONE;
                prim_staging->result = result_none();
                prim_staging->prim_call.prim = prim;
                prim_staging->prim_call.user_name = user_name;
                prim_staging->prim_call.argc = 0;
                prim_staging->prim_call.total_args = prim->default_args;
                prim_staging->prim_call.current_arg = 0;
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

                // Arguments to primitives are not in tail position
                bool old_tail = eval->in_tail_position;
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
                    eval->primitive_arg_depth--;
                    return result_none();
                }
                
                // Propagate errors and control flow (throw, stop, output)
                if (arg.status == RESULT_ERROR || arg.status == RESULT_THROW ||
                    arg.status == RESULT_STOP || arg.status == RESULT_OUTPUT)
                {
                    if (prim_staging) op_stack_pop(eval->op_stack);
                    eval->primitive_arg_depth--;
                    return result_set_error_proc(arg, user_name);
                }
                if (arg.status != RESULT_OK)
                {
                    if (prim_staging) op_stack_pop(eval->op_stack);
                    eval->primitive_arg_depth--;
                    return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_name, NULL);
                }
                args[argc++] = arg.value;
            }

            eval->primitive_arg_depth--;

            // All args collected synchronously — remove speculative OP_PRIM_CALL
            if (prim_staging)
                op_stack_pop(eval->op_stack);

            if (argc < prim->default_args)
            {
                return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_name, NULL);
            }

            // Call primitive and set error_proc if needed
            Result r = prim->func(eval, argc, args);
            return result_set_error_proc(r, user_name);
        }

        // Check for user-defined procedure
        UserProcedure *user_proc = proc_find(name_buf);
        if (user_proc)
        {
            advance(eval);
            // Collect arguments for user procedure
            Value args[MAX_PROC_PARAMS];
            int argc = 0;

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
                    return arg;
                }
                if (arg.status != RESULT_OK)
                {
                    eval->user_arg_depth--;
                    return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_proc->name, NULL);
                }
                args[argc++] = arg.value;
            }

            eval->user_arg_depth--;

            if (argc < user_proc->param_count)
            {
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
                        // Self-recursive tail call - set up TCO
                        TailCall *tc = proc_get_tail_call();
                        tc->is_tail_call = true;
                        tc->proc_name = user_proc->name;
                        tc->arg_count = argc;
                        for (int i = 0; i < argc; i++)
                        {
                            tc->args[i] = args[i];
                        }
                        // Return RESULT_STOP — step_proc_call handles TCO
                        return result_stop();
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
                // Consume closing paren of (proc args) call before deferring
                // so that the saved token source is past the ')' and the
                // infix expression parser can see operators that follow,
                // e.g. (f :x) + (g :y).  Don't decrement paren_depth here;
                // the TOKEN_LEFT_PAREN "grouping" handler does that.
                {
                    Token closing = peek(eval);
                    if (closing.type == TOKEN_RIGHT_PAREN)
                    {
                        advance(eval);
                    }
                }
                // Push frame
                word_offset_t frame_offset = frame_push(eval->frames, user_proc, args, argc);
                if (frame_offset == OFFSET_NONE)
                    return result_error(ERR_OUT_OF_SPACE);
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
                    return result_error(ERR_STACK_OVERFLOW);
                }
                call_op->kind = OP_PROC_CALL;
                call_op->flags = OP_FLAG_NONE;
                call_op->proc_call.proc = user_proc;
                call_op->proc_call.current_line = user_proc->body;
                call_op->proc_call.phase = 0;
                return result_none();
            }

            // At top-level or inside primitive args: synchronous call via sub-trampoline
            return eval_push_proc_call(eval, user_proc, argc, args);
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
Result eval_expr_bp(Evaluator *eval, int min_bp)
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
        // Push OP_EXPR_EVAL above the OP_PROC_CALL, then swap so it's below.
        // Stack order: ... → OP_EXPR_EVAL → OP_PROC_CALL (top)
        // Trampoline runs OP_PROC_CALL first, result flows to OP_EXPR_EVAL.
        EvalOp *expr_op = op_stack_push(eval->op_stack);
        if (!expr_op)
            return result_error(ERR_STACK_OVERFLOW);
        expr_op->kind = OP_EXPR_EVAL;
        expr_op->flags = OP_FLAG_NONE;
        expr_op->saved_source = eval->token_source;
        expr_op->result = result_none();
        expr_op->expr_eval.depth = depth;
        expr_op->expr_eval.min_bp = min_bp;
        expr_op->expr_eval.phase = 0;
        op_stack_swap_top(eval->op_stack);
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
            // Push OP_EXPR_EVAL above the OP_PROC_CALL, then swap so it's below.
            EvalOp *expr_op = op_stack_push(eval->op_stack);
            if (!expr_op)
                return result_error(ERR_STACK_OVERFLOW);
            expr_op->kind = OP_EXPR_EVAL;
            expr_op->flags = OP_FLAG_NONE;
            expr_op->saved_source = eval->token_source;
            expr_op->result = result_none();
            expr_op->expr_eval.depth = depth;
            expr_op->expr_eval.min_bp = min_bp;
            expr_op->expr_eval.phase = 0;
            for (int i = 0; i < depth; i++)
                expr_op->expr_eval.ops[i] = op_stack[i];
            op_stack_swap_top(eval->op_stack);
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
