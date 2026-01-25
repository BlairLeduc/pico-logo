//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Bytecode definitions for VM-based evaluator (Phase 0 scaffolding).
//

#pragma once

#include "value.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Simple arena placeholder (Phase 0)
    typedef struct Arena
    {
        uint8_t *base;
        size_t capacity;
        size_t used;
    } Arena;

    typedef enum
    {
        OP_NOP = 0,
        OP_PUSH_CONST,
        OP_LOAD_VAR,
        OP_CALL_PRIM,
        OP_CALL_PRIM_INSTR,
        OP_CALL_USER,
        OP_CALL_USER_EXPR,
        OP_CALL_USER_TAIL,
        OP_PRIM_ARGS_BEGIN,
        OP_PRIM_ARGS_END,
        OP_NEG,
        OP_ADD,
        OP_SUB,
        OP_MUL,
        OP_DIV,
        OP_EQ,
        OP_LT,
        OP_GT,
        OP_BEGIN_INSTR,
        OP_END_INSTR
    } Op;

    typedef struct
    {
        uint8_t op;
        uint16_t a;
        uint16_t b;
    } Instruction;

    typedef struct
    {
        Instruction *code;
        size_t code_len;
        size_t code_cap;
        Value *const_pool;
        size_t const_len;
        size_t const_cap;
        Arena *arena;
    } Bytecode;

    void bc_init(Bytecode *bc, Arena *arena);
    bool bc_emit(Bytecode *bc, Op op, uint16_t a, uint16_t b);

#ifdef __cplusplus
}
#endif
