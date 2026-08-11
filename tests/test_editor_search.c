//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the editor's incremental search and replace
//  (devices/picocalc/editor_search.c). Verifies case-insensitive matching,
//  forward/backward direction, the wrap-around that lets the up and down
//  keys cycle through every occurrence, and the replacement of every match.
//

#include "unity.h"
#include "editor_search.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// Find in a null-terminated string; returns the match position or -1.
static long find(const char *text, const char *needle, size_t from, bool forward)
{
    size_t pos;
    if (!editor_search_find(text, strlen(text), needle, strlen(needle),
                            from, forward, &pos)) {
        return -1;
    }
    return (long)pos;
}

void test_finds_first_match_from_start(void)
{
    TEST_ASSERT_EQUAL_INT(3, find("to square :size\n", "square", 0, true));
}

void test_no_match_returns_false(void)
{
    TEST_ASSERT_EQUAL_INT(-1, find("to square :size\n", "circle", 0, true));
}

void test_match_at_the_search_origin_counts(void)
{
    // A forward search includes the position it starts at
    TEST_ASSERT_EQUAL_INT(3, find("to square", "square", 3, true));
}

void test_empty_needle_never_matches(void)
{
    TEST_ASSERT_EQUAL_INT(-1, find("to square", "", 0, true));
}

void test_needle_longer_than_text_never_matches(void)
{
    TEST_ASSERT_EQUAL_INT(-1, find("fd", "forward", 0, true));
}

void test_matching_is_case_insensitive(void)
{
    TEST_ASSERT_EQUAL_INT(0, find("FD 100", "fd", 0, true));
    TEST_ASSERT_EQUAL_INT(0, find("fd 100", "FD", 0, true));
    TEST_ASSERT_EQUAL_INT(3, find("to SeTpOs", "setpos", 0, true));
}

void test_forward_finds_the_next_occurrence(void)
{
    const char *text = "fd 10 fd 20 fd 30";
    TEST_ASSERT_EQUAL_INT(0, find(text, "fd", 0, true));
    TEST_ASSERT_EQUAL_INT(6, find(text, "fd", 1, true));
    TEST_ASSERT_EQUAL_INT(12, find(text, "fd", 7, true));
}

void test_forward_wraps_around_to_the_start(void)
{
    const char *text = "fd 10 fd 20 fd 30";
    // Past the last occurrence, so the search cycles back to the first
    TEST_ASSERT_EQUAL_INT(0, find(text, "fd", 13, true));
}

void test_forward_wraps_when_starting_past_the_end(void)
{
    TEST_ASSERT_EQUAL_INT(0, find("fd 10", "fd", 99, true));
}

void test_backward_finds_the_previous_occurrence(void)
{
    const char *text = "fd 10 fd 20 fd 30";
    TEST_ASSERT_EQUAL_INT(6, find(text, "fd", 12, false));
    TEST_ASSERT_EQUAL_INT(0, find(text, "fd", 6, false));
}

void test_backward_excludes_the_search_origin(void)
{
    // Starting on a match, backward skips it and finds the one before
    TEST_ASSERT_EQUAL_INT(0, find("fd fd", "fd", 3, false));
}

void test_backward_wraps_around_to_the_end(void)
{
    const char *text = "fd 10 fd 20 fd 30";
    // Nothing before position 0, so the search cycles back to the last match
    TEST_ASSERT_EQUAL_INT(12, find(text, "fd", 0, false));
}

void test_overlapping_occurrences_are_all_reachable(void)
{
    const char *text = "aaaa";
    TEST_ASSERT_EQUAL_INT(0, find(text, "aa", 0, true));
    TEST_ASSERT_EQUAL_INT(1, find(text, "aa", 1, true));
    TEST_ASSERT_EQUAL_INT(2, find(text, "aa", 2, true));
    // Position 2 is the last one a match can start at, so this wraps
    TEST_ASSERT_EQUAL_INT(0, find(text, "aa", 3, true));
}

void test_match_may_end_at_the_end_of_the_text(void)
{
    TEST_ASSERT_EQUAL_INT(3, find("fd 100", "100", 0, true));
}

void test_search_crosses_line_boundaries(void)
{
    const char *text = "to box\n  repeat 4 [fd 50 rt 90]\nend\n";
    TEST_ASSERT_EQUAL_INT(9, find(text, "repeat", 0, true));
    TEST_ASSERT_EQUAL_INT(32, find(text, "end", 0, true));
}

//
// Replace in a null-terminated copy of text, checking the reported length
// against the text left behind; returns the number of replacements.
//
static size_t replace(char *text, const char *needle, const char *replacement,
                      size_t capacity)
{
    size_t len = strlen(text);
    size_t count = editor_search_replace_all(text, &len, capacity,
                                             needle, strlen(needle),
                                             replacement, strlen(replacement));
    TEST_ASSERT_EQUAL_UINT(strlen(text), len);
    return count;
}

void test_replaces_every_occurrence(void)
{
    char text[64] = "fd 10 fd 20 fd 30";
    TEST_ASSERT_EQUAL_UINT(3, replace(text, "fd", "bk", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("bk 10 bk 20 bk 30", text);
}

void test_replacement_matches_case_insensitively_and_is_inserted_verbatim(void)
{
    char text[64] = "FD 10 fd 20 Fd 30";
    TEST_ASSERT_EQUAL_UINT(3, replace(text, "fd", "Bk", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("Bk 10 Bk 20 Bk 30", text);
}

void test_longer_replacement_grows_the_text(void)
{
    char text[64] = "fd 10 fd 20";
    TEST_ASSERT_EQUAL_UINT(2, replace(text, "fd", "forward", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("forward 10 forward 20", text);
}

void test_shorter_replacement_shrinks_the_text(void)
{
    char text[64] = "forward 10 forward 20";
    TEST_ASSERT_EQUAL_UINT(2, replace(text, "forward", "fd", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("fd 10 fd 20", text);
}

void test_empty_replacement_deletes_every_occurrence(void)
{
    char text[64] = "to box\n  fd 50\nend\n";
    TEST_ASSERT_EQUAL_UINT(2, replace(text, "o", "", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("t bx\n  fd 50\nend\n", text);
}

void test_replacement_containing_the_needle_is_not_matched_again(void)
{
    char text[64] = "fd 10";
    TEST_ASSERT_EQUAL_UINT(1, replace(text, "fd", "fd fd", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("fd fd 10", text);
}

void test_overlapping_occurrences_are_replaced_without_overlap(void)
{
    char text[64] = "aaaa";
    TEST_ASSERT_EQUAL_UINT(2, replace(text, "aa", "b", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("bb", text);
}

void test_replace_crosses_line_boundaries(void)
{
    char text[64] = "to box\n  fd 50\nend\n";
    TEST_ASSERT_EQUAL_UINT(1, replace(text, "box", "square", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("to square\n  fd 50\nend\n", text);
}

void test_no_match_leaves_the_text_alone(void)
{
    char text[64] = "fd 10";
    TEST_ASSERT_EQUAL_UINT(0, replace(text, "bk", "rt", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("fd 10", text);
}

void test_empty_needle_replaces_nothing(void)
{
    char text[64] = "fd 10";
    TEST_ASSERT_EQUAL_UINT(0, replace(text, "", "rt", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("fd 10", text);
}

void test_needle_longer_than_the_text_replaces_nothing(void)
{
    char text[64] = "fd";
    TEST_ASSERT_EQUAL_UINT(0, replace(text, "forward", "bk", sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("fd", text);
}

void test_a_result_that_would_not_fit_changes_nothing(void)
{
    // "fdfd" grows to 6 characters, one too many for the 6 bytes left after the NUL
    char text[16] = "fdfd";
    size_t len = strlen(text);
    TEST_ASSERT_EQUAL_UINT(0, editor_search_replace_all(text, &len, 6, "fd", 2, "bkl", 3));
    TEST_ASSERT_EQUAL_STRING("fdfd", text);
    TEST_ASSERT_EQUAL_UINT(4, len);
}

void test_a_result_that_exactly_fills_the_buffer_is_replaced(void)
{
    char text[16] = "fdfd";
    size_t len = strlen(text);
    TEST_ASSERT_EQUAL_UINT(2, editor_search_replace_all(text, &len, 7, "fd", 2, "bkl", 3));
    TEST_ASSERT_EQUAL_STRING("bklbkl", text);
    TEST_ASSERT_EQUAL_UINT(6, len);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_finds_first_match_from_start);
    RUN_TEST(test_no_match_returns_false);
    RUN_TEST(test_match_at_the_search_origin_counts);
    RUN_TEST(test_empty_needle_never_matches);
    RUN_TEST(test_needle_longer_than_text_never_matches);
    RUN_TEST(test_matching_is_case_insensitive);
    RUN_TEST(test_forward_finds_the_next_occurrence);
    RUN_TEST(test_forward_wraps_around_to_the_start);
    RUN_TEST(test_forward_wraps_when_starting_past_the_end);
    RUN_TEST(test_backward_finds_the_previous_occurrence);
    RUN_TEST(test_backward_excludes_the_search_origin);
    RUN_TEST(test_backward_wraps_around_to_the_end);
    RUN_TEST(test_overlapping_occurrences_are_all_reachable);
    RUN_TEST(test_match_may_end_at_the_end_of_the_text);
    RUN_TEST(test_search_crosses_line_boundaries);
    RUN_TEST(test_replaces_every_occurrence);
    RUN_TEST(test_replacement_matches_case_insensitively_and_is_inserted_verbatim);
    RUN_TEST(test_longer_replacement_grows_the_text);
    RUN_TEST(test_shorter_replacement_shrinks_the_text);
    RUN_TEST(test_empty_replacement_deletes_every_occurrence);
    RUN_TEST(test_replacement_containing_the_needle_is_not_matched_again);
    RUN_TEST(test_overlapping_occurrences_are_replaced_without_overlap);
    RUN_TEST(test_replace_crosses_line_boundaries);
    RUN_TEST(test_no_match_leaves_the_text_alone);
    RUN_TEST(test_empty_needle_replaces_nothing);
    RUN_TEST(test_needle_longer_than_the_text_replaces_nothing);
    RUN_TEST(test_a_result_that_would_not_fit_changes_nothing);
    RUN_TEST(test_a_result_that_exactly_fills_the_buffer_is_replaced);
    return UNITY_END();
}
