//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the operation stack (eval_ops).
//

#include "unity.h"
#include "core/eval_ops.h"

#include <string.h>

static OpStack stack;

void setUp(void)
{
    op_stack_init(&stack);
}

void tearDown(void)
{
    // Nothing to tear down
}

//============================================================================
// Basic Tests
//============================================================================

void test_init_is_empty(void)
{
    TEST_ASSERT_TRUE(op_stack_is_empty(&stack));
    TEST_ASSERT_EQUAL(0, op_stack_depth(&stack));
}

void test_push_returns_non_null(void)
{
    EvalOp *op = op_stack_push(&stack);
    TEST_ASSERT_NOT_NULL(op);
    TEST_ASSERT_EQUAL(1, op_stack_depth(&stack));
}

void test_peek_returns_top(void)
{
    EvalOp *pushed = op_stack_push(&stack);
    pushed->kind = OP_REPEAT;
    EvalOp *peeked = op_stack_peek(&stack);
    TEST_ASSERT_EQUAL_PTR(pushed, peeked);
    TEST_ASSERT_EQUAL(OP_REPEAT, peeked->kind);
}

void test_peek_empty_returns_null(void)
{
    TEST_ASSERT_NULL(op_stack_peek(&stack));
}

void test_pop_decreases_depth(void)
{
    op_stack_push(&stack);
    op_stack_push(&stack);
    TEST_ASSERT_EQUAL(2, op_stack_depth(&stack));
    op_stack_pop(&stack);
    TEST_ASSERT_EQUAL(1, op_stack_depth(&stack));
}

void test_pop_empty_is_safe(void)
{
    op_stack_pop(&stack);
    TEST_ASSERT_TRUE(op_stack_is_empty(&stack));
    TEST_ASSERT_EQUAL(0, op_stack_depth(&stack));
}

void test_push_pop_returns_to_empty(void)
{
    op_stack_push(&stack);
    op_stack_pop(&stack);
    TEST_ASSERT_TRUE(op_stack_is_empty(&stack));
}

//============================================================================
// Swap Tests
//============================================================================

void test_swap_top_two(void)
{
    EvalOp *a = op_stack_push(&stack);
    a->kind = OP_REPEAT;
    EvalOp *b = op_stack_push(&stack);
    b->kind = OP_IF;

    op_stack_swap_top(&stack);

    EvalOp *top = op_stack_peek(&stack);
    TEST_ASSERT_EQUAL(OP_REPEAT, top->kind);

    op_stack_pop(&stack);
    EvalOp *second = op_stack_peek(&stack);
    TEST_ASSERT_EQUAL(OP_IF, second->kind);
}

void test_swap_single_element_no_crash(void)
{
    op_stack_push(&stack);
    op_stack_swap_top(&stack); // Should be safe with < 2 elements
    TEST_ASSERT_EQUAL(1, op_stack_depth(&stack));
}

void test_swap_empty_no_crash(void)
{
    op_stack_swap_top(&stack); // Should be safe when empty
    TEST_ASSERT_TRUE(op_stack_is_empty(&stack));
}

//============================================================================
// Stack Overflow Tests
//============================================================================

void test_push_until_full_returns_null(void)
{
    int pushed = 0;
    for (int i = 0; i < MAX_OP_STACK_DEPTH; i++)
    {
        EvalOp *op = op_stack_push(&stack);
        TEST_ASSERT_NOT_NULL(op);
        pushed++;
    }
    TEST_ASSERT_EQUAL(MAX_OP_STACK_DEPTH, pushed);
    TEST_ASSERT_EQUAL(MAX_OP_STACK_DEPTH, op_stack_depth(&stack));

    // The next push should fail
    EvalOp *overflow = op_stack_push(&stack);
    TEST_ASSERT_NULL(overflow);
    TEST_ASSERT_EQUAL(MAX_OP_STACK_DEPTH, op_stack_depth(&stack));
}

void test_push_at_capacity_does_not_corrupt(void)
{
    // Fill the stack, tagging each entry
    for (int i = 0; i < MAX_OP_STACK_DEPTH; i++)
    {
        EvalOp *op = op_stack_push(&stack);
        op->kind = (i % 2 == 0) ? OP_REPEAT : OP_IF;
    }

    // Attempt overflow
    TEST_ASSERT_NULL(op_stack_push(&stack));

    // Verify top entry is intact
    EvalOp *top = op_stack_peek(&stack);
    TEST_ASSERT_NOT_NULL(top);
    EvalOpKind expected_kind = ((MAX_OP_STACK_DEPTH - 1) % 2 == 0) ? OP_REPEAT : OP_IF;
    TEST_ASSERT_EQUAL(expected_kind, top->kind);
}

void test_multiple_overflow_attempts_safe(void)
{
    // Fill the stack
    for (int i = 0; i < MAX_OP_STACK_DEPTH; i++)
    {
        op_stack_push(&stack);
    }

    // Try to overflow multiple times
    for (int i = 0; i < 10; i++)
    {
        TEST_ASSERT_NULL(op_stack_push(&stack));
        TEST_ASSERT_EQUAL(MAX_OP_STACK_DEPTH, op_stack_depth(&stack));
    }
}

void test_pop_after_overflow_then_push_succeeds(void)
{
    // Fill the stack
    for (int i = 0; i < MAX_OP_STACK_DEPTH; i++)
    {
        op_stack_push(&stack);
    }
    TEST_ASSERT_NULL(op_stack_push(&stack));

    // Pop one
    op_stack_pop(&stack);
    TEST_ASSERT_EQUAL(MAX_OP_STACK_DEPTH - 1, op_stack_depth(&stack));

    // Now push should succeed again
    EvalOp *op = op_stack_push(&stack);
    TEST_ASSERT_NOT_NULL(op);
    TEST_ASSERT_EQUAL(MAX_OP_STACK_DEPTH, op_stack_depth(&stack));
}

void test_fill_drain_refill(void)
{
    // Fill completely
    for (int i = 0; i < MAX_OP_STACK_DEPTH; i++)
    {
        TEST_ASSERT_NOT_NULL(op_stack_push(&stack));
    }
    TEST_ASSERT_NULL(op_stack_push(&stack));

    // Drain completely
    for (int i = 0; i < MAX_OP_STACK_DEPTH; i++)
    {
        op_stack_pop(&stack);
    }
    TEST_ASSERT_TRUE(op_stack_is_empty(&stack));

    // Fill again — should work identically
    for (int i = 0; i < MAX_OP_STACK_DEPTH; i++)
    {
        TEST_ASSERT_NOT_NULL(op_stack_push(&stack));
    }
    TEST_ASSERT_NULL(op_stack_push(&stack));
    TEST_ASSERT_EQUAL(MAX_OP_STACK_DEPTH, op_stack_depth(&stack));
}

void test_push_zeroes_new_entry(void)
{
    EvalOp *op = op_stack_push(&stack);
    // op_stack_push does memset(0) on the new entry
    TEST_ASSERT_EQUAL(0, op->kind);
    TEST_ASSERT_EQUAL(0, op->flags);
}

//============================================================================
// Stress: Repeated Push/Pop Cycles
//============================================================================

void test_rapid_push_pop_cycles(void)
{
    // Push and pop many times to ensure no state leakage
    for (int cycle = 0; cycle < 1000; cycle++)
    {
        EvalOp *op = op_stack_push(&stack);
        TEST_ASSERT_NOT_NULL(op);
        op->kind = OP_FOREVER;
        op_stack_pop(&stack);
    }
    TEST_ASSERT_TRUE(op_stack_is_empty(&stack));
}

void test_sawtooth_depth_pattern(void)
{
    // Push up to half, pop back down, repeat — stresses different depths
    int half = MAX_OP_STACK_DEPTH / 2;
    for (int cycle = 0; cycle < 5; cycle++)
    {
        for (int i = 0; i < half; i++)
        {
            TEST_ASSERT_NOT_NULL(op_stack_push(&stack));
        }
        TEST_ASSERT_EQUAL(half, op_stack_depth(&stack));

        for (int i = 0; i < half; i++)
        {
            op_stack_pop(&stack);
        }
        TEST_ASSERT_TRUE(op_stack_is_empty(&stack));
    }
}

//============================================================================
// Main
//============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Basic tests
    RUN_TEST(test_init_is_empty);
    RUN_TEST(test_push_returns_non_null);
    RUN_TEST(test_peek_returns_top);
    RUN_TEST(test_peek_empty_returns_null);
    RUN_TEST(test_pop_decreases_depth);
    RUN_TEST(test_pop_empty_is_safe);
    RUN_TEST(test_push_pop_returns_to_empty);

    // Swap tests
    RUN_TEST(test_swap_top_two);
    RUN_TEST(test_swap_single_element_no_crash);
    RUN_TEST(test_swap_empty_no_crash);

    // Stack overflow tests
    RUN_TEST(test_push_until_full_returns_null);
    RUN_TEST(test_push_at_capacity_does_not_corrupt);
    RUN_TEST(test_multiple_overflow_attempts_safe);
    RUN_TEST(test_pop_after_overflow_then_push_succeeds);
    RUN_TEST(test_fill_drain_refill);
    RUN_TEST(test_push_zeroes_new_entry);

    // Stress tests
    RUN_TEST(test_rapid_push_pop_cycles);
    RUN_TEST(test_sawtooth_depth_pattern);

    return UNITY_END();
}
