//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Undo journal for the full-screen editor (docs/vi-mode-design.md §8)
//
//  A record holds both sides of a change -- the bytes that went and the bytes
//  that came -- so undo and redo are the same splice read in opposite
//  directions. Storing both costs what the smaller side is worth, and nearly
//  every change has an empty side: a delete inserts nothing and an insert
//  deletes nothing. Only `r`, `~` and `:s` carry both, and they are short.
//

#include "editor_undo.h"

#include <stdint.h>
#include <string.h>

// The header sits at the start of a record, followed by the deleted bytes and
// then the inserted ones. It is copied in and out rather than cast: records are
// packed end to end, so nothing in the store is aligned.
typedef struct
{
    uint32_t pos;
    uint32_t del_len;
    uint32_t ins_len;
    uint32_t prev;         // Offset of the record before it, or NO_PREV
    uint32_t step_start;   // First record of a step: `u` stops after it
} UndoHeader;

#define HDR_SIZE   sizeof(UndoHeader)
#define NO_PREV    UINT32_MAX

static UndoHeader header_at(const EditorUndo *u, size_t off)
{
    UndoHeader h;
    memcpy(&h, u->store + off, HDR_SIZE);
    return h;
}

static void header_put(EditorUndo *u, size_t off, const UndoHeader *h)
{
    memcpy(u->store + off, h, HDR_SIZE);
}

static size_t record_size(const UndoHeader *h)
{
    return HDR_SIZE + h->del_len + h->ins_len;
}

// Where the deleted and inserted payloads of a record live
static char *deleted_of(EditorUndo *u, size_t off)
{
    return u->store + off + HDR_SIZE;
}

static char *inserted_of(EditorUndo *u, size_t off, const UndoHeader *h)
{
    return u->store + off + HDR_SIZE + h->del_len;
}

// The offset just past the last record still applied -- where redo starts
static size_t applied_end(const EditorUndo *u)
{
    if (!u->has_last)
    {
        return 0;
    }
    UndoHeader h = header_at(u, u->last);
    return u->last + record_size(&h);
}

static void clear_records(EditorUndo *u)
{
    u->used = 0;
    u->last = 0;
    u->step = 0;
    u->has_last = false;
    u->has_step = false;
}

//
// Replace [pos, pos + remove) with n bytes of text. `capacity` includes the
// NUL. Returns false, having changed nothing, when the result would not fit.
//
static bool splice(char *buf, size_t *len, size_t capacity, size_t pos,
                   size_t remove, const char *text, size_t n)
{
    if (pos > *len)
    {
        return false;
    }
    if (remove > *len - pos)
    {
        remove = *len - pos;
    }
    if (*len - remove + n + 1 > capacity)
    {
        return false;
    }

    memmove(buf + pos + n, buf + pos + remove, *len - pos - remove);
    if (n > 0)
    {
        memcpy(buf + pos, text, n);
    }
    *len = *len - remove + n;
    buf[*len] = '\0';
    return true;
}

//
//  Recording
//

void editor_undo_init(EditorUndo *u, char *store, size_t capacity)
{
    memset(u, 0, sizeof(*u));
    if (store != NULL && capacity > HDR_SIZE)
    {
        u->store = store;
        u->capacity = capacity;
    }
}

void editor_undo_reset(EditorUndo *u)
{
    clear_records(u);
    u->pending = false;
    u->abandoned = false;
}

void editor_undo_begin(EditorUndo *u)
{
    u->pending = true;
    u->abandoned = false;
    u->has_step = false;
}

//
// Drop the oldest step, which is the run of records from the bottom of the
// store up to the next one that starts a step. Refuses to eat into the step
// being recorded (`limit`), since half a step is worse than none of it.
//
static bool drop_oldest_step(EditorUndo *u, size_t limit)
{
    if (u->used == 0 || limit == 0)
    {
        return false;
    }

    UndoHeader h = header_at(u, 0);
    size_t off = record_size(&h);
    while (off < u->used)
    {
        h = header_at(u, off);
        if (h.step_start)
        {
            break;
        }
        off += record_size(&h);
    }
    if (off > limit)
    {
        return false;
    }

    memmove(u->store, u->store + off, u->used - off);
    u->used -= off;

    // Everything left moved down by `off`, and whatever pointed into what went
    // is now the oldest record there is
    for (size_t o = 0; o < u->used; o += record_size(&h))
    {
        h = header_at(u, o);
        h.prev = (h.prev == NO_PREV || h.prev < off) ? NO_PREV : h.prev - (uint32_t)off;
        header_put(u, o, &h);
    }

    if (u->has_last)
    {
        if (u->last < off)
        {
            u->has_last = false;
            u->last = 0;
        }
        else
        {
            u->last -= off;
        }
    }
    if (u->has_step)
    {
        u->step -= off;
    }
    return true;
}

//
// Fold a change into the record before it where it is a continuation of one:
// typing on from where the last insertion ended, or backspacing over what was
// just typed. Without this an insert session costs a record per keystroke,
// which is what would make the 1 KB SRAM tier useless.
//
static bool coalesce(EditorUndo *u, size_t pos,
                     const char *deleted, size_t deleted_len,
                     const char *inserted, size_t inserted_len)
{
    if (u->pending || !u->has_last)
    {
        return false;
    }

    UndoHeader h = header_at(u, u->last);
    if (u->last + record_size(&h) != u->used)
    {
        return false;  // Not the topmost record; nothing to grow into
    }

    if (deleted_len == 0 && inserted_len > 0 && pos == h.pos + h.ins_len)
    {
        if (u->used + inserted_len > u->capacity)
        {
            return false;
        }
        memcpy(u->store + u->used, inserted, inserted_len);
        u->used += inserted_len;
        h.ins_len += (uint32_t)inserted_len;
        header_put(u, u->last, &h);
        return true;
    }

    if (inserted_len == 0 && deleted_len > 0 && deleted_len <= h.ins_len &&
        pos + deleted_len == h.pos + h.ins_len &&
        memcmp(inserted_of(u, u->last, &h) + h.ins_len - deleted_len,
               deleted, deleted_len) == 0)
    {
        h.ins_len -= (uint32_t)deleted_len;
        u->used -= deleted_len;
        if (h.del_len == 0 && h.ins_len == 0)
        {
            // Typed and then backspaced away: there is no change left to undo
            u->used = u->last;
            u->has_last = (h.prev != NO_PREV);
            u->last = u->has_last ? h.prev : 0;
            if (u->has_step && u->step >= u->used)
            {
                u->has_step = false;
            }
            return true;
        }
        header_put(u, u->last, &h);
        return true;
    }

    return false;
}

void editor_undo_record(EditorUndo *u, size_t pos,
                        const char *deleted, size_t deleted_len,
                        const char *inserted, size_t inserted_len)
{
    if (u->store == NULL || u->abandoned || (deleted_len == 0 && inserted_len == 0))
    {
        return;
    }

    // A new change makes whatever was undone unreachable
    u->used = applied_end(u);
    if (u->has_step && u->step >= u->used)
    {
        u->has_step = false;
    }

    if (coalesce(u, pos, deleted, deleted_len, inserted, inserted_len))
    {
        return;
    }

    size_t need = HDR_SIZE + deleted_len + inserted_len;
    while (u->used + need > u->capacity)
    {
        if (!drop_oldest_step(u, u->has_step ? u->step : u->used))
        {
            // One change bigger than the whole journal: nothing before it can
            // be undone either, since the buffer is about to leave every state
            // the records describe
            clear_records(u);
            u->abandoned = true;
            return;
        }
    }

    size_t off = u->used;
    UndoHeader h;
    h.pos = (uint32_t)pos;
    h.del_len = (uint32_t)deleted_len;
    h.ins_len = (uint32_t)inserted_len;
    h.prev = u->has_last ? (uint32_t)u->last : NO_PREV;
    h.step_start = u->pending ? 1u : 0u;
    header_put(u, off, &h);
    if (deleted_len > 0)
    {
        memcpy(deleted_of(u, off), deleted, deleted_len);
    }
    if (inserted_len > 0)
    {
        memcpy(inserted_of(u, off, &h), inserted, inserted_len);
    }

    u->used = off + need;
    u->last = off;
    u->has_last = true;
    if (!u->has_step)
    {
        u->step = off;
        u->has_step = true;
    }
    u->pending = false;
}

//
//  Reversing and repeating
//

bool editor_undo_undo(EditorUndo *u, char *buf, size_t *len, size_t capacity,
                      size_t *out_pos)
{
    if (u->store == NULL || !u->has_last)
    {
        return false;
    }

    size_t first = *len;
    bool any = false;

    for (;;)
    {
        size_t off = u->last;
        UndoHeader h = header_at(u, off);

        if (!splice(buf, len, capacity, h.pos, h.ins_len, deleted_of(u, off), h.del_len))
        {
            break;  // The earlier text no longer fits; leave the rest applied
        }
        any = true;
        if (h.pos < first)
        {
            first = h.pos;
        }

        u->has_last = (h.prev != NO_PREV);
        u->last = u->has_last ? h.prev : 0;
        if (h.step_start || !u->has_last)
        {
            break;
        }
    }

    if (any)
    {
        *out_pos = first;
    }
    return any;
}

bool editor_undo_redo(EditorUndo *u, char *buf, size_t *len, size_t capacity,
                      size_t *out_pos)
{
    if (u->store == NULL)
    {
        return false;
    }

    size_t off = applied_end(u);
    if (off >= u->used)
    {
        return false;
    }

    size_t first = *len;
    bool any = false;

    for (;;)
    {
        UndoHeader h = header_at(u, off);
        if (!splice(buf, len, capacity, h.pos, h.del_len, inserted_of(u, off, &h), h.ins_len))
        {
            break;
        }
        any = true;
        if (h.pos < first)
        {
            first = h.pos;
        }

        u->last = off;
        u->has_last = true;
        off += record_size(&h);
        if (off >= u->used || header_at(u, off).step_start)
        {
            break;
        }
    }

    if (any)
    {
        *out_pos = first;
    }
    return any;
}
