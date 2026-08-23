//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for Battlezone (P13) M1 -- the world, the camera, the treads, the
//  horizon and the gunsight.  See docs/battlezone-design.md section 17.
//
//  Nothing here checks a timing.  The host is 150-180x faster than the target
//  and `ticks` has millisecond resolution, so every figure the game reports
//  reads as zero here; the frame budget needs a board (docs/measurements/).
//  What these check is the half of the milestone a board cannot: that the
//  projection is RIGHT.
//
//  M1 is the milestone that proves the transform, because a wrong one is
//  obvious the moment you drive past a cube -- and completely invisible in a
//  screenful of plausible milliseconds.  Five of these tests come straight from
//  the design's section 17 list, and each names the failure it exists for:
//
//    * A transposed cos/sin in the second term is the classic 3D error, it is
//      invisible until you turn, and it survives every test that only drives
//      forward.  So the projection is checked against coordinates computed by
//      hand, at a heading that is not zero.
//    * Culling is conservative -- ANY column inside the near plane, not all.
//      The "any" version is the one that swings the projection through infinity
//      and throws a line across the whole screen.
//    * The plain wraps in the ARITHMETIC and not only in the drawing.  B19 is
//      the precedent: Asteroids got the picture right and the comparison wrong
//      and shipped it that way.
//    * The horizon cull keeps the visible points.  Off-by-one at the edge of
//      the field of view is a mountain that flicks in and out as you turn.
//    * No horizon segment spans the whole screen.  M0's harness drew one and it
//      looked exactly like a projection fault; the cause was `wrap`.
//
//  And one that is about how this file is written rather than what it computes:
//  the hot path is on prefixed globals (design section 13, L0.5), which is worth
//  3.9 ms a frame and costs the safety of scope.  M0 lost its entire body column
//  to a temporary called `b` colliding with a camera constant of that name, so
//  `test_every_hot_path_temporary_is_prefixed` reads the names back out of the
//  Logo source and fails on a bare one.
//

#include "test_mock_fs.h"
#include "test_scaffold.h"
#include "mock_device.h"
#include "core/repl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef BATTLEZONE_SOURCE
#error "BATTLEZONE_SOURCE must be defined (path to logo/games/battlezone)"
#endif

// Edges per object.  A cube is twelve: the bottom quad, the top quad and the
// four verticals.  A pyramid is eight: the base quad and four to the apex.
#define EDGES_CUBE 12
#define EDGES_PYR   8

// The gunsight is a fixed overlay with no arithmetic in it at all.
#define EDGES_SIGHT 6

// The horizon walks `mtn.seen` + 1 points, so it strokes `mtn.seen` segments.
#define SEGS_HORIZON 9

// PicoCalc key codes, as the game names them to `keydown?`/`keyhit?`.
#define KEY_LEFT   180
#define KEY_UP     181
#define KEY_DOWN   182
#define KEY_RIGHT  183
#define KEY_PAUSE  112
#define KEY_QUIT   113

// Load a whole Logo file, defining its procedures and running its top-level
// tuning `make`s.  Procedure definitions are not handled by the bare evaluator,
// so we buffer them and hand them to proc_define_from_text the way `load` does.
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
    // _and_hardware gives a clock for `ticks` and for the frame pacing.
    test_scaffold_setUp_with_device_and_hardware();
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
    load_file(BATTLEZONE_SOURCE);
    // The game sets `window` in `battlezone`; the tests below call the frame
    // pieces directly, so they need it too or the default `wrap` folds every
    // off-screen point back onto the far side.
    run_string("window");
}

void tearDown(void)
{
    logo_io_close_all(&mock_io);
    test_scaffold_tearDown();
}

static float num(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    return r.value.as.number;
}

// An element of a flat list is a word until something does arithmetic on it,
// so read one through a `0 +` the way the game's own comparisons coerce it.
static float item_of(const char *list, int i)
{
    char expr[64];
    snprintf(expr, sizeof(expr), "0 + item %d :%s", i, list);
    return num(expr);
}

static void run(const char *input)
{
    Result r = run_string(input);
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, input);
}

// A Logo boolean is the word `true` or `false`, so read it as one.
static bool truth(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    return strcmp(value_to_string(r.value), "true") == 0;
}

static void press(int key_code) { set_mock_key_down(key_code, true); }
static void release(int key_code) { set_mock_key_down(key_code, false); }

// Put the camera somewhere and point it somewhere, then hoist the per-frame
// constants the projection and the field scan read. `step.tank` hoists these in
// two places -- the heading's before the move and the position's after it --
// which is why there are two procedures rather than one.
static void camera_at(float px, float pz, float ph)
{
    char expr[160];
    snprintf(expr, sizeof(expr),
             "make \"px %g  make \"pz %g  make \"ph %g  cam.setup  cam.offsets",
             px, pz, ph);
    run(expr);
}

// Project one object at a world-space offset from the camera, the way
// `draw.field` does: through the `pb.` globals rather than as parameters.
static bool project(const char *which, float dx, float dz)
{
    char expr[160];
    snprintf(expr, sizeof(expr), "make \"p.dx %g  make \"p.dz %g", dx, dz);
    run(expr);
    snprintf(expr, sizeof(expr), "%s", which);
    return truth(expr);
}

//==========================================================================
// The file loads, and its constants are the ones the design cut
//==========================================================================

void test_file_loads_and_sets_its_tuning(void)
{
    TEST_ASSERT_EQUAL_FLOAT(15, num(":fps"));
    // k 260 puts the half-width 160 at atan(160/260) = 31.6 degrees, so the
    // field of view is 63 degrees -- close to the cabinet's.
    TEST_ASSERT_EQUAL_FLOAT(260, num(":k"));
    // The optical centre of a 240-row split viewport is y = +40, not 0
    // (design section 6), and every screen constant is cut against it.
    TEST_ASSERT_EQUAL_FLOAT(40, num(":hz"));
    TEST_ASSERT_EQUAL_FLOAT(12, num(":eye"));
    TEST_ASSERT_EQUAL_FLOAT(8, num(":ob.count"));
}

// The rate-versus-density choice design section 12.3.1b opens -- three
// obstacles at 24 fps or twelve at 15 -- belongs to M4, so M1 has to have
// written the cap as something M4 can turn.  If this is ever inlined into
// `draw.field` as a literal, this test is what says so.
void test_max_obstacles_is_a_constant_the_frame_reads(void)
{
    TEST_ASSERT_EQUAL_FLOAT(3, num(":max.obstacles"));

    camera_at(800, 800, 0);
    run("make \"max.obstacles 0");
    mock_device_clear_graphics();
    run("draw.field");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(),
                                  "the object cap is not read from :max.obstacles");
}

//==========================================================================
// The projection
//==========================================================================

// The test the design asked for by name: known camera, known world point,
// known screen coordinate, computed by hand.  At heading 0 a transposed cos/sin
// in the second term is invisible, because cs = 1 and sn = 0 make both
// spellings agree -- so this drives the check at 30 degrees, where they do not.
//
//   xc = dx*cos(30) - dz*sin(30),  zc = dz*cos(30) + dx*sin(30)
//
// with the camera at the origin of the offsets, `half` 20 and `a`/`b` hoisted.
void test_the_projection_is_right_at_a_heading_that_is_not_zero(void)
{
    camera_at(800, 800, 30);

    const float cs = cosf(30.0f * (float)M_PI / 180.0f);
    const float sn = sinf(30.0f * (float)M_PI / 180.0f);
    const float half = 20.0f, k = 260.0f, eye = 12.0f, hz = 40.0f, boxh = 40.0f;
    const float dx = 120.0f, dz = 400.0f;

    TEST_ASSERT_TRUE(project("project.box", dx, dz));

    const float xc = dx * cs - dz * sn;
    const float zc = dz * cs + dx * sn;
    const float a = half * cs, b = half * sn;

    // Column 1 is the (+a, -b) corner in x and the (+a, +b) corner in z, which
    // is the pairing that makes the four come out in order around the box so
    // that the quads chain without a pen-up.
    const float z1 = zc + a + b;
    const float iz = k / z1;
    TEST_ASSERT_FLOAT_WITHIN(0.02f, (xc + a - b) * iz, item_of("cx", 1));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, hz - eye * iz, item_of("cy1", 1));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, hz + (boxh - eye) * iz, item_of("cy2", 1));

    const float z3 = zc - a - b;
    const float iz3 = k / z3;
    TEST_ASSERT_FLOAT_WITHIN(0.02f, (xc - a + b) * iz3, item_of("cx", 3));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, hz - eye * iz3, item_of("cy1", 3));
}

// The transposition itself, stated as a property rather than as a number: an
// object dead ahead has to move to the LEFT of the view when the camera turns
// right, and the classic error moves it the other way.
void test_turning_right_sweeps_the_world_to_the_left(void)
{
    camera_at(0, 0, 0);
    TEST_ASSERT_TRUE(project("project.box", 0, 400));
    const float ahead = item_of("cx", 1);

    camera_at(0, 0, 15);
    TEST_ASSERT_TRUE(project("project.box", 0, 400));
    const float turned = item_of("cx", 1);

    TEST_ASSERT_TRUE_MESSAGE(turned < ahead - 20.0f,
                             "turning right did not sweep the scene left: cos/sin transposed?");
}

// An object behind the camera has to be culled, and this is the check that a
// sign error in `zc` would fail while every forward-facing test passed.
void test_an_object_behind_the_camera_is_culled(void)
{
    camera_at(0, 0, 0);
    TEST_ASSERT_FALSE(project("project.box", 0, -400));
}

// Design section 17: culling is conservative.  ANY column inside the near plane
// and the object goes, because one corner behind you swings the projection
// through infinity and throws a line across the whole screen.  Both halves,
// because it is the "any" version that is load-bearing.
void test_culling_is_conservative_at_the_near_plane(void)
{
    camera_at(0, 0, 0);
    const float near = num(":near");
    const float half = num(":half");

    // Entirely in front: the nearest column is `half` beyond the near plane.
    TEST_ASSERT_TRUE(project("project.box", 0, near + half + 1.0f));

    // Straddling it: the centre is in front but the near face is not.
    TEST_ASSERT_FALSE(project("project.box", 0, near + half - 1.0f));
}

// A pyramid is the same four ground columns with the top ring collapsed to a
// point, and the column half of `project.pyr` is a copy of `project.box`'s.
// This is what keeps the copy honest.
void test_the_two_projections_agree_on_their_columns(void)
{
    camera_at(800, 800, 47);

    TEST_ASSERT_TRUE(project("project.box", 150, 380));
    float bx[4], bb[4];
    for (int i = 0; i < 4; i++)
    {
        bx[i] = item_of("cx", i + 1);
        bb[i] = item_of("cy1", i + 1);
    }

    TEST_ASSERT_TRUE(project("project.pyr", 150, 380));
    for (int i = 0; i < 4; i++)
    {
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(bx[i], item_of("cx", i + 1),
                                        "the pyramid's columns drifted from the cube's");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(bb[i], item_of("cy1", i + 1),
                                        "the pyramid's ground line drifted from the cube's");
    }

    // And the apex is the object's own centre column lifted -- one extra
    // divide, which is the whole reason a pyramid is the cheaper obstacle.
    const float cs = cosf(47.0f * (float)M_PI / 180.0f);
    const float sn = sinf(47.0f * (float)M_PI / 180.0f);
    const float xc = 150.0f * cs - 380.0f * sn;
    const float zc = 380.0f * cs + 150.0f * sn;
    const float iz = 260.0f / zc;
    TEST_ASSERT_FLOAT_WITHIN(0.02f, xc * iz, num(":apx"));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 40.0f + (num(":pyrh") - 12.0f) * iz, num(":apy"));
}

// Design section 9 gives the near plane a second job and it is the binding one:
// projected size goes as k*h/z, so `near` is what bounds how much screen an
// edge covers, and an edge costs 0.35-0.98 us a step.  At `near` 60 a cube tops
// out at about one screen; at `near` 8 it would be four screens tall and a
// dozen of its edges would be most of a frame.
void test_the_near_plane_bounds_a_cube_to_about_one_screen(void)
{
    camera_at(0, 0, 0);
    const float near = num(":near");
    const float half = num(":half");

    // The closest a cube can legally get: every column just outside the plane.
    TEST_ASSERT_TRUE(project("project.box", 0, near + half + 0.5f));
    const float tall = item_of("cy2", 1) - item_of("cy1", 1);
    TEST_ASSERT_TRUE_MESSAGE(tall < 240.0f, "a cube at the near plane is taller than the viewport");
    TEST_ASSERT_TRUE_MESSAGE(tall > 100.0f, "a cube at the near plane is too small to read");
}

//==========================================================================
// The world wraps, in the arithmetic
//==========================================================================

// B19's precedent, and the design asked for this one by name.  The plain wraps,
// so an obstacle just past the seam is a few steps ahead of a camera just short
// of it -- not a world away.  Checked on the numbers `draw.field` computes, not
// on the picture it draws.
void test_the_plain_wraps_in_the_arithmetic(void)
{
    const float world = num(":world");

    // One obstacle at x = 40, the rest pushed out of range behind the camera.
    run("make \"ox [40 40 40 40]  make \"ox se :ox [40 40 40 40]");
    run("make \"oz [900 900 900 900]  make \"oz se :oz [900 900 900 900]");

    // The camera sits 60 steps short of the seam, looking along +x.
    camera_at(world - 60.0f, 900.0f, 90.0f);
    // The three statements `draw.field` walks: translate by the hoisted camera
    // offset, fold, recentre.
    run("make \"ob.u :half.world - :px");
    run("make \"ob.d :ob.u + item 1 :ox");
    run("make \"ob.d modulo :ob.d :world");
    run("make \"p.dx :ob.d - :half.world");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(100.0f, num(":p.dx"),
                                    "the delta did not wrap: an obstacle 100 steps away read as a world away");
}

// The cull distance and the wrap are tuned separately and would drift apart.
// If `far` ever reaches past half the world, one obstacle is inside the cull in
// both directions at once and gets drawn twice.
void test_the_far_plane_is_inside_the_wrap(void)
{
    TEST_ASSERT_TRUE_MESSAGE(num(":far") < num(":half.world"),
                             "far >= half.world: an obstacle would be drawn twice");
    TEST_ASSERT_EQUAL_FLOAT(num(":world") / 2.0f, num(":half.world"));
}

// The camera's own position is wrapped as it drives, or the deltas above are
// computed from a coordinate that has left the plain.
void test_driving_across_the_seam_keeps_the_camera_on_the_plain(void)
{
    const float world = num(":world");
    char expr[96];
    snprintf(expr, sizeof(expr), "make \"px 0  make \"pz %g  make \"ph 0", world - 6.0f);
    run(expr);
    press(KEY_UP);
    run("pollkeys  step.tank  step.tank  step.tank");
    release(KEY_UP);

    const float pz = num(":pz");
    const float travelled = 3.0f * 2.0f * num(":tread.step");
    TEST_ASSERT_TRUE_MESSAGE(pz >= 0.0f && pz < world, "the camera drove off the plain");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, travelled - 6.0f, pz);
}

//==========================================================================
// Drawing
//==========================================================================

void test_a_cube_draws_twelve_edges(void)
{
    camera_at(0, 0, 0);
    TEST_ASSERT_TRUE(project("project.box", 0, 300));
    mock_device_clear_graphics();
    run("draw.box");
    TEST_ASSERT_EQUAL_INT(EDGES_CUBE, mock_device_line_count());
}

void test_a_pyramid_draws_eight_edges(void)
{
    camera_at(0, 0, 0);
    TEST_ASSERT_TRUE(project("project.pyr", 0, 300));
    mock_device_clear_graphics();
    run("draw.pyr");
    TEST_ASSERT_EQUAL_INT(EDGES_PYR, mock_device_line_count());
}

void test_the_gunsight_is_a_fixed_overlay(void)
{
    mock_device_clear_graphics();
    run("gunsight");
    TEST_ASSERT_EQUAL_INT(EDGES_SIGHT, mock_device_line_count());

    // Twice from different camera states, because "fixed" is the claim: a sight
    // that moved with the heading would be a reticle painted on the world.
    int n = mock_device_line_count();
    float x1 = mock_device_get_line(0)->x1;
    camera_at(123, 456, 217);
    mock_device_clear_graphics();
    run("gunsight");
    TEST_ASSERT_EQUAL_INT(n, mock_device_line_count());
    TEST_ASSERT_EQUAL_FLOAT(x1, mock_device_get_line(0)->x1);
}

// A drawn obstacle is worth nothing if it is drawn in the wrong colour, and the
// cabinet's overlay is the reason the sight is not the world's green.
void test_the_sight_and_the_world_are_different_colours(void)
{
    TEST_ASSERT_TRUE(num(":sight.colour") != num(":world.colour"));
}

//==========================================================================
// The obstacle field
//==========================================================================

// Every obstacle is on the plain and none of them is on top of the start
// point -- the first frame should be a drive, not a bump.
void test_the_field_is_on_the_plain_and_clear_of_the_start(void)
{
    const float world = num(":world");
    const int n = (int)num(":ob.count");
    TEST_ASSERT_EQUAL_INT(n, (int)num("count :ox"));
    TEST_ASSERT_EQUAL_INT(n, (int)num("count :oz"));
    TEST_ASSERT_EQUAL_INT(n, (int)num("count :okind"));

    for (int i = 1; i <= n; i++)
    {
        const float x = item_of("ox", i), z = item_of("oz", i);
        TEST_ASSERT_TRUE_MESSAGE(x >= 0 && x < world, "an obstacle is off the plain in x");
        TEST_ASSERT_TRUE_MESSAGE(z >= 0 && z < world, "an obstacle is off the plain in z");
        const float dx = x - 800.0f, dz = z - 800.0f;
        // Well clear of both the collision radius and the near plane, so the
        // first frame is a drive and not a bump.
        TEST_ASSERT_TRUE_MESSAGE(sqrtf(dx * dx + dz * dz) > 200.0f,
                                 "an obstacle sits on the start point");
        const float kind = item_of("okind", i);
        TEST_ASSERT_TRUE_MESSAGE(kind == 1 || kind == 2, "an obstacle has no model");
    }
}

// The cap is a WORK budget: it counts objects that survived the far cull,
// whether or not the near cull then dropped them, because what it protects is
// the frame and not the picture.
void test_the_frame_draws_no_more_than_max_obstacles(void)
{
    // Put all eight in a tight cluster straight ahead, so nothing is culled.
    run("make \"ox [800 810 790 805]  make \"ox se :ox [795 815 785 820]");
    run("make \"oz [400 460 520 580]  make \"oz se :oz [640 700 340 280]");
    run("make \"okind [1 1 1 1]  make \"okind se :okind [1 1 1 1]");
    camera_at(800, 0, 0);

    mock_device_clear_graphics();
    run("draw.field");
    const int cap = (int)num(":max.obstacles");
    TEST_ASSERT_EQUAL_INT(cap * EDGES_CUBE, mock_device_line_count());
}

// The far cull is a DISTANCE test, so an obstacle behind you survives it and
// only the near cull -- inside `project.box` -- rejects it. If the object cap
// is spent before that rejection, obstacles behind the camera crowd out the one
// in front of it, and with eight on a 1,600-step plain and a 700-step cull that
// is not an edge case: about three survive the far cull on a typical frame and
// the cap is three, so it binds nearly every frame and roughly half of what it
// is spent on is behind you.
//
// What that looks like from the driving seat is a cube dead ahead that simply
// never appears. Found by decomposing the first play test's numbers rather than
// by seeing it.
void test_obstacles_behind_the_camera_do_not_crowd_out_the_one_in_front(void)
{
    // Three behind the camera, one ahead, the rest out of range. All four are
    // inside the far cull; only the near plane separates them.
    run("make \"ox [800 800 800 800]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [500 550 600 1100]  make \"oz se :oz [100 100 100 100]");
    run("make \"okind [1 1 1 1]  make \"okind se :okind [1 1 1 1]");
    camera_at(800, 800, 0);

    mock_device_clear_graphics();
    run("draw.field");
    TEST_ASSERT_EQUAL_INT_MESSAGE(EDGES_CUBE, mock_device_line_count(),
                                  "obstacles behind the camera spent the object cap");
}

void test_an_obstacle_beyond_the_far_plane_is_not_drawn(void)
{
    run("make \"ox [800 800 800 800]  make \"ox se :ox [800 800 800 800]");
    run("make \"oz [1500 1500 1500 1500]  make \"oz se :oz [1500 1500 1500 1500]");
    camera_at(800, 100, 0);
    mock_device_clear_graphics();
    run("draw.field");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(),
                                  "an obstacle past the far plane was drawn");
}

//==========================================================================
// The horizon
//==========================================================================

// Design section 8.4's arithmetic.  A 320-step viewport at 5.06 steps a degree
// holds 63 degrees, so a 40-point table at 9 degrees a point shows about seven
// peaks -- and the walk has to reach past both edges of the screen or a
// mountain range stops short of the frame.
void test_the_horizon_cull_walks_only_the_visible_points(void)
{
    TEST_ASSERT_EQUAL_FLOAT(40, num(":mn.n"));
    TEST_ASSERT_EQUAL_INT(40, (int)num("count :mtn"));
    TEST_ASSERT_EQUAL_FLOAT(SEGS_HORIZON, num(":mn.seen"));

    // Ten points out of forty, whatever the heading -- that is the lever M0
    // priced at 10.3-10.8 ms, and it is spent here rather than retrofitted.
    for (int h = 0; h < 360; h += 5)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "make \"ph %d", h);
        run(expr);
        mock_device_clear_graphics();
        run("horizon");
        TEST_ASSERT_EQUAL_INT_MESSAGE(SEGS_HORIZON, mock_device_line_count(),
                                      "the horizon walk changed length with the heading");
    }
}

// The other half of that: it must cover the screen at every heading.  An
// off-by-one at the field-of-view edge is a mountain that flicks in and out as
// you turn, and `int` truncating toward zero instead of flooring is exactly how
// you get one.
void test_the_horizon_covers_the_whole_view_at_every_heading(void)
{
    for (int h = 0; h < 360; h += 3)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "make \"ph %d", h);
        run(expr);
        mock_device_clear_graphics();
        run("horizon");

        float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < mock_device_line_count(); i++)
        {
            const MockLine *l = mock_device_get_line(i);
            if (l->x1 < lo) lo = l->x1;
            if (l->x2 < lo) lo = l->x2;
            if (l->x1 > hi) hi = l->x1;
            if (l->x2 > hi) hi = l->x2;
        }
        snprintf(expr, sizeof(expr), "the horizon left a gap at heading %d", h);
        TEST_ASSERT_TRUE_MESSAGE(lo <= -160.0f, expr);
        TEST_ASSERT_TRUE_MESSAGE(hi >= 160.0f, expr);
    }
}

// M0's harness scanned the table in index order and drew in screen order, and
// at heading 0 that steps from +171 to -171 in one segment and strokes a line
// clear across the view.  It looked exactly like a projection fault and nothing
// about the timing would have shown it.  Sweep the whole circle, because the
// seam is at one heading and a spot check would miss it.
void test_no_horizon_segment_spans_the_whole_screen(void)
{
    for (int h = 0; h < 360; h += 5)
    {
        char expr[96];
        snprintf(expr, sizeof(expr), "make \"ph %d", h);
        run(expr);
        mock_device_clear_graphics();
        run("horizon");
        for (int i = 0; i < mock_device_line_count(); i++)
        {
            const MockLine *l = mock_device_get_line(i);
            const float span = fabsf(l->x2 - l->x1);
            snprintf(expr, sizeof(expr),
                     "a %.0f-step horizon segment at heading %d: has `wrap` come back?", span, h);
            TEST_ASSERT_TRUE_MESSAGE(span < 120.0f, expr);
        }
    }
}

// The backdrop is at infinity: it scrolls with your heading and never with your
// position.  That is the only way to tell you are turning on an empty plain,
// and a horizon that tracked the camera would be a wall instead.
void test_the_horizon_ignores_the_camera_position(void)
{
    camera_at(0, 0, 40);
    mock_device_clear_graphics();
    run("horizon");
    const int n = mock_device_line_count();
    TEST_ASSERT_TRUE(n > 0);
    float first = mock_device_get_line(0)->x1;

    camera_at(1234, 77, 40);
    mock_device_clear_graphics();
    run("horizon");
    TEST_ASSERT_EQUAL_INT(n, mock_device_line_count());
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(first, mock_device_get_line(0)->x1,
                                    "the horizon moved with the camera position");
}

// The moon is the one backdrop item that is skipped when it is off the side,
// and both halves of that are worth pinning: it appears when it should and it
// costs nothing when it should not.
void test_the_moon_appears_only_when_it_is_in_view(void)
{
    run("make \"ph 300");   // straight at it
    mock_device_clear_graphics();
    run("moon");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > 0, "the moon is not drawn facing it");

    run("make \"ph 120");   // behind
    mock_device_clear_graphics();
    run("moon");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(),
                                  "the moon was drawn from behind the camera");
}

//==========================================================================
// The treads
//==========================================================================

// The pair drives forward speed (l + r) and turn rate (l - r), and the sign is
// the physical one: a tank whose right tread runs forward pivots LEFT, so a
// clockwise turn -- an increasing Logo heading -- needs l > r.  Easy to get
// backwards and impossible to miss once you drive it.
void test_the_arrows_drive_the_treads(void)
{
    run("make \"ph 0");

    press(KEY_UP);
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":left.tread"));
    TEST_ASSERT_EQUAL_FLOAT(1, num(":right.tread"));
    release(KEY_UP);

    press(KEY_RIGHT);
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":left.tread"), "a right pivot needs the left tread forward");
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":right.tread"));
    release(KEY_RIGHT);

    // Up and right together is a genuine one-tread arc, not a scripted curve.
    press(KEY_UP);
    press(KEY_RIGHT);
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":left.tread"), "the tread sum was not clamped");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":right.tread"));
    release(KEY_UP);
    release(KEY_RIGHT);

    press(KEY_DOWN);
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":left.tread"));
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":right.tread"));
    release(KEY_DOWN);
}

void test_a_right_pivot_increases_the_heading(void)
{
    run("make \"ph 0  make \"px 800  make \"pz 800");
    press(KEY_RIGHT);
    run("pollkeys  step.tank");
    release(KEY_RIGHT);
    TEST_ASSERT_EQUAL_FLOAT(2 * num(":turn.rate"), num(":ph"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(800, num(":px"), "a pivot moved the tank");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(800, num(":pz"), "a pivot moved the tank");
}

// Heading 0 looks down +z, so driving forward from a standing start moves in z
// and not in x.  A sign or an axis swap here puts the whole world sideways.
void test_driving_forward_moves_along_the_heading(void)
{
    run("make \"ph 0  make \"px 800  make \"pz 100");
    press(KEY_UP);
    run("pollkeys  step.tank");
    release(KEY_UP);
    TEST_ASSERT_EQUAL_FLOAT(800, num(":px"));
    TEST_ASSERT_EQUAL_FLOAT(100 + 2 * num(":tread.step"), num(":pz"));

    run("make \"ph 90  make \"px 800  make \"pz 100");
    press(KEY_UP);
    run("pollkeys  step.tank");
    release(KEY_UP);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 800 + 2 * num(":tread.step"), num(":px"));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100, num(":pz"));
}

//==========================================================================
// Collision
//==========================================================================

// Design section 9: the collision radius follows the near plane rather than the
// other way round, so the two can never disagree.  The near cull drops an
// object when any column comes inside `near`, and the nearest column of an
// axis-aligned cube is at most half*sqrt2 in front of its centre.
void test_the_collision_radius_covers_the_near_plane(void)
{
    const float near = num(":near");
    const float half = num(":half");
    TEST_ASSERT_TRUE_MESSAGE(num(":coll.r") >= near + half * 1.4143f,
                             "an obstacle can vanish before you bump it");
}

// The consequence, driven rather than argued: run at a cube head-on for long
// enough to reach it and it must still be on the screen when you stop.
void test_you_cannot_drive_close_enough_for_an_obstacle_to_vanish(void)
{
    // One cube ahead and the rest pushed out of range, so the count below is
    // about the one you drove into rather than about the object cap.
    run("make \"ox [800 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [700 1500 1500 1500]  make \"oz se :oz [1500 1500 1500 1500]");
    run("make \"okind [1 1 1 1]  make \"okind se :okind [1 1 1 1]");
    run("make \"px 800  make \"pz 100  make \"ph 0");

    press(KEY_UP);
    for (int i = 0; i < 120; i++)
        run("pollkeys  step.tank");
    release(KEY_UP);

    TEST_ASSERT_TRUE_MESSAGE(truth(":bumped"), "the tank never reached the obstacle");
    mock_device_clear_graphics();
    run("draw.field");
    TEST_ASSERT_EQUAL_INT_MESSAGE(EDGES_CUBE, mock_device_line_count(),
                                  "the obstacle you are pressed against vanished");
}

// Turning away from what you have run into is always allowed -- the arcade's
// behaviour, and the reason `step.tank` refuses the move rather than the frame.
void test_a_blocked_tank_can_still_turn(void)
{
    run("make \"ox [800 800 800 800]  make \"ox se :ox [800 800 800 800]");
    run("make \"oz [200 200 200 200]  make \"oz se :oz [200 200 200 200]");
    run("make \"px 800  make \"pz 150  make \"ph 0");

    press(KEY_UP);
    press(KEY_RIGHT);
    run("pollkeys  step.tank");
    release(KEY_UP);
    release(KEY_RIGHT);

    TEST_ASSERT_TRUE_MESSAGE(truth(":bumped"), "the tank was not blocked");
    TEST_ASSERT_TRUE_MESSAGE(num(":ph") > 0, "a blocked tank could not turn");
}

//==========================================================================
// The frame
//==========================================================================

// The whole frame, from the state the game starts in, drawing what it draws.
// Two objects are in range at the start point, so the count is the horizon plus
// the gunsight plus those -- and if the field is ever re-laid this is the test
// that says the start view changed.
void test_a_frame_runs_and_draws_the_scene(void)
{
    run("make \"px 800  make \"pz 800  make \"ph 0  make \"paused false");
    run("make \"quit false  make \"frame.count 0");
    run("pollkeys");
    mock_device_clear_graphics();
    run("play.frame");

    const int n = mock_device_line_count();
    TEST_ASSERT_TRUE_MESSAGE(n > SEGS_HORIZON + EDGES_SIGHT,
                             "the start view holds no obstacles at all");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":frame.count"));
}

// A frame allocates -- `.setitem` of a number interns it -- so the contract is
// a flat working set rather than a zero.  Run a level's worth and require the
// workspace to still be there: `reclaim` asks how much room is left rather than
// counting frames, which is what Asteroids had to learn three times.
void test_a_long_run_of_frames_reclaims(void)
{
    run("make \"px 800  make \"pz 800  make \"ph 0  make \"paused false");
    run("make \"quit false  make \"frame.count 0");
    run("pollkeys");

    press(KEY_UP);
    for (int i = 0; i < 600; i++)
        run("play.frame");
    release(KEY_UP);

    TEST_ASSERT_EQUAL_FLOAT(600, num(":frame.count"));
    TEST_ASSERT_TRUE_MESSAGE(num("atoms") > 0, "the workspace ran out over a long run");
}

// `poll.input` runs OUTSIDE the paused guard, or a paused game could never read
// the key that unpauses it -- so it has to turn every other key away itself
// while paused, which is the defect both shipped shooters had.
void test_a_paused_frame_neither_drives_nor_draws(void)
{
    run("make \"px 800  make \"pz 800  make \"ph 0  make \"paused false  make \"quit false");
    run("pollkeys");

    press(KEY_PAUSE);
    run("play.frame");
    release(KEY_PAUSE);
    TEST_ASSERT_TRUE(truth(":paused"));

    press(KEY_UP);
    press(KEY_RIGHT);
    run("pollkeys");
    mock_device_clear_graphics();
    run("play.frame");
    release(KEY_UP);
    release(KEY_RIGHT);

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(800, num(":pz"), "a paused tank drove");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":ph"), "a paused tank turned");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(), "a paused frame redrew the scene");

    // And the key that unpauses it still gets through.
    press(KEY_PAUSE);
    run("play.frame");
    release(KEY_PAUSE);
    TEST_ASSERT_FALSE_MESSAGE(truth(":paused"), "a paused game could not be unpaused");
}

void test_quit_ends_the_loop(void)
{
    run("make \"paused false  make \"quit false");
    run("pollkeys");
    press(KEY_QUIT);
    run("play.frame");
    release(KEY_QUIT);
    TEST_ASSERT_TRUE(truth(":quit"));
}

//==========================================================================
// The readout
//==========================================================================

// The defect this exists for: `sync` PRESENTS and then waits, so a figure
// measured up to `sync` leaves out the present -- 19.8 ms of a 66.7 ms budget
// on every board M0 measured. The first version of this file reported only
// that figure and called it the frame, and a board read "low to mid 20s"
// against a predicted 46.5 with nothing wrong except what was being counted.
//
// Nothing on the host can time either one: `ticks` is milliseconds and a host
// frame is microseconds. What can be checked is the property that broke --
// which timer brackets `sync` -- so this reads it back out of the Logo source,
// the same way the prefix test below reads the names. `body.ms` must be taken
// before the `sync` and `frame.ms` after it.
void test_the_frame_timer_brackets_the_present(void)
{
    FILE *f = fopen(BATTLEZONE_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL(f);

    char line[512];
    bool in_play_frame = false;
    int body_at = -1, sync_at = -1, frame_at = -1, n = 0;

    while (fgets(line, sizeof(line), f))
    {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';')
            continue;
        if (strncmp(p, "to play.frame", 13) == 0) { in_play_frame = true; n = 0; continue; }
        if (!in_play_frame)
            continue;
        if (repl_line_is_end(p))
            break;
        n++;
        if (strncmp(p, "make \"body.ms", 13) == 0) body_at = n;
        if (strncmp(p, "make \"frame.ms", 14) == 0) frame_at = n;
        if (strncmp(p, "sync", 4) == 0) sync_at = n;
    }
    fclose(f);

    TEST_ASSERT_TRUE_MESSAGE(body_at > 0, "play.frame does not set body.ms");
    TEST_ASSERT_TRUE_MESSAGE(sync_at > 0, "play.frame does not call sync");
    TEST_ASSERT_TRUE_MESSAGE(frame_at > 0, "play.frame does not set frame.ms");
    TEST_ASSERT_TRUE_MESSAGE(body_at < sync_at,
                             "body.ms is taken after sync, so it includes the present");
    TEST_ASSERT_TRUE_MESSAGE(frame_at > sync_at,
                             "frame.ms is taken before sync, so it leaves out the present");
}

// A figure that changes fifteen times a second cannot be read off a screen by
// somebody driving, so the readout is averaged over `hud.every` frames. Check
// that it fires on that period and resets, rather than every frame or never.
void test_the_readout_is_averaged_over_a_second(void)
{
    run("make \"px 800  make \"pz 800  make \"ph 0  make \"paused false");
    run("make \"quit false  make \"frame.count 0  pollkeys");
    run("make \"hud.n 0  make \"hud.bs 0  make \"hud.fs 0");
    run("make \"hud.bm 0  make \"hud.fm 0");

    const int every = (int)num(":hud.every");
    TEST_ASSERT_TRUE_MESSAGE(every > 1, "the readout is not averaged at all");

    // The first frame has no previous frame to tally, so the count starts on
    // the second and the readout lands on frame `every` + 1.
    for (int i = 0; i < every; i++)
        run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(every - 1, num(":hud.n"),
                                    "the tally did not count every frame");

    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":hud.n"),
                                    "the readout did not fire and reset on its period");

    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":hud.n"));
}

// The peak is the half that matters for the budget -- it is where a `recycle`
// spike shows -- so it has to survive the averaging rather than be an average
// of its own.
void test_the_readout_keeps_a_peak_not_an_average_of_peaks(void)
{
    run("make \"frame.count 9  make \"hud.n 0  make \"hud.bm 0");
    run("make \"body.ms 5  hud.tally");
    run("make \"body.ms 40  hud.tally");
    run("make \"body.ms 5  hud.tally");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(40, num(":hud.bm"), "the peak was averaged away");
}

//==========================================================================
// The naming discipline the globals lever costs
//==========================================================================

// Design section 13's L0.5 is worth 3.9 ms a frame and its price is that every
// hot-path temporary is a global in a language with one flat namespace and no
// shadowing.  M0 lost its ENTIRE body column to it: an accumulator called `b`
// and a camera constant called `b` are one variable, so the callee wrote the
// caller's value once a frame and every split figure came back 0.00 ms.
//
// `local` protects a name from the world, not the world from a name.  So the
// mitigation is a prefix, and this reads the names back out of the source: any
// `make "x` inside a procedure body must be either a prefixed temporary or one
// of the named pieces of game state.  A rename to `b` fails here rather than
// silently returning zeros on a board.
void test_every_hot_path_temporary_is_prefixed(void)
{
    static const char *const prefixes[] = {"p.", "ob.", "mt.", "tk.", "mn.", "hud.", NULL};
    static const char *const state[] = {
        "px", "pz", "ph", "cs", "sn", "a", "b", "apx", "apy",
        "left.tread", "right.tread", "bumped", "paused", "quit",
        "frame.count", "frame.ms", "body.ms", "cpu.at",
        "max.obstacles", "ox", "oz", "okind", NULL};

    FILE *f = fopen(BATTLEZONE_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL(f);

    char line[512];
    bool in_def = false;
    while (fgets(line, sizeof(line), f))
    {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';')
            continue;
        if (!in_def && repl_line_starts_with_to(p)) { in_def = true; continue; }
        if (in_def && repl_line_is_end(p)) { in_def = false; continue; }
        if (!in_def)
            continue;

        // Every `make "name` in a procedure body, however deep in the line.
        for (char *m = strstr(p, "make \""); m; m = strstr(m + 1, "make \""))
        {
            char name[64];
            size_t n = 0;
            for (char *q = m + 6; *q && (isalnum((unsigned char)*q) || *q == '.' || *q == '_') && n + 1 < sizeof(name); q++)
                name[n++] = *q;
            name[n] = '\0';
            if (n == 0)
                continue;

            bool ok = false;
            for (int i = 0; prefixes[i] && !ok; i++)
                ok = strncmp(name, prefixes[i], strlen(prefixes[i])) == 0;
            for (int i = 0; state[i] && !ok; i++)
                ok = strcmp(name, state[i]) == 0;

            char msg[160];
            snprintf(msg, sizeof(msg),
                     "`make \"%s` is neither prefixed nor named game state -- see M0's `b`", name);
            TEST_ASSERT_TRUE_MESSAGE(ok, msg);
        }
    }
    fclose(f);
    TEST_ASSERT_FALSE(in_def);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_file_loads_and_sets_its_tuning);
    RUN_TEST(test_max_obstacles_is_a_constant_the_frame_reads);

    RUN_TEST(test_the_projection_is_right_at_a_heading_that_is_not_zero);
    RUN_TEST(test_turning_right_sweeps_the_world_to_the_left);
    RUN_TEST(test_an_object_behind_the_camera_is_culled);
    RUN_TEST(test_culling_is_conservative_at_the_near_plane);
    RUN_TEST(test_the_two_projections_agree_on_their_columns);
    RUN_TEST(test_the_near_plane_bounds_a_cube_to_about_one_screen);

    RUN_TEST(test_the_plain_wraps_in_the_arithmetic);
    RUN_TEST(test_the_far_plane_is_inside_the_wrap);
    RUN_TEST(test_driving_across_the_seam_keeps_the_camera_on_the_plain);

    RUN_TEST(test_a_cube_draws_twelve_edges);
    RUN_TEST(test_a_pyramid_draws_eight_edges);
    RUN_TEST(test_the_gunsight_is_a_fixed_overlay);
    RUN_TEST(test_the_sight_and_the_world_are_different_colours);

    RUN_TEST(test_the_field_is_on_the_plain_and_clear_of_the_start);
    RUN_TEST(test_the_frame_draws_no_more_than_max_obstacles);
    RUN_TEST(test_obstacles_behind_the_camera_do_not_crowd_out_the_one_in_front);
    RUN_TEST(test_an_obstacle_beyond_the_far_plane_is_not_drawn);

    RUN_TEST(test_the_horizon_cull_walks_only_the_visible_points);
    RUN_TEST(test_the_horizon_covers_the_whole_view_at_every_heading);
    RUN_TEST(test_no_horizon_segment_spans_the_whole_screen);
    RUN_TEST(test_the_horizon_ignores_the_camera_position);
    RUN_TEST(test_the_moon_appears_only_when_it_is_in_view);

    RUN_TEST(test_the_arrows_drive_the_treads);
    RUN_TEST(test_a_right_pivot_increases_the_heading);
    RUN_TEST(test_driving_forward_moves_along_the_heading);

    RUN_TEST(test_the_collision_radius_covers_the_near_plane);
    RUN_TEST(test_you_cannot_drive_close_enough_for_an_obstacle_to_vanish);
    RUN_TEST(test_a_blocked_tank_can_still_turn);

    RUN_TEST(test_a_frame_runs_and_draws_the_scene);
    RUN_TEST(test_a_long_run_of_frames_reclaims);
    RUN_TEST(test_a_paused_frame_neither_drives_nor_draws);
    RUN_TEST(test_quit_ends_the_loop);

    RUN_TEST(test_the_frame_timer_brackets_the_present);
    RUN_TEST(test_the_readout_is_averaged_over_a_second);
    RUN_TEST(test_the_readout_keeps_a_peak_not_an_average_of_peaks);

    RUN_TEST(test_every_hot_path_temporary_is_prefixed);

    return UNITY_END();
}
