//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Vi key layer for the full-screen editor (docs/vi-mode-design.md)
//

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "core/limits.h"
#include "editor_undo.h"

// A page, in lines. The editor shows EDITOR_VISIBLE_ROWS of content, and the
// paging motions have to agree with it; editor.c asserts that they do. It is
// here rather than in editor.h because this file is built on its own for the
// host test and must not depend on the screen.
#define EDITOR_VI_PAGE_LINES 30

typedef enum
{
    VI_NORMAL,
    VI_INSERT,
    VI_VISUAL,
    VI_VISUAL_LINE,
    VI_CMDLINE,
} ViMode;

typedef enum
{
    VI_ACT_NONE,          // Key consumed, nothing to do (a count, half an operator)
    VI_ACT_MOVE,          // Put the cursor at `start`
    VI_ACT_DELETE,        // Cut [start, end) to the copy buffer
    VI_ACT_YANK,          // Copy [start, end) to the copy buffer
    VI_ACT_CHANGE,        // Delete [start, end) and enter insert mode
    VI_ACT_PASTE_AFTER,   // Paste `count` times after the cursor / below the line
    VI_ACT_PASTE_BEFORE,  // ... before the cursor / above the line
    VI_ACT_PASTE_OVER,    // Replace [start, end) with the copy buffer (visual `p`)
    VI_ACT_INDENT,        // Shift the lines in [start, end) by `count` tab stops
    VI_ACT_REPLACE_CHAR,  // Overwrite [start, end) with `ch`, or with a single
                          // line break when `ch` is '\n' (`r` and Return)
    VI_ACT_OPEN_BELOW,    // Open a line below the cursor's and enter insert mode
    VI_ACT_OPEN_ABOVE,    // ... above
    VI_ACT_JOIN,          // Join `count` lines from the cursor's
    VI_ACT_TOGGLE_CASE,   // Flip the case of [start, end)
    VI_ACT_SEARCH,        // Search for `pattern`; `ch` is '/' (forward) or '?'
    VI_ACT_SUBSTITUTE,    // Substitute over the lines spanned by [start, end)
    VI_ACT_UNDO,          // Reverse `count` changes
    VI_ACT_REDO,          // ... and put them back
    VI_ACT_WRITE,         // :w -- write the buffer out and stay in the editor
    VI_ACT_ACCEPT,        // :wq :x ZZ
    VI_ACT_QUIT,          // :q -- accept, but only when nothing has been changed
    VI_ACT_CANCEL,        // :q! ZQ
    VI_ACT_REDRAW,        // Only the mode changed; repaint the footer and cursor
    VI_ACT_BEEP,          // Not a command; `msg` is the footer text
} ViActionKind;

// What a keystroke asks the editor to do. Byte offsets, never screen rows: the
// state machine has no idea where the view is.
typedef struct
{
    ViActionKind kind;
    size_t start;      // Half-open byte range, start <= end. VI_ACT_MOVE puts
    size_t end;        // the target in both.
    bool linewise;     // The range is whole lines, newline included
    char ch;           // VI_ACT_REPLACE_CHAR, VI_ACT_SEARCH
    int count;         // Repeat count; tab stops for VI_ACT_INDENT (may be negative)
    const char *msg;   // Footer text for VI_ACT_BEEP
} ViAction;

typedef struct
{
    ViMode mode;
    int count;             // Digits typed so far, 0 = none
    int op_count;          // Count typed before the pending operator
    char pending_op;       // 0, 'd', 'c', 'y', '<', '>'
    char pending_prefix;   // 0, 'g', 'Z', 'f', 'F', 't', 'T', 'r', or 'i'/'a'
                           // waiting for a text object (§15)
    size_t anchor;         // Visual mode's other end
    bool modified;         // Set by the editor; `:q` refuses when it is true

    char last_find;        // The last f/F/t/T, for `;` and `,`
    char last_find_char;

    // `.` replays the keys of the last change rather than its byte range, so a
    // repeat at a new cursor recomputes its own motion. Only the last key of a
    // command produces an action, and none of the earlier ones touch the
    // buffer, so the replay can feed the whole sequence through and hand back
    // the action the final key returns.
    char stroke[LOGO_VI_REPEAT_MAX];   // The command being typed
    int stroke_len;
    char repeat_keys[LOGO_VI_REPEAT_MAX];  // The last one that changed something
    int repeat_len;
    int repeat_count;      // The count, kept apart so `3.` can replace it
    bool replaying;

    char cmdline[LOGO_VI_CMDLINE_MAX + 1];  // Includes the leading ':', '/' or '?'
    size_t cmdline_len;

    char pattern[LOGO_VI_TEXT_MAX + 1];
    size_t pattern_len;
    char replacement[LOGO_VI_TEXT_MAX + 1];
    size_t replacement_len;
    bool search_forward;
    bool sub_global;
} ViState;

// Start in normal mode with nothing pending. Search text and the `.` record are
// cleared too: the editor calls this once per session.
void editor_vi_reset(ViState *st);

// Feed one key. `buf` is read-only and nothing is mutated but `st`.
//
// Returns true when vi consumed the key. False means the editor should handle
// it as it does outside vi mode -- which is how insert mode gets the arrow
// keys, backspace and every printable character for free, and how Brk keeps
// its unconditional cancel from every mode.
bool editor_vi_key(ViState *st, const char *buf, size_t len, size_t cursor,
                   int key, ViAction *out);

// The footer text for the current mode: "-- NORMAL --" and friends, or the
// command line while one is being typed.
const char *editor_vi_status(const ViState *st);

// Substitute `pat` with `rep` on every line the byte range [range_start,
// range_end) touches -- the first match on each line, or all of them when
// `global`. Matching is case-insensitive, as editor_search_find is.
//
// This is the one place the vi layer writes to the buffer. A substitute is a
// loop with a capacity check and an off-by-one at every step, which is exactly
// what wants a host test, and describing it as a byte range would need one
// action per match.
//
// buf/len: the buffer to rewrite; *len is updated. capacity includes the NUL.
// undo: journal to record each substitution in, or NULL. One match is one
//   record, which is far less than the whole rewritten span would be and is
//   what lets a `:%s` over a large buffer still be undone.
// out_cursor: set to the start of the last line changed.
//
// Returns the number of substitutions. Nothing is changed when there is no
// match, or when the result would not fit in capacity.
size_t editor_vi_substitute(char *buf, size_t *len, size_t capacity,
                            size_t range_start, size_t range_end,
                            const char *pat, size_t pat_len,
                            const char *rep, size_t rep_len,
                            bool global, EditorUndo *undo, size_t *out_cursor);
