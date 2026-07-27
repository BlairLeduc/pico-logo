//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Fullscreen canvas-HUD tests for logo/games/invaders and galaxian.
//

#include "test_scaffold.h"
#include "mock_device.h"
#include "core/repl.h"
#include <stdio.h>
#include <string.h>

#ifndef INVADERS_SOURCE
#error "INVADERS_SOURCE must be defined (path to logo/games/invaders)"
#endif

#ifndef GALAXIAN_SOURCE
#error "GALAXIAN_SOURCE must be defined (path to logo/games/galaxian)"
#endif

static void load_game(const char *path)
{
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);

    char line[512];
    char proc[8192];
    size_t proc_len = 0;
    bool in_def = false;

    while (fgets(line, sizeof(line), f))
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0)
            continue;

        if (!in_def && repl_line_starts_with_to(line))
        {
            in_def = true;
            memcpy(proc, line, len);
            proc[len] = '\n';
            proc_len = len + 1;
            continue;
        }
        if (in_def)
        {
            if (repl_line_is_end(line))
            {
                TEST_ASSERT_MESSAGE(proc_len + 4 <= sizeof(proc), "procedure exceeds load buffer");
                memcpy(proc + proc_len, "end", 4);
                in_def = false;
                Result r = proc_define_from_text(proc);
                TEST_ASSERT_MESSAGE(r.status != RESULT_ERROR, proc);
            }
            else
            {
                TEST_ASSERT_MESSAGE(proc_len + len + 1 < sizeof(proc), "procedure exceeds load buffer");
                memcpy(proc + proc_len, line, len);
                proc[proc_len + len] = '\n';
                proc_len += len + 1;
            }
            continue;
        }

        Result r = run_string(line);
        TEST_ASSERT_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, line);
    }

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

static void assert_hud_write_positions(void)
{
    const float expected_x[] = {-155.0f, -28.0f, 91.0f,
                                -155.0f, -28.0f, 91.0f};
    int write = 0;

    for (int i = 0; i < mock_device_command_count(); i++)
    {
        const MockCommand *command = mock_device_get_command(i);
        if (command->type != MOCK_CMD_WRITE)
            continue;

        int position_index = i - 1;
        while (position_index >= 0 &&
               mock_device_get_command(position_index)->type != MOCK_CMD_SET_POSITION)
            position_index--;
        TEST_ASSERT_TRUE_MESSAGE(position_index >= 0, "HUD write must follow setx/sety");
        const MockCommand *position = mock_device_get_command(position_index);
        TEST_ASSERT_EQUAL(MOCK_CMD_SET_POSITION, position->type);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_x[write], position->params.position.x);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 155.0f, position->params.position.y);
        write++;
    }
    TEST_ASSERT_EQUAL(6, write);
}

static void assert_fullscreen_canvas_hud(const char *path, uint16_t colour)
{
    load_game(path);
    Result r = run_string("make \"score 0 make \"lives 3 make \"level 1 setup.level");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    mock_device_clear_commands();
    r = run_string("draw.hud");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(MOCK_SCREEN_FULLSCREEN, state->screen_mode);
    TEST_ASSERT_EQUAL(6, state->label.count);
    TEST_ASSERT_EQUAL_STRING("LIVES: 3", state->label.last_text);
    TEST_ASSERT_EQUAL_FLOAT(91.0f, state->label.last_x);
    TEST_ASSERT_EQUAL_FLOAT(155.0f, state->label.last_y);
    TEST_ASSERT_EQUAL(colour, state->label.last_colour);
    TEST_ASSERT_EQUAL(6, state->label.last_turtle);
    assert_hud_write_positions();

    r = run_string("draw.hud");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL(6, state->label.count);

    r = run_string("make \"score 10 draw.hud");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL(12, state->label.count);
    TEST_ASSERT_EQUAL_STRING("LIVES: 3", state->label.last_text);
    TEST_ASSERT_EQUAL_FLOAT(155.0f, state->label.last_y);
}

void test_invaders_uses_fullscreen_canvas_hud(void)
{
    assert_fullscreen_canvas_hud(INVADERS_SOURCE, 254);
}

void test_galaxian_uses_fullscreen_canvas_hud(void)
{
    assert_fullscreen_canvas_hud(GALAXIAN_SOURCE, 254);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_invaders_uses_fullscreen_canvas_hud);
    RUN_TEST(test_galaxian_uses_fullscreen_canvas_hud);
    return UNITY_END();
}
