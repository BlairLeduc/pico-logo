//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the pure-Logo graphics showcase (logo/demos/graphics).
//
//  Loading the whole file proves every procedure parses. The scene checks
//  then exercise palette/costume setup and the two non-timed render paths on
//  the mock device, catching broken primitive names, inputs and addressing.
//

#include "test_scaffold.h"
#include "mock_device.h"
#include "core/limits.h"
#include "core/repl.h"
#include <stdio.h>
#include <string.h>

#ifndef GRAPHICS_DEMO_SOURCE
#error "GRAPHICS_DEMO_SOURCE must be defined (path to logo/demos/graphics)"
#endif

static void load_graphics_demo(void)
{
    FILE *f = fopen(GRAPHICS_DEMO_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "cannot open " GRAPHICS_DEMO_SOURCE);

    // The same buffer `load` gives a definition, so a procedure this test
    // accepts is one the real loader accepts too. It used to be 8 KB, which
    // let the demo grow past what `load` would take and still pass here.
    char line[512];
    char proc[LOGO_LOAD_PROC_BUFFER_SIZE];
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
                TEST_ASSERT_MESSAGE(proc_len + 4 <= sizeof(proc),
                                    "procedure exceeds load buffer");
                memcpy(proc + proc_len, "end", 3);
                proc[proc_len + 3] = '\0';
                in_def = false;
                Result r = proc_define_from_text(proc);
                TEST_ASSERT_MESSAGE(r.status != RESULT_ERROR, proc);
            }
            else
            {
                TEST_ASSERT_MESSAGE(proc_len + len + 1 < sizeof(proc),
                                    "procedure exceeds load buffer");
                memcpy(proc + proc_len, line, len);
                proc[proc_len + len] = '\n';
                proc_len += len + 1;
            }
            continue;
        }

        Result r = run_string(line);
        TEST_ASSERT_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                            line);
    }
    fclose(f);
}

static void run_ok(const char *source)
{
    Result r = run_string(source);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_NONE, r.status, source);
}

void setUp(void)
{
    test_scaffold_setUp_with_device_and_hardware();
    load_graphics_demo();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

void test_setup_defines_palette_and_costumes(void)
{
    run_ok("gfx.setup");

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_UINT8(5, state->palette.r[240]);
    TEST_ASSERT_EQUAL_UINT8(8, state->palette.g[240]);
    TEST_ASSERT_EQUAL_UINT8(24, state->palette.b[240]);

    // Every shape is 16x16 pixels of pen (254) and transparent (255).
    // Shape 1's top row is blank until its fifth pixel; shape 8 is the
    // solid block; shape 11's bottom row starts and ends painted.
    uint8_t w = 0, h = 0;
    const uint8_t *shape1 = mock_device_get_shape(1, &w, &h);
    TEST_ASSERT_NOT_NULL(shape1);
    TEST_ASSERT_EQUAL_UINT8(16, w);
    TEST_ASSERT_EQUAL_UINT8(16, h);
    TEST_ASSERT_EQUAL_UINT8(LOGO_SHAPE_TRANSPARENT, shape1[0]);
    TEST_ASSERT_EQUAL_UINT8(LOGO_SHAPE_PEN, shape1[6]);

    const uint8_t *shape8 = mock_device_get_shape(8, &w, &h);
    TEST_ASSERT_NOT_NULL(shape8);
    TEST_ASSERT_EQUAL_UINT8(LOGO_SHAPE_PEN, shape8[0]);

    const uint8_t *shape11 = mock_device_get_shape(11, &w, &h);
    TEST_ASSERT_NOT_NULL(shape11);
    TEST_ASSERT_EQUAL_UINT8(LOGO_SHAPE_PEN, shape11[15 * 16]);
    TEST_ASSERT_EQUAL_UINT8(LOGO_SHAPE_PEN, shape11[15 * 16 + 15]);
}

void test_sprite_scene_configures_rotation_scale_and_layers(void)
{
    run_ok("gfxsprites");

    const MockTurtleState *fixed = mock_device_get_turtle(0);
    const MockTurtleState *flipped = mock_device_get_turtle(1);
    const MockTurtleState *rotated = mock_device_get_turtle(2);
    const MockTurtleState *large = mock_device_get_turtle(4);
    TEST_ASSERT_TRUE(fixed->visible);
    TEST_ASSERT_EQUAL_UINT8(1, fixed->shape);
    TEST_ASSERT_EQUAL_UINT8(LOGO_ROT_FIXED, fixed->rot_style);
    TEST_ASSERT_EQUAL_UINT8(LOGO_ROT_FLIP, flipped->rot_style);
    TEST_ASSERT_EQUAL_UINT8(LOGO_ROT_FULL, rotated->rot_style);
    TEST_ASSERT_EQUAL_UINT8(2, large->mag);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_GREATER_THAN(0, state->label.count);
}

void test_tile_scene_captures_colour_costume_and_stamps_canvas(void)
{
    run_ok("gfxtiles");

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT(1, state->costume.snap_count);
    TEST_ASSERT_EQUAL_UINT8(15, state->costume.last_snap_slot);
    TEST_ASSERT_EQUAL_UINT8(24, state->costume.last_snap_w);
    TEST_ASSERT_EQUAL_UINT8(24, state->costume.last_snap_h);

    int stamps = 0;
    for (int i = 0; i < state->command_count; i++)
    {
        if (mock_device_get_command(i)->type == MOCK_CMD_STAMP)
            stamps++;
    }
    // Position/rotation setup also fills the mock's bounded command history,
    // but the complete 13-tile floor must still be present in the retained
    // prefix. The scene itself executed successfully, including later stamps.
    TEST_ASSERT_GREATER_OR_EQUAL_INT(13, stamps);
    TEST_ASSERT_TRUE(mock_device_get_turtle(0)->visible);
    TEST_ASSERT_EQUAL_UINT8(1, mock_device_get_turtle(0)->shape);
}

void test_timed_scenes_run_and_restore_automatic_refresh(void)
{
    // The mock clock is stationary, so sync presents frames without advancing
    // the autonomous actors. This still executes the complete animation and
    // collision scene bodies, including demon registration and cleanup.
    run_ok("gfxmotion");
    run_ok("gfxcollision");

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_TRUE(state->refresh_auto);
    for (uint8_t turtle = 0; turtle < MOCK_MAX_TURTLES; turtle++)
        TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_device_get_turtle(turtle)->speed);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_setup_defines_palette_and_costumes);
    RUN_TEST(test_sprite_scene_configures_rotation_scale_and_layers);
    RUN_TEST(test_tile_scene_captures_colour_costume_and_stamps_canvas);
    RUN_TEST(test_timed_scenes_run_and_restore_automatic_refresh);
    return UNITY_END();
}
