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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_help_table_is_sorted);
    RUN_TEST(test_help_lookup_returns_text_for_known_primitive);
    RUN_TEST(test_help_lookup_is_case_insensitive);
    RUN_TEST(test_help_lookup_returns_null_for_unknown);
    return UNITY_END();
}
