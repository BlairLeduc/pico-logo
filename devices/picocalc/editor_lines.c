//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Memoised line lookup and word navigation over the editor buffer
//

#include <stdbool.h>

#include "editor_lines.h"

// The memo is (line, pos) where pos is the start of that line, so pos is 0 or
// sits just after a newline. Every walk below preserves that.

void editor_lines_reset(EditorLineIndex *ix)
{
    ix->line = 0;
    ix->pos = 0;
}

void editor_lines_edit(EditorLineIndex *ix, size_t pos)
{
    if (ix->pos > pos)
    {
        editor_lines_reset(ix);
    }
}

// Step the memo back to the start of the preceding line
static void step_back(EditorLineIndex *ix, const char *buf)
{
    size_t p = ix->pos - 1;  // The newline that ends the preceding line
    while (p > 0 && buf[p - 1] != '\n')
    {
        p--;
    }
    ix->pos = p;
    ix->line--;
}

size_t editor_lines_start(EditorLineIndex *ix, const char *buf, size_t len, int line)
{
    if (line <= 0)
    {
        return 0;
    }
    if (ix->pos > len)
    {
        editor_lines_reset(ix);  // Buffer shrank out from under the memo
    }

    while (ix->line > line)
    {
        step_back(ix, buf);
    }

    while (ix->line < line)
    {
        size_t p = ix->pos;
        while (p < len && buf[p] != '\n')
        {
            p++;
        }
        if (p >= len)
        {
            return len;  // No such line; leave the memo on the last one
        }
        ix->pos = p + 1;
        ix->line++;
    }

    return ix->pos;
}

int editor_lines_at_pos(EditorLineIndex *ix, const char *buf, size_t len, size_t pos)
{
    if (pos > len)
    {
        pos = len;
    }
    if (ix->pos > len)
    {
        editor_lines_reset(ix);  // Buffer shrank out from under the memo
    }

    while (ix->pos > pos)
    {
        step_back(ix, buf);
    }

    // Walk forward a line at a time while pos is beyond the end of this one
    while (ix->pos < pos)
    {
        size_t p = ix->pos;
        while (p < len && buf[p] != '\n')
        {
            p++;
        }
        if (p >= pos || p >= len)
        {
            break;  // pos is on this line (a newline belongs to the line it ends)
        }
        ix->pos = p + 1;
        ix->line++;
    }

    return ix->line;
}

//
//  Word navigation
//

static bool is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

size_t editor_word_left(const char *buf, size_t pos)
{
    while (pos > 0 && is_blank(buf[pos - 1]))
    {
        pos--;  // Back over the gap between the words
    }
    while (pos > 0 && !is_blank(buf[pos - 1]))
    {
        pos--;  // ... then to the start of the word it leaves us in
    }
    return pos;
}

size_t editor_word_right(const char *buf, size_t len, size_t pos)
{
    while (pos < len && !is_blank(buf[pos]))
    {
        pos++;  // Past the rest of the word we are in
    }
    while (pos < len && is_blank(buf[pos]))
    {
        pos++;  // ... and the gap after it, landing on the next word
    }
    return pos;
}
