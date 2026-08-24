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
#include "core/limits.h"
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

// The gunsight is a fixed overlay with no arithmetic in it at all.  Eight edges
// since M2 moved it off the horizon: a bracket above the aiming point and one
// below it.  It was six.
#define EDGES_SIGHT 8

// The horizon walks `mtn.seen` + 1 points, so it strokes `mtn.seen` segments.
#define SEGS_HORIZON 9

// The enemy is the hull's twelve edges and the gun's one.  Design section 8.2
// lists a turret box as well; it is not in the shape M0 priced and it is not
// here, so thirteen is the number that has a measurement behind it.
#define EDGES_ENEMY 13

// The explosion is five short strokes on a growing radius.
#define FRAGS_BOOM 5

// PicoCalc key codes, as the game names them to `keydown?`/`keyhit?`.  One key
// per tread per direction, laid out like the cabinet's two sticks: 1 and Q on
// the left of the keyboard drive the left tread, 0 and P on the right drive the
// right one.
#define KEY_LFWD    49 // '1'
#define KEY_LBACK  113 // 'q'
#define KEY_RFWD    48 // '0'
#define KEY_RBACK  112 // 'p'
#define KEY_FIRE    93 // ']'
#define KEY_PAUSE   32 // space
#define KEY_QUIT   177 // escape

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

// Driving straight and pivoting right are two keys each.  Most tests below want
// the tank moving rather than the keys that move it, and say so through these.
static void press_forward(void)
{
    press(KEY_LFWD);
    press(KEY_RFWD);
}

static void release_forward(void)
{
    release(KEY_LFWD);
    release(KEY_RFWD);
}

static void press_pivot_right(void)
{
    press(KEY_LFWD);
    press(KEY_RBACK);
}

static void release_pivot_right(void)
{
    release(KEY_LFWD);
    release(KEY_RBACK);
}

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
    // M2 hoists the wrapped obstacle field into (obx, obz) once a frame, and
    // the field scan and every collision read it rather than wrapping again.
    // `step.tank` rescans after the move commits; a test that places the camera
    // by hand has to do the same or it reads the previous placement's table.
    run("ob.scan");
}

// `init.game` ends by arming `setrefresh "sync`, and on the host `sync` waits
// the real frame period -- a 60-frame loop would take four seconds of wall
// clock and the suite would crawl.  Every test that sets a game up and then
// drives frames goes through this instead of calling `init.game` directly.
static void new_game(void)
{
    run("init.game");
    run("setrefresh \"auto");
}

// Put the enemy somewhere and point it somewhere, then hoist the two offsets
// every reader of it wants: the world-axis pair the hunt and the collisions
// use, and the camera-frame pair the projection and the radar use.  `step.enemy`
// takes them at two different moments -- the world pair before its move and the
// camera pair after it -- which is why they are two procedures.
static void enemy_at(float ex, float ez, float eh)
{
    char expr[200];
    snprintf(expr, sizeof(expr),
             "make \"e.x %g  make \"e.z %g  make \"e.h %g "
             "make \"e.ec cos %g  make \"e.es sin %g "
             "make \"e.alive true  enemy.offsets  enemy.camera",
             ex, ez, eh, eh, eh);
    run(expr);
}

// Put one kind of enemy in front of the camera, the way a spawn would: the
// row read into the live `e.*` names first, then the placement.
static void foe_at(int kind, float ex, float ez, float eh)
{
    char expr[64];
    snprintf(expr, sizeof(expr), "make \"e.kind %d  set.kind", kind);
    run(expr);
    enemy_at(ex, ez, eh);
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
    // Through `camera_at`, because M2's `blocked?` reads a table that belongs
    // to the camera it was hoisted against: teleport the camera with a bare
    // `make` and the tank's first move is decided against the last placement's
    // obstacles.  The game only ever teleports in `battlezone`, which rescans.
    camera_at(0.0f, world - 6.0f, 0.0f);
    press_forward();
    run("pollkeys  step.tank  step.tank  step.tank");
    release_forward();

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

// The defect this exists for, found by looking at the screen: the sight used to
// be two long arms at y = 40 with the verticals hanging off them, and 40 is
// `hz` -- so the arms lay exactly along the ground line section 8.3a added.
// Two things drawn on the same row of pixels in two colours are one thing as
// far as the eye is concerned, and neither the sight nor the horizon could be
// read for what it was.
//
// THE INVARIANT IS NOT "NO HORIZONTALS".  That was one way to fix it and it is
// not the requirement: a horizontal bar at y = 65 is perfectly legible against
// a horizon at y = 40.  What has to hold is that no stroke lies ON `hz`, and
// that the aiming point stays in clear air -- so this checks those two things
// and leaves the shape free to change.
void test_no_part_of_the_gunsight_lies_along_the_horizon(void)
{
    const float hz = num(":hz");
    mock_device_clear_graphics();
    run("gunsight");
    TEST_ASSERT_EQUAL_INT(EDGES_SIGHT, mock_device_line_count());

    for (int i = 0; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        const float lo = l->y1 < l->y2 ? l->y1 : l->y2;
        const float hi = l->y1 < l->y2 ? l->y2 : l->y1;

        // Not lying along the horizon, and not crossing it either: a stroke
        // through `hz` is a stroke through the target and through the shell.
        TEST_ASSERT_FALSE_MESSAGE(lo <= hz && hz <= hi,
                                  "a gunsight segment sits on the horizon");
    }
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
// The plain is `eye` below the camera and screen y is `hz - eye/z * k`, so as z
// goes to infinity the ground rises to exactly `hz` and stops.  One flat line
// across the view at that y is the true horizon -- the line where the plain
// meets the sky -- and it is the whole of what this game draws below the peaks.
//
// It exists because a wireframe silhouette is a line and not a filled shape, so
// nothing anchors the mountain range's lower edge and the range reads as a
// squiggle hanging in space.  Found by playing it, which is what M1's hardware
// pass was for.
//
// It is a separate procedure from `horizon` for a reason the test below makes
// plain: this segment spans the whole screen deliberately, and a *horizon*
// segment that spans the whole screen is the signature of the `wrap` defect.
// Keeping them apart keeps `test_no_horizon_segment_spans_the_whole_screen`
// watching what it was written to watch instead of carrying an exception.
void test_the_ground_is_one_flat_line_at_the_horizon(void)
{
    const float hz = num(":hz");

    mock_device_clear_graphics();
    run("ground");
    TEST_ASSERT_EQUAL_INT(1, mock_device_line_count());

    const MockLine *l = mock_device_get_line(0);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(hz, l->y1, "the ground is not at the horizon");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(hz, l->y2, "the ground is not level");
    TEST_ASSERT_TRUE_MESSAGE(fabsf(l->x2 - l->x1) >= 320.0f,
                             "the ground does not cross the whole view");

    // Every mountain point sits above it, or the range is drawn through the
    // ground rather than standing on it.
    for (int i = 1; i <= (int)num(":mn.n"); i++)
        TEST_ASSERT_TRUE_MESSAGE(item_of("mtn", i) > 0.0f, "a mountain point is below the ground");

    // And it is at infinity like the rest of the backdrop: it does not move
    // with the heading or with the position.
    camera_at(1234, 77, 217);
    mock_device_clear_graphics();
    run("ground");
    const MockLine *m = mock_device_get_line(0);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(l->x1, m->x1, "the ground moved with the camera");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(hz, m->y1, "the ground moved with the camera");
}

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
// The horizon is drawn as one polyline: `pu` to v0, then a pen-down `setpos` to
// each of the rest.  So line j runs v[j] -> v[j+1], and this reads a vertex back
// out of the mock by that.
static float horizon_vertex(int j)
{
    const int n = mock_device_line_count();
    TEST_ASSERT_TRUE_MESSAGE(j >= 0 && j <= n, "no such horizon vertex");
    return j < n ? mock_device_get_line(j)->x1 : mock_device_get_line(n - 1)->x2;
}

// THE INVARIANT THE MOUNTAINS USED TO BREAK, and it is the one that makes a 3D
// scene cohere: IN A PURE PIVOT, DISTANCE DOES NOT MATTER.  A rotation changes
// every bearing by the same amount, so a mountain at infinity and a cube in
// front of you, at the same screen position, must move by the same number of
// pixels.  Distance cancels out of `k * xc/zc` entirely.
//
// The horizon used to map azimuth to screen x LINEARLY -- `(azimuth - ph) *
// 5.06` -- where everything else is `k * tan`.  Linear against a tangent means
// the mountains scrolled at a CONSTANT rate while the world accelerated toward
// the edges: +11.4 % at the centre of the view, 0 at about 20 degrees off it,
// and -13.9 % near the edge.  Over a second of turning a cube and a peak that
// started together ended 32 steps apart, and the drift reversed sign across the
// screen.  Reported from watching it turn, which is the only way it shows.
//
// Design section 8.4 chose the linear form deliberately, to save a tangent a
// point.  This is what that cost.
void test_a_pivot_moves_the_horizon_and_the_world_together(void)
{
    const float k = num(":k"), step = num(":mn.step"), arc = num(":mn.arc");

    // At heading 0 the walk starts at bearing -mn.arc, so vertex 4 is the table
    // point sitting dead ahead.  One frame of turn later it is the same point,
    // because the first index has not moved on.
    camera_at(800, 800, 0);
    mock_device_clear_graphics();
    run("horizon");
    const float peak_before = horizon_vertex(4);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.0f, peak_before,
                                     "the walk does not start at -mn.arc");

    // An object at the same bearing: the enemy's gun root is its centre, which
    // is a plain projected point.
    enemy_at(800, 1100, 180);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    const float obj_before = item_of("gunx", 1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, obj_before);

    const float turn = num(":turn.rate");
    camera_at(800, 800, turn);
    mock_device_clear_graphics();
    run("horizon");
    const float peak_after = horizon_vertex(4);
    enemy_at(800, 1100, 180);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    const float obj_after = item_of("gunx", 1);

    const float peak_moved = peak_after - peak_before;
    const float obj_moved = obj_after - obj_before;

    // Both are k*tan of the same bearing, so they are the same number.
    const float expect = -k * tanf(turn * (float)M_PI / 180.0f);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.05f, expect, obj_moved, "the object projection moved");
    char msg[128];
    snprintf(msg, sizeof(msg),
             "a pivot moved the horizon %.2f steps and the world %.2f", peak_moved, obj_moved);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.05f, obj_moved, peak_moved, msg);

    // And it holds out at the edge of the view, where the old linear map erred
    // the other way.  Vertex j sits at bearing j*step - arc, so vertex 7 is 27
    // degrees off centre -- the last one still inside the 31.6-degree view.
    const float edge_bearing = 7.0f * step - arc;
    camera_at(800, 800, 0);
    mock_device_clear_graphics();
    run("horizon");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.05f, k * tanf(edge_bearing * (float)M_PI / 180.0f),
                                     horizon_vertex(7),
                                     "the horizon is not k*tan at the view's edge");
}

// The map itself, at every vertex of a walk: screen x is `k * tan(bearing)`,
// which is what an object at that bearing would get.  Checked at a heading that
// is not a multiple of the table's step, so the vertices land at bearings the
// table does not name.
void test_the_horizon_maps_bearing_through_the_same_tangent_as_the_world(void)
{
    const float k = num(":k"), step = num(":mn.step"), arc = num(":mn.arc");

    for (float h = 0.0f; h < 360.0f; h += 17.0f)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "make \"ph %g  cam.setup", h);
        run(expr);
        mock_device_clear_graphics();
        run("horizon");

        // The first vertex sits in [-(arc + step), -arc]; each one after it is
        // `step` degrees further round.
        const float first = -arc - fmodf(h + arc, step);
        for (int j = 0; j <= (int)num(":mn.seen"); j++)
        {
            const float bearing = first + (float)j * step;
            const float want = k * tanf(bearing * (float)M_PI / 180.0f);
            snprintf(expr, sizeof(expr), "vertex %d at heading %g", j, h);
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, want, horizon_vertex(j), expr);
        }
    }
}

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

// Each key drives ONE tread in ONE direction, and no key does anything else:
// there is no intent layer to get wrong, so what this pins down is the wiring.
void test_each_key_drives_its_own_tread(void)
{
    run("make \"ph 0");

    press(KEY_LFWD);
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":left.tread"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":right.tread"), "the left key moved the right tread");
    release(KEY_LFWD);

    press(KEY_LBACK);
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":left.tread"));
    TEST_ASSERT_EQUAL_FLOAT(0, num(":right.tread"));
    release(KEY_LBACK);

    press(KEY_RFWD);
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":left.tread"), "the right key moved the left tread");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":right.tread"));
    release(KEY_RFWD);

    press(KEY_RBACK);
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":left.tread"));
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":right.tread"));
    release(KEY_RBACK);

    press_forward();
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":left.tread"));
    TEST_ASSERT_EQUAL_FLOAT(1, num(":right.tread"));
    release_forward();

    press(KEY_LBACK);
    press(KEY_RBACK);
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":left.tread"));
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":right.tread"));
    release(KEY_LBACK);
    release(KEY_RBACK);

    // Both keys of one tread at once is the back one, the way down beat up
    // before: the second `if` simply runs last.
    press(KEY_LFWD);
    press(KEY_LBACK);
    run("pollkeys  treads");
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":left.tread"));
    release(KEY_LFWD);
    release(KEY_LBACK);
}

// The two motions the controls are FOR, stated the way a player would: one key
// arcs, two keys pivot.  The pair drives forward speed (l + r) and turn rate
// (l - r), and the sign is the physical one -- a tank whose right tread runs
// forward pivots LEFT, so a clockwise turn needs l > r.  Easy to get backwards
// and impossible to miss once you drive it.
void test_one_key_arcs_and_two_keys_pivot(void)
{
    // `1` alone: the left tread forward and the right one stopped, so the tank
    // goes forward AND clockwise -- an arc to the right, not a scripted curve.
    camera_at(800, 800, 0);
    press(KEY_LFWD);
    run("pollkeys  step.tank");
    release(KEY_LFWD);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":turn.rate"), num(":ph"), "one tread forward did not arc right");
    TEST_ASSERT_TRUE_MESSAGE(num(":px") > 800.0f || num(":pz") > 800.0f, "an arc stood still");

    // `1` and `p` together: the treads oppose, the sum is zero and the tank
    // spins on the spot at twice the rate.
    camera_at(800, 800, 0);
    press_pivot_right();
    run("pollkeys  step.tank");
    release_pivot_right();
    TEST_ASSERT_EQUAL_FLOAT(2 * num(":turn.rate"), num(":ph"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(800, num(":px"), "a pivot moved the tank");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(800, num(":pz"), "a pivot moved the tank");
}

// Heading 0 looks down +z, so driving forward from a standing start moves in z
// and not in x.  A sign or an axis swap here puts the whole world sideways.
void test_driving_forward_moves_along_the_heading(void)
{
    camera_at(800, 100, 0);
    press_forward();
    run("pollkeys  step.tank");
    release_forward();
    TEST_ASSERT_EQUAL_FLOAT(800, num(":px"));
    TEST_ASSERT_EQUAL_FLOAT(100 + 2 * num(":tread.step"), num(":pz"));

    camera_at(800, 100, 90);
    press_forward();
    run("pollkeys  step.tank");
    release_forward();
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

    press_forward();
    for (int i = 0; i < 120; i++)
        run("pollkeys  step.tank");
    release_forward();

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

    // The arc, not the pivot: a pivot's sum is zero, so it never asks to move
    // and there would be nothing for `blocked?` to refuse.
    press(KEY_LFWD);
    run("pollkeys  step.tank");
    release(KEY_LFWD);

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
    TEST_ASSERT_TRUE_MESSAGE(n > SEGS_HORIZON + EDGES_SIGHT + 1,
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

    press_forward();
    for (int i = 0; i < 600; i++)
        run("play.frame");
    release_forward();

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

    press(KEY_LFWD);
    run("pollkeys");
    mock_device_clear_graphics();
    run("play.frame");
    release(KEY_LFWD);

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
//==========================================================================
// M2 -- the enemy
//==========================================================================

void test_the_enemy_draws_thirteen_edges(void)
{
    camera_at(800, 800, 0);
    enemy_at(800, 1100, 90);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    mock_device_clear_graphics();
    run("draw.enemy");
    TEST_ASSERT_EQUAL_INT(EDGES_ENEMY, mock_device_line_count());
}

// THE TEST THIS MILESTONE MOST NEEDED, and the defect it was written from is
// in M0's own harness: it built the enemy's half-offset from (cos eh, sin eh)
// instead of (sin eh, cos eh), so its gun pointed at 90 - eh.  With a fixed
// heading and nothing aiming down the barrel that is invisible -- which is
// exactly why it survived a measurement run and would have been copied here.
//
// It is not invisible in a game: `hunt` aims the gun and `enemy.fires` shoots
// along it, so a tank whose barrel is 90 degrees off its line of fire shoots
// sideways at you while facing you.
//
// The camera looks down +z from the origin of the offsets and the enemy is
// dead ahead facing +x, so its barrel runs left-to-right across the view: the
// tip is 2*ehalf to the RIGHT of the hull centre.  Under the transposed
// spelling the barrel would point away down +z and project to a point.
void test_the_gun_points_where_the_enemy_faces(void)
{
    const float k = num(":k"), ehalf = num(":ehalf");

    camera_at(800, 800, 0);
    enemy_at(800, 1100, 90);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    const float root = item_of("gunx", 1), tip = item_of("gunx", 2);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, root);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.0f * ehalf * k / 300.0f, tip);

    // And the other way round when it faces the other way.
    enemy_at(800, 1100, 270);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -2.0f * ehalf * k / 300.0f, item_of("gunx", 2));

    // Facing straight away, the barrel foreshortens to nothing rather than
    // swinging sideways -- which is the same claim from the third direction.
    enemy_at(800, 1100, 0);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, item_of("gunx", 2));
}

// The hull is a square that turns with the enemy, which is the other half of
// section 8.2's trick: the camera-frame right offset is the forward one turned
// 90 degrees, and turning 90 degrees commutes with the camera's rotation.  Get
// that wrong and the hull stops being square as it turns.
void test_the_enemy_hull_is_a_square_that_turns(void)
{
    const float k = num(":k"), ehalf = num(":ehalf");
    camera_at(800, 800, 0);

    // Axis-aligned: the widest column is ehalf from the centre.
    enemy_at(800, 1100, 0);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    float widest = 0.0f;
    for (int i = 1; i <= 4; i++)
    {
        const float x = fabsf(item_of("cx", i));
        if (x > widest) widest = x;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.6f, ehalf * k / 300.0f, widest);

    // On the diagonal it is a corner that is widest, at ehalf*sqrt(2).
    enemy_at(800, 1100, 45);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    widest = 0.0f;
    for (int i = 1; i <= 4; i++)
    {
        const float x = fabsf(item_of("cx", i));
        if (x > widest) widest = x;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.6f, ehalf * sqrtf(2.0f) * k / 300.0f, widest);
}

// Conservative in the same way the obstacles are, and for the same reason: one
// corner behind you swings the projection through infinity.
void test_the_enemy_is_culled_at_the_near_plane(void)
{
    const float near = num(":near");
    camera_at(800, 800, 0);

    enemy_at(800, 800 + near + 40.0f, 0);
    TEST_ASSERT_TRUE_MESSAGE(truth("project.enemy"), "an enemy in clear view was culled");

    enemy_at(800, 800 + near - 1.0f, 0);
    TEST_ASSERT_FALSE_MESSAGE(truth("project.enemy"), "an enemy inside the near plane was drawn");

    enemy_at(800, 700, 0);
    TEST_ASSERT_FALSE_MESSAGE(truth("project.enemy"), "an enemy behind the camera was drawn");
}

// The hunt turns towards the player and stops turning when it is looking at
// them, which is the whole of its aim.
void test_the_enemy_turns_towards_the_player(void)
{
    camera_at(800, 800, 0);

    // Ahead and to the player's right, facing back down -z at the player: it
    // has to turn clockwise to look at them.
    enemy_at(900, 1100, 180);
    run("make \"e.cool 99  hunt");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.t"), "the enemy turned away from the player");

    enemy_at(700, 1100, 180);
    run("make \"e.cool 99  hunt");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-1, num(":e.t"), "the enemy turned away from the player");

    enemy_at(800, 1100, 180);
    run("make \"e.cool 99  hunt");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":e.t"), "the enemy kept turning past the player");
}

// Design section 19.3: at 15 fps a decision every third frame is invisible and
// costs a third as much.  The two frames in between act on the intents the
// third left behind, so this checks that the decision is on a period and that
// the acting is not.
void test_the_enemy_thinks_on_one_frame_in_three(void)
{
    camera_at(800, 800, 0);
    enemy_at(900, 1100, 180);
    const int think = (int)num(":e.think");
    TEST_ASSERT_TRUE_MESSAGE(think > 1, "the enemy thinks every frame");

    // A frame that is not a thinking frame leaves the intent alone.
    run("make \"e.t 0  make \"e.f 0  make \"e.fire false");
    run("make \"frame.count 1  step.enemy");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":e.t"), "a non-thinking frame decided");

    // The thinking frame finds the player.
    char expr[64];
    snprintf(expr, sizeof(expr), "make \"frame.count %d  step.enemy", think);
    run(expr);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.t"), "the thinking frame did not decide");

    // And the frame after it acts on what the thinking frame left behind.
    const float before = num(":e.h");
    run("make \"frame.count 1  step.enemy");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(before + num(":e.turn"), num(":e.h"),
                                    "a non-thinking frame did not act on the intent");
}

// It closes to `e.range` and then holds, or it drives into your face and the
// near cull makes it vanish.
void test_the_enemy_closes_and_then_holds_its_range(void)
{
    camera_at(800, 800, 0);
    const float range = num(":e.range");

    enemy_at(800, 800 + range + 200.0f, 180);
    run("make \"e.cool 99  hunt");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.f"), "the enemy would not close");

    enemy_at(800, 800 + range - 60.0f, 180);
    run("make \"e.cool 99  hunt");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":e.f"), "the enemy drove past its stand-off");
}

// Obstacles block the enemy exactly as they block you -- which is what makes
// hiding behind one work in both directions.
void test_the_enemy_cannot_drive_through_an_obstacle(void)
{
    run("make \"ox [800 800 800 800]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [1000 1000 1000 1000]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);

    // Straight in front of the cube at 1,000, driving into it.
    enemy_at(800, 1000 - num(":e.coll") + 2.0f, 0);
    const float before = num(":e.z");
    run("make \"e.f 1  make \"e.t 0  move.enemy");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(before, num(":e.z"), "the enemy drove into a cube");

    // Facing away from it, it is free to go.
    enemy_at(800, 1000 - num(":e.coll") + 2.0f, 180);
    run("make \"e.f 1  make \"e.t 0  move.enemy");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.z") < before, "the enemy could not drive away from a cube");
}

// A spawn is the only way anything in this game can arrive INSIDE an obstacle,
// because every other placement is a move `blocked?` refused.  And an enemy
// that starts inside the guard radius can never leave it in any direction --
// every candidate step is still inside -- so it spends its whole life sitting
// in a cube shooting at you.
//
// Four bearings is not a guarantee and this does not test one.  What it tests
// is the RATE: the field below is eight obstacles laid on the spawning ring
// itself, which blocks about a quarter of all bearings, so without the re-roll
// something like fifteen of these sixty spawns would land in a cube.  `rerandom`
// makes the sequence the same every run, so the threshold is a real bound and
// not a coin toss.
void test_a_spawn_is_re_rolled_out_of_an_obstacle(void)
{
    // A ring of obstacles at exactly spawning distance.
    run("make \"ox [800 1420 800 180]  make \"ox se :ox [1239 1239 361 361]");
    run("make \"oz [1420 800 180 800]  make \"oz se :oz [1239 361 361 1239]");
    camera_at(800, 800, 0);
    run("rerandom");

    int stuck = 0;
    for (int trial = 0; trial < 60; trial++)
    {
        run("spawn.enemy");
        run("make \"tk.dx :e.dx  make \"tk.dz :e.dz");
        run("make \"tk.guard :e.coll");
        if (truth("blocked?"))
            stuck++;
    }
    char msg[96];
    snprintf(msg, sizeof(msg), "%d of 60 spawns landed inside an obstacle", stuck);
    TEST_ASSERT_TRUE_MESSAGE(stuck <= 3, msg);
}

//==========================================================================
// M2 -- the shells
//==========================================================================

// The tunnelling invariant, in arithmetic rather than in play: a shell moves
// `sh.step` between two frames and is tested only at the ends of that step, so
// every guard has to be at least half a step wider than the thing it tests
// against or a fast shell passes through a solid object.
void test_the_shell_guards_clear_half_a_step(void)
{
    const float half_step = num(":sh.step") / 2.0f;
    TEST_ASSERT_TRUE_MESSAGE(num(":sh.guard") >= num(":half") + half_step,
                             "a shell tunnels through an obstacle");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.hit") >= num(":ehalf") + half_step,
                             "a shell tunnels through the enemy");
    TEST_ASSERT_TRUE_MESSAGE(num(":tk.hit") >= half_step,
                             "the enemy's shell tunnels through the player");
}

// A shell flies where the gun was pointing and not where it is pointing now.
// The velocity is fixed at the moment of firing, so turning after the shot
// does not steer it -- which is the difference between a cannon and a missile,
// and the missile is M3.
void test_a_shell_flies_the_heading_it_was_fired_along(void)
{
    camera_at(800, 800, 0);
    run("make \"sh.on false  make \"tk.boom 0  fire");
    TEST_ASSERT_TRUE(truth(":sh.on"));

    // Spin the tank right round before the shell steps.
    camera_at(800, 800, 90);
    run("make \"e.alive false  step.shell");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(800, num(":sh.x"), "the shell followed the turret");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(800 + num(":sh.step"), num(":sh.z"),
                                    "the shell did not fly down its own heading");
}

void test_only_one_shell_is_in_the_air_at_a_time(void)
{
    camera_at(800, 800, 0);
    run("make \"sh.on false  make \"tk.boom 0  fire");
    const float z = num(":sh.z");
    run("make \"e.alive false  step.shell  step.shell");
    run("fire");
    TEST_ASSERT_TRUE_MESSAGE(num(":sh.z") > z, "firing again reloaded the shell in flight");
}

// The kill, the explosion and the replacement, in one run of frames.
void test_a_shell_kills_the_enemy_and_another_arrives(void)
{
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);
    enemy_at(800, 1000, 180);
    run("make \"kills 0  make \"sh.on false  make \"tk.boom 0  fire");

    for (int i = 0; i < 12 && truth(":e.alive"); i++)
        run("step.shell");

    TEST_ASSERT_FALSE_MESSAGE(truth(":e.alive"), "the shell flew through the enemy");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":kills"));
    TEST_ASSERT_FALSE_MESSAGE(truth(":sh.on"), "the shell survived its own kill");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.boom") > 0, "the enemy died without an explosion");

    // The explosion runs down and a new enemy takes the field.
    for (int i = 0; i < (int)num(":boom.frames") + 1; i++)
        run("step.enemy");
    TEST_ASSERT_TRUE_MESSAGE(truth(":e.alive"), "no enemy came back");

    // On the plain, and out at spawning distance rather than on top of you.
    const float world = num(":world");
    TEST_ASSERT_TRUE(num(":e.x") >= 0 && num(":e.x") < world);
    TEST_ASSERT_TRUE(num(":e.z") >= 0 && num(":e.z") < world);
    const float d = fabsf(num(":e.dx")) + fabsf(num(":e.dz"));
    TEST_ASSERT_TRUE_MESSAGE(d > num(":e.range"), "the replacement arrived in your lap");
}

// Design section 2: obstacles block shots and can be hidden behind.  Without
// this the cubes are scenery.
void test_an_obstacle_stops_a_shell(void)
{
    run("make \"ox [800 800 800 800]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [1000 1000 1000 1000]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);
    // The enemy is directly behind the cube: a clean line of sight would kill
    // it, and the cube is what makes the shot miss.
    enemy_at(800, 1200, 180);
    run("make \"kills 0  make \"sh.on false  make \"tk.boom 0  fire");

    for (int i = 0; i < 20 && truth(":sh.on"); i++)
        run("step.shell");

    TEST_ASSERT_TRUE_MESSAGE(truth(":e.alive"), "the shell went through a cube");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":kills"));
    TEST_ASSERT_TRUE_MESSAGE(num(":sh.z") < 1000.0f, "the shell was stopped past the cube");
}

// B19's own case, in this game: a shot that crosses the seam has to hit the
// obstacle that is three steps away and not miss the one that reads as 1,597.
// One wrapped table is what makes this true by construction rather than by a
// comparison somebody remembered to write.
void test_a_shell_hits_an_obstacle_across_the_seam(void)
{
    run("make \"ox [20 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [20 900 900 900]  make \"oz se :oz [900 900 900 900]");
    // Just inside the far corner of the plain, driving at the seam.
    camera_at(1590.0f, 1500.0f, 0.0f);
    run("make \"e.alive false  make \"sh.on false  make \"tk.boom 0  fire");

    for (int i = 0; i < 6 && truth(":sh.on"); i++)
        run("step.shell");
    TEST_ASSERT_FALSE_MESSAGE(truth(":sh.on"),
                              "the shell flew past an obstacle on the far side of the seam");
}

void test_a_shell_that_hits_nothing_expires(void)
{
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);
    run("make \"e.alive false  make \"sh.on false  make \"tk.boom 0  fire");

    for (int i = 0; i < (int)num(":sh.frames") + 2; i++)
        run("step.shell");
    TEST_ASSERT_FALSE_MESSAGE(truth(":sh.on"), "a shell flew for ever");
}

// THE AIM ERROR IS A DISTANCE AND NOT AN ANGLE, and this is the invariant that
// took three attempts to state.
//
// M2 found the enemy came straight at you with perfect aim, diagnosed it as
// `e.range` against `tk.hit`, and fixed it by moving the stand-off out to 400
// and adding a per-shot angular wobble.  It then validated that fix at exactly
// one distance -- the stand-off -- where the tank does miss 55 % of the time.
//
// A BOARD FOUND THE REST OF THE CURVE.  An angular error is thrown
// `d * tan(wob)` sideways, so it shrinks to nothing as the range does -- and
// the range is not the enemy's to choose, because the player can always drive
// closer.  Measured against a stationary player, the old model hit 19 of 40 at
// 400 steps, 33 at 200, and 40 of 40 everywhere inside 150.  No value of an
// angular wobble fixes that: the shrinking is the SHAPE of the error, not its
// size.
//
// So `e.wide` is now a lateral offset AT THE TARGET, in steps, turned into an
// angle for the range it is fired over.  The invariant collapses to one
// comparison in one unit -- a shot thrown fewer steps sideways than the
// half-width of the box it is aimed at cannot miss -- where §16.6.1's version
// joined three numbers in two units with a tangent and was wrong twice.
void test_the_aim_error_is_wider_than_the_box_it_is_aimed_at(void)
{
    int guns = 0;
    for (int k = 1; k <= 4; k++)
    {
        char expr[64], msg[128];
        snprintf(expr, sizeof(expr), "make \"e.kind %d  set.kind", k);
        run(expr);
        if (!truth(":e.gun"))
            continue;
        guns++;

        snprintf(msg, sizeof(msg),
                 "kind %d throws its shot %g steps against a %g box -- it cannot miss",
                 k, (double)num(":e.wide"), (double)num(":tk.hit"));
        TEST_ASSERT_TRUE_MESSAGE(num(":e.wide") > num(":tk.hit"), msg);

        // The span `random` draws from has to cover both sides of it, or the
        // error is one-sided and the enemy leads every shot the same way.
        snprintf(msg, sizeof(msg), "kind %d's wobble span does not straddle zero", k);
        TEST_ASSERT_TRUE_MESSAGE(num(":e.wide2") > 2 * num(":e.wide"), msg);
    }
    TEST_ASSERT_TRUE_MESSAGE(guns >= 2, "the invariant was checked against fewer than two guns");
}

// And the same thing measured rather than argued: drive forty shots at each of
// six ranges, from point blank to beyond the stand-off, and require BOTH
// outcomes at every one of them.  This is the test M2 needed and did not have
// -- its version fired only from the stand-off, which is the one distance the
// old model was safe at.
//
// It asserts both and never a ratio: the ratio is exactly what M4 tunes, and a
// test that pinned it would fight the tuning.
void test_no_range_is_a_range_the_enemy_cannot_miss_from(void)
{
    // The obstacles out of the way, so what stops a shell is the player or the
    // end of its life and never a cube.
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");
    // The mock's hardware random source is a CONSTANT 42, so without this every
    // shot draws the same wobble and the test would be measuring nothing.
    run("rerandom");

    const int ranges[] = {60, 100, 150, 200, 283, 400};
    for (int k = 1; k <= 4; k++)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "make \"e.kind %d  set.kind", k);
        run(expr);
        if (!truth(":e.gun"))
            continue;

        for (int r = 0; r < 6; r++)
        {
            int hits = 0, misses = 0;
            for (int shot = 0; shot < 40; shot++)
            {
                camera_at(800, 800, 0);
                // Dead ahead and facing straight back down the line, so the
                // bearing error is zero and what is left is the wobble alone.
                foe_at(k, 800, (float)(800 + ranges[r]), 180);
                run("make \"hits 0  make \"tk.boom 0  make \"es.on false  enemy.fires");
                for (int i = 0; i < 40 && truth(":es.on"); i++)
                    run("step.eshell");
                if (num(":hits") > 0) hits++; else misses++;
            }

            char msg[160];
            snprintf(msg, sizeof(msg),
                     "kind %d at %d steps: %d hit, %d missed of 40", k, ranges[r], hits, misses);
            TEST_ASSERT_TRUE_MESSAGE(hits > 0, msg);
            TEST_ASSERT_TRUE_MESSAGE(misses > 0, msg);
        }
    }
}

// The cheapest collision in the game: the player is at the origin of the frame
// every offset here is already in, so it is two comparisons and no arithmetic.
void test_the_enemys_shell_hits_the_player_and_pauses_the_tank(void)
{
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);
    enemy_at(800, 900, 180);
    run("make \"hits 0  make \"tk.boom 0  make \"es.on false  enemy.fires");
    TEST_ASSERT_TRUE(truth(":es.on"));

    for (int i = 0; i < 8 && truth(":es.on"); i++)
        run("step.eshell");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":hits"), "the enemy's shell went through the player");
    TEST_ASSERT_TRUE_MESSAGE(num(":tk.boom") > 0, "being hit cost the player nothing");

    // And the pause is a pause: the treads do not drive during it.
    press_forward();
    run("pollkeys  step.tank");
    release_forward();
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(800, num(":pz"), "a burning tank drove off");
    TEST_ASSERT_TRUE_MESSAGE(num(":tk.boom") > 0, "the pause ended in one frame");

    // It runs down and the tank drives again.
    for (int i = 0; i < (int)num(":boom.frames") + 1; i++)
        run("step.tank");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":tk.boom"));
    press_forward();
    run("pollkeys  step.tank");
    release_forward();
    TEST_ASSERT_TRUE_MESSAGE(num(":pz") > 800.0f, "the tank never came back");
}

//==========================================================================
// M2 -- the explosion and the radar
//==========================================================================

void test_the_explosion_draws_its_fragments_and_runs_down(void)
{
    camera_at(800, 800, 0);
    enemy_at(800, 1000, 180);
    run("make \"kills 0  kill.enemy");
    TEST_ASSERT_EQUAL_FLOAT(num(":boom.frames"), num(":bm.n"));

    mock_device_clear_graphics();
    run("draw.boom");
    TEST_ASSERT_EQUAL_INT(FRAGS_BOOM, mock_device_line_count());

    for (int i = 0; i < (int)num(":boom.frames") + 2; i++)
        run("draw.boom");
    mock_device_clear_graphics();
    run("draw.boom");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(), "the explosion never went out");
}

// The blip is the enemy's camera-frame position scaled, which is what makes it
// free: right of you is right on the radar, behind you is below the centre.
// An arctangent here would be the same picture and three statements more.
void test_the_blip_is_the_enemy_in_the_camera_frame(void)
{
    const float cx = num(":rd.x"), cy = num(":rd.y");
    camera_at(800, 800, 0);

    enemy_at(1000, 1000, 0);          // ahead and to the right
    run("blip");
    TEST_ASSERT_TRUE_MESSAGE(num(":rd.bx") > cx, "a blip to the right drew to the left");
    TEST_ASSERT_TRUE_MESSAGE(num(":rd.by") > cy, "a blip ahead drew behind");

    enemy_at(600, 600, 0);            // behind and to the left
    run("blip");
    TEST_ASSERT_TRUE_MESSAGE(num(":rd.bx") < cx, "a blip to the left drew to the right");
    TEST_ASSERT_TRUE_MESSAGE(num(":rd.by") < cy, "a blip behind drew ahead");

    // Turning the tank turns the radar picture, because the frame is the
    // camera's and not the world's.
    camera_at(800, 800, 90);
    enemy_at(1000, 800, 0);           // now dead ahead
    run("blip");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, cx, num(":rd.bx"), "the radar did not turn with the tank");
}

void test_the_radar_is_drawn_and_the_blip_is_inside_it(void)
{
    camera_at(800, 800, 0);
    enemy_at(800, 1000, 180);
    mock_device_clear_graphics();
    run("radar");
    const int with_blip = mock_device_line_count();

    run("make \"e.alive false");
    mock_device_clear_graphics();
    run("radar");
    const int without = mock_device_line_count();
    TEST_ASSERT_TRUE_MESSAGE(with_blip > without, "a live enemy drew no blip");

    // The face of the radar IS the far plane: an enemy at `far` dead ahead sits
    // on the rim, and one further out than that -- which a fresh spawn on the
    // diagonal can be -- falls off the face rather than being drawn outside the
    // circle.
    const float far = num(":far");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, num(":rd.r") / far, num(":rd.sc"),
                                     "the radar's face is not the far plane");
    enemy_at(800, 800 + far - 10.0f, 180);
    mock_device_clear_graphics();
    run("radar");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > without,
                             "an enemy just inside the far plane fell off the radar");

    enemy_at(800 + far, 800 + far, 180);
    mock_device_clear_graphics();
    run("radar");
    TEST_ASSERT_EQUAL_INT_MESSAGE(without, mock_device_line_count(),
                                  "a distant enemy drew a blip outside the radar");
}

//==========================================================================
// M2 -- the clock, and the density that depends on it
//==========================================================================

// M1 closed with this settled: ask for the fast clock, READ IT BACK, and cut
// the field if the board refused.  M2's line items are about 17 ms against 16
// spare at 150 MHz and 33 at 300, so the overclock is this milestone's
// enabling condition and the cut is what makes a refusal playable instead of
// slow.
void test_the_game_asks_for_the_fast_clock_and_reads_it_back(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
    run("ignore clock");
    TEST_ASSERT_EQUAL_STRING("fast", value_to_string(eval_string(":cpu.at").value));
    // Asked for on the HARDWARE and not just recorded: `hw.cpu` reads the board
    // back, so a `cpu.at` of "fast without the clock having moved would mean the
    // read was answering from memory.
    TEST_ASSERT_EQUAL_UINT32(LOGO_CPU_KHZ_FAST, mock_cpu_khz);
}

// B50: a game that leaves the board overclocked has changed the machine and not
// just played on it.  On a board with PSRAM that is not merely impolite -- the
// QMI's timing for the external RAM is computed once at boot against the clock
// running then, so an overclock nothing retunes drives it out of spec, and the
// editor's buffers are the things that live there.
//
// It restores what it FOUND rather than `normal`, so a session that was already
// fast when the game started stays fast when it ends.
void test_the_game_gives_the_clock_back_when_it_exits(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
    run("clock");
    TEST_ASSERT_EQUAL_UINT32(LOGO_CPU_KHZ_FAST, mock_cpu_khz);
    run("restore.clock");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(LOGO_CPU_KHZ_NORMAL, mock_cpu_khz,
                                     "the game left the board overclocked");

    // Already fast when it started: it stays fast, because the clock was not
    // this game's to change back.
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_FAST);
    run("clock");
    run("restore.clock");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(LOGO_CPU_KHZ_FAST, mock_cpu_khz,
                                     "the game undid a clock it did not set");

    // A board with no settable clock has nothing to put back and must not error.
    set_mock_cpu_khz(false, LOGO_CPU_KHZ_NORMAL);
    run("clock");
    run("restore.clock");
    TEST_ASSERT_EQUAL_STRING("unknown", value_to_string(eval_string(":cpu.was").value));
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
}

// The exit path has to actually call it, and no test can enter the loop that
// precedes it -- so this reads the tail of `battlezone` out of the source, the
// way the frame-order test reads `play.frame`.
void test_the_exit_path_restores_the_clock(void)
{
    FILE *f = fopen(BATTLEZONE_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL(f);

    char line[512];
    bool in_body = false, after_loop = false, restores = false;
    while (fgets(line, sizeof(line), f))
    {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';')
            continue;
        if (!in_body) { in_body = strncmp(p, "to battlezone", 13) == 0; continue; }
        if (repl_line_is_end(p))
            break;
        if (strncmp(p, "until", 5) == 0) { after_loop = true; continue; }
        if (after_loop && strncmp(p, "restore.clock", 13) == 0)
            restores = true;
    }
    fclose(f);
    TEST_ASSERT_TRUE_MESSAGE(after_loop, "battlezone has no play loop");
    TEST_ASSERT_TRUE_MESSAGE(restores, "battlezone exits without giving the clock back");
}

// THE FAST CLOCK IS A PRECONDITION AND NOT A PREFERENCE, which is M3's decision
// and it reverses M2's.  There was a fallback -- three obstacles to two -- and
// M2 measured it: the peak frame goes 84.8 to 77.0 against a 66.7 ms budget,
// still over by 10.3.  Getting the peak inside at 150 MHz needs the obstacle
// field gone, which is not a game.  So `clock` now answers a question instead
// of cutting the scene, and a board that says no gets a message.
//
// Both halves, because they are different failures: a board that HAS a settable
// clock and refuses the value, and a board with no `hw.cpu` at all.  They
// answer the same way here, which is the point -- the difference does not
// matter to a game that cannot run either way.
void test_the_fast_clock_is_a_precondition(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
    TEST_ASSERT_TRUE_MESSAGE(truth("clock"), "a board that takes the clock was refused a game");
    TEST_ASSERT_EQUAL_STRING("fast", value_to_string(eval_string(":cpu.at").value));

    set_mock_cpu_khz(false, LOGO_CPU_KHZ_NORMAL);
    TEST_ASSERT_FALSE_MESSAGE(truth("clock"), "a board that refused the clock was given a game");
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
}

// `fps` and `max.obstacles` are tuning numbers again, and nothing decides them
// at startup.  While the fallback existed they were things `clock` WROTE, which
// is what stopped either being readable as a constant -- every per-frame number
// in the file had to be argued against a rate that might move underneath it.
void test_the_clock_does_not_write_the_tuning(void)
{
    const float fps = num(":fps");
    const float objects = num(":max.obstacles");

    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
    run("ignore clock");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(fps, num(":fps"), "`clock` wrote the frame rate");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(objects, num(":max.obstacles"),
                                    "`clock` wrote the object cap");

    set_mock_cpu_khz(false, LOGO_CPU_KHZ_NORMAL);
    run("ignore clock");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(fps, num(":fps"), "a refused clock cut the frame rate");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(objects, num(":max.obstacles"),
                                    "a refused clock cut the object cap");
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
}

// A refused board gets the reason and no game, and the reason has a NUMBER in
// it: this refuses because 84.8 ms was measured against 66.7, and somebody
// reading the message should be able to tell it was measured rather than
// assumed.  The message also has to survive the tail's `textscreen ct`, which
// is why `no.clock` runs after the clear rather than instead of the game --
// a `ct` inside it would wipe what it had just printed.
void test_a_refused_board_is_told_why_and_gets_no_game(void)
{
    set_mock_cpu_khz(false, LOGO_CPU_KHZ_NORMAL);
    proc_define_from_text("to one.game\nmake \"played true\nend");
    run("make \"played false");

    mock_device_clear_output();
    run("battlezone");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_FALSE_MESSAGE(truth(":played"), "a board that cannot run the frame was given a game");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "300 MHz"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "84.8"), screen);
    TEST_ASSERT_NULL_MESSAGE(strstr(screen, "Battlezone M3"), screen);

    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
}

//==========================================================================
// M2 -- one wrapped table, and the order the frame reads it in
//==========================================================================

// The design's own answer to B19: the plain wraps in exactly one place, so a
// collision cannot get it wrong in one test and right in another.  This reads
// the table back at the seam, which is where a fold that is off by a world is
// visible and nowhere else is.
void test_the_hoisted_field_is_wrapped_about_the_camera(void)
{
    const float world = num(":world"), half = num(":half.world");
    run("make \"ox [20 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [1580 100 100 100]  make \"oz se :oz [100 100 100 100]");
    camera_at(1590.0f, 20.0f, 0.0f);

    // The obstacle at x = 20 is 30 steps ahead across the seam, not 1,570 back.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f, item_of("obx", 1));
    // And the one at z = 1,580 is 40 behind, not 1,560 ahead.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -40.0f, item_of("obz", 1));

    for (int i = 1; i <= (int)num(":ob.count"); i++)
    {
        TEST_ASSERT_TRUE_MESSAGE(fabsf(item_of("obx", i)) <= half, "a folded offset left the plain");
        TEST_ASSERT_TRUE_MESSAGE(fabsf(item_of("obz", i)) <= half, "a folded offset left the plain");
    }
    TEST_ASSERT_EQUAL_INT((int)num(":ob.count"), (int)num("count :obx"));
    TEST_ASSERT_EQUAL_INT((int)num(":ob.count"), (int)num("count :obz"));
    TEST_ASSERT_TRUE(world > 0);
}

// Every move happens before any draw, and it is load-bearing rather than tidy.
// The shells step last because the frame that kills the enemy must not also
// draw it: a shell stepped after the drawing would hit a tank the player had
// already watched explode.  And `ob.scan` follows the tank's move, so anything
// that reads the table has to come after `step.tank`.
//
// Nothing on the host can see this by playing -- it is one frame of one
// picture -- so this reads the order back out of the Logo source, the way the
// frame-timer test reads the timers.
void test_the_frame_moves_everything_before_it_draws_anything(void)
{
    FILE *f = fopen(BATTLEZONE_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL(f);

    char line[512];
    bool in_play_frame = false;
    int tank = -1, enemy = -1, shell = -1, eshell = -1, clear = -1, n = 0;

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
        if (strncmp(p, "step.tank", 9) == 0) tank = n;
        if (strncmp(p, "step.enemy", 10) == 0) enemy = n;
        if (strncmp(p, "step.shell", 10) == 0) shell = n;
        if (strncmp(p, "step.eshell", 11) == 0) eshell = n;
        if (strncmp(p, "clean", 5) == 0) clear = n;
    }
    fclose(f);

    TEST_ASSERT_TRUE_MESSAGE(tank > 0 && enemy > 0 && shell > 0 && eshell > 0 && clear > 0,
                             "play.frame is missing one of its steps");
    TEST_ASSERT_TRUE_MESSAGE(tank < enemy, "the enemy steps before the table is rescanned");
    TEST_ASSERT_TRUE_MESSAGE(enemy < shell, "a shell is tested against last frame's enemy");
    TEST_ASSERT_TRUE_MESSAGE(enemy < eshell, "a shell is tested against last frame's enemy");
    TEST_ASSERT_TRUE_MESSAGE(shell < clear, "a shell steps after the frame is drawn");
    TEST_ASSERT_TRUE_MESSAGE(eshell < clear, "a shell steps after the frame is drawn");
}

// `ob.scan` is what makes every collision in this game two comparisons, and it
// only works if the tank's move rebuilds it.  Leave it out and the field is
// drawn from wherever the camera was last time.
void test_the_tank_rescans_the_field_when_it_moves(void)
{
    run("make \"ox [800 800 800 800]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [1200 1200 1200 1200]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);
    const float before = item_of("obz", 1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 400.0f, before);

    press_forward();
    run("pollkeys  step.tank");
    release_forward();
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 400.0f - 2.0f * num(":tread.step"),
                                     item_of("obz", 1),
                                     "the field was not rescanned after the tank moved");
}

// M3 splits what M2's entry point did in one place into two.  `battlezone` is
// now the SESSION -- the sound, the clock, and a loop over games -- and
// `init.game` is one game's setup.  Only the first still ends in a loop no
// test can enter, so only the first still needs the source-reading trick.
//
// `init.game` a test can simply call, which is better than reading it: it runs
// the real procedure rather than a copy of its statements, so a line that
// works only in file order cannot pass here and fail on a board.
void test_the_entry_point_sets_the_game_up(void)
{
    new_game();

    // It leaves a game ready to play: an enemy out on the plain, nothing in
    // the air, the glass intact and the score at zero.
    TEST_ASSERT_TRUE(truth(":e.alive"));
    TEST_ASSERT_FALSE(truth(":sh.on"));
    TEST_ASSERT_FALSE(truth(":es.on"));
    TEST_ASSERT_FALSE(truth(":cracked"));
    TEST_ASSERT_TRUE(truth(":playing"));
    TEST_ASSERT_EQUAL_FLOAT(0, num(":score"));
    TEST_ASSERT_EQUAL_FLOAT(0, num(":kills"));
    TEST_ASSERT_EQUAL_FLOAT(0, num(":hits"));
    TEST_ASSERT_EQUAL_FLOAT(0, num(":tk.boom"));
    TEST_ASSERT_EQUAL_FLOAT(num(":start.lives"), num(":lives"));
    TEST_ASSERT_EQUAL_FLOAT(num(":extra.at"), num(":extra.due"));
    const float d = fabsf(num(":e.dx")) + fabsf(num(":e.dz"));
    TEST_ASSERT_TRUE_MESSAGE(d > num(":e.range"), "the game opens with an enemy in your lap");

}

// The session's own body, which no test can call because it ends in a loop
// only the attract screen leaves.  So this runs its statements, from the
// source, up to that loop -- the same trick the frame-order test uses, pointed
// at running the lines rather than counting them.  A misspelled name in here
// is a crash on the board and nothing at all on the host.
//
// The clock is what it exists to check.  M2 asked for it once per game; M3
// asks once per SESSION, because ESC now returns to an attract screen and a
// player starting their fourth game should not pay for a wireless bus teardown
// again.  If it ever moved back inside `init.game` this is the test that
// notices.
void test_the_session_asks_for_the_clock_before_any_game(void)
{
    run("make \"cpu.at \"unknown  make \"leaving false");

    FILE *f = fopen(BATTLEZONE_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL(f);

    char line[512];
    bool in_body = false;
    int ran = 0;
    while (fgets(line, sizeof(line), f))
    {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        size_t len = strlen(p);
        while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r')) p[--len] = '\0';
        if (len == 0 || *p == ';')
            continue;
        if (!in_body) { in_body = strncmp(p, "to battlezone", 13) == 0; continue; }
        if (strncmp(p, "until", 5) == 0)
            break;
        run(p);
        ran++;
    }
    fclose(f);
    TEST_ASSERT_TRUE_MESSAGE(ran >= 2, "the session's body was not found");

    TEST_ASSERT_EQUAL_STRING_MESSAGE("fast", value_to_string(eval_string(":cpu.at").value),
                                     "the session did not ask for the clock");
    TEST_ASSERT_FALSE_MESSAGE(truth(":leaving"), "the session opened already leaving");
}

// The whole frame, with an enemy and both shells in the air, so that whatever
// the pieces do separately they also do together.
void test_a_frame_with_an_enemy_and_shells_runs(void)
{
    run("make \"paused false  make \"quit false  make \"frame.count 0");
    camera_at(800, 800, 0);
    enemy_at(800, 1100, 180);
    run("make \"tk.boom 0  make \"sh.on false  make \"es.on false");
    run("make \"kills 0  make \"hits 0  pollkeys");

    press(KEY_FIRE);
    run("play.frame");
    release(KEY_FIRE);
    TEST_ASSERT_TRUE_MESSAGE(truth(":sh.on"), "] did not fire");

    mock_device_clear_graphics();
    run("play.frame");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > SEGS_HORIZON + EDGES_SIGHT + EDGES_ENEMY,
                             "the frame did not draw the enemy and the radar");

    press_forward();
    for (int i = 0; i < 200; i++)
        run("play.frame");
    release_forward();
    TEST_ASSERT_TRUE_MESSAGE(num("atoms") > 0, "the workspace ran out with the enemy in the frame");
    const float world = num(":world");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.x") >= 0 && num(":e.x") < world, "the enemy left the plain");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.z") >= 0 && num(":e.z") < world, "the enemy left the plain");
}

//==========================================================================
// M3 -- the game
//
// Lives, a score, the four enemies in a sequence, the cracked screen, the
// attract screen with its high score table, and the sound.  What the host can
// check here is everything except how it feels, which is M4's and a board's.
//==========================================================================

// The mock's hardware random source is a CONSTANT 42, so anything that draws a
// number and expects variety has to switch to the seeded sequence first.  The
// spawn re-roll test set the precedent at M2.
#define SCORES_FILE "/games/battlezone.scores"

static int notes_on(int voice, int from)
{
    const MockDeviceState *st = mock_device_get_state();
    int n = 0;
    for (int i = from; i < st->sound.gate_count; i++)
        if (st->sound.gates[i].voice == voice)
            n++;
    return n;
}

static uint32_t last_freq_on(int voice)
{
    const MockDeviceState *st = mock_device_get_state();
    for (int i = st->sound.gate_count - 1; i >= 0; i--)
        if (st->sound.gates[i].voice == voice)
            return st->sound.gates[i].freq;
    return 0;
}


//--------------------------------------------------------------------------
// The enemy sequence
//--------------------------------------------------------------------------

// The escalation is a ring rather than a state machine, so what this checks is
// that the ring is walked in order and comes back round.  A sequence that ran
// off the end of the list would read `item 9` of an eight-element list, which
// is an error on the board and nothing a play test would predict.
void test_the_sequence_walks_the_ring_and_comes_back_round(void)
{
    const int len = (int)num("count :e.order");
    TEST_ASSERT_TRUE_MESSAGE(len > 1, "the enemy sequence is not a sequence");

    run("make \"e.seq 0");
    for (int i = 1; i <= len; i++)
    {
        char expr[64];
        run("next.kind");
        snprintf(expr, sizeof(expr), "0 + item %d :e.order", i);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(expr), num(":e.kind"),
                                        "the sequence does not follow `e.order`");
    }

    run("next.kind");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.seq"), "the ring did not come back round");
    TEST_ASSERT_EQUAL_FLOAT(num("0 + item 1 :e.order"), num(":e.kind"));
}

// All four appear, and the tank is the staple.  A ring that had turned into
// four saucers would still pass the test above.
void test_the_ring_carries_all_four_kinds(void)
{
    int seen[5] = {0, 0, 0, 0, 0};
    const int len = (int)num("count :e.order");

    run("make \"e.seq 0");
    for (int i = 0; i < len; i++)
    {
        run("next.kind");
        const int k = (int)num(":e.kind");
        TEST_ASSERT_TRUE_MESSAGE(k >= 1 && k <= 4, "the ring holds a kind that does not exist");
        seen[k]++;
    }

    for (int k = 1; k <= 4; k++)
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "kind %d never appears in the sequence", k);
        TEST_ASSERT_TRUE_MESSAGE(seen[k] > 0, msg);
    }
    TEST_ASSERT_TRUE_MESSAGE(seen[1] > seen[4], "the saucer is not rarer than the tank");
}

// Every row is complete.  `set.kind` chooses by comparing `e.kind` against four
// literals, so a kind that matched none of them would leave the PREVIOUS
// enemy's numbers in place -- a supertank wearing a saucer's score, and nothing
// on the host or the board that says so.
void test_every_kind_sets_a_whole_row(void)
{
    for (int k = 1; k <= 4; k++)
    {
        char expr[64], msg[96];
        run("make \"e.pts 0  make \"e.hw 0  make \"e.hit 0");
        snprintf(expr, sizeof(expr), "make \"e.kind %d  set.kind", k);
        run(expr);

        snprintf(msg, sizeof(msg), "kind %d is worth nothing", k);
        TEST_ASSERT_TRUE_MESSAGE(num(":e.pts") > 0, msg);
        snprintf(msg, sizeof(msg), "kind %d has no size", k);
        TEST_ASSERT_TRUE_MESSAGE(num(":e.hw") > 0, msg);
        snprintf(msg, sizeof(msg), "kind %d cannot be hit", k);
        TEST_ASSERT_TRUE_MESSAGE(num(":e.hit") > 0, msg);
        // `e.naim` is the one derived number and it is hoisted out of the four
        // rows, so a row that forgot it would leave the last enemy's.
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-num(":e.aim"), num(":e.naim"),
                                        "the aim window is not symmetric");
    }
}

// The arcade's table, and it is the one thing in M3 that is not this design's
// invention.
void test_each_kind_is_worth_its_arcade_score(void)
{
    const struct { int kind; float pts; } table[] = {
        {1, 1000.0f}, {2, 2000.0f}, {3, 3000.0f}, {4, 5000.0f}};

    for (int i = 0; i < 4; i++)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "make \"e.kind %d  set.kind", table[i].kind);
        run(expr);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(table[i].pts, num(":e.pts"),
                                        "a kind is not worth what the cabinet paid");
    }
}


// "Faster, smarter" is four numbers, and a supertank that was only bigger would
// pass every other test in this file.
void test_a_supertank_outclasses_a_tank(void)
{
    run("make \"e.kind 1  set.kind");
    const float step = num(":e.step"), turn = num(":e.turn");
    const float reload = num(":e.reload"), wide = num(":e.wide"), hw = num(":e.hw");

    run("make \"e.kind 3  set.kind");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.step") > step, "a supertank is no faster");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.turn") > turn, "a supertank turns no harder");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.reload") < reload, "a supertank reloads no faster");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.wide") < wide, "a supertank is no more accurate");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.hw") > hw, "a supertank looks exactly like a tank");
}

//--------------------------------------------------------------------------
// The missile
//--------------------------------------------------------------------------

// It closes forever and it never fires, which is what `e.range` 0 and no gun
// mean.  A missile that held a stand-off would sit out at 400 steps being
// harmless.
void test_a_missile_closes_forever_and_never_fires(void)
{
    camera_at(800, 800, 0);
    foe_at(2, 800, 1100, 180);
    run("make \"e.cool 0  make \"es.on false  make \"frame.count 0  hunt");

    TEST_ASSERT_FALSE_MESSAGE(truth(":e.gun"), "a missile has a gun");
    TEST_ASSERT_TRUE_MESSAGE(truth(":e.ram"), "a missile does not ram");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.fire"), "a missile decided to shoot at you");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.f"), "a missile stopped closing");

    // And from right on top of you it is still closing, which is the whole
    // difference between it and a tank.
    enemy_at(800, 860, 180);
    run("hunt");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.f"), "a missile held a stand-off");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.fire"), "a missile fired a shell");
}

// It kills by ARRIVING, and it dies of the hit it scores.  A missile that rammed
// you and survived would be an unkillable enemy standing in your lap.
void test_a_missile_kills_by_arriving_and_dies_of_it(void)
{
    new_game();
    camera_at(800, 800, 0);
    foe_at(2, 800, 810, 180);
    run("make \"tk.boom 0  make \"e.boom 0  make \"hits 0");
    const float lives = num(":lives");

    run("enemy.rams");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":hits"), "the missile did not reach the player");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(lives - 1, num(":lives"), "the ram cost no tank");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.alive"), "the missile survived hitting you");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.boom") > 0, "the missile did not blow up");

    // And it scores the player nothing: you did not shoot it, it shot you.
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":score"), "being rammed scored points");

}

// Out of range it does nothing at all, which is the other half of two
// comparisons: a guard that tested only one axis would let a missile passing
// 300 steps to your left kill you.
void test_a_missile_that_has_not_arrived_does_nothing(void)
{
    camera_at(800, 800, 0);
    foe_at(2, 1100, 800, 180);
    run("make \"tk.boom 0  make \"hits 0  enemy.rams");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":hits"), "a missile 300 steps away hit the player");
    TEST_ASSERT_TRUE(truth(":e.alive"));
}

// Four edges: the body, two side fins, and both vertical fins as ONE stroke --
// they are three points on a straight line through the tail, so a line through
// the middle one is the same ink as two lines out of it.  The count is what
// pins the model, exactly as the cube's twelve and the enemy's thirteen do,
// and it is four rather than five because of that stroke.
void test_a_missile_draws_four_edges(void)
{
    camera_at(800, 800, 0);
    foe_at(2, 800, 1100, 180);
    mock_device_clear_graphics();
    TEST_ASSERT_TRUE_MESSAGE(truth("project.missile"), "the missile in front of you was culled");
    run("draw.missile");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, mock_device_line_count(), "the missile is not four edges");
}

// It flies at eye height, which is why a missile coming straight at you sits in
// the middle of the gunsight -- and the gunsight's gap is cut so that the shot
// you have to take is never covered.
void test_a_missile_flies_at_eye_height(void)
{
    camera_at(800, 800, 0);
    foe_at(2, 800, 1100, 180);
    run("ignore project.missile");

    const float hz = num(":hz");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, hz, num(":apy"), "the nose is off the eye line");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, hz, item_of("cy1", 1), "the tail is off the eye line");
    // The fins straddle it.
    TEST_ASSERT_TRUE_MESSAGE(item_of("cy2", 1) > hz, "the upper fin is not above the eye line");
    TEST_ASSERT_TRUE_MESSAGE(hz > item_of("cy2", 2), "the lower fin is not below the eye line");
}

//--------------------------------------------------------------------------
// The saucer
//--------------------------------------------------------------------------

// It drifts and it does not hunt: `hunt` leaves immediately, so the turn intent
// it was spawned with is all it ever has.  A saucer that turned towards you
// would be a slow tank that could not shoot.
void test_a_saucer_drifts_and_does_not_hunt(void)
{
    camera_at(800, 800, 0);
    foe_at(4, 800, 1100, 90);
    run("make \"e.t 0  make \"e.f 1  make \"e.fire false  hunt");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":e.t"), "the saucer turned towards the player");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.f"), "the saucer stopped drifting");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.fire"), "the saucer shot at you");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.gun"), "the saucer has a gun");
}

// It is 90 steps up and an obstacle is 40 tall, so it goes over them.  This is
// the one place in the file that knows a saucer flies, and a saucer that ground
// to a halt against an invisible cube would be a defect only a play test finds.
void test_a_saucer_flies_over_the_obstacles(void)
{
    // A cube dead in its path.
    run("make \"ox [800 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [900 100 100 100]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);
    foe_at(4, 800, 880, 0);

    const float before = num(":e.z");
    run("move.enemy");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.z") > before, "the saucer was stopped by an obstacle");

    // And a tank in the same place is not.
    foe_at(1, 800, 880, 0);
    const float tank_before = num(":e.z");
    run("move.enemy");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(tank_before, num(":e.z"),
                                    "a tank drove through the obstacle the saucer flew over");
}

// It leaves rather than circling forever.  Ignoring a saucer should cost you
// the points, not stall the sequence behind it -- and a departure is not a
// death: no explosion and no score.
void test_a_saucer_leaves_when_its_dwell_runs_out(void)
{
    new_game();
    camera_at(800, 800, 0);
    foe_at(4, 800, 1100, 0);
    run("make \"e.boom 0  make \"e.left 2  make \"bm.n 0  make \"frame.count 1");

    run("step.enemy");
    TEST_ASSERT_TRUE_MESSAGE(truth(":e.alive"), "the saucer left a frame early");
    run("step.enemy");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.alive"), "the saucer never left");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":score"), "a departure scored points");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":bm.n"), "a departure drew an explosion");

    // And the ring moves on: one more frame and the next enemy is out there.
    run("step.enemy");
    TEST_ASSERT_TRUE_MESSAGE(truth(":e.alive"), "the sequence stalled behind the saucer");

}

// Twelve edges: the rim quad, four to the dome and four to the keel.
void test_a_saucer_draws_twelve_edges(void)
{
    camera_at(800, 800, 0);
    foe_at(4, 800, 1100, 0);
    mock_device_clear_graphics();
    TEST_ASSERT_TRUE_MESSAGE(truth("project.saucer"), "the saucer in front of you was culled");
    run("draw.saucer");
    TEST_ASSERT_EQUAL_INT_MESSAGE(12, mock_device_line_count(), "the saucer is not twelve edges");
}

// It is rotationally symmetric, so its own heading must not reach the
// transform.  A saucer whose outline turned as it drifted would be a box, and
// the projection would have to do the work the symmetry is there to save.
void test_a_saucers_outline_does_not_turn_with_its_heading(void)
{
    camera_at(800, 800, 0);
    foe_at(4, 800, 1100, 0);
    run("ignore project.saucer");
    const float x1 = item_of("cx", 1), y1 = item_of("cy1", 1);
    const float ax = num(":apx"), ay = num(":apy");

    foe_at(4, 800, 1100, 137);
    run("ignore project.saucer");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, x1, item_of("cx", 1), "the rim turned with the heading");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, y1, item_of("cy1", 1), "the rim turned with the heading");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, ax, num(":apx"), "the dome turned with the heading");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, ay, num(":apy"), "the dome turned with the heading");
}

// It floats, which is the whole reason it is drawn as a plate seen edge-on: the
// dome is above the rim, the keel below it, and all three are above the horizon
// because the periscope is 12 steps off a plain the saucer is 90 above.
void test_a_saucer_floats_above_the_horizon(void)
{
    camera_at(800, 800, 0);
    foe_at(4, 800, 1100, 0);
    run("ignore project.saucer");

    const float hz = num(":hz");
    const float rim = item_of("cy1", 1);
    TEST_ASSERT_TRUE_MESSAGE(rim > hz, "the saucer is sitting on the ground");
    TEST_ASSERT_TRUE_MESSAGE(num(":apy") > rim, "the dome is not above the rim");
    TEST_ASSERT_TRUE_MESSAGE(rim > num(":p.g"), "the keel is not below the rim");
}

//--------------------------------------------------------------------------
// The models, together
//--------------------------------------------------------------------------

// Culling is conservative for the two new shapes exactly as it is for the tank:
// anything with a vertex inside the near plane goes whole, because the "some of
// it" version is what swings a projection through infinity and throws a line
// across the screen.
void test_the_new_models_are_culled_at_the_near_plane(void)
{
    const float near = num(":near");

    camera_at(800, 800, 0);
    foe_at(2, 800, 800 + near * 0.5f, 180);
    TEST_ASSERT_FALSE_MESSAGE(truth("project.missile"), "a missile inside the near plane was drawn");
    foe_at(2, 800, 800 + near * 4.0f, 180);
    TEST_ASSERT_TRUE_MESSAGE(truth("project.missile"), "a missile in clear view was culled");

    foe_at(4, 800, 800 + near * 0.5f, 0);
    TEST_ASSERT_FALSE_MESSAGE(truth("project.saucer"), "a saucer inside the near plane was drawn");
    foe_at(4, 800, 800 + near * 4.0f, 0);
    TEST_ASSERT_TRUE_MESSAGE(truth("project.saucer"), "a saucer in clear view was culled");
}

// The frame asks two booleans and never a kind.  What this pins is that the
// booleans reach the right model: a saucer drawn as a tank would be twelve
// edges either way and the count alone could not tell them apart, so the check
// is that the number of edges MOVES with the kind.
void test_the_frame_draws_the_model_that_matches_the_kind(void)
{
    const struct { int kind; int edges; } table[] = {
        {1, EDGES_ENEMY}, {2, 4}, {3, EDGES_ENEMY}, {4, 12}};

    for (int i = 0; i < 4; i++)
    {
        char msg[80];
        camera_at(800, 800, 0);
        foe_at(table[i].kind, 800, 1150, 180);
        mock_device_clear_graphics();
        run("draw.foe");
        snprintf(msg, sizeof(msg), "kind %d drew %d edges, not %d",
                 table[i].kind, mock_device_line_count(), table[i].edges);
        TEST_ASSERT_EQUAL_INT_MESSAGE(table[i].edges, mock_device_line_count(), msg);
    }
}

//--------------------------------------------------------------------------
// Lives and the score
//--------------------------------------------------------------------------

// Every point in the game arrives through `add.score`, which is what makes the
// bonus tank one `if` rather than a rule scattered over four scorers.
void test_a_kill_scores_what_the_enemy_is_worth(void)
{
    new_game();
    run("make \"score 0");
    camera_at(800, 800, 0);
    foe_at(4, 800, 1100, 0);
    run("kill.enemy");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5000, num(":score"), "a saucer scored the wrong amount");

    foe_at(1, 800, 1100, 180);
    run("kill.enemy");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(6000, num(":score"), "a tank scored the wrong amount");
}

// The bonus tank arrives on STEPPING OVER the boundary and not on landing on
// it, which a `remainder` would get wrong: a 5,000-point saucer can carry a
// score from 14,000 to 19,000 without ever equalling 15,000.
void test_a_bonus_tank_arrives_on_stepping_over_the_boundary(void)
{
    new_game();
    const float at = num(":extra.at");
    run("make \"score 0  make \"lives 3");
    char expr[64];
    snprintf(expr, sizeof(expr), "make \"extra.due %g", at);
    run(expr);

    snprintf(expr, sizeof(expr), "add.score %g", at - 1000.0f);
    run(expr);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":lives"), "a bonus tank arrived early");

    // Over the line rather than onto it.
    run("add.score 5000");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, num(":lives"), "stepping over the boundary won nothing");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(at + at, num(":extra.due"), "the next bonus is in the past");

    // And it does not fire again on the next point scored.
    run("add.score 100");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, num(":lives"), "the bonus repeated");
}

// Being hit costs a tank, cracks the glass and pauses you.  It does NOT end the
// game here: the count goes down and `respawn` reads it ten frames later, so
// the player watches their own tank come apart before the card goes up.
void test_a_hit_costs_a_tank_and_cracks_the_glass(void)
{
    new_game();
    run("make \"lives 3  make \"cracked false  make \"tk.boom 0");
    run("hit.player");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"), "the hit cost no tank");
    TEST_ASSERT_TRUE_MESSAGE(truth(":cracked"), "the glass did not crack");
    TEST_ASSERT_TRUE_MESSAGE(num(":tk.boom") > 0, "the hit did not pause the tank");
    TEST_ASSERT_TRUE_MESSAGE(truth(":playing"), "one hit ended the game");
}

// The pause runs out into a fresh tank, fresh glass and a FRESH ENEMY -- the
// last is the courtesy the cabinet extends, and without it the tank that killed
// you is still sitting at its stand-off with your new one in its sights.
void test_the_pause_runs_out_into_a_new_tank_and_a_new_enemy(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("make \"lives 3  make \"tk.boom 0  make \"es.on false");
    enemy_at(800, 900, 180);
    run("hit.player");

    for (int i = 0; i < (int)num(":boom.frames") + 1; i++)
        run("step.tank");

    TEST_ASSERT_FALSE_MESSAGE(truth(":cracked"), "the glass stayed cracked through the respawn");
    TEST_ASSERT_TRUE_MESSAGE(truth(":playing"), "the game ended with tanks left");
    const float d = fabsf(num(":e.dx")) + fabsf(num(":e.dz"));
    TEST_ASSERT_TRUE_MESSAGE(d > num(":e.range"),
                             "you respawned with the enemy that killed you still in your lap");
}

// TWO SPAWNS FOR ONE DEATH.  A missile sets its own `e.boom` and then kills
// you, so both countdowns start on the same frame and run out on the same one:
// `step.enemy` spawns from its counter and `respawn` spawns from the tank's,
// the ring advances twice, and one of the two enemies is never seen.  Nothing
// on the screen says so -- there is an enemy out there either way -- which is
// why the check is on the sequence position and not on the picture.
void test_a_ram_spawns_one_replacement_and_not_two(void)
{
    new_game();
    camera_at(800, 800, 0);
    foe_at(2, 800, 810, 180);
    run("make \"lives 3  make \"tk.boom 0  make \"e.boom 0  make \"frame.count 1");
    const float seq = num(":e.seq");

    run("enemy.rams");
    for (int i = 0; i < (int)num(":boom.frames") + 2; i++)
        run("step.tank  step.enemy");

    TEST_ASSERT_TRUE_MESSAGE(truth(":e.alive"), "nothing came back after the ram");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(seq + 1, num(":e.seq"),
                                    "one death advanced the enemy sequence twice");
}

// The last tank ends the game, and it ends it AFTER the pause rather than
// during it.
void test_the_last_tank_ends_the_game(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("make \"lives 1  make \"tk.boom 0");
    run("hit.player");
    TEST_ASSERT_TRUE_MESSAGE(truth(":playing"), "the game ended before the explosion was over");

    for (int i = 0; i < (int)num(":boom.frames") + 1; i++)
        run("step.tank");
    TEST_ASSERT_FALSE_MESSAGE(truth(":playing"), "running out of tanks did not end the game");
}

//--------------------------------------------------------------------------
// The cracked screen
//--------------------------------------------------------------------------

// The shatter is STATIC, which is what makes it read as damage to the glass
// rather than as something happening on the plain.  What is stored is a bearing,
// two lengths and a kink, so the turtle redraws the identical figure -- and a
// shatter that was re-rolled every frame would be a snowstorm.
void test_the_shatter_is_static_until_you_respawn(void)
{
    run("rerandom");
    run("make \"bm.x 0  make \"bm.y 40  crack.screen");

    mock_device_clear_graphics();
    run("draw.cracks");
    const int edges = mock_device_line_count();
    TEST_ASSERT_TRUE_MESSAGE(edges > 0, "the shatter drew nothing");

    // Same figure, twice: the endpoints of every stroke, not just the count.
    MockLine first[32];
    TEST_ASSERT_TRUE(edges <= (int)(sizeof(first) / sizeof(first[0])));
    for (int i = 0; i < edges; i++)
        first[i] = *mock_device_get_line(i);

    mock_device_clear_graphics();
    run("draw.cracks");
    TEST_ASSERT_EQUAL_INT_MESSAGE(edges, mock_device_line_count(), "the shatter changed shape");
    for (int i = 0; i < edges; i++)
    {
        const MockLine *l = mock_device_get_line(i);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, first[i].x1, l->x1, "the shatter moved");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, first[i].y1, l->y1, "the shatter moved");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, first[i].x2, l->x2, "the shatter moved");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, first[i].y2, l->y2, "the shatter moved");
    }
}

// Every crack has a kink in it.  A crack in glass runs, catches and runs again;
// six straight rays out of one point is a starburst, which is what an explosion
// looks like and not what damage looks like.
void test_every_crack_runs_in_two_strokes(void)
{
    run("rerandom");
    run("make \"bm.x 0  make \"bm.y 40  crack.screen");
    mock_device_clear_graphics();
    run("draw.cracks");

    const int edges = mock_device_line_count();
    TEST_ASSERT_EQUAL_INT_MESSAGE(12, edges, "the shatter is not six cracks of two strokes");

    // The strokes pair up: the second of each pair starts where the first ended,
    // and it is a KINK and not a continuation.
    for (int i = 0; i < edges; i += 2)
    {
        const MockLine *a = mock_device_get_line(i);
        const MockLine *b = mock_device_get_line(i + 1);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, a->x2, b->x1, "a crack is not one polyline");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, a->y2, b->y1, "a crack is not one polyline");
    }
}

// It is drawn only while it is cracked, and it costs a cracked frame twelve
// edges -- which is the arcade's real penalty for being hit: it takes the view
// rather than a number.
void test_the_shatter_is_drawn_only_while_it_is_cracked(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("make \"paused false  make \"cracked false  pollkeys");

    mock_device_clear_graphics();
    run("play.frame");
    const int clear = mock_device_line_count();

    run("rerandom  make \"bm.x 0  make \"bm.y 40  crack.screen");
    mock_device_clear_graphics();
    run("play.frame");
    const int cracked = mock_device_line_count();

    TEST_ASSERT_TRUE_MESSAGE(cracked > clear, "the cracked frame drew no shatter");
}

//--------------------------------------------------------------------------
// Sound
//--------------------------------------------------------------------------

// The waveform rule is an ERROR and not a shrug: 3 and 7 are the noise voices
// and 0-2 / 4-6 are the tone voices, so a timbre on the wrong family throws.
// The pairs are also the arrangement -- a pair is one sound centred across both
// ears -- so left and right must carry the same timbre or a sound arrives in
// one ear.
void test_the_timbres_are_set_for_every_pair(void)
{
    run("setup.sound");
    const MockDeviceState *st = mock_device_get_state();

    for (int v = 0; v < 4; v++)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "voice %d and %d are not the same timbre", v, v + 4);
        TEST_ASSERT_EQUAL_INT_MESSAGE(st->sound.wave[v].wave, st->sound.wave[v + 4].wave, msg);

        snprintf(msg, sizeof(msg), "voice %d has the wrong wave family", v);
        if (v == 3)
            TEST_ASSERT_TRUE_MESSAGE(st->sound.wave[v].wave >= SOUND_WAVE_WHITE, msg);
        else
            TEST_ASSERT_TRUE_MESSAGE(st->sound.wave[v].wave < SOUND_WAVE_WHITE, msg);
    }

    // The engine's attack is a RAMP and not a step, because it is re-gated on
    // every frame and a step there is a click fifteen times a second.  Under
    // about 7 ms the engine's refill block makes it a step whatever is asked.
    TEST_ASSERT_TRUE_MESSAGE(st->sound.env[0].attack >= 5, "the engine re-gate will click");
    TEST_ASSERT_TRUE_MESSAGE(st->sound.env[1].attack >= 5, "the engine re-gate will click");
}

// The engine is TWO pairs a few hertz apart, and the beat between them is the
// sound rather than a detune to be tidied away.  One pair is a hum.
void test_the_engine_is_two_pairs_that_beat(void)
{
    run("make \"left.tread 0  make \"right.tread 0");
    int mark = mock_sound_gate_count();
    run("engine");

    TEST_ASSERT_TRUE_MESSAGE(notes_on(0, mark) > 0, "the engine's lower pair is silent");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(1, mark) > 0, "the engine's upper pair is silent");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(4, mark) > 0, "the engine is only in one ear");
    TEST_ASSERT_TRUE_MESSAGE(last_freq_on(1) > last_freq_on(0),
                             "the two halves of the engine are the same pitch -- there is no beat");
}

// The pitch follows how much TREAD is turning rather than how fast the tank is
// going, so a pivot -- two treads and no ground speed at all -- revs exactly as
// hard as driving straight does.  That is what a tracked vehicle sounds like
// and it is the wrong answer for anything with wheels.
void test_the_engine_pitch_follows_the_treads(void)
{
    run("make \"left.tread 0  make \"right.tread 0  engine");
    const uint32_t idle = last_freq_on(0);

    run("make \"left.tread 1  make \"right.tread 0  engine");
    const uint32_t one = last_freq_on(0);

    run("make \"left.tread 1  make \"right.tread 1  engine");
    const uint32_t driving = last_freq_on(0);

    // A pivot is one tread each way: no ground speed, both treads turning.
    run("make \"left.tread 1  make \"right.tread -1  engine");
    const uint32_t pivot = last_freq_on(0);

    TEST_ASSERT_TRUE_MESSAGE(one > idle, "the engine does not rev");
    TEST_ASSERT_TRUE_MESSAGE(driving > one, "the second tread adds nothing");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(driving, pivot, "a pivot does not rev like a drive");
}

// The alarm is a TEMPO as much as a pitch, and the gap closes as the enemy
// closes -- so a hunting supertank is frightening before it is visible.
void test_the_alarm_closes_its_gap_as_the_enemy_closes(void)
{
    camera_at(800, 800, 0);
    enemy_at(800, 1100, 0);

    // Two notes and not one: a single repeated note is a metronome.  This goes
    // FIRST because the mock's gate log holds 64 entries and a centred pair
    // spends two of them a note, so the counting below is what fills it.
    run("make \"au.in 1  make \"au.alt false  alarm");
    const uint32_t first = last_freq_on(2);
    run("make \"au.in 1  alarm");
    TEST_ASSERT_TRUE_MESSAGE(last_freq_on(2) != first, "the alarm is one note, not two");

    enemy_at(800, 1450, 0);
    run("make \"au.in 1");
    int mark = mock_sound_gate_count();
    run("repeat 30 [alarm]");
    const int far_off = notes_on(2, mark);

    enemy_at(800, 900, 0);
    run("make \"au.in 1");
    mark = mock_sound_gate_count();
    run("repeat 30 [alarm]");
    const int close = notes_on(2, mark);

    char msg[128];
    snprintf(msg, sizeof(msg), "%d notes in 30 frames at range, %d up close", far_off, close);
    TEST_ASSERT_TRUE_MESSAGE(far_off > 0, msg);
    TEST_ASSERT_TRUE_MESSAGE(close > far_off, msg);

    // And the floor holds, which is not tidiness: two 90 ms notes closer than
    // `al.min` frames apart run together into one tone and lose the tempo that
    // is the whole point of the sound.
    enemy_at(800, 805, 0);
    run("make \"au.in 1  alarm");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":al.min"), num(":au.in"), "the alarm ran past its floor");
}

// Only for something IN FRONT of you.  An alarm that sounded for an enemy
// behind would be telling you to turn round without saying which way, and the
// radar is what answers that.
void test_the_alarm_is_silent_for_what_is_behind_you(void)
{
    camera_at(800, 800, 0);
    enemy_at(800, 500, 0);          // 300 steps behind
    run("make \"au.in 1");
    int mark = mock_sound_gate_count();
    run("repeat 30 [alarm]");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(2, mark), "the alarm sounded for an enemy behind you");

    // And nothing at all when there is no enemy: a dead one is not a threat.
    enemy_at(800, 1100, 0);
    run("make \"e.alive false  make \"au.in 1");
    mark = mock_sound_gate_count();
    run("repeat 30 [alarm]");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(2, mark), "the alarm sounded for a dead enemy");
}

// The cannon and both explosions are noise, on the pair that is allowed to
// carry it.  Pitch says WHAT died: an enemy is a sharp crack and your own tank
// is a low crump, so you never have to look to know which of you it was.
void test_the_cannon_and_the_explosions_are_noise_and_differ_in_pitch(void)
{
    new_game();
    run("make \"sh.on false  make \"tk.boom 0");
    int mark = mock_sound_gate_count();
    run("fire");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(3, mark) > 0, "the cannon made no sound");

    camera_at(800, 800, 0);
    foe_at(1, 800, 1100, 180);
    mark = mock_sound_gate_count();
    run("kill.enemy");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(3, mark) > 0, "the enemy blew up in silence");
    const uint32_t theirs = last_freq_on(3);

    run("make \"lives 3  make \"tk.boom 0");
    mark = mock_sound_gate_count();
    run("hit.player");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(3, mark) > 0, "the player blew up in silence");
    TEST_ASSERT_TRUE_MESSAGE(theirs > last_freq_on(3),
                             "your death and theirs sound the same");
}

//--------------------------------------------------------------------------
// The high score table
//--------------------------------------------------------------------------

// The two lists are `scores.top` long, and a list edited to a different length
// is a silent out-of-range read rather than a visible defect.
void test_the_score_lists_are_as_long_as_the_table(void)
{
    const float top = num(":scores.top");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(top, num("count :hs.score"), "the score list is the wrong length");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(top, num("count :hs.name"), "the name list is the wrong length");
}

// STRICTLY GREATER, so a score equal to one already there ranks below it and
// whoever got there first keeps the higher line.  An empty slot holds 0, so a
// game that scored nothing cannot rank either.
void test_the_table_ranks_a_score_against_what_is_already_there(void)
{
    run("clear.scores");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("score.rank 0"), "a score of nothing ranked");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num("score.rank 1000"), "the first score did not rank first");

    run("insert.score 1 5000 \"BLAIR");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num("score.rank 5000"),
                                    "an equal score displaced the one that got there first");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num("score.rank 5001"), "a higher score did not take the top");

    // A full table with a floor above the score offered.
    run("clear.scores");
    for (int i = 1; i <= (int)num(":scores.top"); i++)
    {
        char expr[80];
        snprintf(expr, sizeof(expr), "insert.score %d %d \"NAME%d", i, (11 - i) * 2000, i);
        run(expr);
    }
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("score.rank 100"), "a score below the table ranked");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num("score.rank 30000"), "a score above the table did not");
}

// Everything below the new line slides down one and the last falls off the
// bottom, which is the half an off-by-one in the shift would silently corrupt.
void test_inserting_a_score_slides_the_rest_down(void)
{
    run("clear.scores");
    run("insert.score 1 3000 \"AAA");
    run("insert.score 2 2000 \"BBB");
    run("insert.score 3 1000 \"CCC");

    run("insert.score 2 2500 \"NEW");
    TEST_ASSERT_EQUAL_FLOAT(3000, item_of("hs.score", 1));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2500, item_of("hs.score", 2), "the new score is not where it ranked");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2000, item_of("hs.score", 3), "the table did not slide down");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1000, item_of("hs.score", 4), "the table did not slide down");
    TEST_ASSERT_EQUAL_STRING("NEW", value_to_string(eval_string("item 2 :hs.name").value));
    TEST_ASSERT_EQUAL_STRING("BBB", value_to_string(eval_string("item 3 :hs.name").value));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, num(":hs.count"), "the table did not grow");
}

// A missing file is the FIRST RUN and not an error, and the `file?` test is
// also what stops `open` creating an empty one just to read it.
void test_no_score_file_is_a_first_run_and_not_an_error(void)
{
    run("insert.score 1 1234 \"GHOST");
    run("load.scores");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":hs.count"), "a missing file left stale scores behind");
}

// Round trip, because the two halves are written apart and a format that only
// one of them agrees with is a table that silently empties itself.
void test_the_table_round_trips_through_the_file(void)
{
    run("clear.scores");
    run("insert.score 1 9000 \"BLAIR");
    run("insert.score 2 4000 \"GUNNER");
    run("save.scores");

    run("clear.scores");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":hs.count"));

    run("load.scores");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":hs.count"), "the table did not come back");
    TEST_ASSERT_EQUAL_FLOAT(9000, item_of("hs.score", 1));
    TEST_ASSERT_EQUAL_FLOAT(4000, item_of("hs.score", 2));
    TEST_ASSERT_EQUAL_STRING("BLAIR", value_to_string(eval_string("item 1 :hs.name").value));
    TEST_ASSERT_EQUAL_STRING("GUNNER", value_to_string(eval_string("item 2 :hs.name").value));
}

// A-Z, a-z and 0-9, and nothing else reaches the file.  Backspace is the one
// non-character that does anything, and it must not run off the front.
void test_the_name_field_filters_what_it_accepts(void)
{
    TEST_ASSERT_EQUAL_STRING("A", value_to_string(eval_string("next.name \"|| 97").value));
    TEST_ASSERT_EQUAL_STRING("AB", value_to_string(eval_string("next.name \"A 66").value));
    TEST_ASSERT_EQUAL_STRING("A7", value_to_string(eval_string("next.name \"A 55").value));
    // A space, a comma and an escape are all ignored.
    TEST_ASSERT_EQUAL_STRING("A", value_to_string(eval_string("next.name \"A 32").value));
    TEST_ASSERT_EQUAL_STRING("A", value_to_string(eval_string("next.name \"A 44").value));
    TEST_ASSERT_EQUAL_STRING("A", value_to_string(eval_string("next.name \"A 27").value));
    // Backspace, including off the front of an empty field.
    TEST_ASSERT_EQUAL_STRING("A", value_to_string(eval_string("next.name \"AB 8").value));
    TEST_ASSERT_EQUAL_STRING("", value_to_string(eval_string("next.name \"|| 8").value));
    // And it stops at the field width rather than running past it.
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJ",
                             value_to_string(eval_string("next.name \"ABCDEFGHIJ 75").value));
    // Enter ends the entry on a board (13) and through a host console (10).
    TEST_ASSERT_TRUE(truth("done.key? 13"));
    TEST_ASSERT_TRUE(truth("done.key? 10"));
    TEST_ASSERT_FALSE(truth("done.key? 65"));
}

//--------------------------------------------------------------------------
// The attract screen and game over
//--------------------------------------------------------------------------

// The attract screen carries the table and the keys that do anything there, and
// it waits on space.
void test_the_attract_screen_prints_the_scores_and_the_keys(void)
{
    mock_fs_create_file(SCORES_FILE, "12000 BLAIR\n5000 GUNNER\n");
    mock_device_clear_output();
    set_mock_input("xy ");            // two keys it must ignore, then space
    run("make \"leaving false  attract.screen");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "BATTLEZONE"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "HIGH SCORES"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "1."), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "2."), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "12000"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "BLAIR"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Press Space to play"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "or H for instructions"), screen);
    TEST_ASSERT_FALSE_MESSAGE(truth(":leaving"), "space left the session instead of starting a game");
}

// An empty table has to SAY so rather than leave the heading over a blank
// screen, which is what a first run and a broken load look like alike.
void test_the_attract_screen_says_so_with_no_scores(void)
{
    mock_device_clear_output();
    set_mock_input(" ");
    run("make \"leaving false  attract.screen");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(mock_device_get_output(), "No scores yet"),
                                 mock_device_get_output());
}

// H is the only other key: it puts the instructions up, any key brings the
// attract screen back, and space still starts the game from there.
void test_h_shows_the_instructions_and_comes_back(void)
{
    mock_device_clear_output();
    set_mock_input("hx ");            // H, a key to dismiss it, then space
    run("make \"leaving false  attract.screen");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Tank       1000"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Saucer     5000"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "left tread"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Press any key"), screen);
    // and it came back: the prompt is redrawn after the instructions.
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(strstr(screen, "Press any key"),
                                        "or H for instructions"), screen);
}

// THE WAY OUT.  M2's entry point could only be left by quitting the game; M3's
// runs games until the attract screen is told to stop, and if that screen took
// only Space there would be no way out at all -- which would strand a board at
// 300 MHz for the rest of the session, and that is exactly what `restore.clock`
// exists to prevent.
void test_escape_leaves_the_attract_screen_and_the_session(void)
{
    set_mock_input("\x1b");
    run("make \"leaving false  attract.screen");
    TEST_ASSERT_TRUE_MESSAGE(truth(":leaving"), "escape did not leave the attract screen");

    // And `one.game` then does nothing at all rather than starting a game
    // nobody asked for.
    run("make \"playing false");
    set_mock_input("\x1b");
    run("one.game");
    TEST_ASSERT_FALSE_MESSAGE(truth(":playing"), "the session started a game on its way out");
}

// Quitting a game is not losing one: only running out of tanks is worth a card.
void test_game_over_prints_the_final_score(void)
{
    // A full table, so the score offered does not rank and ask for a name.
    run("clear.scores");
    for (int i = 1; i <= (int)num(":scores.top"); i++)
    {
        char expr[80];
        snprintf(expr, sizeof(expr), "insert.score %d %d \"NAME%d", i, (11 - i) * 20000, i);
        run(expr);
    }

    run("make \"score 12340  make \"kills 7");
    mock_device_clear_output();
    run("show.game.over");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "GAME OVER"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "12340"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "TANKS KILLED: 7"), screen);
    TEST_ASSERT_NULL_MESSAGE(strstr(screen, "A NEW HIGH SCORE"), screen);
}

// The field is typed with the keyboard the game is already polling rather than
// through `readword`, so this drives it a character at a time.
void test_a_name_is_typed_filtered_and_ended(void)
{
    set_mock_input("bl[a;i|r] 7\n");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("BLAIR7",
                                     value_to_string(eval_string("read.name").value),
                                     "the field let something through that must not reach the file");

    // It stops at the field width rather than running past it.
    set_mock_input("abcdefghijklmno\r");
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJ", value_to_string(eval_string("read.name").value));

    // Backspace, including off the front of an empty field.
    set_mock_input("ADZ\b\bDA\n");
    TEST_ASSERT_EQUAL_STRING("ADA", value_to_string(eval_string("read.name").value));

    // An empty name is filed under the default rather than as a blank line the
    // table could not print.
    set_mock_input("\b\b\n");
    TEST_ASSERT_EQUAL_STRING("GUNNER", value_to_string(eval_string("read.name").value));
}

// A score that ranks asks for a name, files it, and SAVES -- and the file is
// what the next attract screen reads.  `read.name` is stubbed because
// `show.game.over` flushes the key ring before it reads, which is the guard
// that stops the keypress that ended the game arriving as the first letter of
// a name; the test above is what drives the real one.
void test_a_ranking_score_is_filed_and_saved(void)
{
    proc_define_from_text("to read.name\noutput \"BLAIR\nend");
    run("clear.scores  save.scores");
    run("make \"score 25000  make \"kills 12");
    mock_device_clear_output();
    run("show.game.over");

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(mock_device_get_output(), "A NEW HIGH SCORE"),
                                 mock_device_get_output());
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(25000, item_of("hs.score", 1), "the score was not inserted");
    TEST_ASSERT_EQUAL_STRING("BLAIR", value_to_string(eval_string("item 1 :hs.name").value));

    run("clear.scores  load.scores");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(25000, item_of("hs.score", 1), "the score was not saved");
}

//--------------------------------------------------------------------------
// The whole frame, with each of them in it
//--------------------------------------------------------------------------

// Whatever the pieces do separately they also do together, once per kind --
// and the last one is the case the frame budget cares about, because it is the
// one that both draws a model and carries a cracked screen.
void test_a_frame_with_each_kind_runs(void)
{
    for (int k = 1; k <= 4; k++)
    {
        char msg[64];
        new_game();
        camera_at(800, 800, 0);
        foe_at(k, 800, 1150, 180);
        run("make \"paused false  pollkeys");

        press_forward();
        for (int i = 0; i < 60; i++)
            run("play.frame");
        release_forward();

        snprintf(msg, sizeof(msg), "the workspace ran out with kind %d in the frame", k);
        TEST_ASSERT_TRUE_MESSAGE(num("atoms") > 0, msg);
        const float world = num(":world");
        snprintf(msg, sizeof(msg), "kind %d left the plain", k);
        TEST_ASSERT_TRUE_MESSAGE(num(":e.x") >= 0 && num(":e.x") < world, msg);
        TEST_ASSERT_TRUE_MESSAGE(num(":e.z") >= 0 && num(":e.z") < world, msg);
    }
}

// THE GLOBAL TABLE, AND THIS IS THE ONE A BOARD REPORTED.
//
// `MAX_GLOBAL_VARIABLES` is a hard cap shared by everything in the workspace,
// and this game is the program that pushed it: §13's L0.5 buys a 1.31x faster
// frame by putting every hot-path temporary in the flat global namespace, and
// the bill comes due here.  M2 already stood at 189 of the old 192.
//
// WHAT MAKES IT DANGEROUS IS THAT THE PEAK IS INVISIBLE AT LOAD.  Fifty of the
// names are minted the first time a procedure that uses them runs, not by a
// top-level `make` -- every `p.` temporary in the two projections, the `mt.`
// ones in the horizon, `tk.dx`/`tk.dz`/`tk.guard` in the collisions, `e.b` and
// `e.d` in the hunt, `e.left` in `set.kind`.  So the file loads at 186, the
// attract screen runs at 186, and the count only reaches its peak once a game
// has actually been played.
//
// That gap is exactly what a Pico 2 W reported: on firmware built before the
// cap went 192 -> 254 it loaded the file and showed the attract screen, then
// failed with `Out of space in spawn.enemy` -- because `init.game` is where
// the last few names are created and `spawn.enemy` is the procedure that
// happens to be running when the table fills.  Nothing before that point can
// tell you it is about to happen, which is why this test plays a game rather
// than reading the source.
//
// The margin is a budget and not a coincidence.  A player's own program, a
// startup file or a profiler loaded beside the game all come out of the same
// table, so leaving slots free is the point rather than slack to be spent.
#define GLOBAL_HEADROOM 16

void test_the_game_fits_the_global_table_with_room_to_spare(void)
{
    const int at_load = var_global_count(true);

    // Play, and play each kind, so that every procedure that mints a name has
    // run at least once -- which is the only way to reach the peak.
    for (int k = 1; k <= 4; k++)
    {
        new_game();
        camera_at(800, 800, 0);
        foe_at(k, 800, 1150, 180);
        run("make \"paused false  pollkeys");
        press(KEY_FIRE);
        press_forward();
        for (int i = 0; i < 30; i++)
            run("play.frame");
        release_forward();
        release(KEY_FIRE);
    }
    run("make \"bm.x 0  make \"bm.y 40  crack.screen  draw.cracks");
    run("engine  alarm  hit.player");

    const int peak = var_global_count(true);

    char msg[192];
    snprintf(msg, sizeof(msg),
             "the peak is the same as the load-time count (%d) -- this test did not "
             "reach the names that are minted at runtime", at_load);
    TEST_ASSERT_TRUE_MESSAGE(peak > at_load, msg);

    snprintf(msg, sizeof(msg),
             "battlezone peaks at %d globals of %d, leaving %d -- under the %d this game "
             "budgets for whatever else is in the workspace",
             peak, MAX_GLOBAL_VARIABLES, MAX_GLOBAL_VARIABLES - peak, GLOBAL_HEADROOM);
    TEST_ASSERT_TRUE_MESSAGE(peak <= MAX_GLOBAL_VARIABLES - GLOBAL_HEADROOM, msg);
}

void test_every_hot_path_temporary_is_prefixed(void)
{
    static const char *const prefixes[] = {
        "p.", "ob.", "mt.", "tk.", "mn.", "hud.",
        // M2's four: the enemy, the two shells and the radar.  The explosion
        // shares `bm.`.
        "e.", "sh.", "es.", "bm.", "rd.",
        // M3's three: the sound, the cracked glass and the score table.
        "au.", "cr.", "hs.", NULL};
    static const char *const state[] = {
        "px", "pz", "ph", "cs", "sn", "a", "b", "apx", "apy",
        "left.tread", "right.tread", "bumped", "paused", "quit",
        "frame.count", "frame.ms", "body.ms", "cpu.at", "cpu.was",
        "max.obstacles", "ox", "oz", "okind",
        "kills", "hits",
        // M3's: what a player wins and loses, the two flags the loops read,
        // and the two `clock` sets from the board's answer.
        "score", "lives", "extra.due", "cracked", "playing", "leaving",
        "fps", NULL};

    // A name DECLARED LOCAL is exempt, and that is a strengthening of this
    // test rather than a hole in it: the rule it enforces is "a temporary is
    // prefixed or it is scoped", and `local` is the scoping.  M3 is where it
    // starts to matter -- the score table, the name entry and the attract
    // screen are not the frame loop, they run once a game, and L0.5's 1.5x on
    // a local read buys nothing there against the safety of a name that
    // cannot reach the camera.
    char locals[64][64];
    int local_count = 0;

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
        if (!in_def && repl_line_starts_with_to(p)) { in_def = true; local_count = 0; continue; }
        if (in_def && repl_line_is_end(p)) { in_def = false; continue; }
        if (!in_def)
            continue;

        for (char *m = strstr(p, "local \""); m; m = strstr(m + 1, "local \""))
        {
            if (local_count >= (int)(sizeof(locals) / sizeof(locals[0])))
                break;
            size_t n = 0;
            for (char *q = m + 7; *q && (isalnum((unsigned char)*q) || *q == '.' || *q == '_') &&
                                  n + 1 < sizeof(locals[0]); q++)
                locals[local_count][n++] = *q;
            locals[local_count][n] = '\0';
            if (n > 0)
                local_count++;
        }

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
            for (int i = 0; i < local_count && !ok; i++)
                ok = strcmp(name, locals[i]) == 0;

            char msg[160];
            snprintf(msg, sizeof(msg),
                     "`make \"%s` is neither prefixed, declared local, nor named game state", name);
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
    RUN_TEST(test_no_part_of_the_gunsight_lies_along_the_horizon);
    RUN_TEST(test_the_sight_and_the_world_are_different_colours);
    RUN_TEST(test_the_field_is_on_the_plain_and_clear_of_the_start);
    RUN_TEST(test_the_frame_draws_no_more_than_max_obstacles);
    RUN_TEST(test_obstacles_behind_the_camera_do_not_crowd_out_the_one_in_front);
    RUN_TEST(test_an_obstacle_beyond_the_far_plane_is_not_drawn);
    RUN_TEST(test_the_ground_is_one_flat_line_at_the_horizon);
    RUN_TEST(test_the_horizon_cull_walks_only_the_visible_points);
    RUN_TEST(test_the_horizon_covers_the_whole_view_at_every_heading);
    RUN_TEST(test_no_horizon_segment_spans_the_whole_screen);
    RUN_TEST(test_a_pivot_moves_the_horizon_and_the_world_together);
    RUN_TEST(test_the_horizon_maps_bearing_through_the_same_tangent_as_the_world);
    RUN_TEST(test_the_horizon_ignores_the_camera_position);
    RUN_TEST(test_the_moon_appears_only_when_it_is_in_view);
    RUN_TEST(test_each_key_drives_its_own_tread);
    RUN_TEST(test_one_key_arcs_and_two_keys_pivot);
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
    RUN_TEST(test_the_enemy_draws_thirteen_edges);
    RUN_TEST(test_the_gun_points_where_the_enemy_faces);
    RUN_TEST(test_the_enemy_hull_is_a_square_that_turns);
    RUN_TEST(test_the_enemy_is_culled_at_the_near_plane);
    RUN_TEST(test_the_enemy_turns_towards_the_player);
    RUN_TEST(test_the_enemy_thinks_on_one_frame_in_three);
    RUN_TEST(test_the_enemy_closes_and_then_holds_its_range);
    RUN_TEST(test_the_enemy_cannot_drive_through_an_obstacle);
    RUN_TEST(test_a_spawn_is_re_rolled_out_of_an_obstacle);
    RUN_TEST(test_the_shell_guards_clear_half_a_step);
    RUN_TEST(test_a_shell_flies_the_heading_it_was_fired_along);
    RUN_TEST(test_only_one_shell_is_in_the_air_at_a_time);
    RUN_TEST(test_a_shell_kills_the_enemy_and_another_arrives);
    RUN_TEST(test_an_obstacle_stops_a_shell);
    RUN_TEST(test_a_shell_hits_an_obstacle_across_the_seam);
    RUN_TEST(test_a_shell_that_hits_nothing_expires);
    RUN_TEST(test_the_aim_error_is_wider_than_the_box_it_is_aimed_at);
    RUN_TEST(test_no_range_is_a_range_the_enemy_cannot_miss_from);
    RUN_TEST(test_the_enemys_shell_hits_the_player_and_pauses_the_tank);
    RUN_TEST(test_the_explosion_draws_its_fragments_and_runs_down);
    RUN_TEST(test_the_blip_is_the_enemy_in_the_camera_frame);
    RUN_TEST(test_the_radar_is_drawn_and_the_blip_is_inside_it);
    RUN_TEST(test_the_game_asks_for_the_fast_clock_and_reads_it_back);
    RUN_TEST(test_the_game_gives_the_clock_back_when_it_exits);
    RUN_TEST(test_the_exit_path_restores_the_clock);
    RUN_TEST(test_the_fast_clock_is_a_precondition);
    RUN_TEST(test_the_clock_does_not_write_the_tuning);
    RUN_TEST(test_a_refused_board_is_told_why_and_gets_no_game);
    RUN_TEST(test_the_hoisted_field_is_wrapped_about_the_camera);
    RUN_TEST(test_the_frame_moves_everything_before_it_draws_anything);
    RUN_TEST(test_the_tank_rescans_the_field_when_it_moves);
    RUN_TEST(test_the_entry_point_sets_the_game_up);
    RUN_TEST(test_the_session_asks_for_the_clock_before_any_game);
    RUN_TEST(test_a_frame_with_an_enemy_and_shells_runs);
    RUN_TEST(test_the_sequence_walks_the_ring_and_comes_back_round);
    RUN_TEST(test_the_ring_carries_all_four_kinds);
    RUN_TEST(test_every_kind_sets_a_whole_row);
    RUN_TEST(test_each_kind_is_worth_its_arcade_score);
    RUN_TEST(test_a_supertank_outclasses_a_tank);
    RUN_TEST(test_a_missile_closes_forever_and_never_fires);
    RUN_TEST(test_a_missile_kills_by_arriving_and_dies_of_it);
    RUN_TEST(test_a_missile_that_has_not_arrived_does_nothing);
    RUN_TEST(test_a_missile_draws_four_edges);
    RUN_TEST(test_a_missile_flies_at_eye_height);
    RUN_TEST(test_a_saucer_drifts_and_does_not_hunt);
    RUN_TEST(test_a_saucer_flies_over_the_obstacles);
    RUN_TEST(test_a_saucer_leaves_when_its_dwell_runs_out);
    RUN_TEST(test_a_saucer_draws_twelve_edges);
    RUN_TEST(test_a_saucers_outline_does_not_turn_with_its_heading);
    RUN_TEST(test_a_saucer_floats_above_the_horizon);
    RUN_TEST(test_the_new_models_are_culled_at_the_near_plane);
    RUN_TEST(test_the_frame_draws_the_model_that_matches_the_kind);
    RUN_TEST(test_a_kill_scores_what_the_enemy_is_worth);
    RUN_TEST(test_a_bonus_tank_arrives_on_stepping_over_the_boundary);
    RUN_TEST(test_a_hit_costs_a_tank_and_cracks_the_glass);
    RUN_TEST(test_the_pause_runs_out_into_a_new_tank_and_a_new_enemy);
    RUN_TEST(test_a_ram_spawns_one_replacement_and_not_two);
    RUN_TEST(test_the_last_tank_ends_the_game);
    RUN_TEST(test_the_shatter_is_static_until_you_respawn);
    RUN_TEST(test_every_crack_runs_in_two_strokes);
    RUN_TEST(test_the_shatter_is_drawn_only_while_it_is_cracked);
    RUN_TEST(test_the_timbres_are_set_for_every_pair);
    RUN_TEST(test_the_engine_is_two_pairs_that_beat);
    RUN_TEST(test_the_engine_pitch_follows_the_treads);
    RUN_TEST(test_the_alarm_closes_its_gap_as_the_enemy_closes);
    RUN_TEST(test_the_alarm_is_silent_for_what_is_behind_you);
    RUN_TEST(test_the_cannon_and_the_explosions_are_noise_and_differ_in_pitch);
    RUN_TEST(test_the_score_lists_are_as_long_as_the_table);
    RUN_TEST(test_the_table_ranks_a_score_against_what_is_already_there);
    RUN_TEST(test_inserting_a_score_slides_the_rest_down);
    RUN_TEST(test_no_score_file_is_a_first_run_and_not_an_error);
    RUN_TEST(test_the_table_round_trips_through_the_file);
    RUN_TEST(test_the_name_field_filters_what_it_accepts);
    RUN_TEST(test_the_attract_screen_prints_the_scores_and_the_keys);
    RUN_TEST(test_the_attract_screen_says_so_with_no_scores);
    RUN_TEST(test_h_shows_the_instructions_and_comes_back);
    RUN_TEST(test_escape_leaves_the_attract_screen_and_the_session);
    RUN_TEST(test_game_over_prints_the_final_score);
    RUN_TEST(test_a_name_is_typed_filtered_and_ended);
    RUN_TEST(test_a_ranking_score_is_filed_and_saved);
    RUN_TEST(test_a_frame_with_each_kind_runs);

    RUN_TEST(test_the_game_fits_the_global_table_with_room_to_spare);
    RUN_TEST(test_every_hot_path_temporary_is_prefixed);
    return UNITY_END();
}
