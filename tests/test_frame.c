//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unit tests for the frame system.
//

#include "unity.h"
#include "core/frame.h"
#include "core/procedures.h"
#include "core/memory.h"

#include <string.h>
#include <stdlib.h>

// Test arena memory - 8KB
#define TEST_ARENA_SIZE 8192
static uint32_t test_memory[TEST_ARENA_SIZE / sizeof(uint32_t)];
static FrameStack stack;

// Mock procedure for testing
static UserProcedure test_proc;
static const char *test_param_names[3] = {"x", "y", "z"};

void setUp(void)
{
    memset(test_memory, 0, sizeof(test_memory));
    frame_stack_init(&stack, test_memory, sizeof(test_memory));
    logo_mem_init();

    // Initialize mock procedure
    test_proc.name = "testproc";
    test_proc.params[0] = test_param_names[0];
    test_proc.params[1] = test_param_names[1];
    test_proc.params[2] = test_param_names[2];
    test_proc.param_count = 3;
    test_proc.body = NODE_NIL;
    test_proc.buried = false;
    test_proc.stepped = false;
    test_proc.traced = false;
}

void tearDown(void)
{
    // Nothing to tear down
}

//============================================================================
// Frame Stack Initialization Tests
//============================================================================

void test_stack_init_empty(void)
{
    TEST_ASSERT_TRUE(frame_stack_is_empty(&stack));
    TEST_ASSERT_EQUAL(0, frame_stack_depth(&stack));
}

void test_stack_init_available_space(void)
{
    TEST_ASSERT_EQUAL(TEST_ARENA_SIZE, frame_stack_available_bytes(&stack));
}

void test_stack_init_used_zero(void)
{
    TEST_ASSERT_EQUAL(0, frame_stack_used_bytes(&stack));
}

void test_stack_init_null_fails(void)
{
    TEST_ASSERT_FALSE(frame_stack_init(NULL, test_memory, sizeof(test_memory)));
}

void test_stack_reset(void)
{
    // Push some frames
    Value args[3] = {value_number(1), value_number(2), value_number(3)};
    frame_push(&stack, &test_proc, args, 3);
    frame_push(&stack, &test_proc, args, 3);

    // Reset
    frame_stack_reset(&stack);

    TEST_ASSERT_TRUE(frame_stack_is_empty(&stack));
    TEST_ASSERT_EQUAL(0, frame_stack_depth(&stack));
    TEST_ASSERT_EQUAL(TEST_ARENA_SIZE, frame_stack_available_bytes(&stack));
}

//============================================================================
// Frame Push/Pop Tests
//============================================================================

void test_push_null_proc(void)
{
    // Push top-level frame (no procedure)
    word_offset_t off = frame_push(&stack, NULL, NULL, 0);
    TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off);
    TEST_ASSERT_EQUAL(1, frame_stack_depth(&stack));
}

void test_push_with_proc(void)
{
    Value args[3] = {value_number(10), value_number(20), value_number(30)};
    word_offset_t off = frame_push(&stack, &test_proc, args, 3);

    TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off);
    TEST_ASSERT_EQUAL(1, frame_stack_depth(&stack));
    TEST_ASSERT_FALSE(frame_stack_is_empty(&stack));
}

void test_push_argument_mismatch_fails(void)
{
    Value args[2] = {value_number(10), value_number(20)};
    // Procedure expects 3 args, we provide 2
    word_offset_t off = frame_push(&stack, &test_proc, args, 2);
    TEST_ASSERT_EQUAL(OFFSET_NONE, off);
}

void test_push_multiple_frames(void)
{
    Value args[3] = {value_number(1), value_number(2), value_number(3)};

    word_offset_t off1 = frame_push(&stack, &test_proc, args, 3);
    word_offset_t off2 = frame_push(&stack, &test_proc, args, 3);
    word_offset_t off3 = frame_push(&stack, &test_proc, args, 3);

    TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off1);
    TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off2);
    TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off3);
    TEST_ASSERT_EQUAL(3, frame_stack_depth(&stack));
}

void test_pop_returns_previous(void)
{
    Value args[3] = {value_number(1), value_number(2), value_number(3)};

    word_offset_t off1 = frame_push(&stack, &test_proc, args, 3);
    frame_push(&stack, &test_proc, args, 3);

    word_offset_t prev = frame_pop(&stack);
    TEST_ASSERT_EQUAL(off1, prev);
    TEST_ASSERT_EQUAL(1, frame_stack_depth(&stack));
}

void test_pop_empty_returns_none(void)
{
    word_offset_t prev = frame_pop(&stack);
    TEST_ASSERT_EQUAL(OFFSET_NONE, prev);
}

void test_pop_all_frames(void)
{
    Value args[3] = {value_number(1), value_number(2), value_number(3)};

    frame_push(&stack, &test_proc, args, 3);
    frame_push(&stack, &test_proc, args, 3);
    frame_push(&stack, &test_proc, args, 3);

    frame_pop(&stack);
    frame_pop(&stack);
    frame_pop(&stack);

    TEST_ASSERT_TRUE(frame_stack_is_empty(&stack));
    TEST_ASSERT_EQUAL(0, frame_stack_depth(&stack));
}

void test_pop_frees_memory(void)
{
    Value args[3] = {value_number(1), value_number(2), value_number(3)};

    size_t before = frame_stack_used_bytes(&stack);
    frame_push(&stack, &test_proc, args, 3);
    size_t after_push = frame_stack_used_bytes(&stack);
    frame_pop(&stack);
    size_t after_pop = frame_stack_used_bytes(&stack);

    TEST_ASSERT_EQUAL(before, after_pop);
    TEST_ASSERT_TRUE(after_push > before);
}

//============================================================================
// Frame Access Tests
//============================================================================

void test_frame_at_none_returns_null(void)
{
    TEST_ASSERT_NULL(frame_at(&stack, OFFSET_NONE));
}

void test_frame_current_empty_returns_null(void)
{
    TEST_ASSERT_NULL(frame_current(&stack));
}

void test_frame_current_returns_top(void)
{
    Value args[3] = {value_number(1), value_number(2), value_number(3)};
    word_offset_t off = frame_push(&stack, &test_proc, args, 3);

    FrameHeader *frame = frame_current(&stack);
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_PTR(frame_at(&stack, off), frame);
}

void test_frame_has_correct_proc(void)
{
    Value args[3] = {value_number(1), value_number(2), value_number(3)};
    frame_push(&stack, &test_proc, args, 3);

    FrameHeader *frame = frame_current(&stack);
    TEST_ASSERT_EQUAL_PTR(&test_proc, frame->proc);
}

void test_frame_has_correct_param_count(void)
{
    Value args[3] = {value_number(1), value_number(2), value_number(3)};
    frame_push(&stack, &test_proc, args, 3);

    FrameHeader *frame = frame_current(&stack);
    TEST_ASSERT_EQUAL(3, frame->param_count);
}

//============================================================================
// Binding Tests
//============================================================================

void test_bindings_count(void)
{
    Value args[3] = {value_number(1), value_number(2), value_number(3)};
    frame_push(&stack, &test_proc, args, 3);

    FrameHeader *frame = frame_current(&stack);
    TEST_ASSERT_EQUAL(3, frame_binding_count(frame));
}

void test_bindings_have_correct_names(void)
{
    Value args[3] = {value_number(1), value_number(2), value_number(3)};
    frame_push(&stack, &test_proc, args, 3);

    FrameHeader *frame = frame_current(&stack);
    Binding *bindings = frame_get_bindings(frame);

    TEST_ASSERT_EQUAL_STRING("x", bindings[0].name);
    TEST_ASSERT_EQUAL_STRING("y", bindings[1].name);
    TEST_ASSERT_EQUAL_STRING("z", bindings[2].name);
}

void test_bindings_have_correct_values(void)
{
    Value args[3] = {value_number(10), value_number(20), value_number(30)};
    frame_push(&stack, &test_proc, args, 3);

    FrameHeader *frame = frame_current(&stack);
    Binding *bindings = frame_get_bindings(frame);

    TEST_ASSERT_EQUAL_FLOAT(10.0f, bindings[0].value.as.number);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, bindings[1].value.as.number);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, bindings[2].value.as.number);
}

void test_find_binding_exists(void)
{
    Value args[3] = {value_number(10), value_number(20), value_number(30)};
    frame_push(&stack, &test_proc, args, 3);

    FrameHeader *frame = frame_current(&stack);
    Binding *binding = frame_find_binding(frame, "y");

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, binding->value.as.number);
}

void test_find_binding_case_insensitive(void)
{
    Value args[3] = {value_number(10), value_number(20), value_number(30)};
    frame_push(&stack, &test_proc, args, 3);

    FrameHeader *frame = frame_current(&stack);
    Binding *binding = frame_find_binding(frame, "Y");

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, binding->value.as.number);
}

void test_find_binding_not_found(void)
{
    Value args[3] = {value_number(10), value_number(20), value_number(30)};
    frame_push(&stack, &test_proc, args, 3);

    FrameHeader *frame = frame_current(&stack);
    Binding *binding = frame_find_binding(frame, "w");

    TEST_ASSERT_NULL(binding);
}

void test_find_binding_in_chain(void)
{
    // Push two frames with different parameters
    UserProcedure proc1 = test_proc;
    proc1.params[0] = "a";
    proc1.param_count = 1;

    UserProcedure proc2 = test_proc;
    proc2.params[0] = "b";
    proc2.param_count = 1;

    Value args1[1] = {value_number(100)};
    Value args2[1] = {value_number(200)};

    frame_push(&stack, &proc1, args1, 1);
    frame_push(&stack, &proc2, args2, 1);

    // Find "a" which is in the parent frame
    FrameHeader *found_frame = NULL;
    Binding *binding = frame_find_binding_in_chain(&stack, "a", &found_frame);

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, binding->value.as.number);
    TEST_ASSERT_NOT_NULL(found_frame);
    // found_frame should be the parent, not the current
    TEST_ASSERT_TRUE(frame_current(&stack) != found_frame);
}

void test_set_binding(void)
{
    Value args[3] = {value_number(10), value_number(20), value_number(30)};
    frame_push(&stack, &test_proc, args, 3);

    FrameHeader *frame = frame_current(&stack);
    bool result = frame_set_binding(frame, "y", value_number(999));

    TEST_ASSERT_TRUE(result);

    Binding *binding = frame_find_binding(frame, "y");
    TEST_ASSERT_EQUAL_FLOAT(999.0f, binding->value.as.number);
}

//============================================================================
// Local Variable Tests
//============================================================================

void test_add_local(void)
{
    frame_push(&stack, NULL, NULL, 0);  // Top-level frame

    bool result = frame_add_local(&stack, "myvar", value_number(42));
    TEST_ASSERT_TRUE(result);

    FrameHeader *frame = frame_current(&stack);
    TEST_ASSERT_EQUAL(1, frame->local_count);
}

void test_add_local_find(void)
{
    frame_push(&stack, NULL, NULL, 0);
    frame_add_local(&stack, "myvar", value_number(42));

    FrameHeader *frame = frame_current(&stack);
    Binding *binding = frame_find_binding(frame, "myvar");

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, binding->value.as.number);
}

void test_add_multiple_locals(void)
{
    frame_push(&stack, NULL, NULL, 0);

    frame_add_local(&stack, "a", value_number(1));
    frame_add_local(&stack, "b", value_number(2));
    frame_add_local(&stack, "c", value_number(3));

    FrameHeader *frame = frame_current(&stack);
    TEST_ASSERT_EQUAL(3, frame->local_count);

    Binding *b = frame_find_binding(frame, "b");
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, b->value.as.number);
}

void test_declare_local_unbound(void)
{
    frame_push(&stack, NULL, NULL, 0);

    frame_declare_local(&stack, "unbound");

    FrameHeader *frame = frame_current(&stack);
    Binding *binding = frame_find_binding(frame, "unbound");

    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL(VALUE_NONE, binding->value.type);
}

void test_add_local_with_params(void)
{
    Value args[3] = {value_number(10), value_number(20), value_number(30)};
    frame_push(&stack, &test_proc, args, 3);

    frame_add_local(&stack, "local1", value_number(100));
    frame_add_local(&stack, "local2", value_number(200));

    FrameHeader *frame = frame_current(&stack);
    TEST_ASSERT_EQUAL(3, frame->param_count);
    TEST_ASSERT_EQUAL(2, frame->local_count);
    TEST_ASSERT_EQUAL(5, frame_binding_count(frame));

    // Verify all bindings
    Binding *x = frame_find_binding(frame, "x");
    Binding *local2 = frame_find_binding(frame, "local2");

    TEST_ASSERT_EQUAL_FLOAT(10.0f, x->value.as.number);
    TEST_ASSERT_EQUAL_FLOAT(200.0f, local2->value.as.number);
}

//============================================================================
// Expression Value Stack Tests
//============================================================================

void test_push_value(void)
{
    frame_push(&stack, NULL, NULL, 0);

    bool result = frame_push_value(&stack, value_number(42));
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, frame_value_count(&stack));
}

void test_pop_value(void)
{
    frame_push(&stack, NULL, NULL, 0);

    frame_push_value(&stack, value_number(42));
    Value v = frame_pop_value(&stack);

    TEST_ASSERT_EQUAL(VALUE_NUMBER, v.type);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, v.as.number);
    TEST_ASSERT_EQUAL(0, frame_value_count(&stack));
}

void test_value_pop_empty_returns_none(void)
{
    frame_push(&stack, NULL, NULL, 0);

    Value v = frame_pop_value(&stack);
    TEST_ASSERT_EQUAL(VALUE_NONE, v.type);
}

void test_peek_value(void)
{
    frame_push(&stack, NULL, NULL, 0);

    frame_push_value(&stack, value_number(42));
    Value v = frame_peek_value(&stack);

    TEST_ASSERT_EQUAL(VALUE_NUMBER, v.type);
    TEST_ASSERT_EQUAL_FLOAT(42.0f, v.as.number);
    TEST_ASSERT_EQUAL(1, frame_value_count(&stack));  // Still there
}

void test_value_stack_lifo(void)
{
    frame_push(&stack, NULL, NULL, 0);

    frame_push_value(&stack, value_number(1));
    frame_push_value(&stack, value_number(2));
    frame_push_value(&stack, value_number(3));

    TEST_ASSERT_EQUAL_FLOAT(3.0f, frame_pop_value(&stack).as.number);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, frame_pop_value(&stack).as.number);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, frame_pop_value(&stack).as.number);
}

void test_clear_values(void)
{
    frame_push(&stack, NULL, NULL, 0);

    frame_push_value(&stack, value_number(1));
    frame_push_value(&stack, value_number(2));
    frame_push_value(&stack, value_number(3));

    frame_clear_values(&stack);
    TEST_ASSERT_EQUAL(0, frame_value_count(&stack));
}

void test_value_stack_growth(void)
{
    frame_push(&stack, NULL, NULL, 0);

    // Push more than initial capacity (8)
    for (int i = 0; i < 20; i++)
    {
        bool result = frame_push_value(&stack, value_number((float)i));
        TEST_ASSERT_TRUE(result);
    }

    TEST_ASSERT_EQUAL(20, frame_value_count(&stack));

    // Verify values
    for (int i = 19; i >= 0; i--)
    {
        Value v = frame_pop_value(&stack);
        TEST_ASSERT_EQUAL_FLOAT((float)i, v.as.number);
    }
}

void test_values_with_locals(void)
{
    // Test that adding locals doesn't corrupt value stack
    frame_push(&stack, NULL, NULL, 0);

    frame_push_value(&stack, value_number(1));
    frame_push_value(&stack, value_number(2));

    frame_add_local(&stack, "x", value_number(100));

    frame_push_value(&stack, value_number(3));

    // Values should still be correct
    TEST_ASSERT_EQUAL_FLOAT(3.0f, frame_pop_value(&stack).as.number);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, frame_pop_value(&stack).as.number);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, frame_pop_value(&stack).as.number);

    // Local should be intact
    FrameHeader *frame = frame_current(&stack);
    Binding *binding = frame_find_binding(frame, "x");
    TEST_ASSERT_EQUAL_FLOAT(100.0f, binding->value.as.number);
}

//============================================================================
// TEST State Tests
//============================================================================

void test_test_not_valid_initially(void)
{
    frame_push(&stack, NULL, NULL, 0);
    TEST_ASSERT_FALSE(frame_test_is_valid(&stack));
}

void test_set_test_true(void)
{
    frame_push(&stack, NULL, NULL, 0);

    frame_set_test(&stack, true);

    bool value;
    bool valid = frame_get_test(&stack, &value);
    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_TRUE(value);
}

void test_set_test_false(void)
{
    frame_push(&stack, NULL, NULL, 0);

    frame_set_test(&stack, false);

    bool value;
    bool valid = frame_get_test(&stack, &value);
    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_FALSE(value);
}

void test_test_inherited_from_parent(void)
{
    frame_push(&stack, NULL, NULL, 0);
    frame_set_test(&stack, true);

    // Push child frame
    frame_push(&stack, NULL, NULL, 0);

    // Child should inherit parent's test state
    bool value;
    bool valid = frame_get_test(&stack, &value);
    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_TRUE(value);
}

void test_test_shadowed_by_child(void)
{
    frame_push(&stack, NULL, NULL, 0);
    frame_set_test(&stack, true);

    frame_push(&stack, NULL, NULL, 0);
    frame_set_test(&stack, false);

    // Child's test should shadow parent's
    bool value;
    frame_get_test(&stack, &value);
    TEST_ASSERT_FALSE(value);
}

//============================================================================
// Continuation State Tests
//============================================================================

void test_no_pending_continuation_initially(void)
{
    frame_push(&stack, NULL, NULL, 0);
    TEST_ASSERT_FALSE(frame_has_pending_continuation(&stack));
}

void test_save_continuation(void)
{
    frame_push(&stack, NULL, NULL, 0);

    frame_save_continuation(&stack, 42, 10, CONT_FLAG_HAS_PENDING);

    TEST_ASSERT_TRUE(frame_has_pending_continuation(&stack));

    FrameHeader *frame = frame_current(&stack);
    TEST_ASSERT_EQUAL(42, frame->pending_op);
    TEST_ASSERT_EQUAL(10, frame->pending_bp);
}

void test_clear_continuation(void)
{
    frame_push(&stack, NULL, NULL, 0);

    frame_save_continuation(&stack, 42, 10, CONT_FLAG_HAS_PENDING);
    frame_clear_continuation(&stack);

    TEST_ASSERT_FALSE(frame_has_pending_continuation(&stack));
}

//============================================================================
// Frame Iteration Tests
//============================================================================

static int iteration_count;
static bool iteration_callback(FrameStack *s, FrameHeader *f, int depth, void *data)
{
    (void)s;
    (void)f;
    (void)depth;
    (void)data;
    iteration_count++;
    return true;
}

void test_iterate_empty(void)
{
    iteration_count = 0;
    frame_iterate(&stack, iteration_callback, NULL);
    TEST_ASSERT_EQUAL(0, iteration_count);
}

void test_iterate_all_frames(void)
{
    frame_push(&stack, NULL, NULL, 0);
    frame_push(&stack, NULL, NULL, 0);
    frame_push(&stack, NULL, NULL, 0);

    iteration_count = 0;
    frame_iterate(&stack, iteration_callback, NULL);
    TEST_ASSERT_EQUAL(3, iteration_count);
}

static bool stop_at_two(FrameStack *s, FrameHeader *f, int depth, void *data)
{
    (void)s;
    (void)f;
    (void)depth;
    (void)data;
    iteration_count++;
    return iteration_count < 2;  // Stop after 2
}

void test_iterate_early_stop(void)
{
    frame_push(&stack, NULL, NULL, 0);
    frame_push(&stack, NULL, NULL, 0);
    frame_push(&stack, NULL, NULL, 0);

    iteration_count = 0;
    frame_iterate(&stack, stop_at_two, NULL);
    TEST_ASSERT_EQUAL(2, iteration_count);
}

//============================================================================
// Memory Pressure Tests
//============================================================================

void test_push_until_full(void)
{
    // Use a small arena
    uint32_t small_memory[64];  // 256 bytes
    FrameStack small_stack;
    frame_stack_init(&small_stack, small_memory, sizeof(small_memory));

    int frames_pushed = 0;
    while (true)
    {
        word_offset_t off = frame_push(&small_stack, NULL, NULL, 0);
        if (off == OFFSET_NONE)
            break;
        frames_pushed++;
        if (frames_pushed > 100)
            break;  // Safety limit
    }

    TEST_ASSERT_TRUE(frames_pushed > 0);
    TEST_ASSERT_TRUE(frames_pushed < 100);

    // Verify we can pop all frames
    for (int i = 0; i < frames_pushed; i++)
    {
        frame_pop(&small_stack);
    }
    TEST_ASSERT_TRUE(frame_stack_is_empty(&small_stack));
}

void test_reuse_after_pop(void)
{
    Value args[3] = {value_number(1), value_number(2), value_number(3)};

    // Push and pop repeatedly
    for (int i = 0; i < 100; i++)
    {
        word_offset_t off = frame_push(&stack, &test_proc, args, 3);
        TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off);
        frame_pop(&stack);
    }

    TEST_ASSERT_TRUE(frame_stack_is_empty(&stack));
    TEST_ASSERT_EQUAL(TEST_ARENA_SIZE, frame_stack_available_bytes(&stack));
}

//============================================================================
// Frame Reuse Tests (TCO)
//============================================================================

// Mock procedure with different param counts for reuse tests
static UserProcedure reuse_proc_1;  // 1 param
static UserProcedure reuse_proc_2;  // 2 params
static UserProcedure reuse_proc_3;  // 3 params
static const char *reuse_param_names[3] = {"a", "b", "c"};

static void init_reuse_procs(void)
{
    reuse_proc_1.name = "proc1";
    reuse_proc_1.params[0] = reuse_param_names[0];
    reuse_proc_1.param_count = 1;
    reuse_proc_1.body = NODE_NIL;
    reuse_proc_1.buried = false;
    reuse_proc_1.stepped = false;
    reuse_proc_1.traced = false;

    reuse_proc_2.name = "proc2";
    reuse_proc_2.params[0] = reuse_param_names[0];
    reuse_proc_2.params[1] = reuse_param_names[1];
    reuse_proc_2.param_count = 2;
    reuse_proc_2.body = NODE_NIL;
    reuse_proc_2.buried = false;
    reuse_proc_2.stepped = false;
    reuse_proc_2.traced = false;

    reuse_proc_3.name = "proc3";
    reuse_proc_3.params[0] = reuse_param_names[0];
    reuse_proc_3.params[1] = reuse_param_names[1];
    reuse_proc_3.params[2] = reuse_param_names[2];
    reuse_proc_3.param_count = 3;
    reuse_proc_3.body = NODE_NIL;
    reuse_proc_3.buried = false;
    reuse_proc_3.stepped = false;
    reuse_proc_3.traced = false;
}

void test_reuse_empty_stack_fails(void)
{
    init_reuse_procs();
    Value args[1] = {value_number(42)};
    
    // Try to reuse when stack is empty
    TEST_ASSERT_FALSE(frame_reuse(&stack, &reuse_proc_1, args, 1));
}

void test_reuse_same_params(void)
{
    init_reuse_procs();
    Value args1[2] = {value_number(1), value_number(2)};
    Value args2[2] = {value_number(10), value_number(20)};

    // Push initial frame
    word_offset_t off1 = frame_push(&stack, &reuse_proc_2, args1, 2);
    TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off1);
    TEST_ASSERT_EQUAL(1, frame_stack_depth(&stack));
    size_t used1 = frame_stack_used_bytes(&stack);

    // Reuse with same param count
    TEST_ASSERT_TRUE(frame_reuse(&stack, &reuse_proc_2, args2, 2));
    TEST_ASSERT_EQUAL(1, frame_stack_depth(&stack));  // Depth unchanged
    TEST_ASSERT_EQUAL(used1, frame_stack_used_bytes(&stack));  // Memory unchanged

    // Verify new bindings
    FrameHeader *frame = frame_current(&stack);
    Binding *bindings = frame_get_bindings(frame);
    TEST_ASSERT_EQUAL(10, bindings[0].value.as.number);
    TEST_ASSERT_EQUAL(20, bindings[1].value.as.number);
}

void test_reuse_fewer_params(void)
{
    init_reuse_procs();
    Value args3[3] = {value_number(1), value_number(2), value_number(3)};
    Value args1[1] = {value_number(99)};

    // Push frame with 3 params
    word_offset_t off = frame_push(&stack, &reuse_proc_3, args3, 3);
    TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off);

    // Reuse with 1 param (fewer than original)
    TEST_ASSERT_TRUE(frame_reuse(&stack, &reuse_proc_1, args1, 1));
    TEST_ASSERT_EQUAL(1, frame_stack_depth(&stack));

    // Verify procedure and binding
    FrameHeader *frame = frame_current(&stack);
    TEST_ASSERT_EQUAL_STRING("proc1", frame->proc->name);
    TEST_ASSERT_EQUAL(1, frame->param_count);
    Binding *bindings = frame_get_bindings(frame);
    TEST_ASSERT_EQUAL(99, bindings[0].value.as.number);
}

void test_reuse_more_params_fails(void)
{
    init_reuse_procs();
    Value args1[1] = {value_number(1)};
    Value args3[3] = {value_number(10), value_number(20), value_number(30)};

    // Push frame with 1 param
    word_offset_t off = frame_push(&stack, &reuse_proc_1, args1, 1);
    TEST_ASSERT_NOT_EQUAL(OFFSET_NONE, off);

    // Try to reuse with 3 params (more than original) - should fail
    TEST_ASSERT_FALSE(frame_reuse(&stack, &reuse_proc_3, args3, 3));
}

void test_reuse_clears_locals(void)
{
    init_reuse_procs();
    Value args[2] = {value_number(1), value_number(2)};

    // Push frame and add local
    frame_push(&stack, &reuse_proc_2, args, 2);
    frame_add_local(&stack, "local_var", value_number(999));
    
    FrameHeader *frame = frame_current(&stack);
    TEST_ASSERT_EQUAL(1, frame->local_count);

    // Reuse should clear locals
    Value args2[2] = {value_number(10), value_number(20)};
    TEST_ASSERT_TRUE(frame_reuse(&stack, &reuse_proc_2, args2, 2));
    
    frame = frame_current(&stack);
    TEST_ASSERT_EQUAL(0, frame->local_count);
}

void test_reuse_clears_test_state(void)
{
    init_reuse_procs();
    Value args[2] = {value_number(1), value_number(2)};

    // Push frame and set test state
    frame_push(&stack, &reuse_proc_2, args, 2);
    frame_set_test(&stack, true);
    
    bool test_val;
    TEST_ASSERT_TRUE(frame_get_test(&stack, &test_val));
    TEST_ASSERT_TRUE(test_val);

    // Reuse should clear test state
    Value args2[2] = {value_number(10), value_number(20)};
    TEST_ASSERT_TRUE(frame_reuse(&stack, &reuse_proc_2, args2, 2));
    
    TEST_ASSERT_FALSE(frame_get_test(&stack, &test_val));  // No longer valid
}

void test_reuse_clears_value_stack(void)
{
    init_reuse_procs();
    Value args[2] = {value_number(1), value_number(2)};

    // Push frame and push values
    frame_push(&stack, &reuse_proc_2, args, 2);
    frame_push_value(&stack, value_number(100));
    frame_push_value(&stack, value_number(200));
    TEST_ASSERT_EQUAL(2, frame_value_count(&stack));

    // Reuse should clear value stack
    Value args2[2] = {value_number(10), value_number(20)};
    TEST_ASSERT_TRUE(frame_reuse(&stack, &reuse_proc_2, args2, 2));
    TEST_ASSERT_EQUAL(0, frame_value_count(&stack));
}

void test_reuse_many_times_no_memory_growth(void)
{
    init_reuse_procs();
    Value args[2] = {value_number(1), value_number(2)};

    // Push initial frame
    frame_push(&stack, &reuse_proc_2, args, 2);
    size_t used_after_push = frame_stack_used_bytes(&stack);

    // Reuse many times - memory should stay constant
    for (int i = 0; i < 1000; i++)
    {
        Value new_args[2] = {value_number(i), value_number(i * 2)};
        TEST_ASSERT_TRUE(frame_reuse(&stack, &reuse_proc_2, new_args, 2));
        TEST_ASSERT_EQUAL(used_after_push, frame_stack_used_bytes(&stack));
        TEST_ASSERT_EQUAL(1, frame_stack_depth(&stack));
    }

    // Verify final values
    FrameHeader *frame = frame_current(&stack);
    Binding *bindings = frame_get_bindings(frame);
    TEST_ASSERT_EQUAL(999, bindings[0].value.as.number);
    TEST_ASSERT_EQUAL(1998, bindings[1].value.as.number);
}

//============================================================================
// Test Runner
//============================================================================

int main(void)
{
    UNITY_BEGIN();

    // Frame stack initialization tests
    RUN_TEST(test_stack_init_empty);
    RUN_TEST(test_stack_init_available_space);
    RUN_TEST(test_stack_init_used_zero);
    RUN_TEST(test_stack_init_null_fails);
    RUN_TEST(test_stack_reset);

    // Frame push/pop tests
    RUN_TEST(test_push_null_proc);
    RUN_TEST(test_push_with_proc);
    RUN_TEST(test_push_argument_mismatch_fails);
    RUN_TEST(test_push_multiple_frames);
    RUN_TEST(test_pop_returns_previous);
    RUN_TEST(test_pop_empty_returns_none);
    RUN_TEST(test_pop_all_frames);
    RUN_TEST(test_pop_frees_memory);

    // Frame access tests
    RUN_TEST(test_frame_at_none_returns_null);
    RUN_TEST(test_frame_current_empty_returns_null);
    RUN_TEST(test_frame_current_returns_top);
    RUN_TEST(test_frame_has_correct_proc);
    RUN_TEST(test_frame_has_correct_param_count);

    // Binding tests
    RUN_TEST(test_bindings_count);
    RUN_TEST(test_bindings_have_correct_names);
    RUN_TEST(test_bindings_have_correct_values);
    RUN_TEST(test_find_binding_exists);
    RUN_TEST(test_find_binding_case_insensitive);
    RUN_TEST(test_find_binding_not_found);
    RUN_TEST(test_find_binding_in_chain);
    RUN_TEST(test_set_binding);

    // Local variable tests
    RUN_TEST(test_add_local);
    RUN_TEST(test_add_local_find);
    RUN_TEST(test_add_multiple_locals);
    RUN_TEST(test_declare_local_unbound);
    RUN_TEST(test_add_local_with_params);

    // Expression value stack tests
    RUN_TEST(test_push_value);
    RUN_TEST(test_pop_value);
    RUN_TEST(test_value_pop_empty_returns_none);
    RUN_TEST(test_peek_value);
    RUN_TEST(test_value_stack_lifo);
    RUN_TEST(test_clear_values);
    RUN_TEST(test_value_stack_growth);
    RUN_TEST(test_values_with_locals);

    // TEST state tests
    RUN_TEST(test_test_not_valid_initially);
    RUN_TEST(test_set_test_true);
    RUN_TEST(test_set_test_false);
    RUN_TEST(test_test_inherited_from_parent);
    RUN_TEST(test_test_shadowed_by_child);

    // Continuation state tests
    RUN_TEST(test_no_pending_continuation_initially);
    RUN_TEST(test_save_continuation);
    RUN_TEST(test_clear_continuation);

    // Frame iteration tests
    RUN_TEST(test_iterate_empty);
    RUN_TEST(test_iterate_all_frames);
    RUN_TEST(test_iterate_early_stop);

    // Memory pressure tests
    RUN_TEST(test_push_until_full);
    RUN_TEST(test_reuse_after_pop);

    // Frame reuse tests (TCO)
    RUN_TEST(test_reuse_empty_stack_fails);
    RUN_TEST(test_reuse_same_params);
    RUN_TEST(test_reuse_fewer_params);
    RUN_TEST(test_reuse_more_params_fails);
    RUN_TEST(test_reuse_clears_locals);
    RUN_TEST(test_reuse_clears_test_state);
    RUN_TEST(test_reuse_clears_value_stack);
    RUN_TEST(test_reuse_many_times_no_memory_growth);

    return UNITY_END();
}
