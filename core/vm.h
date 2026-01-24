//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  VM evaluator scaffolding (Phase 0).
//

#pragma once

#include "bytecode.h"
#include "value.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct VM
    {
        Bytecode *bc;
    } VM;

    void vm_init(VM *vm);
    Result vm_exec(VM *vm, Bytecode *bc);

#ifdef __cplusplus
}
#endif
