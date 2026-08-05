//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Load and scaling test for logo/samples/xkcd2601.
//

#include "test_scaffold.h"
#include "mock_device.h"
#include "core/error.h"
#include "core/repl.h"
#include <stdio.h>
#include <string.h>

#ifndef XKCD2601_SOURCE
#error "XKCD2601_SOURCE must be defined (path to logo/samples/xkcd2601)"
#endif

static void load_xkcd2601(void)
{
    FILE *f = fopen(XKCD2601_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "cannot open " XKCD2601_SOURCE);

    char line[256];
    char proc[4096];
    size_t proc_len = 0;
    bool in_def = false;

    while (fgets(line, sizeof(line), f))
    {
        size_t len = strlen(line);
        TEST_ASSERT_TRUE_MESSAGE(len < sizeof(line) - 1 || line[len - 1] == '\n',
                                 "source line exceeds load buffer");
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0)
            continue;

        if (!in_def && repl_line_starts_with_to(line))
        {
            in_def = true;
            proc_len = 0;
        }

        if (in_def)
        {
            ProcDefStatus status = repl_proc_def_append(proc, sizeof(proc),
                                                        &proc_len, line);
            TEST_ASSERT_NOT_EQUAL_MESSAGE(PROC_DEF_OVERFLOW, status,
                                          "procedure exceeds load buffer");
            if (status == PROC_DEF_COMPLETE)
            {
                in_def = false;
                Result r = proc_define_from_text(proc);
                TEST_ASSERT_MESSAGE(r.status != RESULT_ERROR, proc);
            }
            continue;
        }

        Result r = run_string(line);
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            fprintf(stderr, "%s: %s\n", line, error_format(r));
            TEST_FAIL_MESSAGE(line);
        }
    }

    TEST_ASSERT_FALSE_MESSAGE(in_def, "unterminated procedure");
    fclose(f);
}

void setUp(void)
{
    test_scaffold_setUp_with_device_and_hardware();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

void test_xkcd2601_loads_and_fits_the_turtle_screen(void)
{
    load_xkcd2601();

    TEST_ASSERT_GREATER_THAN(0, mock_device_line_count());

    const MockTurtleState *turtle = mock_device_get_turtle(0);
    const float scale = 319.0f / 1083.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f + (-169.5f * scale), turtle->x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f + (130.0f * scale), turtle->y);

    Result r = run_string("penup XKCD.SETXY -483 -566");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    turtle = mock_device_get_turtle(0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -159.0f, turtle->x);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(-159.0f, turtle->y);

    r = run_string("XKCD.SETXY 600 476");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    turtle = mock_device_get_turtle(0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 160.0f, turtle->x);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(160.0f, turtle->y);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xkcd2601_loads_and_fits_the_turtle_screen);
    return UNITY_END();
}
