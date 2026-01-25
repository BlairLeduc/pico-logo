//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Compiler scaffolding for VM bytecode (Phase 0).
//

#pragma once

#include "bytecode.h"
#include "eval.h"
#include "value.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct Compiler
    {
        Evaluator *eval;
        Bytecode *bc;
        bool instruction_mode;
        bool tail_position;
        int tail_depth;
    } Compiler;

    Result compile_expression(Compiler *c, Bytecode *bc);
    Result compile_instruction(Compiler *c, Bytecode *bc);
    Result compile_list(Compiler *c, Node list, Bytecode *bc);
    Result compile_list_instructions(Compiler *c, Node list, Bytecode *bc, bool enable_tco);
    Result compile_list_instructions_expr(Compiler *c, Node list, Bytecode *bc, bool enable_tco);
    bool compiler_skip_instruction(TokenSource *ts);

#ifdef __cplusplus
}
#endif
