//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the editor's vi key layer (devices/picocalc/editor_vi.c).
//
//  editor_vi.c decides what a keystroke means and returns a byte range; the
//  editor turns that into an edit. Asserting on raw offsets would test the
//  arithmetic and miss the meaning, so the harness below carries out the
//  actions the way editor.c does and the tests read as "these keys, then this
//  text". Where an offset is the point -- `dd` on the last line taking the
//  newline before it -- the range is checked directly.
//
//  The harness keeps an EditorLineIndex beside the buffer and drives it exactly
//  as editor.c does, so the randomised run at the end catches the failure the
//  design warns about: a mutation that forgets to call editor_lines_edit.
//

#include "unity.h"
#include "editor_vi.h"
#include "editor_lines.h"
#include "editor_undo.h"
#include "keyboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ED_CAP 1024
#define TAB_WIDTH 2

// Big enough that the randomised run below never drops a step, so "undo
// everything and you have what you started with" is an exact property
#define UNDO_CAP (1024 * 1024)

typedef struct
{
    char buf[ED_CAP];
    size_t len;
    size_t cursor;
    char yank[ED_CAP];
    size_t yank_len;
    EditorLineIndex ix;
    ViState vi;
    EditorUndo undo;
    ViAction last;      // The action the last key produced
    bool consumed;      // ... and whether vi took the key at all
    size_t len_at_key;  // The length the last key saw, for range assertions
    int exits;          // VI_ACT_ACCEPT / _QUIT / _CANCEL seen
    int writes;         // VI_ACT_WRITE seen (`:w`, which does not exit)
    ViActionKind exit_kind;
} Ed;

static Ed ed;
static char undo_store[UNDO_CAP];

void setUp(void)
{
    memset(&ed, 0, sizeof(ed));
    editor_vi_reset(&ed.vi);
    editor_lines_reset(&ed.ix);
    editor_undo_init(&ed.undo, undo_store, sizeof(undo_store));
}

void tearDown(void) {}

//
//  The buffer, driven the way editor.c drives it
//

static size_t ed_line_start(const Ed *e, size_t pos)
{
    while (pos > 0 && e->buf[pos - 1] != '\n') pos--;
    return pos;
}

static size_t ed_line_end(const Ed *e, size_t pos)
{
    while (pos < e->len && e->buf[pos] != '\n') pos++;
    return pos;
}

static void ed_insert(Ed *e, size_t pos, const char *text, size_t n)
{
    if (n == 0 || e->len + n >= ED_CAP) return;
    editor_undo_record(&e->undo, pos, &e->buf[pos], 0, text, n);
    editor_lines_edit(&e->ix, pos);
    memmove(&e->buf[pos + n], &e->buf[pos], e->len - pos);
    memcpy(&e->buf[pos], text, n);
    e->len += n;
    e->buf[e->len] = '\0';
}

static void ed_delete(Ed *e, size_t start, size_t end)
{
    if (end > e->len) end = e->len;
    if (start >= end) return;
    editor_undo_record(&e->undo, start, &e->buf[start], end - start, NULL, 0);
    editor_lines_edit(&e->ix, start);
    memmove(&e->buf[start], &e->buf[end], e->len - end);
    e->len -= (end - start);
    e->buf[e->len] = '\0';
}

static void ed_yank(Ed *e, size_t start, size_t end, bool linewise)
{
    if (end > e->len) end = e->len;
    if (start > end) start = end;

    size_t n = end - start;
    memcpy(e->yank, &e->buf[start], n);
    if (linewise) {
        if (n > 0 && e->yank[0] == '\n' && e->yank[n - 1] != '\n') {
            memmove(e->yank, e->yank + 1, --n);
        }
        if (n == 0 || e->yank[n - 1] != '\n') e->yank[n++] = '\n';
    }
    e->yank[n] = '\0';
    e->yank_len = n;
}

static void ed_paste(Ed *e, int count, bool after)
{
    if (e->yank_len == 0) return;

    bool linewise = e->yank[e->yank_len - 1] == '\n';
    size_t at;

    if (linewise) {
        if (after) {
            at = ed_line_end(e, e->cursor);
            if (at < e->len) {
                at++;
            } else {
                ed_insert(e, at, "\n", 1);
                at = e->len;
            }
        } else {
            at = ed_line_start(e, e->cursor);
        }
    } else {
        at = e->cursor;
        if (after && at < ed_line_end(e, e->cursor)) at++;
    }

    size_t before = e->len;
    for (int i = 0; i < count; i++) {
        ed_insert(e, at + (e->len - before), e->yank, e->yank_len);
    }
    if (e->len == before) return;

    if (linewise) {
        size_t p = ed_line_start(e, at);
        size_t end = ed_line_end(e, p);
        while (p < end && (e->buf[p] == ' ' || e->buf[p] == '\t')) p++;
        e->cursor = p;
    } else {
        e->cursor = at + (e->len - before) - 1;
    }
}

static void ed_join(Ed *e, int count)
{
    for (int i = 1; i < count; i++) {
        size_t line_start = ed_line_start(e, e->cursor);
        size_t end = ed_line_end(e, e->cursor);
        if (end >= e->len) break;

        size_t next = end + 1;
        while (next < e->len && (e->buf[next] == ' ' || e->buf[next] == '\t')) next++;
        ed_delete(e, end, next);
        e->cursor = end;

        if (end > line_start && e->buf[end - 1] != ' ' &&
            end < e->len && e->buf[end] != '\n') {
            ed_insert(e, end, " ", 1);
        }
    }
}

static void ed_open(Ed *e, bool below)
{
    size_t line_start = ed_line_start(e, e->cursor);
    char text[ED_CAP];
    size_t indent = 0;
    while (line_start + indent < e->len &&
           (e->buf[line_start + indent] == ' ' || e->buf[line_start + indent] == '\t')) {
        indent++;
    }

    if (below) {
        size_t at = ed_line_end(e, e->cursor);
        text[0] = '\n';
        memcpy(text + 1, &e->buf[line_start], indent);
        ed_insert(e, at, text, indent + 1);
        e->cursor = at + 1 + indent;
    } else {
        memcpy(text, &e->buf[line_start], indent);
        text[indent] = '\n';
        ed_insert(e, line_start, text, indent + 1);
        e->cursor = line_start + indent;
    }
}

// The same shifting the editor's Ctrl+, / Ctrl+. pair does, over a byte range.
// Back to front, so a line start is never moved before it has been shifted.
static void ed_indent(Ed *e, size_t start, size_t end, int stops)
{
    size_t first = ed_line_start(e, start);
    if (end > e->len) end = e->len;

    int lines = 1;
    for (size_t p = first; p < end; ) {
        size_t nl = ed_line_end(e, p);
        if (nl >= e->len || nl + 1 >= end) break;
        p = nl + 1;
        lines++;
    }
    if (lines > 128) lines = 128;

    for (int pass = 0; pass < (stops < 0 ? -stops : stops); pass++) {
        size_t starts[128];
        int n = 0;
        for (size_t p = first; n < lines; ) {
            starts[n++] = p;
            size_t nl = ed_line_end(e, p);
            if (nl >= e->len) break;
            p = nl + 1;
        }
        for (int i = n; i-- > 0; ) {
            if (stops > 0) {
                ed_insert(e, starts[i], "  ", TAB_WIDTH);
            } else {
                size_t k = 0;
                while (k < TAB_WIDTH && starts[i] + k < e->len &&
                       e->buf[starts[i] + k] == ' ') {
                    k++;
                }
                ed_delete(e, starts[i], starts[i] + k);
            }
        }
    }

    size_t p = first, line_end = ed_line_end(e, first);
    while (p < line_end && (e->buf[p] == ' ' || e->buf[p] == '\t')) p++;
    e->cursor = p;
}

static void ed_apply(Ed *e, const ViAction *act)
{
    switch (act->kind) {
        case VI_ACT_NONE:
        case VI_ACT_REDRAW:
        case VI_ACT_BEEP:
            break;

        case VI_ACT_MOVE:
            e->cursor = act->start;
            break;

        case VI_ACT_YANK:
            ed_yank(e, act->start, act->end, act->linewise);
            e->cursor = act->start;
            break;

        case VI_ACT_DELETE:
        case VI_ACT_CHANGE:
            ed_yank(e, act->start, act->end, act->linewise);
            ed_delete(e, act->start, act->end);
            e->cursor = act->start;
            break;

        case VI_ACT_PASTE_AFTER:
        case VI_ACT_PASTE_BEFORE:
            ed_paste(e, act->count > 0 ? act->count : 1, act->kind == VI_ACT_PASTE_AFTER);
            break;

        case VI_ACT_PASTE_OVER:
            ed_delete(e, act->start, act->end);
            e->cursor = act->start;
            ed_paste(e, 1, false);
            break;

        case VI_ACT_INDENT:
            ed_indent(e, act->start, act->end, act->count);
            break;

        case VI_ACT_REPLACE_CHAR:
            if (act->ch == '\n') {
                ed_delete(e, act->start, act->end);
                ed_insert(e, act->start, "\n", 1);
                e->cursor = act->start + 1;
                break;
            }
            for (size_t i = act->start; i < act->end && i < e->len; i++) {
                if (e->buf[i] == act->ch) continue;
                editor_undo_record(&e->undo, i, &e->buf[i], 1, &act->ch, 1);
                e->buf[i] = act->ch;
            }
            e->cursor = act->end > act->start ? act->end - 1 : act->start;
            break;

        case VI_ACT_OPEN_BELOW:
        case VI_ACT_OPEN_ABOVE:
            ed_open(e, act->kind == VI_ACT_OPEN_BELOW);
            break;

        case VI_ACT_JOIN:
            e->cursor = act->start;
            ed_join(e, act->count);
            break;

        case VI_ACT_TOGGLE_CASE:
            for (size_t i = act->start; i < act->end && i < e->len; i++) {
                char c = e->buf[i], flipped = c;
                if (c >= 'a' && c <= 'z')      flipped = (char)(c - 'a' + 'A');
                else if (c >= 'A' && c <= 'Z') flipped = (char)(c - 'A' + 'a');
                if (flipped == c) continue;
                editor_undo_record(&e->undo, i, &c, 1, &flipped, 1);
                e->buf[i] = flipped;
            }
            e->cursor = act->end < e->len ? act->end : act->start;
            break;

        case VI_ACT_SEARCH:
            // The editor runs this through editor_search_find; the state
            // machine's part is choosing the pattern and the direction
            break;

        case VI_ACT_SUBSTITUTE: {
            size_t landed = e->cursor;
            size_t n = editor_vi_substitute(e->buf, &e->len, ED_CAP, act->start, act->end,
                                            e->vi.pattern, e->vi.pattern_len,
                                            e->vi.replacement, e->vi.replacement_len,
                                            e->vi.sub_global, &e->undo, &landed);
            if (n > 0) {
                editor_lines_reset(&e->ix);
                e->cursor = landed;
            }
            break;
        }

        case VI_ACT_UNDO:
        case VI_ACT_REDO: {
            int steps = act->count > 0 ? act->count : 1;
            for (int i = 0; i < steps; i++) {
                size_t at;
                bool moved = (act->kind == VI_ACT_UNDO)
                    ? editor_undo_undo(&e->undo, e->buf, &e->len, ED_CAP, &at)
                    : editor_undo_redo(&e->undo, e->buf, &e->len, ED_CAP, &at);
                if (!moved) break;
                editor_lines_reset(&e->ix);
                e->cursor = at;
            }
            break;
        }

        case VI_ACT_WRITE:
            // The editor writes the buffer out and carries on; the state
            // machine's part is only saying so
            e->writes++;
            break;

        case VI_ACT_ACCEPT:
        case VI_ACT_QUIT:
        case VI_ACT_CANCEL:
            e->exits++;
            e->exit_kind = act->kind;
            break;
    }

    if (e->cursor > e->len) e->cursor = e->len;
}

//
//  Driving keys
//

static void ed_set(const char *text)
{
    strcpy(ed.buf, text);
    ed.len = strlen(text);
    ed.cursor = 0;
    editor_lines_reset(&ed.ix);
    editor_undo_reset(&ed.undo);
}

static void feed_key(int key)
{
    ViAction act;

    // One keystroke is one undo step, except while insert mode is running --
    // the same boundary editor.c draws
    if (ed.vi.mode != VI_INSERT) {
        editor_undo_begin(&ed.undo);
    }

    ed.consumed = editor_vi_key(&ed.vi, ed.buf, ed.len, ed.cursor, key, &act);
    ed.last = act;
    ed.len_at_key = ed.len;

    if (ed.consumed) {
        ed_apply(&ed, &act);
        return;
    }

    // What the editor's own key handling does with what vi hands back. Only
    // insert mode ever does, and only for the keys that put text in a buffer.
    if (ed.vi.mode == VI_INSERT) {
        if (key >= 0x20 && key <= 0x7E) {
            char c = (char)key;
            ed_insert(&ed, ed.cursor, &c, 1);
            ed.cursor++;
        } else if (key == KEY_RETURN || key == KEY_ENTER) {
            ed_insert(&ed, ed.cursor, "\n", 1);
            ed.cursor++;
        } else if (key == KEY_BACKSPACE && ed.cursor > 0) {
            ed_delete(&ed, ed.cursor - 1, ed.cursor);
            ed.cursor--;
        }
    }
}

static void feed(const char *keys)
{
    for (const char *k = keys; *k; k++) feed_key((unsigned char)*k);
}

static void assert_text(const char *expected)
{
    TEST_ASSERT_EQUAL_STRING(expected, ed.buf);
    TEST_ASSERT_EQUAL_UINT(strlen(expected), ed.len);
}

// The offsets of the range the last key produced
static void assert_range(size_t start, size_t end)
{
    TEST_ASSERT_EQUAL_UINT(start, ed.last.start);
    TEST_ASSERT_EQUAL_UINT(end, ed.last.end);
}

// Put the cursor at an offset without going through a motion
static void at(size_t pos)
{
    ed.cursor = pos;
}

//
//  Motions
//

static void test_hjkl_move_by_one(void)
{
    ed_set("abc\ndefg\n");
    at(1);
    feed("l");   TEST_ASSERT_EQUAL_UINT(2, ed.cursor);
    feed("h");   TEST_ASSERT_EQUAL_UINT(1, ed.cursor);
    feed("j");   TEST_ASSERT_EQUAL_UINT(5, ed.cursor);
    feed("k");   TEST_ASSERT_EQUAL_UINT(1, ed.cursor);
}

static void test_a_motion_takes_a_count(void)
{
    ed_set("abcdefgh");
    feed("5l");  TEST_ASSERT_EQUAL_UINT(5, ed.cursor);
    feed("3h");  TEST_ASSERT_EQUAL_UINT(2, ed.cursor);
    feed("12l"); TEST_ASSERT_EQUAL_UINT(8, ed.cursor);  // Clamped to the line
}

static void test_h_and_l_stop_at_the_ends_of_the_line(void)
{
    ed_set("ab\ncd\n");
    at(0);
    feed("h");  TEST_ASSERT_EQUAL_UINT(0, ed.cursor);   // Never onto the line before
    feed("9l"); TEST_ASSERT_EQUAL_UINT(2, ed.cursor);   // Never onto the newline after
    feed("h");  TEST_ASSERT_EQUAL_UINT(1, ed.cursor);
}

static void test_j_and_k_keep_the_column_where_the_line_is_long_enough(void)
{
    ed_set("abcdef\nxy\nghijkl\n");
    at(4);
    feed("j");  TEST_ASSERT_EQUAL_UINT(9, ed.cursor);   // Clamped to the end of "xy"
    feed("j");  TEST_ASSERT_EQUAL_UINT(12, ed.cursor);  // Column 2 of "ghijkl"
    feed("k");  TEST_ASSERT_EQUAL_UINT(9, ed.cursor);
}

static void test_j_and_k_stop_at_the_ends_of_the_buffer(void)
{
    ed_set("a\nb\nc");
    feed("k");   TEST_ASSERT_EQUAL_UINT(0, ed.cursor);
    feed("9j");  TEST_ASSERT_EQUAL_UINT(4, ed.cursor);
    feed("9j");  TEST_ASSERT_EQUAL_UINT(4, ed.cursor);
}

static void test_zero_caret_and_dollar(void)
{
    ed_set("  to box\nend\n");
    at(6);
    feed("0");   TEST_ASSERT_EQUAL_UINT(0, ed.cursor);
    feed("^");   TEST_ASSERT_EQUAL_UINT(2, ed.cursor);
    feed("$");   TEST_ASSERT_EQUAL_UINT(8, ed.cursor);
}

static void test_zero_is_a_motion_until_there_is_a_count_to_extend(void)
{
    ed_set("abcdefghijklmnopqrstuvwxyz");
    at(5);
    feed("0");    TEST_ASSERT_EQUAL_UINT(0, ed.cursor);
    feed("10l");  TEST_ASSERT_EQUAL_UINT(10, ed.cursor);
}

static void test_word_motions_stop_at_punctuation(void)
{
    //             0123456789
    ed_set("fd :size]\n");
    feed("w");   TEST_ASSERT_EQUAL_UINT(3, ed.cursor);   // ':'
    feed("w");   TEST_ASSERT_EQUAL_UINT(4, ed.cursor);   // "size"
    feed("w");   TEST_ASSERT_EQUAL_UINT(8, ed.cursor);   // ']'
    feed("b");   TEST_ASSERT_EQUAL_UINT(4, ed.cursor);
    feed("b");   TEST_ASSERT_EQUAL_UINT(3, ed.cursor);
}

static void test_big_word_motions_only_stop_at_blanks(void)
{
    ed_set("fd :size]\n");
    feed("W");   TEST_ASSERT_EQUAL_UINT(3, ed.cursor);
    feed("W");   TEST_ASSERT_EQUAL_UINT(10, ed.cursor);  // Past the end of the line
    feed("B");   TEST_ASSERT_EQUAL_UINT(3, ed.cursor);
    feed("B");   TEST_ASSERT_EQUAL_UINT(0, ed.cursor);
}

static void test_e_lands_on_the_last_character_of_a_word(void)
{
    ed_set("to box\nend\n");
    feed("e");   TEST_ASSERT_EQUAL_UINT(1, ed.cursor);
    feed("e");   TEST_ASSERT_EQUAL_UINT(5, ed.cursor);
    feed("e");   TEST_ASSERT_EQUAL_UINT(9, ed.cursor);
    feed("e");   TEST_ASSERT_EQUAL_UINT(9, ed.cursor);   // No word left; stays put
}

static void test_a_word_motion_crosses_lines(void)
{
    ed_set("to box\nfd 10\n");
    feed("2w");  TEST_ASSERT_EQUAL_UINT(7, ed.cursor);
    feed("b");   TEST_ASSERT_EQUAL_UINT(3, ed.cursor);
}

static void test_gg_and_G(void)
{
    ed_set("one\n  two\nthree\n");
    feed("G");    TEST_ASSERT_EQUAL_UINT(16, ed.cursor);  // The empty line the trailing newline leaves
    feed("gg");   TEST_ASSERT_EQUAL_UINT(0, ed.cursor);
    feed("2G");   TEST_ASSERT_EQUAL_UINT(6, ed.cursor);   // First non-blank of line 2
    feed("2gg");  TEST_ASSERT_EQUAL_UINT(6, ed.cursor);
}

static void test_G_past_the_last_line_clamps(void)
{
    ed_set("one\ntwo\n");
    feed("99G");
    TEST_ASSERT_EQUAL_UINT(8, ed.cursor);
}

static void test_paragraph_motions_find_blank_lines(void)
{
    //      0         1          2       3      4
    ed_set("to a\nend\n\nto b\nend\n");
    feed("}");   TEST_ASSERT_EQUAL_UINT(9, ed.cursor);    // The blank line
    feed("}");   TEST_ASSERT_EQUAL_UINT(19, ed.cursor);   // End of the buffer
    feed("{");   TEST_ASSERT_EQUAL_UINT(9, ed.cursor);
    feed("{");   TEST_ASSERT_EQUAL_UINT(0, ed.cursor);
}

static void test_find_char_within_the_line(void)
{
    //      0123456789
    ed_set("fd 100 rt\n");
    feed("f0");   TEST_ASSERT_EQUAL_UINT(4, ed.cursor);
    feed("f0");   TEST_ASSERT_EQUAL_UINT(5, ed.cursor);
    feed("Fd");   TEST_ASSERT_EQUAL_UINT(1, ed.cursor);
    feed("t0");   TEST_ASSERT_EQUAL_UINT(3, ed.cursor);
}

static void test_semicolon_and_comma_repeat_a_find(void)
{
    ed_set("a.b.c.d\n");
    feed("f.");   TEST_ASSERT_EQUAL_UINT(1, ed.cursor);
    feed(";");    TEST_ASSERT_EQUAL_UINT(3, ed.cursor);
    feed(";");    TEST_ASSERT_EQUAL_UINT(5, ed.cursor);
    feed(",");    TEST_ASSERT_EQUAL_UINT(3, ed.cursor);
}

static void test_a_find_that_misses_leaves_the_cursor_alone(void)
{
    ed_set("abc\nzzz\n");
    feed("fz");
    TEST_ASSERT_EQUAL_UINT(0, ed.cursor);
    TEST_ASSERT_EQUAL_INT(VI_ACT_BEEP, ed.last.kind);
}

static void test_percent_matches_its_own_bracket(void)
{
    //      0123456789012345
    ed_set("repeat 4 [fd 10]\n");
    feed("%");    TEST_ASSERT_EQUAL_UINT(15, ed.cursor);  // Looks along the line for the '[', matches it
    feed("%");    TEST_ASSERT_EQUAL_UINT(9, ed.cursor);
    feed("%");    TEST_ASSERT_EQUAL_UINT(15, ed.cursor);
}

// Reported from a board as a `d%` failure, and checked against vim 9.1, which
// does exactly this. `%` is "the next bracket at or after the cursor, then its
// match" -- it is not "the group I am standing inside". With the cursor on the
// `f`, the next bracket is the `]`, whose match is the `[` behind the cursor,
// so `d%` deletes backwards to it. Reaching the bracket first (`F[`) is what
// takes the whole group; vim needs `di[` for the other reading, and this mode
// has no text objects.
static void test_percent_takes_the_next_bracket_not_the_enclosing_group(void)
{
    ed_set("when [wifi?] [pr \"connected]\n");
    at(8);   // The `f` of `wifi?`
    feed("%");
    TEST_ASSERT_EQUAL_UINT(5, ed.cursor);   // The opening `[`

    at(8);
    feed("d%");
    assert_text("when i?] [pr \"connected]\n");

    // The same rule inside the second group: the first bracket at or after the
    // space is that group's `]`, so the motion runs back to its `[`
    ed_set("when [wifi?] [pr \"connected]\n");
    at(16);
    feed("d%");
    assert_text("when [wifi?] \"connected]\n");

    // From the bracket itself, which is how the whole group goes
    ed_set("when [wifi?] [pr \"connected]\n");
    at(8);
    feed("F[d%");
    assert_text("when  [pr \"connected]\n");
}

static void test_percent_counts_nesting(void)
{
    //      0123456789012345678901
    ed_set("[a [b] c]\n");
    at(0);
    feed("%");    TEST_ASSERT_EQUAL_UINT(8, ed.cursor);
    at(3);
    feed("%");    TEST_ASSERT_EQUAL_UINT(5, ed.cursor);
}

//
//  Operators over motions
//

static void test_d_over_every_motion(void)
{
    ed_set("to box\nfd 10\nend\n");   feed("dw");  assert_text("box\nfd 10\nend\n");
    ed_set("to box\nfd 10\nend\n");   feed("d$");  assert_text("\nfd 10\nend\n");
    ed_set("to box\nfd 10\nend\n");   at(3); feed("d0"); assert_text("box\nfd 10\nend\n");
    ed_set("to box\nfd 10\nend\n");   feed("de");  assert_text(" box\nfd 10\nend\n");
    ed_set("to box\nfd 10\nend\n");   at(6); feed("db"); assert_text("to \nfd 10\nend\n");
    ed_set("to box\nfd 10\nend\n");   feed("dj");  assert_text("end\n");
    ed_set("to box\nfd 10\nend\n");   at(7); feed("dk"); assert_text("end\n");
    ed_set("to box\nfd 10\nend\n");   feed("dG");  assert_text("");
    ed_set("to box\nfd 10\nend\n");   at(7); feed("dgg"); assert_text("end\n");
    ed_set("to box\nfd 10\nend\n");   feed("dfx"); assert_text("\nfd 10\nend\n");
    ed_set("[a [b] c] x\n");          feed("d%");  assert_text(" x\n");
}

static void test_c_over_a_motion_leaves_insert_mode(void)
{
    ed_set("to box\nend\n");
    feed("cw");
    assert_text(" box\nend\n");
    TEST_ASSERT_EQUAL_INT(VI_INSERT, ed.vi.mode);
    feed("proc");
    assert_text("proc box\nend\n");
    feed_key(KEY_ESC);
    TEST_ASSERT_EQUAL_INT(VI_NORMAL, ed.vi.mode);
}

static void test_cw_changes_to_the_end_of_the_word_not_the_next_one(void)
{
    ed_set("to box\nend\n");
    at(3);
    feed("cw");
    assert_text("to \nend\n");   // The space after "box" survives, as vi has it
}

static void test_y_leaves_the_buffer_alone_and_fills_the_copy_buffer(void)
{
    ed_set("to box\nend\n");
    feed("yw");
    assert_text("to box\nend\n");
    TEST_ASSERT_EQUAL_STRING("to ", ed.yank);
}

static void test_the_doubled_operators_are_linewise(void)
{
    ed_set("one\ntwo\nthree\n");   feed("dd");  assert_text("two\nthree\n");
    ed_set("one\ntwo\nthree\n");   feed("2dd"); assert_text("three\n");
    ed_set("one\ntwo\nthree\n");   feed("yy");  TEST_ASSERT_EQUAL_STRING("one\n", ed.yank);
    ed_set("one\ntwo\nthree\n");   feed("cc");  assert_text("\ntwo\nthree\n");
}

static void test_an_operator_count_multiplies_the_motion_count(void)
{
    ed_set("a b c d e f g\n");
    feed("2d3w");
    assert_text("g\n");
}

static void test_indent_operators_shift_by_a_tab_stop(void)
{
    ed_set("one\ntwo\nthree\n");
    feed(">>");
    assert_text("  one\ntwo\nthree\n");
    feed("<<");
    assert_text("one\ntwo\nthree\n");
    feed(">j");
    assert_text("  one\n  two\nthree\n");
}

//
//  Ranges that have to be exactly right
//

static void test_dd_on_the_last_line_takes_the_newline_before_it(void)
{
    ed_set("one\ntwo");
    at(4);
    feed("dd");
    assert_range(3, 7);          // Back over the newline that ends "one"
    assert_text("one");
}

static void test_dd_on_the_only_line_empties_the_buffer(void)
{
    ed_set("one");
    feed("dd");
    assert_text("");
}

static void test_yy_on_a_one_line_buffer_gains_a_newline(void)
{
    ed_set("one");
    feed("yy");
    TEST_ASSERT_EQUAL_STRING("one\n", ed.yank);   // Linewise text ends in one
}

static void test_dd_past_the_end_takes_what_is_left(void)
{
    ed_set("one\ntwo\n");
    feed("3dd");
    assert_text("");
}

static void test_a_degenerate_range_is_empty_never_inverted(void)
{
    ed_set("abc\n");
    at(0);
    feed("d0");                          // Column 0 already
    TEST_ASSERT_TRUE(ed.last.start <= ed.last.end);
    assert_range(0, 0);
    assert_text("abc\n");

    ed_set("abc");
    at(3);
    feed("dw");                          // Nothing left to the right
    TEST_ASSERT_TRUE(ed.last.start <= ed.last.end);
    assert_text("abc");
}

static void test_dw_stops_at_the_end_of_the_line(void)
{
    ed_set("to box\nfd 10\n");
    at(3);
    feed("dw");
    assert_text("to \nfd 10\n");   // Not pulling "fd 10" up onto the first line
}

static void test_operators_on_an_empty_line(void)
{
    ed_set("one\n\ntwo\n");
    at(4);
    feed("x");    assert_text("one\n\ntwo\n");   // Nothing on the line to take
    feed("D");    assert_text("one\n\ntwo\n");
    feed("dw");   assert_text("one\ntwo\n");     // `w` leaves the line, so the break goes
}

//
//  Single-key edits
//

static void test_x_and_X(void)
{
    ed_set("abcdef\n");
    at(2);
    feed("x");    assert_text("abdef\n");
    feed("2x");   assert_text("abf\n");
    feed("X");    assert_text("af\n");
}

static void test_x_stops_at_the_end_of_the_line(void)
{
    ed_set("ab\ncd\n");
    at(1);
    feed("9x");
    assert_text("a\ncd\n");
}

static void test_D_C_Y_S_and_s(void)
{
    ed_set("abcdef\nxyz\n");  at(3); feed("D");  assert_text("abc\nxyz\n");
    ed_set("abcdef\nxyz\n");  at(3); feed("C");  assert_text("abc\nxyz\n");
    TEST_ASSERT_EQUAL_INT(VI_INSERT, ed.vi.mode);
    feed_key(KEY_ESC);
    ed_set("abcdef\nxyz\n");  feed("Y");   TEST_ASSERT_EQUAL_STRING("abcdef\n", ed.yank);
    ed_set("  abc\nxyz\n");   feed("S");   assert_text("  \nxyz\n");
    feed_key(KEY_ESC);
    ed_set("abcdef\n");       feed("2s");  assert_text("cdef\n");
}

static void test_r_replaces_characters(void)
{
    ed_set("abcdef\n");
    feed("rz");   assert_text("zbcdef\n");
    at(1);
    feed("3r-");  assert_text("z---ef\n");
}

static void test_r_with_return_splits_the_line(void)
{
    // Vi's `r` takes Return as the character to put there, which is a split
    ed_set("abcdef\n");
    at(3);
    feed("r"); feed_key(KEY_RETURN);
    assert_text("abc\nef\n");
    TEST_ASSERT_EQUAL_INT(4, (int)ed.cursor);   // The first character of the new line

    // A count takes that many characters out and leaves one line break
    ed_set("abcdef\n");
    at(1);
    feed("3r"); feed_key(KEY_ENTER);
    assert_text("a\nef\n");
    TEST_ASSERT_EQUAL_INT(2, (int)ed.cursor);

    // And `.` repeats it, which is what recording the raw key buys: the split
    // leaves the cursor on `c`, so the repeat takes that one
    ed_set("abcd\n");
    at(1);
    feed("r"); feed_key(KEY_RETURN);
    assert_text("a\ncd\n");
    feed(".");
    assert_text("a\n\nd\n");
}

static void test_r_refuses_to_run_past_the_end_of_the_line(void)
{
    ed_set("ab\ncd\n");
    feed("9rz");
    assert_text("ab\ncd\n");
    TEST_ASSERT_EQUAL_INT(VI_ACT_BEEP, ed.last.kind);
}

static void test_J_joins_lines_with_one_space(void)
{
    ed_set("to box\n  fd 10\nend\n");
    feed("J");
    assert_text("to box fd 10\nend\n");   // The indentation goes with the break
    feed("J");
    assert_text("to box fd 10 end\n");
}

static void test_tilde_flips_case_and_moves_on(void)
{
    ed_set("abc DEF\n");
    feed("3~");
    assert_text("ABC DEF\n");
    TEST_ASSERT_EQUAL_UINT(3, ed.cursor);
}

static void test_p_and_P_are_linewise_after_a_linewise_yank(void)
{
    ed_set("one\ntwo\n");
    feed("yy");
    feed("p");    assert_text("one\none\ntwo\n");
    TEST_ASSERT_EQUAL_UINT(4, ed.cursor);
    feed("P");    assert_text("one\none\none\ntwo\n");
}

static void test_p_is_charwise_after_a_charwise_yank(void)
{
    ed_set("abcd\n");
    feed("2yl");           // Yank "ab"
    TEST_ASSERT_EQUAL_STRING("ab", ed.yank);
    feed("p");
    assert_text("aabbcd\n");
}

static void test_p_takes_a_count(void)
{
    ed_set("one\n");
    feed("yy");
    feed("3p");
    assert_text("one\none\none\none\n");
}

static void test_p_after_the_last_line_adds_the_newline_it_needs(void)
{
    ed_set("one\ntwo");
    feed("yy");            // "one\n"
    at(4);
    feed("p");
    assert_text("one\ntwo\none\n");
}

static void test_insert_entry_puts_the_cursor_in_the_right_place(void)
{
    ed_set("  abc\n");
    at(3);
    feed("i");  TEST_ASSERT_EQUAL_UINT(3, ed.cursor);
    feed_key(KEY_ESC);
    feed("a");  TEST_ASSERT_EQUAL_UINT(3, ed.cursor);
    feed_key(KEY_ESC);
    at(3);
    feed("I");  TEST_ASSERT_EQUAL_UINT(2, ed.cursor);
    feed_key(KEY_ESC);
    feed("A");  TEST_ASSERT_EQUAL_UINT(5, ed.cursor);
}

static void test_o_and_O_carry_the_indentation(void)
{
    ed_set("to box\n  fd 10\nend\n");
    at(9);
    feed("o");
    assert_text("to box\n  fd 10\n  \nend\n");
    TEST_ASSERT_EQUAL_UINT(17, ed.cursor);
    feed_key(KEY_ESC);
    at(9);
    feed("O");
    assert_text("to box\n  \n  fd 10\n  \nend\n");
}

//
//  Modes
//

static void test_esc_returns_to_normal_from_every_mode(void)
{
    ed_set("abc\n");
    feed("i");             TEST_ASSERT_EQUAL_INT(VI_INSERT, ed.vi.mode);
    feed_key(KEY_ESC);     TEST_ASSERT_EQUAL_INT(VI_NORMAL, ed.vi.mode);
    feed("v");             TEST_ASSERT_EQUAL_INT(VI_VISUAL, ed.vi.mode);
    feed_key(KEY_ESC);     TEST_ASSERT_EQUAL_INT(VI_NORMAL, ed.vi.mode);
    feed("V");             TEST_ASSERT_EQUAL_INT(VI_VISUAL_LINE, ed.vi.mode);
    feed_key(KEY_ESC);     TEST_ASSERT_EQUAL_INT(VI_NORMAL, ed.vi.mode);
    feed(":");             TEST_ASSERT_EQUAL_INT(VI_CMDLINE, ed.vi.mode);
    feed_key(KEY_ESC);     TEST_ASSERT_EQUAL_INT(VI_NORMAL, ed.vi.mode);
}

static void test_esc_clears_a_pending_count_and_operator(void)
{
    ed_set("one\ntwo\nthree\n");
    feed("12");
    feed_key(KEY_ESC);
    TEST_ASSERT_EQUAL_INT(0, ed.vi.count);
    feed("d");
    feed_key(KEY_ESC);
    TEST_ASSERT_EQUAL_INT(0, ed.vi.pending_op);
    feed("w");                       // Just a motion now, not the end of a delete
    assert_text("one\ntwo\nthree\n");
}

static void test_esc_in_insert_mode_steps_back_off_the_last_character(void)
{
    ed_set("abc\n");
    at(0);
    feed("a");
    feed("Z");
    assert_text("aZbc\n");
    feed_key(KEY_ESC);
    TEST_ASSERT_EQUAL_UINT(1, ed.cursor);
}

static void test_brk_is_never_consumed(void)
{
    ed_set("abc\n");
    feed_key(KEY_BREAK);   TEST_ASSERT_FALSE(ed.consumed);
    feed("i");
    feed_key(KEY_BREAK);   TEST_ASSERT_FALSE(ed.consumed);
    feed_key(KEY_ESC);
    feed(":");
    feed_key(KEY_BREAK);   TEST_ASSERT_FALSE(ed.consumed);
}

static void test_insert_mode_hands_ordinary_keys_back_to_the_editor(void)
{
    ed_set("");
    feed("i");
    feed_key(KEY_LEFT);
    TEST_ASSERT_FALSE(ed.consumed);
    feed_key('x');
    TEST_ASSERT_FALSE(ed.consumed);
    assert_text("x");
}

//
//  Visual mode
//

static void test_visual_mode_deletes_the_selection(void)
{
    ed_set("abcdef\n");
    feed("v2l");
    TEST_ASSERT_EQUAL_UINT(0, ed.vi.anchor);
    feed("d");
    assert_text("def\n");          // Charwise visual takes the character it is on
    TEST_ASSERT_EQUAL_INT(VI_NORMAL, ed.vi.mode);
}

static void test_visual_line_mode_is_linewise(void)
{
    ed_set("one\ntwo\nthree\n");
    feed("Vj");
    feed("d");
    assert_text("three\n");
}

static void test_visual_o_swaps_the_ends(void)
{
    ed_set("abcdef\n");
    at(2);
    feed("v");
    feed("2l");   TEST_ASSERT_EQUAL_UINT(4, ed.cursor);
    feed("o");    TEST_ASSERT_EQUAL_UINT(2, ed.cursor);
    TEST_ASSERT_EQUAL_UINT(4, ed.vi.anchor);
    feed("d");
    assert_text("abf\n");
}

static void test_visual_y_and_c_and_indent(void)
{
    ed_set("abcdef\n");   feed("vly");  assert_text("abcdef\n");
    TEST_ASSERT_EQUAL_STRING("ab", ed.yank);
    ed_set("abcdef\n");   feed("vlc");  assert_text("cdef\n");
    TEST_ASSERT_EQUAL_INT(VI_INSERT, ed.vi.mode);
    feed_key(KEY_ESC);
    ed_set("one\ntwo\n");  feed("Vj>"); assert_text("  one\n  two\n");
}

static void test_visual_selection_never_swallows_the_newline(void)
{
    ed_set("ab\ncd\n");
    feed("v9l");
    feed("d");
    assert_text("\ncd\n");
}

static void test_visual_p_replaces_the_selection(void)
{
    ed_set("abc def\n");
    feed("yw");                  // "abc "
    at(4);
    feed("v2lp");
    assert_text("abc abc \n");
}

//
//  `.`
//

static void test_dot_repeats_the_last_change_at_the_new_cursor(void)
{
    ed_set("aa bb cc dd\n");
    feed("dw");
    assert_text("bb cc dd\n");
    feed(".");
    assert_text("cc dd\n");
    feed(".");
    assert_text("dd\n");
}

static void test_dot_recomputes_its_own_motion(void)
{
    ed_set("aaaa bb\ncc\n");
    feed("dw");            // Deletes "aaaa "
    assert_text("bb\ncc\n");
    feed(".");             // A different span of bytes, the same command
    assert_text("\ncc\n");
}

static void test_dot_takes_a_new_count(void)
{
    ed_set("abcdefghij\n");
    feed("2x");
    assert_text("cdefghij\n");
    feed("3.");
    assert_text("fghij\n");
}

static void test_dot_with_nothing_recorded_complains(void)
{
    ed_set("abc\n");
    feed(".");
    TEST_ASSERT_EQUAL_INT(VI_ACT_BEEP, ed.last.kind);
    assert_text("abc\n");
}

static void test_a_motion_is_not_a_change_to_repeat(void)
{
    ed_set("aa bb cc\n");
    feed("x");             // The change
    feed("w");             // A motion; must not become what `.` repeats
    feed(".");
    assert_text("a b cc\n");
}

//
//  Ex commands
//

static void test_write_and_quit_commands(void)
{
    // `:w` writes without leaving; the editor decides what a write means
    ed_set("abc\n");
    feed(":w");  feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_INT(1, ed.writes);
    TEST_ASSERT_EQUAL_INT(0, ed.exits);

    setUp();  ed_set("abc\n");
    feed(":wq"); feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_INT(VI_ACT_ACCEPT, ed.exit_kind);

    setUp();  ed_set("abc\n");
    feed(":x");  feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_INT(VI_ACT_ACCEPT, ed.exit_kind);

    setUp();  ed_set("abc\n");
    feed(":q!"); feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_INT(VI_ACT_CANCEL, ed.exit_kind);

    setUp();  ed_set("abc\n");
    feed(":q");  feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_INT(VI_ACT_QUIT, ed.exit_kind);   // The editor checks `modified`
}

static void test_ZZ_and_ZQ(void)
{
    ed_set("abc\n");
    feed("ZZ");
    TEST_ASSERT_EQUAL_INT(VI_ACT_ACCEPT, ed.exit_kind);

    setUp();  ed_set("abc\n");
    feed("ZQ");
    TEST_ASSERT_EQUAL_INT(VI_ACT_CANCEL, ed.exit_kind);

    setUp();  ed_set("abc\n");
    feed("Zx");
    TEST_ASSERT_EQUAL_INT(VI_ACT_BEEP, ed.last.kind);
}

static void test_colon_number_goes_to_a_line(void)
{
    ed_set("one\n  two\nthree\n");
    feed(":2"); feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_UINT(6, ed.cursor);
    feed(":999"); feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_UINT(16, ed.cursor);   // Clamped to the last line
}

static void test_substitute_on_the_current_line(void)
{
    ed_set("aa aa\naa aa\n");
    feed(":s/aa/bb/"); feed_key(KEY_RETURN);
    assert_text("bb aa\naa aa\n");
}

static void test_substitute_with_g_takes_every_match_on_the_line(void)
{
    ed_set("aa aa\naa aa\n");
    feed(":s/aa/bb/g"); feed_key(KEY_RETURN);
    assert_text("bb bb\naa aa\n");
}

static void test_substitute_over_the_whole_buffer(void)
{
    ed_set("aa aa\naa aa\n");
    feed(":%s/aa/bb/"); feed_key(KEY_RETURN);
    assert_text("bb aa\nbb aa\n");

    ed_set("aa aa\naa aa\n");
    feed(":%s/aa/bb/g"); feed_key(KEY_RETURN);
    assert_text("bb bb\nbb bb\n");
}

static void test_a_malformed_ex_command_complains_and_changes_nothing(void)
{
    const char *bad[] = { ":s", ":s/a", ":w junk", ":zz", ":%q", ":%s/a" };

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        setUp();
        ed_set("aa bb\n");
        feed(bad[i]);
        feed_key(KEY_RETURN);
        TEST_ASSERT_EQUAL_INT_MESSAGE(VI_ACT_BEEP, ed.last.kind, bad[i]);
        assert_text("aa bb\n");
        TEST_ASSERT_EQUAL_INT(VI_NORMAL, ed.vi.mode);
    }
}

static void test_backspacing_off_the_colon_leaves_the_command_line(void)
{
    ed_set("abc\n");
    feed(":w");
    feed_key(KEY_BACKSPACE);
    feed_key(KEY_BACKSPACE);
    TEST_ASSERT_EQUAL_INT(VI_NORMAL, ed.vi.mode);
    feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_INT(0, ed.exits);
}

static void test_the_command_line_stops_growing_when_it_is_full(void)
{
    ed_set("abc\n");
    feed(":");
    for (int i = 0; i < LOGO_VI_CMDLINE_MAX + 20; i++) feed("a");
    TEST_ASSERT_EQUAL_UINT(LOGO_VI_CMDLINE_MAX, ed.vi.cmdline_len);
    TEST_ASSERT_EQUAL_UINT(LOGO_VI_CMDLINE_MAX, strlen(ed.vi.cmdline));
}

//
//  Search
//

static void test_slash_records_a_pattern_and_a_direction(void)
{
    ed_set("one two\n");
    feed("/two"); feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_INT(VI_ACT_SEARCH, ed.last.kind);
    TEST_ASSERT_EQUAL_CHAR('/', ed.last.ch);
    TEST_ASSERT_EQUAL_STRING("two", ed.vi.pattern);

    feed("n");   TEST_ASSERT_EQUAL_CHAR('/', ed.last.ch);
    feed("N");   TEST_ASSERT_EQUAL_CHAR('?', ed.last.ch);

    feed("?one"); feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_CHAR('?', ed.last.ch);
    feed("n");   TEST_ASSERT_EQUAL_CHAR('?', ed.last.ch);
    feed("N");   TEST_ASSERT_EQUAL_CHAR('/', ed.last.ch);
}

static void test_search_without_a_pattern_complains(void)
{
    ed_set("one\n");
    feed("n");
    TEST_ASSERT_EQUAL_INT(VI_ACT_BEEP, ed.last.kind);
    feed("/"); feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_INT(VI_ACT_BEEP, ed.last.kind);
}

static void test_an_empty_search_repeats_the_last_pattern(void)
{
    ed_set("one two\n");
    feed("/two"); feed_key(KEY_RETURN);
    feed("/");    feed_key(KEY_RETURN);
    TEST_ASSERT_EQUAL_INT(VI_ACT_SEARCH, ed.last.kind);
    TEST_ASSERT_EQUAL_STRING("two", ed.vi.pattern);
}

//
//  editor_vi_substitute
//

static size_t substitute(char *buf, const char *pat, const char *rep, bool global,
                         size_t start, size_t end, size_t capacity)
{
    size_t len = strlen(buf);
    size_t cursor = 0;
    return editor_vi_substitute(buf, &len, capacity, start, end,
                                pat, strlen(pat), rep, strlen(rep), global, NULL, &cursor);
}

static void test_substitute_matches_case_insensitively(void)
{
    char buf[64] = "Foo foo FOO\n";
    TEST_ASSERT_EQUAL_UINT(3, substitute(buf, "foo", "x", true, 0, 12, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("x x x\n", buf);
}

static void test_substitute_can_grow_and_shrink_the_text(void)
{
    char grow[64] = "a\na\n";
    TEST_ASSERT_EQUAL_UINT(2, substitute(grow, "a", "long", true, 0, 4, sizeof(grow)));
    TEST_ASSERT_EQUAL_STRING("long\nlong\n", grow);

    char shrink[64] = "long\nlong\n";
    TEST_ASSERT_EQUAL_UINT(2, substitute(shrink, "long", "a", true, 0, 10, sizeof(shrink)));
    TEST_ASSERT_EQUAL_STRING("a\na\n", shrink);
}

static void test_substitute_deleting_the_pattern(void)
{
    char buf[64] = "axbxc\n";
    TEST_ASSERT_EQUAL_UINT(2, substitute(buf, "x", "", true, 0, 6, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("abc\n", buf);
}

static void test_substitute_leaves_the_text_alone_when_it_would_not_fit(void)
{
    char buf[64] = "aaaa\n";
    size_t len = strlen(buf);
    size_t cursor = 0;
    // Ten bytes for each of four matches cannot fit in a capacity of 12
    TEST_ASSERT_EQUAL_UINT(0, editor_vi_substitute(buf, &len, 12, 0, len,
                                                   "a", 1, "0123456789", 10, true, NULL,
                                                   &cursor));
    TEST_ASSERT_EQUAL_STRING("aaaa\n", buf);
    TEST_ASSERT_EQUAL_UINT(5, len);
}

static void test_substitute_without_a_match_changes_nothing(void)
{
    char buf[64] = "abc\n";
    TEST_ASSERT_EQUAL_UINT(0, substitute(buf, "zz", "y", true, 0, 4, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("abc\n", buf);
}

static void test_substitute_replacement_containing_the_pattern_is_not_rematched(void)
{
    char buf[64] = "a\n";
    TEST_ASSERT_EQUAL_UINT(1, substitute(buf, "a", "aa", true, 0, 2, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("aa\n", buf);
}

static void test_substitute_reports_the_last_line_it_changed(void)
{
    char buf[64] = "a\nb\na\n";
    size_t len = strlen(buf);
    size_t cursor = 0;
    TEST_ASSERT_EQUAL_UINT(2, editor_vi_substitute(buf, &len, sizeof(buf), 0, len,
                                                   "a", 1, "z", 1, true, NULL, &cursor));
    TEST_ASSERT_EQUAL_UINT(4, cursor);
}

//
//  The randomised differential run
//
//  Thousands of random commands against a buffer whose line memo is driven
//  exactly as editor.c drives it. Nothing here predicts what the text should
//  become; it asserts what must hold after every single step.
//

static int naive_line_at(const char *buf, size_t len, size_t pos)
{
    int line = 0;
    for (size_t i = 0; i < pos && i < len; i++) {
        if (buf[i] == '\n') line++;
    }
    return line;
}

static size_t naive_line_start(const char *buf, size_t len, int line)
{
    if (line <= 0) return 0;
    int seen = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\n' && ++seen == line) return i + 1;
    }
    return len;
}

//
//  Undo and redo
//

static void test_u_reverses_a_change_and_ctrl_r_puts_it_back(void)
{
    ed_set("print [a b c]\n");
    at(6);
    feed("D");
    assert_text("print \n");

    feed("u");
    assert_text("print [a b c]\n");
    TEST_ASSERT_EQUAL_UINT(6, ed.cursor);

    feed_key(0x12);   // Ctrl+R
    assert_text("print \n");
}

static void test_u_reverses_one_command_at_a_time(void)
{
    ed_set("abc\n");
    feed("xxx");
    assert_text("\n");

    feed("u");   assert_text("c\n");
    feed("u");   assert_text("bc\n");
    feed("u");   assert_text("abc\n");
    feed("u");   assert_text("abc\n");   // Nothing left
    TEST_ASSERT_EQUAL_INT(VI_ACT_UNDO, ed.last.kind);
}

static void test_u_takes_a_count(void)
{
    ed_set("abcde\n");
    feed("xxxx");
    assert_text("e\n");

    feed("3u");
    assert_text("bcde\n");
    feed("2");
    feed_key(0x12);   // The two deletions just reversed, put back
    assert_text("de\n");
}

static void test_an_insert_session_is_one_undo(void)
{
    ed_set("fd\n");
    at(2);
    feed("a");            // Insert mode
    feed(" 100");
    feed_key(KEY_ESC);
    assert_text("fd 100\n");

    feed("u");
    assert_text("fd\n");
}

static void test_a_change_command_and_what_was_typed_undo_together(void)
{
    ed_set("print hello\n");
    at(6);
    feed("cw");
    feed("bye");
    feed_key(KEY_ESC);
    assert_text("print bye\n");

    feed("u");
    assert_text("print hello\n");
}

static void test_a_linewise_operator_undoes_every_line_it_touched(void)
{
    ed_set("a\nb\nc\nd\n");
    feed("3dd");
    assert_text("d\n");

    feed("u");
    assert_text("a\nb\nc\nd\n");
}

static void test_indenting_a_block_is_one_undo(void)
{
    ed_set("a\nb\nc\n");
    feed("Vj>");
    assert_text("  a\n  b\nc\n");

    feed("u");
    assert_text("a\nb\nc\n");
}

static void test_a_substitute_over_the_buffer_is_one_undo(void)
{
    ed_set("fd 10\nfd 20\nfd 30\n");
    feed(":%s/fd/bk/");
    feed_key(KEY_RETURN);
    assert_text("bk 10\nbk 20\nbk 30\n");

    feed("u");
    assert_text("fd 10\nfd 20\nfd 30\n");

    feed_key(0x12);
    assert_text("bk 10\nbk 20\nbk 30\n");
}

static void test_a_new_change_after_an_undo_drops_the_redo(void)
{
    ed_set("abc\n");
    feed("x");
    feed("u");
    assert_text("abc\n");

    feed("x");
    feed_key(0x12);   // Nothing to put back
    assert_text("bc\n");
}

static void test_undo_with_nothing_recorded_says_so(void)
{
    ed_set("abc\n");
    feed("u");
    TEST_ASSERT_EQUAL_INT(VI_ACT_UNDO, ed.last.kind);
    assert_text("abc\n");
}

static void test_undo_leaves_visual_mode(void)
{
    ed_set("abcdef\n");
    feed("x");
    feed("vll");
    TEST_ASSERT_EQUAL_INT(VI_VISUAL, ed.vi.mode);

    feed("u");
    TEST_ASSERT_EQUAL_INT(VI_NORMAL, ed.vi.mode);
    assert_text("abcdef\n");
}

static void test_random_commands_keep_the_buffer_and_the_memo_consistent(void)
{
    static const char *keys =
        "hjklwbeWBE0^$GxXDCYSspPJ~ivVoOaAircdy<>.;,%nfFtT{}23uu\x12";

    srand(20260817);

    for (int round = 0; round < 60; round++) {
        setUp();
        ed_set("to box :size\n  repeat 4 [fd :size rt 90]\n\nend\n\nprint [a b c]\n");

        for (int step = 0; step < 400; step++) {
            int key = keys[rand() % (int)strlen(keys)];

            // Never leave the editor, and never sit in a mode the fuzz cannot
            // get out of: Esc every so often, and Return to close a command line
            if (ed.vi.mode == VI_CMDLINE && (rand() % 4) == 0) {
                feed_key(KEY_RETURN);
            } else if ((rand() % 8) == 0) {
                feed_key(KEY_ESC);
            } else if (key == 'Z' || key == ':' || key == '/' || key == '?') {
                feed_key('l');
            } else {
                feed_key(key);
            }

            TEST_ASSERT_TRUE_MESSAGE(ed.cursor <= ed.len, "cursor left the buffer");
            TEST_ASSERT_TRUE_MESSAGE(ed.len < ED_CAP, "buffer overran");
            TEST_ASSERT_EQUAL_CHAR_MESSAGE('\0', ed.buf[ed.len], "buffer lost its NUL");
            TEST_ASSERT_TRUE_MESSAGE(ed.last.start <= ed.last.end, "inverted range");
            TEST_ASSERT_TRUE_MESSAGE(ed.last.end <= ed.len_at_key, "range past the end");
            TEST_ASSERT_EQUAL_INT(0, ed.exits);

            // The memo has to agree with a count from the start of the buffer;
            // this is what catches a mutation that skipped editor_lines_edit
            int line = editor_lines_at_pos(&ed.ix, ed.buf, ed.len, ed.cursor);
            TEST_ASSERT_EQUAL_INT_MESSAGE(naive_line_at(ed.buf, ed.len, ed.cursor), line,
                                          "the line memo drifted");
            TEST_ASSERT_EQUAL_UINT_MESSAGE(naive_line_start(ed.buf, ed.len, line),
                                           editor_lines_start(&ed.ix, ed.buf, ed.len, line),
                                           "the line memo drifted");
        }

        // Whatever those four hundred commands did, undoing all of it has to
        // give back the text the round started with -- exactly. This is what
        // catches a change the journal was not told about, the same way the
        // memo check above catches one editor_lines_edit did not see.
        feed_key(KEY_ESC);
        feed_key(KEY_ESC);
        for (int i = 0; i < 500 && ed.undo.has_last; i++) {
            feed_key('u');
        }
        TEST_ASSERT_EQUAL_STRING_MESSAGE(
            "to box :size\n  repeat 4 [fd :size rt 90]\n\nend\n\nprint [a b c]\n",
            ed.buf, "undo did not give back the text the round started with");
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_hjkl_move_by_one);
    RUN_TEST(test_a_motion_takes_a_count);
    RUN_TEST(test_h_and_l_stop_at_the_ends_of_the_line);
    RUN_TEST(test_j_and_k_keep_the_column_where_the_line_is_long_enough);
    RUN_TEST(test_j_and_k_stop_at_the_ends_of_the_buffer);
    RUN_TEST(test_zero_caret_and_dollar);
    RUN_TEST(test_zero_is_a_motion_until_there_is_a_count_to_extend);
    RUN_TEST(test_word_motions_stop_at_punctuation);
    RUN_TEST(test_big_word_motions_only_stop_at_blanks);
    RUN_TEST(test_e_lands_on_the_last_character_of_a_word);
    RUN_TEST(test_a_word_motion_crosses_lines);
    RUN_TEST(test_gg_and_G);
    RUN_TEST(test_G_past_the_last_line_clamps);
    RUN_TEST(test_paragraph_motions_find_blank_lines);
    RUN_TEST(test_find_char_within_the_line);
    RUN_TEST(test_semicolon_and_comma_repeat_a_find);
    RUN_TEST(test_a_find_that_misses_leaves_the_cursor_alone);
    RUN_TEST(test_percent_matches_its_own_bracket);
    RUN_TEST(test_percent_counts_nesting);
    RUN_TEST(test_percent_takes_the_next_bracket_not_the_enclosing_group);

    RUN_TEST(test_d_over_every_motion);
    RUN_TEST(test_c_over_a_motion_leaves_insert_mode);
    RUN_TEST(test_cw_changes_to_the_end_of_the_word_not_the_next_one);
    RUN_TEST(test_y_leaves_the_buffer_alone_and_fills_the_copy_buffer);
    RUN_TEST(test_the_doubled_operators_are_linewise);
    RUN_TEST(test_an_operator_count_multiplies_the_motion_count);
    RUN_TEST(test_indent_operators_shift_by_a_tab_stop);

    RUN_TEST(test_dd_on_the_last_line_takes_the_newline_before_it);
    RUN_TEST(test_dd_on_the_only_line_empties_the_buffer);
    RUN_TEST(test_yy_on_a_one_line_buffer_gains_a_newline);
    RUN_TEST(test_dd_past_the_end_takes_what_is_left);
    RUN_TEST(test_a_degenerate_range_is_empty_never_inverted);
    RUN_TEST(test_dw_stops_at_the_end_of_the_line);
    RUN_TEST(test_operators_on_an_empty_line);

    RUN_TEST(test_x_and_X);
    RUN_TEST(test_x_stops_at_the_end_of_the_line);
    RUN_TEST(test_D_C_Y_S_and_s);
    RUN_TEST(test_r_replaces_characters);
    RUN_TEST(test_r_with_return_splits_the_line);
    RUN_TEST(test_r_refuses_to_run_past_the_end_of_the_line);
    RUN_TEST(test_J_joins_lines_with_one_space);
    RUN_TEST(test_tilde_flips_case_and_moves_on);
    RUN_TEST(test_p_and_P_are_linewise_after_a_linewise_yank);
    RUN_TEST(test_p_is_charwise_after_a_charwise_yank);
    RUN_TEST(test_p_takes_a_count);
    RUN_TEST(test_p_after_the_last_line_adds_the_newline_it_needs);
    RUN_TEST(test_insert_entry_puts_the_cursor_in_the_right_place);
    RUN_TEST(test_o_and_O_carry_the_indentation);

    RUN_TEST(test_esc_returns_to_normal_from_every_mode);
    RUN_TEST(test_esc_clears_a_pending_count_and_operator);
    RUN_TEST(test_esc_in_insert_mode_steps_back_off_the_last_character);
    RUN_TEST(test_brk_is_never_consumed);
    RUN_TEST(test_insert_mode_hands_ordinary_keys_back_to_the_editor);

    RUN_TEST(test_visual_mode_deletes_the_selection);
    RUN_TEST(test_visual_line_mode_is_linewise);
    RUN_TEST(test_visual_o_swaps_the_ends);
    RUN_TEST(test_visual_y_and_c_and_indent);
    RUN_TEST(test_visual_selection_never_swallows_the_newline);
    RUN_TEST(test_visual_p_replaces_the_selection);

    RUN_TEST(test_dot_repeats_the_last_change_at_the_new_cursor);
    RUN_TEST(test_dot_recomputes_its_own_motion);
    RUN_TEST(test_dot_takes_a_new_count);
    RUN_TEST(test_dot_with_nothing_recorded_complains);
    RUN_TEST(test_a_motion_is_not_a_change_to_repeat);

    RUN_TEST(test_write_and_quit_commands);
    RUN_TEST(test_ZZ_and_ZQ);
    RUN_TEST(test_colon_number_goes_to_a_line);
    RUN_TEST(test_substitute_on_the_current_line);
    RUN_TEST(test_substitute_with_g_takes_every_match_on_the_line);
    RUN_TEST(test_substitute_over_the_whole_buffer);
    RUN_TEST(test_a_malformed_ex_command_complains_and_changes_nothing);
    RUN_TEST(test_backspacing_off_the_colon_leaves_the_command_line);
    RUN_TEST(test_the_command_line_stops_growing_when_it_is_full);

    RUN_TEST(test_slash_records_a_pattern_and_a_direction);
    RUN_TEST(test_search_without_a_pattern_complains);
    RUN_TEST(test_an_empty_search_repeats_the_last_pattern);

    RUN_TEST(test_substitute_matches_case_insensitively);
    RUN_TEST(test_substitute_can_grow_and_shrink_the_text);
    RUN_TEST(test_substitute_deleting_the_pattern);
    RUN_TEST(test_substitute_leaves_the_text_alone_when_it_would_not_fit);
    RUN_TEST(test_substitute_without_a_match_changes_nothing);
    RUN_TEST(test_substitute_replacement_containing_the_pattern_is_not_rematched);
    RUN_TEST(test_substitute_reports_the_last_line_it_changed);

    RUN_TEST(test_u_reverses_a_change_and_ctrl_r_puts_it_back);
    RUN_TEST(test_u_reverses_one_command_at_a_time);
    RUN_TEST(test_u_takes_a_count);
    RUN_TEST(test_an_insert_session_is_one_undo);
    RUN_TEST(test_a_change_command_and_what_was_typed_undo_together);
    RUN_TEST(test_a_linewise_operator_undoes_every_line_it_touched);
    RUN_TEST(test_indenting_a_block_is_one_undo);
    RUN_TEST(test_a_substitute_over_the_buffer_is_one_undo);
    RUN_TEST(test_a_new_change_after_an_undo_drops_the_redo);
    RUN_TEST(test_undo_with_nothing_recorded_says_so);
    RUN_TEST(test_undo_leaves_visual_mode);

    RUN_TEST(test_random_commands_keep_the_buffer_and_the_memo_consistent);

    return UNITY_END();
}
