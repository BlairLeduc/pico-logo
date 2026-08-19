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
#include "primitives.h"
#include "procedures.h"

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


    //==========================================================================
    // Name resolution (P10 M2)
    //==========================================================================

    // What a word token's name resolves to. At most one of `prim` and `proc`
    // is non-NULL; both NULL means the name is neither, which is itself a
    // cached answer rather than a failure to look.
    typedef struct
    {
        const Primitive *prim;
        UserProcedure *proc;
        Node atom;         // the interned name, NODE_NIL if interning failed
        const char *name;  // the interned characters, for error messages
    } WordBinding;

    // Resolve a word token's name, memoising the answer on its atom when the
    // token came from a list. Replaces a binary search over ~390 primitives
    // followed by a linear scan of the procedure table, per call, per frame.
    WordBinding resolve_word(Token t);

    // Parse a primary expression (number, word, list, proc call, etc.)
    Result eval_primary(Evaluator *eval);

    // Pratt parser for infix expressions with minimum binding power
    Result eval_expr_bp(Evaluator *eval, int min_bp);

    //==========================================================================
    // Step functions from eval_steps.c (called by trampoline in eval.c)
    //==========================================================================
    Result step_run_list(Evaluator *eval, EvalOp *op);
    Result step_if(Evaluator *eval, EvalOp *op);
    Result step_repeat(Evaluator *eval, EvalOp *op);
    Result step_forever(Evaluator *eval, EvalOp *op);
    Result step_loop(Evaluator *eval, EvalOp *op);
    Result step_for(Evaluator *eval, EvalOp *op);
    Result step_catch(Evaluator *eval, EvalOp *op);
    Result step_runresult(Evaluator *eval, EvalOp *op);
    Result step_proc_call(Evaluator *eval, EvalOp *op);
    Result step_expr_eval(Evaluator *eval, EvalOp *op);
    Result step_prim_call(Evaluator *eval, EvalOp *op);
    Result step_paren_group(Evaluator *eval, EvalOp *op);

#ifdef __cplusplus
}
#endif
