//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Evaluator for Logo expressions and instructions.
//

#pragma once

#include "value.h"
#include "token_source.h"
#include "frame.h"

#ifndef EVAL_USE_VM
#define EVAL_USE_VM 0
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    // Evaluator state
    typedef struct Evaluator
    {
        TokenSource token_source;  // Current token source (lexer or node iterator)
        FrameStack *frames;        // Procedure call frame stack (NULL at top-level)
        int paren_depth;           // Track nested parentheses for greedy args
        int error_code;
        const char *error_context; // For error messages
        bool in_tail_position;     // True if evaluating last instruction of procedure body
        int proc_depth;            // Depth of user procedure calls (for TCO)
        int repcount;              // Current repeat count (for REPCOUNT)
        int primitive_arg_depth;   // > 0 when collecting args for primitives (CPS fallback)
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

    // Run a list as code with tail call optimization enabled
    // Used internally by proc_call for procedure bodies
    Result eval_run_list_with_tco(Evaluator *eval, Node list, bool enable_tco);

    // Check if there are more tokens to process
    bool eval_at_end(Evaluator *eval);

#ifdef __cplusplus
}
#endif
