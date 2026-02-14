//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "eval.h"
#include "eval_ops.h"
#include "error.h"
#include "primitives.h"
#include "procedures.h"
#include "repl.h"
#include "variables.h"
#include "format.h"
#include "frame.h"
#include "devices/io.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Global operation stack (shared by all evaluators)
static OpStack global_op_stack;

// Binding powers for Pratt parser
#define BP_NONE 0
#define BP_COMPARISON 10      // = < >
#define BP_ADDITIVE 20        // + -
#define BP_MULTIPLICATIVE 30  // * /

// Forward declarations
static Result eval_expr_bp(Evaluator *eval, int min_bp);
static Result step_expr_eval(Evaluator *eval, EvalOp *op);
static Result step_prim_call(Evaluator *eval, EvalOp *op);

// Apply a binary infix operator to two values.
// Returns RESULT_OK with the computed value, or RESULT_ERROR on type/divide errors.
static Result apply_binary_op(TokenType op_type, Value left, Value right)
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
static Result eval_primary(Evaluator *eval);

void eval_init(Evaluator *eval, Lexer *lexer)
{
    token_source_init_lexer(&eval->token_source, lexer);
    eval->frames = NULL;  // Caller sets this if needed
    eval->op_stack = &global_op_stack;
    // Only re-init if the stack is empty — a nested REPL (e.g. pause) must
    // not destroy ops that belong to the parent evaluation.
    if (op_stack_is_empty(eval->op_stack))
        op_stack_init(eval->op_stack);
    eval->paren_depth = 0;
    eval->error_code = 0;
    eval->error_context = NULL;
    eval->in_tail_position = false;
    eval->proc_depth = 0;
    eval->repcount = -1;
    eval->primitive_arg_depth = 0;
    eval->user_arg_depth = 0;
}

void eval_set_frames(Evaluator *eval, FrameStack *frames)
{
    eval->frames = frames;
}

FrameStack *eval_get_frames(Evaluator *eval)
{
    return eval->frames;
}

bool eval_in_procedure(Evaluator *eval)
{
    return eval->frames != NULL && !frame_stack_is_empty(eval->frames);
}

// Get current token, fetching if needed
static Token peek(Evaluator *eval)
{
    return token_source_peek(&eval->token_source);
}

// Consume current token
static void advance(Evaluator *eval)
{
    // Consume the peeked token by getting next
    token_source_next(&eval->token_source);
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
static Result eval_expr_bp(Evaluator *eval, int min_bp)
{
    PendingBinOp op_stack[MAX_EXPR_OPS];
    int depth = 0;
    int depth_before_primary = op_stack_depth(eval->op_stack);

    Result lhs = eval_primary(eval);

    // If eval_primary pushed a deferred proc call, save expression state
    if (lhs.status == RESULT_NONE && op_stack_depth(eval->op_stack) > depth_before_primary)
    {
        // When depth == 0 (no pending infix operators), OP_EXPR_EVAL would be
        // a no-op pass-through, so skip it and save an op stack slot per level.
        if (depth == 0)
            return result_none();

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
        expr_op->expr_eval.depth = depth;  // 0 — no pending ops yet
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

Result eval_instruction(Evaluator *eval)
{
    // Check for user interrupt at the start of each instruction
    LogoIO *io = primitives_get_io();
    if (io && logo_io_check_user_interrupt(io))
    {
        return result_error(ERR_STOPPED);
    }

    // Check for freeze request (F4 key) - temporarily pause until key pressed
    if (io && logo_io_check_freeze_request(io))
    {
        // Wait for any key to be pressed (except F4)
        // During freeze: F9 triggers pause, Brk stops execution, F4 is ignored
        bool freeze_done = false;
        while (!freeze_done)
        {
            // Check for user interrupt (Brk) while waiting
            if (logo_io_check_user_interrupt(io))
            {
                return result_error(ERR_STOPPED);
            }
            // Check for pause request (F9) while waiting
            if (logo_io_check_pause_request(io))
            {
                const char *proc_name = proc_get_current();
                if (proc_name != NULL && io->console)
                {
                    logo_io_clear_pause_request(io);
                    logo_io_console_write_line(io, "Pausing...");
                    
                    ReplState state;
                    if (repl_init(&state, io, REPL_FLAGS_PAUSE, proc_name))
                    {
                        Result r = repl_run(&state);
                        repl_cleanup(&state);
                        
                        if (r.status != RESULT_OK && r.status != RESULT_NONE)
                        {
                            return r;
                        }
                    }
                }
                else
                {
                    // Not in a procedure or no console — clear flag since we're
                    // already in a freeze; the flag isn't useful when deferred
                    logo_io_clear_pause_request(io);
                }
                // After pause REPL exits, continue execution
                freeze_done = true;
            }
            // Eat any additional F4 presses while frozen
            else if (logo_io_check_freeze_request(io))
            {
                // Just consume it and keep waiting
            }
            else if (logo_io_key_available(io))
            {
                // Consume the key that ended the freeze
                logo_io_read_char(io);
                freeze_done = true;
            }
            else
            {
                logo_io_sleep(io, 10);  // Small sleep to avoid busy waiting
            }
        }
    }

    // Check for pause request (F9 key) - only works inside a procedure
    if (io && logo_io_check_pause_request(io))
    {
        const char *proc_name = proc_get_current();
        if (proc_name != NULL && io->console)
        {
            // Clear the flag now that we're actually pausing
            logo_io_clear_pause_request(io);
            
            // Run the pause REPL - this blocks until co is called or throw "toplevel
            logo_io_console_write_line(io, "Pausing...");
            
            ReplState state;
            if (repl_init(&state, io, REPL_FLAGS_PAUSE, proc_name))
            {
                Result r = repl_run(&state);
                repl_cleanup(&state);
                
                // If pause REPL exited with throw or error, propagate it
                if (r.status != RESULT_OK && r.status != RESULT_NONE)
                {
                    return r;
                }
            }
            // Otherwise continue execution
        }
        // If not in a procedure, defer — flag stays set until we enter one
    }

    if (eval_at_end(eval))
    {
        return result_none();
    }

    Result r = eval_expression(eval);

    // If we got a value but this was at top level, it's an error
    // (unless it's from inside a run/repeat which handles this)
    return r;
}

// Serialize a list to a string buffer for evaluation
// Returns the number of characters written
// Forward declaration for recursive serialization
static int serialize_node_to_buffer(Node element, char *buffer, int max_len, bool needs_space);

static int serialize_list_to_buffer(Node list, char *buffer, int max_len)
{
    int pos = 0;
    Node node = list;
    
    while (!mem_is_nil(node) && pos < max_len - 5)
    {
        Node element = mem_car(node);
        
        int written = serialize_node_to_buffer(element, buffer + pos, max_len - pos, pos > 0);
        pos += written;
        
        node = mem_cdr(node);
    }
    buffer[pos] = '\0';
    return pos;
}

// Serialize a single node (word or list) to buffer
static int serialize_node_to_buffer(Node element, char *buffer, int max_len, bool needs_space)
{
    int pos = 0;
    
    if (mem_is_word(element))
    {
        const char *str = mem_word_ptr(element);
        size_t len = mem_word_len(element);
        
        if (pos + (int)len + 1 < max_len)
        {
            if (needs_space)
                buffer[pos++] = ' ';
            memcpy(buffer + pos, str, len);
            pos += len;
        }
    }
    else if (mem_is_nil(element))
    {
        // Empty list []
        if (needs_space)
            buffer[pos++] = ' ';
        buffer[pos++] = '[';
        buffer[pos++] = ']';
    }
    else if (mem_is_list(element))
    {
        // Serialize nested list recursively
        if (needs_space)
            buffer[pos++] = ' ';
        buffer[pos++] = '[';
        
        // Iterate through inner list elements
        Node inner = element;
        bool first_in_list = true;
        while (!mem_is_nil(inner) && pos < max_len - 3)
        {
            Node inner_elem = mem_car(inner);
            int written = serialize_node_to_buffer(inner_elem, buffer + pos, max_len - pos, !first_in_list);
            pos += written;
            first_in_list = false;
            inner = mem_cdr(inner);
        }
        buffer[pos++] = ']';
    }
    
    return pos;
}

// Skip/peek helpers for tail-call lookahead (no execution)
// Forward declarations
static bool skip_primary(Evaluator *eval);
static bool skip_expr_bp(Evaluator *eval, int min_bp);
static bool skip_instruction(Evaluator *eval);

// Skip a single primary expression (literal, variable, list, or parens)
static bool skip_primary(Evaluator *eval)
{
    Token t = peek(eval);

    switch (t.type)
    {
    case TOKEN_NUMBER:
    case TOKEN_QUOTED:
    case TOKEN_COLON:
        advance(eval);
        return true;

    case TOKEN_WORD:
    {
        // For words, we need to check if it's a procedure and skip its args
        // But this is called from skip_expr_bp for arguments, where the word
        // might be a variable reference or just a literal in context.
        // For primitive args, we just need to skip one expression.
        
        // Check if it's a number (self-quoting)
        if (is_number_string(t.start, t.length))
        {
            advance(eval);
            return true;
        }
        
        // Look up as primitive or user proc
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
            // Skip the expected number of arguments
            for (int i = 0; i < prim->default_args && !eval_at_end(eval); i++)
            {
                Token next = peek(eval);
                if (next.type == TOKEN_RIGHT_PAREN || next.type == TOKEN_RIGHT_BRACKET)
                    break;
                if (!skip_expr_bp(eval, BP_NONE))
                    return false;
            }
            return true;
        }
        
        UserProcedure *user_proc = proc_find(name_buf);
        if (user_proc)
        {
            advance(eval);
            // Skip the expected number of arguments
            for (int i = 0; i < user_proc->param_count && !eval_at_end(eval); i++)
            {
                Token next = peek(eval);
                if (next.type == TOKEN_RIGHT_PAREN || next.type == TOKEN_RIGHT_BRACKET)
                    break;
                if (!skip_expr_bp(eval, BP_NONE))
                    return false;
            }
            return true;
        }
        
        // Unknown word - just skip it (might be undefined procedure)
        advance(eval);
        return true;
    }

    case TOKEN_LEFT_BRACKET:
    {
        advance(eval);
        // For NodeIterator, nested lists are atomic - check for pending sublist
        Node sublist = token_source_get_sublist(&eval->token_source);
        if (!mem_is_nil(sublist))
        {
            // This was a nested list node, consume it and we're done
            token_source_consume_sublist(&eval->token_source);
            return true;
        }
        // For Lexer (flat brackets), scan through contents
        while (true)
        {
            Token inner = peek(eval);
            if (inner.type == TOKEN_EOF)
                return false;
            if (inner.type == TOKEN_RIGHT_BRACKET)
            {
                advance(eval);
                break;
            }
            if (!skip_primary(eval))
                return false;
        }
        return true;
    }    case TOKEN_LEFT_PAREN:
        advance(eval);
        // Skip contents of parens - could be grouped expr or varargs call
        while (true)
        {
            Token inner = peek(eval);
            if (inner.type == TOKEN_EOF)
                return false;
            if (inner.type == TOKEN_RIGHT_PAREN)
            {
                advance(eval);
                break;
            }
            if (!skip_expr_bp(eval, BP_NONE))
                return false;
        }
        return true;

    case TOKEN_MINUS:
    case TOKEN_UNARY_MINUS:
        advance(eval);
        return skip_primary(eval);

    default:
        return false;
    }
}

static bool skip_expr_bp(Evaluator *eval, int min_bp)
{
    if (!skip_primary(eval))
        return false;

    while (true)
    {
        Token op = peek(eval);
        int bp = get_infix_bp(op.type);
        if (bp == BP_NONE || bp < min_bp)
            break;

        advance(eval);
        if (!skip_expr_bp(eval, bp + 1))
            return false;
    }
    return true;
}

// Skip a complete instruction (used for TCO lookahead)
// This mirrors eval_instruction/eval_expression logic
static bool skip_instruction(Evaluator *eval)
{
    if (eval_at_end(eval))
        return false;
    
    return skip_expr_bp(eval, BP_NONE);
}

// Find position in buffer after a label instruction with given name
// Returns -1 if not found
// Scans for pattern: label "labelname or label "labelname (with any whitespace)
static int find_label_position(const char *buffer, const char *label_name)
{
    const char *pos = buffer;
    while (*pos)
    {
        // Skip whitespace
        while (*pos && isspace((unsigned char)*pos))
            pos++;
        
        if (!*pos)
            break;
        
        // Check for "label" keyword (case-insensitive)
        if (strncasecmp(pos, "label", 5) == 0 && 
            (isspace((unsigned char)pos[5]) || pos[5] == '"'))
        {
            const char *after_label = pos + 5;
            
            // Skip whitespace between label and its argument
            while (*after_label && isspace((unsigned char)*after_label))
                after_label++;
            
            // Check for quoted word: "labelname
            if (*after_label == '"')
            {
                after_label++; // skip quote
                const char *name_start = after_label;
                
                // Find end of label name (word boundary)
                while (*after_label && !isspace((unsigned char)*after_label) && 
                       *after_label != '[' && *after_label != ']')
                    after_label++;
                
                size_t name_len = after_label - name_start;
                
                // Compare label names (case-insensitive)
                if (name_len == strlen(label_name) && 
                    strncasecmp(name_start, label_name, name_len) == 0)
                {
                    // Found! Return position after the label instruction
                    return (int)(after_label - buffer);
                }
            }
            
            // Move past this label instruction
            pos = after_label;
        }
        else
        {
            // Skip to next whitespace or special char
            while (*pos && !isspace((unsigned char)*pos))
            {
                if (*pos == '[')
                {
                    // Skip nested list
                    int depth = 1;
                    pos++;
                    while (*pos && depth > 0)
                    {
                        if (*pos == '[') depth++;
                        else if (*pos == ']') depth--;
                        pos++;
                    }
                }
                else
                {
                    pos++;
                }
            }
        }
    }
    return -1; // Not found
}

//==========================================================================
// Trampoline-based eval_run_list / eval_run_list_expr
//==========================================================================

// Step function for OP_RUN_LIST and OP_RUN_LIST_EXPR.
// Processes one instruction per call, or handles a child op result.
// Returns the op's final result if the op is done (pops itself), or
// RESULT_NONE to indicate "continue processing this op."
static Result step_run_list(Evaluator *eval, EvalOp *op)
{
    bool is_expr = (op->kind == OP_RUN_LIST_EXPR);
    bool enable_tco = (op->flags & OP_FLAG_ENABLE_TCO) != 0;

    // Check if a child op just completed (e.g., OP_REPEAT pushed by a primitive)
    if (op->result.status != RESULT_NONE)
    {
        Result child_r = op->result;
        op->result = result_none(); // consume

        // Check if at end after child completed
        bool at_end = eval_at_end(eval);

        // Handle TCO: RESULT_STOP with tail call at end
        if (enable_tco && at_end && child_r.status == RESULT_STOP)
        {
            TailCall *tc = proc_get_tail_call();
            if (tc->is_tail_call)
            {
                eval->token_source = op->saved_source;
                op_stack_pop(eval->op_stack);
                return child_r;
            }
        }

        // Propagate non-normal results from child
        if (child_r.status != RESULT_NONE && child_r.status != RESULT_OK)
        {
            eval->token_source = op->saved_source;
            op_stack_pop(eval->op_stack);
            return child_r;
        }

        // Handle value from child
        if (child_r.status == RESULT_OK)
        {
            if (is_expr && at_end)
            {
                eval->token_source = op->saved_source;
                op_stack_pop(eval->op_stack);
                return child_r;
            }
            // Not in expression mode, or not at end — error
            child_r = result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(child_r.value));
            eval->token_source = op->saved_source;
            op_stack_pop(eval->op_stack);
            return child_r;
        }
        // RESULT_NONE from child: continue with next instruction
    }

    // Check if at end of input
    if (eval_at_end(eval))
    {
        // List completed
        eval->token_source = op->saved_source;
        op_stack_pop(eval->op_stack);
        return result_none();
    }

    // Determine tail position for TCO
    if (enable_tco)
    {
        TokenSource saved_ts = eval->token_source;
        bool last = skip_instruction(eval) && eval_at_end(eval);
        eval->token_source = saved_ts;
        eval->in_tail_position = last;
    }
    else
    {
        eval->in_tail_position = false;
    }

    // Execute one instruction
    int depth_pre = op_stack_depth(eval->op_stack);
    Result r = eval_instruction(eval);
    int depth_post = op_stack_depth(eval->op_stack);

    // Check if eval_instruction (or a primitive) pushed an op on the stack
    if (depth_post > depth_pre)
    {
        // If eval_instruction returned an error but left ops on the stack
        // (e.g., speculative PRIM_CALL remains after a failed EXPR_EVAL push),
        // clean up the orphaned ops and propagate the error.
        if (r.status == RESULT_ERROR || r.status == RESULT_THROW)
        {
            while (op_stack_depth(eval->op_stack) > depth_pre)
                op_stack_pop(eval->op_stack);
            eval->token_source = op->saved_source;
            op_stack_pop(eval->op_stack);
            return r;
        }
        // An op was pushed — yield to trampoline to process it.
        // The result will come back via op->result on the next call.
        return result_none();
    }

    // Check if there's more to execute
    bool at_end = eval_at_end(eval);

    // Handle TCO: RESULT_STOP with tail call at end of list
    if (enable_tco && at_end && r.status == RESULT_STOP)
    {
        TailCall *tc = proc_get_tail_call();
        if (tc->is_tail_call)
        {
            eval->token_source = op->saved_source;
            op_stack_pop(eval->op_stack);
            return r;
        }
    }

    // Propagate stop/output/error/throw
    if (r.status != RESULT_NONE && r.status != RESULT_OK)
    {
        eval->token_source = op->saved_source;
        op_stack_pop(eval->op_stack);
        return r;
    }

    // Handle value result
    if (r.status == RESULT_OK)
    {
        if (is_expr && at_end)
        {
            eval->token_source = op->saved_source;
            op_stack_pop(eval->op_stack);
            return r;
        }
        r = result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(r.value));
        eval->token_source = op->saved_source;
        op_stack_pop(eval->op_stack);
        return r;
    }

    // RESULT_NONE: instruction completed, continue with next
    return result_none();
}

// Step function for OP_REPEAT.
// Manages iteration and pushes OP_RUN_LIST for each body execution.
static Result step_repeat(Evaluator *eval, EvalOp *op)
{
    RepeatState *st = &op->repeat;

    // Check if a child run-list just completed
    if (op->result.status != RESULT_NONE)
    {
        Result child_r = op->result;
        op->result = result_none();

        // Propagate stop/output/error/throw from body
        if (child_r.status != RESULT_NONE && child_r.status != RESULT_OK)
        {
            eval->repcount = st->previous_repcount;
            op_stack_pop(eval->op_stack);
            return child_r;
        }

        // RESULT_OK or RESULT_NONE — body completed normally, continue
    }

    // Check if loop is done
    if (st->i >= st->count)
    {
        eval->repcount = st->previous_repcount;
        op_stack_pop(eval->op_stack);
        return result_none();
    }

    // Set up next iteration
    eval->repcount = st->i + 1;  // repcount is 1-based
    st->i++;

    // Push OP_RUN_LIST for the body
    EvalOp *body_op = op_stack_push(eval->op_stack);
    if (!body_op)
    {
        eval->repcount = st->previous_repcount;
        op_stack_pop(eval->op_stack);
        return result_error(ERR_STACK_OVERFLOW);
    }
    body_op->kind = OP_RUN_LIST;
    body_op->flags = OP_FLAG_NONE;
    body_op->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, st->body);

    return result_none(); // trampoline processes the child
}

// Step function for OP_FOREVER.
// Like repeat but no count limit.
static Result step_forever(Evaluator *eval, EvalOp *op)
{
    ForeverState *st = &op->forever;

    // Check if a child run-list just completed
    if (op->result.status != RESULT_NONE)
    {
        Result child_r = op->result;
        op->result = result_none();

        // Propagate stop/output/error/throw from body
        if (child_r.status != RESULT_NONE && child_r.status != RESULT_OK)
        {
            eval->repcount = st->previous_repcount;
            op_stack_pop(eval->op_stack);
            return child_r;
        }
    }

    // Set up next iteration
    eval->repcount = st->iteration + 1;  // repcount is 1-based
    st->iteration++;

    // Push OP_RUN_LIST for the body
    EvalOp *body_op = op_stack_push(eval->op_stack);
    if (!body_op)
    {
        eval->repcount = st->previous_repcount;
        op_stack_pop(eval->op_stack);
        return result_error(ERR_STACK_OVERFLOW);
    }
    body_op->kind = OP_RUN_LIST;
    body_op->flags = OP_FLAG_NONE;
    body_op->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, st->body);

    return result_none();
}

// Step function for OP_WHILE.
// Phase 0: push predicate. Phase 1: handle predicate result, push body. Phase 2: handle body result.
static Result step_while(Evaluator *eval, EvalOp *op)
{
    WhileState *st = &op->while_state;

    if (st->phase == 1)
    {
        // Predicate completed
        Result child_r = op->result;
        op->result = result_none();

        if (child_r.status == RESULT_ERROR)
        {
            op_stack_pop(eval->op_stack);
            return child_r;
        }
        if (child_r.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
        }
        if (!value_is_word(child_r.value))
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(child_r.value));
        }
        const char *str = value_to_string(child_r.value);
        if (strcasecmp(str, "true") == 0)
        {
            // Predicate true: run body
            st->phase = 2;
            EvalOp *body_op = op_stack_push(eval->op_stack);
            if (!body_op) { op_stack_pop(eval->op_stack); return result_error(ERR_STACK_OVERFLOW); }
            body_op->kind = OP_RUN_LIST;
            body_op->flags = OP_FLAG_NONE;
            body_op->saved_source = eval->token_source;
            token_source_init_list(&eval->token_source, st->body);
            return result_none();
        }
        else if (strcasecmp(str, "false") == 0)
        {
            op_stack_pop(eval->op_stack);
            return result_none();
        }
        else
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, str);
        }
    }

    if (st->phase == 2)
    {
        // Body completed
        Result child_r = op->result;
        op->result = result_none();

        if (child_r.status != RESULT_NONE && child_r.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return child_r;
        }
        // Fall through to push predicate again
    }

    // Phase 0 (initial) or after body: push predicate
    st->phase = 1;
    EvalOp *pred_op = op_stack_push(eval->op_stack);
    if (!pred_op) { op_stack_pop(eval->op_stack); return result_error(ERR_STACK_OVERFLOW); }
    pred_op->kind = OP_RUN_LIST_EXPR;
    pred_op->flags = OP_FLAG_NONE;
    pred_op->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, st->predicate);
    return result_none();
}

// Step function for OP_UNTIL.
// Phase 0: push predicate. Phase 1: handle predicate result, push body. Phase 2: handle body result.
// Opposite of while: loop while predicate is FALSE.
static Result step_until(Evaluator *eval, EvalOp *op)
{
    UntilState *st = &op->until_state;

    if (st->phase == 1)
    {
        // Predicate completed
        Result child_r = op->result;
        op->result = result_none();

        if (child_r.status == RESULT_ERROR)
        {
            op_stack_pop(eval->op_stack);
            return child_r;
        }
        if (child_r.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
        }
        if (!value_is_word(child_r.value))
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(child_r.value));
        }
        const char *str = value_to_string(child_r.value);
        if (strcasecmp(str, "true") == 0)
        {
            // Predicate true: stop looping
            op_stack_pop(eval->op_stack);
            return result_none();
        }
        else if (strcasecmp(str, "false") == 0)
        {
            // Predicate false: run body
            st->phase = 2;
            EvalOp *body_op = op_stack_push(eval->op_stack);
            if (!body_op) { op_stack_pop(eval->op_stack); return result_error(ERR_STACK_OVERFLOW); }
            body_op->kind = OP_RUN_LIST;
            body_op->flags = OP_FLAG_NONE;
            body_op->saved_source = eval->token_source;
            token_source_init_list(&eval->token_source, st->body);
            return result_none();
        }
        else
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, str);
        }
    }

    if (st->phase == 2)
    {
        // Body completed
        Result child_r = op->result;
        op->result = result_none();

        if (child_r.status != RESULT_NONE && child_r.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return child_r;
        }
        // Fall through to push predicate again
    }

    // Phase 0 (initial) or after body: push predicate
    st->phase = 1;
    EvalOp *pred_op = op_stack_push(eval->op_stack);
    if (!pred_op) { op_stack_pop(eval->op_stack); return result_error(ERR_STACK_OVERFLOW); }
    pred_op->kind = OP_RUN_LIST_EXPR;
    pred_op->flags = OP_FLAG_NONE;
    pred_op->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, st->predicate);
    return result_none();
}

// Step function for OP_DO_WHILE.
// Phase 0: push body. Phase 1: handle body result, push predicate. Phase 2: handle predicate result.
static Result step_do_while(Evaluator *eval, EvalOp *op)
{
    DoWhileState *st = &op->do_while;

    if (st->phase == 1)
    {
        // Body completed
        Result child_r = op->result;
        op->result = result_none();

        if (child_r.status != RESULT_NONE && child_r.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return child_r;
        }
        // Push predicate
        st->phase = 2;
        EvalOp *pred_op = op_stack_push(eval->op_stack);
        if (!pred_op) { op_stack_pop(eval->op_stack); return result_error(ERR_STACK_OVERFLOW); }
        pred_op->kind = OP_RUN_LIST_EXPR;
        pred_op->flags = OP_FLAG_NONE;
        pred_op->saved_source = eval->token_source;
        token_source_init_list(&eval->token_source, st->predicate);
        return result_none();
    }

    if (st->phase == 2)
    {
        // Predicate completed
        Result child_r = op->result;
        op->result = result_none();

        if (child_r.status == RESULT_ERROR)
        {
            op_stack_pop(eval->op_stack);
            return child_r;
        }
        if (child_r.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
        }
        if (!value_is_word(child_r.value))
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(child_r.value));
        }
        const char *str = value_to_string(child_r.value);
        if (strcasecmp(str, "false") == 0)
        {
            // Done
            op_stack_pop(eval->op_stack);
            return result_none();
        }
        else if (strcasecmp(str, "true") == 0)
        {
            // Continue looping: fall through to push body
        }
        else
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, str);
        }
    }

    // Phase 0 (initial) or continue from phase 2: push body
    st->phase = 1;
    EvalOp *body_op = op_stack_push(eval->op_stack);
    if (!body_op) { op_stack_pop(eval->op_stack); return result_error(ERR_STACK_OVERFLOW); }
    body_op->kind = OP_RUN_LIST;
    body_op->flags = OP_FLAG_NONE;
    body_op->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, st->body);
    return result_none();
}

// Step function for OP_DO_UNTIL.
// Phase 0: push body. Phase 1: handle body result, push predicate. Phase 2: handle predicate result.
// Loop until predicate is TRUE.
static Result step_do_until(Evaluator *eval, EvalOp *op)
{
    DoUntilState *st = &op->do_until;

    if (st->phase == 1)
    {
        // Body completed
        Result child_r = op->result;
        op->result = result_none();

        if (child_r.status != RESULT_NONE && child_r.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return child_r;
        }
        // Push predicate
        st->phase = 2;
        EvalOp *pred_op = op_stack_push(eval->op_stack);
        if (!pred_op) { op_stack_pop(eval->op_stack); return result_error(ERR_STACK_OVERFLOW); }
        pred_op->kind = OP_RUN_LIST_EXPR;
        pred_op->flags = OP_FLAG_NONE;
        pred_op->saved_source = eval->token_source;
        token_source_init_list(&eval->token_source, st->predicate);
        return result_none();
    }

    if (st->phase == 2)
    {
        // Predicate completed
        Result child_r = op->result;
        op->result = result_none();

        if (child_r.status == RESULT_ERROR)
        {
            op_stack_pop(eval->op_stack);
            return child_r;
        }
        if (child_r.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, NULL);
        }
        if (!value_is_word(child_r.value))
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(child_r.value));
        }
        const char *str = value_to_string(child_r.value);
        if (strcasecmp(str, "true") == 0)
        {
            // Predicate true: stop
            op_stack_pop(eval->op_stack);
            return result_none();
        }
        else if (strcasecmp(str, "false") == 0)
        {
            // Continue looping: fall through to push body
        }
        else
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_BOOL, NULL, str);
        }
    }

    // Phase 0 (initial) or continue from phase 2: push body
    st->phase = 1;
    EvalOp *body_op = op_stack_push(eval->op_stack);
    if (!body_op) { op_stack_pop(eval->op_stack); return result_error(ERR_STACK_OVERFLOW); }
    body_op->kind = OP_RUN_LIST;
    body_op->flags = OP_FLAG_NONE;
    body_op->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, st->body);
    return result_none();
}

// Helper: restore variable state and pop the for op
static Result for_cleanup(Evaluator *eval, ForState *st, Result r)
{
    if (!st->in_procedure)
    {
        if (st->had_value)
        {
            var_set(st->varname, st->saved_value);
        }
        else
        {
            var_erase(st->varname);
        }
    }
    op_stack_pop(eval->op_stack);
    return r;
}

// Step function for OP_FOR.
// Phase 0: check termination, set variable, push body.
// Phase 1: handle body result, increment, go back to phase 0.
static Result step_for(Evaluator *eval, EvalOp *op)
{
    ForState *st = &op->for_state;

    if (st->phase == 1)
    {
        // Body completed
        Result child_r = op->result;
        op->result = result_none();

        if (child_r.status != RESULT_NONE && child_r.status != RESULT_OK)
        {
            return for_cleanup(eval, st, child_r);
        }

        // Increment
        st->current += st->step;
        st->phase = 0;
        // Fall through to check termination
    }

    // Phase 0: check termination and push body
    float diff = st->current - st->limit;
    if (diff * st->step > 0)
    {
        // Past the limit — done
        return for_cleanup(eval, st, result_none());
    }

    // Set the control variable
    if (!var_set(st->varname, value_number(st->current)))
    {
        return for_cleanup(eval, st, result_error(ERR_OUT_OF_SPACE));
    }

    // Push body
    st->phase = 1;
    EvalOp *body_op = op_stack_push(eval->op_stack);
    if (!body_op)
    {
        return for_cleanup(eval, st, result_error(ERR_STACK_OVERFLOW));
    }
    body_op->kind = OP_RUN_LIST;
    body_op->flags = OP_FLAG_NONE;
    body_op->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, st->body);
    return result_none();
}

// Step function for OP_CATCH.
// Phase 0: push body. Phase 1: handle body result (check for throw/error match).
static Result step_catch(Evaluator *eval, EvalOp *op)
{
    CatchState *st = &op->catch_state;

    if (st->phase == 1)
    {
        // Body completed
        Result child_r = op->result;
        op->result = result_none();

        op_stack_pop(eval->op_stack);

        if (child_r.status == RESULT_THROW)
        {
            // "toplevel" always propagates — never caught
            if (strcasecmp(child_r.throw_tag, "toplevel") == 0)
            {
                return child_r;
            }
            // Check tag match
            if (strcasecmp(child_r.throw_tag, st->tag) == 0)
            {
                return result_none();
            }
            // No match: propagate
            return child_r;
        }
        else if (child_r.status == RESULT_ERROR && strcasecmp(st->tag, "error") == 0)
        {
            // catch "error catches errors
            error_set_caught(&child_r);
            return result_none();
        }

        // Normal completion or non-matching result
        return child_r;
    }

    // Phase 0: push body
    st->phase = 1;
    EvalOp *body_op = op_stack_push(eval->op_stack);
    if (!body_op) { op_stack_pop(eval->op_stack); return result_error(ERR_STACK_OVERFLOW); }
    body_op->kind = OP_RUN_LIST;
    body_op->flags = OP_FLAG_NONE;
    body_op->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, st->body);
    return result_none();
}

// Helper: write string to trace output
static void eval_trace_write(const char *str)
{
    LogoIO *io = primitives_get_io();
    if (io)
        logo_io_write(io, str);
}

// Helper: trace procedure entry (prints proc name, indent, args)
static void eval_trace_entry(Evaluator *eval, UserProcedure *proc)
{
    if (!proc->traced)
        return;
    for (int i = 0; i < eval->proc_depth; i++)
        eval_trace_write("  ");
    eval_trace_write(proc->name);

    FrameHeader *frame = frame_current(eval->frames);
    if (frame)
    {
        Binding *bindings = frame_get_bindings(frame);
        for (int i = 0; i < frame->param_count; i++)
        {
            eval_trace_write(" ");
            char buf[64];
            Value v = bindings[i].value;
            if (v.type == VALUE_NUMBER)
            {
                format_number(buf, sizeof(buf), v.as.number);
                eval_trace_write(buf);
            }
            else if (v.type == VALUE_WORD)
            {
                eval_trace_write(mem_word_ptr(v.as.node));
            }
            else if (v.type == VALUE_LIST)
            {
                eval_trace_write("[...]");
            }
        }
    }
    eval_trace_write("\n");
}

// Helper: trace procedure exit
static void eval_trace_exit(Evaluator *eval, UserProcedure *proc, Result body_result)
{
    if (!proc->traced)
        return;
    for (int i = 0; i < eval->proc_depth; i++)
        eval_trace_write("  ");
    if (body_result.status == RESULT_OUTPUT)
    {
        char buf[64];
        Value v = body_result.value;
        if (v.type == VALUE_NUMBER)
        {
            format_number(buf, sizeof(buf), v.as.number);
            eval_trace_write(buf);
        }
        else if (v.type == VALUE_WORD)
        {
            eval_trace_write(mem_word_ptr(v.as.node));
        }
        else if (v.type == VALUE_LIST)
        {
            eval_trace_write("[...]");
        }
        eval_trace_write("\n");
    }
    else
    {
        eval_trace_write(proc->name);
        eval_trace_write(" stopped\n");
    }
}

// Helper: cleanup procedure call frame and return appropriate result
static Result proc_call_cleanup(Evaluator *eval, EvalOp *op, Result body_result)
{
    ProcCallState *st = &op->proc_call;

    eval_trace_exit(eval, st->proc, body_result);

    eval->proc_depth--;
    proc_pop_current();
    frame_pop(eval->frames);
    op_stack_pop(eval->op_stack);

    if (body_result.status == RESULT_STOP)
        return result_none();
    if (body_result.status == RESULT_OUTPUT)
        return result_ok(body_result.value);
    if (body_result.status == RESULT_ERROR)
        return result_error_in(body_result, st->proc->name);
    return body_result; // RESULT_THROW, RESULT_NONE, etc.
}

// Step function for OP_PROC_CALL.
// Manages frame push/pop and iterates through procedure body lines.
// Phase 0: push first body line. Phase 1+: handle line result, push next.
static Result step_proc_call(Evaluator *eval, EvalOp *op)
{
    ProcCallState *st = &op->proc_call;

    if (st->phase >= 1)
    {
        // A body line completed
        Result child_r = op->result;
        op->result = result_none();

        // RESULT_STOP: check for tail call (TCO)
        if (child_r.status == RESULT_STOP)
        {
            TailCall *tc = proc_get_tail_call();
            if (tc->is_tail_call)
            {
                UserProcedure *target = proc_find(tc->proc_name);
                if (target && target == st->proc)
                {
                    // Self-recursive tail call: reuse frame, restart body
                    int argc = tc->arg_count;
                    Value args[MAX_PROC_PARAMS];
                    for (int i = 0; i < argc; i++)
                        args[i] = tc->args[i];
                    proc_clear_tail_call();

                    proc_pop_current();
                    proc_push_current(st->proc->name);

                    if (!frame_reuse(eval->frames, st->proc, args, argc))
                    {
                        frame_pop(eval->frames);
                        word_offset_t fo = frame_push(eval->frames, st->proc, args, argc);
                        if (fo == OFFSET_NONE)
                        {
                            eval->proc_depth--;
                            proc_pop_current();
                            op_stack_pop(eval->op_stack);
                            return result_error(ERR_OUT_OF_SPACE);
                        }
                    }

                    // Restart body
                    st->current_line = st->proc->body;
                    st->phase = 0;
                    // Fall through to push first line
                }
                else
                {
                    // Non-self-recursive tail call: stop normally
                    proc_clear_tail_call();
                    return proc_call_cleanup(eval, op, child_r);
                }
            }
            else
            {
                return proc_call_cleanup(eval, op, child_r);
            }
        }
        else if (child_r.status == RESULT_OUTPUT)
        {
            return proc_call_cleanup(eval, op, child_r);
        }
        else if (child_r.status == RESULT_ERROR)
        {
            return proc_call_cleanup(eval, op, child_r);
        }
        else if (child_r.status == RESULT_THROW)
        {
            return proc_call_cleanup(eval, op, child_r);
        }
        else if (child_r.status == RESULT_GOTO)
        {
            // Search for label in body
            const char *label_name = child_r.goto_label;
            bool found = false;
            Node search = st->proc->body;
            while (!mem_is_nil(search))
            {
                Node search_line = mem_car(search);
                Node search_tokens = search_line;
                if (NODE_GET_TYPE(search_line) == NODE_TYPE_LIST)
                    search_tokens = NODE_MAKE_LIST(NODE_GET_INDEX(search_line));
                if (!mem_is_nil(search_tokens))
                {
                    Node first_elem = mem_car(search_tokens);
                    if (mem_is_word(first_elem))
                    {
                        const char *word = mem_word_ptr(first_elem);
                        if (strcasecmp(word, "label") == 0)
                        {
                            Node rest = mem_cdr(search_tokens);
                            if (!mem_is_nil(rest))
                            {
                                Node label_arg = mem_car(rest);
                                if (mem_is_word(label_arg))
                                {
                                    const char *arg = mem_word_ptr(label_arg);
                                    if (arg[0] == '"') arg++;
                                    if (strcasecmp(arg, label_name) == 0)
                                    {
                                        st->current_line = mem_cdr(search);
                                        found = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                search = mem_cdr(search);
            }
            if (!found)
            {
                Result err = result_error_arg(ERR_CANT_FIND_LABEL, NULL, label_name);
                return proc_call_cleanup(eval, op, err);
            }
            // Fall through to push next body line
        }
        else if (child_r.status == RESULT_OK)
        {
            // Value at statement level in proc body
            Result err = result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(child_r.value));
            return proc_call_cleanup(eval, op, err);
        }
        else
        {
            // RESULT_NONE: advance to next body line
            st->current_line = mem_cdr(st->current_line);
        }
    }

    // Push next non-empty body line
    while (!mem_is_nil(st->current_line))
    {
        Node line = mem_car(st->current_line);
        Node line_tokens = line;
        if (NODE_GET_TYPE(line) == NODE_TYPE_LIST)
            line_tokens = NODE_MAKE_LIST(NODE_GET_INDEX(line));

        if (mem_is_nil(line_tokens))
        {
            st->current_line = mem_cdr(st->current_line);
            continue;
        }

        // Trace entry on first non-empty line (phase 0 → first line)
        if (st->phase == 0)
        {
            eval_trace_entry(eval, st->proc);
        }

        // Stepped execution: print line, wait for key
        if (st->proc->stepped)
        {
            LogoIO *io = primitives_get_io();
            if (io)
            {
                char line_buf[512];
                int pos = 0;
                Node n = line_tokens;
                bool first = true;
                while (!mem_is_nil(n) && pos < (int)sizeof(line_buf) - 1)
                {
                    Node elem = mem_car(n);
                    if (!first && pos < (int)sizeof(line_buf) - 1)
                        line_buf[pos++] = ' ';
                    first = false;
                    if (mem_is_word(elem))
                    {
                        const char *w = mem_word_ptr(elem);
                        int len = strlen(w);
                        if (pos + len < (int)sizeof(line_buf) - 1)
                        {
                            memcpy(line_buf + pos, w, len);
                            pos += len;
                        }
                    }
                    n = mem_cdr(n);
                }
                line_buf[pos] = '\0';
                if (pos > 0)
                {
                    logo_io_write(io, line_buf);
                    logo_io_write(io, "\n");
                    logo_io_flush(io);
                    int ch = logo_io_read_char(io);
                    if (ch == LOGO_STREAM_INTERRUPTED)
                    {
                        return proc_call_cleanup(eval, op, result_error(ERR_STOPPED));
                    }
                }
            }
        }

        bool is_last_line = mem_is_nil(mem_cdr(st->current_line));

        st->phase++;
        EvalOp *line_op = op_stack_push(eval->op_stack);
        if (!line_op)
        {
            return proc_call_cleanup(eval, op, result_error(ERR_STACK_OVERFLOW));
        }
        line_op->kind = OP_RUN_LIST;
        line_op->flags = is_last_line ? OP_FLAG_ENABLE_TCO : OP_FLAG_NONE;
        line_op->saved_source = eval->token_source;
        token_source_init_list(&eval->token_source, line_tokens);
        return result_none();
    }

    // All body lines completed without stop/output
    // Trace entry if body was empty (phase still 0)
    if (st->phase == 0)
        eval_trace_entry(eval, st->proc);
    return proc_call_cleanup(eval, op, result_none());
}

// Step function for OP_EXPR_EVAL.
// Resumes an iterative Pratt parse after a deferred procedure call completes.
// The proc call result is in op->result; the saved operator stack is in op->expr_eval.
static Result step_expr_eval(Evaluator *eval, EvalOp *op)
{
    ExprEvalState *st = &op->expr_eval;

    // The proc call just completed. Get its result.
    Result lhs = op->result;
    op->result = result_none();

    // If the proc call returned an error/stop/output/throw, propagate it
    if (lhs.status != RESULT_OK && lhs.status != RESULT_NONE)
    {
        op_stack_pop(eval->op_stack);
        return lhs;
    }

    // RESULT_NONE means the proc had no output (void call).
    // If we have pending operators (depth > 0), we need a value — error.
    // If depth == 0, this is a void expression at statement level (e.g., print)
    // — just pass through.
    if (lhs.status == RESULT_NONE)
    {
        if (st->depth > 0)
        {
            op_stack_pop(eval->op_stack);
            return result_error(ERR_DIDNT_OUTPUT);
        }
        // No pending operators — pass through void result
        op_stack_pop(eval->op_stack);
        return result_none();
    }

    // Resume the iterative Pratt parse with the proc call's return value.
    // We have the pending operator stack and current min_bp from when we yielded.
    PendingBinOp local_ops[MAX_EXPR_OPS];
    int depth = st->depth;
    int min_bp = st->min_bp;
    for (int i = 0; i < depth; i++)
        local_ops[i] = st->ops[i];

    for (;;)
    {
        Token op_tok = peek(eval);
        int bp = get_infix_bp(op_tok.type);

        // Reduce: apply pending operators with binding power > current
        while ((bp == BP_NONE || bp < min_bp) && depth > 0)
        {
            depth--;
            Result r = apply_binary_op(local_ops[depth].op_type, local_ops[depth].left, lhs.value);
            if (r.status != RESULT_OK)
            {
                op_stack_pop(eval->op_stack);
                return r;
            }
            lhs = r;
            min_bp = local_ops[depth].min_bp;
            op_tok = peek(eval);
            bp = get_infix_bp(op_tok.type);
        }

        if (bp == BP_NONE || bp < min_bp)
            break;

        // Shift: save current state and parse right operand
        advance(eval);
        if (depth >= MAX_EXPR_OPS)
        {
            op_stack_pop(eval->op_stack);
            return result_error(ERR_STACK_OVERFLOW);
        }
        local_ops[depth].left = lhs.value;
        local_ops[depth].op_type = (uint8_t)op_tok.type;
        local_ops[depth].min_bp = min_bp;
        depth++;
        min_bp = bp + 1;

        int depth_before_primary = op_stack_depth(eval->op_stack);
        lhs = eval_primary(eval);

        // If eval_primary pushed another deferred proc call, save and yield again
        if (lhs.status == RESULT_NONE && op_stack_depth(eval->op_stack) > depth_before_primary)
        {
            // Update our saved state for the next resume
            st->depth = depth;
            st->min_bp = min_bp;
            for (int i = 0; i < depth; i++)
                st->ops[i] = local_ops[i];
            // OP_PROC_CALL was pushed above us by eval_primary.
            // Return result_none() to yield. The trampoline will process the
            // OP_PROC_CALL, and when it pops, store its result in our op->result.
            return result_none();
        }

        if (lhs.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return lhs;
        }
    }

    // Expression complete — return the final value
    op_stack_pop(eval->op_stack);
    return lhs;
}

// Step function for OP_PRIM_CALL.
// Called after a deferred expression evaluation completes for a primitive's argument.
// Stores the argument value and calls the primitive when all args are ready.
static Result step_prim_call(Evaluator *eval, EvalOp *op)
{
    PrimCallState *st = &op->prim_call;
    const struct Primitive *prim = st->prim;

    // Get the result from the child op (expression evaluation)
    Result arg_result = op->result;
    op->result = result_none();

    // Propagate errors and control flow
    if (arg_result.status != RESULT_OK && arg_result.status != RESULT_NONE)
    {
        op_stack_pop(eval->op_stack);
        return result_set_error_proc(arg_result, st->user_name);
    }

    // RESULT_NONE means the expression didn't produce a value.
    // For statement-level void calls (e.g., print without output), pass through.
    if (arg_result.status == RESULT_NONE)
    {
        // The child expression was void — propagate as-is
        op_stack_pop(eval->op_stack);
        return result_none();
    }

    // Store the argument value
    int idx = st->current_arg;
    if (idx < MAX_PRIM_STAGED_ARGS)
    {
        st->args[idx] = arg_result.value;
    }
    st->argc = idx + 1;
    st->current_arg = idx + 1;

    // Check if this was a varargs call (total_args == -1) and we need to
    // check for more args by peeking the token source.
    // For the paren path, we'd need to continue collecting until ')'.
    // For simplicity, varargs OP_PRIM_CALL currently completes after the
    // first deferred arg batch — remaining args would have been collected
    // synchronously before the deferral.

    // Collect remaining args synchronously, yielding to trampoline only on deferral
    while (st->total_args > 0 && st->argc < st->total_args)
    {
        // Need more args. Evaluate the next expression.
        eval->primitive_arg_depth++;
        bool old_tail = eval->in_tail_position;
        eval->in_tail_position = false;

        int depth_before = op_stack_depth(eval->op_stack);
        Result next_arg = eval_expression(eval);

        eval->in_tail_position = old_tail;

        // Check for another deferral (async proc call pushed above us)
        if (next_arg.status == RESULT_NONE &&
            op_stack_depth(eval->op_stack) > depth_before)
        {
            // Another expression deferred. The ops are above us.
            // Return result_none() to yield; we'll resume on next call.
            eval->primitive_arg_depth--;
            return result_none();
        }

        eval->primitive_arg_depth--;

        if (next_arg.status == RESULT_ERROR || next_arg.status == RESULT_THROW ||
            next_arg.status == RESULT_STOP || next_arg.status == RESULT_OUTPUT)
        {
            op_stack_pop(eval->op_stack);
            return result_set_error_proc(next_arg, st->user_name);
        }
        if (next_arg.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_ENOUGH_INPUTS, st->user_name, NULL);
        }

        if (st->current_arg < MAX_PRIM_STAGED_ARGS)
            st->args[st->current_arg] = next_arg.value;
        st->argc++;
        st->current_arg++;
    }

    // All args collected — call the primitive
    op_stack_pop(eval->op_stack);
    Result r = prim->func(eval, st->argc, st->args);
    return result_set_error_proc(r, st->user_name);
}

// Main trampoline dispatch loop.
// Processes operations on the op stack until the stack returns to base_depth.
Result eval_trampoline(Evaluator *eval, int base_depth)
{
    OpStack *stack = eval->op_stack;
    Result r = result_none();

    while (op_stack_depth(stack) > base_depth)
    {
        EvalOp *op = op_stack_peek(stack);
        int depth_before = op_stack_depth(stack);

        switch (op->kind)
        {
        case OP_RUN_LIST:
        case OP_RUN_LIST_EXPR:
            r = step_run_list(eval, op);
            break;

        case OP_REPEAT:
            r = step_repeat(eval, op);
            break;

        case OP_FOREVER:
            r = step_forever(eval, op);
            break;

        case OP_WHILE:
            r = step_while(eval, op);
            break;

        case OP_UNTIL:
            r = step_until(eval, op);
            break;

        case OP_DO_WHILE:
            r = step_do_while(eval, op);
            break;

        case OP_DO_UNTIL:
            r = step_do_until(eval, op);
            break;

        case OP_CATCH:
            r = step_catch(eval, op);
            break;

        case OP_FOR:
            r = step_for(eval, op);
            break;

        case OP_PROC_CALL:
            r = step_proc_call(eval, op);
            break;

        case OP_EXPR_EVAL:
            r = step_expr_eval(eval, op);
            break;

        case OP_PRIM_CALL:
            r = step_prim_call(eval, op);
            break;

        default:
            r = result_error(ERR_STACK_OVERFLOW);
            eval->token_source = op->saved_source;
            op_stack_pop(stack);
            break;
        }

        int depth_after = op_stack_depth(stack);

        if (depth_after < depth_before)
        {
            // Op was popped by step function
            if (depth_after > base_depth)
            {
                // Store result in parent op for inter-phase communication
                EvalOp *parent = op_stack_peek(stack);
                parent->result = r;
            }
            // If depth_after == base_depth, loop exits and we return r
        }
        // If depth_after >= depth_before: op continues or pushed child, loop back
    }

    return r;
}

Result eval_run_list(Evaluator *eval, Node list)
{
    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);
    bool old_tail = eval->in_tail_position;

    // Push OP_RUN_LIST operation
    EvalOp *op = op_stack_push(stack);
    if (!op)
    {
        return result_error(ERR_STACK_OVERFLOW);
    }
    op->kind = OP_RUN_LIST;
    op->flags = OP_FLAG_NONE;
    op->saved_source = eval->token_source;

    // Set up token source for this list
    token_source_init_list(&eval->token_source, list);

    // Run the trampoline
    Result r = eval_trampoline(eval, base_depth);

    eval->in_tail_position = old_tail;
    return r;
}

// Run a list as an expression - allows the list to output a value
// Used by 'run' and 'if' when they act as operations
// Propagates tail position for TCO
Result eval_run_list_expr(Evaluator *eval, Node list)
{
    // If we're in tail position inside a procedure, the last instruction
    // in this list is also in tail position - enables TCO for:
    //   if :n > 0 [recurse :n - 1]
    bool enable_tco = eval->in_tail_position && eval->proc_depth > 0;

    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);
    bool old_tail = eval->in_tail_position;

    // Push OP_RUN_LIST_EXPR operation
    EvalOp *op = op_stack_push(stack);
    if (!op)
    {
        return result_error(ERR_STACK_OVERFLOW);
    }
    op->kind = OP_RUN_LIST_EXPR;
    op->flags = enable_tco ? OP_FLAG_ENABLE_TCO : OP_FLAG_NONE;
    op->saved_source = eval->token_source;

    // Set up token source for this list
    token_source_init_list(&eval->token_source, list);

    // Run the trampoline
    Result r = eval_trampoline(eval, base_depth);

    eval->in_tail_position = old_tail;
    return r;
}

//==========================================================================
// Operation Push Helpers (called by primitives)
//==========================================================================

Result eval_push_repeat(Evaluator *eval, int count, Node body)
{
    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);
    EvalOp *op = op_stack_push(stack);
    if (!op)
        return result_error(ERR_STACK_OVERFLOW);
    op->kind = OP_REPEAT;
    op->flags = OP_FLAG_NONE;
    op->repeat.body = body;
    op->repeat.count = count;
    op->repeat.i = 0;
    op->repeat.previous_repcount = eval->repcount;
    if (base_depth > 0)
        return result_none();
    return eval_trampoline(eval, base_depth);
}

Result eval_push_forever(Evaluator *eval, Node body)
{
    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);
    EvalOp *op = op_stack_push(stack);
    if (!op)
        return result_error(ERR_STACK_OVERFLOW);
    op->kind = OP_FOREVER;
    op->flags = OP_FLAG_NONE;
    op->forever.body = body;
    op->forever.previous_repcount = eval->repcount;
    op->forever.iteration = 0;
    if (base_depth > 0)
        return result_none();
    return eval_trampoline(eval, base_depth);
}

Result eval_push_while(Evaluator *eval, Node predicate, Node body)
{
    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);
    EvalOp *op = op_stack_push(stack);
    if (!op)
        return result_error(ERR_STACK_OVERFLOW);
    op->kind = OP_WHILE;
    op->flags = OP_FLAG_NONE;
    op->while_state.predicate = predicate;
    op->while_state.body = body;
    op->while_state.phase = 0;
    if (base_depth > 0)
        return result_none();
    return eval_trampoline(eval, base_depth);
}

Result eval_push_until(Evaluator *eval, Node predicate, Node body)
{
    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);
    EvalOp *op = op_stack_push(stack);
    if (!op)
        return result_error(ERR_STACK_OVERFLOW);
    op->kind = OP_UNTIL;
    op->flags = OP_FLAG_NONE;
    op->until_state.predicate = predicate;
    op->until_state.body = body;
    op->until_state.phase = 0;
    if (base_depth > 0)
        return result_none();
    return eval_trampoline(eval, base_depth);
}

Result eval_push_do_while(Evaluator *eval, Node body, Node predicate)
{
    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);
    EvalOp *op = op_stack_push(stack);
    if (!op)
        return result_error(ERR_STACK_OVERFLOW);
    op->kind = OP_DO_WHILE;
    op->flags = OP_FLAG_NONE;
    op->do_while.body = body;
    op->do_while.predicate = predicate;
    op->do_while.phase = 0;
    if (base_depth > 0)
        return result_none();
    return eval_trampoline(eval, base_depth);
}

Result eval_push_do_until(Evaluator *eval, Node body, Node predicate)
{
    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);
    EvalOp *op = op_stack_push(stack);
    if (!op)
        return result_error(ERR_STACK_OVERFLOW);
    op->kind = OP_DO_UNTIL;
    op->flags = OP_FLAG_NONE;
    op->do_until.body = body;
    op->do_until.predicate = predicate;
    op->do_until.phase = 0;
    if (base_depth > 0)
        return result_none();
    return eval_trampoline(eval, base_depth);
}

Result eval_push_for(Evaluator *eval, const char *varname, float start,
                     float limit, float step, Node body,
                     Value saved_value, bool had_value, bool in_procedure)
{
    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);
    EvalOp *op = op_stack_push(stack);
    if (!op)
        return result_error(ERR_STACK_OVERFLOW);
    op->kind = OP_FOR;
    op->flags = OP_FLAG_NONE;
    op->for_state.varname = varname;
    op->for_state.start = start;
    op->for_state.limit = limit;
    op->for_state.step = step;
    op->for_state.current = start;
    op->for_state.body = body;
    op->for_state.saved_value = saved_value;
    op->for_state.had_value = had_value;
    op->for_state.in_procedure = in_procedure;
    op->for_state.phase = 0;
    if (base_depth > 0)
        return result_none();
    return eval_trampoline(eval, base_depth);
}

Result eval_push_catch(Evaluator *eval, const char *tag, Node body)
{
    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);
    EvalOp *op = op_stack_push(stack);
    if (!op)
        return result_error(ERR_STACK_OVERFLOW);
    op->kind = OP_CATCH;
    op->flags = OP_FLAG_NONE;
    op->catch_state.tag = tag;
    op->catch_state.body = body;
    return eval_trampoline(eval, base_depth);
}

Result eval_push_proc_call(Evaluator *eval, UserProcedure *proc, int argc, Value *args)
{
    OpStack *stack = eval->op_stack;
    int base_depth = op_stack_depth(stack);

    // Push frame first
    word_offset_t frame_offset = frame_push(eval->frames, proc, args, argc);
    if (frame_offset == OFFSET_NONE)
        return result_error(ERR_OUT_OF_SPACE);

    eval->proc_depth++;
    proc_push_current(proc->name);
    proc_clear_tail_call();

    // Push OP_PROC_CALL
    EvalOp *op = op_stack_push(stack);
    if (!op)
    {
        eval->proc_depth--;
        proc_pop_current();
        frame_pop(eval->frames);
        return result_error(ERR_STACK_OVERFLOW);
    }
    op->kind = OP_PROC_CALL;
    op->flags = OP_FLAG_NONE;
    op->proc_call.proc = proc;
    op->proc_call.current_line = proc->body;
    op->proc_call.phase = 0;

    return eval_trampoline(eval, base_depth);
}
