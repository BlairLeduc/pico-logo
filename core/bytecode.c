//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Bytecode definitions for VM-based evaluator (Phase 0 scaffolding).
//

#include "bytecode.h"

void bc_init(Bytecode *bc, Arena *arena)
{
    if (!bc)
        return;
    bc->code = NULL;
    bc->code_len = 0;
    bc->code_cap = 0;
    bc->const_pool = NULL;
    bc->const_len = 0;
    bc->const_cap = 0;
    bc->arena = arena;
}

bool bc_emit(Bytecode *bc, Op op, uint16_t a, uint16_t b)
{
    (void)bc;
    (void)op;
    (void)a;
    (void)b;
    return false;
}
