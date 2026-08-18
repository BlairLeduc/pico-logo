//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the editor's memoised line lookup
//  (devices/picocalc/editor_lines.c). The memo is only an optimisation, so
//  every test checks it against the obvious count-from-the-start answer: the
//  two must agree whatever order lines are asked for, and whatever the editor
//  does to the buffer in between.
//

#include "unity.h"
#include "editor_lines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EditorLineIndex ix;

void setUp(void)
{
    editor_lines_reset(&ix);
}

void tearDown(void) {}

//
// Reference implementations: count from the start of the buffer every time,
// which is what the editor did before the memo existed.
//
static size_t naive_start(const char *buf, size_t len, int line)
{
    if (line <= 0) return 0;

    int seen = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            seen++;
            if (seen == line) return i + 1;
        }
    }
    return len;
}

static int naive_at_pos(const char *buf, size_t len, size_t pos)
{
    int line = 0;
    for (size_t i = 0; i < pos && i < len; i++) {
        if (buf[i] == '\n') line++;
    }
    return line;
}

// Ask for every line and every position, in the given order, and compare
static void check_all_lines(const char *buf, int extra_lines)
{
    size_t len = strlen(buf);
    int lines = naive_at_pos(buf, len, len) + 1;

    for (int line = 0; line < lines + extra_lines; line++) {
        TEST_ASSERT_EQUAL_UINT64(naive_start(buf, len, line),
                                 editor_lines_start(&ix, buf, len, line));
    }
    for (int line = lines + extra_lines - 1; line >= 0; line--) {
        TEST_ASSERT_EQUAL_UINT64(naive_start(buf, len, line),
                                 editor_lines_start(&ix, buf, len, line));
    }
}

static void check_all_positions(const char *buf)
{
    size_t len = strlen(buf);

    for (size_t pos = 0; pos <= len; pos++) {
        TEST_ASSERT_EQUAL_INT(naive_at_pos(buf, len, pos),
                              editor_lines_at_pos(&ix, buf, len, pos));
    }
    for (size_t pos = len + 1; pos-- > 0;) {
        TEST_ASSERT_EQUAL_INT(naive_at_pos(buf, len, pos),
                              editor_lines_at_pos(&ix, buf, len, pos));
    }
}

void test_empty_buffer_has_one_line_at_zero(void)
{
    TEST_ASSERT_EQUAL_UINT64(0, editor_lines_start(&ix, "", 0, 0));
    TEST_ASSERT_EQUAL_UINT64(0, editor_lines_start(&ix, "", 0, 1));
    TEST_ASSERT_EQUAL_INT(0, editor_lines_at_pos(&ix, "", 0, 0));
}

void test_line_starts_match_a_count_from_the_start(void)
{
    check_all_lines("to square :size\n  repeat 4 [fd :size rt 90]\nend\n", 3);
}

void test_positions_match_a_count_from_the_start(void)
{
    check_all_positions("to square :size\n  repeat 4 [fd :size rt 90]\nend\n");
}

void test_a_newline_belongs_to_the_line_it_ends(void)
{
    const char *buf = "ab\ncd";
    TEST_ASSERT_EQUAL_INT(0, editor_lines_at_pos(&ix, buf, 5, 2));  // the '\n'
    TEST_ASSERT_EQUAL_INT(1, editor_lines_at_pos(&ix, buf, 5, 3));  // 'c'
}

void test_the_line_after_a_trailing_newline_starts_at_the_end(void)
{
    // "abc\n" is two lines: the second is empty and starts past the newline
    TEST_ASSERT_EQUAL_UINT64(4, editor_lines_start(&ix, "abc\n", 4, 1));
    TEST_ASSERT_EQUAL_UINT64(4, editor_lines_start(&ix, "abc\n", 4, 2));
}

void test_empty_lines_are_counted(void)
{
    check_all_lines("\n\n\na\n\n", 2);
    check_all_positions("\n\n\na\n\n");
}

void test_buffer_without_a_trailing_newline(void)
{
    check_all_lines("one\ntwo\nthree", 2);
    check_all_positions("one\ntwo\nthree");
}

void test_jumping_between_distant_lines(void)
{
    static char buf[4096];
    size_t len = 0;
    for (int i = 0; i < 200; i++) {
        len += (size_t)sprintf(buf + len, "line %d\n", i);
    }

    // Deliberately out of order, forwards and backwards over long distances
    const int order[] = {0, 199, 1, 150, 3, 199, 200, 50, 199, 0, 100};
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        TEST_ASSERT_EQUAL_UINT64(naive_start(buf, len, order[i]),
                                 editor_lines_start(&ix, buf, len, order[i]));
        size_t pos = naive_start(buf, len, order[i]);
        TEST_ASSERT_EQUAL_INT(naive_at_pos(buf, len, pos),
                              editor_lines_at_pos(&ix, buf, len, pos));
    }
}

//
// The memo survives an edit that lands at or after the line it remembers, and
// must be dropped by one that lands before it. These tests do the edit the way
// the editor does, then ask for lines again.
//

void test_an_edit_after_the_memo_keeps_it_valid(void)
{
    char buf[64] = "one\ntwo\nthree\n";
    size_t len = strlen(buf);

    TEST_ASSERT_EQUAL_UINT64(4, editor_lines_start(&ix, buf, len, 1));  // memo on line 1

    // Insert 'X' at the end (after the memo)
    editor_lines_edit(&ix, len);
    buf[len++] = 'X';
    buf[len] = '\0';

    check_all_lines(buf, 2);
}

void test_an_edit_before_the_memo_drops_it(void)
{
    char buf[64] = "one\ntwo\nthree\n";
    size_t len = strlen(buf);

    TEST_ASSERT_EQUAL_UINT64(8, editor_lines_start(&ix, buf, len, 2));  // memo on line 2

    // Delete the first newline: every later line moves back one, and there is
    // one fewer of them
    editor_lines_edit(&ix, 3);
    memmove(buf + 3, buf + 4, len - 4 + 1);
    len--;

    // Ask for the memoised line first — a memo that survived would answer 8
    TEST_ASSERT_EQUAL_UINT64(len, editor_lines_start(&ix, buf, len, 2));
    TEST_ASSERT_EQUAL_INT(1, editor_lines_at_pos(&ix, buf, len, 8));

    check_all_lines(buf, 2);
    check_all_positions(buf);
}

void test_inserting_a_newline_at_the_memo_keeps_it_valid(void)
{
    char buf[64] = "one\ntwo\nthree\n";
    size_t len = strlen(buf);

    size_t line2 = editor_lines_start(&ix, buf, len, 2);
    TEST_ASSERT_EQUAL_UINT64(8, line2);

    // Insert a newline exactly at the start of the memoised line: line 2 is now
    // empty but still starts at the same place
    editor_lines_edit(&ix, line2);
    memmove(buf + line2 + 1, buf + line2, len - line2 + 1);
    buf[line2] = '\n';
    len++;

    check_all_lines(buf, 2);
    check_all_positions(buf);
}

void test_a_shrinking_buffer_leaves_no_stale_memo(void)
{
    char buf[64] = "one\ntwo\nthree\n";

    TEST_ASSERT_EQUAL_UINT64(8, editor_lines_start(&ix, buf, strlen(buf), 2));

    // Truncate past the memoised line without telling the index (what a
    // wholesale rewrite looks like if a reset is ever missed)
    buf[3] = '\0';

    check_all_lines(buf, 2);
    check_all_positions(buf);
}

//
// Random edit sequences: the memo must never disagree with a count from the
// start, whatever mix of inserts, deletes and lookups the editor performs.
//
void test_random_edits_and_lookups_agree_with_a_count(void)
{
    static char buf[2048];
    size_t len = 0;
    srand(20260817);

    for (int step = 0; step < 20000; step++) {
        int action = rand() % 4;

        if (action == 0 && len + 1 < sizeof(buf)) {
            size_t pos = (size_t)rand() % (len + 1);
            char c = (rand() % 5 == 0) ? '\n' : 'a';
            editor_lines_edit(&ix, pos);
            memmove(buf + pos + 1, buf + pos, len - pos);
            buf[pos] = c;
            len++;
            buf[len] = '\0';
        } else if (action == 1 && len > 0) {
            size_t pos = (size_t)rand() % len;
            editor_lines_edit(&ix, pos);
            memmove(buf + pos, buf + pos + 1, len - pos - 1);
            len--;
            buf[len] = '\0';
        } else if (action == 2) {
            int lines = naive_at_pos(buf, len, len) + 1;
            int line = rand() % (lines + 2);
            TEST_ASSERT_EQUAL_UINT64(naive_start(buf, len, line),
                                     editor_lines_start(&ix, buf, len, line));
        } else {
            size_t pos = (size_t)rand() % (len + 1);
            TEST_ASSERT_EQUAL_INT(naive_at_pos(buf, len, pos),
                                  editor_lines_at_pos(&ix, buf, len, pos));
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_buffer_has_one_line_at_zero);
    RUN_TEST(test_line_starts_match_a_count_from_the_start);
    RUN_TEST(test_positions_match_a_count_from_the_start);
    RUN_TEST(test_a_newline_belongs_to_the_line_it_ends);
    RUN_TEST(test_the_line_after_a_trailing_newline_starts_at_the_end);
    RUN_TEST(test_empty_lines_are_counted);
    RUN_TEST(test_buffer_without_a_trailing_newline);
    RUN_TEST(test_jumping_between_distant_lines);
    RUN_TEST(test_an_edit_after_the_memo_keeps_it_valid);
    RUN_TEST(test_an_edit_before_the_memo_drops_it);
    RUN_TEST(test_inserting_a_newline_at_the_memo_keeps_it_valid);
    RUN_TEST(test_a_shrinking_buffer_leaves_no_stale_memo);
    RUN_TEST(test_random_edits_and_lookups_agree_with_a_count);
    return UNITY_END();
}
