//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the editor's undo journal (devices/picocalc/editor_undo.c).
//
//  The journal never sees a key or a screen: it is told what a change did and
//  asked to reverse it. So the tests below make changes to a buffer the way the
//  editor does -- record first, then move the bytes -- and assert on the text
//  that comes back.
//

#include "unity.h"
#include "editor_undo.h"

#include <stdlib.h>
#include <string.h>

#define BUF_CAP   256
#define STORE_CAP 4096

static char buf[BUF_CAP];
static size_t len;
static char store[STORE_CAP];
static EditorUndo undo;

void setUp(void)
{
    memset(buf, 0, sizeof(buf));
    len = 0;
    editor_undo_init(&undo, store, sizeof(store));
}

void tearDown(void) {}

static void set_text(const char *text)
{
    strcpy(buf, text);
    len = strlen(text);
}

// Insert and delete exactly as the editor does: tell the journal, then move the
// bytes. Neither goes through the journal itself.
static void ed_insert(size_t pos, const char *text)
{
    size_t n = strlen(text);
    editor_undo_record(&undo, pos, &buf[pos], 0, text, n);
    memmove(&buf[pos + n], &buf[pos], len - pos);
    memcpy(&buf[pos], text, n);
    len += n;
    buf[len] = '\0';
}

static void ed_delete(size_t pos, size_t n)
{
    editor_undo_record(&undo, pos, &buf[pos], n, NULL, 0);
    memmove(&buf[pos], &buf[pos + n], len - pos - n);
    len -= n;
    buf[len] = '\0';
}

// An overwrite, which is what `r` and `~` are: both sides the same length
static void ed_overwrite(size_t pos, const char *text)
{
    size_t n = strlen(text);
    editor_undo_record(&undo, pos, &buf[pos], n, text, n);
    memcpy(&buf[pos], text, n);
}

static bool undo_step(void)
{
    size_t at;
    return editor_undo_undo(&undo, buf, &len, BUF_CAP, &at);
}

static bool redo_step(void)
{
    size_t at;
    return editor_undo_redo(&undo, buf, &len, BUF_CAP, &at);
}

//
//  One change at a time
//

static void test_an_insert_is_reversed_and_repeated(void)
{
    set_text("fd 100");

    editor_undo_begin(&undo);
    ed_insert(3, "to ");
    TEST_ASSERT_EQUAL_STRING("fd to 100", buf);

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("fd 100", buf);
    TEST_ASSERT_EQUAL_UINT(6, len);

    TEST_ASSERT_TRUE(redo_step());
    TEST_ASSERT_EQUAL_STRING("fd to 100", buf);
    TEST_ASSERT_EQUAL_UINT(9, len);
}

static void test_a_delete_is_reversed_and_repeated(void)
{
    set_text("print [a b c]");

    editor_undo_begin(&undo);
    ed_delete(6, 7);
    TEST_ASSERT_EQUAL_STRING("print ", buf);

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("print [a b c]", buf);

    TEST_ASSERT_TRUE(redo_step());
    TEST_ASSERT_EQUAL_STRING("print ", buf);
}

static void test_an_overwrite_carries_both_sides(void)
{
    set_text("abcd");

    editor_undo_begin(&undo);
    ed_overwrite(1, "XY");
    TEST_ASSERT_EQUAL_STRING("aXYd", buf);

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("abcd", buf);

    TEST_ASSERT_TRUE(redo_step());
    TEST_ASSERT_EQUAL_STRING("aXYd", buf);
}

static void test_the_position_reported_is_the_earliest_one_touched(void)
{
    set_text("one two three");

    editor_undo_begin(&undo);
    ed_delete(8, 5);   // "three"
    ed_delete(4, 3);   // "two", earlier in the buffer

    size_t at = 999;
    TEST_ASSERT_TRUE(editor_undo_undo(&undo, buf, &len, BUF_CAP, &at));
    TEST_ASSERT_EQUAL_STRING("one two three", buf);
    TEST_ASSERT_EQUAL_UINT(4, at);
}

static void test_nothing_to_undo_or_redo_is_reported(void)
{
    set_text("abc");

    TEST_ASSERT_FALSE(undo_step());
    TEST_ASSERT_FALSE(redo_step());

    editor_undo_begin(&undo);
    ed_insert(3, "d");
    TEST_ASSERT_FALSE(redo_step());   // Nothing undone yet
    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_FALSE(undo_step());   // ... and nothing left behind it
    TEST_ASSERT_EQUAL_STRING("abc", buf);
}

static void test_a_journal_with_no_store_records_nothing(void)
{
    editor_undo_init(&undo, NULL, 0);
    set_text("abc");

    editor_undo_begin(&undo);
    ed_insert(3, "d");
    TEST_ASSERT_EQUAL_STRING("abcd", buf);
    TEST_ASSERT_FALSE(undo_step());
    TEST_ASSERT_FALSE(redo_step());
}

//
//  Steps
//

static void test_a_step_is_reversed_whole(void)
{
    set_text("a\nb\nc\n");

    // What `>>` over three lines does: one record per line, one step
    editor_undo_begin(&undo);
    ed_insert(4, "  ");
    ed_insert(2, "  ");
    ed_insert(0, "  ");
    TEST_ASSERT_EQUAL_STRING("  a\n  b\n  c\n", buf);

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("a\nb\nc\n", buf);
    TEST_ASSERT_FALSE(undo_step());

    TEST_ASSERT_TRUE(redo_step());
    TEST_ASSERT_EQUAL_STRING("  a\n  b\n  c\n", buf);
    TEST_ASSERT_FALSE(redo_step());
}

static void test_steps_are_reversed_one_at_a_time(void)
{
    set_text("");

    editor_undo_begin(&undo);
    ed_insert(0, "one");
    editor_undo_begin(&undo);
    ed_insert(3, " two");
    editor_undo_begin(&undo);
    ed_insert(7, " three");
    TEST_ASSERT_EQUAL_STRING("one two three", buf);

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("one two", buf);
    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("one", buf);
    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("", buf);
    TEST_ASSERT_FALSE(undo_step());

    TEST_ASSERT_TRUE(redo_step());
    TEST_ASSERT_EQUAL_STRING("one", buf);
    TEST_ASSERT_TRUE(redo_step());
    TEST_ASSERT_EQUAL_STRING("one two", buf);
    TEST_ASSERT_TRUE(redo_step());
    TEST_ASSERT_EQUAL_STRING("one two three", buf);
    TEST_ASSERT_FALSE(redo_step());
}

static void test_a_new_change_drops_what_was_undone(void)
{
    set_text("");

    editor_undo_begin(&undo);
    ed_insert(0, "one");
    editor_undo_begin(&undo);
    ed_insert(3, " two");

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("one", buf);

    editor_undo_begin(&undo);
    ed_insert(3, " else");
    TEST_ASSERT_FALSE(redo_step());   // " two" is gone for good

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("one", buf);
    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("", buf);
}

static void test_reset_forgets_everything(void)
{
    set_text("abc");

    editor_undo_begin(&undo);
    ed_insert(3, "d");
    editor_undo_reset(&undo);

    TEST_ASSERT_FALSE(undo_step());
    TEST_ASSERT_FALSE(redo_step());
    TEST_ASSERT_EQUAL_STRING("abcd", buf);
}

//
//  An insert session
//

static void test_typing_coalesces_into_one_record(void)
{
    set_text("");

    // `i` begins the step; the characters that follow join it
    editor_undo_begin(&undo);
    ed_insert(0, "f");
    ed_insert(1, "d");
    ed_insert(2, " ");
    ed_insert(3, "1");
    ed_insert(4, "0");
    ed_insert(5, "0");
    TEST_ASSERT_EQUAL_STRING("fd 100", buf);

    // One record, not six: the header is paid once, not per keystroke
    TEST_ASSERT_TRUE(undo.used < 40);

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("", buf);
    TEST_ASSERT_TRUE(redo_step());
    TEST_ASSERT_EQUAL_STRING("fd 100", buf);
}

static void test_backspacing_over_typing_shrinks_the_record(void)
{
    set_text("");

    editor_undo_begin(&undo);
    ed_insert(0, "abc");
    ed_delete(2, 1);   // Backspace over the 'c'
    ed_insert(2, "X");
    TEST_ASSERT_EQUAL_STRING("abX", buf);

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("", buf);
    TEST_ASSERT_TRUE(redo_step());
    TEST_ASSERT_EQUAL_STRING("abX", buf);
}

static void test_typing_and_backspacing_it_all_away_leaves_no_step(void)
{
    set_text("ab");

    editor_undo_begin(&undo);
    ed_insert(2, "cd");
    ed_delete(2, 2);
    TEST_ASSERT_EQUAL_STRING("ab", buf);

    TEST_ASSERT_FALSE(undo_step());   // There is no change left to reverse
    TEST_ASSERT_EQUAL_UINT(0, undo.used);
}

static void test_backspacing_past_the_start_of_the_session_is_its_own_record(void)
{
    set_text("abc");

    editor_undo_begin(&undo);
    ed_insert(3, "d");
    ed_delete(2, 2);   // The 'd' just typed *and* the 'c' that was there
    TEST_ASSERT_EQUAL_STRING("ab", buf);

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("abc", buf);
    TEST_ASSERT_FALSE(undo_step());
}

static void test_typing_after_a_step_boundary_does_not_coalesce(void)
{
    set_text("");

    editor_undo_begin(&undo);
    ed_insert(0, "ab");
    editor_undo_begin(&undo);
    ed_insert(2, "cd");

    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_STRING("ab", buf);
}

//
//  Running out of room
//

static void test_the_oldest_steps_are_dropped_to_make_room(void)
{
    char small[128];
    editor_undo_init(&undo, small, sizeof(small));
    set_text("");

    // Each step is a header plus its text, so a handful fill 128 bytes
    for (int i = 0; i < 12; i++) {
        editor_undo_begin(&undo);
        ed_insert(len, "xy");
    }
    TEST_ASSERT_EQUAL_STRING("xyxyxyxyxyxyxyxyxyxyxyxy", buf);

    // What is left undoes cleanly; what fell off the bottom is simply gone
    int reversed = 0;
    while (undo_step()) {
        reversed++;
    }
    TEST_ASSERT_TRUE(reversed > 0);
    TEST_ASSERT_TRUE(reversed < 12);
    TEST_ASSERT_EQUAL_UINT(strlen(buf), len);
    TEST_ASSERT_EQUAL_UINT((size_t)(12 - reversed) * 2, len);

    // And what is left can be put back
    int repeated = 0;
    while (redo_step()) {
        repeated++;
    }
    TEST_ASSERT_EQUAL_INT(reversed, repeated);
    TEST_ASSERT_EQUAL_STRING("xyxyxyxyxyxyxyxyxyxyxyxy", buf);
}

static void test_a_step_larger_than_the_journal_clears_it(void)
{
    char small[64];
    editor_undo_init(&undo, small, sizeof(small));
    set_text("");

    editor_undo_begin(&undo);
    ed_insert(0, "small");

    editor_undo_begin(&undo);
    ed_insert(5, "0123456789012345678901234567890123456789012345678901234567890123456789");

    // The big change cannot be reversed, and neither can anything before it:
    // the journal no longer describes the text that is there
    TEST_ASSERT_FALSE(undo_step());
    TEST_ASSERT_FALSE(redo_step());

    // ... and the next change is recorded as usual
    editor_undo_begin(&undo);
    ed_insert(0, "ok");
    TEST_ASSERT_TRUE(undo_step());
    TEST_ASSERT_EQUAL_UINT(75, len);
}

static void test_the_rest_of_an_abandoned_step_is_not_recorded(void)
{
    char small[64];
    editor_undo_init(&undo, small, sizeof(small));
    set_text("");

    editor_undo_begin(&undo);
    ed_insert(0, "0123456789012345678901234567890123456789012345678901234567890123456789");
    ed_insert(0, "z");   // Same step; half of it would be worse than none

    TEST_ASSERT_FALSE(undo_step());
}

//
//  A random run: undoing everything must give back what we started with
//

static void test_undoing_a_random_run_gives_back_the_original(void)
{
    static const char *original = "to box :size\n  repeat 4 [fd :size]\nend\n";

    srand(20260818);

    for (int round = 0; round < 200; round++) {
        setUp();
        set_text(original);

        int steps = 1 + rand() % 20;
        for (int i = 0; i < steps; i++) {
            editor_undo_begin(&undo);

            // A step is one to three changes, the way an operator over several
            // lines is
            int changes = 1 + rand() % 3;
            for (int c = 0; c < changes; c++) {
                size_t pos = len > 0 ? (size_t)rand() % len : 0;
                switch (rand() % 3) {
                    case 0: {
                        static const char *bits[] = { "x", "hello ", "\n", "  ", "[]" };
                        const char *text = bits[rand() % 5];
                        if (len + strlen(text) + 1 < BUF_CAP) {
                            ed_insert(pos, text);
                        }
                        break;
                    }
                    case 1: {
                        size_t n = 1 + (size_t)rand() % 8;
                        if (pos + n <= len) {
                            ed_delete(pos, n);
                        }
                        break;
                    }
                    default:
                        if (pos + 2 <= len) {
                            ed_overwrite(pos, "AB");
                        }
                        break;
                }
            }
        }

        while (undo_step()) {
            /* all the way back */
        }
        TEST_ASSERT_EQUAL_STRING_MESSAGE(original, buf, "undo did not restore the text");
        TEST_ASSERT_EQUAL_UINT(strlen(original), len);

        while (redo_step()) {
            /* and all the way forward again */
        }
        TEST_ASSERT_EQUAL_UINT_MESSAGE(strlen(buf), len, "redo lost the length");
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_an_insert_is_reversed_and_repeated);
    RUN_TEST(test_a_delete_is_reversed_and_repeated);
    RUN_TEST(test_an_overwrite_carries_both_sides);
    RUN_TEST(test_the_position_reported_is_the_earliest_one_touched);
    RUN_TEST(test_nothing_to_undo_or_redo_is_reported);
    RUN_TEST(test_a_journal_with_no_store_records_nothing);

    RUN_TEST(test_a_step_is_reversed_whole);
    RUN_TEST(test_steps_are_reversed_one_at_a_time);
    RUN_TEST(test_a_new_change_drops_what_was_undone);
    RUN_TEST(test_reset_forgets_everything);

    RUN_TEST(test_typing_coalesces_into_one_record);
    RUN_TEST(test_backspacing_over_typing_shrinks_the_record);
    RUN_TEST(test_typing_and_backspacing_it_all_away_leaves_no_step);
    RUN_TEST(test_backspacing_past_the_start_of_the_session_is_its_own_record);
    RUN_TEST(test_typing_after_a_step_boundary_does_not_coalesce);

    RUN_TEST(test_the_oldest_steps_are_dropped_to_make_room);
    RUN_TEST(test_a_step_larger_than_the_journal_clears_it);
    RUN_TEST(test_the_rest_of_an_abandoned_step_is_not_recorded);

    RUN_TEST(test_undoing_a_random_run_gives_back_the_original);

    return UNITY_END();
}
