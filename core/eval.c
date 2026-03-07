//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Core evaluator: trampoline dispatch, instruction evaluation, and
//  operation push helpers for primitives.
//

#include "eval_internal.h"
#include "error.h"
#include "primitives.h"
#include "procedures.h"
#include "repl.h"
#include "frame.h"
#include "devices/io.h"
#include <string.h>

// Global operation stack (shared by all evaluators)
OpStack global_op_stack;

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

// Check if at end of input
bool eval_at_end(Evaluator *eval)
{
    Token t = peek(eval);
    return t.type == TOKEN_EOF;
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
