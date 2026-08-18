//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Memoised line lookup over the editor buffer
//

#pragma once

#include <stddef.h>

// Turning a line number into a buffer offset means counting newlines, and a
// single keystroke asks for one several times (cursor, horizontal scroll, the
// dirty-line redraw), a full redraw once per visible row. Counting from the
// start of the buffer every time costs no more than the buffer is big, which is
// why it never mattered while the buffer was a few kilobytes; with the 256 KB
// PSRAM buffer it does.
//
// So remember the last line resolved and scan from there. Lookups are local —
// the next one is nearly always the same line or one beside it — so the scan is
// a handful of bytes rather than a quarter of a megabyte.
typedef struct
{
    int line;    // Line number the memo describes
    size_t pos;  // Offset of that line's first character
} EditorLineIndex;

// Forget the memo. Call after any change that rewrites the buffer wholesale.
void editor_lines_reset(EditorLineIndex *ix);

// Note an edit at pos. Text before an edit does not move, so the memo survives
// any edit at or after it; earlier ones drop it.
void editor_lines_edit(EditorLineIndex *ix, size_t pos);

// The offset of the first character of line (0-based), or len when the line is
// past the end of the buffer.
size_t editor_lines_start(EditorLineIndex *ix, const char *buf, size_t len, int line);

// The line (0-based) that the character at pos belongs to. A newline belongs to
// the line it ends.
int editor_lines_at_pos(EditorLineIndex *ix, const char *buf, size_t len, size_t pos);
