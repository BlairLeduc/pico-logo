//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Evaluator for Logo expressions and instructions.
//

#pragma once

#include "value.h"
#include "lexer.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Evaluator state
    typedef struct Evaluator
    {
        Lexer *lexer;
        Token current;
        bool has_current;
        int paren_depth;           // Track nested parentheses for greedy args
        int error_code;
        const char *error_context; // For error messages
        bool in_tail_position;     // True if evaluating last instruction of procedure body
        int proc_depth;            // Depth of user procedure calls (for TCO)
    } Evaluator;

    // Initialize evaluator with a lexer
    void eval_init(Evaluator *eval, Lexer *lexer);

    // Evaluate a single instruction (command with args)
    Result eval_instruction(Evaluator *eval);

    // Evaluate a single expression (operation that returns a value)
    Result eval_expression(Evaluator *eval);

    // Run a list as code (for run, repeat, if, etc.)
    Result eval_run_list(Evaluator *eval, Node list);

    // Run a list as code with tail call optimization enabled
    // Used internally by proc_call for procedure bodies
    Result eval_run_list_with_tco(Evaluator *eval, Node list, bool enable_tco);

    // Check if there are more tokens to process
    bool eval_at_end(Evaluator *eval);

#ifdef __cplusplus
}
#endif
