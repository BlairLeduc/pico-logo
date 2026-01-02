//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "frame_arena.h"
#include <string.h>

bool arena_init(FrameArena *arena, void *memory, size_t size_bytes)
{
    // Validate inputs
    if (arena == NULL || memory == NULL)
    {
        return false;
    }

    // Check alignment (must be word-aligned)
    if ((uintptr_t)memory % sizeof(uint32_t) != 0)
    {
        return false;
    }

    // Calculate capacity in words
    size_t capacity_words = size_bytes / sizeof(uint32_t);

    // Check capacity fits in word_offset_t (must be < OFFSET_NONE)
    if (capacity_words >= OFFSET_NONE)
    {
        // Clamp to maximum addressable
        capacity_words = OFFSET_NONE - 1;
    }

    arena->base = (uint32_t *)memory;
    arena->top = 0;
    arena->capacity = (word_offset_t)capacity_words;

    return true;
}

word_offset_t arena_alloc_words(FrameArena *arena, uint16_t word_count)
{
    // Check for zero allocation
    if (word_count == 0)
    {
        return OFFSET_NONE;
    }

    // Check if we have enough space
    word_offset_t available = arena->capacity - arena->top;
    if (word_count > available)
    {
        return OFFSET_NONE;
    }

    // Allocate by bumping the top pointer
    word_offset_t offset = arena->top;
    arena->top += word_count;

    return offset;
}

void arena_free_to(FrameArena *arena, word_offset_t mark)
{
    // Validate mark is within bounds
    if (mark <= arena->top)
    {
        arena->top = mark;
    }
    // If mark > top, this is invalid but we silently ignore
    // (could happen if arena was corrupted)
}

word_offset_t arena_top(FrameArena *arena)
{
    return arena->top;
}

bool arena_extend(FrameArena *arena, uint16_t additional_words)
{
    // Check for zero extension
    if (additional_words == 0)
    {
        return true;
    }

    // Check if we have enough space
    word_offset_t available = arena->capacity - arena->top;
    if (additional_words > available)
    {
        return false;
    }

    // Extend by bumping the top pointer
    arena->top += additional_words;

    return true;
}

bool arena_is_top_allocation(FrameArena *arena, word_offset_t offset, uint16_t size_words)
{
    // Check if the allocation at 'offset' with 'size_words' ends at the current top
    // This means it's the most recent allocation and can be extended
    if (offset == OFFSET_NONE)
    {
        return false;
    }

    word_offset_t end = offset + size_words;
    return end == arena->top;
}
