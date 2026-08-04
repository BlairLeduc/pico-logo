//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Layout of the per-atom memo word (P10 M1/M2).
//
//  Words are interned and immutable, so anything derivable from a word's
//  characters is a pure function of its atom and can be derived once. The
//  atom entry carries 16 bits for that purpose (mem_word_view in memory.h);
//  this header is the single definition of what those bits mean, shared by
//  the three files that touch them:
//
//    token_source.c  writes and reads the word class
//    eval_expr.c     writes and reads the name binding
//    procedures.c    drops every binding when the procedure table changes
//
//  memory.c deliberately knows none of this: it owns the storage, callers
//  own the meaning.
//
//      bit  0-4   word class     (0 = not yet computed)
//      bit  5-6   binding kind   (0 = not yet resolved)
//      bit  7-15  binding index  (0..511)
//
//  A memo of 0 therefore means "nothing known", which is what a freshly
//  interned atom always reads back.
//

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Word class occupies the low bits. token_source.c owns the values; the
    // only thing fixed here is how many bits they get.
    #define ATOM_MEMO_CLASS_MASK   0x001Fu

    // Binding kind. ATOM_BIND_NONE is a resolved answer, not an absence: it
    // records that the name is neither a primitive nor a user procedure, so a
    // repeat lookup is skipped as well.
    #define ATOM_BIND_UNRESOLVED   0u
    #define ATOM_BIND_PRIMITIVE    1u
    #define ATOM_BIND_PROCEDURE    2u
    #define ATOM_BIND_NONE         3u

    #define ATOM_MEMO_BIND_SHIFT   5
    #define ATOM_MEMO_BIND_MASK    0x0060u

    #define ATOM_MEMO_INDEX_SHIFT  7
    #define ATOM_MEMO_INDEX_MASK   0xFF80u

    // Largest table index a memo can hold. MAX_PRIMITIVES and MAX_PROCEDURES
    // are static-asserted against this where they are defined.
    #define ATOM_MEMO_INDEX_LIMIT  512u

    // Mask that keeps the class and drops the binding — what procedures.c
    // sweeps the atom region with when the procedure table changes.
    #define ATOM_MEMO_KEEP_CLASS   ATOM_MEMO_CLASS_MASK

    static inline uint16_t atom_memo_class(uint16_t memo)
    {
        return (uint16_t)(memo & ATOM_MEMO_CLASS_MASK);
    }

    static inline uint16_t atom_memo_set_class(uint16_t memo, uint16_t cls)
    {
        return (uint16_t)((memo & ~ATOM_MEMO_CLASS_MASK) | cls);
    }

    static inline uint16_t atom_memo_bind_kind(uint16_t memo)
    {
        return (uint16_t)((memo & ATOM_MEMO_BIND_MASK) >> ATOM_MEMO_BIND_SHIFT);
    }

    static inline unsigned atom_memo_bind_index(uint16_t memo)
    {
        return (unsigned)((memo & ATOM_MEMO_INDEX_MASK) >> ATOM_MEMO_INDEX_SHIFT);
    }

    static inline uint16_t atom_memo_set_binding(uint16_t memo, uint16_t kind,
                                                 unsigned index)
    {
        return (uint16_t)((memo & ~(ATOM_MEMO_BIND_MASK | ATOM_MEMO_INDEX_MASK)) |
                          (uint16_t)(kind << ATOM_MEMO_BIND_SHIFT) |
                          (uint16_t)(index << ATOM_MEMO_INDEX_SHIFT));
    }

#ifdef __cplusplus
}
#endif
