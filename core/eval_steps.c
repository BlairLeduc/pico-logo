//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Step functions for the trampoline-based evaluator.
//  Each step function handles one operation kind from the op stack.
//

#include "eval_internal.h"
#include "error.h"
#include "primitives.h"
#include "procedures.h"
#include "variables.h"
#include "format.h"
#include "frame.h"
#include "repl.h"
#include "devices/io.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

//==========================================================================
// Skip/peek helpers for tail-call lookahead (no execution)
//==========================================================================

// Forward declarations
static bool skip_expr_bp(Evaluator *eval, int min_bp);

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
    }

    case TOKEN_LEFT_PAREN:
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
static bool skip_instruction(Evaluator *eval)
{
    if (eval_at_end(eval))
        return false;
    
    return skip_expr_bp(eval, BP_NONE);
}

//==========================================================================
// Step functions
//==========================================================================

// Step function for OP_RUN_LIST and OP_RUN_LIST_EXPR.
// Processes one instruction per call, or handles a child op result.
Result step_run_list(Evaluator *eval, EvalOp *op)
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

// Step function for OP_IF.
// Phase 0: push child OP_RUN_LIST/OP_RUN_LIST_EXPR for the chosen branch.
// Phase 1: child completed — return its result.
Result step_if(Evaluator *eval, EvalOp *op)
{
    IfState *st = &op->if_state;

    if (st->phase == 1)
    {
        // Phase 1: child completed
        Result child_r = op->result;
        op->result = result_none();
        op_stack_pop(eval->op_stack);
        return child_r;
    }

    // Phase 0: push the chosen branch as a child run-list
    st->phase = 1;
    bool is_expr = (op->flags & OP_FLAG_IS_EXPR) != 0;
    bool enable_tco = (op->flags & OP_FLAG_ENABLE_TCO) != 0;

    EvalOp *body_op = op_stack_push(eval->op_stack);
    if (!body_op)
    {
        op_stack_pop(eval->op_stack);
        return result_error(ERR_STACK_OVERFLOW);
    }
    body_op->kind = is_expr ? OP_RUN_LIST_EXPR : OP_RUN_LIST;
    body_op->flags = enable_tco ? OP_FLAG_ENABLE_TCO : OP_FLAG_NONE;
    body_op->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, st->chosen_branch);
    return result_none();
}

// Step function for OP_REPEAT.
Result step_repeat(Evaluator *eval, EvalOp *op)
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
Result step_forever(Evaluator *eval, EvalOp *op)
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

//==========================================================================
// Predicate-driven loops (while / until / do.while / do.until)
//==========================================================================

#define LOOP_PHASE_START     0  // Nothing run yet
#define LOOP_PHASE_PREDICATE 1  // Predicate child is running
#define LOOP_PHASE_BODY      2  // Body child is running

// Interpret a predicate child result as a boolean word. Returns RESULT_NONE
// with *out set on success; otherwise the error Result to propagate.
static Result read_predicate_bool(Result child_r, const char *name, bool *out)
{
    if (child_r.status == RESULT_ERROR)
    {
        return child_r;
    }
    if (child_r.status != RESULT_OK)
    {
        return result_error_arg(ERR_NOT_BOOL, name, NULL);
    }
    if (!value_is_word(child_r.value))
    {
        return result_error_arg(ERR_NOT_BOOL, name, value_to_string(child_r.value));
    }
    const char *str = value_to_string(child_r.value);
    if (strcasecmp(str, "true") == 0)
    {
        *out = true;
        return result_none();
    }
    if (strcasecmp(str, "false") == 0)
    {
        *out = false;
        return result_none();
    }
    return result_error_arg(ERR_NOT_BOOL, name, str);
}

// Push a child op that runs `list`, saving the current token source.
// On stack overflow, pops the loop op itself and returns the error.
static Result push_loop_child(Evaluator *eval, EvalOpKind kind, Node list)
{
    EvalOp *child = op_stack_push(eval->op_stack);
    if (!child)
    {
        op_stack_pop(eval->op_stack);
        return result_error(ERR_STACK_OVERFLOW);
    }
    child->kind = kind;
    child->flags = OP_FLAG_NONE;
    child->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, list);
    return result_none();
}

// Shared step function for OP_WHILE, OP_UNTIL, OP_DO_WHILE, and OP_DO_UNTIL.
// while/until evaluate the predicate before the first body run; the do.*
// forms run the body once first. while/do.while keep looping as long as the
// predicate is true; until/do.until stop as soon as it becomes true.
Result step_loop(Evaluator *eval, EvalOp *op)
{
    LoopState *st = &op->loop;

    const char *name;
    bool body_first;
    bool continue_on;
    switch (op->kind)
    {
    case OP_WHILE:    name = "while";    body_first = false; continue_on = true;  break;
    case OP_UNTIL:    name = "until";    body_first = false; continue_on = false; break;
    case OP_DO_WHILE: name = "do.while"; body_first = true;  continue_on = true;  break;
    default:          name = "do.until"; body_first = true;  continue_on = false; break;
    }

    if (st->phase == LOOP_PHASE_PREDICATE)
    {
        Result child_r = op->result;
        op->result = result_none();

        bool pred;
        Result br = read_predicate_bool(child_r, name, &pred);
        if (br.status != RESULT_NONE)
        {
            op_stack_pop(eval->op_stack);
            return br;
        }
        if (pred != continue_on)
        {
            // Loop finished
            op_stack_pop(eval->op_stack);
            return result_none();
        }
        st->phase = LOOP_PHASE_BODY;
        return push_loop_child(eval, OP_RUN_LIST, st->body);
    }

    if (st->phase == LOOP_PHASE_BODY)
    {
        Result child_r = op->result;
        op->result = result_none();

        // Propagate stop/output/error/throw from the body
        if (child_r.status != RESULT_NONE && child_r.status != RESULT_OK)
        {
            op_stack_pop(eval->op_stack);
            return child_r;
        }
        // Body completed: fall through to evaluate the predicate.
    }
    else if (body_first)
    {
        // LOOP_PHASE_START for do.while / do.until: run the body once first.
        st->phase = LOOP_PHASE_BODY;
        return push_loop_child(eval, OP_RUN_LIST, st->body);
    }

    st->phase = LOOP_PHASE_PREDICATE;
    return push_loop_child(eval, OP_RUN_LIST_EXPR, st->predicate);
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
Result step_for(Evaluator *eval, EvalOp *op)
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
Result step_catch(Evaluator *eval, EvalOp *op)
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

// Step function for OP_RUNRESULT.
// Runs the body as an expression, then wraps the outcome: a value becomes a
// one-member list [value]; a command list that produces nothing becomes [].
Result step_runresult(Evaluator *eval, EvalOp *op)
{
    RunResultState *st = &op->runresult;

    if (st->phase == 1)
    {
        // Body completed
        Result child_r = op->result;
        op->result = result_none();

        op_stack_pop(eval->op_stack);

        if (child_r.status == RESULT_OK)
        {
            // The body output a value: wrap it in a one-member list.
            Node elem;
            if (child_r.value.type == VALUE_NUMBER)
            {
                elem = number_to_word(child_r.value.as.number);
                if (mem_is_nil(elem))
                    return result_error(ERR_OUT_OF_SPACE);
            }
            else
            {
                elem = child_r.value.as.node;  // word or list node
            }
            Node cell = mem_cons(elem, NODE_NIL);
            if (mem_is_nil(cell))
                return result_error(ERR_OUT_OF_SPACE);
            return result_ok(value_list(cell));
        }
        if (child_r.status == RESULT_NONE)
        {
            // The body ran commands but produced no value: the empty list.
            return result_ok(value_list(NODE_NIL));
        }
        // stop / output / error / throw propagate unchanged.
        return child_r;
    }

    // Phase 0: push body as an expression
    st->phase = 1;
    EvalOp *body_op = op_stack_push(eval->op_stack);
    if (!body_op) { op_stack_pop(eval->op_stack); return result_error(ERR_STACK_OVERFLOW); }
    body_op->kind = OP_RUN_LIST_EXPR;
    body_op->flags = OP_FLAG_NONE;
    body_op->saved_source = eval->token_source;
    token_source_init_list(&eval->token_source, st->body);
    return result_none();
}

//==========================================================================
// Trace helpers
//==========================================================================

static void eval_trace_write(const char *str)
{
    LogoIO *io = primitives_get_io();
    if (io)
        logo_io_write(io, str);
}

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

//==========================================================================
// Procedure call step function
//==========================================================================

static Result proc_call_cleanup(Evaluator *eval, EvalOp *op, Result body_result)
{
    ProcCallState *st = &op->proc_call;

    // Bare tail call that produced a value via output — this is an error.
    // Without TCO, the bare call on the last line would have returned the
    // value to a non-expression OP_RUN_LIST context, which reports
    // "You don't say what to do with..."
    if (body_result.status == RESULT_OUTPUT && st->tco_mode == TCO_MODE_BARE)
    {
        body_result = result_error_arg(ERR_DONT_KNOW_WHAT, NULL,
                                       value_to_string(body_result.value));
    }

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

Result step_proc_call(Evaluator *eval, EvalOp *op)
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

                    // Track TCO context: bare call vs output call.
                    // Only set on FIRST hop — preserves the original entry
                    // context. A bare call that later hops through an output
                    // branch must still error if a value is produced.
                    if (st->tco_mode == TCO_MODE_NONE)
                        st->tco_mode = tc->is_output_call ? TCO_MODE_OUTPUT
                                                          : TCO_MODE_BARE;
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

//==========================================================================
// Expression evaluation resume and deferred primitive call
//==========================================================================

Result step_expr_eval(Evaluator *eval, EvalOp *op)
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

Result step_prim_call(Evaluator *eval, EvalOp *op)
{
    PrimCallState *st = &op->prim_call;
    const struct Primitive *prim = st->prim;
    Value *args = st->arg_base >= 0
        ? op_stack_get_prim_args(eval->op_stack, st->arg_base)
        : st->args;

    if (!args)
    {
        op_stack_pop(eval->op_stack);
        return result_error(ERR_STACK_OVERFLOW);
    }

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
    if (arg_result.status == RESULT_NONE)
    {
        op_stack_pop(eval->op_stack);
        return result_none();
    }

    // Store the argument value
    int idx = st->current_arg;
    if (idx >= st->arg_capacity)
    {
        op_stack_pop(eval->op_stack);
        return result_error(ERR_TOO_MUCH_PARENS);
    }
    args[idx] = arg_result.value;
    st->argc = idx + 1;
    st->current_arg = idx + 1;

    // Collect remaining args
    while (st->total_args == -1 || (st->total_args > 0 && st->argc < st->total_args))
    {
        // For varargs, stop at closing paren or end of input
        if (st->total_args == -1)
        {
            Token t = peek(eval);
            if (t.type == TOKEN_RIGHT_PAREN || t.type == TOKEN_EOF)
                break;
        }

        // Need more args. Evaluate the next expression.
        eval->primitive_arg_depth++;
        bool old_tail = eval->in_tail_position;
        // Restore the per-arg tail position captured at deferral time.
        // For output/op this is true (preserves the tail-position exception);
        // for every other primitive it is false. Today no built-in primitive
        // both has multiple args AND wants tail-position semantics, so this
        // loop never executes for the output/op case (output has 1 arg). The
        // saved value is plumbed through anyway so future multi-arg primitives
        // with tail-position semantics inherit the correct context, instead
        // of silently losing it to a hardcoded `false`.
        eval->in_tail_position = st->saved_in_tail_position;

        int depth_before = op_stack_depth(eval->op_stack);
        Result next_arg = eval_expression(eval);

        eval->in_tail_position = old_tail;

        // Check for another deferral (async proc call pushed above us)
        if (next_arg.status == RESULT_NONE &&
            op_stack_depth(eval->op_stack) > depth_before)
        {
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
            if (st->total_args == -1)
                break; // varargs: end of args
            op_stack_pop(eval->op_stack);
            return result_error_arg(ERR_NOT_ENOUGH_INPUTS, st->user_name, NULL);
        }

        if (st->current_arg >= st->arg_capacity)
        {
            op_stack_pop(eval->op_stack);
            return result_error(ERR_TOO_MUCH_PARENS);
        }
        args[st->current_arg] = next_arg.value;
        st->argc++;
        st->current_arg++;
    }

    // For varargs paren calls, consume the closing ')' and decrement paren_depth
    if (st->total_args == -1)
    {
        Token closing = peek(eval);
        if (closing.type == TOKEN_RIGHT_PAREN)
            advance(eval);
        eval->paren_depth--;
    }

    // All args collected — call the primitive
    Value call_args[MAX_PRIM_ARGS];
    for (int i = 0; i < st->argc && i < MAX_PRIM_ARGS; i++)
        call_args[i] = args[i];
    int argc = st->argc;
    const char *user_name = st->user_name;

    op_stack_pop(eval->op_stack);
    Result r = prim->func(eval, argc, call_args);
    return result_set_error_proc(r, user_name);
}
