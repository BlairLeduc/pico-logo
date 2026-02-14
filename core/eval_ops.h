//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Evaluator operation stack for explicit control flow.
//
//  Instead of using C recursion for control-flow primitives (repeat, if,
//  while, catch, etc.), we use an explicit operation stack. Each operation
//  is a tagged union representing "what to do next." The trampoline loop
//  in eval.c pops the top operation and executes one step.
//
//  This eliminates C stack overflow for deeply nested or highly iterating
//  Logo programs, which is critical on the RP2040/RP2350 with limited
//  C stack space (4KB).
//

#pragma once

#include "value.h"
#include "token_source.h"

// Forward declaration to avoid circular include with procedures.h
struct UserProcedure;

#ifdef __cplusplus
extern "C"
{
#endif

    //==========================================================================
    // Operation Kinds
    //==========================================================================

    typedef enum
    {
        OP_RUN_LIST,      // Run a list as a sequence of instructions
        OP_RUN_LIST_EXPR, // Run a list as an expression (last value returned)
        OP_REPEAT,        // repeat count [body]
        OP_FOREVER,       // forever [body]
        OP_IF,            // if condition [then] [else]
        OP_WHILE,         // while [pred] [body]
        OP_UNTIL,         // until [pred] [body]
        OP_DO_WHILE,      // do.while [body] [pred]
        OP_DO_UNTIL,      // do.until [body] [pred]
        OP_FOR,           // for [var start limit step] [body]
        OP_RUN,           // run [list]
        OP_CATCH,         // catch "tag [body]
        OP_PROC_CALL,     // User procedure call (frame + body execution)
    } EvalOpKind;

    //==========================================================================
    // Per-Operation State
    //==========================================================================

    typedef struct
    {
        // No extra state needed - OP_RUN_LIST just iterates tokens
    } RunListState;

    typedef struct
    {
        Node body;
        int count;
        int i;
        int previous_repcount;
    } RepeatState;

    typedef struct
    {
        Node body;
        int previous_repcount;
        int iteration;
    } ForeverState;

    typedef struct
    {
        Node chosen_branch;  // The branch to execute (already chosen by primitive)
    } IfState;

    typedef struct
    {
        Node predicate;
        Node body;
        uint8_t phase;       // 0 = evaluate predicate, 1 = execute body
    } WhileState;

    typedef struct
    {
        Node predicate;
        Node body;
        uint8_t phase;       // 0 = evaluate predicate, 1 = execute body
    } UntilState;

    typedef struct
    {
        Node body;
        Node predicate;
        uint8_t phase;       // 0 = execute body, 1 = evaluate predicate
    } DoWhileState;

    typedef struct
    {
        Node body;
        Node predicate;
        uint8_t phase;       // 0 = execute body, 1 = evaluate predicate
    } DoUntilState;

    typedef struct
    {
        const char *varname;
        float start;
        float limit;
        float step;
        float current;
        Node body;
        Value saved_value;
        bool had_value;
        bool in_procedure;   // Whether we're in a procedure scope
        uint8_t phase;       // 0 = loop iteration, 1 = cleanup
    } ForState;

    typedef struct
    {
        Node body;            // The list to run
    } RunState;

    typedef struct
    {
        const char *tag;
        Node body;            // The list to run under catch protection
        int phase;
    } CatchState;

    typedef struct
    {
        struct UserProcedure *proc;  // The procedure being executed
        Node current_line;           // Current body line cursor
        uint8_t phase;               // 0 = push first line, 1+ = handle line result
    } ProcCallState;

    //==========================================================================
    // Operation Flags
    //==========================================================================

    #define OP_FLAG_NONE          0x00
    #define OP_FLAG_IS_EXPR       0x01   // This run_list should return a value
    #define OP_FLAG_ENABLE_TCO    0x02   // Tail call optimization enabled

    //==========================================================================
    // EvalOp Structure
    //==========================================================================

    // Maximum operation stack depth (configurable per platform via cmake)
    // Default 256; host/tests can use 512, RP2040 should use 128.
    #ifndef MAX_OP_STACK_DEPTH
    #define MAX_OP_STACK_DEPTH 256
    #endif

    typedef struct EvalOp
    {
        EvalOpKind kind;
        uint8_t flags;
        TokenSource saved_source;    // Token source to restore when this op completes
        Result result;               // Result from sub-operation (inter-phase passing)
        union
        {
            RunListState run_list;
            RepeatState repeat;
            ForeverState forever;
            IfState if_state;
            WhileState while_state;
            UntilState until_state;
            DoWhileState do_while;
            DoUntilState do_until;
            ForState for_state;
            RunState run;
            CatchState catch_state;
            ProcCallState proc_call;
        };
    } EvalOp;

    //==========================================================================
    // Operation Stack
    //==========================================================================

    typedef struct
    {
        EvalOp ops[MAX_OP_STACK_DEPTH];
        int top;              // Index of next free slot (0 = empty)
    } OpStack;

    // Initialize operation stack
    void op_stack_init(OpStack *stack);

    // Push a new operation, returns pointer to it or NULL if full
    EvalOp *op_stack_push(OpStack *stack);

    // Pop the top operation
    void op_stack_pop(OpStack *stack);

    // Peek at the top operation, returns NULL if empty
    EvalOp *op_stack_peek(OpStack *stack);

    // Check if stack is empty
    bool op_stack_is_empty(OpStack *stack);

    // Get current depth
    int op_stack_depth(OpStack *stack);

#ifdef __cplusplus
}
#endif
