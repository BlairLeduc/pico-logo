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
    } Evaluator;

    // Initialize evaluator with a lexer
    void eval_init(Evaluator *eval, Lexer *lexer);

    // Evaluate a single instruction (command with args)
    Result eval_instruction(Evaluator *eval);

    // Evaluate a single expression (operation that returns a value)
    Result eval_expression(Evaluator *eval);

    // Run a list as code (for run, repeat, if, etc.)
    Result eval_run_list(Evaluator *eval, Node list);

    // Check if there are more tokens to process
    bool eval_at_end(Evaluator *eval);

#ifdef __cplusplus
}
#endif
