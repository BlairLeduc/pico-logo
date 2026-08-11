//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the editor's incremental search
//  (devices/picocalc/editor_search.c). Verifies case-insensitive matching,
//  forward/backward direction, and the wrap-around that lets the up and down
//  keys cycle through every occurrence.
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
    return UNITY_END();
}
