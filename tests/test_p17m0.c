//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the Dungeons of Daggorath M0 timing harness (tests/logo/p17m0).
//
//  M0 is the gate in docs/daggorath-design.md section 15/12, and it has NOT
//  run on a board. The host is far faster than the target and `ticks` has
//  millisecond resolution, so every timing figure the harness produces reads
//  as zero here -- same caveat as test_p13m0.c. What is worth pinning even so:
//
//    * The three list walks of section 6.3 must draw the SAME picture. A
//      "fastest" candidate that silently draws nothing, or draws the wrong
//      points, is not a result -- it is a bug wearing a good time.
//    * The transform (section 6.2) must match the design's own hand-worked
//      numbers, at range 1 (the check the design itself works out) and at
//      ranges 0 and 9 (section 17's "hand-computed corners at ranges 0, 1
//      and 9").
//    * The fade table (section 8) must round-trip both ways -- the dash
//      periods P18 M2 already owns, and the eight grey levels section 8.1
//      adds beside them.
//    * It has to run end to end, with the report reaching the file.
//

#include "core/limits.h"
#include "test_mock_fs.h"
#include "test_scaffold.h"
#include "mock_device.h"
#include "core/repl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef P17M0_SOURCE
#error "P17M0_SOURCE must be defined (path to tests/logo/p17m0)"
#endif

// Load a whole Logo file, defining its procedures and running its top-level
// `make`s. Procedure definitions are not handled by the bare evaluator, so we
// buffer them and hand them to proc_define_from_text the way `load` does.
static void load_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);

    char line[512];
    char proc[LOGO_LOAD_PROC_BUFFER_SIZE];  // what `load` gives a definition
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
            proc_len = 0;
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
                memcpy(proc + proc_len, "end", 3);
                proc[proc_len + 3] = '\0';
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
    TEST_ASSERT_FALSE_MESSAGE(in_def, "file ends inside a procedure definition");
    fclose(f);
}

void setUp(void)
{
    test_scaffold_setUp_with_device_and_hardware();
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
    load_file(P17M0_SOURCE);
    run_string("splitscreen  window");
}

void tearDown(void)
{
    logo_io_close_all(&mock_io);
    test_scaffold_tearDown();
}

// Through `value_to_number` and not `r.value.as.number`, for the reason
// test_p13m0.c gives: `item` hands back whatever the slot holds, and a list
// element is a word until something does arithmetic on it.
static float num(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    float n = 0.0f;
    TEST_ASSERT_TRUE_MESSAGE(value_to_number(r.value, &n), expr);
    return n;
}

static void run(const char *input)
{
    Result r = run_string(input);
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, input);
}

//==========================================================================
// The scale tables, section 6.1
//==========================================================================

void test_the_scale_tables_are_ten_long(void)
{
    TEST_ASSERT_EQUAL_FLOAT(10, num("count :p17m0.norscl"));
    TEST_ASSERT_EQUAL_FLOAT(10, num("count :p17m0.hlfscl"));
}

void test_norscl_range_one_is_exactly_one_to_one(void)
{
    // Section 6.1: "Range 1 is 128/128 -- exactly 1:1."
    TEST_ASSERT_EQUAL_FLOAT(128, num("item 2 :p17m0.norscl"));
}

//==========================================================================
// The transform, section 6.2
//==========================================================================

// The design's own check, worked by hand at range 1: k = 1.25, kx0 = 160,
// c = 160, and the four corners of the byte range land at known turtle
// coordinates, with the centroid (128, 76) landing at (0, 65).
void test_the_transform_matches_the_designs_range_one_check(void)
{
    run("p17m0.setscale 1 :p17m0.norscl");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.25f, num(":p17m0.k"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 160.0f, num(":p17m0.kx0"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 160.0f, num(":p17m0.c"));

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -160.0f, num("(:p17m0.k * 0) - :p17m0.kx0"));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 158.75f, num("(:p17m0.k * 255) - :p17m0.kx0"));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 160.0f, num(":p17m0.c - (:p17m0.k * 0)"));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -28.75f, num(":p17m0.c - (:p17m0.k * 151)"));

    // The centroid: VCNTRX = 128, VCNTRY = 76, lands at turtle (0, 65).
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, num("(:p17m0.k * 128) - :p17m0.kx0"));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.0f, num(":p17m0.c - (:p17m0.k * 76)"));
}

// Section 17: "the transform -- section 6.2's k/kx0/c against hand-computed
// corners at ranges 0, 1 and 9." Range 1 is covered above; these are the
// other two, worked out the same way from section 6.1's table.
void test_the_transform_at_range_zero(void)
{
    run("p17m0.setscale 0 :p17m0.norscl");
    // scale = 200/128 = 1.5625
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.953125f, num(":p17m0.k"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 250.0f, num(":p17m0.kx0"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 213.4375f, num(":p17m0.c"));
}

void test_the_transform_at_range_nine(void)
{
    run("p17m0.setscale 9 :p17m0.norscl");
    // scale = 2/128 = 0.015625
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.01953125f, num(":p17m0.k"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, num(":p17m0.kx0"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 66.484375f, num(":p17m0.c"));
}

//==========================================================================
// The three list walks, section 6.3 -- they must agree
//==========================================================================

// VARC.ASM:LWALL, the one real ROM table transcribed into the design
// (section 6.2) and into the harness. At range 1 its last point, (136, 27),
// transforms to a known turtle position -- worked by hand the same way as
// the range-one check above.
static void check_lwall_walk_ends_at_known_point(const char *which, const char *xs_or_flat_call)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "p17m0.setscale 1 :p17m0.norscl  %s", xs_or_flat_call);
    run(cmd);
    // X = 27 -> x = 1.25*27 - 160 = -126.25 ; Y = 136 -> y = 160 - 1.25*136 = -10
    char msg[64];
    snprintf(msg, sizeof(msg), "%s: x", which);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -126.25f, num("item 1 pos"));
    snprintf(msg, sizeof(msg), "%s: y", which);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -10.0f, num("item 2 pos"));
}

void test_the_item_walk_draws_lwall_to_the_right_place(void)
{
    check_lwall_walk_ends_at_known_point(
        "item", "p17m0.walk.item :p17m0.lwall.flat");
}

void test_the_bf_walk_draws_lwall_to_the_right_place(void)
{
    check_lwall_walk_ends_at_known_point(
        "bf", "p17m0.walk.bf :p17m0.lwall.flat");
}

void test_the_foreach_walk_draws_lwall_to_the_right_place(void)
{
    check_lwall_walk_ends_at_known_point(
        "foreach", "p17m0.walk.foreach :p17m0.lwall.ys :p17m0.lwall.xs");
}

// The three candidates must agree on the 55-point creature shape too, not
// just the four-point wall -- a walk that only gets a short list right is
// still a bug the timing numbers would hide.
void test_all_three_walks_agree_on_the_creature(void)
{
    run("p17m0.setscale 0 :p17m0.norscl");
    run("p17m0.walk.item :p17m0.creature.flat");
    float item_x = num("item 1 pos");
    float item_y = num("item 2 pos");

    run("p17m0.walk.bf :p17m0.creature.flat");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, item_x, num("item 1 pos"));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, item_y, num("item 2 pos"));

    run("p17m0.walk.foreach :p17m0.creature.ys :p17m0.creature.xs");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, item_x, num("item 1 pos"));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, item_y, num("item 2 pos"));
}

// The `ys`/`xs` split must be the exact inverse of the flatten used to build
// the creature -- if it silently offset by one, every `foreach` figure would
// still "work" and just draw the wrong picture.
void test_the_parallel_split_agrees_with_the_flat_table(void)
{
    TEST_ASSERT_EQUAL_FLOAT(4, num("count :p17m0.lwall.ys"));
    TEST_ASSERT_EQUAL_FLOAT(4, num("count :p17m0.lwall.xs"));
    TEST_ASSERT_EQUAL_FLOAT(16, num("item 1 :p17m0.lwall.ys"));
    TEST_ASSERT_EQUAL_FLOAT(27, num("item 1 :p17m0.lwall.xs"));
    TEST_ASSERT_EQUAL_FLOAT(136, num("item 4 :p17m0.lwall.ys"));
    TEST_ASSERT_EQUAL_FLOAT(27, num("item 4 :p17m0.lwall.xs"));
}

// Section 6.3's own arithmetic: "a 60-point creature is 120 item calls."
// This one is 55, so it should intern to 55 y/x pairs -- 110 items flat.
void test_the_creature_is_fifty_five_points(void)
{
    TEST_ASSERT_EQUAL_FLOAT(55, num("count :p17m0.creature.ys"));
    TEST_ASSERT_EQUAL_FLOAT(55, num("count :p17m0.creature.xs"));
    TEST_ASSERT_EQUAL_FLOAT(110, num("count :p17m0.creature.flat"));
}

//==========================================================================
// The fade, section 8 and 8.1
//==========================================================================

// VECTOR.ASM's own column: one pixel in every VCTFAD+1, at A = 0 .. -7.
void test_the_dash_periods_match_the_rom_table(void)
{
    TEST_ASSERT_EQUAL_FLOAT(1, num("item 1 :p17m0.dash"));
    TEST_ASSERT_EQUAL_FLOAT(2, num("item 2 :p17m0.dash"));
    TEST_ASSERT_EQUAL_FLOAT(3, num("item 3 :p17m0.dash"));
    TEST_ASSERT_EQUAL_FLOAT(5, num("item 4 :p17m0.dash"));
    TEST_ASSERT_EQUAL_FLOAT(9, num("item 5 :p17m0.dash"));
    TEST_ASSERT_EQUAL_FLOAT(17, num("item 6 :p17m0.dash"));
    TEST_ASSERT_EQUAL_FLOAT(33, num("item 7 :p17m0.dash"));
    TEST_ASSERT_EQUAL_FLOAT(65, num("item 8 :p17m0.dash"));
}

// Section 8.1: the same table read as luminance.
void test_the_grey_levels_match_the_rom_table(void)
{
    TEST_ASSERT_EQUAL_FLOAT(255, num("item 1 :p17m0.grey"));
    TEST_ASSERT_EQUAL_FLOAT(128, num("item 2 :p17m0.grey"));
    TEST_ASSERT_EQUAL_FLOAT(85, num("item 3 :p17m0.grey"));
    TEST_ASSERT_EQUAL_FLOAT(51, num("item 4 :p17m0.grey"));
    TEST_ASSERT_EQUAL_FLOAT(28, num("item 5 :p17m0.grey"));
    TEST_ASSERT_EQUAL_FLOAT(15, num("item 6 :p17m0.grey"));
    TEST_ASSERT_EQUAL_FLOAT(8, num("item 7 :p17m0.grey"));
    TEST_ASSERT_EQUAL_FLOAT(4, num("item 8 :p17m0.grey"));
}

// `p17m0.fade.demo` must not error, on a mock with no real panel to check
// the result against by eye. The screen and palette side of it is what a
// board photograph judges (section 15's M0 gate 2).
void test_the_fade_demo_runs_without_error(void)
{
    run("p17m0.fade.demo");
}

//==========================================================================
// The whole script, end to end
//==========================================================================

void test_p17m0_script_runs_end_to_end(void)
{
    run("p17m0");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the three list walks"), screen);
    // `pr` tokenises "--" as two separate "-" words, so it renders as "- -".
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the 1.25x check"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "did this board take"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the gate, section 12"), screen);

    MockFile *report = mock_fs_get_file("p17m0.txt", false);
    TEST_ASSERT_NOT_NULL_MESSAGE(report, "p17m0.txt was not written");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->data, "the gate, section 12"), report->data);
}

// Q3's checks must all read OK against the same run the report prints --
// if the harness and the report ever disagree, this is what would show it.
void test_p17m0_script_reports_q3_all_ok(void)
{
    run("p17m0");
    const char *report = mock_fs_get_file("p17m0.txt", false)->data;
    TEST_ASSERT_NULL_MESSAGE(strstr(report, "FAIL"), report);
}

// Same convention as P13 M0: a timing script that leaves manual refresh set
// hands the prompt back frozen.
void test_the_script_puts_the_screen_back(void)
{
    run("p17m0");
    TEST_ASSERT_EQUAL_STRING("auto", value_to_string(eval_string("refreshmode").value));
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_scale_tables_are_ten_long);
    RUN_TEST(test_norscl_range_one_is_exactly_one_to_one);
    RUN_TEST(test_the_transform_matches_the_designs_range_one_check);
    RUN_TEST(test_the_transform_at_range_zero);
    RUN_TEST(test_the_transform_at_range_nine);
    RUN_TEST(test_the_item_walk_draws_lwall_to_the_right_place);
    RUN_TEST(test_the_bf_walk_draws_lwall_to_the_right_place);
    RUN_TEST(test_the_foreach_walk_draws_lwall_to_the_right_place);
    RUN_TEST(test_all_three_walks_agree_on_the_creature);
    RUN_TEST(test_the_parallel_split_agrees_with_the_flat_table);
    RUN_TEST(test_the_creature_is_fifty_five_points);
    RUN_TEST(test_the_dash_periods_match_the_rom_table);
    RUN_TEST(test_the_grey_levels_match_the_rom_table);
    RUN_TEST(test_the_fade_demo_runs_without_error);
    RUN_TEST(test_p17m0_script_runs_end_to_end);
    RUN_TEST(test_p17m0_script_reports_q3_all_ok);
    RUN_TEST(test_the_script_puts_the_screen_back);
    return UNITY_END();
}
