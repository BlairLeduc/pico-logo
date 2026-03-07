//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Evaluator for Logo expressions and instructions.
//

#pragma once

#include "value.h"
#include "token_source.h"
#include "frame.h"
#include "eval_ops.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Evaluator state
    typedef struct Evaluator
    {
        TokenSource token_source;  // Current token source (lexer or node iterator)
        FrameStack *frames;        // Procedure call frame stack (NULL at top-level)
        OpStack *op_stack;         // Operation stack for explicit control flow
        int paren_depth;           // Track nested parentheses for greedy args
        int error_code;
        const char *error_context; // For error messages
        bool in_tail_position;     // True if evaluating last instruction of procedure body
        int proc_depth;            // Depth of user procedure calls (for TCO)
        int repcount;              // Current repeat count (for REPCOUNT)
        int primitive_arg_depth;   // > 0 when collecting args for primitives
        int user_arg_depth;        // > 0 when collecting args for user procedures
    } Evaluator;

    // Initialize evaluator with a lexer
    void eval_init(Evaluator *eval, Lexer *lexer);

    // Set the frame stack for procedure calls
    void eval_set_frames(Evaluator *eval, FrameStack *frames);

    // Get the frame stack (may be NULL)
    FrameStack *eval_get_frames(Evaluator *eval);

    // Check if evaluator is currently inside a procedure
    bool eval_in_procedure(Evaluator *eval);

    // Evaluate a single instruction (command with args)
    Result eval_instruction(Evaluator *eval);

    // Evaluate a single expression (operation that returns a value)
    Result eval_expression(Evaluator *eval);

    // Run a list as code (for repeat, etc.) - errors if list outputs a value
    Result eval_run_list(Evaluator *eval, Node list);

    // Run a list as an expression (for run, if as operation)
    // Allows the list to output a value which is returned
    Result eval_run_list_expr(Evaluator *eval, Node list);

    // Check if there are more tokens to process
    bool eval_at_end(Evaluator *eval);

    // Run the op stack trampoline from the given base depth.
    // Processes operations until the stack returns to base_depth.
    Result eval_trampoline(Evaluator *eval, int base_depth);

    //==========================================================================
    // Operation Stack Helpers (for primitives)
    //==========================================================================

    // Push an OP_REPEAT operation. Returns RESULT_NONE on success.
    Result eval_push_repeat(Evaluator *eval, int count, Node body);

    // Push an OP_FOREVER operation. Returns RESULT_NONE on success.
    Result eval_push_forever(Evaluator *eval, Node body);

    // Push an OP_WHILE operation. Returns RESULT_NONE on success.
    Result eval_push_while(Evaluator *eval, Node predicate, Node body);

    // Push an OP_UNTIL operation. Returns RESULT_NONE on success.
    Result eval_push_until(Evaluator *eval, Node predicate, Node body);

    // Push an OP_DO_WHILE operation. Returns RESULT_NONE on success.
    Result eval_push_do_while(Evaluator *eval, Node body, Node predicate);

    // Push an OP_DO_UNTIL operation. Returns RESULT_NONE on success.
    Result eval_push_do_until(Evaluator *eval, Node body, Node predicate);

    // Push an OP_FOR operation. Returns RESULT_NONE on success.
    Result eval_push_for(Evaluator *eval, const char *varname, float start,
                         float limit, float step, Node body,
                         Value saved_value, bool had_value, bool in_procedure);

    // Push an OP_CATCH operation. Returns RESULT_NONE on success.
    Result eval_push_catch(Evaluator *eval, const char *tag, Node body);

    // Push an OP_PROC_CALL operation. Always uses sub-trampoline (synchronous).
    Result eval_push_proc_call(Evaluator *eval, struct UserProcedure *proc, int argc, Value *args);

#ifdef __cplusplus
}
#endif
