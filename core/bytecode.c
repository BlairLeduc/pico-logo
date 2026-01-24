//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Bytecode definitions for VM-based evaluator (Phase 0 scaffolding).
//

#include "bytecode.h"

enum
{
    BC_DEFAULT_CODE_CAP = 256,
    BC_DEFAULT_CONST_CAP = 64
};

static void *arena_alloc(Arena *arena, size_t size)
{
    if (!arena || !arena->base)
        return NULL;
    if (arena->used + size > arena->capacity)
        return NULL;
    void *ptr = arena->base + arena->used;
    arena->used += size;
    return ptr;
}

void bc_init(Bytecode *bc, Arena *arena)
{
    if (!bc)
        return;

    bc->arena = arena;
    bc->code_len = 0;
    bc->const_len = 0;

    if (!bc->code && arena)
    {
        bc->code = (Instruction *)arena_alloc(arena, sizeof(Instruction) * BC_DEFAULT_CODE_CAP);
        bc->code_cap = bc->code ? BC_DEFAULT_CODE_CAP : 0;
    }

    if (!bc->const_pool && arena)
    {
        bc->const_pool = (Value *)arena_alloc(arena, sizeof(Value) * BC_DEFAULT_CONST_CAP);
        bc->const_cap = bc->const_pool ? BC_DEFAULT_CONST_CAP : 0;
    }
}

bool bc_emit(Bytecode *bc, Op op, uint16_t a, uint16_t b)
{
    if (!bc || !bc->code || bc->code_len >= bc->code_cap)
        return false;
    bc->code[bc->code_len++] = (Instruction){.op = (uint8_t)op, .a = a, .b = b};
    return true;
}
