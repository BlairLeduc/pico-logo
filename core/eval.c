//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "eval.h"
#include "error.h"
#include "primitives.h"
#include "procedures.h"
#include "repl.h"
#include "variables.h"
#include "frame.h"
#include "devices/io.h"
#include "compiler.h"
#include "vm.h"
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
    token_source_init_lexer(&eval->token_source, lexer);
    eval->frames = NULL;  // Caller sets this if needed
    eval->paren_depth = 0;
    eval->error_code = 0;
    eval->error_context = NULL;
    eval->in_tail_position = false;
    eval->proc_depth = 0;
    eval->repcount = -1;
    eval->primitive_arg_depth = 0;
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
        Node sublist = token_source_get_sublist(&eval->token_source);
        if (!mem_is_nil(sublist))
        {
            // For NodeIterator with nested list: sublist is already parsed, just use it
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
                
                // Track that we're collecting primitive args (CPS fallback zone)
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
                    
                    if (arg.status == RESULT_ERROR || arg.status == RESULT_CALL)
                    {
                        eval->primitive_arg_depth--;
                        eval->paren_depth--;
                        return result_set_error_proc(arg, user_name);
                    }
                    if (arg.status != RESULT_OK)
                        break;
                    args[argc++] = arg.value;
                }
                
                eval->primitive_arg_depth--;
                
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

            // Track that we're collecting primitive args (CPS fallback zone)
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
                
                // Propagate errors and control flow (throw, stop, output, call)
                if (arg.status == RESULT_ERROR || arg.status == RESULT_THROW ||
                    arg.status == RESULT_STOP || arg.status == RESULT_OUTPUT ||
                    arg.status == RESULT_CALL)
                {
                    eval->primitive_arg_depth--;
                    return result_set_error_proc(arg, user_name);
                }
                if (arg.status != RESULT_OK)
                {
                    eval->primitive_arg_depth--;
                    return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_name, NULL);
                }
                args[argc++] = arg.value;
            }

            eval->primitive_arg_depth--;

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
                
                // Propagate errors and control flow (throw, stop, output, call)
                if (arg.status == RESULT_ERROR || arg.status == RESULT_THROW ||
                    arg.status == RESULT_STOP || arg.status == RESULT_OUTPUT ||
                    arg.status == RESULT_CALL)
                    return arg;
                if (arg.status != RESULT_OK)
                {
                    return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_proc->name, NULL);
                }
                args[argc++] = arg.value;
            }

            if (argc < user_proc->param_count)
            {
                return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_proc->name, NULL);
            }

            // Tail call optimization: if we're in tail position inside a procedure,
            // and this is a SELF-RECURSIVE call, set up a tail call for frame reuse.
            // Non-self-recursive tail calls use CPS instead to preserve dynamic scoping.
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
                        // Return a special marker - proc_call will check tail_call
                        return result_stop();
                    }
                }
                // Non-self-recursive tail call - fall through to CPS
            }

            // CPS: if we're inside a procedure and NOT collecting primitive args,
            // return RESULT_CALL to let proc_call handle the call iteratively
            if (eval->proc_depth > 0 && eval->primitive_arg_depth == 0)
            {
                return result_call(user_proc, argc, args);
            }

            // Otherwise use direct call (at top-level or inside primitive args)
            return proc_call(eval, user_proc, argc, args);
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
        case TOKEN_LESS_THAN:
            // Return boolean word "true" or "false"
            lhs = result_ok(value_word(mem_atom_cstr((left_n < right_n) ? "true" : "false")));
            continue;
        case TOKEN_GREATER_THAN:
            // Return boolean word "true" or "false"
            lhs = result_ok(value_word(mem_atom_cstr((left_n > right_n) ? "true" : "false")));
            continue;
        default:
            return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, op_name);
        }

        lhs = result_ok(value_number(result));
    }

    return lhs;
}

Result eval_expression(Evaluator *eval)
{
#if EVAL_USE_VM
    TokenSource saved_source;
    token_source_copy(&saved_source, &eval->token_source);
    Lexer saved_lexer;
    bool saved_lexer_valid = false;
    if (eval->token_source.type == TOKEN_SOURCE_LEXER && eval->token_source.lexer)
    {
        saved_lexer = *eval->token_source.lexer;
        saved_lexer_valid = true;
    }
    int saved_paren_depth = eval->paren_depth;
    int saved_primitive_depth = eval->primitive_arg_depth;

    Instruction code_buf[256];
    Value const_buf[64];
    Bytecode bc = {
        .code = code_buf,
        .code_len = 0,
        .code_cap = sizeof(code_buf) / sizeof(code_buf[0]),
        .const_pool = const_buf,
        .const_len = 0,
        .const_cap = sizeof(const_buf) / sizeof(const_buf[0]),
        .arena = NULL
    };
    bc_init(&bc, NULL);

    Compiler c = {
        .eval = eval,
        .bc = &bc,
        .instruction_mode = false
    };

    Result cr = compile_expression(&c, &bc);
    if (cr.status != RESULT_OK)
    {
        token_source_copy(&eval->token_source, &saved_source);
        if (saved_lexer_valid && eval->token_source.type == TOKEN_SOURCE_LEXER && eval->token_source.lexer)
        {
            *eval->token_source.lexer = saved_lexer;
        }
        eval->paren_depth = saved_paren_depth;
        eval->primitive_arg_depth = saved_primitive_depth;
        return eval_expr_bp(eval, BP_NONE);
    }

    VM vm;
    vm_init(&vm);
    vm.eval = eval;
    return vm_exec(&vm, &bc);
#else
    return eval_expr_bp(eval, BP_NONE);
#endif
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
                if (proc_name != NULL)
                {
                    logo_io_clear_pause_request(io);
                    logo_io_write_line(io, "Pausing...");
                    
                    ReplState state;
                    repl_init(&state, io, REPL_FLAGS_PAUSE, proc_name);
                    Result r = repl_run(&state);
                    
                    if (r.status != RESULT_OK && r.status != RESULT_NONE)
                    {
                        return r;
                    }
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
    // We check if the flag is set, but only clear it if we actually pause
    if (io && logo_io_check_pause_request(io))
    {
        const char *proc_name = proc_get_current();
        if (proc_name != NULL)
        {
            // Clear the flag now that we're actually pausing
            logo_io_clear_pause_request(io);
            
            // Run the pause REPL - this blocks until co is called or throw "toplevel
            logo_io_write_line(io, "Pausing...");
            
            ReplState state;
            repl_init(&state, io, REPL_FLAGS_PAUSE, proc_name);
            Result r = repl_run(&state);
            
            // If pause REPL exited with throw or error, propagate it
            if (r.status != RESULT_OK && r.status != RESULT_NONE)
            {
                return r;
            }
            // Otherwise continue execution
        }
        // Don't clear flag if at top level - defer to when we're inside a procedure
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

// Evaluate a list as procedure body with tail call optimization
// The last instruction in the list is evaluated in tail position
Result eval_run_list_with_tco(Evaluator *eval, Node list, bool enable_tco)
{
    TokenSource old_source;
    bool old_tail = false;
#if EVAL_USE_VM
    {
        LogoIO *io = primitives_get_io();
        if (io && (logo_io_check_user_interrupt(io) ||
                   logo_io_check_freeze_request(io) ||
                   logo_io_check_pause_request(io)))
        {
            goto legacy_list_eval;
        }

        bool saved_tail = eval->in_tail_position;

        Instruction code_buf[256];
        Value const_buf[64];
        Bytecode bc = {
            .code = code_buf,
            .code_len = 0,
            .code_cap = sizeof(code_buf) / sizeof(code_buf[0]),
            .const_pool = const_buf,
            .const_len = 0,
            .const_cap = sizeof(const_buf) / sizeof(const_buf[0]),
            .arena = NULL
        };
        bc_init(&bc, NULL);

        Compiler c = {
            .eval = eval,
            .bc = &bc,
            .instruction_mode = true,
            .tail_position = false,
            .tail_depth = 0
        };

        Result cr = compile_list_instructions(&c, list, &bc, enable_tco);
        if (cr.status == RESULT_NONE || cr.status == RESULT_OK)
        {
            VM vm;
            vm_init(&vm);
            vm.eval = eval;
            Result r = vm_exec(&vm, &bc);
            eval->in_tail_position = saved_tail;
            return r;
        }
        eval->in_tail_position = saved_tail;
    }
#endif
legacy_list_eval:
    // Save current token source state
    old_source = eval->token_source;
    old_tail = eval->in_tail_position;

    // Create new token source from the Node list
    token_source_init_list(&eval->token_source, list);

    Result r = result_none();

    while (!eval_at_end(eval))
    {
        // Determine if this instruction is the last one
        bool last_instruction = false;
        if (enable_tco)
        {
            // Create lookahead copy
            Evaluator lookahead = *eval;
            token_source_copy(&lookahead.token_source, &eval->token_source);
            last_instruction = skip_instruction(&lookahead) && eval_at_end(&lookahead);
        }

        // Set tail position for this instruction
        eval->in_tail_position = enable_tco && last_instruction;

        // Execute instruction
        r = eval_instruction(eval);

        // Restore tail flag
        eval->in_tail_position = old_tail;

        // Check if there's more to execute
        bool at_end = eval_at_end(eval);
        
        // If TCO is enabled and this was the last instruction and we got RESULT_STOP
        // from a tail call setup, that's correct - return it
        if (enable_tco && at_end && r.status == RESULT_STOP)
        {
            TailCall *tc = proc_get_tail_call();
            if (tc->is_tail_call)
            {
                // This is a valid tail call - return to let proc_call handle it
                break;
            }
        }

        // Handle RESULT_CALL - save token position for continuation and propagate
        if (r.status == RESULT_CALL)
        {
            // Save current token position in frame for later resumption
            // The caller (execute_body_with_step) will save body_cursor
            FrameStack *frames = eval->frames;
            if (frames && !frame_stack_is_empty(frames))
            {
                FrameHeader *frame = frame_current(frames);
                if (frame)
                {
                    // Save position within this line (token source current node)
                    frame->line_cursor = token_source_get_position(&eval->token_source);
                }
            }
            break;  // Propagate to caller
        }

        // Handle RESULT_GOTO - let it bubble up to execute_body_with_step
        // which has access to the full procedure body with all lines
        if (r.status == RESULT_GOTO)
        {
            break;  // Propagate to caller
        }

        // Propagate stop/output/error/throw immediately
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

    // Restore state
    eval->in_tail_position = old_tail;
    eval->token_source = old_source;

    return r;
}

Result eval_run_list(Evaluator *eval, Node list)
{
    // Increment primitive_arg_depth to disable CPS during this call
    // This ensures nested procedure calls complete before returning to caller
    // (CPS would return RESULT_CALL which primitives don't handle)
    eval->primitive_arg_depth++;
    Result r = eval_run_list_with_tco(eval, list, false);
    eval->primitive_arg_depth--;
    return r;
}

// Run a list as an expression - allows the list to output a value
// Used by 'run' and 'if' when they act as operations
// Propagates tail position for TCO
Result eval_run_list_expr(Evaluator *eval, Node list)
{
#if EVAL_USE_VM
    Instruction code_buf[256];
    Value const_buf[64];
    Bytecode bc = {
        .code = code_buf,
        .code_len = 0,
        .code_cap = sizeof(code_buf) / sizeof(code_buf[0]),
        .const_pool = const_buf,
        .const_len = 0,
        .const_cap = sizeof(const_buf) / sizeof(const_buf[0]),
        .arena = NULL
    };
    bc_init(&bc, NULL);

    Compiler c = {
        .eval = eval,
        .bc = &bc,
        .instruction_mode = false
    };

    bool saved_tail = eval->in_tail_position;
    eval->primitive_arg_depth++;
    Result cr = compile_list(&c, list, &bc);
    if (cr.status == RESULT_OK)
    {
        VM vm;
        vm_init(&vm);
        vm.eval = eval;
        Result r = vm_exec(&vm, &bc);
        eval->primitive_arg_depth--;
        eval->in_tail_position = saved_tail;
        return r;
    }
    eval->primitive_arg_depth--;
    eval->in_tail_position = saved_tail;
#endif
    // If we're in tail position inside a procedure, the last instruction 
    // in this list is also in tail position - enables TCO for:
    //   if :n > 0 [recurse :n - 1]
    bool enable_tco = eval->in_tail_position && eval->proc_depth > 0;
    
    // Increment primitive_arg_depth to disable CPS during this call
    // This ensures nested procedure calls complete before returning to caller
    // (TCO still works via the tail call mechanism)
    eval->primitive_arg_depth++;
    
    // Save current token source state
    TokenSource old_source = eval->token_source;
    bool old_tail = eval->in_tail_position;

    // Create new token source from the Node list
    token_source_init_list(&eval->token_source, list);

    Result r = result_none();

    while (!eval_at_end(eval))
    {
        // Determine if this instruction is the last one (for TCO)
        bool last_instruction = false;
        if (enable_tco)
        {
            // Create lookahead copy
            Evaluator lookahead = *eval;
            token_source_copy(&lookahead.token_source, &eval->token_source);
            last_instruction = skip_instruction(&lookahead) && eval_at_end(&lookahead);
        }

        // Set tail position for this instruction
        eval->in_tail_position = enable_tco && last_instruction;
        
        r = eval_instruction(eval);
        
        // Restore tail flag
        eval->in_tail_position = old_tail;
        
        // Check if there's more to execute
        bool at_end = eval_at_end(eval);
        
        // If TCO is enabled and this was the last instruction and we got RESULT_STOP
        // from a tail call setup, return it to let proc_call handle it
        if (enable_tco && at_end && r.status == RESULT_STOP)
        {
            TailCall *tc = proc_get_tail_call();
            if (tc->is_tail_call)
            {
                break;
            }
        }
        
        // Propagate stop/output/error/throw immediately
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            break;
        }

        // If we got a value and there's more to execute, that's an error
        if (r.status == RESULT_OK && !at_end)
        {
            r = result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(r.value));
            break;
        }
        
        // If we got a value and we're at the end, return it (operation case)
        if (r.status == RESULT_OK)
        {
            break;
        }
    }

    // Restore state
    eval->in_tail_position = old_tail;
    eval->token_source = old_source;
    eval->primitive_arg_depth--;

    return r;
}
