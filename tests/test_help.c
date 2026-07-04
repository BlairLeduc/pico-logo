//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the help-text lookup and the sort-order invariant on
//  `help_entries[]` (the binary search in `help_lookup` silently
//  misbehaves if the table is not sorted, so we pin the contract here).
//

#include "unity.h"
#include "core/help.h"

#include <string.h>
#include <strings.h>

void setUp(void) {}
void tearDown(void) {}

void test_help_table_is_sorted(void)
{
    TEST_ASSERT_TRUE_MESSAGE(help_check_sorted(),
        "help_entries[] must be sorted ascending case-insensitively. "
        "Re-run scripts/generate_help.* and rebuild.");
}

void test_help_lookup_returns_text_for_known_primitive(void)
{
    // `forward` is one of the most fundamental primitives; if it has no
    // help entry the generator is broken.
    const char *text = help_lookup("forward");
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_TRUE(strlen(text) > 0);
}

void test_help_lookup_is_case_insensitive(void)
{
    const char *lower = help_lookup("forward");
    const char *upper = help_lookup("FORWARD");
    const char *mixed = help_lookup("ForWArd");
    TEST_ASSERT_NOT_NULL(lower);
    TEST_ASSERT_EQUAL_PTR(lower, upper);
    TEST_ASSERT_EQUAL_PTR(lower, mixed);
}

void test_help_lookup_returns_null_for_unknown(void)
{
    TEST_ASSERT_NULL(help_lookup("definitely_not_a_primitive_xyzzy"));
    TEST_ASSERT_NULL(help_lookup(""));
}

void test_help_categories_are_valid(void)
{
    TEST_ASSERT_TRUE(help_category_count > 0);
    for (int i = 0; i < help_entry_count; i++)
    {
        TEST_ASSERT_TRUE_MESSAGE(help_entries[i].category < help_category_count,
            "Entry category index out of range");
    }
}

void test_help_entry_category_matches_chapter(void)
{
    // `forward` is documented in the Turtle Graphics chapter
    for (int i = 0; i < help_entry_count; i++)
    {
        if (strcasecmp(help_entries[i].name, "forward") == 0)
        {
            TEST_ASSERT_EQUAL_STRING("Turtle Graphics",
                help_categories[help_entries[i].category]);
            return;
        }
    }
    TEST_FAIL_MESSAGE("forward has no help entry");
}

void test_help_contains_nocase(void)
{
    TEST_ASSERT_TRUE(help_contains_nocase("rerandom", "rand"));
    TEST_ASSERT_TRUE(help_contains_nocase("ReRandom", "RAND"));
    TEST_ASSERT_TRUE(help_contains_nocase("abc", "abc"));
    TEST_ASSERT_FALSE(help_contains_nocase("abc", "abcd"));
    TEST_ASSERT_FALSE(help_contains_nocase("forward", "rand"));
    // Empty needle matches nothing (an empty word must not list every entry)
    TEST_ASSERT_FALSE(help_contains_nocase("abc", ""));
    TEST_ASSERT_FALSE(help_contains_nocase("", "abc"));
    TEST_ASSERT_FALSE(help_contains_nocase("", ""));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_help_table_is_sorted);
    RUN_TEST(test_help_lookup_returns_text_for_known_primitive);
    RUN_TEST(test_help_lookup_is_case_insensitive);
    RUN_TEST(test_help_lookup_returns_null_for_unknown);
    RUN_TEST(test_help_categories_are_valid);
    RUN_TEST(test_help_entry_category_matches_chapter);
    RUN_TEST(test_help_contains_nocase);
    return UNITY_END();
}
