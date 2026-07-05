//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Colour costume pool. See costumes.h for the model.
//
//  Pixel blocks are kept packed at the front of the pool in slot-offset
//  order. Deleting or replacing a costume memmoves the blocks above it
//  down and fixes up the affected offsets. Nothing outside this module
//  holds pointers into the pool across a put/delete (turtle rasters are
//  copies), so compaction is safe.
//

#include "costumes.h"

#include <string.h>

typedef struct
{
    bool used;
    uint8_t w, h;
    uint16_t offset;  // into pool[]
} CostumeSlot;

static CostumeSlot slots[COSTUME_SLOTS];
static uint8_t pool[COSTUME_POOL_SIZE];
static int pool_used = 0;

void costumes_clear(void)
{
    memset(slots, 0, sizeof(slots));
    pool_used = 0;
}

// Remove a slot's block from the pool, shifting later blocks down
static void pool_release(uint8_t slot)
{
    int size = slots[slot].w * slots[slot].h;
    int offset = slots[slot].offset;

    memmove(pool + offset, pool + offset + size, (size_t)(pool_used - offset - size));
    pool_used -= size;
    slots[slot].used = false;

    for (int i = 0; i < COSTUME_SLOTS; i++)
    {
        if (slots[i].used && slots[i].offset > offset)
        {
            slots[i].offset = (uint16_t)(slots[i].offset - size);
        }
    }
}

bool costume_put(uint8_t slot, uint8_t w, uint8_t h, const uint8_t *pixels)
{
    if (slot == 0 || slot >= COSTUME_SLOTS || !pixels ||
        w < COSTUME_MIN_DIM || w > COSTUME_MAX_DIM ||
        h < COSTUME_MIN_DIM || h > COSTUME_MAX_DIM)
    {
        return false;
    }

    if (slots[slot].used)
    {
        pool_release(slot);
    }

    int size = w * h;
    if (pool_used + size > COSTUME_POOL_SIZE)
    {
        return false;  // Pool full (caller reports ERR_OUT_OF_SPACE)
    }

    memcpy(pool + pool_used, pixels, (size_t)size);
    slots[slot].used = true;
    slots[slot].w = w;
    slots[slot].h = h;
    slots[slot].offset = (uint16_t)pool_used;
    pool_used += size;
    return true;
}

bool costume_get(uint8_t slot, uint8_t *w, uint8_t *h, const uint8_t **pixels)
{
    if (slot >= COSTUME_SLOTS || !slots[slot].used)
    {
        return false;
    }
    if (w) *w = slots[slot].w;
    if (h) *h = slots[slot].h;
    if (pixels) *pixels = pool + slots[slot].offset;
    return true;
}

void costume_delete(uint8_t slot)
{
    if (slot < COSTUME_SLOTS && slots[slot].used)
    {
        pool_release(slot);
    }
}

int costume_pool_free(void)
{
    return COSTUME_POOL_SIZE - pool_used;
}
