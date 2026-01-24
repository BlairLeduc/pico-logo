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
    } Compiler;

    Result compile_expression(Compiler *c, Bytecode *bc);
    Result compile_instruction(Compiler *c, Bytecode *bc);
    Result compile_list(Compiler *c, Node list, Bytecode *bc);

#ifdef __cplusplus
}
#endif
