//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Frame Arena - LIFO stack allocator for procedure call frames.
//
//  This arena allocator provides O(1) allocation and deallocation for
//  procedure call frames. All allocations are word-aligned (4 bytes).
//  Only the most recent allocation can be extended, which matches the
//  LIFO nature of procedure calls.
//
//  Memory is addressed using word offsets (16-bit) instead of pointers,
//  giving a maximum arena size of 256KB while saving memory on references.
//

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    //==========================================================================
    // Types
    //==========================================================================

    // Word offset type - index into arena measured in words (4-byte units)
    // Maximum addressable: 65535 words = 262140 bytes = ~256KB
    typedef uint16_t word_offset_t;

    // Special value indicating no valid offset (null reference)
    #define OFFSET_NONE ((word_offset_t)0xFFFF)

    // Frame arena structure
    typedef struct
    {
        uint32_t *base;          // Start of arena memory (word-aligned)
        word_offset_t top;       // Current allocation point (in words from base)
        word_offset_t capacity;  // Total capacity (in words)
    } FrameArena;

    //==========================================================================
    // Offset/Pointer Conversion
    //==========================================================================

    // Convert a word offset to a pointer
    // Returns NULL if offset is OFFSET_NONE
    static inline void *arena_offset_to_ptr(FrameArena *arena, word_offset_t offset)
    {
        if (offset == OFFSET_NONE)
            return NULL;
        return (void *)(arena->base + offset);
    }

    // Convert a pointer within the arena to a word offset
    // Caller must ensure ptr is within arena bounds
    static inline word_offset_t arena_ptr_to_offset(FrameArena *arena, void *ptr)
    {
        if (ptr == NULL)
            return OFFSET_NONE;
        return (word_offset_t)((uint32_t *)ptr - arena->base);
    }

    //==========================================================================
    // Arena Operations
    //==========================================================================

    // Initialize an arena with the given memory region
    // Memory must be word-aligned. Size is in bytes.
    // Returns true if successful, false if memory is NULL or misaligned.
    bool arena_init(FrameArena *arena, void *memory, size_t size_bytes);

    // Allocate word_count words from the arena
    // Returns the offset of the allocated block, or OFFSET_NONE if full
    word_offset_t arena_alloc_words(FrameArena *arena, uint16_t word_count);

    // Free all memory after (and including) the given mark
    // This effectively pops all allocations made after the mark
    void arena_free_to(FrameArena *arena, word_offset_t mark);

    // Get the current top of the arena (useful for saving a mark before allocation)
    word_offset_t arena_top(FrameArena *arena);

    // Extend the most recent allocation by additional words
    // Only valid if no other allocations have been made since
    // Returns true if successful, false if not enough space
    bool arena_extend(FrameArena *arena, uint16_t additional_words);

    // Check if an offset points to the most recent (top) allocation
    // This is required for arena_extend to work
    bool arena_is_top_allocation(FrameArena *arena, word_offset_t offset, uint16_t size_words);

    //==========================================================================
    // Arena Queries
    //==========================================================================

    // Get the number of words currently used
    static inline word_offset_t arena_used(FrameArena *arena)
    {
        return arena->top;
    }

    // Get the number of words available for allocation
    static inline word_offset_t arena_available(FrameArena *arena)
    {
        return arena->capacity - arena->top;
    }

    // Check if the arena is empty
    static inline bool arena_is_empty(FrameArena *arena)
    {
        return arena->top == 0;
    }

    // Get total capacity in words
    static inline word_offset_t arena_capacity(FrameArena *arena)
    {
        return arena->capacity;
    }

    // Get total capacity in bytes
    static inline size_t arena_capacity_bytes(FrameArena *arena)
    {
        return (size_t)arena->capacity * sizeof(uint32_t);
    }

    // Get used space in bytes
    static inline size_t arena_used_bytes(FrameArena *arena)
    {
        return (size_t)arena->top * sizeof(uint32_t);
    }

    // Get available space in bytes
    static inline size_t arena_available_bytes(FrameArena *arena)
    {
        return (size_t)(arena->capacity - arena->top) * sizeof(uint32_t);
    }

#ifdef __cplusplus
}
#endif
