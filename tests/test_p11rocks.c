//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the Asteroids M0 timing harness (logo/tests/p11rocks).
//
//  M0 is the measurement that decided how an Asteroids frame gets erased, and
//  it has run: clear-and-redraw beat erase-in-place at every rock count the
//  game plays at (docs/asteroids-design.md section 3.3).  The harness is kept
//  runnable because it is the record of how those numbers were taken, and
//  because a second board -- a pico2 has never been measured -- would want it
//  unchanged.
//
//  Two things are worth pinning even in a timing script.  The outlines are
//  generated (scripts/gen_rocks.py) and pasted in, so the file is the only
//  place a bad paste would show; and an unclosed rock leaves a gap that looks
//  broken, which is exactly the kind of defect a screenful of numbers hides.
//  And the script has to run end to end before it is worth carrying to a
//  board -- one that dies half way through wastes a hardware session (the
//  p9m0 convention).
//
//  The game's own tests are in test_asteroids.c.  They are separate binaries
//  because both files define `place` and `draw.rock`, and the harness must
//  keep the shape it was measured in.
//

#include "test_mock_fs.h"
#include "mock_device.h"
#include "core/repl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef P11ROCKS_SOURCE
#error "P11ROCKS_SOURCE must be defined (path to logo/tests/p11rocks)"
#endif

// Load a whole Logo file, defining its procedures and running its top-level
// `make`s. Procedure definitions are not handled by the bare evaluator, so we
// buffer them and hand them to proc_define_from_text the way `load` does.
static void load_file(const char *path)
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
    load_file(P11ROCKS_SOURCE);
}

void tearDown(void)
{
    logo_io_close_all(&mock_io);
    test_scaffold_tearDown();
}

// Evaluate a Logo expression and return its number.
static float num(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    return r.value.as.number;
}

static void run(const char *input)
{
    Result r = run_string(input);
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, input);
}

//==========================================================================
// The scene
//==========================================================================

// Five parallel lists, hand-written, read by index every frame -- so a table
// edited unevenly is a silent out-of-range read rather than a visible defect.
void test_the_scene_tables_are_all_twelve_long(void)
{
    TEST_ASSERT_EQUAL_FLOAT(12, num("count :p11.x"));
    TEST_ASSERT_EQUAL_FLOAT(12, num("count :p11.y"));
    TEST_ASSERT_EQUAL_FLOAT(12, num("count :p11.sz"));
    TEST_ASSERT_EQUAL_FLOAT(12, num("count :p11.sh"));
    TEST_ASSERT_EQUAL_FLOAT(12, num("count :p11.a"));

    // Both subsets the script measures hold an equal number of each size, so
    // a 6- and a 9-rock reading are not secretly a heavier or lighter mix.
    // `0 +` because an element of a list literal is a word until something
    // does arithmetic on it, and the game's own `=` tests coerce it too.
    for (int size = 1; size <= 3; size++)
    {
        char expr[64];
        for (int upto = 6; upto <= 12; upto += 3)
        {
            int seen = 0;
            for (int i = 1; i <= upto; i++)
            {
                snprintf(expr, sizeof(expr), "0 + item %d :p11.sz", i);
                if ((int)num(expr) == size)
                    seen++;
            }
            char msg[96];
            snprintf(msg, sizeof(msg), "size %d appears %d times in the first %d rocks",
                     size, seen, upto);
            TEST_ASSERT_EQUAL_INT_MESSAGE(upto / 3, seen, msg);
        }
    }
}

//==========================================================================
// The outlines
//==========================================================================

// A rock is authored as radii and converted to a turtle walk by
// scripts/gen_rocks.py, because hand-written turns do not close.  Walk each
// outline at the origin and check it arrives back at the vertex it started
// from -- the design's section 6.3 promise, and the one property of the
// pasted-in literals that a bad paste would break.
static void assert_outline_closes(const char *name, int segments)
{
    run("clean  setpc 254  pu setx 0 sety 0 seth 0");
    mock_device_clear_graphics();
    run(name);

    char msg[128];
    snprintf(msg, sizeof(msg), "%s drew %d segments, expected %d",
             name, mock_device_line_count(), segments);
    TEST_ASSERT_EQUAL_INT_MESSAGE(segments, mock_device_line_count(), msg);

    const MockLine *first = mock_device_get_line(0);
    const MockLine *last = mock_device_get_line(segments - 1);
    float dx = last->x2 - first->x1;
    float dy = last->y2 - first->y1;
    float gap = sqrtf(dx * dx + dy * dy);
    snprintf(msg, sizeof(msg), "%s closes %.2f px from where it started", name, gap);
    TEST_ASSERT_TRUE_MESSAGE(gap < 1.0f, msg);
}

void test_every_outline_closes_on_itself(void)
{
    // Segment counts fall with size, which is where the saving belongs: the
    // small rocks are the numerous ones.
    assert_outline_closes("rock.a.l", 8);
    assert_outline_closes("rock.b.l", 8);
    assert_outline_closes("rock.c.l", 8);
    assert_outline_closes("rock.a.m", 6);
    assert_outline_closes("rock.b.m", 6);
    assert_outline_closes("rock.c.m", 6);
    assert_outline_closes("rock.a.s", 5);
    assert_outline_closes("rock.b.s", 5);
    assert_outline_closes("rock.c.s", 5);
}

// The prologue walks from the rock's stored centre out to its first vertex
// with the pen up, which is what lets the stored position mean the centre.
// If that ever drew, every rock would wear a spoke.
void test_the_walk_out_to_the_first_vertex_does_not_draw(void)
{
    run("clean  setpc 254  pu setx 0 sety 0 seth 0");
    mock_device_clear_graphics();
    run("rock.a.l");
    // Eight segments and not nine: the reach is pen-up, and there is no turn
    // after the last segment because `place` sets the heading every pass.
    TEST_ASSERT_EQUAL_INT(8, mock_device_line_count());
    // The first vertex is 21.1 steps straight ahead of the centre.
    const MockLine *first = mock_device_get_line(0);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, first->x1);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.1f, first->y1);
}

//==========================================================================
// Dispatch
//==========================================================================

// Nine outlines behind a two-level `if` chain, and the harness times what
// that chain costs.  It has to pick the right one, or the timing is of a
// scene nobody asked for.  Rock 1 is large outline A, which calib.dispatch
// also depends on.
void test_the_dispatch_draws_the_outline_it_names(void)
{
    run("clean  setpc 254  pu setx 0 sety 0 seth 0");
    mock_device_clear_graphics();
    run("rock.a.l");
    int direct = mock_device_line_count();
    float x2 = mock_device_get_line(direct - 1)->x2;

    run("clean  pu setx 0 sety 0 seth 0");
    mock_device_clear_graphics();
    run("draw.rock 1");
    TEST_ASSERT_EQUAL_INT(direct, mock_device_line_count());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, x2, mock_device_get_line(direct - 1)->x2);
}

//==========================================================================
// The erase pass
//==========================================================================

// The design's signature failure mode: an erase that does not retrace what
// drew the pixels leaves permanent litter on the canvas.  Here the two
// passes are one frame apart with nothing moving between them, so they must
// be identical segment for segment and differ only in the pen colour.
//
// Note what makes this checkable at all.  The erase is a pen *colour*, not
// `pe`: pen up/down/erase/reverse are one enum, so a `pe` would both be
// cancelled by the `pd` inside every outline's prologue and stop the mock
// recording the pass (mock_device.c records a line only with the pen DOWN).
void test_the_erase_pass_retraces_the_draw_pass(void)
{
    run("clean");
    mock_device_clear_graphics();
    run("frame.inplace 12");

    int drawn = mock_device_line_count();
    // 4 large, 4 medium, 4 small -> 76 segments a pass, twice.
    TEST_ASSERT_EQUAL_INT_MESSAGE(152, drawn, "one erase pass and one draw pass");

    int half = drawn / 2;
    for (int i = 0; i < half; i++)
    {
        const MockLine *erased = mock_device_get_line(i);
        const MockLine *redrawn = mock_device_get_line(i + half);
        char msg[96];
        snprintf(msg, sizeof(msg), "segment %d of the erase does not retrace the draw", i);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, redrawn->x1, erased->x1, msg);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, redrawn->y1, erased->y1, msg);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, redrawn->x2, erased->x2, msg);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, redrawn->y2, erased->y2, msg);
        TEST_ASSERT_EQUAL_INT_MESSAGE(255, erased->colour, "the erase pass is not in the background colour");
        TEST_ASSERT_EQUAL_INT_MESSAGE(254, redrawn->colour, "the draw pass is not in the pen colour");
    }
}

// Pen size stays 1.  A wide pen's round caps spill outside the stroke and, in
// wrap mode, across the screen edge -- the effect that made an early
// present-cost harness read every frame as a full screen (hardware-notes
// section 9.1), which would make M0's present column meaningless.
void test_the_rocks_are_drawn_with_a_one_pixel_pen(void)
{
    run("clean");
    mock_device_clear_graphics();
    run("frame.inplace 12");
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, mock_device_get_line(i)->pen_size,
                                      "a rock was drawn with a pen wider than one pixel");
    }
}

//==========================================================================
// The script itself
//==========================================================================

// It must run end to end before it is worth carrying to a board: a script
// that dies half way through wastes a hardware session, and its own numbers
// are only readable if the report reaches the file.
void test_p11rocks_script_runs(void)
{
    // The board runs 60 frames a point; the mock only has to reach every line.
    run("make \"p11.frames 2  make \"p11.calib 20");
    mock_device_clear_output();
    run("p11rocks");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "in place"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "clear redraw"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "drawing statement"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the decision"), screen);

    // And the same report reached the file, which is the copy that leaves the
    // board -- a screenful of numbers on the PicoCalc cannot be typed out.
    MockFile *report = mock_fs_get_file("p11rocks.txt", false);
    TEST_ASSERT_NOT_NULL_MESSAGE(report, "p11rocks.txt was not written");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->data, "the decision"), report->data);
}

// A timing script that leaves the screen in manual refresh hands the prompt
// back frozen: nothing the user types afterwards appears until something
// presents.  It is also how a board session gets thrown away.
void test_the_script_puts_the_screen_back(void)
{
    run("make \"p11.frames 2  make \"p11.calib 20");
    run("p11rocks");
    TEST_ASSERT_EQUAL_STRING("auto", value_to_string(eval_string("refreshmode").value));
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_scene_tables_are_all_twelve_long);
    RUN_TEST(test_every_outline_closes_on_itself);
    RUN_TEST(test_the_walk_out_to_the_first_vertex_does_not_draw);
    RUN_TEST(test_the_dispatch_draws_the_outline_it_names);
    RUN_TEST(test_the_erase_pass_retraces_the_draw_pass);
    RUN_TEST(test_the_rocks_are_drawn_with_a_one_pixel_pen);
    RUN_TEST(test_p11rocks_script_runs);
    RUN_TEST(test_the_script_puts_the_screen_back);
    return UNITY_END();
}
