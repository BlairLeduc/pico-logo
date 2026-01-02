//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Frame - Procedure call frames stored in a FrameArena.
//
//  Each frame represents an active procedure call and contains:
//  - Link to previous frame (for stack traversal)
//  - Procedure being executed
//  - Continuation state (where to resume after a call returns)
//  - Parameter bindings
//  - Local variable bindings (dynamically added via LOCAL command)
//  - Expression value stack (for evaluating expressions)
//
//  Frames are variable-sized: they grow dynamically when locals are
//  added or when the expression stack needs more space.
//

#pragma once

#include "frame_arena.h"
#include "value.h"
#include "memory.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Forward declaration
    typedef struct UserProcedure UserProcedure;

    //==========================================================================
    // Constants
    //==========================================================================

    // Size of a Value in words (should be 2: type enum + union)
    #define VALUE_WORDS ((sizeof(Value) + sizeof(uint32_t) - 1) / sizeof(uint32_t))

    // Initial capacity for expression value stack (in Values)
    #define FRAME_INITIAL_VALUE_CAPACITY 8

    //==========================================================================
    // Binding Structure
    //==========================================================================

    // A variable binding: name + value
    // Names are interned pointers (from mem_atom), so pointer comparison works
    typedef struct
    {
        const char *name;  // Interned string pointer
        Value value;       // The bound value
    } Binding;

    // Size of a Binding in words
    #define BINDING_WORDS ((sizeof(Binding) + sizeof(uint32_t) - 1) / sizeof(uint32_t))

    //==========================================================================
    // Continuation Flags
    //==========================================================================

    // Flags for continuation state
    #define CONT_FLAG_NONE           0x00
    #define CONT_FLAG_HAS_PENDING    0x01  // Has a pending value waiting for operation result
    #define CONT_FLAG_IN_PAREN       0x02  // Inside parenthesized expression
    #define CONT_FLAG_TAIL_POSITION  0x04  // Current instruction is in tail position

    //==========================================================================
    // Frame Header Structure
    //==========================================================================

    // Frame header - fixed size portion at the start of each frame
    // Variable-length data (bindings, values) follows immediately after
    typedef struct
    {
        // Navigation (word offsets within arena)
        word_offset_t prev_offset;     // Previous frame, OFFSET_NONE if top-level
        word_offset_t arena_mark;      // Arena top before this frame (for freeing)
        uint16_t size_words;           // Total frame size including all sections

        // Section layout
        uint8_t param_count;           // Number of parameter bindings
        uint8_t local_count;           // Number of LOCAL variable bindings
        uint8_t value_count;           // Current number of values on expression stack
        uint8_t value_capacity;        // Max values before needing to extend

        // Procedure info
        UserProcedure *proc;           // Procedure being executed (NULL for top-level)

        // Continuation state - where to resume execution
        Node body_cursor;              // Current line in procedure body
        Node line_cursor;              // Current position within line (for mid-line resume)

        // Expression continuation (when a call happens mid-expression)
        uint8_t pending_op;            // Pending operator token type (0 if none)
        uint8_t pending_bp;            // Binding power for Pratt parser resumption
        uint8_t cont_flags;            // Continuation flags (CONT_FLAG_*)
        uint8_t _reserved1;            // Reserved for alignment

        // TEST state (per Logo spec, local to procedure)
        bool test_valid;               // True if TEST has been executed in this frame
        bool test_value;               // Result of most recent TEST
        uint16_t _reserved2;           // Reserved for alignment

    } FrameHeader;

    // Size of FrameHeader in words (rounded up)
    #define FRAME_HEADER_WORDS ((sizeof(FrameHeader) + sizeof(uint32_t) - 1) / sizeof(uint32_t))

    //==========================================================================
    // Frame Stack Manager
    //==========================================================================

    // Manages the frame stack using a FrameArena
    typedef struct
    {
        FrameArena arena;              // Memory arena for frames
        word_offset_t current;         // Current (top) frame offset, OFFSET_NONE if empty
        int depth;                     // Number of frames on stack (for debugging/limits)
    } FrameStack;

    //==========================================================================
    // Frame Stack Operations
    //==========================================================================

    // Initialize the frame stack with a memory region
    // Returns true if successful
    bool frame_stack_init(FrameStack *stack, void *memory, size_t size_bytes);

    // Reset the frame stack (pop all frames)
    void frame_stack_reset(FrameStack *stack);

    // Check if the frame stack is empty
    bool frame_stack_is_empty(FrameStack *stack);

    // Get current stack depth
    int frame_stack_depth(FrameStack *stack);

    // Get bytes used by all frames
    size_t frame_stack_used_bytes(FrameStack *stack);

    // Get bytes available for more frames
    size_t frame_stack_available_bytes(FrameStack *stack);

    //==========================================================================
    // Frame Operations
    //==========================================================================

    // Push a new frame for a procedure call
    // proc: procedure being called (NULL for top-level frame)
    // args: array of argument values (can be NULL if argc is 0)
    // argc: number of arguments
    // Returns the new frame's offset, or OFFSET_NONE if out of space
    word_offset_t frame_push(FrameStack *stack, UserProcedure *proc,
                             Value *args, int argc);

    // Reuse the current frame for a tail call (TCO)
    // Replaces procedure reference and rebinds parameters without deallocating
    // Clears local variables, test state, and expression stack
    // Returns true if successful, false if argument count mismatch or no frame
    bool frame_reuse(FrameStack *stack, UserProcedure *proc,
                     Value *args, int argc);

    // Pop the current frame
    // Returns the previous frame's offset (which becomes the new current)
    word_offset_t frame_pop(FrameStack *stack);

    // Get a pointer to the frame at the given offset
    // Returns NULL if offset is OFFSET_NONE
    FrameHeader *frame_at(FrameStack *stack, word_offset_t offset);

    // Get the current (top) frame
    // Returns NULL if stack is empty
    FrameHeader *frame_current(FrameStack *stack);

    // Get the current frame offset
    word_offset_t frame_current_offset(FrameStack *stack);

    //==========================================================================
    // Binding Operations
    //==========================================================================

    // Get all bindings for a frame (params followed by locals)
    Binding *frame_bindings(FrameStack *stack, FrameHeader *frame);

    // Get total number of bindings (params + locals)
    int frame_binding_count(FrameHeader *frame);

    // Find a binding by name in a single frame
    // Returns pointer to binding, or NULL if not found
    Binding *frame_find_binding(FrameStack *stack, FrameHeader *frame, const char *name);

    // Find a binding by name, searching from current frame up to root
    // Sets *found_frame to the frame containing the binding (if found)
    // Returns pointer to binding, or NULL if not found
    Binding *frame_find_binding_in_chain(FrameStack *stack, const char *name,
                                         FrameHeader **found_frame);

    //==========================================================================
    // Local Variable Operations
    //==========================================================================

    // Add a local variable to the current frame
    // Returns true if successful, false if out of space
    bool frame_add_local(FrameStack *stack, const char *name, Value value);

    // Declare a local variable (unbound) in the current frame
    // Returns true if successful
    bool frame_declare_local(FrameStack *stack, const char *name);

    // Set a binding's value (parameter or local)
    // Returns true if binding was found and updated
    bool frame_set_binding(FrameStack *stack, FrameHeader *frame,
                           const char *name, Value value);

    //==========================================================================
    // Expression Value Stack Operations
    //==========================================================================

    // Get the expression value stack for a frame
    Value *frame_values(FrameStack *stack, FrameHeader *frame);

    // Push a value onto the current frame's expression stack
    // Returns true if successful, false if out of space
    bool frame_push_value(FrameStack *stack, Value value);

    // Pop a value from the current frame's expression stack
    // Returns VALUE_NONE if stack is empty
    Value frame_pop_value(FrameStack *stack);

    // Peek at the top value without removing it
    // Returns VALUE_NONE if stack is empty
    Value frame_peek_value(FrameStack *stack);

    // Get the number of values on the current frame's expression stack
    int frame_value_count(FrameStack *stack);

    // Clear the expression stack of the current frame
    void frame_clear_values(FrameStack *stack);

    //==========================================================================
    // TEST State Operations
    //==========================================================================

    // Set TEST state in the current frame
    void frame_set_test(FrameStack *stack, bool value);

    // Get TEST state, searching from current frame up
    // Returns false if TEST has not been executed in any frame
    bool frame_get_test(FrameStack *stack, bool *out_value);

    // Check if TEST state is valid in the current scope chain
    bool frame_test_is_valid(FrameStack *stack);

    //==========================================================================
    // Continuation State Operations
    //==========================================================================

    // Save continuation state for mid-expression call
    void frame_save_continuation(FrameStack *stack, uint8_t pending_op,
                                 uint8_t pending_bp, uint8_t flags);

    // Clear continuation state (after resuming)
    void frame_clear_continuation(FrameStack *stack);

    // Check if current frame has pending continuation
    bool frame_has_pending_continuation(FrameStack *stack);

    //==========================================================================
    // Iteration / Debugging
    //==========================================================================

    // Iterate through all frames from current to root
    // callback is called with each frame; return false to stop iteration
    typedef bool (*FrameIterCallback)(FrameStack *stack, FrameHeader *frame,
                                      int depth, void *user_data);
    void frame_iterate(FrameStack *stack, FrameIterCallback callback, void *user_data);

    // Mark all values in all frames for garbage collection
    void frame_gc_mark_all(FrameStack *stack);

#ifdef __cplusplus
}
#endif
