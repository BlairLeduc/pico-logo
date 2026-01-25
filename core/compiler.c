//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Compiler scaffolding for VM bytecode (Phase 0).
//

#include "compiler.h"
#include "error.h"
#include "lexer.h"
#include "memory.h"
#include "primitives.h"
#include "token_source.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "procedures.h"

// Binding powers for Pratt parser
enum
{
    BP_NONE = 0,
    BP_COMPARISON = 10,
    BP_ADDITIVE = 20,
    BP_MULTIPLICATIVE = 30
};

static Token peek(Compiler *c)
{
    return token_source_peek(&c->eval->token_source);
}

static void advance(Compiler *c)
{
    token_source_next(&c->eval->token_source);
}

static int get_infix_bp(TokenType type)
{
    switch (type)
    {
    case TOKEN_EQUALS:
    case TOKEN_LESS_THAN:
    case TOKEN_GREATER_THAN:
        return BP_COMPARISON;
    case TOKEN_PLUS:
    case TOKEN_MINUS:
        return BP_ADDITIVE;
    case TOKEN_MULTIPLY:
    case TOKEN_DIVIDE:
        return BP_MULTIPLICATIVE;
    default:
        return BP_NONE;
    }
}

static bool is_number_string(const char *str, size_t len)
{
    if (len == 0)
        return false;

    bool has_digit = false;
    for (size_t i = 0; i < len; i++)
    {
        char c = str[i];
        if (c >= '0' && c <= '9')
        {
            has_digit = true;
            continue;
        }
        if (c == '.' || c == '-' || c == 'E' || c == 'e' || c == 'N' || c == 'n')
            continue;
        return false;
    }
    return has_digit;
}

static float parse_number(const char *start, size_t length)
{
    if (length >= 63)
        length = 63;
    char buf[64];
    memcpy(buf, start, length);
    buf[length] = '\0';

    char *n_pos = strchr(buf, 'N');
    if (!n_pos)
        n_pos = strchr(buf, 'n');
    if (n_pos)
    {
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

static Node parse_list(Compiler *c)
{
    Node list = NODE_NIL;
    Node tail = NODE_NIL;

    while (true)
    {
        Token t = peek(c);
        if (t.type == TOKEN_EOF || t.type == TOKEN_RIGHT_BRACKET)
        {
            if (t.type == TOKEN_RIGHT_BRACKET)
                advance(c);
            break;
        }

        Node item = NODE_NIL;

        if (t.type == TOKEN_LEFT_BRACKET)
        {
            advance(c);
            item = parse_list(c);
            item = NODE_MAKE_LIST(NODE_GET_INDEX(item));
        }
        else if (t.type == TOKEN_WORD || t.type == TOKEN_NUMBER)
        {
            item = mem_atom(t.start, t.length);
            advance(c);
        }
        else if (t.type == TOKEN_QUOTED)
        {
            // Keep the quote - needed when list is run
            item = mem_atom(t.start, t.length);
            advance(c);
        }
        else if (t.type == TOKEN_COLON)
        {
            // Keep the colon - needed when list is run
            item = mem_atom(t.start, t.length);
            advance(c);
        }
        else if (t.type == TOKEN_PLUS || t.type == TOKEN_MINUS ||
                 t.type == TOKEN_UNARY_MINUS ||
                 t.type == TOKEN_MULTIPLY || t.type == TOKEN_DIVIDE ||
                 t.type == TOKEN_EQUALS || t.type == TOKEN_LESS_THAN ||
                 t.type == TOKEN_GREATER_THAN)
        {
            // Store operators as words in lists
            item = mem_atom(t.start, t.length);
            advance(c);
        }
        else if (t.type == TOKEN_LEFT_PAREN || t.type == TOKEN_RIGHT_PAREN)
        {
            item = mem_atom(t.start, t.length);
            advance(c);
        }
        else
        {
            advance(c);
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

static uint16_t bc_add_const(Bytecode *bc, Value v)
{
    if (!bc || !bc->const_pool || bc->const_len >= bc->const_cap)
        return UINT16_MAX;
    bc->const_pool[bc->const_len] = v;
    return (uint16_t)bc->const_len++;
}

static Result compile_expr_bp(Compiler *c, int min_bp);

static bool compiler_skip_expr_bp(TokenSource *ts, int min_bp);
static bool compiler_skip_primary(TokenSource *ts);

static bool compiler_skip_instruction(TokenSource *ts)
{
    if (token_source_at_end(ts))
        return false;
    return compiler_skip_expr_bp(ts, BP_NONE);
}

static bool compiler_skip_primary(TokenSource *ts)
{
    Token t = token_source_peek(ts);
    switch (t.type)
    {
    case TOKEN_NUMBER:
    case TOKEN_QUOTED:
    case TOKEN_COLON:
        token_source_next(ts);
        return true;
    case TOKEN_WORD:
    {
        if (is_number_string(t.start, t.length))
        {
            token_source_next(ts);
            return true;
        }

        char name_buf[64];
        size_t name_len = t.length;
        if (name_len >= sizeof(name_buf))
            name_len = sizeof(name_buf) - 1;
        memcpy(name_buf, t.start, name_len);
        name_buf[name_len] = '\0';

        const Primitive *prim = primitive_find(name_buf);
        if (prim)
        {
            token_source_next(ts);
            for (int i = 0; i < prim->default_args && !token_source_at_end(ts); i++)
            {
                Token next = token_source_peek(ts);
                if (next.type == TOKEN_RIGHT_PAREN || next.type == TOKEN_RIGHT_BRACKET)
                    break;
                if (!compiler_skip_expr_bp(ts, BP_NONE))
                    return false;
            }
            return true;
        }

        UserProcedure *user_proc = proc_find(name_buf);
        if (user_proc)
        {
            token_source_next(ts);
            for (int i = 0; i < user_proc->param_count && !token_source_at_end(ts); i++)
            {
                Token next = token_source_peek(ts);
                if (next.type == TOKEN_RIGHT_PAREN || next.type == TOKEN_RIGHT_BRACKET)
                    break;
                if (!compiler_skip_expr_bp(ts, BP_NONE))
                    return false;
            }
            return true;
        }

        token_source_next(ts);
        return true;
    }
    case TOKEN_LEFT_BRACKET:
    {
        token_source_next(ts);
        if (ts->type == TOKEN_SOURCE_NODE_ITERATOR)
        {
            Node sublist = token_source_get_sublist(ts);
            if (!mem_is_nil(sublist))
            {
                token_source_consume_sublist(ts);
                return true;
            }
        }

        int depth = 1;
        while (!token_source_at_end(ts) && depth > 0)
        {
            Token t2 = token_source_next(ts);
            if (t2.type == TOKEN_LEFT_BRACKET)
                depth++;
            else if (t2.type == TOKEN_RIGHT_BRACKET)
                depth--;
        }
        return true;
    }
    case TOKEN_LEFT_PAREN:
    {
        token_source_next(ts);
        if (!compiler_skip_expr_bp(ts, BP_NONE))
            return false;
        Token closing = token_source_peek(ts);
        if (closing.type == TOKEN_RIGHT_PAREN)
            token_source_next(ts);
        return true;
    }
    case TOKEN_MINUS:
    case TOKEN_UNARY_MINUS:
        token_source_next(ts);
        return compiler_skip_primary(ts);
    case TOKEN_RIGHT_PAREN:
    case TOKEN_RIGHT_BRACKET:
    case TOKEN_EOF:
        return false;
    default:
        token_source_next(ts);
        return true;
    }
}

static bool compiler_skip_expr_bp(TokenSource *ts, int min_bp)
{
    if (!compiler_skip_primary(ts))
        return false;

    while (true)
    {
        Token op = token_source_peek(ts);
        int bp = get_infix_bp(op.type);
        if (bp == BP_NONE || bp < min_bp)
            break;
        token_source_next(ts);
        if (!compiler_skip_expr_bp(ts, bp + 1))
            return false;
    }
    return true;
}

static Result compile_infix_tail(Compiler *c, int min_bp)
{
    while (true)
    {
        Token op = peek(c);
        int bp = get_infix_bp(op.type);
        if (bp == BP_NONE || bp < min_bp)
            break;

        advance(c);
        Result rhs = compile_expr_bp(c, bp + 1);
        if (rhs.status != RESULT_OK)
            return rhs;

        Op bop = OP_NOP;
        switch (op.type)
        {
        case TOKEN_EQUALS: bop = OP_EQ; break;
        case TOKEN_PLUS: bop = OP_ADD; break;
        case TOKEN_MINUS: bop = OP_SUB; break;
        case TOKEN_MULTIPLY: bop = OP_MUL; break;
        case TOKEN_DIVIDE: bop = OP_DIV; break;
        case TOKEN_LESS_THAN: bop = OP_LT; break;
        case TOKEN_GREATER_THAN: bop = OP_GT; break;
        default: bop = OP_NOP; break;
        }

        if (!bc_emit(c->bc, bop, 0, 0))
            return result_error(ERR_OUT_OF_SPACE);
    }
    return result_ok(value_none());
}

static Result compile_primary(Compiler *c)
{
    Token t = peek(c);

    switch (t.type)
    {
    case TOKEN_NUMBER:
    {
        advance(c);
        uint16_t idx = bc_add_const(c->bc, value_number(parse_number(t.start, t.length)));
        if (idx == UINT16_MAX || !bc_emit(c->bc, OP_PUSH_CONST, idx, 0))
            return result_error(ERR_OUT_OF_SPACE);
        return result_ok(value_none());
    }
    case TOKEN_QUOTED:
    {
        advance(c);
        Node atom = mem_atom_unescape(t.start + 1, t.length - 1);
        uint16_t idx = bc_add_const(c->bc, value_word(atom));
        if (idx == UINT16_MAX || !bc_emit(c->bc, OP_PUSH_CONST, idx, 0))
            return result_error(ERR_OUT_OF_SPACE);
        return result_ok(value_none());
    }
    case TOKEN_COLON:
    {
        advance(c);
        Node name_atom = mem_atom_unescape(t.start + 1, t.length - 1);
        uint16_t idx = bc_add_const(c->bc, value_word(name_atom));
        if (idx == UINT16_MAX || !bc_emit(c->bc, OP_LOAD_VAR, idx, 0))
            return result_error(ERR_OUT_OF_SPACE);
        return result_ok(value_none());
    }
    case TOKEN_LEFT_BRACKET:
    {
        advance(c);
        if (c->eval->token_source.type == TOKEN_SOURCE_NODE_ITERATOR)
        {
            Node sublist = token_source_get_sublist(&c->eval->token_source);
            if (!mem_is_nil(sublist))
            {
                token_source_consume_sublist(&c->eval->token_source);
                if (NODE_GET_TYPE(sublist) == NODE_TYPE_LIST)
                    sublist = NODE_MAKE_LIST(NODE_GET_INDEX(sublist));
                uint16_t idx = bc_add_const(c->bc, value_list(sublist));
                if (idx == UINT16_MAX || !bc_emit(c->bc, OP_PUSH_CONST, idx, 0))
                    return result_error(ERR_OUT_OF_SPACE);
                return result_ok(value_none());
            }
        }

        Node list = parse_list(c);
        uint16_t idx = bc_add_const(c->bc, value_list(list));
        if (idx == UINT16_MAX || !bc_emit(c->bc, OP_PUSH_CONST, idx, 0))
            return result_error(ERR_OUT_OF_SPACE);
        return result_ok(value_none());
    }
    case TOKEN_LEFT_PAREN:
    {
        advance(c);
        c->eval->paren_depth++;

        Token next = peek(c);
        if (next.type == TOKEN_WORD && !is_number_string(next.start, next.length))
        {
            char name_buf[64];
            size_t name_len = next.length;
            if (name_len >= sizeof(name_buf))
                name_len = sizeof(name_buf) - 1;
            memcpy(name_buf, next.start, name_len);
            name_buf[name_len] = '\0';

            const Primitive *prim = primitive_find(name_buf);
            if (prim)
            {
                Node user_name_atom = mem_atom(next.start, next.length);
                const char *user_name = mem_word_ptr(user_name_atom);

                advance(c);

                if (prim->default_args == 0)
                {
                    Token after = peek(c);
                    int bp = get_infix_bp(after.type);
                    if (bp != BP_NONE)
                    {
                        uint16_t name_idx = bc_add_const(c->bc, value_word(user_name_atom));
                        Op call_op = c->instruction_mode ? OP_CALL_PRIM_INSTR : OP_CALL_PRIM;
                        if (name_idx == UINT16_MAX || !bc_emit(c->bc, call_op, name_idx, 0))
                            return result_error(ERR_OUT_OF_SPACE);

                        Result tail = compile_infix_tail(c, BP_NONE);
                        if (tail.status != RESULT_OK)
                            return tail;

                        Token closing = peek(c);
                        if (closing.type == TOKEN_RIGHT_PAREN)
                            advance(c);
                        c->eval->paren_depth--;
                        return result_ok(value_none());
                    }
                }

                int argc = 0;
                if (!bc_emit(c->bc, OP_PRIM_ARGS_BEGIN, 0, 0))
                    return result_error(ERR_OUT_OF_SPACE);
                c->eval->primitive_arg_depth++;

                while (argc < 16)
                {
                    Token t2 = peek(c);
                    if (t2.type == TOKEN_RIGHT_PAREN || t2.type == TOKEN_EOF)
                        break;

                    Result arg = compile_expr_bp(c, BP_NONE);
                    if (arg.status != RESULT_OK)
                    {
                        c->eval->primitive_arg_depth--;
                        c->eval->paren_depth--;
                        return result_set_error_proc(arg, user_name);
                    }
                    argc++;
                }

                c->eval->primitive_arg_depth--;
                if (!bc_emit(c->bc, OP_PRIM_ARGS_END, 0, 0))
                    return result_error(ERR_OUT_OF_SPACE);

                Token closing = peek(c);
                if (closing.type == TOKEN_RIGHT_PAREN)
                    advance(c);
                c->eval->paren_depth--;

                uint16_t name_idx = bc_add_const(c->bc, value_word(user_name_atom));
                Op call_op = c->instruction_mode ? OP_CALL_PRIM_INSTR : OP_CALL_PRIM;
                if (name_idx == UINT16_MAX || !bc_emit(c->bc, call_op, name_idx, (uint16_t)argc))
                    return result_error(ERR_OUT_OF_SPACE);

                return result_ok(value_none());
            }
        }

        Result r = compile_expr_bp(c, BP_NONE);
        if (r.status != RESULT_OK)
        {
            c->eval->paren_depth--;
            return r;
        }
        Token closing = peek(c);
        if (closing.type == TOKEN_RIGHT_PAREN)
            advance(c);
        c->eval->paren_depth--;
        return result_ok(value_none());
    }
    case TOKEN_MINUS:
    case TOKEN_UNARY_MINUS:
    {
        advance(c);
        Result r = compile_primary(c);
        if (r.status != RESULT_OK)
            return r;
        if (!bc_emit(c->bc, OP_NEG, 0, 0))
            return result_error(ERR_OUT_OF_SPACE);
        return result_ok(value_none());
    }
    case TOKEN_WORD:
    {
        if (is_number_string(t.start, t.length))
        {
            advance(c);
            uint16_t idx = bc_add_const(c->bc, value_number(parse_number(t.start, t.length)));
            if (idx == UINT16_MAX || !bc_emit(c->bc, OP_PUSH_CONST, idx, 0))
                return result_error(ERR_OUT_OF_SPACE);
            return result_ok(value_none());
        }

        char name_buf[64];
        size_t name_len = t.length;
        if (name_len >= sizeof(name_buf))
            name_len = sizeof(name_buf) - 1;
        memcpy(name_buf, t.start, name_len);
        name_buf[name_len] = '\0';

        const Primitive *prim = primitive_find(name_buf);
        if (prim)
        {
            Node user_name_atom = mem_atom(t.start, t.length);
            const char *user_name = mem_word_ptr(user_name_atom);

            advance(c);
            int argc = 0;

            c->eval->primitive_arg_depth++;
            if (!bc_emit(c->bc, OP_PRIM_ARGS_BEGIN, 0, 0))
                return result_error(ERR_OUT_OF_SPACE);

            for (int i = 0; i < prim->default_args; i++)
            {
                Token next = peek(c);
                if (next.type == TOKEN_RIGHT_PAREN || next.type == TOKEN_RIGHT_BRACKET || next.type == TOKEN_EOF)
                {
                    c->eval->primitive_arg_depth--;
                    return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_name, NULL);
                }

                Result arg = compile_expr_bp(c, BP_NONE);
                if (arg.status != RESULT_OK)
                {
                    c->eval->primitive_arg_depth--;
                    return result_set_error_proc(arg, user_name);
                }
                argc++;
            }

            c->eval->primitive_arg_depth--;
            if (!bc_emit(c->bc, OP_PRIM_ARGS_END, 0, 0))
                return result_error(ERR_OUT_OF_SPACE);

            uint16_t name_idx = bc_add_const(c->bc, value_word(user_name_atom));
            Op call_op = c->instruction_mode ? OP_CALL_PRIM_INSTR : OP_CALL_PRIM;
            if (name_idx == UINT16_MAX || !bc_emit(c->bc, call_op, name_idx, (uint16_t)argc))
                return result_error(ERR_OUT_OF_SPACE);
            return result_ok(value_none());
        }

        UserProcedure *user_proc = proc_find(name_buf);
        if (user_proc)
        {
            Node user_name_atom = mem_atom(t.start, t.length);
            advance(c);

            int argc = 0;
            for (int i = 0; i < user_proc->param_count && !token_source_at_end(&c->eval->token_source); i++)
            {
                Token next = peek(c);
                if (next.type == TOKEN_RIGHT_PAREN || next.type == TOKEN_RIGHT_BRACKET || next.type == TOKEN_EOF)
                {
                    return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_proc->name, NULL);
                }

                Result arg = compile_expr_bp(c, BP_NONE);
                if (arg.status != RESULT_OK)
                {
                    return result_set_error_proc(arg, user_proc->name);
                }
                argc++;
            }

            if (argc < user_proc->param_count)
            {
                return result_error_arg(ERR_NOT_ENOUGH_INPUTS, user_proc->name, NULL);
            }

            uint16_t name_idx = bc_add_const(c->bc, value_word(user_name_atom));
            Op call_op = OP_CALL_USER_EXPR;
            if (c->instruction_mode)
                call_op = (c->tail_position && c->tail_depth == 1) ? OP_CALL_USER_TAIL : OP_CALL_USER;

            if (name_idx == UINT16_MAX || !bc_emit(c->bc, call_op, name_idx, (uint16_t)argc))
                return result_error(ERR_OUT_OF_SPACE);
            return result_ok(value_none());
        }

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
        Node token_atom = mem_atom(t.start, t.length);
        return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, mem_word_ptr(token_atom));
    }
    }
}

static Result compile_expr_bp(Compiler *c, int min_bp)
{
    c->tail_depth++;
    Result lhs = compile_primary(c);
    if (lhs.status != RESULT_OK)
    {
        c->tail_depth--;
        return lhs;
    }
    Result r = compile_infix_tail(c, min_bp);
    c->tail_depth--;
    return r;
}

Result compile_expression(Compiler *c, Bytecode *bc)
{
    if (!c || !c->eval || !bc)
        return result_error(ERR_UNSUPPORTED_ON_DEVICE);
    c->bc = bc;
    c->instruction_mode = false;
    c->tail_position = false;
    c->tail_depth = 0;
    return compile_expr_bp(c, BP_NONE);
}

Result compile_instruction(Compiler *c, Bytecode *bc)
{
    (void)c;
    (void)bc;
    return result_error(ERR_UNSUPPORTED_ON_DEVICE);
}

Result compile_list(Compiler *c, Node list, Bytecode *bc)
{
    if (!c || !c->eval || !bc)
        return result_error(ERR_UNSUPPORTED_ON_DEVICE);

    TokenSource old_source = c->eval->token_source;
    token_source_init_list(&c->eval->token_source, list);

    c->bc = bc;
    c->instruction_mode = false;
    c->tail_position = false;
    c->tail_depth = 0;
    Result r = compile_expr_bp(c, BP_NONE);

    if (r.status == RESULT_OK && !token_source_at_end(&c->eval->token_source))
    {
        r = result_error(ERR_UNSUPPORTED_ON_DEVICE);
    }

    c->eval->token_source = old_source;
    return r;
}

Result compile_list_instructions(Compiler *c, Node list, Bytecode *bc, bool enable_tco)
{
    if (!c || !c->eval || !bc)
        return result_error(ERR_UNSUPPORTED_ON_DEVICE);

    TokenSource old_source = c->eval->token_source;
    token_source_init_list(&c->eval->token_source, list);

    c->bc = bc;
    c->instruction_mode = true;
    c->tail_position = false;
    c->tail_depth = 0;

    Result r = result_none();
    while (!token_source_at_end(&c->eval->token_source))
    {
        if (enable_tco)
        {
            TokenSource lookahead = c->eval->token_source;
            bool last_instruction = compiler_skip_instruction(&lookahead) && token_source_at_end(&lookahead);
            c->tail_position = last_instruction;
        }
        else
        {
            c->tail_position = false;
        }

        if (!bc_emit(c->bc, OP_BEGIN_INSTR, (uint16_t)(c->tail_position ? 1 : 0), 0))
        {
            r = result_error(ERR_OUT_OF_SPACE);
            break;
        }

        r = compile_expr_bp(c, BP_NONE);
        if (r.status != RESULT_OK)
            break;
        if (!bc_emit(c->bc, OP_END_INSTR, 0, 0))
        {
            r = result_error(ERR_OUT_OF_SPACE);
            break;
        }
    }

    c->eval->token_source = old_source;
    return r.status == RESULT_OK ? result_none() : r;
}
