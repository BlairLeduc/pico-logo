//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "frame.h"
#include "procedures.h"
#include <string.h>
#include <strings.h>

//==========================================================================
// Internal Helpers
//==========================================================================

// Calculate initial frame size in words
static uint16_t calc_frame_size(int param_count, int value_capacity)
{
    return FRAME_HEADER_WORDS
         + param_count * BINDING_WORDS
         + value_capacity * VALUE_WORDS;
}

// Get pointer to bindings array (immediately after header)
static Binding *get_bindings_ptr(FrameStack *stack, FrameHeader *frame)
{
    uint32_t *base = (uint32_t *)frame;
    return (Binding *)(base + FRAME_HEADER_WORDS);
}

// Get pointer to values array (after all bindings)
static Value *get_values_ptr(FrameStack *stack, FrameHeader *frame)
{
    Binding *bindings = get_bindings_ptr(stack, frame);
    int total_bindings = frame->param_count + frame->local_count;
    return (Value *)(bindings + total_bindings);
}

//==========================================================================
// Frame Stack Operations
//==========================================================================

bool frame_stack_init(FrameStack *stack, void *memory, size_t size_bytes)
{
    if (stack == NULL)
    {
        return false;
    }

    if (!arena_init(&stack->arena, memory, size_bytes))
    {
        return false;
    }

    stack->current = OFFSET_NONE;
    stack->depth = 0;

    return true;
}

void frame_stack_reset(FrameStack *stack)
{
    arena_free_to(&stack->arena, 0);
    stack->current = OFFSET_NONE;
    stack->depth = 0;
}

bool frame_stack_is_empty(FrameStack *stack)
{
    return stack->current == OFFSET_NONE;
}

int frame_stack_depth(FrameStack *stack)
{
    return stack->depth;
}

size_t frame_stack_used_bytes(FrameStack *stack)
{
    return arena_used_bytes(&stack->arena);
}

size_t frame_stack_available_bytes(FrameStack *stack)
{
    return arena_available_bytes(&stack->arena);
}

//==========================================================================
// Frame Operations
//==========================================================================

word_offset_t frame_push(FrameStack *stack, UserProcedure *proc,
                         Value *args, int argc)
{
    // Determine parameter count
    int param_count = proc ? proc->param_count : 0;

    // Validate argument count matches parameters
    if (argc != param_count)
    {
        return OFFSET_NONE;  // Argument count mismatch
    }

    // Calculate initial frame size
    uint16_t size = calc_frame_size(param_count, FRAME_INITIAL_VALUE_CAPACITY);

    // Save arena position for later freeing
    word_offset_t mark = arena_top(&stack->arena);

    // Allocate frame
    word_offset_t offset = arena_alloc_words(&stack->arena, size);
    if (offset == OFFSET_NONE)
    {
        return OFFSET_NONE;  // Out of space
    }

    // Initialize header
    FrameHeader *frame = (FrameHeader *)arena_offset_to_ptr(&stack->arena, offset);
    frame->prev_offset = stack->current;
    frame->arena_mark = mark;
    frame->size_words = size;
    frame->param_count = (uint8_t)param_count;
    frame->local_count = 0;
    frame->value_count = 0;
    frame->value_capacity = FRAME_INITIAL_VALUE_CAPACITY;
    frame->proc = proc;
    frame->body_cursor = proc ? proc->body : NODE_NIL;
    frame->line_cursor = NODE_NIL;
    frame->pending_op = 0;
    frame->pending_bp = 0;
    frame->cont_flags = CONT_FLAG_NONE;
    frame->_reserved1 = 0;
    frame->test_valid = false;
    frame->test_value = false;
    frame->_reserved2 = 0;

    // Bind parameters
    if (param_count > 0 && args != NULL)
    {
        Binding *bindings = get_bindings_ptr(stack, frame);
        for (int i = 0; i < param_count; i++)
        {
            bindings[i].name = proc->params[i];
            bindings[i].value = args[i];
        }
    }

    // Update stack state
    stack->current = offset;
    stack->depth++;

    return offset;
}

bool frame_reuse(FrameStack *stack, UserProcedure *proc,
                 Value *args, int argc)
{
    if (stack->current == OFFSET_NONE)
    {
        return false;  // No frame to reuse
    }

    // Determine parameter count for new procedure
    int param_count = proc ? proc->param_count : 0;

    // Validate argument count matches parameters
    if (argc != param_count)
    {
        return false;  // Argument count mismatch
    }

    FrameHeader *frame = (FrameHeader *)arena_offset_to_ptr(&stack->arena, stack->current);

    // Check if we have enough space for the new parameters
    // If the new procedure has more parameters than the old frame had capacity for,
    // we need to fall back to pop+push (but this is rare in practice)
    // For now, we'll handle the common case where param counts are similar
    
    // Calculate space available for bindings (header + existing bindings + values)
    // The frame's size was calculated for its original param_count
    // We need to ensure we have room for the new param_count
    
    // Simple approach: if new param_count > old param_count, we can't reuse
    // (A more sophisticated implementation could resize the frame)
    if (param_count > frame->param_count)
    {
        return false;  // Need more parameter slots than available
    }

    // Reuse the frame: update procedure and rebind parameters
    frame->proc = proc;
    frame->body_cursor = proc ? proc->body : NODE_NIL;
    frame->line_cursor = NODE_NIL;
    frame->param_count = (uint8_t)param_count;
    
    // Clear local variables
    frame->local_count = 0;
    
    // Clear expression stack
    frame->value_count = 0;
    
    // Reset TEST state (each procedure call starts with no TEST)
    frame->test_valid = false;
    frame->test_value = false;
    
    // Clear continuation state
    frame->pending_op = 0;
    frame->pending_bp = 0;
    frame->cont_flags = CONT_FLAG_NONE;

    // Rebind parameters with new values
    if (param_count > 0 && args != NULL)
    {
        Binding *bindings = get_bindings_ptr(stack, frame);
        for (int i = 0; i < param_count; i++)
        {
            bindings[i].name = proc->params[i];
            bindings[i].value = args[i];
        }
    }

    // Note: depth doesn't change since we're reusing the same logical frame
    return true;
}

word_offset_t frame_pop(FrameStack *stack)
{
    if (stack->current == OFFSET_NONE)
    {
        return OFFSET_NONE;  // Stack is empty
    }

    FrameHeader *frame = (FrameHeader *)arena_offset_to_ptr(&stack->arena, stack->current);
    word_offset_t prev = frame->prev_offset;
    word_offset_t mark = frame->arena_mark;

    // Free frame memory
    arena_free_to(&stack->arena, mark);

    // Update stack state
    stack->current = prev;
    stack->depth--;

    return prev;
}

FrameHeader *frame_at(FrameStack *stack, word_offset_t offset)
{
    if (offset == OFFSET_NONE)
    {
        return NULL;
    }
    return (FrameHeader *)arena_offset_to_ptr(&stack->arena, offset);
}

FrameHeader *frame_current(FrameStack *stack)
{
    return frame_at(stack, stack->current);
}

word_offset_t frame_current_offset(FrameStack *stack)
{
    return stack->current;
}

//==========================================================================
// Binding Operations
//==========================================================================

Binding *frame_bindings(FrameStack *stack, FrameHeader *frame)
{
    if (frame == NULL)
    {
        return NULL;
    }
    return get_bindings_ptr(stack, frame);
}

int frame_binding_count(FrameHeader *frame)
{
    if (frame == NULL)
    {
        return 0;
    }
    return frame->param_count + frame->local_count;
}

Binding *frame_find_binding(FrameStack *stack, FrameHeader *frame, const char *name)
{
    if (frame == NULL || name == NULL)
    {
        return NULL;
    }

    Binding *bindings = get_bindings_ptr(stack, frame);
    int count = frame->param_count + frame->local_count;

    for (int i = 0; i < count; i++)
    {
        if (bindings[i].name != NULL && strcasecmp(bindings[i].name, name) == 0)
        {
            return &bindings[i];
        }
    }

    return NULL;
}

Binding *frame_find_binding_in_chain(FrameStack *stack, const char *name,
                                     FrameHeader **found_frame)
{
    if (name == NULL)
    {
        return NULL;
    }

    word_offset_t offset = stack->current;
    while (offset != OFFSET_NONE)
    {
        FrameHeader *frame = frame_at(stack, offset);
        Binding *binding = frame_find_binding(stack, frame, name);
        if (binding != NULL)
        {
            if (found_frame != NULL)
            {
                *found_frame = frame;
            }
            return binding;
        }
        offset = frame->prev_offset;
    }

    if (found_frame != NULL)
    {
        *found_frame = NULL;
    }
    return NULL;
}

//==========================================================================
// Local Variable Operations
//==========================================================================

bool frame_add_local(FrameStack *stack, const char *name, Value value)
{
    FrameHeader *frame = frame_current(stack);
    if (frame == NULL)
    {
        return false;
    }

    // Check if we're the top frame (required for extension)
    word_offset_t frame_end = stack->current + frame->size_words;
    if (frame_end != arena_top(&stack->arena))
    {
        return false;  // Can't extend non-top frame
    }

    // Calculate current layout
    int binding_words_used = (frame->param_count + frame->local_count) * BINDING_WORDS;
    int value_words_used = frame->value_count * VALUE_WORDS;
    int total_data_words = binding_words_used + value_words_used;
    int data_capacity = frame->size_words - FRAME_HEADER_WORDS;
    int free_words = data_capacity - total_data_words;

    // Check if we need to extend
    if (free_words < (int)BINDING_WORDS)
    {
        // Need to extend the frame
        uint16_t extend_words = BINDING_WORDS;
        if (!arena_extend(&stack->arena, extend_words))
        {
            return false;  // Out of arena space
        }
        frame->size_words += extend_words;
    }

    // Move values if there are any on the stack
    if (frame->value_count > 0)
    {
        Value *old_values = get_values_ptr(stack, frame);
        // After adding local, values will be BINDING_WORDS further
        Value *new_values = (Value *)((uint32_t *)old_values + BINDING_WORDS);
        // Move backwards to avoid overlap issues
        for (int i = frame->value_count - 1; i >= 0; i--)
        {
            new_values[i] = old_values[i];
        }
    }

    // Add the local binding
    Binding *bindings = get_bindings_ptr(stack, frame);
    int local_index = frame->param_count + frame->local_count;
    bindings[local_index].name = name;
    bindings[local_index].value = value;
    frame->local_count++;

    // Update value capacity (available space after all bindings, divided by value size)
    int new_binding_words = (frame->param_count + frame->local_count) * BINDING_WORDS;
    int remaining_words = frame->size_words - FRAME_HEADER_WORDS - new_binding_words;
    frame->value_capacity = remaining_words / VALUE_WORDS;

    return true;
}

bool frame_declare_local(FrameStack *stack, const char *name)
{
    return frame_add_local(stack, name, value_none());
}

bool frame_set_binding(FrameStack *stack, FrameHeader *frame,
                       const char *name, Value value)
{
    Binding *binding = frame_find_binding(stack, frame, name);
    if (binding == NULL)
    {
        return false;
    }
    binding->value = value;
    return true;
}

//==========================================================================
// Expression Value Stack Operations
//==========================================================================

Value *frame_values(FrameStack *stack, FrameHeader *frame)
{
    if (frame == NULL)
    {
        return NULL;
    }
    return get_values_ptr(stack, frame);
}

bool frame_push_value(FrameStack *stack, Value value)
{
    FrameHeader *frame = frame_current(stack);
    if (frame == NULL)
    {
        return false;
    }

    // Check if we need more capacity
    if (frame->value_count >= frame->value_capacity)
    {
        // Check if we're the top frame
        word_offset_t frame_end = stack->current + frame->size_words;
        if (frame_end != arena_top(&stack->arena))
        {
            return false;  // Can't extend non-top frame
        }

        // Extend by 4 value slots
        uint16_t extend_words = 4 * VALUE_WORDS;
        if (!arena_extend(&stack->arena, extend_words))
        {
            return false;  // Out of arena space
        }
        frame->size_words += extend_words;
        frame->value_capacity += 4;
    }

    // Push the value
    Value *values = get_values_ptr(stack, frame);
    values[frame->value_count++] = value;

    return true;
}

Value frame_pop_value(FrameStack *stack)
{
    FrameHeader *frame = frame_current(stack);
    if (frame == NULL || frame->value_count == 0)
    {
        return value_none();
    }

    Value *values = get_values_ptr(stack, frame);
    return values[--frame->value_count];
}

Value frame_peek_value(FrameStack *stack)
{
    FrameHeader *frame = frame_current(stack);
    if (frame == NULL || frame->value_count == 0)
    {
        return value_none();
    }

    Value *values = get_values_ptr(stack, frame);
    return values[frame->value_count - 1];
}

int frame_value_count(FrameStack *stack)
{
    FrameHeader *frame = frame_current(stack);
    if (frame == NULL)
    {
        return 0;
    }
    return frame->value_count;
}

void frame_clear_values(FrameStack *stack)
{
    FrameHeader *frame = frame_current(stack);
    if (frame != NULL)
    {
        frame->value_count = 0;
    }
}

//==========================================================================
// TEST State Operations
//==========================================================================

void frame_set_test(FrameStack *stack, bool value)
{
    FrameHeader *frame = frame_current(stack);
    if (frame != NULL)
    {
        frame->test_valid = true;
        frame->test_value = value;
    }
}

bool frame_get_test(FrameStack *stack, bool *out_value)
{
    // Search from current frame up to root
    word_offset_t offset = stack->current;
    while (offset != OFFSET_NONE)
    {
        FrameHeader *frame = frame_at(stack, offset);
        if (frame->test_valid)
        {
            if (out_value != NULL)
            {
                *out_value = frame->test_value;
            }
            return true;
        }
        offset = frame->prev_offset;
    }

    return false;  // No TEST has been executed
}

bool frame_test_is_valid(FrameStack *stack)
{
    return frame_get_test(stack, NULL);
}

//==========================================================================
// Continuation State Operations
//==========================================================================

void frame_save_continuation(FrameStack *stack, uint8_t pending_op,
                             uint8_t pending_bp, uint8_t flags)
{
    FrameHeader *frame = frame_current(stack);
    if (frame != NULL)
    {
        frame->pending_op = pending_op;
        frame->pending_bp = pending_bp;
        frame->cont_flags = flags;
    }
}

void frame_clear_continuation(FrameStack *stack)
{
    FrameHeader *frame = frame_current(stack);
    if (frame != NULL)
    {
        frame->pending_op = 0;
        frame->pending_bp = 0;
        frame->cont_flags = CONT_FLAG_NONE;
    }
}

bool frame_has_pending_continuation(FrameStack *stack)
{
    FrameHeader *frame = frame_current(stack);
    if (frame == NULL)
    {
        return false;
    }
    return (frame->cont_flags & CONT_FLAG_HAS_PENDING) != 0;
}

//==========================================================================
// Iteration / Debugging
//==========================================================================

void frame_iterate(FrameStack *stack, FrameIterCallback callback, void *user_data)
{
    if (callback == NULL)
    {
        return;
    }

    int depth = stack->depth;
    word_offset_t offset = stack->current;

    while (offset != OFFSET_NONE)
    {
        FrameHeader *frame = frame_at(stack, offset);
        if (!callback(stack, frame, depth, user_data))
        {
            break;  // Callback requested stop
        }
        offset = frame->prev_offset;
        depth--;
    }
}

void frame_gc_mark_all(FrameStack *stack)
{
    word_offset_t offset = stack->current;

    while (offset != OFFSET_NONE)
    {
        FrameHeader *frame = frame_at(stack, offset);

        // Mark all binding values
        Binding *bindings = get_bindings_ptr(stack, frame);
        int binding_count = frame->param_count + frame->local_count;
        for (int i = 0; i < binding_count; i++)
        {
            Value *val = &bindings[i].value;
            if (val->type == VALUE_WORD || val->type == VALUE_LIST)
            {
                mem_gc_mark(val->as.node);
            }
        }

        // Mark all values on expression stack
        Value *values = get_values_ptr(stack, frame);
        for (int i = 0; i < frame->value_count; i++)
        {
            Value *val = &values[i];
            if (val->type == VALUE_WORD || val->type == VALUE_LIST)
            {
                mem_gc_mark(val->as.node);
            }
        }

        // Mark body cursor nodes
        if (!mem_is_nil(frame->body_cursor))
        {
            mem_gc_mark(frame->body_cursor);
        }
        if (!mem_is_nil(frame->line_cursor))
        {
            mem_gc_mark(frame->line_cursor);
        }

        offset = frame->prev_offset;
    }
}
