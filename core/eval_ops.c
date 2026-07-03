//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//

#include "eval_ops.h"
#include <string.h>

void op_stack_init(OpStack *stack)
{
    stack->top = 0;
    stack->prim_arg_top = 0;
}

EvalOp *op_stack_push(OpStack *stack)
{
    if (stack->top >= MAX_OP_STACK_DEPTH)
        return NULL;
    EvalOp *op = &stack->ops[stack->top++];
    memset(op, 0, sizeof(EvalOp));
    op->result = result_none();
    return op;
}

void op_stack_pop(OpStack *stack)
{
    if (stack->top > 0)
    {
        EvalOp *op = &stack->ops[stack->top - 1];
        if (op->kind == OP_PRIM_CALL && op->prim_call.arg_base >= 0)
        {
            int expected_top = op->prim_call.arg_base + op->prim_call.arg_capacity;
            if (stack->prim_arg_top == expected_top)
                stack->prim_arg_top = op->prim_call.arg_base;
        }
        stack->top--;
    }
}

EvalOp *op_stack_peek(OpStack *stack)
{
    if (stack->top == 0)
        return NULL;
    return &stack->ops[stack->top - 1];
}

bool op_stack_is_empty(OpStack *stack)
{
    return stack->top == 0;
}

int op_stack_depth(OpStack *stack)
{
    return stack->top;
}

void op_stack_swap_top(OpStack *stack)
{
    if (stack->top < 2)
        return;
    EvalOp tmp = stack->ops[stack->top - 1];
    stack->ops[stack->top - 1] = stack->ops[stack->top - 2];
    stack->ops[stack->top - 2] = tmp;
}

int op_stack_alloc_prim_args(OpStack *stack, int capacity)
{
    if (capacity <= 0 || stack->prim_arg_top + capacity > MAX_PRIM_ARG_SPILL_VALUES)
        return -1;

    int base = stack->prim_arg_top;
    stack->prim_arg_top += capacity;
    return base;
}

Value *op_stack_get_prim_args(OpStack *stack, int base)
{
    if (base < 0 || base >= MAX_PRIM_ARG_SPILL_VALUES)
        return NULL;
    return &stack->prim_arg_spill[base];
}

//==========================================================================
// GC root marking
//==========================================================================

static void mark_value(Value v)
{
    if (v.type == VALUE_WORD || v.type == VALUE_LIST)
        mem_gc_mark(v.as.node);
}

void op_stack_gc_mark(OpStack *stack)
{
    for (int i = 0; i < stack->top; i++)
    {
        EvalOp *op = &stack->ops[i];

        token_source_gc_mark(&op->saved_source);
        mark_value(op->result.value);

        switch (op->kind)
        {
        case OP_REPEAT:
            mem_gc_mark(op->repeat.body);
            break;
        case OP_FOREVER:
            mem_gc_mark(op->forever.body);
            break;
        case OP_IF:
            mem_gc_mark(op->if_state.chosen_branch);
            break;
        case OP_WHILE:
        case OP_UNTIL:
        case OP_DO_WHILE:
        case OP_DO_UNTIL:
            mem_gc_mark(op->loop.predicate);
            mem_gc_mark(op->loop.body);
            break;
        case OP_FOR:
            mem_gc_mark(op->for_state.body);
            mark_value(op->for_state.saved_value);
            break;
        case OP_CATCH:
            mem_gc_mark(op->catch_state.body);
            break;
        case OP_PROC_CALL:
            // The body is marked via the procedure table; the cursor keeps
            // the old body alive if the procedure was redefined mid-call.
            mem_gc_mark(op->proc_call.current_line);
            break;
        case OP_EXPR_EVAL:
            for (int j = 0; j < op->expr_eval.depth; j++)
                mark_value(op->expr_eval.ops[j].left);
            break;
        case OP_PRIM_CALL:
        {
            Value *args = op->prim_call.arg_base >= 0
                ? &stack->prim_arg_spill[op->prim_call.arg_base]
                : op->prim_call.args;
            for (int j = 0; j < op->prim_call.argc; j++)
                mark_value(args[j]);
            break;
        }
        default:
            // OP_RUN_LIST / OP_RUN_LIST_EXPR: saved_source only
            break;
        }
    }
}
