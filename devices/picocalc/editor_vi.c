//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Vi key layer for the full-screen editor (docs/vi-mode-design.md)
//
//  Everything here is a pure function of (buffer, cursor, key): no LCD, no
//  screen rows, and nothing written to the buffer except by
//  editor_vi_substitute, which says why in the header. That is what lets the
//  whole command set be tested on the host, which editor.c cannot be.
//

#include "editor_vi.h"
#include "editor_pattern.h"
#include "keyboard.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

//
//  Lines
//
//  No memo here: the state machine runs once per keystroke, where editor.c's
//  lookups run several times per redrawn row.
//

static size_t line_start_of(const char *buf, size_t pos)
{
    while (pos > 0 && buf[pos - 1] != '\n')
    {
        pos--;
    }
    return pos;
}

static size_t line_end_of(const char *buf, size_t len, size_t pos)
{
    while (pos < len && buf[pos] != '\n')
    {
        pos++;
    }
    return pos;
}

// The start of the line after the one pos is on, or len on the last line
static size_t next_line_start(const char *buf, size_t len, size_t pos)
{
    size_t end = line_end_of(buf, len, pos);
    return end < len ? end + 1 : len;
}

static size_t first_non_blank(const char *buf, size_t len, size_t pos)
{
    size_t start = line_start_of(buf, pos);
    size_t end = line_end_of(buf, len, pos);
    while (start < end && (buf[start] == ' ' || buf[start] == '\t'))
    {
        start++;
    }
    return start;
}

static bool line_is_blank(const char *buf, size_t len, size_t line_start)
{
    size_t end = line_end_of(buf, len, line_start);
    for (size_t i = line_start; i < end; i++)
    {
        if (buf[i] != ' ' && buf[i] != '\t')
        {
            return false;
        }
    }
    return true;
}

// The start of line n, counting from 1. A line past the end clamps to the last.
static size_t goto_line(const char *buf, size_t len, int n)
{
    if (n <= 1)
    {
        return 0;
    }
    size_t pos = 0;
    for (int i = 1; i < n; i++)
    {
        size_t end = line_end_of(buf, len, pos);
        if (end >= len)
        {
            return pos;
        }
        pos = end + 1;
    }
    return pos;
}

// The number of the line `pos` is on, counting from 1. Passing `len` gives the
// number of the last line, which is how many lines the buffer has: one, plus
// one for every newline before the point -- the same count editor.c's line
// index keeps, so a buffer ending in a newline has an empty last line and `G`
// reaches it.
static int line_number_of(const char *buf, size_t pos)
{
    int n = 1;
    for (size_t i = 0; i < pos; i++)
    {
        if (buf[i] == '\n')
        {
            n++;
        }
    }
    return n;
}

// Move `delta` lines, keeping the column where the line is long enough
static size_t move_lines(const char *buf, size_t len, size_t pos, int delta)
{
    size_t start = line_start_of(buf, pos);
    size_t col = pos - start;

    while (delta > 0)
    {
        size_t end = line_end_of(buf, len, start);
        if (end >= len)
        {
            break;  // Already on the last line
        }
        start = end + 1;
        delta--;
    }
    while (delta < 0 && start > 0)
    {
        start = line_start_of(buf, start - 1);
        delta++;
    }

    size_t end = line_end_of(buf, len, start);
    return start + col > end ? end : start + col;
}

//
//  Words
//
//  Vi's three classes, not editor_lines.c's blank/non-blank pair: Logo is
//  brackets, quotes and colons all the way down, and a `dw` that took the `]`
//  with the word before it would be wrong far more often than it was right.
//  `W`, `B` and `E` are the blank-separated forms for when that is what is
//  wanted.
//

#define CLASS_BLANK 0
#define CLASS_PUNCT 1
#define CLASS_WORD  2

static int char_class(char c)
{
    if (c == ' ' || c == '\t' || c == '\n')
    {
        return CLASS_BLANK;
    }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_')
    {
        return CLASS_WORD;
    }
    return CLASS_PUNCT;
}

// The class as `w` sees it, or as `W` does, where every non-blank is one class
static int class_at(const char *buf, size_t pos, bool big)
{
    int c = char_class(buf[pos]);
    return (big && c != CLASS_BLANK) ? CLASS_PUNCT : c;
}

static size_t word_fwd(const char *buf, size_t len, size_t pos, bool big)
{
    if (pos >= len)
    {
        return len;
    }
    int start_class = class_at(buf, pos, big);
    while (pos < len && start_class != CLASS_BLANK && class_at(buf, pos, big) == start_class)
    {
        pos++;
    }
    while (pos < len && char_class(buf[pos]) == CLASS_BLANK)
    {
        pos++;
    }
    return pos;
}

static size_t word_back(const char *buf, size_t pos, bool big)
{
    if (pos == 0)
    {
        return 0;
    }
    pos--;
    while (pos > 0 && char_class(buf[pos]) == CLASS_BLANK)
    {
        pos--;
    }
    if (char_class(buf[pos]) == CLASS_BLANK)
    {
        return 0;
    }
    int start_class = class_at(buf, pos, big);
    while (pos > 0 && class_at(buf, pos - 1, big) == start_class)
    {
        pos--;
    }
    return pos;
}

static size_t word_end(const char *buf, size_t len, size_t pos, bool big)
{
    if (len == 0 || pos + 1 >= len)
    {
        return pos;
    }
    size_t p = pos + 1;
    while (p < len && char_class(buf[p]) == CLASS_BLANK)
    {
        p++;
    }
    if (p >= len)
    {
        return pos;  // No word left to end on; a motion that fails stays put
    }
    int start_class = class_at(buf, p, big);
    while (p + 1 < len && class_at(buf, p + 1, big) == start_class)
    {
        p++;
    }
    return p;
}

// The end of the run of one character class starting at `pos`, within a line
static size_t chunk_end(const char *buf, size_t line_end, size_t pos, bool big)
{
    if (pos >= line_end)
    {
        return pos;
    }
    int cls = class_at(buf, pos, big);
    while (pos < line_end && class_at(buf, pos, big) == cls)
    {
        pos++;
    }
    return pos;
}

// `iw` / `aw`: the run of one class the cursor is in -- blanks are a class, so
// `diw` on a gap deletes the gap. `aw` adds the blanks after the word, or the
// ones before it when there are none after. An object stays on its line: a
// Logo line is short and one that swallowed the break would join two.
static bool word_object(const char *buf, size_t len, size_t pos, bool big, bool around,
                        int count, size_t *out_start, size_t *out_end)
{
    size_t ls = line_start_of(buf, pos);
    size_t le = line_end_of(buf, len, pos);

    if (ls == le)
    {
        *out_start = *out_end = ls;  // An empty line has nothing to take
        return true;
    }
    if (pos >= le)
    {
        pos = le - 1;  // The cursor sits on the line break; use the last character
    }

    int cls = class_at(buf, pos, big);
    size_t s = pos, e = pos;
    while (s > ls && class_at(buf, s - 1, big) == cls)
    {
        s--;
    }
    e = chunk_end(buf, le, e, big);

    if (around)
    {
        if (cls == CLASS_BLANK)
        {
            e = chunk_end(buf, le, e, big);  // Started on a gap: take the word after it
        }
        for (int i = 1; i < count; i++)
        {
            if (e < le && char_class(buf[e]) == CLASS_BLANK)
            {
                e = chunk_end(buf, le, e, big);
            }
            e = chunk_end(buf, le, e, big);
        }
        size_t after = e;
        while (after < le && char_class(buf[after]) == CLASS_BLANK)
        {
            after++;
        }
        if (after > e)
        {
            e = after;
        }
        else
        {
            while (s > ls && char_class(buf[s - 1]) == CLASS_BLANK)
            {
                s--;
            }
        }
    }
    else
    {
        for (int i = 1; i < count; i++)
        {
            e = chunk_end(buf, le, e, big);
        }
    }

    *out_start = s;
    *out_end = e;
    return true;
}

// The word the cursor stands on, or the next one along the line -- vi's rule
// for `*`, `#` and `gd`. Word characters only: `:size` gives `size`, which is
// what a search for the name wants, since the same name is spelled `:size` and
// `"size` in the two lines that use it.
static bool word_at(const char *buf, size_t len, size_t cursor,
                    size_t *out_start, size_t *out_end)
{
    size_t line_end = line_end_of(buf, len, cursor);
    size_t p = cursor;
    while (p < line_end && char_class(buf[p]) != CLASS_WORD)
    {
        p++;
    }
    if (p >= line_end)
    {
        return false;
    }

    size_t s = p;
    while (s > 0 && char_class(buf[s - 1]) == CLASS_WORD)
    {
        s--;  // A newline is blank, so this cannot leave the line
    }
    size_t e = p;
    while (e < line_end && char_class(buf[e]) == CLASS_WORD)
    {
        e++;
    }

    *out_start = s;
    *out_end = e;
    return true;
}

// Logo is case-insensitive and so is every search in the editor, so `gd` is
// too. editor_pattern.c keeps its own copy of this for the same reason
// editor_vi.c has no memo: one line is cheaper than a shared header.
static char to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool same_word(const char *buf, size_t s, size_t e,
                      const char *word, size_t word_len)
{
    if (e - s != word_len)
    {
        return false;
    }
    for (size_t i = 0; i < word_len; i++)
    {
        if (to_lower(buf[s + i]) != to_lower(word[i]))
        {
            return false;
        }
    }
    return true;
}

// The first non-blank word on `line` is `word`, case-insensitively, and only a
// blank or the line's end follows it -- so an indented `to` counts and a line
// beginning `total` does not. Logo has two markers and this is the whole test
// for both of them, which is what makes `gd`, `]]` and `ip` one predicate
// rather than three (docs/vi-mode-design.md §24.3). `*out_after` is left just
// past the word.
static bool line_starts_word(const char *buf, size_t len, size_t line,
                             const char *word, size_t word_len, size_t *out_after)
{
    size_t end = line_end_of(buf, len, line);
    size_t p = line;
    while (p < end && (buf[p] == ' ' || buf[p] == '\t'))
    {
        p++;
    }
    if (end - p < word_len)
    {
        return false;
    }
    for (size_t i = 0; i < word_len; i++)
    {
        if (to_lower(buf[p + i]) != word[i])
        {
            return false;
        }
    }
    size_t after = p + word_len;
    if (after < end && char_class(buf[after]) != CLASS_BLANK)
    {
        return false;
    }
    if (out_after != NULL)
    {
        *out_after = after;
    }
    return true;
}

static bool line_starts_definition(const char *buf, size_t len, size_t line)
{
    return line_starts_word(buf, len, line, "to", 2, NULL);
}

// `gd` -- where a procedure is defined. Not a pattern search: a Logo definition
// is `to name` at the head of a line, and matching that shape directly is both
// exact and shorter than the pattern it would take. Case-insensitive, as the
// language is. Returns the offset of the name on the `to` line.
static bool find_definition(const char *buf, size_t len,
                            const char *word, size_t word_len, size_t *out_pos)
{
    for (size_t line = 0; line < len; line = next_line_start(buf, len, line))
    {
        size_t p;
        if (!line_starts_word(buf, len, line, "to", 2, &p))
        {
            continue;
        }

        size_t end = line_end_of(buf, len, line);
        while (p < end && (buf[p] == ' ' || buf[p] == '\t'))
        {
            p++;
        }
        size_t name_end = p;
        while (name_end < end && char_class(buf[name_end]) == CLASS_WORD)
        {
            name_end++;
        }
        if (name_end > p && same_word(buf, p, name_end, word, word_len))
        {
            *out_pos = p;
            return true;
        }
    }
    return false;
}

//
//  Procedures -- what a Logo buffer is actually made of, and what `edall` puts
//  many of in one place (§24)
//

// `]]` -- the next definition after the cursor's line, or the end of the
// buffer. Clamping rather than beeping is what makes `d]]` in the last
// procedure delete the rest of the file, which is the operation you want there.
static size_t def_fwd(const char *buf, size_t len, size_t pos)
{
    for (size_t line = next_line_start(buf, len, pos); line < len;
         line = next_line_start(buf, len, line))
    {
        if (line_starts_definition(buf, len, line))
        {
            return line;
        }
    }
    return len;
}

// `[[` -- back to the definition this line belongs to, or to the one before it
// when the cursor is already at the head of a definition.
static size_t def_back(const char *buf, size_t len, size_t pos)
{
    size_t line = line_start_of(buf, pos);
    if (pos > line && line_starts_definition(buf, len, line))
    {
        return line;
    }
    while (line > 0)
    {
        line = line_start_of(buf, line - 1);
        if (line_starts_definition(buf, len, line))
        {
            return line;
        }
    }
    return 0;
}

// `ip` and `ap` -- the body of the definition the cursor is in, or the whole of
// it. Both markers are needed, and a second `to` met before the `end` refuses
// the object: a definition that has not been closed yet is not one, and
// bounding it at the next `to` instead would have `dap` on a half-typed
// procedure eat the blank lines and comments under it (§24.3). Linewise, so
// `dap` takes whole lines and `cip` empties the body.
static bool procedure_object(const char *buf, size_t len, size_t cursor, bool around,
                             size_t *out_start, size_t *out_end)
{
    size_t here = line_start_of(buf, cursor);

    size_t head = here;
    while (!line_starts_definition(buf, len, head))
    {
        if (head == 0)
        {
            return false;
        }
        head = line_start_of(buf, head - 1);
    }

    // Forward from the cursor's own line, not from the head: a cursor sitting
    // below an `end` is not inside anything, and starting here is what finds
    // the next `to` before an `end` and says so
    size_t tail = (here == head) ? next_line_start(buf, len, head) : here;
    while (tail < len && !line_starts_word(buf, len, tail, "end", 3, NULL))
    {
        if (line_starts_definition(buf, len, tail))
        {
            return false;
        }
        tail = next_line_start(buf, len, tail);
    }
    if (tail >= len)
    {
        return false;
    }

    *out_start = around ? head : next_line_start(buf, len, head);
    *out_end = around ? next_line_start(buf, len, tail) : tail;
    return true;
}

//
//  Paragraphs -- a blank line either way, which in Logo is the gap between
//  two procedure definitions
//

static size_t para_fwd(const char *buf, size_t len, size_t pos)
{
    size_t line = next_line_start(buf, len, pos);
    while (line < len && !line_is_blank(buf, len, line))
    {
        size_t next = next_line_start(buf, len, line);
        if (next == line)
        {
            break;
        }
        line = next;
    }
    return line;
}

static size_t para_back(const char *buf, size_t len, size_t pos)
{
    size_t line = line_start_of(buf, pos);
    if (line == 0)
    {
        return 0;
    }
    line = line_start_of(buf, line - 1);
    while (line > 0 && !line_is_blank(buf, len, line))
    {
        line = line_start_of(buf, line - 1);
    }
    return line;
}

//
//  Brackets
//
//  `%` matches its own kind and counts only that kind's nesting, which is what
//  makes `d%` over a `[...]` reliable in a language written in brackets.
//

static const char bracket_open[] = "([{";
static const char bracket_close[] = ")]}";

static int bracket_index(const char *set, char c)
{
    for (int i = 0; set[i] != '\0'; i++)
    {
        if (set[i] == c)
        {
            return i;
        }
    }
    return -1;
}

static bool match_bracket(const char *buf, size_t len, size_t pos, size_t *out)
{
    // Vi looks along the rest of the line for something to match
    size_t end = line_end_of(buf, len, pos);
    int open_idx = -1, close_idx = -1;
    while (pos < end)
    {
        open_idx = bracket_index(bracket_open, buf[pos]);
        close_idx = bracket_index(bracket_close, buf[pos]);
        if (open_idx >= 0 || close_idx >= 0)
        {
            break;
        }
        pos++;
    }
    if (pos >= end)
    {
        return false;
    }

    bool forward = open_idx >= 0;
    char here = buf[pos];
    char other = forward ? bracket_close[open_idx] : bracket_open[close_idx];

    int depth = 0;
    for (;;)
    {
        if (buf[pos] == here)
        {
            depth++;
        }
        else if (buf[pos] == other)
        {
            depth--;
            if (depth == 0)
            {
                *out = pos;
                return true;
            }
        }
        if (forward)
        {
            if (++pos >= len)
            {
                return false;
            }
        }
        else
        {
            if (pos == 0)
            {
                return false;
            }
            pos--;
        }
    }
}

// The `open`/`close` pair the cursor is inside, `depth` levels out: back to an
// unmatched opener, then forward to its match, counting that kind's nesting
// only. It crosses lines, because a Logo group routinely does, and the cursor
// on either bracket counts as being inside that pair.
//
// Not match_bracket: that one scans along the line for a bracket to start
// from, which is what makes `%` run backwards from inside a group (B35). This
// one starts where the cursor is and works outwards.
static bool enclosing_pair(const char *buf, size_t len, size_t pos, char open, char close,
                           int depth, size_t *out_open, size_t *out_close)
{
    if (pos >= len)
    {
        return false;
    }

    for (int level = 0; level < depth; level++)
    {
        size_t p = pos;

        if (level > 0 || buf[p] != open)
        {
            int nest = 0;
            for (;;)
            {
                if (p == 0)
                {
                    return false;
                }
                p--;
                if (buf[p] == close)
                {
                    nest++;
                }
                else if (buf[p] == open)
                {
                    if (nest == 0)
                    {
                        break;
                    }
                    nest--;
                }
            }
        }

        size_t q = p;
        int nest = 0;
        for (;;)
        {
            if (buf[q] == open)
            {
                nest++;
            }
            else if (buf[q] == close && --nest == 0)
            {
                break;
            }
            if (++q >= len)
            {
                return false;
            }
        }

        *out_open = p;
        *out_close = q;
        pos = p;  // The next level out starts from this opener and looks past it
    }
    return true;
}

// Resolve an object key after `i` or `a` into the byte range it names. Objects
// are absolute ranges rather than motions, so they skip operator_range: there
// is no cursor to pair them with and nothing to make inclusive. `*out_linewise`
// says whether the range is whole lines -- only `ip` and `ap` are, and every
// object that came before them was charwise (§24.3).
static bool text_object(const char *buf, size_t len, size_t cursor, int key, bool around,
                        int count, size_t *out_start, size_t *out_end, bool *out_linewise)
{
    *out_linewise = false;

    if (key == 'w' || key == 'W')
    {
        return word_object(buf, len, cursor, key == 'W', around, count, out_start, out_end);
    }

    if (key == 'p')
    {
        // A count means nothing to a procedure -- they do not nest -- and is
        // ignored rather than refused
        *out_linewise = true;
        return procedure_object(buf, len, cursor, around, out_start, out_end);
    }

    int idx = bracket_index(bracket_open, (char)key);
    if (idx < 0)
    {
        idx = bracket_index(bracket_close, (char)key);  // `]` is a synonym for `[`
    }
    if (idx < 0)
    {
        return false;
    }

    size_t open, close;
    if (!enclosing_pair(buf, len, cursor, bracket_open[idx], bracket_close[idx],
                        count, &open, &close))
    {
        return false;
    }
    *out_start = around ? open : open + 1;
    *out_end = around ? close + 1 : close;
    return true;
}

//
//  Motions
//

typedef struct
{
    size_t pos;
    bool linewise;   // The operator takes whole lines
    bool inclusive;  // The operator takes the character under `pos` as well
} ViMotion;

// Find `ch` on the cursor's line, `count` occurrences away. f/t look forward
// from the character after the cursor, F/T back from the one before it.
static bool find_char(const char *buf, size_t len, size_t cursor, char kind, char ch,
                      int count, size_t *out)
{
    size_t start = line_start_of(buf, cursor);
    size_t end = line_end_of(buf, len, cursor);
    size_t pos = cursor;

    if (kind == 'f' || kind == 't')
    {
        for (int i = 0; i < count; i++)
        {
            do
            {
                pos++;
            } while (pos < end && buf[pos] != ch);
            if (pos >= end)
            {
                return false;
            }
        }
        *out = (kind == 't') ? pos - 1 : pos;
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            do
            {
                if (pos == start)
                {
                    return false;
                }
                pos--;
            } while (pos > start && buf[pos] != ch);
            if (buf[pos] != ch)
            {
                return false;
            }
        }
        *out = (kind == 'T') ? pos + 1 : pos;
    }
    return true;
}

// Resolve `key` as a motion. `find_ch` is the target of an f/F/t/T, and `mark`
// the one mark for `` ` `` and `'`, SIZE_MAX when none has been set.
// Returns false when the key is not a motion, or when the motion fails.
static bool vi_motion(const char *buf, size_t len, size_t cursor,
                      int key, char find_ch, size_t mark, int count, ViMotion *m)
{
    m->linewise = false;
    m->inclusive = false;
    m->pos = cursor;

    switch (key)
    {
        case 'h':
        case KEY_LEFT:
        case KEY_BACKSPACE:
        {
            size_t start = line_start_of(buf, cursor);
            m->pos = (cursor - start < (size_t)count) ? start : cursor - count;
            return true;
        }

        case 'l':
        case ' ':
        case KEY_RIGHT:
        {
            size_t end = line_end_of(buf, len, cursor);
            m->pos = (cursor + count > end) ? end : cursor + count;
            return true;
        }

        case 'j':
        case KEY_DOWN:
        case KEY_ENTER:
        case KEY_RETURN:
        case '+':
            m->pos = move_lines(buf, len, cursor, count);
            m->linewise = true;
            return true;

        case 'k':
        case KEY_UP:
        case '-':
            m->pos = move_lines(buf, len, cursor, -count);
            m->linewise = true;
            return true;

        case 0x06:  // Ctrl+F -- a page down. Outside vi mode this is search;
        case KEY_PAGE_DOWN:  // vi mode owns its keys and `/` searches instead.
            m->pos = move_lines(buf, len, cursor, count * EDITOR_VI_PAGE_LINES);
            m->linewise = true;
            return true;

        case 0x02:  // Ctrl+B -- a page up (Ctrl+B is block select outside vi mode)
        case KEY_PAGE_UP:
            m->pos = move_lines(buf, len, cursor, -count * EDITOR_VI_PAGE_LINES);
            m->linewise = true;
            return true;

        case 0x04:  // Ctrl+D -- half a page down
            m->pos = move_lines(buf, len, cursor, count * (EDITOR_VI_PAGE_LINES / 2));
            m->linewise = true;
            return true;

        case 0x15:  // Ctrl+U -- half a page up
            m->pos = move_lines(buf, len, cursor, -count * (EDITOR_VI_PAGE_LINES / 2));
            m->linewise = true;
            return true;

        case '0':
        case KEY_HOME:
            m->pos = line_start_of(buf, cursor);
            return true;

        case '^':
            m->pos = first_non_blank(buf, len, cursor);
            return true;

        case '$':
        case KEY_END:
            // Exclusive with the end of the line as its target, so `d$` takes
            // the rest of the line and stops short of the newline
            m->pos = line_end_of(buf, len, move_lines(buf, len, cursor, count - 1));
            return true;

        case 'w':
        case 'W':
            for (int i = 0; i < count; i++)
            {
                m->pos = word_fwd(buf, len, m->pos, key == 'W');
            }
            return true;

        case 'b':
        case 'B':
            for (int i = 0; i < count; i++)
            {
                m->pos = word_back(buf, m->pos, key == 'B');
            }
            return true;

        case 'e':
        case 'E':
            for (int i = 0; i < count; i++)
            {
                m->pos = word_end(buf, len, m->pos, key == 'E');
            }
            m->inclusive = true;
            return true;

        case 'G':
            m->pos = first_non_blank(buf, len,
                                     count > 0 ? goto_line(buf, len, count)
                                               : line_start_of(buf, len));
            m->linewise = true;
            return true;

        case '{':
            for (int i = 0; i < count; i++)
            {
                m->pos = para_back(buf, len, m->pos);
            }
            return true;

        case '}':
            for (int i = 0; i < count; i++)
            {
                m->pos = para_fwd(buf, len, m->pos);
            }
            return true;

        case 'f':
        case 'F':
        case 't':
        case 'T':
            if (!find_char(buf, len, cursor, (char)key, find_ch, count, &m->pos))
            {
                return false;
            }
            m->inclusive = (key == 'f' || key == 't');
            return true;

        case '`':
        case '\'':
            // `'` is linewise and lands on the first non-blank, as vi has it;
            // `` ` `` is the exact byte. Both are exclusive.
            if (mark == SIZE_MAX)
            {
                return false;
            }
            m->pos = mark > len ? len : mark;
            if (key == '\'')
            {
                m->pos = first_non_blank(buf, len, m->pos);
                m->linewise = true;
            }
            return true;

        case '%':
            if (!match_bracket(buf, len, cursor, &m->pos))
            {
                return false;
            }
            m->inclusive = true;
            return true;

        default:
            return false;
    }
}

//
//  Pending state
//

static void clear_pending(ViState *st)
{
    st->count = 0;
    st->op_count = 0;
    st->pending_op = 0;
    st->pending_prefix = 0;
    st->stroke_len = 0;
}

// The count a command runs with. `2d3w` is `d6w`, as vi has it.
static int take_count(ViState *st)
{
    int n = (st->op_count > 0 ? st->op_count : 1) * (st->count > 0 ? st->count : 1);
    st->count = 0;
    st->op_count = 0;
    return n;
}

static void stroke_push(ViState *st, int key)
{
    if (!st->replaying && st->stroke_len < LOGO_VI_REPEAT_MAX)
    {
        st->stroke[st->stroke_len++] = (char)key;
    }
}

static bool is_change(ViActionKind kind)
{
    switch (kind)
    {
        case VI_ACT_DELETE:
        case VI_ACT_PASTE_AFTER:
        case VI_ACT_PASTE_BEFORE:
        case VI_ACT_PASTE_OVER:
        case VI_ACT_INDENT:
        case VI_ACT_REPLACE_CHAR:
        case VI_ACT_JOIN:
        case VI_ACT_TOGGLE_CASE:
        case VI_ACT_INCREMENT:
            return true;
        default:
            // VI_ACT_CHANGE and the two opens are changes too, but they are
            // only half of one until the `Esc` that closes the insert session
            // they open, so `record_insert` records those there instead
            // (docs/vi-mode-design.md §20).
            return false;
    }
}

// Finish a command: remember it for `.` when it changed something, and drop
// whatever was pending either way.
static bool commit(ViState *st, ViAction *out, int count)
{
    if (st->mode == VI_INSERT && !st->replaying && !st->from_visual)
    {
        // A change that ends in insert mode waits for its `Esc`. Its keys stay
        // in `stroke` -- nothing else uses them while insert mode runs, since
        // the editor handles those keys itself -- and its count with them.
        int keys = st->stroke_len;
        clear_pending(st);
        st->stroke_len = keys;
        st->pending_count = count;
        return true;
    }
    if (is_change(out->kind) && !st->replaying && !st->from_visual && st->stroke_len > 0)
    {
        memcpy(st->repeat_keys, st->stroke, (size_t)st->stroke_len);
        st->repeat_len = st->stroke_len;
        st->repeat_count = count;
        st->repeat_insert_set = false;
    }
    clear_pending(st);
    return true;
}

// `Esc` closes an insert session: record the whole change for `.` (§20). What
// was typed is the span between where the editor put the cursor when insert
// began and where it is now, so a backspace inside the session simply leaves
// less text and nothing has to watch keys the state machine never sees. A
// session that moved somewhere else -- an arrow, a backspace past the origin,
// anything that deleted text that was already there -- has no such span, and
// so does a session too long to record. Either drops the record rather than
// leaving `.` to put back a change the user did not make.
static void record_insert(ViState *st, const char *buf, size_t len, size_t cursor)
{
    if (!st->insert_recording)
    {
        return;
    }
    st->insert_recording = false;

    size_t typed = (cursor >= st->insert_origin) ? cursor - st->insert_origin : 0;
    if (cursor < st->insert_origin || len < st->insert_len0 ||
        len - st->insert_len0 != typed || typed > LOGO_VI_INSERT_MAX ||
        st->stroke_len == 0)
    {
        st->repeat_len = 0;
        st->repeat_insert_set = false;
        st->stroke_len = 0;
        return;
    }

    memcpy(st->repeat_insert, buf + st->insert_origin, typed);
    st->repeat_insert_len = (int)typed;
    st->repeat_insert_set = true;
    memcpy(st->repeat_keys, st->stroke, (size_t)st->stroke_len);
    st->repeat_len = st->stroke_len;
    st->repeat_count = st->pending_count;
    st->stroke_len = 0;
}

// The one mark. A jump records where it left from, so `` ` `` comes back --
// and since jumping to the mark is itself a jump, the two places toggle.
static void set_mark(ViState *st, size_t cursor)
{
    st->mark = cursor;
    st->mark_set = true;
}

static size_t vi_mark(const ViState *st)
{
    return st->mark_set ? st->mark : SIZE_MAX;
}

static bool beep(ViState *st, ViAction *out, const char *msg)
{
    clear_pending(st);
    out->kind = VI_ACT_BEEP;
    out->msg = msg;
    return true;
}

// Say something that is not a complaint. The text is already in `st->msg`,
// which outlives the keystroke; `out->msg` only points at it.
static bool message(ViState *st, ViAction *out)
{
    clear_pending(st);
    out->kind = VI_ACT_MESSAGE;
    out->msg = st->msg;
    return true;
}

// `Ctrl` `G` -- where the cursor is. Vi's report without the file name: there
// is none to give, since which procedure or file the editor is over was fixed
// by the primitive that opened it, and the footer is 40 columns wide.
static bool report_position(ViState *st, const char *buf, size_t len, size_t cursor,
                            ViAction *out)
{
    int line = line_number_of(buf, cursor);
    int total = line_number_of(buf, len);
    snprintf(st->msg, sizeof(st->msg), "%sline %d of %d --%d%%--",
             st->modified ? "[Modified] " : "", line, total, line * 100 / total);
    return message(st, out);
}

// `:=` and `:.=` -- a line number on its own, as ex prints it
static bool report_line_number(ViState *st, int line, ViAction *out)
{
    snprintf(st->msg, sizeof(st->msg), "%d", line);
    return message(st, out);
}

// `*` and `#` -- search for the word the cursor is on. The pattern is built
// rather than typed, which is the whole point: `\<` and `\>` (§16) make it the
// whole word, so `*` on `n` walks every `n` and stops on no `then`. Word
// characters are never pattern metacharacters, so nothing needs escaping.
//
// The search runs from the start of the word, not from the cursor, so `*` from
// the middle of one still finds the next occurrence and `#` still finds the
// previous -- which is why VI_ACT_SEARCH carries an origin at all.
static bool search_word(ViState *st, const char *buf, size_t len, size_t cursor,
                        bool forward, ViAction *out)
{
    size_t s, e;
    if (!word_at(buf, len, cursor, &s, &e))
    {
        return beep(st, out, "E348: no word under the cursor");
    }
    if (e - s + 4 > LOGO_VI_TEXT_MAX)
    {
        return beep(st, out, "E486: word too long to search for");
    }

    size_t n = 0;
    st->pattern[n++] = '\\';
    st->pattern[n++] = '<';
    memcpy(st->pattern + n, buf + s, e - s);
    n += e - s;
    st->pattern[n++] = '\\';
    st->pattern[n++] = '>';
    st->pattern[n] = '\0';
    st->pattern_len = n;
    st->search_forward = forward;

    set_mark(st, cursor);
    out->kind = VI_ACT_SEARCH;
    out->ch = forward ? '/' : '?';
    out->start = out->end = s;
    return commit(st, out, 1);
}

//
//  Operators
//

// Turn the cursor and a motion into the range an operator works over.
static void operator_range(const char *buf, size_t len, size_t cursor,
                           char op, const ViMotion *m, ViAction *out)
{
    size_t lo = cursor < m->pos ? cursor : m->pos;
    size_t hi = cursor < m->pos ? m->pos : cursor;

    if (m->linewise)
    {
        lo = line_start_of(buf, lo);
        if (op == 'c')
        {
            // `cc` empties the line and leaves it there to be typed into
            hi = line_end_of(buf, len, hi);
        }
        else
        {
            hi = next_line_start(buf, len, hi);
            // On the last line there is no newline to take, so take the one
            // before it -- otherwise `dd` leaves a blank line behind
            if (op == 'd' && hi >= len && lo > 0 && buf[lo - 1] == '\n')
            {
                lo--;
            }
        }
        out->linewise = true;
    }
    else
    {
        if (m->inclusive && hi < len)
        {
            hi++;
        }
        out->linewise = false;
    }

    out->start = lo;
    out->end = hi;
}

static ViActionKind op_kind(char op)
{
    switch (op)
    {
        case 'd': return VI_ACT_DELETE;
        case 'c': return VI_ACT_CHANGE;
        case 'y': return VI_ACT_YANK;
        default:  return VI_ACT_INDENT;  // '<' and '>'
    }
}

// Apply `op` over `key` as a motion. Returns false when the key is not a
// motion the operator can use.
static bool apply_operator(const char *buf, size_t len, size_t cursor,
                           char op, int key, char find_ch, size_t mark, int count,
                           ViAction *out)
{
    ViMotion m;

    if (key == op || (op == 'd' && key == 'D'))
    {
        // The doubled form -- `dd`, `yy`, `>>` -- is `count` whole lines
        m.pos = move_lines(buf, len, cursor, count - 1);
        m.linewise = true;
        m.inclusive = false;
    }
    else if (op == 'c' && (key == 'w' || key == 'W'))
    {
        // `cw` on a word changes to the end of it, not to the start of the
        // next one: vi's one deliberate inconsistency, and the one everybody
        // relies on
        if (cursor < len && char_class(buf[cursor]) != CLASS_BLANK)
        {
            m.pos = cursor;
            for (int i = 0; i < count; i++)
            {
                m.pos = word_end(buf, len, i == 0 ? (m.pos > 0 ? m.pos - 1 : 0) : m.pos,
                                 key == 'W');
            }
            m.linewise = false;
            m.inclusive = true;
        }
        else if (!vi_motion(buf, len, cursor, key, find_ch, mark, count, &m))
        {
            return false;
        }
    }
    else if (!vi_motion(buf, len, cursor, key, find_ch, mark, count, &m))
    {
        return false;
    }
    else if ((key == 'w' || key == 'W') && m.pos > line_end_of(buf, len, cursor) &&
             cursor < line_end_of(buf, len, cursor))
    {
        // `dw` on the last word of a line stops at the end of it rather than
        // pulling the next line up
        m.pos = line_end_of(buf, len, cursor);
    }

    operator_range(buf, len, cursor, op, &m, out);
    out->kind = op_kind(op);
    out->count = (op == '>') ? 1 : (op == '<') ? -1 : count;
    return true;
}

//
//  Visual mode
//

static void visual_range(const ViState *st, const char *buf, size_t len, size_t cursor,
                         ViAction *out)
{
    size_t lo = st->anchor < cursor ? st->anchor : cursor;
    size_t hi = st->anchor < cursor ? cursor : st->anchor;

    if (st->mode == VI_VISUAL_LINE)
    {
        out->start = line_start_of(buf, lo);
        out->end = next_line_start(buf, len, hi);
        out->linewise = true;
    }
    else
    {
        // Charwise visual takes the character under the cursor, but never the
        // newline: a selection that swallowed it would join two lines
        out->start = lo;
        out->end = (hi < len && buf[hi] != '\n') ? hi + 1 : hi;
        out->linewise = false;
    }
}

//
//  The ex command line
//

// Skip the spaces vi tolerates around a command
static size_t skip_spaces(const char *s, size_t i, size_t n)
{
    while (i < n && s[i] == ' ')
    {
        i++;
    }
    return i;
}

static void copy_field(char *dst, size_t *dst_len, const char *src, size_t n)
{
    if (n > LOGO_VI_TEXT_MAX)
    {
        n = LOGO_VI_TEXT_MAX;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
    *dst_len = n;
}

// Parse `s/pat/rep/flags`, starting at the `s`. The delimiter is whatever
// follows it, as long as it is not a letter or a digit.
//
// The pattern goes into `st->pattern`, which is also the last one searched for
// -- except when `out_pat` is given, and then it is handed back as a slice of
// `s` and `st->pattern` is left alone. That is what a `:g` needs: the pattern
// that finds the lines is already in there, and a nested `s` with one of its
// own must not write over it while the pass is still using it (§23.5).
static bool parse_substitute(ViState *st, const char *s, size_t i, size_t n,
                             const char **out_pat, size_t *out_pat_len)
{
    if (i >= n || s[i] != 's')
    {
        return false;
    }
    i++;
    if (i >= n)
    {
        return false;
    }
    char delim = s[i];
    if (char_class(delim) != CLASS_PUNCT)
    {
        return false;
    }
    i++;

    size_t pat = i;
    while (i < n && s[i] != delim)
    {
        i++;
    }
    if (i >= n)
    {
        return false;  // Unterminated
    }
    if (i == pat)
    {
        // An empty pattern reuses the last `/` or `:s` -- both fill st->pattern,
        // and now they share a dialect, so `:%s//count/g` after `/\<n\>` renames
        // exactly what the search walked (§16.5). Nothing to reuse is a refusal.
        if (st->pattern_len == 0)
        {
            return false;
        }
        if (out_pat != NULL)
        {
            // Under a `:g`, that is the pattern which found the line
            *out_pat = st->pattern;
            *out_pat_len = st->pattern_len;
        }
    }
    else
    {
        if (!editor_pattern_valid(s + pat, i - pat))
        {
            return false;  // run_ex turns this into "E486: bad substitute"
        }
        if (out_pat != NULL)
        {
            *out_pat = s + pat;
            *out_pat_len = i - pat;
        }
        else
        {
            copy_field(st->pattern, &st->pattern_len, s + pat, i - pat);
        }
    }
    i++;

    size_t rep = i;
    while (i < n && s[i] != delim)
    {
        i++;
    }
    copy_field(st->replacement, &st->replacement_len, s + rep, i - rep);
    if (i < n)
    {
        i++;  // The closing delimiter
    }

    st->sub_global = false;
    for (; i < n; i++)
    {
        if (s[i] == 'g')
        {
            st->sub_global = true;
        }
        else if (s[i] != ' ')
        {
            return false;
        }
    }
    return true;
}

//
//  Ex addresses (§19)
//

// One address: a line number, `.`, `$`, `'<` or `'>`, followed by any number of
// `+n` / `-n` offsets -- a bare `+` or `-` counts as one line, and one on its
// own is measured from the cursor's line. `*i` is advanced past what was read,
// spaces and all.
//
// Returns false when there is no address here, which is not an error: a command
// may simply have no range. `*err` is set when there was one and it was
// malformed, and then the caller complains rather than reading on.
static bool parse_address(const ViState *st, const char *buf, size_t len,
                          size_t cursor, const char *s, size_t *i, size_t n,
                          int *out_line, const char **err)
{
    size_t k = skip_spaces(s, *i, n);
    int line;

    if (k < n && s[k] >= '0' && s[k] <= '9')
    {
        line = 0;
        while (k < n && s[k] >= '0' && s[k] <= '9')
        {
            if (line < 1000000)
            {
                line = line * 10 + (s[k] - '0');   // Clamped below either way
            }
            k++;
        }
    }
    else if (k < n && s[k] == '.')
    {
        line = line_number_of(buf, cursor);
        k++;
    }
    else if (k < n && s[k] == '$')
    {
        line = line_number_of(buf, len);
        k++;
    }
    else if (k + 1 < n && s[k] == '\'' && (s[k + 1] == '<' || s[k + 1] == '>'))
    {
        if (!st->vis_set)
        {
            *err = "E20: no selection";
            return false;
        }
        // The selection is held as byte offsets and the buffer may have been
        // rewritten since, so clamp before counting the lines to it
        size_t at = (s[k + 1] == '<') ? st->vis_start : st->vis_end;
        line = line_number_of(buf, at < len ? at : len);
        k += 2;
    }
    else if (k < n && s[k] == '\'')
    {
        // The one mark (§18.2) is a place in a line, not a line, and `'` on its
        // own is already the motion that jumps to it
        *err = "E16: invalid range";
        return false;
    }
    else if (k < n && (s[k] == '+' || s[k] == '-'))
    {
        line = line_number_of(buf, cursor);   // What the offset is measured from
    }
    else
    {
        return false;
    }

    while (k < n && (s[k] == '+' || s[k] == '-'))
    {
        int sign = (s[k] == '+') ? 1 : -1;
        int delta = 0;
        bool digits = false;
        k++;
        while (k < n && s[k] >= '0' && s[k] <= '9')
        {
            if (delta < 1000000)
            {
                delta = delta * 10 + (s[k] - '0');
            }
            k++;
            digits = true;
        }
        line += sign * (digits ? delta : 1);
    }

    *i = skip_spaces(s, k, n);
    *out_line = line;
    return true;
}

static bool run_ex(ViState *st, const char *buf, size_t len, size_t cursor, ViAction *out)
{
    const char *s = st->cmdline + 1;  // Past the ':'
    size_t n = st->cmdline_len - 1;
    while (n > 0 && s[n - 1] == ' ')
    {
        n--;
    }
    size_t i = skip_spaces(s, 0, n);

    st->mode = VI_NORMAL;

    if (i >= n)
    {
        out->kind = VI_ACT_REDRAW;
        return true;
    }

    // The range, if one was typed: `%`, one address, or two
    int last = line_number_of(buf, len);
    int lo = 0, hi = 0;
    bool have_range = false;
    const char *err = NULL;

    if (s[i] == '%')
    {
        lo = 1;
        hi = last;
        have_range = true;
        i = skip_spaces(s, i + 1, n);
    }
    else if (parse_address(st, buf, len, cursor, s, &i, n, &lo, &err))
    {
        hi = lo;
        have_range = true;
        if (i < n && s[i] == ',')
        {
            i = skip_spaces(s, i + 1, n);
            if (!parse_address(st, buf, len, cursor, s, &i, n, &hi, &err))
            {
                // `:1,s/a/b/` -- an omitted address is the cursor's line, as ex
                // has it, so only a malformed one is a complaint
                hi = line_number_of(buf, cursor);
            }
        }
    }
    if (err != NULL)
    {
        return beep(st, out, err);
    }

    if (lo < 1) lo = 1;
    if (hi < 1) hi = 1;
    if (lo > last) lo = last;
    if (hi > last) hi = last;
    if (lo > hi)
    {
        // Vi offers to swap the two; on a 40-column footer, saying so is better
        // than asking a question the command line has no room to answer
        return beep(st, out, "E493: backwards range");
    }

    // A range on its own moves to its last line -- which is what `:{n}` is
    if (i >= n)
    {
        set_mark(st, cursor);
        out->kind = VI_ACT_MOVE;
        out->start = out->end = first_non_blank(buf, len, goto_line(buf, len, hi));
        return true;
    }

    // `:=` prints the number of the last line, `:.=` of the line the cursor is
    // on, `:'<,'>=` of the last line selected. It goes before the substitute:
    // `=` is not a substitute's delimiter.
    if (n - i == 1 && s[i] == '=')
    {
        return report_line_number(st, have_range ? hi : last, out);
    }

    // Substitute, over the range or -- with none -- the current line
    if (s[i] == 's')
    {
        if (!parse_substitute(st, s, i, n, NULL, NULL))
        {
            return beep(st, out, "E486: bad substitute");
        }
        out->kind = VI_ACT_SUBSTITUTE;
        out->start = have_range ? goto_line(buf, len, lo) : line_start_of(buf, cursor);
        out->end = next_line_start(buf, len,
                                   have_range ? goto_line(buf, len, hi) : cursor);
        return true;
    }

    // `:g` and `:v` run a command on the lines a pattern picks out, which is
    // the job `:s` structurally cannot do: it conditions on the text it is
    // replacing and never on the line (§23). The range defaults to the whole
    // buffer rather than to the cursor's line, unlike every other command in
    // this parser -- that is ex's rule, and it is the useful one, since a `:g`
    // over a single line is a `:s` with extra steps.
    if (s[i] == 'g' || s[i] == 'v')
    {
        bool invert = (s[i] == 'v');
        i++;
        if (!invert && i < n && s[i] == '!')
        {
            invert = true;   // `:g!` is how ex spells `:v`
            i++;
        }
        if (i >= n || char_class(s[i]) != CLASS_PUNCT)
        {
            return beep(st, out, "E486: bad pattern");
        }
        char delim = s[i++];
        size_t pat = i;
        while (i < n && s[i] != delim)
        {
            i++;
        }
        if (i >= n)
        {
            return beep(st, out, "E486: bad pattern");   // Unterminated
        }
        if (i == pat)
        {
            // An empty pattern is the last one searched for, exactly as `:s`
            // has it -- and leaving it there is what lets a nested `s` with no
            // pattern of its own mean "the pattern that found the line"
            if (st->pattern_len == 0)
            {
                return beep(st, out, "E35: no previous search");
            }
        }
        else
        {
            if (!editor_pattern_valid(s + pat, i - pat))
            {
                return beep(st, out, "E486: bad pattern");
            }
            copy_field(st->pattern, &st->pattern_len, s + pat, i - pat);
        }
        i = skip_spaces(s, i + 1, n);

        // `d` and `s`, and nothing else: they are the two commands whose result
        // does not depend on the direction the walk runs in, and the walk runs
        // backwards so that it needs no mark set (§23.2, §23.3)
        out->sub_pattern = NULL;
        out->sub_pattern_len = 0;
        if (i < n && s[i] == 'd' && skip_spaces(s, i + 1, n) >= n)
        {
            out->ch = 'd';
        }
        else if (i < n && s[i] == 's')
        {
            if (!parse_substitute(st, s, i, n,
                                  &out->sub_pattern, &out->sub_pattern_len))
            {
                return beep(st, out, "E486: bad substitute");
            }
            out->ch = 's';
        }
        else
        {
            return beep(st, out, "E492: not an editor command");
        }

        out->kind = VI_ACT_GLOBAL;
        out->invert = invert;
        out->linewise = true;
        out->start = have_range ? goto_line(buf, len, lo) : 0;
        out->end = have_range
            ? next_line_start(buf, len, goto_line(buf, len, hi))
            : len;
        return true;
    }

    // `:m` and `:t` (`:co`) take a destination address after the command rather
    // than a range in front of it, and are the only commands that do (§22)
    if (s[i] == 'm' || s[i] == 't' ||
        (s[i] == 'c' && i + 1 < n && s[i + 1] == 'o'))
    {
        bool copy = (s[i] != 'm');
        i = skip_spaces(s, i + ((s[i] == 'c') ? 2 : 1), n);

        int dest = 0;
        if (!parse_address(st, buf, len, cursor, s, &i, n, &dest, &err) || i < n)
        {
            return beep(st, out, err != NULL ? err : "E14: invalid address");
        }
        // The one place a line 0 means something: text goes *after* the
        // destination, so "above the first line" has no other spelling
        if (dest < 0) dest = 0;
        if (dest > last) dest = last;

        out->start = have_range ? goto_line(buf, len, lo) : line_start_of(buf, cursor);
        out->end = next_line_start(buf, len,
                                   have_range ? goto_line(buf, len, hi) : cursor);
        out->dest = (dest == 0) ? 0 : next_line_start(buf, len,
                                                      goto_line(buf, len, dest));
        if (out->end <= out->start)
        {
            // The empty line past the buffer's last newline, which `G` lands on
            // and which has no text to take anywhere
            return beep(st, out, "E16: invalid range");
        }
        if (!copy && out->dest >= out->start && out->dest <= out->end)
        {
            // Inside its own range, or either side of it, where the move would
            // change nothing and the footer is the only thing that can say so
            return beep(st, out, "E134: cannot move into itself");
        }
        out->kind = VI_ACT_MOVE_LINES;
        out->ch = copy ? 't' : 'm';
        return true;
    }

    if (have_range)
    {
        return beep(st, out, "E481: no range allowed");
    }

    size_t cmd_len = n - i;
    const char *cmd = s + i;

    // `:w` writes and stays; every other write form also leaves
    if (cmd_len == 1 && cmd[0] == 'w')
    {
        out->kind = VI_ACT_WRITE;
        return true;
    }
    if ((cmd_len == 1 && cmd[0] == 'x') ||
        (cmd_len == 2 && cmd[0] == 'w' && cmd[1] == 'q') ||
        (cmd_len == 2 && cmd[0] == 'x' && cmd[1] == '!') ||
        (cmd_len == 3 && cmd[0] == 'w' && cmd[1] == 'q' && cmd[2] == '!'))
    {
        out->kind = VI_ACT_ACCEPT;
        return true;
    }
    if (cmd_len == 2 && cmd[0] == 'q' && cmd[1] == '!')
    {
        out->kind = VI_ACT_CANCEL;
        return true;
    }
    // `:q` never writes: it throws the buffer away, and so refuses while there
    // is anything to throw away
    if (cmd_len == 1 && cmd[0] == 'q')
    {
        if (st->modified)
        {
            return beep(st, out, "E37: no write since last change");
        }
        out->kind = VI_ACT_CANCEL;
        return true;
    }

    return beep(st, out, "E492: not an editor command");
}

static bool cmdline_key(ViState *st, const char *buf, size_t len, size_t cursor,
                        int key, ViAction *out)
{
    switch (key)
    {
        case KEY_BREAK:
            return false;  // Brk cancels the editor from every mode

        case KEY_ESC:
            st->mode = VI_NORMAL;
            st->cmdline_len = 0;
            out->kind = VI_ACT_REDRAW;
            return true;

        case KEY_BACKSPACE:
            if (st->cmdline_len > 1)
            {
                st->cmdline[--st->cmdline_len] = '\0';
                out->kind = VI_ACT_REDRAW;
            }
            else
            {
                // Rubbing out the introducer leaves the command line
                st->mode = VI_NORMAL;
                st->cmdline_len = 0;
                out->kind = VI_ACT_REDRAW;
            }
            return true;

        case KEY_ENTER:
        case KEY_RETURN:
            if (st->cmdline[0] == ':')
            {
                return run_ex(st, buf, len, cursor, out);
            }
            // A search: an empty pattern repeats the last one
            st->mode = VI_NORMAL;
            st->search_forward = (st->cmdline[0] == '/');
            if (st->cmdline_len > 1)
            {
                // Validation is only needed here, on the Return: vi's search is
                // not incremental, so a half-typed pattern is never matched
                // (§16.5).
                if (!editor_pattern_valid(st->cmdline + 1, st->cmdline_len - 1))
                {
                    return beep(st, out, "E486: bad pattern");
                }
                copy_field(st->pattern, &st->pattern_len,
                           st->cmdline + 1, st->cmdline_len - 1);
            }
            if (st->pattern_len == 0)
            {
                return beep(st, out, "E35: no previous search");
            }
            set_mark(st, cursor);
            out->kind = VI_ACT_SEARCH;
            out->ch = st->search_forward ? '/' : '?';
            return true;

        default:
            if (key >= 0x20 && key <= 0x7E && st->cmdline_len < LOGO_VI_CMDLINE_MAX)
            {
                st->cmdline[st->cmdline_len++] = (char)key;
                st->cmdline[st->cmdline_len] = '\0';
                out->kind = VI_ACT_REDRAW;
            }
            return true;
    }
}

static bool enter_cmdline(ViState *st, int key, ViAction *out)
{
    bool visual = (st->mode == VI_VISUAL || st->mode == VI_VISUAL_LINE);
    clear_pending(st);
    st->mode = VI_CMDLINE;
    st->cmdline[0] = (char)key;
    st->cmdline_len = 1;
    if (key == ':' && visual)
    {
        // A `:` typed over a selection is all but always about the lines it
        // covers, so vi types the range in for you. Backspace rubs it out again
        memcpy(st->cmdline + 1, "'<,'>", 5);
        st->cmdline_len = 6;
    }
    st->cmdline[st->cmdline_len] = '\0';
    out->kind = VI_ACT_REDRAW;
    return true;
}

//
//  Normal and visual mode
//

static bool normal_key(ViState *st, const char *buf, size_t len, size_t cursor,
                       int key, ViAction *out);

// `.` replays the recorded keys. Only the last key of a command produces an
// action and none of the earlier ones touch the buffer, so feeding the whole
// sequence through and returning what the final key gives back is enough.
static bool replay(ViState *st, const char *buf, size_t len, size_t cursor, ViAction *out)
{
    if (st->repeat_len == 0)
    {
        return beep(st, out, "Nothing to repeat");
    }

    int count = st->count > 0 ? st->count : st->repeat_count;
    char keys[LOGO_VI_REPEAT_MAX];
    int n = st->repeat_len;
    memcpy(keys, st->repeat_keys, (size_t)n);

    clear_pending(st);
    st->replaying = true;
    st->count = count;

    bool consumed = false;
    for (int i = 0; i < n; i++)
    {
        consumed = normal_key(st, buf, len, cursor, keys[i], out);
    }

    st->replaying = false;

    if (st->mode == VI_INSERT)
    {
        // The recorded change ends in an insert session. Close it here and
        // hand the text back with the action: the editor types it once it has
        // carried the action out, so the repeat puts back the whole change
        // rather than leaving the user in insert mode (§20).
        st->mode = VI_NORMAL;
        if (st->repeat_insert_set)
        {
            out->insert = st->repeat_insert;
            out->insert_len = (size_t)st->repeat_insert_len;
        }
    }
    return consumed;
}

// The second key of a two-key command
static bool prefixed_key(ViState *st, const char *buf, size_t len, size_t cursor,
                         int key, ViAction *out)
{
    char prefix = st->pending_prefix;
    st->pending_prefix = 0;

    switch (prefix)
    {
        case 'Z':
            if (key == 'Z')
            {
                out->kind = VI_ACT_ACCEPT;
                return commit(st, out, 0);
            }
            if (key == 'Q')
            {
                out->kind = VI_ACT_CANCEL;
                return commit(st, out, 0);
            }
            return beep(st, out, "E492: not an editor command");

        case 'r':
        {
            // Return is a character like any other to `r`: it puts a line
            // break where the characters were, which splits the line
            bool split = (key == KEY_RETURN || key == KEY_ENTER);
            if (!split && (key < 0x20 || key > 0x7E))
            {
                return beep(st, out, "Nothing to replace");
            }
            int count = take_count(st);
            size_t end = line_end_of(buf, len, cursor);
            if (cursor + (size_t)count > end)
            {
                return beep(st, out, "Nothing to replace");
            }
            out->kind = VI_ACT_REPLACE_CHAR;
            out->start = cursor;
            out->end = cursor + count;
            out->ch = split ? '\n' : (char)key;
            out->count = count;
            return commit(st, out, count);
        }

        case 'i':
        case 'a':
        {
            bool visual = (st->mode == VI_VISUAL || st->mode == VI_VISUAL_LINE);
            char op = st->pending_op;
            int count = take_count(st);
            size_t start, end;
            bool linewise;

            if (!text_object(buf, len, cursor, key, prefix == 'a', count,
                             &start, &end, &linewise))
            {
                return beep(st, out, key == 'p' ? "Not inside a procedure"
                                                : "E492: not an editor command");
            }

            if (visual)
            {
                // The selection is the anchor and the cursor, and editor.c
                // copies the anchor back out after every action (§6.2), so an
                // object in visual mode is a move with the anchor moved too
                st->anchor = start;
                if (linewise)
                {
                    st->mode = VI_VISUAL_LINE;
                }
                out->kind = VI_ACT_MOVE;
                out->start = out->end = (end > start) ? end - 1 : start;
                return commit(st, out, count);
            }

            out->kind = op_kind(op);
            out->start = start;
            out->end = end;
            out->linewise = linewise;
            out->count = (op == '>') ? 1 : (op == '<') ? -1 : count;
            if (linewise && end > start)
            {
                // The two linewise adjustments the visual path makes, and for
                // the same reasons: `cip` leaves the emptied line to type into
                // the way `cc` does, and a `dap` that reaches the end of the
                // buffer takes the newline before it rather than leaving a
                // blank line behind
                if (op == 'c')
                {
                    out->end = line_end_of(buf, len, end - 1);
                }
                else if (op == 'd' && end >= len && start > 0 && buf[start - 1] == '\n')
                {
                    out->start--;
                }
            }
            if (op == 'c')
            {
                st->mode = VI_INSERT;
            }
            return commit(st, out, count);
        }

        case 'z':
            // The view moves, the cursor does not. editor.c does the row
            // arithmetic: this file has never known where the view is (§6.1).
            if (key != 'z' && key != 't' && key != 'b')
            {
                return beep(st, out, "E492: not an editor command");
            }
            out->kind = VI_ACT_SCROLL;
            out->ch = (char)key;
            return commit(st, out, 0);

        case ']':
        case '[':
        {
            // `]]` and `[[` step definition to definition. They are motions, so
            // `d]]` and `y]]` come free, and jumps, so `` ` `` comes back (§24.2)
            if (key != prefix)
            {
                return beep(st, out, "E492: not an editor command");
            }
            char op = st->pending_op;
            int count = take_count(st);
            ViMotion m = { .pos = cursor, .linewise = false, .inclusive = false };
            for (int i = 0; i < count; i++)
            {
                m.pos = (prefix == ']') ? def_fwd(buf, len, m.pos)
                                        : def_back(buf, len, m.pos);
            }
            if (op != 0)
            {
                operator_range(buf, len, cursor, op, &m, out);
                out->kind = op_kind(op);
                out->count = (op == '>') ? 1 : (op == '<') ? -1 : count;
                if (op == 'c')
                {
                    st->mode = VI_INSERT;
                }
            }
            else
            {
                set_mark(st, cursor);
                out->kind = VI_ACT_MOVE;
                out->start = out->end = m.pos;
            }
            return commit(st, out, count);
        }

        case 'g':
        {
            if (key == 'd')
            {
                // `gd` -- go to where this procedure is defined. In an `edall`
                // buffer the whole workspace is one file, which is what makes
                // it worth a key here.
                size_t ws, we, pos;
                if (!word_at(buf, len, cursor, &ws, &we))
                {
                    return beep(st, out, "E348: no word under the cursor");
                }
                if (!find_definition(buf, len, buf + ws, we - ws, &pos))
                {
                    return beep(st, out, "E388: definition not found");
                }
                set_mark(st, cursor);
                out->kind = VI_ACT_MOVE;
                out->start = out->end = pos;
                return commit(st, out, take_count(st));
            }
            if (key != 'g')
            {
                return beep(st, out, "E492: not an editor command");
            }
            int count = st->count;  // `gg` with no count is line 1
            char op = st->pending_op;
            int total = take_count(st);
            ViMotion m = {
                .pos = first_non_blank(buf, len, goto_line(buf, len, count > 0 ? count : 1)),
                .linewise = true,
                .inclusive = false,
            };
            if (op != 0)
            {
                operator_range(buf, len, cursor, op, &m, out);
                out->kind = op_kind(op);
                out->count = (op == '>') ? 1 : (op == '<') ? -1 : total;
                if (op == 'c')
                {
                    st->mode = VI_INSERT;
                }
            }
            else
            {
                set_mark(st, cursor);  // `gg` is a jump
                out->kind = VI_ACT_MOVE;
                out->start = out->end = m.pos;
            }
            return commit(st, out, total);
        }

        case 'f':
        case 'F':
        case 't':
        case 'T':
        {
            if (key < 0x20 || key > 0x7E)
            {
                return beep(st, out, "E492: not an editor command");
            }
            st->last_find = prefix;
            st->last_find_char = (char)key;

            char op = st->pending_op;
            int count = take_count(st);
            ViMotion m;
            if (!vi_motion(buf, len, cursor, prefix, (char)key, SIZE_MAX, count, &m))
            {
                return beep(st, out, "E486: pattern not found");
            }
            if (op != 0)
            {
                operator_range(buf, len, cursor, op, &m, out);
                out->kind = op_kind(op);
                out->count = (op == '>') ? 1 : (op == '<') ? -1 : count;
                if (op == 'c')
                {
                    st->mode = VI_INSERT;
                }
            }
            else
            {
                out->kind = VI_ACT_MOVE;
                out->start = out->end = m.pos;
            }
            return commit(st, out, count);
        }

        default:
            return beep(st, out, "E492: not an editor command");
    }
}

static bool normal_key(ViState *st, const char *buf, size_t len, size_t cursor,
                       int key, ViAction *out)
{
    bool visual = (st->mode == VI_VISUAL || st->mode == VI_VISUAL_LINE);

    if (key == KEY_BREAK)
    {
        return false;  // Brk cancels the editor from every mode
    }

    if (key == KEY_ESC)
    {
        bool had_state = visual || st->count > 0 || st->pending_op != 0 ||
                         st->pending_prefix != 0;
        clear_pending(st);
        if (visual)
        {
            st->mode = VI_NORMAL;
        }
        out->kind = had_state ? VI_ACT_REDRAW : VI_ACT_NONE;
        return true;
    }

    if (st->pending_prefix != 0)
    {
        stroke_push(st, key);
        return prefixed_key(st, buf, len, cursor, key, out);
    }

    // A count. `0` is a motion until there is one to extend.
    if (key >= '1' && key <= '9')
    {
        st->count = st->count * 10 + (key - '0');
        out->kind = VI_ACT_NONE;
        return true;
    }
    if (key == '0' && st->count > 0)
    {
        st->count *= 10;
        out->kind = VI_ACT_NONE;
        return true;
    }

    stroke_push(st, key);

    // The keys that need their own second key
    if (key == 'Z' || key == 'r' || key == 'g' || key == 'z' ||
        key == 'f' || key == 'F' || key == 't' || key == 'T' ||
        key == ']' || key == '[')
    {
        if ((key == 'Z' || key == 'r') && visual)
        {
            return beep(st, out, "E492: not an editor command");
        }
        st->pending_prefix = (char)key;
        out->kind = VI_ACT_NONE;
        return true;
    }

    // `i` and `a` are text-object prefixes only where they cannot be insert
    // entry: with an operator waiting for a range, or in visual mode (§15)
    if ((key == 'i' || key == 'a') && (st->pending_op != 0 || visual))
    {
        st->pending_prefix = (char)key;
        out->kind = VI_ACT_NONE;
        return true;
    }

    // Operators. In visual mode they act on the selection at once; in normal
    // mode they wait for a motion, and a second press of the same key is the
    // linewise form.
    if (key == 'd' || key == 'c' || key == 'y' || key == '<' || key == '>' ||
        (visual && (key == 'x' || key == 's')))
    {
        char op = (key == 'x') ? 'd' : (key == 's') ? 'c' : (char)key;

        if (visual)
        {
            int count = take_count(st);
            visual_range(st, buf, len, cursor, out);
            if (op == 'c' && out->linewise)
            {
                out->end = line_end_of(buf, len, out->end > out->start ? out->end - 1 : out->start);
            }
            if (op == 'd' && out->linewise && out->end >= len &&
                out->start > 0 && buf[out->start - 1] == '\n')
            {
                out->start--;
            }
            out->kind = op_kind(op);
            out->count = (op == '>') ? count : (op == '<') ? -count : count;
            st->mode = (op == 'c') ? VI_INSERT : VI_NORMAL;
            return commit(st, out, count);
        }

        if (st->pending_op == 0)
        {
            st->pending_op = op;
            st->op_count = st->count;
            st->count = 0;
            out->kind = VI_ACT_NONE;
            return true;
        }
        // A second operator key -- `dd`, `yy`, `>>` -- is the linewise form,
        // and falls through to the motion path where apply_operator knows it
    }

    // Visual mode's own commands
    if (visual)
    {
        switch (key)
        {
            case 'v':
                st->mode = (st->mode == VI_VISUAL) ? VI_NORMAL : VI_VISUAL;
                out->kind = VI_ACT_REDRAW;
                return commit(st, out, 0);

            case 'V':
                st->mode = (st->mode == VI_VISUAL_LINE) ? VI_NORMAL : VI_VISUAL_LINE;
                out->kind = VI_ACT_REDRAW;
                return commit(st, out, 0);

            case 'o':
                out->kind = VI_ACT_MOVE;
                out->start = out->end = st->anchor;
                st->anchor = cursor;
                return commit(st, out, 0);

            case '~':
            {
                int count = take_count(st);
                visual_range(st, buf, len, cursor, out);
                out->kind = VI_ACT_TOGGLE_CASE;
                out->count = 1;
                st->mode = VI_NORMAL;
                return commit(st, out, count);
            }

            case 'p':
            case 'P':
            {
                int count = take_count(st);
                visual_range(st, buf, len, cursor, out);
                out->kind = VI_ACT_PASTE_OVER;
                out->count = 1;
                st->mode = VI_NORMAL;
                return commit(st, out, count);
            }

            case 'J':
            {
                take_count(st);
                visual_range(st, buf, len, cursor, out);
                int lines = 1;
                for (size_t i = out->start; i < out->end && i < len; i++)
                {
                    if (buf[i] == '\n')
                    {
                        lines++;
                    }
                }
                out->kind = VI_ACT_JOIN;
                out->start = out->end = line_start_of(buf, out->start);
                out->count = lines;
                st->mode = VI_NORMAL;
                return commit(st, out, lines);
            }

            default:
                break;
        }
    }

    if ((key == '`' || key == '\'') && !st->mark_set)
    {
        return beep(st, out, "E20: mark not set");
    }

    // A motion, on its own or completing a pending operator
    {
        int count = (st->op_count > 0 ? st->op_count : 1) * (st->count > 0 ? st->count : 1);
        bool counted = (st->count > 0 || st->op_count > 0);
        char op = st->pending_op;
        char find_ch = st->last_find_char;

        // `;` and `,` re-run the last f/F/t/T, `,` the other way
        int motion_key = key;
        if (key == ';' || key == ',')
        {
            if (st->last_find == 0)
            {
                return beep(st, out, "E492: not an editor command");
            }
            static const char flip[] = "fFtT";
            static const char back[] = "FfTt";
            motion_key = key == ';' ? st->last_find
                                    : back[bracket_index(flip, st->last_find)];
        }

        // `G` with no count goes to the last line
        int motion_count = (motion_key == 'G' && !counted) ? 0 : count;

        // The keys that count as a jump, and so leave the mark behind them.
        // `` ` `` is one itself, which is what makes it a toggle rather than a
        // one-way trip. An operator's motion is not: `d}` is an edit, and the
        // place to come back to is the one before it.
        bool jumps = (motion_key == 'G' || motion_key == '{' || motion_key == '}' ||
                      motion_key == '%' || motion_key == '`' || motion_key == '\'');

        ViMotion m;
        if (op != 0)
        {
            take_count(st);
            if (!apply_operator(buf, len, cursor, op, motion_key, find_ch,
                                vi_mark(st), motion_count, out))
            {
                return beep(st, out, "E492: not an editor command");
            }
            if (op == 'c')
            {
                st->mode = VI_INSERT;
            }
            return commit(st, out, count);
        }
        if (vi_motion(buf, len, cursor, motion_key, find_ch, vi_mark(st), motion_count, &m))
        {
            take_count(st);
            if (jumps)
            {
                set_mark(st, cursor);
            }
            out->kind = VI_ACT_MOVE;
            out->start = out->end = m.pos;
            return commit(st, out, count);
        }
        if (motion_key != key || key == 'f' || key == 'F' || key == 't' || key == 'T')
        {
            return beep(st, out, "E492: not an editor command");  // The find failed
        }
    }

    // Single-key commands
    {
        int count = take_count(st);
        size_t line_start = line_start_of(buf, cursor);
        size_t line_end = line_end_of(buf, len, cursor);

        switch (key)
        {
            case 'x':
                out->kind = VI_ACT_DELETE;
                out->start = cursor;
                out->end = (cursor + count > line_end) ? line_end : cursor + count;
                return commit(st, out, count);

            case 'X':
                out->kind = VI_ACT_DELETE;
                out->start = (cursor - line_start < (size_t)count) ? line_start : cursor - count;
                out->end = cursor;
                return commit(st, out, count);

            case 'D':
                out->kind = VI_ACT_DELETE;
                out->start = cursor;
                out->end = line_end_of(buf, len, move_lines(buf, len, cursor, count - 1));
                return commit(st, out, count);

            case 'C':
                out->kind = VI_ACT_CHANGE;
                out->start = cursor;
                out->end = line_end_of(buf, len, move_lines(buf, len, cursor, count - 1));
                st->mode = VI_INSERT;
                return commit(st, out, count);

            case 'Y':
                out->kind = VI_ACT_YANK;
                out->start = line_start;
                out->end = next_line_start(buf, len, move_lines(buf, len, cursor, count - 1));
                out->linewise = true;
                return commit(st, out, count);

            case 'S':
                out->kind = VI_ACT_CHANGE;
                out->start = first_non_blank(buf, len, cursor);
                out->end = line_end_of(buf, len, move_lines(buf, len, cursor, count - 1));
                out->linewise = true;
                st->mode = VI_INSERT;
                return commit(st, out, count);

            case 's':
                out->kind = VI_ACT_CHANGE;
                out->start = cursor;
                out->end = (cursor + count > line_end) ? line_end : cursor + count;
                st->mode = VI_INSERT;
                return commit(st, out, count);

            case 'p':
                out->kind = VI_ACT_PASTE_AFTER;
                out->start = out->end = cursor;
                out->count = count;
                return commit(st, out, count);

            case 'P':
                out->kind = VI_ACT_PASTE_BEFORE;
                out->start = out->end = cursor;
                out->count = count;
                return commit(st, out, count);

            case 'J':
                out->kind = VI_ACT_JOIN;
                out->start = out->end = cursor;
                out->count = count < 2 ? 2 : count;
                return commit(st, out, count);

            case '~':
                out->kind = VI_ACT_TOGGLE_CASE;
                out->start = cursor;
                out->end = (cursor + count > line_end) ? line_end : cursor + count;
                out->count = count;
                return commit(st, out, count);

            case 'i':
                st->mode = VI_INSERT;
                out->kind = VI_ACT_MOVE;
                out->start = out->end = cursor;
                return commit(st, out, count);

            case 'I':
                st->mode = VI_INSERT;
                out->kind = VI_ACT_MOVE;
                out->start = out->end = first_non_blank(buf, len, cursor);
                return commit(st, out, count);

            case 'a':
                st->mode = VI_INSERT;
                out->kind = VI_ACT_MOVE;
                out->start = out->end = (cursor < line_end) ? cursor + 1 : line_end;
                return commit(st, out, count);

            case 'A':
                st->mode = VI_INSERT;
                out->kind = VI_ACT_MOVE;
                out->start = out->end = line_end;
                return commit(st, out, count);

            case 'o':
                st->mode = VI_INSERT;
                out->kind = VI_ACT_OPEN_BELOW;
                out->start = out->end = cursor;
                return commit(st, out, count);

            case 'O':
                st->mode = VI_INSERT;
                out->kind = VI_ACT_OPEN_ABOVE;
                out->start = out->end = cursor;
                return commit(st, out, count);

            case 'v':
                st->mode = VI_VISUAL;
                st->anchor = cursor;
                out->kind = VI_ACT_REDRAW;
                return commit(st, out, count);

            case 'V':
                st->mode = VI_VISUAL_LINE;
                st->anchor = cursor;
                out->kind = VI_ACT_REDRAW;
                return commit(st, out, count);

            case 'n':
            case 'N':
                if (st->pattern_len == 0)
                {
                    return beep(st, out, "E35: no previous search");
                }
                set_mark(st, cursor);
                out->kind = VI_ACT_SEARCH;
                out->ch = (st->search_forward == (key == 'n')) ? '/' : '?';
                return commit(st, out, count);

            case '*':
            case '#':
                return search_word(st, buf, len, cursor, key == '*', out);

            case 0x01:  // Ctrl+A -- add to the number under the cursor, and
            case 0x18:  // Ctrl+X take away from it. Ctrl+X is cut outside vi
                        // mode; vi mode owns its keys (§5.1, §24.4).
                out->kind = VI_ACT_INCREMENT;
                out->start = out->end = cursor;
                out->count = (key == 0x01) ? count : -count;
                return commit(st, out, count);

            case 0x07:  // Ctrl+G -- where the cursor is. A count means nothing
                        // to it, and take_count above has already dropped one
                return report_position(st, buf, len, cursor, out);

            case ':':
            case '/':
            case '?':
                return enter_cmdline(st, key, out);

            case 'u':
            case 0x12:  // Ctrl+R
                // Undoing from visual mode drops the selection: what comes back
                // is the text as it was, not a range to keep working on
                st->mode = VI_NORMAL;
                out->kind = (key == 'u') ? VI_ACT_UNDO : VI_ACT_REDO;
                out->count = count;
                return commit(st, out, count);

            default:
                return beep(st, out, "E492: not an editor command");
        }
    }
}

//
//  The interface
//

void editor_vi_reset(ViState *st)
{
    memset(st, 0, sizeof(*st));
    st->mode = VI_NORMAL;
    st->search_forward = true;
}

bool editor_vi_key(ViState *st, const char *buf, size_t len, size_t cursor,
                   int key, ViAction *out)
{
    if (cursor > len)
    {
        cursor = len;
    }

    // An edit under a live selection -- `X` in visual mode, an undo -- shortens
    // the buffer without touching the anchor, and a stale one would put an
    // operator's range past the end (B39)
    if (st->anchor > len)
    {
        st->anchor = len;
    }

    out->kind = VI_ACT_NONE;
    out->start = cursor;
    out->end = cursor;
    out->linewise = false;
    out->ch = 0;
    out->count = 0;
    out->msg = NULL;
    out->insert = NULL;
    out->insert_len = 0;

    // A visual command builds up over several keys, and each of them completes
    // and clears its own stroke on the way -- so nothing is left of `vld` but
    // the `d`, which replays as an operator waiting for a motion that never
    // comes. `commit` records no change made from visual mode (B43).
    st->from_visual = (st->mode == VI_VISUAL || st->mode == VI_VISUAL_LINE);

    if (st->mode == VI_VISUAL || st->mode == VI_VISUAL_LINE)
    {
        // `'<` and `'>` name the selection as it stood when the key arrived, so
        // the key that ends visual mode leaves the right pair behind (§19)
        st->vis_start = st->anchor < cursor ? st->anchor : cursor;
        st->vis_end = st->anchor < cursor ? cursor : st->anchor;
        st->vis_set = true;
    }

    switch (st->mode)
    {
        case VI_INSERT:
            if (key == KEY_ESC)
            {
                // Vi steps back off the character just typed
                size_t start = line_start_of(buf, cursor);
                st->mode = VI_NORMAL;
                record_insert(st, buf, len, cursor);
                out->kind = VI_ACT_MOVE;
                out->start = out->end = (cursor > start) ? cursor - 1 : start;
                return true;
            }
            // Everything else -- the arrows, backspace, every printable
            // character -- is the editor's own handling, unchanged
            return false;

        case VI_CMDLINE:
            return cmdline_key(st, buf, len, cursor, key, out);

        default:
            if (key == '.' && st->pending_op == 0 && st->pending_prefix == 0 &&
                st->mode == VI_NORMAL)
            {
                return replay(st, buf, len, cursor, out);
            }
            return normal_key(st, buf, len, cursor, key, out);
    }
}

void editor_vi_insert_began(ViState *st, size_t cursor, size_t len)
{
    st->insert_origin = cursor;
    st->insert_len0 = len;
    // The keys of the command that opened the session, still in `stroke`. A
    // session opened without them -- there is no such path today, but a stroke
    // buffer that overflowed would look like one -- is simply not recorded.
    st->insert_recording = st->stroke_len > 0;
}

const char *editor_vi_status(const ViState *st)
{
    switch (st->mode)
    {
        case VI_INSERT:      return "-- INSERT --";
        case VI_VISUAL:      return "-- VISUAL --";
        case VI_VISUAL_LINE: return "-- VISUAL LINE --";
        case VI_CMDLINE:     return st->cmdline;
        default:             return "-- NORMAL --";
    }
}

//
//  Substitute
//

// The next accepted match in line [ls, le), searching at or after `at`. Both
// passes call this identically, which is what keeps the count and the rewrite
// from ever disagreeing (§16.4). `prev_end` is where the last accepted match
// ended: an empty match landing there is vi's "step one character" case and is
// skipped rather than matched forever. Groups come back as absolute offsets.
static bool sub_next(const char *buf, size_t ls, size_t le,
                     const char *pat, size_t pat_len,
                     size_t at, size_t prev_end,
                     EditorPatternGroups g, size_t *ms, size_t *me,
                     bool *too_complex)
{
    while (at <= le)
    {
        if (!editor_pattern_search(pat, pat_len, buf + ls, le - ls, at - ls, g,
                                   too_complex))
        {
            return false;
        }
        for (int k = 0; k < 10; k++)
        {
            g[k].start += ls;
            g[k].end += ls;
        }
        size_t s = g[0].start;
        size_t e = g[0].end;
        if (s == e && s == prev_end)
        {
            at = s + 1;  // Empty match at the last one's end: step over it
            continue;
        }
        *ms = s;
        *me = e;
        return true;
    }
    return false;
}

//
//  Moving and copying lines (§22)
//

static void reverse_bytes(char *p, size_t len)
{
    for (size_t i = 0, j = len; i + 1 < j; i++)
    {
        j--;
        char t = p[i];
        p[i] = p[j];
        p[j] = t;
    }
}

// Rotate the k bytes at the front of [p, p + n) to the back of it, in order.
// Three reversals, so a move needs nothing to hold the text it is moving and
// no bound on how much of it there is (§22.3).
static void rotate_left(char *p, size_t n, size_t k)
{
    if (k == 0 || k >= n)
    {
        return;
    }
    reverse_bytes(p, k);
    reverse_bytes(p + k, n - k);
    reverse_bytes(p, n);
}

bool editor_vi_move_lines(char *buf, size_t *len, size_t capacity,
                          size_t start, size_t end, size_t dest, bool copy,
                          EditorUndo *undo, size_t *out_cursor)
{
    size_t n = *len;

    if (end > n) end = n;
    if (dest > n) dest = n;
    if (start >= end)
    {
        return false;
    }

    // A line is text and the newline that ends it, and the last line of a
    // buffer that does not end in one has neither a newline to carry away nor
    // one to attach to. Lend it one for the length of the operation and take
    // one back at the end, so everything between is uniform (§22.4).
    bool borrowed = (buf[n - 1] != '\n' && (end == n || dest == n));

    // Everything that has to fit, weighed before a byte moves: the newline the
    // buffer may be lent, and a second span when the lines are being copied
    size_t span = (end - start) + ((borrowed && end == n) ? 1 : 0);
    if (n + (borrowed ? 1 : 0) + (copy ? span : 0) >= capacity)
    {
        return false;
    }

    if (borrowed)
    {
        editor_undo_record(undo, n, NULL, 0, "\n", 1);
        buf[n] = '\n';
        n++;
        buf[n] = '\0';
        if (end == n - 1) end = n;
        if (dest == n - 1) dest = n;
    }

    size_t landing;

    if (copy)
    {
        // Recorded before the bytes move, while the span is still one piece --
        // which is what a copy of the buffer into itself needs, since opening
        // the hole below may split it
        editor_undo_record(undo, dest, NULL, 0, buf + start, span);

        memmove(buf + dest + span, buf + dest, n - dest);
        if (start >= dest)
        {
            memmove(buf + dest, buf + start + span, span);   // The source moved with the tail
        }
        else if (end <= dest)
        {
            memmove(buf + dest, buf + start, span);
        }
        else
        {
            // The hole opened inside the source: its head stayed put and its
            // tail went up with everything else
            size_t head = dest - start;
            memmove(buf + dest, buf + start, head);
            memmove(buf + dest + head, buf + dest + span, span - head);
        }
        n += span;
        landing = dest;
    }
    else
    {
        // The delete and the insert it becomes, both taken from the span while
        // it is still where it was: the journal wants the bytes, not a buffer
        // to keep them in (§22.3)
        landing = (dest > start) ? dest - span : dest;
        editor_undo_record(undo, start, buf + start, span, NULL, 0);
        editor_undo_record(undo, landing, NULL, 0, buf + start, span);

        if (dest > end)
        {
            rotate_left(buf + start, dest - start, span);
        }
        else
        {
            rotate_left(buf + dest, end - dest, start - dest);
        }
    }

    buf[n] = '\0';

    // Vi leaves the cursor on the last line of what it moved, which after a
    // copy is the copy rather than the original
    size_t landed = first_non_blank(buf, n, line_start_of(buf, landing + span - 1));

    if (borrowed && buf[n - 1] == '\n')
    {
        // Give the newline back. Whichever one is at the end now, the buffer
        // is left the shape it was found in, and this record is still inside
        // the caller's undo step.
        editor_undo_record(undo, n - 1, buf + n - 1, 1, NULL, 0);
        n--;
        buf[n] = '\0';
    }

    if (landed > n) landed = n;
    *len = n;
    *out_cursor = landed;
    return true;
}

size_t editor_vi_substitute(char *buf, size_t *len, size_t capacity,
                            size_t range_start, size_t range_end,
                            const char *pat, size_t pat_len,
                            const char *rep, size_t rep_len,
                            bool global, EditorUndo *undo, size_t *out_cursor)
{
    size_t n = *len;

    if (pat_len == 0 || !editor_pattern_valid(pat, pat_len))
    {
        return 0;
    }
    if (range_end > n)
    {
        range_end = n;
    }
    range_start = line_start_of(buf, range_start > n ? n : range_start);
    if (range_start >= range_end)
    {
        return 0;
    }

    char expand[LOGO_VI_SUB_EXPAND_MAX];
    EditorPatternGroups g;
    bool too_complex = false;

    // Count first, accumulating the length change: patterns make every match
    // and every replacement a different size, so the old `count * len`
    // arithmetic no longer holds. A substitute that would not fit -- or one
    // match that expands past the stack buffer -- refuses here, before a byte
    // moves, which keeps the all-or-nothing property the buffer's safety rests
    // on.
    size_t count = 0;
    long delta = 0;  // added - removed, over every match
    for (size_t line = range_start; line < range_end; )
    {
        size_t end = line_end_of(buf, n, line);
        size_t at = line;
        size_t prev_end = SIZE_MAX;
        size_t ms, me;
        while (sub_next(buf, line, end, pat, pat_len, at, prev_end, g, &ms, &me,
                        &too_complex))
        {
            size_t explen = editor_pattern_expand(rep, rep_len, buf, g,
                                                  expand, sizeof(expand));
            if (explen == SIZE_MAX)
            {
                return 0;  // The replacement of one match is too long
            }
            count++;
            delta += (long)explen - (long)(me - ms);
            prev_end = me;
            if (!global)
            {
                break;
            }
            at = (ms == me) ? ms + 1 : me;
        }
        if (too_complex)
        {
            return SIZE_MAX;  // Abandoned, not finished (B36) -- nothing moved
        }
        if (end >= n)
        {
            break;
        }
        line = end + 1;
    }

    if (count == 0 || (long)n + delta + 1 > (long)capacity)
    {
        return 0;
    }

    size_t last_changed = range_start;
    size_t limit = range_end;
    for (size_t line = range_start; line < limit; )
    {
        size_t end = line_end_of(buf, n, line);
        size_t at = line;
        size_t prev_end = SIZE_MAX;
        size_t ms, me;
        bool changed = false;
        // The counting pass cleared the budget on these same lines, so this one
        // does too; the flag is passed for the case where a longer replacement
        // makes a line dearer to rescan. If it ever trips here the loop simply
        // ends -- every splice already made is complete and journalled, so the
        // buffer stays consistent, which is the property that matters.
        while (sub_next(buf, line, end, pat, pat_len, at, prev_end, g, &ms, &me,
                        &too_complex))
        {
            size_t explen = editor_pattern_expand(rep, rep_len, buf, g,
                                                  expand, sizeof(expand));
            // One record per match, before the bytes move: the whole rewritten
            // span would be far larger, and a `:%s` has to stay undoable
            if (undo != NULL)
            {
                editor_undo_record(undo, ms, buf + ms, me - ms, expand, explen);
            }
            memmove(buf + ms + explen, buf + me, n - me);
            memcpy(buf + ms, expand, explen);
            size_t matchlen = me - ms;
            n = n - matchlen + explen;
            end = end - matchlen + explen;
            limit = limit - matchlen + explen;
            changed = true;
            // Resume past the replacement so it is never rescanned; the tail
            // beyond it is the original bytes the counting pass saw, shifted, so
            // both passes walk the same matches.
            prev_end = ms + explen;
            at = (ms == me) ? ms + explen + 1 : ms + explen;
            if (!global)
            {
                break;
            }
        }
        if (changed)
        {
            last_changed = line;
        }
        if (end >= n)
        {
            break;
        }
        line = end + 1;
    }

    buf[n] = '\0';
    *len = n;
    if (out_cursor != NULL)
    {
        *out_cursor = last_changed;
    }
    return count;
}

//
//  The global commands (§23)
//

size_t editor_vi_global(char *buf, size_t *len, size_t capacity,
                        size_t range_start, size_t range_end,
                        const char *pat, size_t pat_len, bool invert, char cmd,
                        const char *sub_pat, size_t sub_pat_len,
                        const char *rep, size_t rep_len, bool sub_global,
                        EditorUndo *undo, size_t *out_cursor)
{
    size_t n = *len;

    if (pat_len == 0 || !editor_pattern_valid(pat, pat_len))
    {
        return 0;
    }
    if (range_end > n)
    {
        range_end = n;
    }
    range_start = line_start_of(buf, range_start > n ? n : range_start);
    if (range_start >= range_end)
    {
        return 0;
    }

    EditorPatternGroups g;
    size_t count = 0;
    size_t landed = range_start;
    bool refused = false;

    // Backwards, from the last line of the range: what a line does happens
    // below the lines still to be visited, so nothing this walk still has to
    // find ever moves and there is nothing to carry from one line to the next
    // (§23.3). `range_end` is the start of the line after the last one in the
    // range, or the end of the buffer.
    for (size_t line = line_start_of(buf, range_end - 1); ; )
    {
        size_t end = line_end_of(buf, n, line);
        bool too_complex = false;
        bool hit = editor_pattern_search(pat, pat_len, buf + line, end - line, 0,
                                         g, &too_complex);
        if (too_complex)
        {
            // The next line would pay the same budget again (B36). Everything
            // already done is complete and journalled, so the walk simply ends;
            // only a pass that had done nothing yet is a refusal to report.
            refused = true;
            break;
        }

        if (hit != invert)
        {
            if (cmd == 'd')
            {
                size_t to = (end < n) ? end + 1 : n;   // The line and its break
                editor_undo_record(undo, line, buf + line, to - line, NULL, 0);
                memmove(buf + line, buf + to, n - to);
                n -= to - line;
                buf[n] = '\0';
                count++;
                landed = line;
            }
            else
            {
                // One line is a range of one line, so the substitute is the
                // same loop it always was rather than a second one (§23.5)
                size_t did = editor_vi_substitute(buf, &n, capacity, line,
                                                  (end < n) ? end + 1 : n,
                                                  sub_pat, sub_pat_len,
                                                  rep, rep_len, sub_global,
                                                  undo, NULL);
                if (did == SIZE_MAX)
                {
                    refused = true;
                    break;
                }
                if (did > 0)
                {
                    count++;
                    landed = line;
                }
            }
        }

        if (line <= range_start)
        {
            break;
        }
        line = line_start_of(buf, line - 1);
    }

    *len = n;
    if (out_cursor != NULL)
    {
        // The walk ends at the top, so this is the first line the pass changed
        *out_cursor = first_non_blank(buf, n, landed > n ? n : landed);
    }
    return (refused && count == 0) ? SIZE_MAX : count;
}

//
//  `Ctrl` `A` and `Ctrl` `X` (§24.4)
//

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

// Long enough for any run this will touch: the scan refuses more digits than a
// long long can hold, and the result is rendered back through the same width
#define VI_INC_DIGITS_MAX 18

ViIncrement editor_vi_increment(char *buf, size_t *len, size_t capacity,
                                size_t cursor, int delta,
                                EditorUndo *undo, size_t *out_cursor)
{
    size_t n = *len;
    if (cursor > n)
    {
        cursor = n;
    }
    size_t line = line_start_of(buf, cursor);
    size_t line_end = line_end_of(buf, n, cursor);

    // The number under the cursor, or the first one to its right on this line
    size_t first = cursor;
    while (first < line_end && !is_digit(buf[first]))
    {
        first++;
    }
    if (first >= line_end)
    {
        return VI_INC_NO_NUMBER;
    }
    while (first > line && is_digit(buf[first - 1]))
    {
        first--;   // The cursor was standing in the middle of the run
    }
    size_t last = first;
    while (last < line_end && is_digit(buf[last]))
    {
        last++;
    }
    if (last - first > VI_INC_DIGITS_MAX)
    {
        return VI_INC_NO_NUMBER;
    }

    long long value = 0;
    for (size_t i = first; i < last; i++)
    {
        value = value * 10 + (buf[i] - '0');
    }

    // A `-` against the digits is part of the number, so `fd -100` counts down.
    // A `.` is not: `10.5` is two numbers, which is vim's rule and keeps
    // single-precision rounding out of a literal the user typed.
    size_t start = first;
    if (start > line && buf[start - 1] == '-')
    {
        start--;
        value = -value;
    }

    char text[VI_INC_DIGITS_MAX + 3];
    int m = snprintf(text, sizeof(text), "%lld", value + delta);
    if (m < 0 || (size_t)m >= sizeof(text))
    {
        return VI_INC_NO_NUMBER;
    }

    size_t old = last - start;
    if (n - old + (size_t)m >= capacity)
    {
        return VI_INC_NO_ROOM;
    }

    editor_undo_record(undo, start, buf + start, old, text, (size_t)m);

    memmove(buf + start + m, buf + last, n - last);
    memcpy(buf + start, text, (size_t)m);
    n = n - old + (size_t)m;
    buf[n] = '\0';

    *len = n;
    *out_cursor = start + (size_t)m - 1;   // Vi leaves it on the last digit
    return VI_INC_OK;
}
