//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Coverage test: every registered primitive must have a corresponding
//  entry in `help_entries[]`. The two tables are maintained separately
//  (primitives are registered at startup by primitives_init(); help is
//  generated from the reference Markdown), and historically they have
//  drifted apart silently. This test pins the contract: if it fails,
//  re-run the help generator script for the new primitive(s).
//

#include "unity.h"
#include "test_scaffold.h"
#include "core/primitives.h"
#include "core/help.h"

#include <stdio.h>
#include <string.h>

void setUp(void)    { test_scaffold_setUp(); }
void tearDown(void) { test_scaffold_tearDown(); }

void test_every_primitive_has_help_entry(void)
{
    int total = primitive_get_count();
    TEST_ASSERT_GREATER_THAN(0, total);

    int missing = 0;
    char first_missing[64] = {0};

    for (int i = 0; i < total; i++)
    {
        const Primitive *p = primitive_get_by_index(i);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_NOT_NULL(p->name);

        if (help_lookup(p->name) == NULL)
        {
            if (missing == 0)
            {
                strncpy(first_missing, p->name, sizeof(first_missing) - 1);
            }
            // Print to stderr so CI logs show every gap, not just the first.
            fprintf(stderr,
                    "[help-coverage] no help entry for primitive '%s'\n",
                    p->name);
            missing++;
        }
    }

    if (missing > 0)
    {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "%d primitive(s) lack help entries (first: '%s'). "
                 "Add them to the help source and regenerate help_data.c.",
                 missing, first_missing);
        TEST_FAIL_MESSAGE(msg);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_every_primitive_has_help_entry);
    return UNITY_END();
}
