//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Internal header shared between eval.c, eval_expr.c, and eval_steps.c.
//  Not part of the public API — do not include from other modules.
//

#pragma once

#include "eval.h"
#include "eval_ops.h"
#include "token_source.h"
#include "value.h"

#ifdef __cplusplus
extern "C"
{
#endif

    //==========================================================================
    // Binding powers for the Pratt parser
    //==========================================================================
    #define BP_NONE 0
    #define BP_COMPARISON 10      // = < >
    #define BP_ADDITIVE 20        // + -
    #define BP_MULTIPLICATIVE 30  // * /

    //==========================================================================
    // Shared global state
    //==========================================================================
    extern OpStack global_op_stack;

    //==========================================================================
    // Inline helpers (used by all eval_*.c files)
    //==========================================================================

    // Get current token without consuming it
    static inline Token peek(Evaluator *eval)
    {
        return token_source_peek(&eval->token_source);
    }

    // Consume the current token
    static inline void advance(Evaluator *eval)
    {
        token_source_next(&eval->token_source);
    }

    // Get binding power for an infix operator token
    static inline int get_infix_bp(TokenType type)
    {
        switch (type)
        {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return BP_ADDITIVE;
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
            return BP_MULTIPLICATIVE;
        case TOKEN_EQUALS:
        case TOKEN_LESS_THAN:
        case TOKEN_GREATER_THAN:
            return BP_COMPARISON;
        default:
            return BP_NONE;
        }
    }

    //==========================================================================
    // Functions from eval_expr.c (expression parsing)
    //==========================================================================

    // Apply a binary infix operator to two values
    Result apply_binary_op(TokenType op_type, Value left, Value right);

    // Check if a string looks like a number
    bool is_number_string(const char *str, size_t len);

    // Parse a primary expression (number, word, list, proc call, etc.)
    Result eval_primary(Evaluator *eval);

    // Pratt parser for infix expressions with minimum binding power
    Result eval_expr_bp(Evaluator *eval, int min_bp);

    //==========================================================================
    // Step functions from eval_steps.c (called by trampoline in eval.c)
    //==========================================================================
    Result step_run_list(Evaluator *eval, EvalOp *op);
    Result step_repeat(Evaluator *eval, EvalOp *op);
    Result step_forever(Evaluator *eval, EvalOp *op);
    Result step_while(Evaluator *eval, EvalOp *op);
    Result step_until(Evaluator *eval, EvalOp *op);
    Result step_do_while(Evaluator *eval, EvalOp *op);
    Result step_do_until(Evaluator *eval, EvalOp *op);
    Result step_for(Evaluator *eval, EvalOp *op);
    Result step_catch(Evaluator *eval, EvalOp *op);
    Result step_proc_call(Evaluator *eval, EvalOp *op);
    Result step_expr_eval(Evaluator *eval, EvalOp *op);
    Result step_prim_call(Evaluator *eval, EvalOp *op);

#ifdef __cplusplus
}
#endif
