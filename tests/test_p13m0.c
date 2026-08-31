//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the Battlezone M0 timing harness (tests/logo/p13m0).
//
//  M0 is the gate in docs/battlezone-design.md section 16, and it has NOT run.
//  The design's whole budget is host measurements scaled by 180x, and this is
//  the script that goes to a board to find out which of those numbers are
//  wrong.  Nothing here checks a timing -- the host is ~180x faster than the
//  target and `ticks` has millisecond resolution, so every figure the harness
//  produces reads as zero here.  What these tests check is that the script is
//  worth carrying to a board at all.
//
//  Three things are worth pinning even in a timing script:
//
//    * The projection has to be RIGHT.  A transposed cos/sin in the second
//      term is the classic error, it is invisible until you turn, and a
//      screenful of plausible milliseconds hides it completely.
//    * `frame.body` and `frame.raw` have to stay in step.  The first carries
//      the phase timers that split projection from drawing and the second does
//      not, and the difference between them is reported as the instrumentation
//      cost -- which is a lie the moment they draw different things.
//    * It has to run end to end, with the report reaching the file.  A script
//      that dies half way through wastes a hardware session (the p9m0
//      convention), and numbers on the PicoCalc's display cannot be copied
//      anywhere.
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

#ifndef P13M0_SOURCE
#error "P13M0_SOURCE must be defined (path to tests/logo/p11rocks)"
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
    // _and_hardware gives a clock for `ticks`; the mock filesystem is here for
    // the report, which goes to a file as well as the screen because numbers
    // on the PicoCalc's display cannot be copied off it.
    test_scaffold_setUp_with_device_and_hardware();
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
    load_file(P13M0_SOURCE);
    // The harness sets `window` at load; nothing here may quietly undo it.
    run_string("window");
}

void tearDown(void)
{
    logo_io_close_all(&mock_io);
    test_scaffold_tearDown();
}

// Evaluate a Logo expression and return its number.
//
// Through `value_to_number` and not `r.value.as.number`, because `item` hands
// back whatever the slot holds and a list element is a word until something
// does arithmetic on it -- reading the union directly gives a pointer
// reinterpreted as a float. test_p11rocks.c writes `0 + item ...` at every call
// site for the same reason; converting here fixes it once.
static float num(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    float n = 0.0f;
    TEST_ASSERT_TRUE_MESSAGE(value_to_number(r.value, &n), expr);
    return n;
}

// The clock reads back as a word now, not a number.
static const char *word(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    return value_to_string(r.value);
}

static void run(const char *input)
{
    Result r = run_string(input);
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, input);
}


//==========================================================================
// The scene
//==========================================================================

// Two parallel lists read by index every frame, so a table edited unevenly is
// a silent out-of-range read rather than a visible defect. Eight because the
// series' last point draws eight.
void test_the_obstacle_tables_are_both_eight_long(void)
{
    TEST_ASSERT_EQUAL_FLOAT(8, num("count :ox"));
    TEST_ASSERT_EQUAL_FLOAT(8, num("count :oz"));
}

// Every obstacle must be in front of a camera at the origin facing 0, because
// a culled object costs four comparisons and no divides -- a series that
// measured culling would not be measuring drawing, and would do it silently.
void test_every_obstacle_is_in_front_of_the_camera(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    for (int i = 1; i <= 8; i++)
    {
        char expr[96];
        snprintf(expr, sizeof(expr),
                 "project.box ((item %d :ox) - :px) ((item %d :oz) - :pz)", i, i);
        Result r = eval_string(expr);
        TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
        char msg[128];
        snprintf(msg, sizeof(msg), "obstacle %d is culled at the origin", i);
        TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(r.value), msg);
    }
}

// The column lists are the projection's whole output and are mutated in place;
// four long, because a box has four ground columns.
void test_the_column_tables_are_all_four_long(void)
{
    TEST_ASSERT_EQUAL_FLOAT(4, num("count :colx"));
    TEST_ASSERT_EQUAL_FLOAT(4, num("count :colb"));
    TEST_ASSERT_EQUAL_FLOAT(4, num("count :colt"));
}

//==========================================================================
// The projection
//==========================================================================

// The arithmetic, checked against a hand-computed answer. A box 300 steps
// dead ahead with the camera at the origin facing 0: the near face sits at
// z = 280 and the far at 320, its half-width is 20, and the focal length is
// 260. So the near corners project to x = +-260*20/280 = +-18.57 and the far
// to +-260*20/320 = +-16.25, and the near pair is the WIDER -- which is the
// whole of perspective and the thing a transposed term destroys.
void test_a_box_dead_ahead_projects_to_a_known_quad(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    run("ignore project.box 0 300");

    // Corners 1 and 2 are the far pair (z = 320), 3 and 4 the near (z = 280).
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 16.25f, num("item 1 :colx"));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -16.25f, num("item 4 :colx"));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 18.571f, num("item 2 :colx"));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -18.571f, num("item 3 :colx"));

    // And the heights. The eye is 12 above the plain and the horizon is at
    // y = 40, so a ground corner at z = 280 sits at 40 - 260*12/280 = 28.86
    // and its top, 40 steps up, at 40 + 260*28/280 = 66.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 28.857f, num("item 2 :colb"));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 66.0f, num("item 2 :colt"));
}

// The near pair must be further apart than the far pair. Stated on its own
// because it is the one property that survives every constant being retuned,
// and because a cos/sin transposition can still land plausible numbers.
void test_the_near_corners_are_wider_than_the_far_ones(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    run("ignore project.box 0 300");
    float far_span = num("(item 1 :colx) - (item 4 :colx)");
    float near_span = num("(item 2 :colx) - (item 3 :colx)");
    TEST_ASSERT_TRUE_MESSAGE(near_span > far_span,
        "the near face of a box must project wider than the far face");
}

// Turning the camera right must move a box dead ahead to the LEFT of the
// screen, and by more than turning it a little. Sign errors in the rotation
// are otherwise invisible from a single reading.
void test_turning_right_moves_the_scene_left(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    run("ignore project.box 0 300");
    float ahead = num("item 1 :colx");

    run("make \"ph 10  cam.setup  ignore project.box 0 300");
    float turned = num("item 1 :colx");

    run("make \"ph 20  cam.setup  ignore project.box 0 300");
    float turned_more = num("item 1 :colx");

    TEST_ASSERT_TRUE_MESSAGE(turned < ahead, "a right turn must sweep the scene left");
    TEST_ASSERT_TRUE_MESSAGE(turned_more < turned, "and further for a bigger turn");
}

// Culling is conservative: ANY column inside the near plane culls the whole
// box. Not "all" -- one corner behind the camera swings the projection through
// infinity and throws a line across the screen (design section 9). Both halves,
// because the permissive version is the one that looks broken on a board.
void test_a_box_behind_the_camera_is_culled(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    Result r = eval_string("project.box 0 -300");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(r.value));
}

void test_a_box_straddling_the_near_plane_is_culled_whole(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    // Centre at z = 20 with a half-width of 20 puts the near face at z = 0,
    // inside `near` (8), while the far face at z = 40 is comfortably outside.
    Result r = eval_string("project.box 0 20");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", value_to_string(r.value),
        "one column inside the near plane must cull the whole box");
}

// The enemy's hull uses the same column trick with its own rotation folded in,
// so it must behave the same way at the two ends.
void test_the_enemy_is_projected_in_front_and_culled_behind(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    run("make \"ex 0  make \"ez 300  make \"eh 200");
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string("project.enemy").value));

    run("make \"ez -300");
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string("project.enemy").value));
    run("make \"ez 300");
}

// A rotating hull keeps its size: spin the enemy through a full turn and its
// projected width must stay within the bounds a square seen face-on and
// corner-on allows. A transposition inside the combined rotation shows up here
// as a hull that grows or collapses.
//
// The extent is min-to-max across all four columns, not corner 1 to corner 3:
// which corner is leftmost changes as the hull turns, so a fixed pair reads a
// collapse that is only the corners having swapped places.
void test_the_enemy_hull_keeps_its_size_through_a_full_turn(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    run("make \"ex 0  make \"ez 600");

    for (int heading = 0; heading < 360; heading += 15)
    {
        char expr[96];
        snprintf(expr, sizeof(expr), "make \"eh %d  ignore project.enemy", heading);
        run(expr);

        float lo = num("item 1 :colx");
        float hi = lo;
        for (int c = 2; c <= 4; c++)
        {
            snprintf(expr, sizeof(expr), "item %d :colx", c);
            float v = num(expr);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        float span = hi - lo;

        // Half-width 14 at z ~= 600 with k = 260: a face-on hull spans about
        // 2*14*260/600 = 12.1 and a corner-on one about 17.2. Generous bounds,
        // because the point is to catch a collapse or a blow-up, not to pin the
        // geometry twice.
        char msg[128];
        snprintf(msg, sizeof(msg), "hull spans %.2f at heading %d", span, heading);
        TEST_ASSERT_TRUE_MESSAGE(span > 9.0f && span < 21.0f, msg);
    }
}

//==========================================================================
// The drawing
//==========================================================================

// Twelve edges: the bottom quad, the top quad, and the four verticals. The
// count is what the design's section 12 budgets against, so it is worth an
// assertion rather than a comment.
void test_a_box_draws_twelve_edges(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    run("ignore project.box 0 300");
    run("clean");
    mock_device_clear_graphics();
    run("draw.box");
    TEST_ASSERT_EQUAL_INT(12, mock_device_line_count());
}

// And the quads close on themselves. An unclosed box is one missing edge in a
// wireframe, which reads as a rendering bug rather than as a bad table.
void test_the_box_quads_close(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    run("ignore project.box 0 300");
    run("clean");
    mock_device_clear_graphics();
    run("draw.box");

    // The bottom quad is lines 0-3 and the top quad lines 4-7.
    const MockLine *bottom_first = mock_device_get_line(0);
    const MockLine *bottom_last = mock_device_get_line(3);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, bottom_first->x1, bottom_last->x2);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, bottom_first->y1, bottom_last->y2);

    const MockLine *top_first = mock_device_get_line(4);
    const MockLine *top_last = mock_device_get_line(7);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, top_first->x1, top_last->x2);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, top_first->y1, top_last->y2);
}

// The enemy is its hull plus the gun line.
void test_the_enemy_draws_thirteen_edges(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0  cam.setup");
    run("make \"ex 0  make \"ez 300  make \"eh 200  ignore project.enemy");
    run("clean");
    mock_device_clear_graphics();
    run("draw.enemy");
    TEST_ASSERT_EQUAL_INT(13, mock_device_line_count());
}

//==========================================================================
// The horizon
//==========================================================================

// The design's section 8.4 assumed "about 12 of 32" points fall inside the
// field of view. They do not: 32 points over 360 degrees at 5.06 steps a
// degree are 57 steps apart, and a 320-step viewport holds about seven. This
// test states the real number so that changing the table is a decision rather
// than an accident.
void test_the_horizon_cull_keeps_about_seven_of_thirty_two_points(void)
{
    TEST_ASSERT_EQUAL_FLOAT(32, num("count :mtn"));
    run("make \"ph 0  cam.setup  clean  horizon");
    float seen = num(":mtn.seen");
    char msg[96];
    snprintf(msg, sizeof(msg), "%d horizon points were on screen", (int)seen);
    TEST_ASSERT_TRUE_MESSAGE(seen >= 5 && seen <= 9, msg);
}

// A polyline of n points is n-1 segments, so a horizon of `seen` points drawn
// as `runs` separate polylines is `seen - runs` lines.
//
// THIS TEST FOUND A REAL DEFECT and is the reason the harness has `mtn.runs` at
// all. The table is scanned in index order and drawn in screen order, and those
// differ: at heading 0 the visible points are indices 1-4 (azimuth 0 to 33.75)
// and 30-32 (which wrap to -33.75 to -11.25), so a single polyline stepped from
// +171 to -171 in one segment and stroked a line clear across the screen. It
// looked exactly like a rendering fault, and nothing about the timing would
// have shown it.
void test_the_horizon_draws_a_connected_polyline(void)
{
    run("make \"ph 0  cam.setup  clean");
    mock_device_clear_graphics();
    run("horizon");
    int seen = (int)num(":mtn.seen");
    int runs = (int)num(":mtn.runs");
    TEST_ASSERT_TRUE_MESSAGE(runs >= 1, "the first point must lift the pen");
    TEST_ASSERT_EQUAL_INT(seen - runs, mock_device_line_count());

    // And every segment starts where the last one ended, which is what says the
    // kept points are contiguous within each run.
    for (int i = 1; i < mock_device_line_count(); i++)
    {
        const MockLine *prev = mock_device_get_line(i - 1);
        const MockLine *cur = mock_device_get_line(i);
        if (fabsf(prev->x2 - cur->x1) < 0.01f)
            TEST_ASSERT_FLOAT_WITHIN(0.01f, prev->y2, cur->y1);
    }
}

// No segment may span more than the gap between two adjacent points. This is
// the assertion that would have caught the wrap directly: the bad segment was
// 342 steps wide against a legitimate 57.
void test_no_horizon_segment_spans_the_whole_screen(void)
{
    for (int heading = 0; heading < 360; heading += 5)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "make \"ph %d  cam.setup  clean", heading);
        run(expr);
        mock_device_clear_graphics();
        run("horizon");

        for (int i = 0; i < mock_device_line_count(); i++)
        {
            const MockLine *line = mock_device_get_line(i);
            float span = fabsf(line->x2 - line->x1);
            char msg[128];
            snprintf(msg, sizeof(msg), "segment %d spans %.1f at heading %d",
                     i, span, heading);
            TEST_ASSERT_TRUE_MESSAGE(span < 80.0f, msg);
        }
    }
}

// Turning must move the range across the screen, or the mountains are not
// doing the one job they have -- telling the player they are turning.
void test_the_horizon_scrolls_with_the_heading(void)
{
    run("make \"ph 0  cam.setup  clean");
    mock_device_clear_graphics();
    run("horizon");
    float at_zero = mock_device_get_line(0)->x1;

    run("make \"ph 10  cam.setup  clean");
    mock_device_clear_graphics();
    run("horizon");
    float at_ten = mock_device_get_line(0)->x1;

    TEST_ASSERT_TRUE_MESSAGE(fabsf(at_ten - at_zero) > 1.0f,
        "the horizon must move when the camera turns");
}

//==========================================================================
// The two frames
//==========================================================================

// The reported instrumentation cost is `frame.body` minus `frame.raw`, which
// is a lie the moment the two draw different things. This is the harness's
// version of P11's test_the_harness_frame_matches_the_game_frame.
void test_the_timed_and_untimed_frames_draw_the_same_thing(void)
{
    for (int shown = 1; shown <= 8; shown *= 2)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "make \"shown %d", shown);
        run(expr);
        run("make \"px 0  make \"pz 0  make \"ph 0");

        mock_device_clear_graphics();
        run("frame.raw");
        int raw_lines = mock_device_line_count();

        mock_device_clear_graphics();
        run("frame.body");
        int timed_lines = mock_device_line_count();

        char msg[128];
        snprintf(msg, sizeof(msg), "at %d objects: raw drew %d, timed drew %d",
                 shown, raw_lines, timed_lines);
        TEST_ASSERT_EQUAL_INT_MESSAGE(raw_lines, timed_lines, msg);
        TEST_ASSERT_TRUE_MESSAGE(raw_lines > 0, msg);
    }
    run("make \"shown 1");
}

// The frame has to be linear in the object count or the series says nothing.
// Each extra box is twelve more edges; the horizon and the enemy are flat.
void test_the_frame_grows_by_twelve_edges_an_object(void)
{
    run("make \"px 0  make \"pz 0  make \"ph 0");

    run("make \"shown 1");
    mock_device_clear_graphics();
    run("frame.raw");
    int one = mock_device_line_count();

    run("make \"shown 4");
    mock_device_clear_graphics();
    run("frame.raw");
    int four = mock_device_line_count();

    TEST_ASSERT_EQUAL_INT_MESSAGE(36, four - one,
        "three more boxes should be 36 more edges");
    run("make \"shown 1");
}

// Logo is dynamically scoped, so a procedure's `make` finds the innermost
// binding of that name anywhere up the call chain -- including a caller's
// `local`. `cam.setup` declares no locals and writes four globals (`cs`, `sn`,
// `a`, `b`), so any caller holding one of those names as a local has it
// overwritten once a frame.
//
// THIS IS NOT HYPOTHETICAL. The first board run of this harness reported
// 0.00 ms in every body column, because `measure`'s accumulator was called `b`
// and `cam.setup` wrote `:half * :sn` into it with the camera at heading 0.
// The run's most important column was gone and nothing about it looked wrong.
//
// `local` protects a name from the world; it does not protect the world from a
// name. So `measure` prefixes its accumulators `m.` and this test walks them.
void test_the_frame_does_not_write_the_measure_accumulators(void)
{
    // The names are READ OUT OF THE HARNESS, not listed here. A hardcoded list
    // pins the names that happen to be in the file; reading them back pins the
    // property, so renaming an accumulator to `b` fails this test instead of
    // silently reintroducing the defect.
    char names[32][64];
    int count = 0;

    FILE *f = fopen(P13M0_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL(f);
    char line[512];
    bool in_measure = false;
    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "to measure", 10) == 0)
        {
            in_measure = true;
            continue;
        }
        if (!in_measure)
            continue;
        const char *p = strstr(line, "local \"");
        if (p == NULL)
            break;  // the `local` block is contiguous and comes first
        while (p != NULL && count < 32)
        {
            p += 7;
            int n = 0;
            while (p[n] != '\0' && !isspace((unsigned char)p[n]) && n < 63)
                n++;
            memcpy(names[count], p, (size_t)n);
            names[count][n] = '\0';
            count++;
            p = strstr(p, "local \"");
        }
    }
    fclose(f);
    TEST_ASSERT_TRUE_MESSAGE(count >= 8, "no locals found in `measure`");

    run("make \"shown 2");
    for (int i = 0; i < count; i++)
    {
        char def[512];
        snprintf(def, sizeof(def),
                 "to probe\n"
                 "  local \"%s\n"
                 "  make \"%s 12345\n"
                 "  frame.raw\n"
                 "  frame.body\n"
                 "  output :%s\n"
                 "end",
                 names[i], names[i], names[i]);
        Result d = proc_define_from_text(def);
        TEST_ASSERT_MESSAGE(d.status != RESULT_ERROR, def);

        char msg[160];
        snprintf(msg, sizeof(msg),
                 "a frame overwrote `measure`'s local `%s` -- rename it", names[i]);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(12345.0f, num("probe"), msg);
    }
    run("make \"shown 1");
}

// The other half of the same hazard, stated positively: `cam.setup` is the one
// procedure in this file that declares no locals, so it is the whole leak
// surface. If it ever gains a fifth global, this test says so and the
// accumulator list above has to be checked against it.
void test_cam_setup_writes_exactly_four_names(void)
{
    static const char *written[] = { "cs", "sn", "a", "b" };

    for (size_t i = 0; i < sizeof(written) / sizeof(written[0]); i++)
    {
        char expr[128];
        snprintf(expr, sizeof(expr), "make \"%s 12345", written[i]);
        run(expr);
    }
    run("make \"ph 30  cam.setup");

    for (size_t i = 0; i < sizeof(written) / sizeof(written[0]); i++)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), ":%s", written[i]);
        char msg[128];
        snprintf(msg, sizeof(msg), "cam.setup did not write `%s`", written[i]);
        TEST_ASSERT_TRUE_MESSAGE(fabsf(num(expr) - 12345.0f) > 0.5f, msg);
    }
    run("make \"ph 0  cam.setup");
}

// The harness may be pointed at a clock, and it must report the one the
// hardware actually took rather than the one it was asked for -- a board that
// refused the change would otherwise read as an overclock that bought nothing.
void test_the_harness_reports_the_clock_it_actually_ran_at(void)
{
    run("make \"p13m0.frames 2");

    run("make \"p13m0.cpu \"fast");
    run("p13m0");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("fast", word(":p13m0.cpu.ran"),
        "asked for fast and the mock can make it");

    run("make \"p13m0.cpu \"normal");
    run("p13m0");
    TEST_ASSERT_EQUAL_STRING("normal", word(":p13m0.cpu.ran"));
    run("make \"p13m0.cpu \"same");
}

// `same` means "leave the board wherever it was", which is what every run
// before this feature existed did.
void test_the_same_clock_leaves_the_board_alone(void)
{
    run("make \"p13m0.frames 2");
    run("make \"p13m0.cpu \"fast");
    run("p13m0");

    run("make \"p13m0.cpu \"same");
    run("p13m0");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("fast", word(":p13m0.cpu.ran"),
        "`same` retuned the board instead of leaving it");

    run("hw.setcpu \"normal");
    run("make \"p13m0.cpu \"same");
}

// The temperature is read either side of the run and reported. A board with no
// sensor reports 0 rather than dying on the harness's last line.
void test_the_harness_reads_the_temperature_either_side(void)
{
    set_mock_temperature(true, 31.5f);
    run("make \"p13m0.frames 2");
    run("p13m0");

    TEST_ASSERT_EQUAL_FLOAT(31.5f, num(":p13m0.temp0"));
    TEST_ASSERT_EQUAL_FLOAT(31.5f, num(":p13m0.temp1"));
}

void test_the_harness_survives_a_board_with_no_sensor_or_clock(void)
{
    set_mock_temperature(false, 0.0f);
    set_mock_cpu_khz(false, 150000u);
    run("make \"p13m0.frames 2  make \"p13m0.cpu \"fast");

    mock_device_clear_output();
    run("p13m0");

    // It still reached its last line.
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(mock_device_get_output(), "the gate"),
                                 mock_device_get_output());
    TEST_ASSERT_EQUAL_STRING("unknown", word(":p13m0.cpu.ran"));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, num(":p13m0.temp0"));

    set_mock_temperature(true, 25.0f);
    set_mock_cpu_khz(true, 150000u);
    run("make \"p13m0.cpu \"same");
}

//==========================================================================
// The script
//==========================================================================

// It must run end to end before it is worth carrying to a board: a script that
// dies half way through wastes a hardware session, and its own numbers are only
// readable if the report reaches the file.
void test_p13m0_script_runs(void)
{
    // The board runs 200 frames a point; the mock only has to reach every line.
    run("make \"p13m0.frames 2");
    mock_device_clear_output();
    run("p13m0");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "does the 180x ratio hold"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "what a long line costs"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "what the present costs"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the series"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the gate"), screen);

    // And the same report reached the file, which is the copy that leaves the
    // board -- a screenful of numbers on the PicoCalc cannot be typed out.
    MockFile *report = mock_fs_get_file("p13m0.txt", false);
    TEST_ASSERT_NOT_NULL_MESSAGE(report, "p13m0.txt was not written");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->data, "the gate"), report->data);
}

// A timing script that leaves the screen in manual refresh hands the prompt
// back frozen: nothing the user types afterwards appears until something
// presents. It is also how a board session gets thrown away.
void test_the_script_puts_the_screen_back(void)
{
    run("make \"p13m0.frames 2");
    run("p13m0");
    TEST_ASSERT_EQUAL_STRING("auto", value_to_string(eval_string("refreshmode").value));
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_obstacle_tables_are_both_eight_long);
    RUN_TEST(test_every_obstacle_is_in_front_of_the_camera);
    RUN_TEST(test_the_column_tables_are_all_four_long);
    RUN_TEST(test_a_box_dead_ahead_projects_to_a_known_quad);
    RUN_TEST(test_the_near_corners_are_wider_than_the_far_ones);
    RUN_TEST(test_turning_right_moves_the_scene_left);
    RUN_TEST(test_a_box_behind_the_camera_is_culled);
    RUN_TEST(test_a_box_straddling_the_near_plane_is_culled_whole);
    RUN_TEST(test_the_enemy_is_projected_in_front_and_culled_behind);
    RUN_TEST(test_the_enemy_hull_keeps_its_size_through_a_full_turn);
    RUN_TEST(test_a_box_draws_twelve_edges);
    RUN_TEST(test_the_box_quads_close);
    RUN_TEST(test_the_enemy_draws_thirteen_edges);
    RUN_TEST(test_the_horizon_cull_keeps_about_seven_of_thirty_two_points);
    RUN_TEST(test_the_horizon_draws_a_connected_polyline);
    RUN_TEST(test_no_horizon_segment_spans_the_whole_screen);
    RUN_TEST(test_the_horizon_scrolls_with_the_heading);
    RUN_TEST(test_the_timed_and_untimed_frames_draw_the_same_thing);
    RUN_TEST(test_the_frame_grows_by_twelve_edges_an_object);
    RUN_TEST(test_the_frame_does_not_write_the_measure_accumulators);
    RUN_TEST(test_cam_setup_writes_exactly_four_names);
    RUN_TEST(test_the_harness_reports_the_clock_it_actually_ran_at);
    RUN_TEST(test_the_same_clock_leaves_the_board_alone);
    RUN_TEST(test_the_harness_reads_the_temperature_either_side);
    RUN_TEST(test_the_harness_survives_a_board_with_no_sensor_or_clock);
    RUN_TEST(test_p13m0_script_runs);
    RUN_TEST(test_the_script_puts_the_screen_back);
    return UNITY_END();
}
