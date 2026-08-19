//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Undo journal for the full-screen editor (docs/vi-mode-design.md §8)
//

#pragma once

#include <stdbool.h>
#include <stddef.h>

// A journal of the changes made to the edit buffer, enough to reverse them and
// to put them back. It is fed by the editor at every point that moves a byte,
// and it knows nothing about keys, screens or lines: a change is a position, the
// bytes that were there, and the bytes that took their place.
//
// The store is memory the caller owns -- a slice of the aux/PSRAM region on a
// board that has one, a small heap block otherwise -- so the tier is a run-time
// decision. With no store there is no undo, which is what a board whose
// allocation failed gets.
//
// Records are packed in the order they were made, each linked back to the one
// before it. `u` reverses everything back to the start of a *step* (§ below),
// leaving the records in place so `Ctrl R` can put them back; the next change
// drops whatever was undone. When a record will not fit, the oldest whole steps
// are dropped to make room.
typedef struct
{
    char *store;      // Journal memory, NULL when there is no undo
    size_t capacity;  // Its size in bytes
    size_t used;      // Bytes through the last record, undone ones included
    size_t last;      // Header offset of the last record still applied
    size_t step;      // ... of the first record of the step being recorded
    bool has_last;
    bool has_step;
    bool pending;     // The next record starts a new step
    bool abandoned;   // This step did not fit; the journal was cleared
} EditorUndo;

// Point the journal at its store, or at nothing (store NULL, capacity 0)
void editor_undo_init(EditorUndo *u, char *store, size_t capacity);

// Forget every record, keeping the store. Called on editor entry, beside
// editor_lines_reset.
void editor_undo_reset(EditorUndo *u);

// Start a new step: the next record recorded begins one, and `u` reverses a
// whole step at a time. The editor calls this once per keystroke, except while
// insert mode is running, so that everything typed between `i` and `Esc` is one
// undo -- which is what makes the SRAM tier tolerable rather than useless.
void editor_undo_begin(EditorUndo *u);

// Note a change about to be made: `deleted_len` bytes at `pos` (which must
// still be there, so call this *before* the change) are replaced by the
// `inserted_len` bytes at `inserted`. Either side may be empty. Typing coalesces
// into the record before it rather than making one per character.
void editor_undo_record(EditorUndo *u, size_t pos,
                        const char *deleted, size_t deleted_len,
                        const char *inserted, size_t inserted_len);

// Reverse one step / put one step back, rewriting buf and *len. `capacity`
// includes the terminating NUL, as the editor's buffer size does. *out_pos is
// set to the earliest offset the step touched -- where the cursor goes, and
// what the line memo has to be told about.
//
// Returns false when there is nothing left in that direction, having changed
// nothing.
bool editor_undo_undo(EditorUndo *u, char *buf, size_t *len, size_t capacity,
                      size_t *out_pos);
bool editor_undo_redo(EditorUndo *u, char *buf, size_t *len, size_t capacity,
                      size_t *out_pos);
