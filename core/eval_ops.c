//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "eval_ops.h"
#include <string.h>

void op_stack_init(OpStack *stack)
{
    stack->top = 0;
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
        stack->top--;
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
