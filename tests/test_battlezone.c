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
//    * Nothing is culled for being CLOSE (B59).  An object goes only when its
//      bounding circle is outside the view cone, and a vertex that arrives
//      inside the near plane is floored rather than dropped -- because the
//      thing that swings a projection through infinity is a z at or behind the
//      eye, and a floor is enough to stop that.
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

// The enemy is the hull's twelve edges, the turret's twelve and the barrel's
// eight.  It was thirteen -- a hull and one line for a gun -- and §8.2 refused
// the turret at M0 "with a price attached", because the shape that ships should
// be the shape that was measured.  M3's board run left 18 ms of peak headroom
// and M4 is spending some of it.
#define EDGES_ENEMY 32

// A missile and a saucer are the same solid: a four-point ring with an apex
// either side of it.  Twelve edges over five divides, one draw procedure.
#define EDGES_SPINDLE 12

// A shell is a cube now rather than a four-pixel dash.
#define EDGES_SHELL 12

// The player's own explosion is five short strokes on a growing radius, drawn
// on the glass because the player is inside the thing that blew up.
#define FRAGS_BOOM 5

// An enemy's explosion is the enemy: the hull's twelve edges, the turret's
// twelve and the barrel's eight, thrown apart in the world.  The wreck costs
// exactly what the live tank cost, on frames where the live tank is not drawn.
#define EDGES_WRECK 32

// A missile and a saucer have no hull, turret and gun -- they are one solid
// with no parts -- so they come apart into three of themselves: three spindles
// at twelve edges each, over five divides each, which is cheaper than the
// tank's wreck at both ends.
#define EDGES_SHARDS (3 * EDGES_SPINDLE)

// PicoCalc key codes, as the game names them to `keydown?`/`keyhit?`.  One key
// per tread per direction, laid out like the cabinet's two sticks: 1 and Q on
// the left of the keyboard drive the left tread, 0 and P on the right drive the
// right one.
#define KEY_LFWD    49 // '1'
#define KEY_LBACK  113 // 'q'
#define KEY_RFWD    48 // '0'
#define KEY_RBACK  112 // 'p'
#define KEY_FIRE    32 // space -- fires in BOTH schemes
#define KEY_FIRE2   93 // ']'   -- the tread scheme's right-hand alternative
#define KEY_PAUSE  122 // 'z'
#define KEY_QUIT   177 // escape
// The arrow scheme, which `C` on the attract screen selects.
#define KEY_UP     181
#define KEY_DOWN   182
#define KEY_LEFT   180
#define KEY_RIGHT  183

// Load a whole Logo file, defining its procedures and running its top-level
// tuning `make`s.  Procedure definitions are not handled by the bare evaluator,
// so we buffer them and hand them to proc_define_from_text the way `load` does.
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
    // Two steering schemes ship and the player picks (`C` on the attract
    // screen).  The arrows are the default; the suite below drives the tread
    // keys, so it selects that scheme here and the arrow tests turn it back on
    // for themselves.  `init.game` does not touch it -- it is session state
    // rather than per-game state, which is what makes pinning it here sound.
    run_string("make \"arrows false");
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

// The two distances a spawn may use -- the whole of `e.spawn` and half of it
// -- read off the camera-relative offsets in the metric the game uses.  It is
// a range and not a value because the bearing is random and the offsets are
// Manhattan: half the ring is 310 steps and the far ring is at most 620 * root
// two.  What it is really asserting is "out on the plain and not in your lap",
// which used to be written `> e.range` and cannot be any more: M5's near
// distance is 310 steps against a tank's 400-step stand-off, and that is the
// cabinet's behaviour rather than a regression.
static void assert_out_at_a_spawning_distance(const char *what)
{
    const float d = fabsf(num(":e.dx")) + fabsf(num(":e.dz"));
    const float spawn = num(":e.spawn");
    char msg[160];
    snprintf(msg, sizeof(msg), "%s: %g steps away, and the two spawning distances are %g and %g",
             what, (double)d, (double)(spawn * 0.5f), (double)spawn);
    TEST_ASSERT_TRUE_MESSAGE(d >= spawn * 0.5f - 1.0f && d <= spawn * 1.5f, msg);
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

// THERE IS NO CAP FROM M6 ON.  `max.obstacles` held the drawn-object count at
// three, which was the honest number against the STOCK frame slope of 7.26-8.11
// ms an object.  This game requires 300 MHz, where design section 12.3.1b
// measured `25.32 + 3.223 n` and put twelve objects inside 15 fps -- and the
// whole table plus the enemy is nine.  The cap outlived its measurement by two
// milestones.
void test_the_whole_field_is_drawable_with_no_cap(void)
{
    TEST_ASSERT_EQUAL_FLOAT(8, num(":ob.count"));

    // Eight cubes in a ring dead ahead, all of them in view and none of them
    // inside the near plane: every one draws its twelve edges.
    run("make \"ox [800 800 800 800]  make \"ox se :ox [800 800 800 800]");
    run("make \"oz [1000 1100 1200 1300]  make \"oz se :oz [1400 1500 1550 1580]");
    run("make \"okind [1 1 1 1]  make \"okind se :okind [1 1 1 1]");
    camera_at(800, 900, 0);
    mock_device_clear_graphics();
    run("draw.field");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8 * 12, mock_device_line_count(),
                                  "something is still capping the field");
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

// Design section 9: NOTHING IS CULLED FOR BEING CLOSE any more.  The near plane
// dropped an object the moment a column crossed it; what replaces it is a floor
// under the projection, so a column that crosses is projected from `zmin`
// instead of being a reason to throw the object away.
void test_nothing_is_culled_for_being_close(void)
{
    camera_at(0, 0, 0);
    const float near = num(":near");
    const float half = num(":half");
    const float zmin = num(":zmin");

    // Straddling the plane: the centre in front of it, the near face behind.
    // This is the case the old cull dropped and the one B59 was reported for.
    TEST_ASSERT_TRUE(project("project.box", 0, near + half - 1.0f));

    // And wholly inside it.  A cube 30 steps away fills the view, which is what
    // a cube 30 steps away should do; `coll.r` is what stops you getting there.
    TEST_ASSERT_TRUE(project("project.box", 0, near * 0.5f));

    // The floor is the only thing left, and it is inside your own tank.
    TEST_ASSERT_FALSE(project("project.box", 0, zmin - 1.0f));
    TEST_ASSERT_FALSE(project("project.box", 0, -400));
}

// The other half of it: a column that came inside the plane is projected from
// `zmin`, so every screen coordinate stays finite and stays on the side of the
// screen the column is really on.  A negative z is what throws a line across
// the whole view, and this is what makes one unreachable.
void test_a_column_inside_the_near_plane_is_floored(void)
{
    camera_at(0, 0, 0);
    const float zmin = num(":zmin");
    const float k = num(":k");
    const float half = num(":half");

    // Centre at 30, so the near pair of columns sits at 10 -- inside the floor.
    TEST_ASSERT_TRUE(project("project.box", 0, 30.0f));

    // Nothing is projected from nearer than the floor, so nothing is further
    // out than `k` * half / zmin, and the two columns on each side keep their
    // signs.  Without the floor the near pair would divide by 10 and land at
    // twice this.
    const float bound = k * half / zmin;
    for (int i = 1; i <= 4; i++)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "column %d projected from inside the floor", i);
        TEST_ASSERT_TRUE_MESSAGE(fabsf(item_of("cx", i)) <= bound + 0.5f, msg);
    }
    TEST_ASSERT_TRUE_MESSAGE(item_of("cx", 1) > 0 && item_of("cx", 4) < 0,
                             "a floored column changed sides");
}

// The cone test's margin is WRITTEN 48 AND NOT NAMED, because the file peaks 16
// slots from the global ceiling and a third name would spend the last one
// (design section 9.1).  So this is what stops it drifting away from the object
// it was cut for: it has to cover the widest thing in the game -- a supertank
// measured to the end of its barrel -- times root one plus vw squared, because
// the test measures across the cone's face and not across x.  Under that and
// the cone culls something with a vertex still on the glass, which is B59
// coming back in a shape no cull test would name.
void test_the_view_cone_margin_covers_every_object(void)
{
    const float vw = num(":vw");
    const float face = sqrtf(1.0f + vw * vw);
    camera_at(800, 800, 0);

    // An obstacle is the corner of a `half` square, at any camera heading.
    TEST_ASSERT_TRUE_MESSAGE(48.0f >= num(":half") * sqrtf(2.0f) * face,
                             "the view cone can cull a visible obstacle");

    // A tank and a supertank reach to the end of the barrel, which is `e.bl`
    // 2.2 hulls out and so is over the hull's own diagonal.  These are the two
    // that 48 was cut for.
    for (int kind = 1; kind <= 3; kind += 2)
    {
        foe_at(kind, 800, 1150, 0);
        char msg[96];
        snprintf(msg, sizeof(msg), "the view cone can cull a visible enemy of kind %d", kind);
        TEST_ASSERT_TRUE_MESSAGE(48.0f >= (num(":e.bl") + num(":e.bw")) * face, msg);
        TEST_ASSERT_TRUE_MESSAGE(num(":e.bl") + num(":e.bw") >= num(":e.hw") * sqrtf(2.0f), msg);
    }

    // The other two have no barrel and `project.missile` and `project.saucer`
    // never read one: a dart reaches its nose and a saucer reaches its rim.
    TEST_ASSERT_TRUE_MESSAGE(48.0f >= num(":ms.ln") * face,
                             "the view cone can cull a visible missile");
    TEST_ASSERT_TRUE_MESSAGE(num(":ms.ln") >= num(":ms.fin"),
                             "a missile is wider than it is long: its reach is not `ms.ln`");
    TEST_ASSERT_TRUE_MESSAGE(48.0f >= num(":sc.r") * face,
                             "the view cone can cull a visible saucer");
}

// The cull that is left, and the only one: an object goes when its bounding
// circle is outside the view cone.  `vw` is the cone's half-slope, so a centre
// at x = vw * z sits on the edge of the glass with half the object inside it.
void test_only_the_view_cone_culls(void)
{
    camera_at(0, 0, 0);
    const float vw = num(":vw");

    // On the edge of the view at 300 steps: half of it is on the screen.
    TEST_ASSERT_TRUE_MESSAGE(project("project.box", vw * 300.0f, 300.0f),
                             "an obstacle on the edge of the view was culled");
    TEST_ASSERT_TRUE_MESSAGE(project("project.box", 0 - vw * 300.0f, 300.0f),
                             "an obstacle on the other edge of the view was culled");

    // Out at right angles to it, where nothing of it can reach the glass.
    TEST_ASSERT_FALSE(project("project.box", 900.0f, 300.0f));
    TEST_ASSERT_FALSE(project("project.box", -900.0f, 300.0f));
}

// B59, and it is what the driving seat sees: a cube you are scraping past
// vanishes whole while two of its columns are still on the glass.
//
// The old cull dropped an object when ANY of its four columns came inside the
// plane, and section 9 defended that with the collision radius: `coll.r` 90 is
// `near` + half*sqrt2, so no corner could reach the plane.  THAT ARGUMENT ONLY
// HOLDS DEAD AHEAD.  The guard bounds the obstacle's DISTANCE; the cull
// compares its camera-frame z, which is d*cos(bearing), and the cosine is what
// the two arguments do not share.  At 45 degrees of heading a cube 91 steps out
// on one axis and 30 on the other has its centre at z = 85.6 and its near
// column at 57.3 -- inside the plane, with the cube a third of the way across
// the view.
void test_a_cube_beside_the_tank_is_still_drawn(void)
{
    camera_at(800, 800, 45);
    TEST_ASSERT_TRUE_MESSAGE(project("project.box", 91, 30),
                             "a cube in the middle of the view was culled by one corner");

    // Two of the four columns are inside the viewport, which is the half of the
    // claim a boolean cannot make.
    int on_screen = 0;
    for (int i = 1; i <= 4; i++)
    {
        if (fabsf(item_of("cx", i)) <= 160.0f)
        {
            on_screen++;
        }
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, on_screen,
                                  "the placement stopped straddling the edge of the view");

    // And the tank is allowed to be there: 91 steps clears the 90-step guard on
    // the x axis, so this is a position a player drives into rather than a
    // geometry the collision test already forbids.
    TEST_ASSERT_TRUE(91.0f > num(":coll.r"));

    // All twelve edges, not the two the on-screen columns share.  `window` is
    // what makes that the cheap answer: a line that leaves the glass is clipped
    // per pixel, so the off-screen columns cost iterations and nothing else.
    mock_device_clear_graphics();
    run("draw.box");
    TEST_ASSERT_EQUAL_INT_MESSAGE(EDGES_CUBE, mock_device_line_count(),
                                  "an edge with a vertex off the glass was dropped");
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

// Design section 9.2: what used to bound edge length was the near plane, and
// since B59 it is the floor.  An edge costs 0.35-0.98 us a step and projected
// size goes as k*h/z, so this is the number that decides what a close object
// costs to draw.  `zmin` 20 puts a cube at three screens where `near` 60 put it
// at one -- affordable because only one object can be that close, and because
// most of what it draws is off the glass.
void test_the_floor_bounds_how_big_a_cube_can_get(void)
{
    camera_at(0, 0, 0);
    const float near = num(":near");
    const float half = num(":half");
    const float zmin = num(":zmin");
    const float k = num(":k");
    const float boxh = num(":boxh");

    // Where the old near cull put the closest legal cube: still about a screen.
    TEST_ASSERT_TRUE(project("project.box", 0, near + half + 0.5f));
    const float tall = item_of("cy2", 1) - item_of("cy1", 1);
    TEST_ASSERT_TRUE_MESSAGE(tall < 240.0f, "a cube at the near plane is taller than the viewport");
    TEST_ASSERT_TRUE_MESSAGE(tall > 100.0f, "a cube at the near plane is too small to read");

    // And the floor is the ceiling on it now: nothing is ever projected from
    // nearer than `zmin`, whatever the cull does or does not drop.
    TEST_ASSERT_TRUE(project("project.box", 0, 30.0f));
    const float worst = item_of("cy2", 3) - item_of("cy1", 3);
    TEST_ASSERT_TRUE_MESSAGE(worst <= k * boxh / zmin + 0.5f,
                             "a column was projected from inside the floor");
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
    run("make \"e.alive false");
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
//
// IT HOLDS FOR BOTH FORMS.  The locked sight swings its teeth in toward the
// middle, which is exactly the direction the horizon is, so it is the form
// most able to break this and the one worth naming in the loop.
void test_no_part_of_the_gunsight_lies_along_the_horizon(void)
{
    const float hz = num(":hz");
    static const char *const forms[] = {"sight.free", "sight.lock"};

    for (int f = 0; f < 2; f++)
    {
        mock_device_clear_graphics();
        run(forms[f]);
        TEST_ASSERT_EQUAL_INT(EDGES_SIGHT, mock_device_line_count());

        for (int i = 0; i < mock_device_line_count(); i++)
        {
            const MockLine *l = mock_device_get_line(i);
            const float lo = l->y1 < l->y2 ? l->y1 : l->y2;
            const float hi = l->y1 < l->y2 ? l->y2 : l->y1;

            // Not lying along the horizon, and not crossing it either: a stroke
            // through `hz` is a stroke through the target and through the shell.
            char msg[80];
            snprintf(msg, sizeof(msg), "a %s segment sits on the horizon", forms[f]);
            TEST_ASSERT_FALSE_MESSAGE(lo <= hz && hz <= hi, msg);
        }
    }
}

// A drawn object is worth nothing if it is drawn in the wrong colour, and the
// cabinet's glass is the reason there are two.  The tube is green and the band
// of plastic across the top of it is red, so the sight standing down in the
// view is the world's green while it is resting -- and the red, which the
// cabinet spends on the radar, is what this display has left to say "locked"
// with.
void test_the_resting_sight_is_the_world_and_the_locked_one_is_not(void)
{
    TEST_ASSERT_TRUE(num(":sight.colour") != num(":world.colour"));

    mock_device_clear_graphics();
    run("sight.free");
    for (int i = 0; i < mock_device_line_count(); i++)
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)num(":world.colour"), mock_device_get_line(i)->colour,
                                      "the resting sight is not the world's green");

    mock_device_clear_graphics();
    run("sight.lock");
    for (int i = 0; i < mock_device_line_count(); i++)
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)num(":sight.colour"), mock_device_get_line(i)->colour,
                                      "the locked sight is not the overlay's red");
}

// The ROM picks between `vg_reticle1` and `vg_reticle2` at $50f3, and the
// second is the first with its four teeth swung round to 45 degrees so that
// they point INTO the middle.  That is the whole difference and it is the
// message: the shape says the gun is on something before you fire.
//
// The bars do not move -- a sight whose frame jumped would read as two sights
// rather than as one changing -- so this checks that the four horizontals are
// identical between the forms and that the teeth are not.
void test_the_locked_sight_turns_its_teeth_toward_the_middle(void)
{
    const float hz = num(":hz");

    mock_device_clear_graphics();
    run("sight.lock");
    TEST_ASSERT_EQUAL_INT(EDGES_SIGHT, mock_device_line_count());

    int teeth = 0;
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        const bool horizontal = fabsf(l->y1 - l->y2) < 0.01f;
        const bool vertical = fabsf(l->x1 - l->x2) < 0.01f;
        if (horizontal || vertical)
            continue;
        teeth++;

        // A tooth runs from the end of a bar back in toward the aiming point,
        // in both axes at once: nearer the centreline AND nearer the eye line
        // at the inner end than at the outer one.
        const bool a_is_outer = fabsf(l->x1) > fabsf(l->x2);
        const float xo = a_is_outer ? l->x1 : l->x2, xi = a_is_outer ? l->x2 : l->x1;
        const float yo = a_is_outer ? l->y1 : l->y2, yi = a_is_outer ? l->y2 : l->y1;
        TEST_ASSERT_TRUE_MESSAGE(fabsf(xi) < fabsf(xo), "a tooth does not turn in");
        TEST_ASSERT_TRUE_MESSAGE(fabsf(yi - hz) < fabsf(yo - hz),
                                 "a tooth does not reach back toward the eye line");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, fabsf(xo - xi), fabsf(yo - yi),
                                         "a tooth is not at 45 degrees");
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, teeth, "the locked sight is not four teeth and four straights");

    // The resting sight has none of them: its teeth stand out from the bars,
    // parallel to the stalks.
    mock_device_clear_graphics();
    run("sight.free");
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        TEST_ASSERT_TRUE_MESSAGE(fabsf(l->x1 - l->x2) < 0.01f || fabsf(l->y1 - l->y2) < 0.01f,
                                 "the resting sight has a diagonal in it");
    }
}

// Two strokes are the same stroke if they join the same two points, whichever
// end the pen started at.
static bool same_segment(float x1, float y1, float x2, float y2, const MockLine *m)
{
    const bool fwd = fabsf(x1 - m->x1) < 0.01f && fabsf(y1 - m->y1) < 0.01f &&
                     fabsf(x2 - m->x2) < 0.01f && fabsf(y2 - m->y2) < 0.01f;
    const bool rev = fabsf(x1 - m->x2) < 0.01f && fabsf(y1 - m->y2) < 0.01f &&
                     fabsf(x2 - m->x1) < 0.01f && fabsf(y2 - m->y1) < 0.01f;
    return fwd || rev;
}

// Both forms are mirrored about the aiming point, which is the property a sight
// cannot lose: one that is not symmetric says the gun is somewhere it is not.
void test_both_sights_are_symmetric_about_the_aiming_point(void)
{
    static const char *const forms[] = {"sight.free", "sight.lock"};
    const float hz = num(":hz");

    for (int f = 0; f < 2; f++)
    {
        mock_device_clear_graphics();
        run(forms[f]);
        const int n = mock_device_line_count();

        // Every stroke has its mirror image in x, and its mirror in y about hz.
        for (int i = 0; i < n; i++)
        {
            const MockLine *l = mock_device_get_line(i);
            // Endpoint ORDER is not part of the shape: a bar drawn left to
            // right is its own mirror image, drawn in the same direction.
            bool mx = false, my = false;
            for (int j = 0; j < n; j++)
            {
                const MockLine *m = mock_device_get_line(j);
                mx = mx || same_segment(-l->x1, l->y1, -l->x2, l->y2, m);
                my = my || same_segment(l->x1, 2 * hz - l->y1, l->x2, 2 * hz - l->y2, m);
            }
            char msg[96];
            snprintf(msg, sizeof(msg), "%s is not symmetric about its aiming point", forms[f]);
            TEST_ASSERT_TRUE_MESSAGE(mx && my, msg);
        }
    }
}

//--------------------------------------------------------------------------
// What the sight locks on
//--------------------------------------------------------------------------

// The ROM's test is a bearing and nothing else: $50e7 compares the angle to the
// enemy against 2/256ths of a turn -- 2.8125 degrees -- and the range does not
// enter it.  So the same bearing locks at any distance, and this checks it at
// two that differ by more than three times.
void test_the_sight_locks_on_a_bearing_and_not_a_range(void)
{
    // tan 2.8125 degrees is 0.0491, so at z the sight holds out to 0.0491 z.
    // The plain wraps at `half.world`, so both distances stay inside it or the
    // enemy comes round behind the camera.
    static const float z[] = {200.0f, 700.0f};

    for (int i = 0; i < 2; i++)
    {
        char msg[96];
        camera_at(800, 800, 0);

        foe_at(1, 800 + z[i] * 0.03f, 800 + z[i], 180);
        snprintf(msg, sizeof(msg), "1.7 degrees off at %g steps did not lock the sight", z[i]);
        TEST_ASSERT_TRUE_MESSAGE(truth("in.sights"), msg);

        foe_at(1, 800 + z[i] * 0.07f, 800 + z[i], 180);
        snprintf(msg, sizeof(msg), "4 degrees off at %g steps locked the sight", z[i]);
        TEST_ASSERT_FALSE_MESSAGE(truth("in.sights"), msg);
    }
}

// And what it locks is always in the clear air the sight frames.  At k = 260
// the half-angle is 12.8 pixels of screen against a gap of 30, so a sight that
// has locked has never covered the thing it locked on to.
void test_what_the_sight_locks_is_inside_its_gap(void)
{
    camera_at(800, 800, 0);
    foe_at(1, 800 + 34.0f, 800 + 700.0f, 180);
    TEST_ASSERT_TRUE_MESSAGE(truth("in.sights"), "the edge of the lock is not locked");

    const float x = num(":e.xc") * num(":k") / num(":e.zc");
    TEST_ASSERT_TRUE_MESSAGE(fabsf(x) < 30.0f, "a locked target is under the sight's own frame");
}

// It needs no "is it in front of you" test, because the threshold it compares
// against goes negative behind you and no absolute value is less than a
// negative number.  The enemy 2.8 degrees off your BACK does not light the
// sight, and that is worth a test because it is an absence in the code.
void test_the_sight_does_not_lock_on_what_is_behind_you(void)
{
    camera_at(800, 800, 0);
    foe_at(1, 800 + 10.0f, 800 - 400.0f, 0);
    TEST_ASSERT_TRUE_MESSAGE(0 > num(":e.zc"), "the enemy is not behind the camera");
    TEST_ASSERT_FALSE_MESSAGE(truth("in.sights"), "the sight locked on something behind you");
}

// A wreck is not a target.  `e.alive` goes false the moment the shell lands,
// and a sight still locked on the explosion would be telling the player to
// spend a round on it.
void test_a_dead_enemy_does_not_lock_the_sight(void)
{
    camera_at(800, 800, 0);
    foe_at(1, 800, 800 + 500.0f, 180);
    TEST_ASSERT_TRUE(truth("in.sights"));
    run("make \"e.alive false");
    TEST_ASSERT_FALSE_MESSAGE(truth("in.sights"), "a dead enemy still locks the sight");
}

// And the frame draws whichever form the bearing asks for, which is the only
// thing that joins the two halves of this section.
void test_the_frame_draws_the_form_the_bearing_asks_for(void)
{
    camera_at(800, 800, 0);
    foe_at(1, 800, 800 + 500.0f, 180);
    mock_device_clear_graphics();
    run("gunsight");
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)num(":sight.colour"), mock_device_get_line(0)->colour,
                                  "an enemy dead ahead did not draw the locked sight");

    foe_at(1, 800 + 400.0f, 800 + 500.0f, 180);
    mock_device_clear_graphics();
    run("gunsight");
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)num(":world.colour"), mock_device_get_line(0)->colour,
                                  "an enemy 39 degrees off drew the locked sight");
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

// AND NO FAR CULL EITHER, which is the one place M6 moves AWAY from the cabinet
// rather than toward it: the ROM culls at $7aff, 769 steps.  It was asked for,
// and the two plains are the same size, so a far plane at 769 was hiding the
// back quarter of a world that fits.  What survives is the NEAR cull, which is
// not a budget at all -- design section 9, this file has no clipper.
//
// 750 steps rather than anything grander because the plain wraps at 1,600: the
// furthest anything can BE is 800 on an axis, and asking for 1,400 puts the
// obstacle 200 steps behind you the short way round.
void test_nothing_in_view_is_culled_by_distance(void)
{
    run("make \"ox [800 800 800 800]  make \"ox se :ox [800 800 800 800]");
    run("make \"oz [850 850 850 850]  make \"oz se :oz [850 850 850 850]");
    camera_at(800, 100, 0);
    mock_device_clear_graphics();
    run("draw.field");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > 0,
                             "an obstacle 750 steps out -- past the old far plane -- was culled");

    // And nothing culls by nearness either (B59): an obstacle 20 steps ahead is
    // filling the view, not missing from it.
    camera_at(800, 830, 0);
    mock_device_clear_graphics();
    run("draw.field");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > 0,
                             "an obstacle 20 steps ahead was culled for being close");

    // The view cone is what bites.  The same eight obstacles with the camera
    // turned to put them all out at right angles draw nothing at all.
    camera_at(800, 100, 90);
    mock_device_clear_graphics();
    run("draw.field");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(),
                                  "an obstacle square out to the side was drawn");
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

    // An object at the same bearing: the enemy's own centre, projected.  Taken
    // from `e.xc`/`e.zc` rather than off any model, because the midpoint of two
    // projected corners is NOT the projection of the midpoint -- they sit at
    // different ranges and the divide is per point, which is worth 0.2 steps
    // even on a barrel two steps wide.
    foe_at(1, 800, 1100, 180);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    const float obj_before = k * num(":e.xc") / num(":e.zc");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, obj_before);

    const float turn = num(":turn.rate");
    camera_at(800, 800, turn);
    mock_device_clear_graphics();
    run("horizon");
    const float peak_after = horizon_vertex(4);
    foe_at(1, 800, 1100, 180);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    const float obj_after = k * num(":e.xc") / num(":e.zc");

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
// The volcano
//==========================================================================

// Five particle slots, five fields each: countdown, sideways velocity, upward
// velocity, screen offset, screen y.
#define VP_SLOTS 5
#define VP_FIELDS 5

// The crater notch is the horizon table's 17th point, and the vent bearing is
// read off the table rather than written down twice: the points are `mn.step`
// degrees apart starting at azimuth 0, so point 17 is at 16 * 9 = 144.
static int vent_index(void)
{
    return (int)(144.0f / num(":mn.step")) + 1;
}

// Field `f` (1-5) of slot `s` (1-5) of the particle list.
static float vp_field(int s, int f)
{
    return item_of("vp", (s - 1) * VP_FIELDS + f);
}

// Face the vent, so that its screen x is `k * tan 0` = 0 and every dot's x is
// the sideways drift alone.
static void face_the_volcano(void)
{
    // The mock's hardware random source is a constant (`mock_random` returns
    // 42), so a 1-in-4 roll would never come up and the vent would never
    // throw.  `rerandom` puts the seeded generator in front of it, which is
    // what every other test in this file that needs a roll does.
    run("rerandom");
    run("make \"ph 144");
}

// The sparks come out of the CRATER and not out of the ridge beside it: the
// vent bearing and the height a new one starts at are both the horizon table's
// own notch, so this re-derives them from `mtn` rather than repeating the two
// numbers the game writes down.
void test_the_vent_is_the_craters_own_notch(void)
{
    const int v = vent_index();
    TEST_ASSERT_EQUAL_INT_MESSAGE(17, v, "azimuth 144 is not a point in the table");

    const float notch = item_of("mtn", v);
    TEST_ASSERT_TRUE_MESSAGE(item_of("mtn", v - 1) > notch && item_of("mtn", v + 1) > notch,
                             "the vent bearing is not the notch between the two lips");

    // Empty every slot, then run frames until the 1-in-4 roll fills one, and
    // read the height it started at back out.
    for (int s = 1; s <= VP_SLOTS; s++)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), ".setitem %d :vp 0", (s - 1) * VP_FIELDS + 1);
        run(expr);
    }
    face_the_volcano();

    int born = 0;
    for (int i = 0; i < 200 && !born; i++)
    {
        run("volcano");
        for (int s = 1; s <= VP_SLOTS; s++)
            if (vp_field(s, 1) == 15.0f)   // the countdown, at its full value
                born = s;
    }
    TEST_ASSERT_TRUE_MESSAGE(born, "no spark was thrown in 200 frames");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":hz") + notch, vp_field(born, 5),
                                    "a new spark does not start at the crater notch");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, vp_field(born, 4),
                                    "a new spark does not start at the vent");
    TEST_ASSERT_TRUE_MESSAGE(fabsf(vp_field(born, 2)) >= 0.5f && fabsf(vp_field(born, 2)) <= 2.0f,
                             "the sideways velocity is outside the ROM's converted range");
    TEST_ASSERT_TRUE_MESSAGE(vp_field(born, 3) >= 6.0f && vp_field(born, 3) <= 14.0f,
                             "the upward velocity is outside the ROM's converted range");
}

// What the cabinet shows: a handful of sparks over the crater, none of them
// below the ground and none of them anywhere but the crater.
void test_the_volcano_throws_sparks_out_of_its_crater(void)
{
    const float hz = num(":hz");
    const float vent = hz + item_of("mtn", vent_index());
    face_the_volcano();

    int seen = 0;
    float highest = hz;
    float widest = 0;
    for (int i = 0; i < 150; i++)
    {
        mock_device_clear_graphics();
        run("volcano");
        const int n = mock_device_dot_count();
        TEST_ASSERT_TRUE_MESSAGE(n <= VP_SLOTS, "more than five sparks were in the air");
        seen += n;
        for (int j = 0; j < n; j++)
        {
            const MockDot *d = mock_device_get_dot(j);
            TEST_ASSERT_TRUE_MESSAGE(d->y >= hz, "a spark was drawn below the ground");
            if (d->y > highest) highest = d->y;
            if (fabsf(d->x) > widest) widest = fabsf(d->x);
        }
    }

    TEST_ASSERT_TRUE_MESSAGE(seen > 0, "the volcano threw nothing in 150 frames");
    TEST_ASSERT_TRUE_MESSAGE(highest > vent, "no spark ever cleared the crater");
    // The throw is bounded at both ends by things outside itself: it has to
    // clear the lips to be seen, and it has to stay inside the 120 steps of
    // sky §6 leaves above the horizon.
    TEST_ASSERT_TRUE_MESSAGE(highest > hz + item_of("mtn", vent_index() + 1),
                             "no spark ever cleared the lip beside the vent");
    TEST_ASSERT_TRUE_MESSAGE(highest < hz + 120.0f, "a spark went off the top of the band");
    TEST_ASSERT_TRUE_MESSAGE(widest > 0.0f, "every spark went straight up");
    // Half the crater mouth: a spark that drifts further than this falls down
    // the OUTSIDE of the cone and reads as coming off the ridge.
    TEST_ASSERT_TRUE_MESSAGE(widest < 41.0f, "a spark drifted clear of the crater");
}

// The cull covers the update as well as the draw (design section 16.13), which
// is the one place this differs from the ROM -- so a volcano behind you costs
// nothing at all, and this pins BOTH halves of that: no dots, and no state
// moved either.
void test_a_volcano_behind_you_costs_nothing(void)
{
    face_the_volcano();
    for (int i = 0; i < 40; i++)
        run("volcano");

    float before[VP_SLOTS * VP_FIELDS];
    for (int i = 0; i < VP_SLOTS * VP_FIELDS; i++)
        before[i] = item_of("vp", i + 1);

    run("make \"ph 324");   // 144 + 180, the far side of the plain
    mock_device_clear_graphics();
    for (int i = 0; i < 40; i++)
        run("volcano");

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_dot_count(),
                                  "the volcano was drawn from behind the camera");
    for (int i = 0; i < VP_SLOTS * VP_FIELDS; i++)
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(before[i], item_of("vp", i + 1),
                                        "a spark moved while the volcano was out of view");
}

// One spark, stepped by hand, so that the arc is checked rather than sampled:
// it rises, it turns over, it comes down, and the frame it reaches the ground
// is the frame it is killed on and not drawn on.
void test_a_spark_arcs_over_and_dies_on_the_ground(void)
{
    const float hz = num(":hz");

    // `vp.b`, `vp.t` and `vp.x` are `volcano`'s locals; setting them as globals
    // here lets one slot be stepped on its own, with the other four empty.
    for (int s = 2; s <= VP_SLOTS; s++)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), ".setitem %d :vp 0", (s - 1) * VP_FIELDS + 1);
        run(expr);
    }
    char expr[160];
    snprintf(expr, sizeof(expr),
             ".setitem 1 :vp 15  .setitem 2 :vp 1  .setitem 3 :vp 10 "
             ".setitem 4 :vp 0  .setitem 5 :vp %g", hz + 56.0f);
    run(expr);
    run("make \"vp.b 1  make \"vp.x 0");

    float last = hz + 56.0f;
    bool rose = false, fell = false;
    int frames = 0;
    for (int i = 0; i < 30; i++)
    {
        if (vp_field(1, 1) < 1.0f)
            break;
        snprintf(expr, sizeof(expr), "make \"vp.t %g", vp_field(1, 1) - 1.0f);
        run(expr);
        mock_device_clear_graphics();
        run("vp.fly");
        frames++;

        if (vp_field(1, 1) < 1.0f)
        {
            TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_dot_count(),
                                          "a spark was drawn on the frame it landed");
            break;
        }
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, mock_device_dot_count(), "the spark was not drawn");

        const float y = vp_field(1, 5);
        if (y > last) rose = true;
        if (rose && y < last) fell = true;
        last = y;
        // The sideways velocity does not change -- the ROM's does not either.
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)frames, vp_field(1, 4),
                                        "the drift is not one step a frame");
    }

    TEST_ASSERT_TRUE_MESSAGE(rose && fell, "the spark did not arc");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, vp_field(1, 1), "the spark did not die on the ground");
    TEST_ASSERT_TRUE_MESSAGE(frames < 15, "the spark outlived the ROM's countdown");
}

// A spark leaves the vent bright and dims as it goes: two levels where the
// cabinet ramps eight, picked by the countdown and not by the height.
void test_a_spark_cools_as_it_flies(void)
{
    face_the_volcano();

    int bright = 0, dim = 0;
    const float hot = num(":moon.colour");
    for (int i = 0; i < 150; i++)
    {
        mock_device_clear_graphics();
        run("volcano");
        for (int j = 0; j < mock_device_dot_count(); j++)
        {
            const float c = (float)mock_device_get_dot(j)->colour;
            if (c == hot) bright++;
            else if (c == 165.0f) dim++;   // the palette's neutral grey
            else TEST_FAIL_MESSAGE("a spark was drawn in a colour that is neither");
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(bright > 0, "no spark was drawn at full intensity");
    TEST_ASSERT_TRUE_MESSAGE(dim > 0, "no spark ever dimmed");

    // And the pen is handed back the way every other backdrop piece hands it
    // back, or the obstacles drawn after it come out the wrong colour.
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":world.colour"), num("pencolor"),
                                    "the volcano left the pen its own colour");
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

// (The paragraph that stood here argued `coll.r` out of the near plane, and B59
// retired the argument along with the cull -- see design section 9.  The test it
// belonged to went before that; the comment is gone with it.)

// TWO STEERING SCHEMES, AND THE PLAYER PICKS.  M2 replaced the arrows with one
// key per tread and said that retired the question of which feels better.  It
// did not -- it answered it for one player, and a board asked for the choice
// back.  So both ship.
//
// The arrows are a forward intent and a turn intent summed into the pair, which
// is a steering wheel wearing a tank's controls.  What this checks is the sum,
// because the sum is the whole of the scheme.
void test_the_arrows_drive_and_steer(void)
{
    run("make \"arrows true");

    press(KEY_UP);
    run("pollkeys  treads");
    release(KEY_UP);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":left.tread"), "up did not drive");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":right.tread"), "up did not drive");

    press(KEY_DOWN);
    run("pollkeys  treads");
    release(KEY_DOWN);
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":left.tread"));
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":right.tread"));

    // Turning right is a pivot: the left tread forward and the right one back.
    // The sign is the physical one -- a tank whose RIGHT tread runs forward
    // pivots LEFT -- so a clockwise turn needs left > right.
    press(KEY_RIGHT);
    run("pollkeys  treads");
    release(KEY_RIGHT);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":left.tread"), "right did not pivot right");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-1, num(":right.tread"), "right did not pivot right");

    press(KEY_LEFT);
    run("pollkeys  treads");
    release(KEY_LEFT);
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":left.tread"));
    TEST_ASSERT_EQUAL_FLOAT(1, num(":right.tread"));
}

// THE CLAMP IS THE WHOLE OF THE SCHEME.  Forward and right sums to left 2,
// right 0, and a tread has three states; without the clamp the tank would drive
// at DOUBLE SPEED whenever it turned.  `clamp1` existed for this, went with the
// arrows at M2, and comes back inline.
void test_the_arrow_sum_clamps_to_one_tread(void)
{
    run("make \"arrows true");

    press(KEY_UP);
    press(KEY_RIGHT);
    run("pollkeys  treads");
    release(KEY_UP);
    release(KEY_RIGHT);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":left.tread"),
                                    "forward and right drove the left tread past its stop");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":right.tread"),
                                    "forward and right is a one-tread arc");

    // And the same going backwards, which is the half a one-sided clamp misses.
    press(KEY_DOWN);
    press(KEY_LEFT);
    run("pollkeys  treads");
    release(KEY_DOWN);
    release(KEY_LEFT);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-1, num(":left.tread"), "the clamp is one-sided");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":right.tread"), "the clamp is one-sided");
}

// Each scheme answers only its own keys, or a player would be driving with both
// at once and the tread keys would fight the arrows.
void test_each_scheme_ignores_the_other_scheme_s_keys(void)
{
    run("make \"arrows true");
    press(KEY_LFWD);
    press(KEY_RFWD);
    run("pollkeys  treads");
    release(KEY_LFWD);
    release(KEY_RFWD);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":left.tread"), "a tread key drove the arrow scheme");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":right.tread"), "a tread key drove the arrow scheme");

    run("make \"arrows false");
    press(KEY_UP);
    run("pollkeys  treads");
    release(KEY_UP);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":left.tread"), "an arrow drove the tread scheme");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":right.tread"), "an arrow drove the tread scheme");
}

// SPACE, Z AND ESC DO NOT MOVE WITH THE SCHEME.  A control you press without
// thinking should not depend on a menu you set once, so all three are checked
// under both schemes rather than under the default one.
void test_fire_pause_and_quit_are_the_same_in_both_schemes(void)
{
    for (int arrows = 0; arrows <= 1; arrows++)
    {
        char msg[80];
        run(arrows ? "make \"arrows true" : "make \"arrows false");
        new_game();
        run("make \"paused false  make \"quit false  make \"sh.on false  pollkeys");

        press(KEY_FIRE);
        run("play.frame");
        release(KEY_FIRE);
        snprintf(msg, sizeof(msg), "space did not fire with arrows %d", arrows);
        TEST_ASSERT_TRUE_MESSAGE(truth(":sh.on"), msg);

        press(KEY_PAUSE);
        run("play.frame");
        release(KEY_PAUSE);
        snprintf(msg, sizeof(msg), "z did not pause with arrows %d", arrows);
        TEST_ASSERT_TRUE_MESSAGE(truth(":paused"), msg);

        press(KEY_PAUSE);
        run("play.frame");
        release(KEY_PAUSE);
        TEST_ASSERT_FALSE_MESSAGE(truth(":paused"), "z did not unpause");

        press(KEY_QUIT);
        run("play.frame");
        release(KEY_QUIT);
        snprintf(msg, sizeof(msg), "escape did not quit with arrows %d", arrows);
        TEST_ASSERT_TRUE_MESSAGE(truth(":quit"), msg);
        TEST_ASSERT_FALSE_MESSAGE(truth(":playing"), msg);
    }

    // `]` still fires as well, which is the key a right hand on 0/P can reach.
    run("make \"arrows false");
    new_game();
    run("make \"sh.on false  pollkeys");
    press(KEY_FIRE2);
    run("play.frame");
    release(KEY_FIRE2);
    TEST_ASSERT_TRUE_MESSAGE(truth(":sh.on"), "] no longer fires");
}

// The choice is made on the attract screen and it is SESSION state: `init.game`
// must not reset it, or every new game would throw the setting away.
void test_the_attract_screen_picks_the_steering(void)
{
    run("make \"arrows false");
    mock_device_clear_output();
    set_mock_input("c ");                 // toggle, then space to play
    run("make \"leaving false  attract.screen");
    TEST_ASSERT_TRUE_MESSAGE(truth(":arrows"), "C did not change the steering");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "TREADS"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "ARROWS"), screen);

    // Upper case too, and it toggles back.
    set_mock_input("C ");
    run("attract.screen");
    TEST_ASSERT_FALSE_MESSAGE(truth(":arrows"), "shifted C did not change the steering");

    // And a new game keeps it.
    run("make \"arrows true");
    new_game();
    TEST_ASSERT_TRUE_MESSAGE(truth(":arrows"), "starting a game threw the steering away");
}

// `coll.r` IS THE ARCADE'S STAND-OFF AND NOT THE NEAR PLANE'S.  The test that
// stood here asserted `coll.r` >= `near` + half*sqrt2 -- the invariant B59
// retired, because the guard bounds a distance and the cull compared a
// camera-frame z.  With the cull gone the number went back to what it is for:
// how close a tank may bring its centre to a cube's.  The cabinet's is $0480,
// 28.8 steps at 40 raw units to the step ($6923, `CheckObstUnitColl`), against
// obstacles smaller than this file draws; 20 + 14 is the same idea in this
// file's units and 40 leaves a few steps of daylight on top of it.  90 left
// FIFTY-SIX, and a board reported it as the collision being wrong.
void test_you_can_drive_up_to_an_obstacle_and_not_up_to_two_of_them(void)
{
    const float half = num(":half");
    const float guard = num(":coll.r");

    // Daylight between the hulls when you stop, head-on: the cube's face is
    // `half` from its centre and the tank is `ehalf` from yours.
    const float daylight = guard - half - num(":ehalf");
    char msg[128];
    snprintf(msg, sizeof(msg), "a tank stops %g steps short of a cube's face", (double)daylight);
    TEST_ASSERT_TRUE_MESSAGE(daylight > 0.0f && daylight < 20.0f, msg);
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

    // 150 frames and not 120: the tank drives 4.64 steps a frame and `coll.r`
    // is 40 rather than 90, so it now has fifty more steps to cover before
    // anything stops it.
    press_forward();
    for (int i = 0; i < 150; i++)
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

// THE SHIPPED GAME DOES NOT SHOW ITS DEVELOPMENT READOUT.  `hud.every` is both
// the averaging period and the switch, and the file it loads sets it to zero:
// a player is not reading milliseconds, and a game that prints them on rows 25
// to 27 is a harness with a title screen.  This is the load-time value, so it
// is checked against the file rather than against a variable a test has set.
void test_the_readout_is_off_in_a_shipped_game(void)
{
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":hud.every"),
                                    "the frame readout is on in the shipped file");
}

// And the frame does not pay for a readout it is not showing: `play.frame`
// asks before it tallies.  With the switch off, fifteen frames leave the
// accumulator where they found it -- which is also what says the rows stay
// blank, since `draw.hud` is only ever reached through `hud.tally`.
void test_the_frame_does_not_tally_while_the_readout_is_off(void)
{
    run("make \"px 800  make \"pz 800  make \"ph 0  make \"paused false");
    run("make \"quit false  make \"frame.count 0  pollkeys");
    run("make \"hud.every 0  make \"hud.n 0");

    for (int i = 0; i < 15; i++)
        run("play.frame");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":hud.n"),
                                    "the frame tallied for a readout nobody is showing");
}

// `D` on the attract screen is the way back to the instrument, and it is a
// toggle rather than a switch that only goes on: a board left showing the
// readout has to be able to put it away without restarting the session.  The
// arithmetic form is `15 - :hud.every`, so check both directions.
void test_d_toggles_the_readout_both_ways(void)
{
    run("make \"hud.every 0");
    run("make \"hud.every 15 - :hud.every");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(15, num(":hud.every"), "D did not turn the readout on");

    run("make \"hud.every 15 - :hud.every");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":hud.every"), "D did not turn the readout off again");
}

// A figure that changes fifteen times a second cannot be read off a screen by
// somebody driving, so the readout is averaged over `hud.every` frames. Check
// that it fires on that period and resets, rather than every frame or never.
// The switch is turned on here, because the shipped default is off.
void test_the_readout_is_averaged_over_a_second(void)
{
    run("make \"px 800  make \"pz 800  make \"ph 0  make \"paused false");
    run("make \"quit false  make \"frame.count 0  pollkeys");
    run("make \"hud.every 15");
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
    const float k = num(":k");

    // Broadside, facing east: the muzzle end of the barrel is `e.bl` further
    // east than the base, and the base straddles the tank's centre.
    camera_at(800, 800, 0);
    foe_at(1, 800, 1100, 90);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    run("barrel.columns");
    const float reach = num(":e.bl");
    // Columns 1 and 2 are the base pair, 3 and 4 the muzzle pair; each pair
    // straddles the axis by the barrel's half-width, so the midpoints are what
    // carry the direction.
    const float base = 0.5f * (item_of("cx", 1) + item_of("cx", 2));
    const float muzzle = 0.5f * (item_of("cx", 3) + item_of("cx", 4));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, base);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, reach * k / 300.0f, muzzle,
                                     "the barrel does not point where the tank faces");

    // And the other way round when it faces the other way.
    foe_at(1, 800, 1100, 270);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    run("barrel.columns");
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -reach * k / 300.0f,
                             0.5f * (item_of("cx", 3) + item_of("cx", 4)));

    // Facing straight away, the barrel foreshortens to nothing rather than
    // swinging sideways -- which is the same claim from the third direction,
    // and the one that caught M0's transposed half-offset.  The hull is square,
    // so that error shows there only as a rotation; here it is a barrel
    // pointing 90 degrees wrong.
    foe_at(1, 800, 1100, 0);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    run("barrel.columns");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, 0.0f,
                                     0.5f * (item_of("cx", 3) + item_of("cx", 4)),
                                     "the barrel swung sideways instead of foreshortening");
}

// The turret sits on the hull and inside its footprint, which is what lets it
// skip the near-plane test: every column of it is further from the near plane
// than the hull column it sits under.  If that stopped being true the turret
// would be the thing that divides by nearly zero.
void test_the_turret_sits_on_the_hull_and_inside_it(void)
{
    camera_at(800, 800, 0);
    foe_at(1, 800, 1100, 0);
    TEST_ASSERT_TRUE(truth("project.enemy"));
    const float hull_x = item_of("cx", 1), hull_top = item_of("cy2", 1);

    run("turret.columns");
    // The same WORLD height, so nearly the same screen height -- but not
    // exactly, because the turret's corner is narrower and therefore at a
    // slightly different range, and the divide is per column.  A pixel of
    // tolerance is the honest assertion; zero would be asserting a coincidence.
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, hull_top, item_of("cy1", 1),
                                     "the turret does not stand on the hull");
    TEST_ASSERT_TRUE_MESSAGE(item_of("cy2", 1) > item_of("cy1", 1),
                             "the turret has no height");
    TEST_ASSERT_TRUE_MESSAGE(fabsf(item_of("cx", 1)) < fabsf(hull_x),
                             "the turret is wider than the hull it skips the cull behind");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.tw") < num(":e.hw"),
                             "the turret is not narrower than the hull");
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
// LINES ALL OVER, which is what a board saw and what no test in this file could.
//
// Every perspective divide here is `k / z` for a hoisted range z.  The three
// parts M4 added wrote theirs INLINE -- `:k / :p.za - :p.px` -- and `/` binds
// tighter than `-`, so that is `(k / za) - px`.  At 300 steps it is
// 0.867 - 8 = -7.13 instead of 0.839: `p.iz` comes out negative and an order of
// magnitude too big, and every vertex of the turret, the barrel and the shell
// cube lands somewhere arbitrary.
//
// THE EDGE COUNT IS BLIND TO IT.  Twelve edges drawn through nonsense is still
// twelve edges, so every model test passed while the picture was wrecked.  What
// this checks instead is WHERE the vertices land: every column of every part of
// a tank has to sit near the tank.  The bound is generous on purpose -- it is
// catching a projection that has come apart, not tuning a silhouette.
static void assert_part_lands_on_the_object(const char *part, float centre, float bound)
{
    for (int i = 1; i <= 4; i++)
    {
        char msg[160];
        const float x = item_of("cx", i);
        const float lo = item_of("cy1", i), hi = item_of("cy2", i);
        snprintf(msg, sizeof(msg), "%s column %d is at x %.1f, %.1f from the object's centre",
                 part, i, (double)x, (double)fabsf(x - centre));
        TEST_ASSERT_TRUE_MESSAGE(fabsf(x - centre) < bound, msg);
        snprintf(msg, sizeof(msg), "%s column %d spans y %.1f to %.1f", part, i,
                 (double)lo, (double)hi);
        TEST_ASSERT_TRUE_MESSAGE(hi > lo, msg);
        TEST_ASSERT_TRUE_MESSAGE(fabsf(lo) < 400.0f && fabsf(hi) < 400.0f, msg);
    }
}

void test_every_part_of_a_tank_lands_on_the_tank(void)
{
    const float k = num(":k");
    // Broadside, so the barrel reaches sideways and is at its widest on screen.
    camera_at(800, 800, 0);
    foe_at(1, 800, 1200, 90);
    TEST_ASSERT_TRUE(truth("project.enemy"));

    const float centre = k * num(":e.xc") / num(":e.zc");
    // The barrel reaches `e.bl` from the centre, so allow twice that and some.
    const float bound = 3.0f * k * num(":e.bl") / num(":e.zc");

    assert_part_lands_on_the_object("the hull", centre, bound);
    run("turret.columns");
    assert_part_lands_on_the_object("the turret", centre, bound);
    run("barrel.columns");
    assert_part_lands_on_the_object("the barrel", centre, bound);
}

// The same failure, in the object it was most visible on: a shell cube at 300
// steps is a few pixels across, so a divide that has come apart throws its
// twelve edges across the whole screen.
// M5's dart, and the two things it has to be.  It has to LAND ON THE SHELL --
// §16.8.1's regression, where an inline `k / z` that lost its parentheses threw
// every vertex of the old cube across the screen and twelve edges of nonsense
// still counted as twelve edges -- and its point has to be WHERE THE SHELL IS
// GOING, which is the whole reason it stopped being a cube.
void test_a_shell_is_a_dart_pointing_where_it_flies(void)
{
    const float k = num(":k");
    camera_at(800, 800, 0);
    run("make \"sh.on true  make \"sh.dx 0  make \"sh.dz 300");

    // Flying away from you: the point is on the axis, between the base square's
    // top and bottom, and the base straddles it.
    run("make \"sh.vx 0  make \"sh.vz :sh.step  draw.shell");
    const float across = k * num(":sh.r") / 300.0f;
    TEST_ASSERT_TRUE_MESSAGE(across > 1.0f, "the test placed the shell too far to see");

    const float left = item_of("cx", 1), right = item_of("cx", 2);
    char msg[160];
    snprintf(msg, sizeof(msg), "the base square is at x %.1f and %.1f, and the shell is at 0",
             (double)left, (double)right);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(left) < 6.0f * across && fabsf(right) < 6.0f * across, msg);
    TEST_ASSERT_TRUE_MESSAGE((left > 0) != (right > 0), msg);
    TEST_ASSERT_TRUE_MESSAGE(item_of("cy2", 1) > item_of("cy1", 1), "a base corner has no height");

    snprintf(msg, sizeof(msg), "the point is at (%.1f, %.1f) and the shell's axis is at (0, %.1f)",
             (double)num(":apx"), (double)num(":apy"), (double)num(":hz"));
    TEST_ASSERT_TRUE_MESSAGE(fabsf(num(":apx")) < 6.0f * across, msg);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, num(":hz"), num(":apy"), msg);

    // Flying across your view: the point leads the base, on the side it is
    // going, and it is the same dart the other way round.
    run("make \"sh.vx :sh.step  make \"sh.vz 0  draw.shell");
    snprintf(msg, sizeof(msg), "flying right, the point is at x %.1f and the base at %.1f and %.1f",
             (double)num(":apx"), (double)item_of("cx", 1), (double)item_of("cx", 2));
    TEST_ASSERT_TRUE_MESSAGE(num(":apx") > item_of("cx", 1) && num(":apx") > item_of("cx", 2), msg);

    run("make \"sh.vx 0 - :sh.step  make \"sh.vz 0  draw.shell");
    snprintf(msg, sizeof(msg), "flying left, the point is at x %.1f and the base at %.1f and %.1f",
             (double)num(":apx"), (double)item_of("cx", 1), (double)item_of("cx", 2));
    TEST_ASSERT_TRUE_MESSAGE(num(":apx") < item_of("cx", 1) && num(":apx") < item_of("cx", 2), msg);
}

// Eight edges in two strokes, and the count is the cheap half of the change: a
// square pyramid is four edges and one divide less than the cube it replaces,
// so two shells in the air is sixteen edges rather than twenty-four.
// TEMPORARY DIAGNOSTIC -- remove.
static void diag_render(const char *label, float cx0, float cy0, int half)
{
    // Rasterise the recorded lines into an ASCII grid centred on (cx0, cy0).
    int n = mock_device_line_count();
    int w = half * 2 + 1;
    static char grid[81][161];
    for (int r = 0; r < w && r < 81; r++) { for (int c = 0; c < w && c < 161; c++) grid[r][c] = '.'; }
    for (int i = 0; i < n; i++) {
        const MockLine *L = mock_device_get_line(i);
        float dx = L->x2 - L->x1, dy = L->y2 - L->y1;
        int steps = (int)(fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy)) * 4 + 1;
        for (int t = 0; t <= steps; t++) {
            float x = L->x1 + dx * t / steps, y = L->y1 + dy * t / steps;
            int c = (int)lrintf(x - cx0) + half, r = half - (int)lrintf(y - cy0);
            if (r >= 0 && r < w && c >= 0 && c < w && r < 81 && c < 161) grid[r][c] = '#';
        }
    }
    printf("DIAGPIC %s (%d lines)\n", label, n);
    for (int r = 0; r < w && r < 81; r++) { printf("DIAGPIC |"); for (int c = 0; c < w && c < 161; c++) putchar(grid[r][c]); printf("|\n"); }
}

void test_ZZDIAG(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("fire");
    printf("DIAG fired vx=%g vz=%g on=%s\n", (double)num(":sh.vx"), (double)num(":sh.vz"),
           truth(":sh.on") ? "true" : "false");
    for (int f = 1; f <= 8; f++) {
        run("step.shell");
        if (!truth(":sh.on")) { printf("DIAG f%d shell gone\n", f); break; }
        run("make \"p.zc :sh.dz * :cs + :sh.dx * :sn");
        float zc = num(":p.zc");
        mock_device_clear_graphics();
        run("draw.shell");
        printf("DIAG f%d dz=%7.1f zc=%7.1f lines=%d cx=(%8.2f,%8.2f) cy1=%8.2f cy2=%8.2f apex=(%8.2f,%8.2f)\n",
               f, (double)num(":sh.dz"), (double)zc, mock_device_line_count(),
               (double)item_of("cx",1), (double)item_of("cx",2),
               (double)item_of("cy1",1), (double)item_of("cy2",1),
               (double)num(":apx"), (double)num(":apy"));
        if (f == 2 || f == 4) {
            char lbl[64]; snprintf(lbl, sizeof(lbl), "player shell at %g steps", (double)num(":sh.dz"));
            diag_render(lbl, 0, num(":hz"), 16);
            run("gunsight");
            snprintf(lbl, sizeof(lbl), "player shell at %g steps WITH GUNSIGHT", (double)num(":sh.dz"));
            diag_render(lbl, 0, num(":hz"), 16);
        }
    }

    // A round coming straight at you, dead on, walked in by hand.
    printf("DIAG ---- incoming round, dead on ----\n");
    run("make \"es.on true  make \"es.y 0  make \"es.vx 0  make \"es.vz 0 - :sh.step");
    for (int d = 400; d >= 80; d -= 80) {
        char expr[96];
        snprintf(expr, sizeof(expr), "make \"es.dx 0  make \"es.dz %d", d);
        run(expr);
        mock_device_clear_graphics();
        run("draw.eshell");
        char lbl[64]; snprintf(lbl, sizeof(lbl), "incoming round at %d steps", d);
        diag_render(lbl, 0, num(":hz"), 16);
    }

}

void test_a_shell_draws_eight_edges(void)
{
    camera_at(800, 800, 0);
    run("make \"sh.on true  make \"sh.dx 0  make \"sh.dz 300");
    run("make \"sh.vx 0  make \"sh.vz :sh.step");
    mock_device_clear_graphics();
    run("draw.shell");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, mock_device_line_count(), "a dart is not eight edges");

    // And the enemy's is the same dart at the height its barrel left.
    run("make \"es.on true  make \"es.dx 0  make \"es.dz 300  make \"es.y 20");
    run("make \"es.vx 0  make \"es.vz 0 - :sh.step");
    mock_device_clear_graphics();
    run("draw.eshell");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, mock_device_line_count(), "the enemy's dart is not eight edges");
    TEST_ASSERT_TRUE_MESSAGE(num(":apy") > num(":hz"), "the enemy's shell flies at the eye");
}

// B59 costs the enemy more than it costs an obstacle, and `e.range` is why:
// the cabinet's tank does not stand off, it drives into your face and stops at
// 38 steps.  The near cull dropped it at about 80 -- so the tank that killed
// you was not on the screen when it did -- and this is the half of the bug a
// board would have found before an obstacle's.
void test_the_enemy_is_drawn_right_into_your_face(void)
{
    const float near = num(":near");
    camera_at(800, 800, 0);

    enemy_at(800, 800 + near + 40.0f, 0);
    TEST_ASSERT_TRUE_MESSAGE(truth("project.enemy"), "an enemy in clear view was culled");

    enemy_at(800, 800 + num(":e.range"), 0);
    TEST_ASSERT_TRUE_MESSAGE(truth("project.enemy"),
                             "an enemy at its own stand-off was culled");

    enemy_at(800, 700, 0);
    TEST_ASSERT_FALSE_MESSAGE(truth("project.enemy"), "an enemy behind the camera was drawn");

    // And out at right angles, where the view cone is the thing that drops it.
    enemy_at(800 + 900.0f, 800 + 300.0f, 0);
    TEST_ASSERT_FALSE_MESSAGE(truth("project.enemy"), "an enemy off the glass was drawn");
}

// The hunt turns towards the player and stops turning when it is looking at
// them, which is the whole of its aim.
void test_the_enemy_turns_towards_the_player(void)
{
    camera_at(800, 800, 0);

    // `hunt` turns toward the heading it DECIDED on, and it only decides when
    // `e.mvc` runs out -- so a single call has to be given an expired counter
    // or it is still steering toward last decision's heading.  `e.rage` 0 is
    // the seventeen-second override, which takes the coin flip out of it and
    // makes the choice "head at the player" every time.
    //
    // Ahead and to the player's right, facing back down -z at the player: it
    // has to turn clockwise to look at them.
    enemy_at(900, 1100, 180);
    run("make \"e.cool 99  make \"e.rage 0  make \"e.mvc 1  hunt");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.t") > 0, "the enemy turned away from the player");

    enemy_at(700, 1100, 180);
    run("make \"e.cool 99  make \"e.rage 0  make \"e.mvc 1  hunt");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.t") < 0, "the enemy turned away from the player");

    enemy_at(800, 1100, 180);
    run("make \"e.cool 99  make \"e.rage 0  make \"e.mvc 1  hunt");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":e.t"), "the enemy kept turning past the player");
}

// IT ACTS EVERY FRAME AND DECIDES ON A COUNTER, which is the ROM's shape and
// the reverse of M5's.  `UpdateTank` turns, drives and tries to shoot on every
// single frame; what it does on `move_counter` is choose a new DESIRED heading.
// M5 ran the whole hunt on one frame in three, which confused a decision with
// an action -- design section 19.3's argument was about the decision.
void test_the_enemy_acts_every_frame_and_decides_on_a_counter(void)
{
    camera_at(800, 800, 0);
    foe_at(1, 800, 1100, 180);
    run("make \"e.rage 0  make \"e.cool 99  make \"e.mvc 4");

    // Four frames of hunting spend the counter and only the fourth re-aims.
    const float aim0 = num(":e.aimh");
    run("hunt  hunt  hunt");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(aim0, num(":e.aimh"),
                                    "the enemy re-decided before its counter ran out");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.mvc"), "the counter did not run down");

    run("make \"e.aimh 0  hunt");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.aimh") != 0.0f, "the counter ran out and nothing was decided");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, num(":e.mvc"), "the counter was not rewound");

    // And the acting half is unconditional: a turn intent every frame.
    run("make \"e.aimh 90  make \"e.h 0  hunt");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.t") != 0.0f, "the enemy did not turn on a non-deciding frame");
}

// It closes to `e.range` and then holds, or it drives into your face and the
// near cull makes it vanish.
void test_the_enemy_closes_and_then_holds_its_range(void)
{
    camera_at(800, 800, 0);
    const float range = num(":e.range");

    enemy_at(800, 800 + range + 200.0f, 180);
    run("make \"e.cool 99  make \"e.rage 0  make \"e.mvc 1  hunt");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.f") > 0, "the enemy would not close");

    enemy_at(800, 800 + range - 20.0f, 180);
    run("make \"e.cool 99  make \"e.rage 0  make \"e.mvc 1  hunt");
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
    enemy_at(800, 1000 - num(":coll.r") + 2.0f, 0);
    const float before = num(":e.z");
    run("make \"e.f 1  make \"e.t 0  move.enemy");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(before, num(":e.z"), "the enemy drove into a cube");

    // Facing away from it, it is free to go.
    enemy_at(800, 1000 - num(":coll.r") + 2.0f, 180);
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
        run("make \"tk.guard :coll.r");
        if (truth("blocked?"))
            stuck++;
    }
    char msg[96];
    snprintf(msg, sizeof(msg), "%d of 60 spawns landed inside an obstacle", stuck);
    TEST_ASSERT_TRUE_MESSAGE(stuck <= 3, msg);
}


// THE SHOT AT THE EDGE OF THE WINDOW, and it is the whole of a board's "the
// tank's shots seem to be always accurate -- in the arcade, driving towards a
// tank, the shots miss to one side or the other at first".
//
// `hunt` fires when the bearing error is under 2.8 degrees and `enemy.fires`
// sends the shell down the tank's own heading, so the worst shot the enemy can
// take is thrown d*tan(2.8) sideways: 7 steps at 150 and 29 at 600.  Against
// the guard that was here -- `tk.hit` 30 square, 42 across a corner -- every
// one of those was a hit, at every range the plain has, and no amount of
// driving changed it.
//
// The corridor is HALF `ehalf` -- seven steps, which is the cabinet's own kill
// radius: `TestProjCollU` ($5fb2) compares a true distance against 224 to 320
// raw units, 5.6 to 8 steps at 40 raw to the step.  So the shot stops being
// certain at 7/tan(2.8) = 143 steps.
//
// SEVEN AND NOT FOURTEEN, and the first cut of this test said fourteen.  It
// read the ROM's window as 1.4 degrees -- `TryShootPlayer` at $65c5 takes an
// angle difference of 0 or 1 -- and doubled the box to keep the ratio against
// this file's 2.8.  Both sides of that difference are the HIGH BYTE of a 9-bit
// facing at 1.406 degrees a unit (`RotateLeft` steps the low byte by $80), so
// one unit of difference is a true error of up to 2.81 degrees: the cabinet's
// window is this file's, and the box that goes with it is the cabinet's seven.
// The board saw the difference as "I still get hit by shells that look like
// they should miss", which is what a round passing 13 steps to one side looks
// like from inside a tank you cannot see.
//
// It asserts both outcomes and never a ratio, and it drives `hunt` rather than
// assuming the window, so a change to either number has to face this test.
void test_the_enemys_worst_shot_misses_at_range_and_kills_up_close(void)
{
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");

    const int ranges[] = {60, 100, 400, 620};
    for (int r = 0; r < 4; r++)
    {
        camera_at(800, 800, 0);
        // Dead ahead, aimed 2.7 degrees off you.
        foe_at(1, 800, (float)(800 + ranges[r]), 182.7f);
        run("make \"e.cool 0  make \"e.rage 0  make \"e.mvc 9  make \"tk.boom 0  hunt");
        TEST_ASSERT_TRUE_MESSAGE(truth(":e.fire"),
                                 "the fire window no longer admits a 2.7 degree error");

        // `lives` is reset every round: `hit.player` refuses to spend a life
        // the player does not have (B58).
        run("make \"hits 0  make \"lives 3  make \"tk.boom 0  make \"es.on false  enemy.fires");
        for (int i = 0; i < 40 && truth(":es.on"); i++)
            run("step.eshell");

        char msg[128];
        const bool hit = num(":hits") > 0;
        snprintf(msg, sizeof(msg), "the worst shot at %d steps %s", ranges[r],
                 hit ? "hit" : "missed");
        if (ranges[r] < 143)
            TEST_ASSERT_TRUE_MESSAGE(hit, msg);
        else
            TEST_ASSERT_FALSE_MESSAGE(hit, msg);
    }
}

// The same defect seen from the side, with no aiming in it at all: a shell on a
// heading that takes it past you is a shell that missed.  `tk.hit` is 30 and
// square, so a round going by 25 steps to your right killed you; what decides
// now is the distance from the shell's LINE, and the square is only the reject
// in front of it.
void test_a_shell_that_goes_by_you_is_a_shell_that_missed(void)
{
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");

    // Straight down the z axis, 25 steps to one side of the tank: inside the
    // old square in both axes, and a miss.  Ten would be a miss too; 25 is kept
    // because it is what the square used to call a hit.
    camera_at(800, 800, 0);
    foe_at(1, 825, 1100, 180);
    run("make \"hits 0  make \"lives 3  make \"tk.boom 0  make \"es.on false  enemy.fires");
    for (int i = 0; i < 40 && truth(":es.on"); i++)
        run("step.eshell");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":hits"),
                                    "a shell that flew past killed the player");

    // And five steps to one side is a hit: seven is what the cabinet kills
    // inside, and a shell that close is coming down the middle of the hull.
    camera_at(800, 800, 0);
    foe_at(1, 805, 1100, 180);
    run("make \"hits 0  make \"lives 3  make \"tk.boom 0  make \"es.on false  enemy.fires");
    for (int i = 0; i < 40 && truth(":es.on"); i++)
        run("step.eshell");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":hits"), "a shell down the hull missed");
}

// Your own gun is the same test with the guards the other way round, and it was
// the same size of wrong: `e.hit` is 34, so a round passing 25 steps beside a
// 14-step tank killed it.  The cabinet runs BOTH directions through the one
// routine, so what kills a tank is the same seven steps that kill you -- a
// round through the edge of a tank's silhouette goes past it, which is why the
// cabinet's long shots miss and closing is worth doing.
void test_your_shell_kills_what_the_gunsight_covers(void)
{
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");

    camera_at(800, 800, 0);
    foe_at(1, 825, 1100, 180);
    run("make \"score 0  make \"sh.on false  make \"sh.cool 0  make \"tk.boom 0  fire");
    for (int i = 0; i < 40 && truth(":sh.on"); i++)
        run("step.shell");
    TEST_ASSERT_TRUE_MESSAGE(truth(":e.alive"), "a shell that went by killed the tank");

    camera_at(800, 800, 0);
    foe_at(1, 805, 1100, 180);
    run("make \"score 0  make \"sh.on false  make \"sh.cool 0  make \"tk.boom 0  fire");
    for (int i = 0; i < 40 && truth(":sh.on"); i++)
        run("step.shell");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.alive"), "a shell down the hull missed");
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
    assert_out_at_a_spawning_distance("the replacement");
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

// IT FIRES ALONG ITS OWN HEADING AND NOTHING ELSE.  There was a deliberate
// lateral miss here -- a random offset in steps at the target, converted to an
// angle for the range -- which was M3's answer to a tank that stood where it
// could not miss.  The cabinet has no such thing: `TryShootPlayer` builds the
// velocity straight from `enemy_facing`, and the miss comes from somewhere
// else entirely (see the test below).
void test_the_enemy_fires_exactly_along_its_heading(void)
{
    run("rerandom");
    for (int k = 1; k <= 4; k++)
    {
        char expr[64], msg[128];
        snprintf(expr, sizeof(expr), "make \"e.kind %d  set.kind", k);
        run(expr);
        if (!truth(":e.gun"))
            continue;

        // Forty shots from a tank pointed 30 degrees off the z axis: every one
        // of them leaves on exactly that heading, with no spread at all.
        run("make \"e.h 30  make \"e.ec cos 30  make \"e.es sin 30");
        run("make \"e.x 800  make \"e.z 200  make \"e.dx 0  make \"e.dz -600");
        for (int shot = 0; shot < 40; shot++)
        {
            run("make \"es.on false  make \"e.cool 0  enemy.fires");
            const float bearing = atan2f(num(":es.vx"), num(":es.vz")) * 57.2958f;
            snprintf(msg, sizeof(msg), "kind %d threw a shot %g degrees off its heading",
                     k, (double)(bearing - 30.0));
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 30.0f, bearing, msg);
        }
    }
}

// AND THE MISS IS LEAD, NOT SPREAD.  This is the cabinet's whole gunnery model
// and it is better than the random offset it replaces: a shell takes frames to
// arrive and NOTHING IN BATTLEZONE LEADS ITS TARGET, so a player who is driving
// is missed and a player who has stopped to aim is hit.  M2's and M3's versions
// of this test asked for both outcomes from a stationary player, which the
// cabinet will not give -- it hits that player every time, at every range, and
// that is the point.
void test_a_still_player_is_hit_and_a_moving_one_is_missed(void)
{
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");

    const int ranges[] = {60, 100, 150, 200, 283, 400};
    for (int r = 0; r < 6; r++)
    {
        // Standing still, dead ahead, and lined up: it cannot miss.
        camera_at(800, 800, 0);
        foe_at(1, 800, (float)(800 + ranges[r]), 180);
        // `lives` is reset every round: `hit.player` refuses to spend a life
        // the player does not have (B58), and this loop kills them six times.
        run("make \"hits 0  make \"lives 3  make \"tk.boom 0  make \"es.on false  enemy.fires");
        for (int i = 0; i < 40 && truth(":es.on"); i++)
            run("step.eshell");

        char msg[160];
        snprintf(msg, sizeof(msg), "a still player at %d steps was missed", ranges[r]);
        TEST_ASSERT_TRUE_MESSAGE(num(":hits") > 0, msg);
    }

    // Driving across the shot at a range that gives it time to arrive: the
    // shell goes where the player was.
    camera_at(800, 800, 90);
    foe_at(1, 800, 1200, 180);
    run("make \"hits 0  make \"lives 3  make \"tk.boom 0  make \"es.on false  enemy.fires");
    press_forward();
    for (int i = 0; i < 40 && truth(":es.on"); i++)
        run("pollkeys  step.tank  cam.offsets  ob.scan  step.eshell");
    release_forward();
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":hits"),
                                    "a moving player was hit by a shot that never led them");
}

// The cheapest collision in the game: the player is at the origin of the frame
// every offset here is already in, so it is two comparisons and no arithmetic.
void test_the_enemys_shell_hits_the_player_and_pauses_the_tank(void)
{
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");
    // `respawn` re-rolls the player's position from M6 on, and the mock's
    // hardware random source is the CONSTANT 42 -- so without this the four
    // re-rolls in `place.player` are four identical rolls, and against this
    // fixture's eight stacked cubes they can all land inside one.
    run("rerandom");
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

    // It runs down and the tank drives again -- SOMEWHERE ELSE, because
    // `respawn` re-rolls the player's position and facing as `:PlacePlayer`
    // does.  So "it came back" is measured against where it came back TO.
    for (int i = 0; i < (int)num(":boom.frames") + 1; i++)
        run("step.tank");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":tk.boom"));
    const float rx = num(":px"), rz = num(":pz");
    press_forward();
    run("pollkeys  step.tank");
    release_forward();
    const float moved = sqrtf((num(":px") - rx) * (num(":px") - rx) +
                              (num(":pz") - rz) * (num(":pz") - rz));
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.0f * num(":tread.step"), moved,
                                     "the tank never came back");
}

//==========================================================================
// M2 -- the explosion and the radar
//==========================================================================

// THE PLAYER'S OWN DEATH IS THE ONLY SCREEN-SPACE EXPLOSION LEFT, and it is
// screen-space for a reason rather than for economy: you are inside the tank
// that blew up, so there is no object out on the plain to project.
void test_the_players_explosion_draws_its_fragments_and_runs_down(void)
{
    camera_at(800, 800, 0);
    run("make \"lives 3  make \"tk.boom 0  make \"es.on false  hit.player 15");
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

// WHAT YOU KILL COMES APART INTO THE THREE SOLIDS IT WAS BUILT FROM: the hull,
// the turret and the gun.  The count is the whole of the assertion -- a wreck
// that is thirty-two edges is a wreck that is being drawn by the same two box
// projectors the live tank used, which is what makes it free.
void test_the_wreck_is_the_tank_in_three_pieces_and_runs_down(void)
{
    camera_at(800, 800, 0);
    foe_at(1, 800, 1000, 180);
    run("make \"kills 0  kill.enemy");
    TEST_ASSERT_EQUAL_FLOAT(num(":boom.frames") - 1, num(":wr.n"));

    mock_device_clear_graphics();
    run("draw.wreck");
    TEST_ASSERT_EQUAL_INT(EDGES_WRECK, mock_device_line_count());

    for (int i = 0; i < (int)num(":boom.frames") + 2; i++)
        run("draw.wreck");
    mock_device_clear_graphics();
    run("draw.wreck");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(), "the wreck never went out");
}

// Measure how wide the drawn wreck is on the glass.
static float wreck_extent(void)
{
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        lo = fminf(lo, fminf(l->x1, l->x2));
        hi = fmaxf(hi, fmaxf(l->x1, l->x2));
    }
    return hi - lo;
}

// THE PIECES FLY APART, AND THEY DO IT IN THE WORLD.  Two things are asserted
// and the second is the one that matters: the wreck spreads as it burns, and
// it spreads AROUND THE PLACE THE TANK DIED rather than around a point on the
// screen -- so a camera that turns sweeps it across the view exactly as it
// sweeps a cube, which is the whole difference between this and the five
// screen-space strokes it replaced.
void test_the_wreck_flies_apart_in_the_world(void)
{
    camera_at(800, 800, 0);
    foe_at(1, 800, 1000, 180);
    run("make \"kills 0  kill.enemy");

    mock_device_clear_graphics();
    run("draw.wreck");
    const float first = wreck_extent();

    for (int i = 0; i < 12; i++)
        run("draw.wreck");
    mock_device_clear_graphics();
    run("draw.wreck");
    const float later = wreck_extent();
    // 50 px across on the frame it dies and 124 twelve frames later, at 200
    // steps.  The threshold is well under that because what is being asserted
    // is that the pieces separate at all, not the tuning of how far.
    TEST_ASSERT_TRUE_MESSAGE(later > first * 1.5f, "the wreck did not fly apart");

    // Turn the camera and the wreck must move with the plain.  It is dead
    // ahead, so a turn to the left puts it to the right.
    camera_at(800, 800, 340);
    mock_device_clear_graphics();
    run("draw.wreck");
    float lo = 1e9f;
    for (int i = 0; i < mock_device_line_count(); i++)
        lo = fminf(lo, fminf(mock_device_get_line(i)->x1, mock_device_get_line(i)->x2));
    TEST_ASSERT_TRUE_MESSAGE(lo > 0, "the wreck stayed on the screen when the camera turned");
}

// A WRECK CAN BE THROWN AT YOUR FEET, and that is what `near` guards in
// `wreck.hull` and its two neighbours.  The live enemy is held off by its
// collision radius; a PIECE of one is thrown, and `turret.columns` clamps its
// range at `zmin` but then subtracts a half-width from it -- so a piece drawn
// from inside 60 steps could divide by something arbitrarily near zero and put
// a vertex anywhere at all.  This kills a tank at point-blank range and reads
// every stroke of every frame of the wreck back.
void test_a_wreck_at_your_feet_stays_on_the_arithmetic(void)
{
    camera_at(800, 800, 0);
    foe_at(1, 800, 870, 180);          // 70 steps ahead: a rammed missile's range
    run("make \"kills 0  kill.enemy");

    for (int f = 0; f < (int)num(":boom.frames"); f++)
    {
        mock_device_clear_graphics();
        run("draw.wreck");
        for (int i = 0; i < mock_device_line_count(); i++)
        {
            const MockLine *l = mock_device_get_line(i);
            const float v[4] = {l->x1, l->y1, l->x2, l->y2};
            for (int j = 0; j < 4; j++)
            {
                char msg[96];
                snprintf(msg, sizeof(msg), "frame %d, stroke %d: %g", f, i, (double)v[j]);
                TEST_ASSERT_TRUE_MESSAGE(isfinite(v[j]) && fabsf(v[j]) < 2000.0f, msg);
            }
        }
    }
}

// Every stroke of the wreck, top and bottom, in screen rows.
static void wreck_rows(float *lo, float *hi)
{
    *lo = 1e9f;
    *hi = -1e9f;
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        *lo = fminf(*lo, fminf(l->y1, l->y2));
        *hi = fmaxf(*hi, fmaxf(l->y1, l->y2));
    }
}

// Kill whatever is out there and draw the first frame of its wreck.
static int kill_and_draw(int kind)
{
    foe_at(kind, 800, 1100, 180);
    run("make \"kills 0  kill.enemy");
    mock_device_clear_graphics();
    run("draw.wreck");
    return mock_device_line_count();
}

// EACH KIND COMES APART INTO ITSELF.  A tank and a supertank are a hull, a
// turret and a gun, so their wreck is three boxes; a missile and a saucer are
// one solid with no parts, so theirs is three smaller copies of that solid.
// `e.gun` is the only question `draw.wreck` asks about what died, which is the
// same boolean `draw.foe` branches on when it is alive (design §16.7.2).
void test_each_kind_comes_apart_into_its_own_shape(void)
{
    camera_at(800, 800, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(EDGES_WRECK, kill_and_draw(1), "the tank");
    TEST_ASSERT_EQUAL_INT_MESSAGE(EDGES_WRECK, kill_and_draw(3), "the supertank");
    TEST_ASSERT_EQUAL_INT_MESSAGE(EDGES_SHARDS, kill_and_draw(2), "the missile");
    TEST_ASSERT_EQUAL_INT_MESSAGE(EDGES_SHARDS, kill_and_draw(4), "the saucer");
}

// A SAUCER BLOWS UP WHERE IT WAS FLYING AND THEN COMES DOWN.  Its keel is 41
// steps above the eye and that is the whole reason you cannot shoot one from
// behind a cube; a wreck built around eye level would have dropped the wreckage
// on the ground the instant the shot landed.  `e.t` is where it died and the
// decay in `draw.wreck` is what brings the pieces down out of the sky.
void test_a_saucer_explodes_where_it_was_flying_and_falls(void)
{
    camera_at(800, 800, 0);
    const float hz = num(":hz");

    TEST_ASSERT_EQUAL_INT(EDGES_SHARDS, kill_and_draw(4));
    float lo, hi, early_hi;
    wreck_rows(&lo, &hi);
    early_hi = hi;
    TEST_ASSERT_TRUE_MESSAGE(lo > hz, "the saucer's wreck was on the ground, not in the sky");

    for (int i = 0; i < (int)num(":boom.frames") - 3; i++)
        run("draw.wreck");
    mock_device_clear_graphics();
    run("draw.wreck");
    wreck_rows(&lo, &hi);
    TEST_ASSERT_TRUE_MESSAGE(hi < early_hi - 10.0f, "the wreckage hung in the air");

    // And the tank, whose hull sits ON the plain, still straddles the horizon.
    TEST_ASSERT_EQUAL_INT(EDGES_WRECK, kill_and_draw(1));
    wreck_rows(&lo, &hi);
    TEST_ASSERT_TRUE_MESSAGE(lo < hz && hi > hz, "the tank's wreck left the ground");
}

// THE WRECK HAS TO BE OVER BEFORE THE NEXT ENEMY ARRIVES, because it is drawn
// out of the dead enemy's own slots -- `e.x`, `e.z`, `e.h` and `e.hw` -- and
// `spawn.enemy` writes every one of them.  Both countdowns run in the same
// frame, `step.enemy` first, so `wr.n` is one shorter than `e.boom` and this
// is the test that says so.
void test_the_wreck_is_finished_before_the_next_enemy_spawns(void)
{
    new_game();
    camera_at(800, 800, 0);
    foe_at(1, 800, 1000, 180);
    run("make \"kills 0  make \"tk.boom 0  kill.enemy");
    run("make \"paused false");

    for (int i = 0; i < (int)num(":boom.frames") + 2; i++)
    {
        const bool spawned = num(":e.boom") <= 1;
        run("play.frame");
        if (spawned)
        {
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(
                0, num(":wr.n"),
                "a piece of the old tank was still being drawn on the frame the new one spawned");
            return;
        }
    }
    TEST_FAIL_MESSAGE("the enemy never came back");
}

// The blip is the enemy's camera-frame position scaled, which is what makes it
// free: right of you is right on the radar, behind you is below the centre.
// An arctangent here would be the same picture and three statements more.
void test_the_blip_is_the_enemy_in_the_camera_frame(void)
{
    const float cx = num(":rd.x"), cy = num(":rd.y");
    camera_at(800, 800, 0);

    // The blip is a PING now: it exists only for the thirty frames after the
    // sweep crosses the enemy, so a test about WHERE it draws has to arm it.
    enemy_at(1000, 1000, 0);          // ahead and to the right
    run("make \"rd.bi 30  blip");
    TEST_ASSERT_TRUE_MESSAGE(num(":rd.bx") > cx, "a blip to the right drew to the left");
    TEST_ASSERT_TRUE_MESSAGE(num(":rd.by") > cy, "a blip ahead drew behind");

    enemy_at(600, 600, 0);            // behind and to the left
    run("make \"rd.bi 30  blip");
    TEST_ASSERT_TRUE_MESSAGE(num(":rd.bx") < cx, "a blip to the left drew to the right");
    TEST_ASSERT_TRUE_MESSAGE(num(":rd.by") < cy, "a blip behind drew ahead");

    // Turning the tank turns the radar picture, because the frame is the
    // camera's and not the world's.
    camera_at(800, 800, 90);
    enemy_at(1000, 800, 0);           // now dead ahead
    run("make \"rd.bi 30  blip");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, cx, num(":rd.bx"), "the radar did not turn with the tank");
}

void test_the_radar_is_drawn_and_the_blip_is_inside_it(void)
{
    camera_at(800, 800, 0);
    enemy_at(800, 1000, 180);
    mock_device_clear_graphics();
    run("make \"rd.bi 30  radar");
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
    run("make \"rd.bi 30  radar");
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
    const float objects = num(":ob.count");

    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
    run("ignore clock");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(fps, num(":fps"), "`clock` wrote the frame rate");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(objects, num(":ob.count"),
                                    "`clock` wrote the object cap");

    set_mock_cpu_khz(false, LOGO_CPU_KHZ_NORMAL);
    run("ignore clock");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(fps, num(":fps"), "a refused clock cut the frame rate");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(objects, num(":ob.count"),
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
    TEST_ASSERT_NULL_MESSAGE(strstr(screen, "Battlezone M5"), screen);

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
    assert_out_at_a_spawning_distance("the game opens with an enemy");

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

// Loudness and length are the whole vocabulary of the discrete hardware, so
// they are what the explosion and cannon tests read.
static int last_vol_on(int voice)
{
    const MockDeviceState *st = mock_device_get_state();
    for (int i = st->sound.gate_count - 1; i >= 0; i--)
        if (st->sound.gates[i].voice == voice)
            return st->sound.gates[i].vol;
    return -1;
}

static uint32_t last_dur_on(int voice)
{
    const MockDeviceState *st = mock_device_get_state();
    for (int i = st->sound.gate_count - 1; i >= 0; i--)
        if (st->sound.gates[i].voice == voice)
            return st->sound.gates[i].dur;
    return 0;
}

// `play` goes to the queue rather than the gate log, and the queue keeps no
// voice with the event -- these count and read what was compiled into it.
static int queued_count(void)
{
    return mock_device_get_state()->sound.queued_count;
}

static uint16_t queued_freq(int i)
{
    return mock_device_get_state()->sound.queued[i].freq_hz;
}

// The engine CHASES its target now, so a test that wants the pitch for a tread
// setting has to let it arrive: `eng.slew` is 6 Hz a frame over a 44 Hz range.
static uint32_t settled(int left, int right)
{
    char expr[96];
    snprintf(expr, sizeof(expr), "make \"left.tread %d  make \"right.tread %d  repeat 19 [engine]",
             left, right);
    run(expr);
    // Nineteen frames to arrive, then the log is emptied and the twentieth is
    // the one that gets read: the engine is four gates a frame and the log
    // holds sixty-four.
    mock_sound_clear_gates();
    run("engine");
    return last_freq_on(0);
}


//--------------------------------------------------------------------------
// The enemy sequence
//--------------------------------------------------------------------------

// M5 -- THE CAMPAIGN.  M4 walked a ring of eight kinds; the cabinet does not,
// and these are its rules (design section 16.9, and the disassembly notes at
// 6502disassembly.com/va-battlezone).  Every one of them is arithmetic on
// events -- a spawn, a death, a clock running out -- so none of it is in the
// frame, and all of it is testable without drawing anything.

// The staple, and the two thresholds that hold the other kinds back.  A saucer
// is worth 5,000 points and cannot hurt you, so it does not appear until the
// plain is worth one; a missile is the cabinet's answer to a player who is
// winning, and it waits.
//
// THE MISSILE THRESHOLD IS 20,000 AND THE CABINET'S IS 5,000 (the default DIP
// setting).  It is a deliberate departure and not a number nobody checked: it
// was moved after M6 measured everything else against the ROM, and it is worth
// knowing that one saucer is 5,000 points, so the cabinet's threshold can be
// cleared by a single kill.  The saucer's 2,000 is the cabinet's, untouched.
void test_the_plain_opens_with_tanks_and_nothing_else(void)
{
    run("rerandom");
    run("make \"score 0  make \"ms.n 0");
    for (int i = 0; i < 60; i++)
    {
        run("pick.kind");
        const int k = (int)num(":e.kind");
        char msg[96];
        snprintf(msg, sizeof(msg), "a kind %d turned up at zero points, before it is earned", k);
        TEST_ASSERT_TRUE_MESSAGE(k == 1, msg);
    }
}

void test_a_saucer_waits_for_two_thousand_and_a_missile_for_twenty(void)
{
    run("rerandom");

    // Between the two thresholds: saucers, and still no missiles.
    run("make \"score 3000  make \"ms.n 0");
    int saucers = 0;
    for (int i = 0; i < 60; i++)
    {
        run("pick.kind");
        const int k = (int)num(":e.kind");
        TEST_ASSERT_TRUE_MESSAGE(k != 2, "a missile flew before the score earned one");
        if (k == 4)
            saucers++;
    }
    TEST_ASSERT_TRUE_MESSAGE(saucers > 0, "no saucer crossed the plain above 2,000 points");

    // Above both: missiles as well, and the tank is still the staple.
    run("make \"score 25000  make \"ms.n 0");
    int missiles = 0, tanks = 0;
    for (int i = 0; i < 60; i++)
    {
        run("pick.kind");
        const int k = (int)num(":e.kind");
        if (k == 2) missiles++;
        if (k == 1) tanks++;
    }
    // ONE SPAWN IN TWO, not one in three: `:MaybeMissile` is a straight coin
    // flip once the score clears the threshold, so above it a missile is as
    // likely as everything else put together and the tank stops being the
    // staple.  M5 read the cabinet's 50/50 as a third.
    TEST_ASSERT_TRUE_MESSAGE(missiles > 0, "no missile flew above 20,000 points");
    TEST_ASSERT_TRUE_MESSAGE(tanks > 0, "the tank vanished above 20,000 points");
    TEST_ASSERT_TRUE_MESSAGE(missiles > 15, "missiles are rarer than the cabinet's coin flip");
}

// The cabinet's counter: the 6th missile promotes the tanks and the 129th
// demotes them again.  It is an 8-bit counter started at $ff wrapping past 127,
// and the second half of it is a joke a game has to run a very long time to
// hear.
void test_six_missiles_promote_the_tank_to_a_supertank(void)
{
    run("make \"score 0");

    run("make \"ms.n 5");
    run("pick.kind");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.kind"), "the fifth missile promoted the tanks");

    run("make \"ms.n 6");
    run("pick.kind");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":e.kind"), "six missiles did not promote the tanks");

    run("make \"ms.n 129");
    run("pick.kind");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.kind"), "the counter never wrapped back to slow tanks");
}

// A launch is counted at the spawn, which is the only place a missile can be
// chosen -- and `next.kind` is what does the counting, not `pick.kind`, because
// a missile sent by the evade timer is a launch too.
void test_every_missile_launch_is_counted(void)
{
    run("make \"score 9000  make \"ms.n 0");
    run("make \"e.gun true  make \"e.ram false  make \"e.drift false  make \"e.tmr 0");
    run("next.kind");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":e.kind"), "the evade timer did not send a missile");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":ms.n"), "the launch was not counted");
}

// Drive away from a tank for long enough and the cabinet stops sending tanks.
// The clock is 48-64 seconds, it is wound at the spawn, and running it out is
// not a kill: no explosion and no points, just a tank that is not there any
// more and a missile on its way.
void test_a_tank_you_drive_away_from_is_replaced_by_a_missile(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("make \"score 9000  make \"ms.n 0");
    foe_at(1, 800, 1400, 180);
    run("make \"e.boom 0  make \"e.rage 500  make \"e.tmr 1  make \"frame.count 3");

    run("step.enemy");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.alive"), "the tank stayed past its welcome");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9000, num(":score"), "a tank that left scored points");

    run("step.enemy");
    TEST_ASSERT_TRUE_MESSAGE(truth(":e.alive"), "nothing replaced the tank");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":e.kind"),
                                    "running away from a tank was answered with another tank");
}

// And a missile that is dodged is followed by another, and another, until the
// cycle clock runs out -- 16 to 32 seconds of them.  The clock is wound ONCE,
// by the missile that starts the cycle: a missile that re-wound it would be a
// cycle that never ends.
void test_a_dodged_missile_is_followed_by_another_until_the_cycle_ends(void)
{
    run("make \"score 9000  make \"ms.n 0");
    run("make \"e.gun false  make \"e.drift false");

    // A missile is out there and the cycle has time left on it.
    run("make \"e.ram true  make \"e.tmr 100");
    run("next.kind");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":e.kind"), "a dodged missile was not followed by another");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(100, num(":e.tmr"),
                                    "the second missile re-wound the cycle, which never ends");

    // And when it has none.  The score comes back below the threshold for the
    // question, because above it `pick.kind` may draw a missile of its own and
    // the answer would be a die roll rather than the rule.
    run("make \"score 1000  make \"e.ram true  make \"e.tmr 0");
    run("next.kind");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":e.kind"), "the missile cycle never ended");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.tmr") > 500, "the tank that ended the cycle got no clock");
}

// THE ENEMY KEEPS SCORE TOO, and the difference between the two scores is the
// only difficulty knob in the game.
//
// IT IS A DIFFERENCE AND NOT A RAMP.  M5 divided it by 7,000 and clamped it to
// 0..1, then spent that smoothly on four numbers.  The ROM reads the SIGN of it
// to pick a behaviour and four BUCKETS of it to pick a spawn cone, and the
// 7,000 belongs only to the top of that ladder.
void test_the_enemy_keeps_score_and_the_difference_is_the_difficulty(void)
{
    new_game();
    run("make \"score 0  make \"e.score 0  spawn.cone");
    const float even = num(":e.b");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":e.diff"), "an even game is not an even game");

    // The ladder: 2,000, 4,000 and 6,000 points of daylight are the rungs, and
    // it clamps at the top.
    run("make \"score 2000  spawn.cone");
    const float rung1 = num(":e.b");
    run("make \"score 4000  spawn.cone");
    const float rung2 = num(":e.b");
    run("make \"score 6000  spawn.cone");
    const float rung3 = num(":e.b");
    TEST_ASSERT_TRUE_MESSAGE(rung1 > even && rung2 > rung1 && rung3 > rung2,
                             "the spawn cone does not widen as the player pulls ahead");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(360, rung3, "the top rung is not the whole plain");

    run("make \"score 99000  spawn.cone");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(360, num(":e.b"), "the ladder is not clamped above");

    // Dying pays the enemy, and the next enemy is easier for it.
    run("make \"score 3000  make \"tk.boom 0  make \"lives 3");
    run("hit.player 15");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1000, num(":e.score"), "the enemy scored nothing for killing you");
    run("spawn.cone");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2000, num(":e.diff"), "a death did not cost you daylight");
}

// A MILD ENEMY DRIVES SOMEWHERE ELSE.  It does not drive slower, aim worse or
// think less often -- M5 softened all three by `e.agg` and the ROM softens none
// of them.  `SetTankTurnTo` picks a HEADING: level on score it drives 90
// degrees off you and holds that for four seconds, behind on score it wanders,
// and ahead on score it charges.  Difficulty is a change of intent.
void test_a_mild_enemy_drives_somewhere_else_at_full_speed(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("rerandom");

    // The row is the row, whatever the score: nothing is scaled on the way in.
    run("make \"score 0  make \"e.score 0  make \"e.kind 1  set.kind");
    const float mild_step = num(":e.step"), mild_turn = num(":e.turn");
    run("make \"score 9000  make \"e.kind 1  set.kind");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(mild_step, num(":e.step"), "a mild enemy was slowed down");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(mild_turn, num(":e.turn"), "a mild enemy was made to turn slower");

    // Level on score, thirty decisions: the ones that are not the coin flip
    // head about 90 degrees away from the player rather than at them.
    foe_at(1, 800, 1100, 180);
    run("make \"score 3000  make \"e.score 3000  make \"e.rage 200");
    int away = 0, at = 0;
    for (int i = 0; i < 60; i++)
    {
        run("make \"e.mvc 1  make \"e.rev 0  hunt");
        const float off = num("abs wrap.diff :e.aimh - (arctan :e.dz :e.dx) - 180");
        if (off > 45.0f) away++; else at++;
    }
    TEST_ASSERT_TRUE_MESSAGE(away > 5, "a level enemy never drove anywhere but at the player");

    // AND THE COIN FLIP IS READ FIRST, so about half of even a mild enemy's
    // decisions are a straight attack.  This is the piece M5 never had, and it
    // is why the cabinet's easy tank reads as inconsistent rather than passive.
    TEST_ASSERT_TRUE_MESSAGE(at > 15, "the coin flip never sent a mild enemy at the player");
    TEST_ASSERT_TRUE_MESSAGE(away > 10, "the coin flip sent a mild enemy at the player every time");
}

// NOTHING IS SOFTENED BY THE SCORE any more -- not a missile, and not a tank
// either.  M5 scaled speed, aim and think rate by `e.agg` for anything with a
// gun and exempted the missile from it by name; the ROM scales none of the four
// kinds, so the exemption has nothing left to be an exemption from.  What is
// worth keeping is the assertion that a row survives `set.kind` unmodified.
void test_no_row_is_rewritten_on_the_way_in(void)
{
    new_game();
    for (int k = 1; k <= 4; k++)
    {
        char expr[80], msg[128];
        snprintf(expr, sizeof(expr), "make \"score 0  make \"e.score 0  make \"e.kind %d  set.kind", k);
        run(expr);
        const float step = num(":e.step"), turn = num(":e.turn"), hit = num(":e.hit");

        snprintf(expr, sizeof(expr), "make \"score 40000  make \"e.score 0  make \"e.kind %d  set.kind", k);
        run(expr);
        snprintf(msg, sizeof(msg), "kind %d changed speed with the score", k);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(step, num(":e.step"), msg);
        snprintf(msg, sizeof(msg), "kind %d changed turn rate with the score", k);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(turn, num(":e.turn"), msg);
        snprintf(msg, sizeof(msg), "kind %d changed size with the score", k);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(hit, num(":e.hit"), msg);
    }
}

// Seventeen seconds are up, and the tank stops being careful whatever the score
// says.  `rez_protect` counts to $ff and `TryShootPlayer` and `SetTankTurnTo`
// both read it: at the top it charges, and it takes free shots.
//
// M5 re-read the row at full aggression here, which is gone with the scaling.
// What is left is the rule itself, and it is now the FIRST rung of the ladder
// in `aim.enemy` rather than a separate event.
void test_seventeen_seconds_makes_a_mild_enemy_aggressive(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("rerandom");
    foe_at(1, 800, 1100, 180);
    // Behind on score, which is the mildest the enemy ever gets.
    run("make \"score 0  make \"e.score 4000  make \"e.cool 99");

    // While the clock runs it may wander: thirty decisions, and some of them
    // point somewhere other than at the player.
    run("make \"e.rage 200");
    int wandered = 0;
    for (int i = 0; i < 30; i++)
    {
        run("make \"e.mvc 1  make \"e.rev 0  hunt");
        if (num("abs wrap.diff :e.aimh - (arctan :e.dz :e.dx) - 180") > 5.0f)
            wandered++;
    }
    TEST_ASSERT_TRUE_MESSAGE(wandered > 0, "a losing enemy never wandered at all");

    // Once it has run out, every decision is the player's bearing.
    run("make \"e.rage 0");
    for (int i = 0; i < 30; i++)
    {
        run("make \"e.mvc 1  make \"e.rev 0  hunt");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.0f,
            num("wrap.diff :e.aimh - (arctan :e.dz :e.dx) - 180"),
            "the enemy never lost its patience");
    }
}

// A MISSILE THAT GETS PAST YOU IS GONE, and the test is a DISTANCE and not a
// clock.  The ROM checks it in the radar pass at $6ba0: once the enemy's
// distance high byte reaches $80 -- 800 steps -- a missile is written off as
// missed.  M5 used `e.rage`, 255 frames, so a missile that shot past at 12.5
// steps a frame kept flying for another eleven seconds.
void test_a_missile_that_gets_past_you_has_been_dodged(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("make \"score 9000");

    // Still inside 800 steps: it is very much alive.
    foe_at(2, 800, 1400, 180);
    run("make \"e.boom 0  make \"e.tmr 500  make \"e.rage 1  make \"bm.n 0  make \"frame.count 2");
    run("step.enemy");
    TEST_ASSERT_TRUE_MESSAGE(truth(":e.alive"), "a missile 600 steps out was written off");

    // Past it, on the diagonal where `e.d` is furthest from a real distance.
    foe_at(2, 1300, 1300, 180);
    run("make \"e.boom 0  make \"e.tmr 500  make \"e.rage 1  make \"bm.n 0  make \"frame.count 2");
    run("step.enemy");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.alive"), "the missile flew on for ever");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9000, num(":score"), "dodging a missile scored points");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":bm.n"), "a dodge drew an explosion");
}

//--------------------------------------------------------------------------
// M5 -- where a spawn arrives
//--------------------------------------------------------------------------

// Two distances, evenly -- the cabinet's 3/4 and 3/8 of its maximum range --
// and the near one is INSIDE a tank's stand-off, which is what makes a spawn
// occasionally alarming rather than always distant.
void test_a_spawn_is_at_one_of_two_distances(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("rerandom");
    run("make \"e.agg 1  make \"e.kind 1  set.kind");

    const float far_d = num(":e.spawn"), near_d = far_d * 0.5f;
    int fars = 0, nears = 0;
    for (int i = 0; i < 40; i++)
    {
        run("place.enemy");
        const float d = sqrtf(num(":e.dx") * num(":e.dx") + num(":e.dz") * num(":e.dz"));
        if (fabsf(d - far_d) < 2.0f) fars++;
        else if (fabsf(d - near_d) < 2.0f) nears++;
        else
        {
            char msg[96];
            snprintf(msg, sizeof(msg), "a spawn arrived at %g steps, which is neither distance",
                     (double)d);
            TEST_ASSERT_TRUE_MESSAGE(false, msg);
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(fars > 5 && nears > 5, "the two distances are not evenly drawn");
}

// The spawn cone, and the rung it starts on.
//
// THE RUNGS ARE THE CABINET'S, SCALED BY FIELD OF VIEW.  The ROM masks a random
// bearing with $0f, $1f, $3f or $7f -- 45, 90, 180 or 360 degrees -- and its
// bottom rung of 45 is EXACTLY its own field of view.  So the rule it encodes
// is "somewhere in view", and with a 63-degree view here that is 63.  Every
// rung carries the same 1.4.
// AND IT ARRIVES POINTING ANYWHERE.  The other half of the same board's report:
// "when I kill a tank a new one appears in front of me; in the arcade I see it
// off in the distance, pointing in a random direction".
//
// The distance and the bearing were already the cabinet's -- $6000 or $3000 of
// range on a cone about your own facing, whose bottom rung is NARROWER than the
// view -- so a replacement in front of you is what the cabinet does.  What it
// does not do is point it at you: `CreateTank` ($69e8) reads POKEY_RANDOM into
// `enemy_turn_to` and never touches `enemy_facing`, and the only unit whose
// facing it aims at the player is the missile ($6adb).  A tank that arrives
// broadside has to turn before it can shoot, and 180 degrees of that is eight
// seconds at `e.turn` -- which is the whole difference between a new tank on
// the horizon and a new tank in your face.
void test_a_new_tank_arrives_pointing_anywhere_and_a_missile_at_you(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("rerandom");

    run("make \"e.kind 1  set.kind");
    int at_you = 0, flank_or_back = 0;
    for (int i = 0; i < 40; i++)
    {
        run("place.enemy");
        // How far the tank's facing is from the bearing back to the player.
        const float back = atan2f(-num(":e.dx"), -num(":e.dz")) * 57.2958f;
        float err = fmodf(num(":e.h") - back + 540.0f, 360.0f) - 180.0f;
        if (fabsf(err) < 45.0f)
            at_you++;
        if (fabsf(err) > 90.0f)
            flank_or_back++;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "%d of 40 spawns arrived aimed at the player, %d showing a flank",
             at_you, flank_or_back);
    TEST_ASSERT_TRUE_MESSAGE(at_you < 20, msg);
    TEST_ASSERT_TRUE_MESSAGE(flank_or_back > 8, msg);

    // A missile is the exception the ROM makes, and it is the one that matters:
    // it is a dodge and not a search, so it arrives already looking at you.
    run("make \"e.kind 2  set.kind");
    for (int i = 0; i < 20; i++)
    {
        run("place.enemy");
        const float back = atan2f(-num(":e.dx"), -num(":e.dz")) * 57.2958f;
        const float err = fmodf(num(":e.h") - back + 540.0f, 360.0f) - 180.0f;
        snprintf(msg, sizeof(msg), "a missile arrived %g degrees off the player", (double)err);
        TEST_ASSERT_TRUE_MESSAGE(fabsf(err) < 1.0f, msg);
    }
}

void test_a_losing_player_gets_the_enemy_in_front_of_them(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("rerandom");

    // Behind on score: the forward ARC, which is the rule the cabinet has.
    run("make \"score 0  make \"e.score 4000  make \"e.kind 1  set.kind");
    for (int i = 0; i < 40; i++)
    {
        run("place.enemy");
        char msg[96];
        snprintf(msg, sizeof(msg), "a mild spawn arrived at (%g, %g), which is behind you",
                 (double)num(":e.dx"), (double)num(":e.dz"));
        TEST_ASSERT_TRUE_MESSAGE(num(":e.dz") > 0, msg);
    }

    // Six thousand clear: anywhere at all, including behind you.
    run("make \"score 9000  make \"e.score 0  make \"e.kind 1  set.kind");
    int behind = 0;
    for (int i = 0; i < 40; i++)
    {
        run("place.enemy");
        if (num(":e.dz") < 0)
            behind++;
    }
    TEST_ASSERT_TRUE_MESSAGE(behind > 5, "at the top rung the enemy still only comes from in front");
}

// Design section 16.9.6's defect, and M6's re-cut of it.
//
// A board found a 40-degree cone unplayable: it is narrower than the 63-degree
// field of view, so a replacement arrived in the same third of the screen, at
// one of two ranges, against the same stretch of empty horizon.  Two discrete
// distances inside a cone narrower than the view is TWO PLACES.  M5's fix was
// a 150-degree floor.
//
// The ROM's floor is its own field of view exactly -- $0f is 45 degrees and the
// cabinet sees 45 -- so the rule is "somewhere in view", and here that is 63.
// THAT SITS RIGHT ON THIS TEST'S OLD BOUNDARY, which asserted that thirty
// spawns cover MORE bearing than the view.  A cone equal to the view cannot.
//
// So the question is asked where the cabinet actually opens up: 2,000 points
// clear is one rung up and 126 degrees, which is the state a player is in for
// almost all of a real game -- you are ahead by two kills within the first
// minute.  The bottom rung keeps the weaker half of the claim, which is the
// half the board's complaint was really about: consecutive spawns have to be
// somewhere else, even when the cone is one screen wide.
void test_two_spawns_running_are_not_the_same_place(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("rerandom");

    // One rung up, which is where a game spends its time.
    run("make \"score 3000  make \"e.score 0  make \"e.kind 1  set.kind");
    float lo = 999.0f, hi = -999.0f, last_x = 0.0f, last_z = 0.0f;
    int moved = 0;
    for (int i = 0; i < 30; i++)
    {
        run("place.enemy");
        const float x = num(":e.dx"), z = num(":e.dz");
        const float bearing = atan2f(x, z) * 57.2958f;
        if (bearing < lo) lo = bearing;
        if (bearing > hi) hi = bearing;
        if (i > 0 && sqrtf((x - last_x) * (x - last_x) + (z - last_z) * (z - last_z)) > 200.0f)
            moved++;
        last_x = x;
        last_z = z;
    }

    char msg[192];
    snprintf(msg, sizeof(msg),
             "thirty spawns covered %g degrees, and the field of view is 63 -- they are all "
             "in the same place", (double)(hi - lo));
    TEST_ASSERT_TRUE_MESSAGE(hi - lo > 100.0f, msg);
    snprintf(msg, sizeof(msg),
             "only %d of 29 spawns landed more than 200 steps from the one before it", moved);
    TEST_ASSERT_TRUE_MESSAGE(moved >= 20, msg);

    // And on the bottom rung -- level or behind on score, a cone one screen
    // wide -- a spawn still has to land somewhere other than the last one.
    run("make \"score 0  make \"e.score 0  make \"e.kind 1  set.kind");
    moved = 0;
    for (int i = 0; i < 30; i++)
    {
        run("place.enemy");
        const float x = num(":e.dx"), z = num(":e.dz");
        if (i > 0 && sqrtf((x - last_x) * (x - last_x) + (z - last_z) * (z - last_z)) > 200.0f)
            moved++;
        last_x = x;
        last_z = z;
    }
    snprintf(msg, sizeof(msg),
             "on the bottom rung only %d of 29 spawns moved -- the two-places defect is back", moved);
    TEST_ASSERT_TRUE_MESSAGE(moved >= 15, msg);
}

// A missile always comes from the far point, and from close to dead ahead.
//
// THE CONE IS 45 DEGREES AND NOT 10.  `CreateMissile` masks a random bearing
// with $0f, which is +/- 22.5 degrees about the player's facing -- wider than
// M5's 10, and it is the one spawn rule the ROM does NOT widen with the score.
void test_a_missile_comes_from_the_far_point_and_from_in_front(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("rerandom");
    run("make \"score 9000  make \"e.score 0  make \"e.kind 2  set.kind");

    float widest = 0.0f;
    for (int i = 0; i < 30; i++)
    {
        run("place.enemy");
        const float d = sqrtf(num(":e.dx") * num(":e.dx") + num(":e.dz") * num(":e.dz"));
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, num(":e.spawn"), d,
                                         "a missile appeared somewhere other than the far point");
        const float off = fabsf(atan2f(num(":e.dx"), num(":e.dz")) * 57.2958f);
        if (off > widest) widest = off;
        char msg[128];
        snprintf(msg, sizeof(msg), "a missile appeared %g degrees off, outside the cabinet's cone", (double)off);
        TEST_ASSERT_TRUE_MESSAGE(off < 23.0f, msg);
    }
    // And it really is the whole cone rather than a tighter one by accident:
    // the score does not widen it, so this is all the spread there ever is.
    TEST_ASSERT_TRUE_MESSAGE(widest > 12.0f, "the missile cone is much narrower than the cabinet's");
}

// A saucer's course has nothing to do with you, which is one line in
// `place.enemy` and is the difference between a saucer and everything else on
// the plain.
void test_a_saucer_takes_a_heading_of_its_own(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("rerandom");
    run("make \"e.agg 0  make \"e.kind 4  set.kind");

    int facing_you = 0;
    for (int i = 0; i < 30; i++)
    {
        run("place.enemy");
        // Facing you is the bearing back along the spawn, which is what every
        // hunting kind takes.  A saucer should only manage it by accident.
        const float toward = atan2f(-num(":e.dx"), -num(":e.dz")) * 57.2958f;
        float off = fmodf(fabsf(num(":e.h") - toward), 360.0f);
        if (off > 180.0f) off = 360.0f - off;
        if (off < 15.0f)
            facing_you++;
    }
    TEST_ASSERT_TRUE_MESSAGE(facing_you < 8, "every saucer arrived pointed straight at you");
}

// For two seconds after either of you spawns, the enemy may not fire.  The
// cabinet gives you that and this game does too: 30 frames at 15 fps, set at
// the placement and counted down by the frame.
void test_nothing_fires_for_two_seconds_after_a_spawn(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("make \"e.agg 1  make \"e.kind 1  set.kind");
    run("place.enemy");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.cool") >= 30, "a new enemy arrived with a loaded gun");

    // Put it right in front, aimed, and let it try.
    enemy_at(800, 1000, 180);
    run("make \"e.boom 0  make \"e.tmr 500  make \"e.rage 200  make \"es.on false");
    for (int i = 0; i < 29; i++)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "make \"frame.count %d  step.enemy", i + 1);
        run(expr);
        TEST_ASSERT_FALSE_MESSAGE(truth(":es.on"), "the enemy fired inside its two seconds");
    }
}

// And after you die, the enemy that killed you spends three seconds going
// nowhere in particular -- which is the difference between respawning and
// respawning into the same shot.
void test_the_enemy_is_confused_for_three_seconds_after_you_respawn(void)
{
    new_game();
    camera_at(800, 800, 0);
    foe_at(1, 800, 900, 180);
    run("make \"lives 3  make \"tk.boom 0  make \"e.boom 0  make \"es.on false");
    run("respawn");

    TEST_ASSERT_TRUE_MESSAGE(num(":e.rage") > 255, "the enemy was not confused at all");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.cool") >= 30, "the enemy could fire the moment you came back");

    // `hunt` decides nothing while it is confused, so the intents it was given
    // survive: no turn, and driving on whatever heading `respawn` handed it.
    // The desired heading is put 90 degrees off the one it is on, because a
    // spawn no longer arrives pointing at you -- `e.h` and `e.aimh` come out
    // of `place.enemy` on the same random bearing, and a tank already on its
    // desired heading has nothing to do whether it is confused or not.
    run("make \"e.aimh wrap.deg :e.h + 90  make \"e.mvc 9");
    run("make \"e.t 0  make \"e.f 1  make \"e.fire false");
    run("hunt");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":e.t"), "a confused enemy turned onto you");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.fire"), "a confused enemy took aim");

    // And it comes to its senses.
    run("make \"e.rage 255");
    run("hunt");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.t") != 0 || truth(":e.fire"),
                             "the enemy never came out of it");
}

//--------------------------------------------------------------------------
// M5 -- the missile's final turn
//--------------------------------------------------------------------------

// The ROM's threshold is (missile score + 25,000 - score), floored at $0800 --
// 231 steps at 5,000 points down to 50 at 30,000.
//
// THE FIRST MISSILE HAS TO WEAVE, and this is the assertion M5 had backwards.
// `e.range` opened at 700 against an `e.spawn` of 620, so `e.d > e.range` was
// false from the moment a missile appeared and the swerve never ran -- at any
// score below 8,125.  Every missile M5 ever fired flew straight in.  The test
// that was here asserted exactly that as correct.
void test_the_missiles_final_turn_comes_later_as_the_score_climbs(void)
{
    run("make \"score 5000  make \"e.kind 2  set.kind");
    const float early = num(":e.range");
    TEST_ASSERT_TRUE_MESSAGE(early < num(":e.spawn"),
                             "the first missiles home from the moment they appear -- they cannot weave");

    run("make \"score 30000  make \"e.kind 2  set.kind");
    const float late = num(":e.range");
    TEST_ASSERT_TRUE_MESSAGE(late < early, "the final turn does not come later as the score climbs");
    TEST_ASSERT_TRUE_MESSAGE(late < 100, "the last missiles still home from the far point");

    run("make \"score 99000  make \"e.kind 2  set.kind");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(late, num(":e.range"), "the final turn is not clamped");
}

// Outside its final turn a missile swerves rather than homing, and the swerve
// takes both sides -- it is a weave and not a drift.
//
// IT IS IN THE FACING AND NOT IN A TURN RATE, which is the ROM's shape: the
// small rotations track `enemy_turn_to` onto the player, and the big swerve is
// applied as an OFFSET from that when the facing is set.  So the thing to
// measure is how far `e.h` sits off the bearing, not the sign of `e.t`.
void test_a_missile_swerves_until_its_final_turn(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("make \"score 30000  make \"e.kind 2  set.kind");
    enemy_at(800, 1400, 180);           // 600 steps out, well outside the turn
    run("make \"e.rage 200  make \"e.aimh 180");

    int left = 0, right = 0;
    for (int i = 0; i < 64; i++)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "make \"frame.count %d  hunt", i);
        run(expr);
        const float off = num("wrap.diff :e.h - :e.aimh");
        if (off > 1.0f) right++;
        if (off < -1.0f) left++;
    }
    TEST_ASSERT_TRUE_MESSAGE(left > 10 && right > 10, "the missile does not weave, it drifts");

    // And inside the final turn the swerve stops: facing and desired heading
    // become the same thing, which is what makes the last second readable.
    enemy_at(800, 830, 210);
    run("make \"frame.count 7  hunt");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.0f, num("wrap.diff :e.h - :e.aimh"),
                                     "the missile was still weaving inside its final turn");
}

//--------------------------------------------------------------------------
// M5 -- the radar
//--------------------------------------------------------------------------

// The cabinet's radar shows tanks and missiles.  It does not show obstacles and
// it does not show saucers -- so the 5,000 points are something you have to see
// out of the window, which is most of what makes a saucer worth having.
void test_the_radar_does_not_show_a_saucer(void)
{
    camera_at(800, 800, 0);
    foe_at(1, 800, 1000, 180);
    mock_device_clear_graphics();
    run("make \"rd.bi 30  radar");
    const int with_tank = mock_device_line_count();

    foe_at(4, 800, 1000, 180);
    mock_device_clear_graphics();
    run("make \"rd.bi 30  radar");
    const int with_saucer = mock_device_line_count();

    TEST_ASSERT_TRUE_MESSAGE(with_saucer < with_tank, "the radar drew a blip for a saucer");

    run("make \"e.alive false");
    mock_device_clear_graphics();
    run("make \"rd.bi 30  radar");
    TEST_ASSERT_EQUAL_INT_MESSAGE(with_saucer, mock_device_line_count(),
                                  "a saucer is not exactly as invisible as nothing at all");
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
        // `e.naim` was here, holding minus `e.aim` so that `hunt` did not have
        // to negate it -- and M5's dart wanted its slot (design section
        // 16.9.6).  The aim window is still symmetric; `hunt` now writes the
        // negation inline, which arithmetic binding tighter than comparison is
        // what makes safe.
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
    const float hw = num(":e.hw"), range = num(":e.range");

    run("make \"e.kind 3  set.kind");
    // The ROM shifts the supertank's move delta left one place and calls
    // `RotateLeft` four times instead of two, so it is the slow tank doubled
    // in both axes -- which is also the player doubled.
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.0f * step, num(":e.step"),
                                     "a supertank is not twice a tank's speed");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 2.0f * turn, num(":e.turn"),
                                     "a supertank does not turn at twice a tank's rate");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.range") > range, "a supertank holds no further off");
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

// Twelve edges: the ring of four, a fan to the nose and a fan to the tail.  The
// ring IS the fins -- a dart with a ring wider than its body has them by
// construction, and separate spikes would have wanted four more divides and
// four more x slots than `cx` has.
void test_a_missile_draws_twelve_edges(void)
{
    camera_at(800, 800, 0);
    foe_at(2, 800, 1100, 180);
    mock_device_clear_graphics();
    TEST_ASSERT_TRUE_MESSAGE(truth("project.missile"), "the missile in front of you was culled");
    run("draw.spindle");
    TEST_ASSERT_EQUAL_INT_MESSAGE(EDGES_SPINDLE, mock_device_line_count(),
                                  "the missile is not twelve edges");
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
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, hz, num(":p.ty"), "the tail is off the eye line");
    // The ring straddles it: 1 and 3 are the lateral pair, on the axis; 2 and 4
    // are above and below.
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, hz, item_of("cy1", 1), "the ring is off the axis");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, hz, item_of("cy1", 3), "the ring is off the axis");
    TEST_ASSERT_TRUE_MESSAGE(item_of("cy1", 2) > hz, "the upper fin is not above the eye line");
    TEST_ASSERT_TRUE_MESSAGE(hz > item_of("cy1", 4), "the lower fin is not below the eye line");
    // And it is LONG: the nose reaches further than the ring is wide, which is
    // the difference between a dart and the saucer's plate.
    TEST_ASSERT_TRUE_MESSAGE(num(":ms.ln") > num(":ms.fin"), "the missile is not a long pyramid");
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

    // `e.f` is a STEP COUNT from M6 on rather than a flag, and `hunt` is what
    // sets it -- so a test that calls `move.enemy` on its own has to state the
    // intent first, exactly as the frame does.
    const float before = num(":e.z");
    run("make \"e.f 1  move.enemy");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.z") > before, "the saucer was stopped by an obstacle");

    // And a tank in the same place is not.
    foe_at(1, 800, 880, 0);
    const float tank_before = num(":e.z");
    run("make \"e.f 1  move.enemy");
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
    run("make \"e.boom 0  make \"e.tmr 2  make \"bm.n 0  make \"frame.count 1");

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
    run("draw.spindle");
    TEST_ASSERT_EQUAL_INT_MESSAGE(EDGES_SPINDLE, mock_device_line_count(),
                                  "the saucer is not twelve edges");
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
    TEST_ASSERT_TRUE_MESSAGE(rim > num(":p.ty"), "the keel is not below the rim");
}

// B54.  A saucer is shot by lining it up with the gunsight, so it has to be
// somewhere the gunsight reaches.  At 90 steps above the eye it sat above the
// sight's upper centre tick for two thirds of its dwell and off the top of the
// screen for a fifth of it, and a player lining one up was guessing.
void test_a_saucer_stays_where_the_gunsight_can_reach_it(void)
{
    // `gunsight` draws its upper centre tick from the box's top bar to here,
    // and the graphics window ends at the top of the screen.
    const float tick_top = 100.0f, screen_top = 160.0f;
    const float spawn = num(":e.spawn");
    camera_at(800, 800, 0);

    // At either distance a spawn uses, the rim is on the tick: there is a drawn
    // mark to line the thing up against.
    const float shot_from[] = {spawn * 0.5f, spawn};
    for (int i = 0; i < 2; i++)
    {
        char msg[80];
        foe_at(4, 800, 800 + shot_from[i], 0);
        run("ignore project.saucer");
        snprintf(msg, sizeof(msg), "at %g steps the saucer is above the gunsight",
                 shot_from[i]);
        TEST_ASSERT_TRUE_MESSAGE(item_of("cy1", 1) <= tick_top, msg);
    }

    // And it drifts, so the near end is what matters: a quarter of the spawn
    // ring is as close as it usefully comes, and the whole model is still on
    // the glass there -- the dome is the highest point it has.
    foe_at(4, 800, 800 + spawn * 0.25f, 0);
    run("ignore project.saucer");
    TEST_ASSERT_TRUE_MESSAGE(num(":apy") <= screen_top,
                             "a saucer that drifts in climbs off the top of the screen");

    // The altitude is not free to fall any further: the keel is the lowest
    // point of the model and `kind.saucer` promises it clears a cube.
    TEST_ASSERT_TRUE_MESSAGE(num(":sc.k") + num(":eye") > num(":boxh"),
                             "the saucer's keel is inside the cubes it flies over");
}

//--------------------------------------------------------------------------
// The models, together
//--------------------------------------------------------------------------

// The two new shapes cull the way everything else does: the view cone and
// nothing else.  The missile matters most of the three -- it is aimed at your
// eye and `tk.hit` is 30, so under the old rule it vanished at about 75 steps
// and killed you off the screen.
void test_the_new_models_are_culled_by_the_view_cone(void)
{
    const float near = num(":near");

    camera_at(800, 800, 0);
    foe_at(2, 800, 800 + near * 0.5f, 180);
    TEST_ASSERT_TRUE_MESSAGE(truth("project.missile"),
                             "a missile about to hit you was not drawn");
    foe_at(2, 800, 800 + near * 4.0f, 180);
    TEST_ASSERT_TRUE_MESSAGE(truth("project.missile"), "a missile in clear view was culled");
    foe_at(2, 800 + 900.0f, 800 + 300.0f, 180);
    TEST_ASSERT_FALSE_MESSAGE(truth("project.missile"), "a missile off the glass was drawn");

    foe_at(4, 800, 800 + near * 0.5f, 0);
    TEST_ASSERT_TRUE_MESSAGE(truth("project.saucer"), "a saucer overhead was not drawn");
    foe_at(4, 800, 800 + near * 4.0f, 0);
    TEST_ASSERT_TRUE_MESSAGE(truth("project.saucer"), "a saucer in clear view was culled");
    foe_at(4, 800 + 900.0f, 800 + 300.0f, 0);
    TEST_ASSERT_FALSE_MESSAGE(truth("project.saucer"), "a saucer off the glass was drawn");
}

// The frame asks two booleans and never a kind.  What this pins is that the
// booleans reach the right model: a saucer drawn as a tank would be twelve
// edges either way and the count alone could not tell them apart, so the check
// is that the number of edges MOVES with the kind.
void test_the_frame_draws_the_model_that_matches_the_kind(void)
{
    const struct { int kind; int edges; } table[] = {
        {1, EDGES_ENEMY}, {2, EDGES_SPINDLE}, {3, EDGES_ENEMY}, {4, EDGES_SPINDLE}};

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
//
// AND THERE ARE TWO OF THEM IN A GAME, NOT ONE EVERY 15,000.  `CheckAwardLife`
// reads a single DIP threshold and fires only when the old score was below it
// and the new one is not, so it can fire at most once; the second and last is a
// separate rule at 100,000.  M5 rolled the threshold forward every time and
// handed out a tank every 15,000 for ever.
void test_two_bonus_tanks_a_game_and_never_a_third(void)
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
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(100000, num(":extra.due"),
                                    "the second bonus is not the cabinet's 100,000");

    // And it does not fire again on the next point scored, nor at 15,000 more.
    run("add.score 100");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, num(":lives"), "the bonus repeated");
    run("add.score 20000");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, num(":lives"), "a third bonus arrived at 15,000 intervals");

    // The second and last, at 100,000.
    run("add.score 70000");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5, num(":lives"), "no bonus tank at 100,000");

    // And never again, however long the game runs.
    run("add.score 500000");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5, num(":lives"), "a third bonus tank arrived");
}

// Being hit costs a tank, cracks the glass and pauses you.  It does NOT end the
// game here: the count goes down and `respawn` reads it ten frames later, so
// the player watches their own tank come apart before the card goes up.
void test_a_hit_costs_a_tank_and_cracks_the_glass(void)
{
    new_game();
    run("make \"lives 3  make \"cracked false  make \"tk.boom 0");
    run("hit.player 15");

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
    run("hit.player 15");

    for (int i = 0; i < (int)num(":boom.frames") + 1; i++)
        run("step.tank");

    TEST_ASSERT_FALSE_MESSAGE(truth(":cracked"), "the glass stayed cracked through the respawn");
    TEST_ASSERT_TRUE_MESSAGE(truth(":playing"), "the game ended with tanks left");
    assert_out_at_a_spawning_distance("you respawned with an enemy");
}

// TWO SPAWNS FOR ONE DEATH.  A missile sets its own `e.boom` and then kills
// you, so both countdowns start on the same frame and run out on the same one:
// `step.enemy` spawns from its counter and `respawn` spawns from the tank's,
// and one of the two enemies is never seen.  Nothing on the screen says so --
// there is an enemy out there either way -- so the check has to be on the
// mechanism.
//
// M4 read the ring position, which M5's campaign does not have.  What replaces
// it is a COUNT OF SPAWNS, taken by wrapping the procedure that does the
// spawning: the same trick the suite already plays on `show.game.over`, and a
// truer statement of the claim than a sequence index ever was.
void test_a_ram_spawns_one_replacement_and_not_two(void)
{
    new_game();
    camera_at(800, 800, 0);
    foe_at(2, 800, 810, 180);
    run("make \"lives 3  make \"tk.boom 0  make \"e.boom 0  make \"frame.count 1");

    run("make \"tk.spawns 0");
    proc_define_from_text("to spawn.enemy\nmake \"tk.spawns :tk.spawns + 1\n"
                          "next.kind\nplace.enemy\nend");

    run("enemy.rams");
    for (int i = 0; i < (int)num(":boom.frames") + 2; i++)
        run("step.tank  step.enemy");

    TEST_ASSERT_TRUE_MESSAGE(truth(":e.alive"), "nothing came back after the ram");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":tk.spawns"),
                                    "one death put two enemies on the plain");
}

// The last tank ends the game, and it ends it AFTER the pause rather than
// during it.
void test_the_last_tank_ends_the_game(void)
{
    new_game();
    camera_at(800, 800, 0);
    run("make \"lives 1  make \"tk.boom 0");
    run("hit.player 15");
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

    // And the POKEY pair's envelope is FLAT, because POKEY's is: AUDC holds a
    // volume until something writes another one.  It is not tidiness -- the
    // effects on that pair step every 25 ms and cannot afford a 40 ms tail
    // behind each step, which is what the bell M2 put there needed.
    TEST_ASSERT_EQUAL_INT_MESSAGE(15, st->sound.env[2].sustain, "the POKEY pair decays");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, st->sound.env[2].decay, "the POKEY pair decays");
    TEST_ASSERT_TRUE_MESSAGE(st->sound.env[2].release <= 25, "a 25 ms step will smear into the next");
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
    const uint32_t idle = settled(0, 0);
    const uint32_t one = settled(1, 0);
    const uint32_t driving = settled(1, 1);
    // A pivot is one tread each way: no ground speed, both treads turning.
    const uint32_t pivot = settled(1, -1);

    TEST_ASSERT_TRUE_MESSAGE(one > idle, "the engine does not rev");
    TEST_ASSERT_TRUE_MESSAGE(driving > one, "the second tread adds nothing");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(driving, pivot, "a pivot does not rev like a drive");
}

// The rev is a RAMP and not a dial.  The discrete hardware has one bit of
// pitch -- rev up or rev down -- with an analog slope behind it, so the engine
// chases its speed instead of arriving at it.  A frame that jumped straight to
// the target would be the one thing the cabinet could not do.
void test_the_engine_revs_up_and_back_down(void)
{
    const uint32_t idle = settled(0, 0);
    const uint32_t up = settled(1, 1);

    run("make \"au.rev :eng.hz  make \"left.tread 1  make \"right.tread 1");
    mock_sound_clear_gates();
    run("engine");
    const uint32_t first = last_freq_on(0);
    mock_sound_clear_gates();
    run("engine");
    const uint32_t second = last_freq_on(0);

    TEST_ASSERT_TRUE_MESSAGE(second > first, "the engine does not ramp -- it arrived at once");
    TEST_ASSERT_TRUE_MESSAGE(first < up, "the engine reached full rev in one frame");

    // And back down again: a released tread is a rev-down and not a cut.
    settled(1, 1);
    run("make \"left.tread 0  make \"right.tread 0");
    mock_sound_clear_gates();
    run("engine");
    const uint32_t easing = last_freq_on(0);
    TEST_ASSERT_TRUE_MESSAGE(easing < up && easing > idle,
                             "the engine dropped to idle in one frame");
}

//--------------------------------------------------------------------------
// The POKEY pair
//
// The cabinet plays the ping, the alert, the collision, the extra tank, the
// saucer and the Overture on POKEY channels 1 and 2, one at a time, and starts
// them all through one routine that asks whether the channel is busy first.
// `au.busy` is that question and `tone.fx` is that routine.
//--------------------------------------------------------------------------

// The sound you remember, and the ROM plays it where the blip FLARES: once a
// revolution as the sweep crosses the bearing, and only for an enemy that is
// on the glass at all.
void test_the_radar_pings_when_the_sweep_crosses_the_blip(void)
{
    camera_at(800, 800, 0);
    enemy_at(800, 1100, 0);
    run("make \"e.known true  make \"au.busy 0  make \"rd.bi 0");

    // The sweep one degree past the enemy's bearing: inside `rd.spin`, so this
    // is the frame it crossed.
    run("make \"rd.sw (arctan :e.zc :e.xc) + 1");
    int mark = mock_sound_gate_count();
    run("blip");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(2, mark) > 0, "the sweep crossed the blip in silence");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(6, mark) > 0, "the ping is only in one ear");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(750, last_freq_on(2), "the ping is not the cabinet's 750 Hz");

    // The frames after it: the sweep has moved on, the blip is still drawn,
    // fading, and silent.  `radar` is what advances the sweep, so a test that
    // calls `blip` on its own has to do it -- and a sweep parked on the
    // bearing would re-flare every frame in the ROM as well.
    run("make \"au.busy 0  make \"rd.sw :rd.sw + 40");
    mark = mock_sound_gate_count();
    run("repeat 8 [blip]");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(2, mark), "the radar pings every frame");

    // And an enemy off the face of the radar does not ping at all, however
    // often the sweep goes past it.
    enemy_at(800, 1600, 0);
    run("make \"au.busy 0  make \"rd.bi 0");
    run("make \"rd.sw (arctan :e.zc :e.xc) + 1");
    mark = mock_sound_gate_count();
    run("blip");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(2, mark), "an enemy off the glass pinged");
}

// Three rising boops, ONCE for each enemy -- the ROM's `enemy_known_flag`,
// cleared at the spawn and set the first time the blip is drawn.  A second
// alert for the same tank would say a second tank had arrived.
void test_the_enemy_alert_sounds_once_for_each_enemy(void)
{
    camera_at(800, 800, 0);
    enemy_at(800, 1100, 0);
    run("make \"e.known false  make \"au.busy 0  make \"rd.bi 0");
    run("make \"rd.sw (arctan :e.zc :e.xc) + 1");

    int qmark = queued_count();
    run("blip");
    const int notes = queued_count() - qmark;
    TEST_ASSERT_TRUE_MESSAGE(notes > 0, "a new enemy arrived in silence");
    TEST_ASSERT_TRUE_MESSAGE(truth(":e.known"), "the alert did not mark the enemy known");

    // It RISES, which is the whole shape of it: the cabinet sweeps 415 Hz up
    // to 659 and does it three times over.
    TEST_ASSERT_TRUE_MESSAGE(queued_freq(qmark + 1) > queued_freq(qmark),
                             "the alert does not rise");

    run("make \"au.busy 0");
    qmark = queued_count();
    run("repeat 20 [blip]");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, queued_count() - qmark, "the alert sounded twice for one enemy");

    // A fresh enemy is a fresh alert.
    run("spawn.enemy");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.known"), "the spawn did not clear the alert flag");
}

// ONE AT A TIME.  An effect that arrives while another is sounding is DROPPED
// and not queued behind it, because the cabinet had one channel for all six of
// them and checked it before it started anything.
void test_the_pokey_pair_carries_one_sound_at_a_time(void)
{
    run("make \"au.busy 0");
    int qmark = queued_count();
    run("bonus");
    const int first = queued_count() - qmark;
    TEST_ASSERT_TRUE_MESSAGE(first > 0, "the extra tank was silent");
    TEST_ASSERT_TRUE_MESSAGE(num(":au.busy") > 0, "the effect did not claim the pair");

    qmark = queued_count();
    run("saucer.hit  alert  bumped.sound");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, queued_count() - qmark,
                                  "three effects played over the top of a fourth");

    // And the pair comes free on its own: `voices` is what counts it down.
    run("make \"e.alive false  repeat 30 [voices]");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":au.busy"), "the pair never came free");
    qmark = queued_count();
    run("bonus");
    TEST_ASSERT_TRUE_MESSAGE(queued_count() - qmark > 0, "nothing could be played again");
}

// The missile's buzz is two tones a hertz apart and its volume is RANGE and
// nothing else -- the ROM shifts the distance right and subtracts, a straight
// line to silence at the far plane.  So a missile you cannot see is one you
// cannot hear, and it arrives with the thing.
void test_the_missile_buzzes_and_the_buzz_follows_the_range(void)
{
    camera_at(800, 800, 0);
    foe_at(2, 800, 1000, 180);          // kind 2 is the missile
    run("make \"au.busy 0");

    int mark = mock_sound_gate_count();
    run("voices");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(2, mark) > 0, "the missile made no sound");
    const int close = last_vol_on(2);

    // The two halves are a hertz apart, which is the beat and is why they are
    // in different ears: one pair cannot hold two frequencies any other way.
    TEST_ASSERT_TRUE_MESSAGE(last_freq_on(6) > last_freq_on(2),
                             "both halves of the buzz are the same pitch -- there is no beat");

    enemy_at(800, 1300, 180);
    run("make \"au.busy 0  voices");
    TEST_ASSERT_TRUE_MESSAGE(last_vol_on(2) < close, "the buzz does not fade with range");

    // Past the far plane it is gone entirely.
    enemy_at(800, 1560, 180);
    run("make \"au.busy 0");
    mark = mock_sound_gate_count();
    run("voices");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(2, mark), "a missile beyond the far plane was audible");

    // And it holds off while the pair is spoken for, rather than cutting the
    // effect that owns it short.
    enemy_at(800, 1000, 180);
    run("make \"au.busy 4");
    mark = mock_sound_gate_count();
    run("voices");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(2, mark), "the buzz talked over an effect");
}

// Once per arrival and not once per frame: a player leaning on a cube would
// otherwise retrigger the warble fifteen times a second.
void test_running_into_an_obstacle_sounds_once(void)
{
    run("make \"ox [800 800 800 800]  make \"ox se :ox [800 800 800 800]");
    run("make \"oz [200 200 200 200]  make \"oz se :oz [200 200 200 200]");
    run("make \"okind [1 1 1 1]  make \"okind se :okind [1 1 1 1]");
    // Through `camera_at`, which rescans the wrapped obstacle table: a
    // placement made by hand leaves `blocked?` reading the previous test's
    // field, and the first frame reports a bump that is not there.
    camera_at(800, 150, 0);
    run("make \"au.busy 0  make \"au.bumped false");

    press_forward();
    int qmark = queued_count();
    int arrived = -1;
    for (int i = 0; i < 10 && arrived < 0; i++)
    {
        run("pollkeys  step.tank  make \"au.busy 0");
        if (truth(":bumped"))
            arrived = i;
    }
    TEST_ASSERT_TRUE_MESSAGE(arrived >= 0, "the tank never reached the obstacle");
    TEST_ASSERT_TRUE_MESSAGE(queued_count() - qmark > 0, "running into a cube was silent");

    // And then leaning on it, which is the frame after and the fourteen after
    // that: silence, or the warble runs fifteen times a second.
    qmark = queued_count();
    for (int i = 0; i < 15; i++)
        run("pollkeys  step.tank  make \"au.busy 0");
    TEST_ASSERT_TRUE_MESSAGE(truth(":bumped"), "the tank came free of the obstacle");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, queued_count() - qmark, "leaning on the cube sounded again");

    // Backing off and driving in again is a second arrival and a second
    // warble -- the flag is about leaning, not about the obstacle.
    run("make \"au.bumped false  make \"au.busy 0");
    qmark = queued_count();
    run("pollkeys  step.tank");
    release_forward();
    TEST_ASSERT_TRUE_MESSAGE(queued_count() - qmark > 0, "arriving a second time was silent");
}

// THE SOUND OF BEING STUCK.  `recent_coll_flag` does two things in the ROM and
// we had only one of them: it flashes MOTION BLOCKED BY OBJECT, and it loops
// sound $04 -- 1588 Hz, volume 1, 64 ms on and 64 off -- for as long as you
// lean on the cube.  The warble says you HIT something; this says you are
// still against it.
void test_leaning_on_an_obstacle_loops_the_merp(void)
{
    run("make \"au.busy 0  make \"au.bumped true  make \"e.alive false");
    int mark = mock_sound_gate_count();
    run("voices");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(2, mark) > 0, "leaning on the cube is silent");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(6, mark) > 0, "the merp is only in one ear");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1588, last_freq_on(2), "the merp is not the cabinet's 1588 Hz");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(64, last_dur_on(2), "the merp is not 64 ms long");

    // It holds the pair for a frame, so it re-arms every second one: 134 ms
    // against the cabinet's 128, which is as near as a 67 ms frame gets.
    mark = mock_sound_gate_count();
    run("voices");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(2, mark), "the merp sounded every frame");
    mark = mock_sound_gate_count();
    run("voices");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(2, mark) > 0, "the merp did not come back");

    // Coming free of the cube stops it.
    run("make \"au.bumped false  make \"au.busy 0");
    mark = mock_sound_gate_count();
    run("voices");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(2, mark), "the merp outlived the obstacle");

    // And it takes the pair off the saucer, which is $55c7's order: a dying
    // saucer, then the merp, then the hum.
    run("make \"au.bumped true  make \"au.busy 0");
    foe_at(3, 800, 1000, 180);          // kind 3 is the saucer
    int qmark = queued_count();
    mark = mock_sound_gate_count();
    run("voices");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, queued_count() - qmark, "the saucer hummed over the merp");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1588, last_freq_on(2), "the merp lost the pair to the saucer");
}

// EVERY SWEEP IN THIS FILE IS LINEAR IN POKEY'S DIVISOR AND NOT IN PITCH, and
// the saucer is where it shows: half way up AUDF 64 to 32 is AUDF 48, which is
// 551 Hz -- a fifth above the bottom, where an ear expects the sixth.
void test_the_saucer_sweeps_through_the_divisor_and_not_the_scale(void)
{
    run("make \"au.busy 0");
    int qmark = queued_count();
    run("saucer.hum");
    TEST_ASSERT_TRUE_MESSAGE(queued_count() - qmark >= 4, "the saucer hum is not a sweep");

    // 415, 551, 831 -- the cabinet's 64, 48 and 32 read through 27000/(n+1).
    TEST_ASSERT_UINT32_WITHIN_MESSAGE(6, 415, queued_freq(qmark),
                                      "the hum does not start at the cabinet's 415 Hz");
    TEST_ASSERT_UINT32_WITHIN_MESSAGE(10, 554, queued_freq(qmark + 1),
                                      "the middle of the sweep is not the divisor's midpoint");
    TEST_ASSERT_UINT32_WITHIN_MESSAGE(14, 831, queued_freq(qmark + 2),
                                      "the hum does not reach the cabinet's 818 Hz");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(queued_freq(qmark + 1), queued_freq(qmark + 3),
                                     "the sweep does not come back down the way it went up");
}

// A SWEEP IS A SWEEP, and the cabinet's is 24 steps of its counter.  28.5 ms
// is the shortest note this sequencer will hold, so a 192 ms sweep is seven of
// them and they are the ROM's counter read at seven even places -- a chromatic
// run, which is what the three-boop alert is, and not a chord.
void test_the_enemy_alert_is_a_chromatic_run(void)
{
    run("make \"au.busy 0");
    int qmark = queued_count();
    run("alert");
    // Twenty-one notes into each half of the pair, and the pair's two halves
    // are queued one after the other rather than interleaved.
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, queued_count() - qmark,
                                  "the alert is not three sweeps of seven");

    // Each sweep starts at the cabinet's 415 and climbs without repeating.
    for (int sweep = 0; sweep < 3; sweep++)
    {
        const int base = qmark + sweep * 7;
        TEST_ASSERT_UINT32_WITHIN_MESSAGE(6, 415, queued_freq(base),
                                          "a sweep does not start at 415 Hz");
        for (int i = 1; i < 7; i++)
            TEST_ASSERT_TRUE_MESSAGE(queued_freq(base + i) > queued_freq(base + i - 1),
                                     "the alert does not climb the whole way");
    }
}

// THE POKEY PAIR RELEASES IN ZERO, which is a TEMPO decision and not a timbre
// one: the sequencer starts the next queued note only after the current one
// has finished releasing, so any release at all stretches every sequence in
// the file past the millisecond counts the ROM was read for.  POKEY has no
// envelope -- a sequence there is a register written and written again.
void test_the_pokey_pair_has_no_release_to_stretch_a_sequence(void)
{
    run("setup.sound");
    const MockDeviceState *st = mock_device_get_state();
    for (int v = 2; v <= 6; v += 4)
    {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, st->sound.env[v].release,
                                         "the POKEY pair releases, and every sequence runs long");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, st->sound.env[v].attack,
                                         "the POKEY pair ramps in, and POKEY steps");
    }

    // The engine and the noise pair keep theirs: they are gated, not queued,
    // and the tail is the sound.
    TEST_ASSERT_TRUE_MESSAGE(st->sound.env[0].release > 0, "the engine lost its release");
    TEST_ASSERT_TRUE_MESSAGE(st->sound.env[3].release > 0, "the explosion lost its tail");
}

// The cannon and both explosions are noise, on the pair that is allowed to
// carry it.  LOUDNESS AND LENGTH say what happened and pitch says nothing:
// the cabinet has one noise circuit, a gate bit and a volume bit, and M7's
// sharp crack against a low crump was an invention.
//
// AND THE LOUD ONE IS NOT YOURS.  $6027 hands the $ff counter and the loud
// bit to the unit that was HIT and the $70 and the soft bit to the one that
// fired, so a tank you kill and a shell you put into a wall are the SAME
// sound, and the only long loud explosion in the game is your own death.
void test_the_explosions_differ_in_loudness_and_length(void)
{
    new_game();
    camera_at(800, 800, 0);
    foe_at(1, 800, 1100, 180);
    run("make \"au.busy 0");
    int mark = mock_sound_gate_count();
    run("kill.enemy");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(3, mark) > 0, "the enemy blew up in silence");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(7, mark) > 0, "the explosion is only in one ear");
    const int tank = last_vol_on(3);
    const uint32_t tank_ms = last_dur_on(3);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(448, tank_ms, "a tank you kill is not the ROM's $70");

    // A shell that hit a wall is the same $70 and the same soft bit: the
    // cabinet cannot tell the two apart and neither can this.
    run("make \"sh.on true  make \"sh.life 20  make \"e.alive false");
    run("make \"px 800  make \"pz 150  make \"ph 0  make \"sh.x 800  make \"sh.z 195");
    run("make \"sh.vx 0  make \"sh.vz 5  cam.offsets  ob.scan");
    run("make \"ox [800 800 800 800]  make \"ox se :ox [800 800 800 800]");
    run("make \"oz [200 200 200 200]  make \"oz se :oz [200 200 200 200]");
    run("ob.scan");
    mark = mock_sound_gate_count();
    run("step.shell");
    TEST_ASSERT_FALSE_MESSAGE(truth(":sh.on"), "the shell did not strike the obstacle");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(3, mark) > 0, "the shell struck the wall in silence");
    TEST_ASSERT_EQUAL_INT_MESSAGE(tank, last_vol_on(3), "a wall is not as loud as a tank");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(tank_ms, last_dur_on(3), "a wall is not as long as a tank");

    // Your own death is the $ff, and it is NOT told apart by pitch: the
    // cabinet says which of you it was with the volume and the length, and had
    // no way of saying it any other way.
    const uint32_t shell_hz = last_freq_on(3);
    run("make \"lives 3  make \"tk.boom 0");
    mark = mock_sound_gate_count();
    run("hit.player 15");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(3, mark) > 0, "the player blew up in silence");
    TEST_ASSERT_TRUE_MESSAGE(last_vol_on(3) > tank, "your death is no louder than theirs");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1020, last_dur_on(3), "your death is not the ROM's $ff");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(shell_hz, last_freq_on(3), "pitch is being used to say what died");

    // A missile that RAMS you is the same $ff with the loud bit cleared: as
    // long, and quieter.  It is the one place the two deaths are told apart.
    run("make \"lives 3  make \"tk.boom 0");
    mark = mock_sound_gate_count();
    run("hit.player 8");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1020, last_dur_on(3), "a ram is shorter than a shell");
    TEST_ASSERT_TRUE_MESSAGE(last_vol_on(3) < 15, "a ram is as loud as a shell");
}

// The soft cannon is not a detail.  It is how you hear the ENEMY shoot, and it
// is the only warning you get.
void test_the_cannon_is_softer_when_the_enemy_fires(void)
{
    new_game();
    run("make \"sh.on false  make \"sh.cool 0  make \"tk.boom 0");
    int mark = mock_sound_gate_count();
    run("fire");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(3, mark) > 0, "the cannon made no sound");
    const int mine = last_vol_on(3);

    camera_at(800, 800, 0);
    enemy_at(800, 1100, 180);
    run("make \"es.on false  enemy.fires");
    TEST_ASSERT_TRUE_MESSAGE(truth(":es.on"), "the enemy did not fire");
    TEST_ASSERT_TRUE_MESSAGE(last_vol_on(3) < mine, "the enemy's shot is as loud as your own");
}

// Nine notes of the 1812 Overture over the game-over card, doubled an octave
// up as the ROM doubles them across channels 1 and 2.  It takes the pair
// whatever is on it: the frame loop has stopped by then.
void test_the_game_over_card_plays_the_overture(void)
{
    run("make \"au.busy 30");
    int qmark = queued_count();
    run("overture");
    const int notes = queued_count() - qmark;
    TEST_ASSERT_TRUE_MESSAGE(notes >= 18, "the Overture is not both halves of nine notes");

    // The doubling: the second voice starts an octave above the first, and the
    // first note of the phrase is the cabinet's B.
    const uint16_t low = queued_freq(qmark);
    const uint16_t high = queued_freq(qmark + notes / 2);
    char msg[128];
    snprintf(msg, sizeof(msg), "the two halves open on %u Hz and %u Hz", low, high);
    TEST_ASSERT_TRUE_MESSAGE(high >= low * 2 - 2 && high <= low * 2 + 2, msg);
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
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "ARROWS"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "1 Q left, 0 P right"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "SPACE Fire   Z Pause"), screen);
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

// AND THE OTHER TABLE, WHICH HAS NO MARGIN LEFT AT ALL.  `procedures[]` is
// `MAX_PROCEDURES` slots and this file defines exactly that many, which was
// found the way these things are always found: a 129th `to` was added during
// the polish pass and the test suite failed on the LAST procedure in the file
// rather than on the new one.  That is what overflow looks like from the
// outside -- `proc_define` returns false, the definition is dropped, and the
// name that goes missing is whichever one happened to be at the end.
//
// So the guard reads the source rather than the workspace, and it names the
// count in its message: the next person to want a procedure here has to take
// one back or raise the cap, and raising it costs SRAM on a board where SRAM
// is the scarce thing.  Folding a switch into arithmetic, the way
// `hud.every`'s `15 - :hud.every` does, is the cheap way out and it is the
// one this file took.
void test_the_game_fits_the_procedure_table(void)
{
    FILE *f = fopen(BATTLEZONE_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL(f);

    char line[512];
    int defs = 0;
    while (fgets(line, sizeof(line), f))
        if (repl_line_starts_with_to(line))
            defs++;
    fclose(f);

    char msg[192];
    snprintf(msg, sizeof(msg),
             "battlezone defines %d procedures of %d -- the table overflows and the "
             "LAST definition in the file is the one that silently goes missing",
             defs, MAX_PROCEDURES);
    TEST_ASSERT_TRUE_MESSAGE(defs <= MAX_PROCEDURES, msg);
}

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
    // The backdrop's two culled pieces: the play loop above drives at heading
    // 0, where the moon and the volcano are both off the side and neither
    // reaches the names it mints.  That is the §16.12.4 gap -- a count that is
    // not under budget but under-MEASURED -- so face each of them once.
    run("make \"ph 300  moon  make \"ph 144  volcano");
    run("engine  voices  hit.player 15");

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
        "au.", "cr.", "hs.",
        // M5's one: the missile, which already owned `ms.fin` and now owns the
        // launch counter that promotes a tank to a supertank.
        "ms.",
        // M7's one: the wreck.  It is a family of four and not a `bm.` because
        // the two explosions are two different things -- one is three solids
        // out on the plain and the other is five strokes on the glass -- and a
        // shared prefix would have invited sharing the slots.
        "wr.",
        // The volcano's sparks.  `vp` itself -- the particle list -- is a
        // top-level name and not a temporary, and the temporaries that walk it
        // are all `local` because the table had one slot and it went to the
        // list (section 16.13).
        "vp.", NULL};
    static const char *const state[] = {
        "px", "pz", "ph", "cs", "sn", "a", "b", "apx", "apy",
        "left.tread", "right.tread", "bumped", "paused", "quit",
        "frame.count", "frame.ms", "body.ms", "cpu.at", "cpu.was",
        "max.obstacles", "ox", "oz", "okind",
        "kills", "hits",
        // M3's: what a player wins and loses, the two flags the loops read,
        // and the two `clock` sets from the board's answer.
        "score", "lives", "extra.due", "cracked", "playing", "leaving",
        "fps", "arrows", NULL};

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







// B57.  `modulo` is an INTEGER operation -- the reference's own heading line
// says `modulo integer1 integer2` -- and it truncates, so every position and
// heading in this game was quantised to whole units by the statement that
// wrapped it.  Nothing noticed for five milestones because everything that fed
// one was an integer; M6's tuning is fractional throughout, so the wrap had to
// keep the fraction.
//
// It bit once before it was found: M5 scaled a mild enemy's speed to
// `e.step * 0.6`, which is 3.6, and the enemy moved 3.
void test_the_wrap_keeps_the_fraction(void)
{
    // A whole plain's worth of driving, one tread-step at a time, and the
    // fraction has to survive every one of them.
    camera_at(800, 800, 0);
    press_forward();
    run("pollkeys  step.tank");
    release_forward();
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 800.0f + 2.0f * num(":tread.step"), num(":pz"),
                                     "the plain's wrap threw away the fraction of a step");

    // And across the seam, which is the only place the wrap actually fires.
    camera_at(0.0f, num(":world") - 1.0f, 0.0f);
    press_forward();
    run("pollkeys  step.tank");
    release_forward();
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, 2.0f * num(":tread.step") - 1.0f, num(":pz"),
                                     "the wrap truncated at the seam");

    // The heading is the same story, and it is the one that showed: a one-tread
    // arc turns 0.703 degrees, and an integer wrap makes that exactly nothing.
    camera_at(800, 800, 0);
    press(KEY_LFWD);
    run("pollkeys  step.tank");
    release(KEY_LFWD);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, num(":turn.rate"), num(":ph"),
                                     "the heading wrap threw away a sub-degree turn");
}

// `:HitSomething` at $651a: the tank sets a reverse flag with a random turn
// direction and backs up for 48 frames, and the ROM runs that WITH NO COLLISION
// TEST AT ALL -- which is not sloppiness, it is the only way a tank that
// spawned inside a cube can ever get out of one.
//
// M5 refused the step and kept the turn, and the comment above `move.enemy`
// claimed that was the cabinet's behaviour.  It is not, and an enemy that
// ground against a cube for ever is what it bought.
void test_a_blocked_enemy_backs_out_and_can_leave_a_cube_it_spawned_in(void)
{
    new_game();
    run("rerandom");
    run("make \"ox [800 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [1000 100 100 100]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);

    // Driving straight into the cube.
    foe_at(1, 800, 1000 - num(":coll.r") + 2.0f, 0);
    run("make \"e.rev 0  make \"e.f 1  make \"e.t 0  move.enemy");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.rev") > 0, "a blocked enemy did not start backing out");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.mvc") > 40, "the reverse got no clock");

    // And now it reverses -- turning, and without asking about obstacles.
    const float before = num(":e.z");
    run("make \"e.rage 200  hunt");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-1, num(":e.f"), "the enemy did not reverse");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.t") != 0.0f, "the enemy reversed without turning");
    run("step.enemy");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.z") < before, "the enemy did not actually back up");

    // The case the ROM disables collision for: dead inside the cube, which only
    // a spawn can produce, and it still gets out.
    foe_at(1, 800, 1000, 0);
    run("make \"e.rev 1  make \"e.mvc 48  make \"e.rage 200  make \"e.alive true");
    for (int i = 0; i < 40; i++)
        run("step.enemy");
    run("enemy.offsets");
    run("make \"tk.dx :e.dx  make \"tk.dz :e.dz  make \"tk.guard :coll.r");
    TEST_ASSERT_FALSE_MESSAGE(truth("blocked?"),
                              "an enemy that spawned inside a cube never got out of it");
}

// The ROM's missile spawns at altitude $1800 and, on touching anything, is put
// back where it was and climbs -- so it hops obstacles and you cannot hide from
// one behind a cube.  A saucer flies over them for a different reason and both
// end up in the same branch of `move.enemy`.
void test_a_missile_hops_the_obstacles_and_a_tank_does_not(void)
{
    new_game();
    run("make \"ox [800 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [900 100 100 100]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);

    foe_at(2, 800, 880, 0);
    const float before = num(":e.z");
    run("make \"e.rev 0  make \"e.f 1  move.enemy");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.z") > before, "a missile was stopped by a cube");

    foe_at(1, 800, 880, 0);
    const float tank_before = num(":e.z");
    run("make \"e.rev 0  make \"e.f 1  move.enemy");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(tank_before, num(":e.z"),
                                    "a tank drove through the cube the missile hopped");
}

// The blip is refreshed to full only on the frame the sweep line crosses the
// enemy's bearing ($6b5d) and decays from there, so it flares about once a
// revolution and is dark in between.  That is a MECHANIC: between pings the
// player has to carry the enemy's position themselves.  M5 drew a steady dot,
// which is a tracking radar the cabinet never gave anybody.
void test_the_blip_pings_on_the_sweep_and_then_goes_dark(void)
{
    camera_at(800, 800, 0);
    enemy_at(800, 1000, 180);
    run("make \"rd.bi 0  make \"rd.sw 0");

    // The sweep is nowhere near it: nothing is drawn and nothing is armed.
    run("make \"rd.sw 180  blip");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":rd.bi"), "the blip armed with the sweep elsewhere");
    mock_device_clear_graphics();
    run("make \"rd.sw 180  blip");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(), "a dark blip still drew");

    // The sweep crosses it -- dead ahead is bearing zero -- and it lights up.
    run("make \"rd.sw 0  blip");
    TEST_ASSERT_TRUE_MESSAGE(num(":rd.bi") > 0, "the sweep crossed the enemy and nothing pinged");

    // Then it fades, and is gone well before the sweep comes round again.
    mock_device_clear_graphics();
    run("make \"rd.sw 180  blip");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > 0, "the blip went out immediately");
    for (int i = 0; i < 40; i++)
        run("make \"rd.sw 180  blip");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":rd.bi"), "the blip never faded");
    mock_device_clear_graphics();
    run("make \"rd.sw 180  blip");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(), "a faded blip still drew");
}

// B58, reported from a board: "a tank was close, almost on top of me, and it
// kept firing in fast repetition, screen cracked, but I did not die."
//
// THE PLAYER IS NEVER DEAD LONG ENOUGH TO DIE.  `hit.player` re-arms
// `tk.boom` to `boom.frames` on every hit, and `step.tank` only calls
// `respawn` when that counter reaches zero -- so a tank at point-blank range,
// firing faster than ten frames, refreshes the pause before it can expire.
// The player sits in a permanent death animation with the glass cracked,
// `lives` going arbitrarily negative, and the game unable to end.
//
// THE ROM HAS THREE GATES HERE AND M6 HAD NONE OF THEM:
//
//  * `TryShootPlayer` at $65cd reads `unit_state` and will not fire at a
//    player who is already dying.
//  * `TestProjCollU` at $5f6f reads the same byte and will not let a shell
//    already in the air hit one either.
//  * a projectile that strikes anything goes to $80 (a unit) or $a0 (an
//    obstacle) and spends about six frames EXPLODING, during which $65c9
//    refuses to fire again.  That is the cabinet's rate limiter at point-blank
//    range, and it is not a reload -- it is the shell still being on screen.
void test_a_point_blank_tank_cannot_hold_the_player_in_a_permanent_death(void)
{
    new_game();
    run("rerandom");
    // The obstacles well out of the way, so nothing but the tank is in play.
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);

    // Right on top of the player, aimed, loaded, and out of patience.
    foe_at(1, 800, 845, 180);
    run("make \"lives 1  make \"e.cool 0  make \"e.rage 0  make \"tk.boom 0");
    run("make \"playing true  make \"paused false  make \"quit false  make \"e.boom 0");

    for (int i = 0; i < 200 && truth(":playing"); i++)
        run("play.frame");

    char msg[160];
    snprintf(msg, sizeof(msg),
             "200 frames at point-blank range and the game never ended -- lives %g, "
             "tk.boom %g: the death pause is being re-armed faster than it runs down",
             (double)num(":lives"), (double)num(":tk.boom"));
    TEST_ASSERT_FALSE_MESSAGE(truth(":playing"), msg);

    // And the last life is spent once, not many times over.
    snprintf(msg, sizeof(msg), "the player lost %g lives from one death", (double)(1.0 - num(":lives")));
    TEST_ASSERT_TRUE_MESSAGE(num(":lives") >= 0.0f, msg);
}

// The gate on its own: a dying player is not shot at, which is $65cd.
void test_the_enemy_does_not_fire_at_a_dying_player(void)
{
    new_game();
    camera_at(800, 800, 0);
    foe_at(1, 800, 845, 180);
    run("make \"e.cool 0  make \"e.rage 0  make \"e.mvc 1  make \"tk.boom 0  hunt");
    TEST_ASSERT_TRUE_MESSAGE(truth(":e.fire"), "the enemy would not shoot a live player at point blank");

    run("make \"e.cool 0  make \"e.mvc 1  make \"tk.boom 5  hunt");
    TEST_ASSERT_FALSE_MESSAGE(truth(":e.fire"), "the enemy shot at a player who was already dying");
}

// And a shell already in the air cannot land on one either, which is $5f6f.
// Without this a single shot fired the frame before a death still re-arms the
// pause after it.
void test_a_shell_in_the_air_cannot_hit_a_dying_player(void)
{
    new_game();
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);
    foe_at(1, 800, 900, 180);
    run("make \"hits 0  make \"tk.boom 0  make \"es.on false  enemy.fires");
    TEST_ASSERT_TRUE(truth(":es.on"));

    // The player dies to something else on the way.
    run("make \"tk.boom 8  make \"lives 3");
    for (int i = 0; i < 8 && truth(":es.on"); i++)
        run("step.eshell");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":hits"), "a shell landed on a player who was already dying");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":lives"), "a dying player lost a second life to a shell in flight");
}

// The rate limiter, and it is the shell rather than a timer: a shot that
// STRIKES something leaves an explosion behind, and the enemy cannot fire
// through it.  A shot that simply runs out of life leaves nothing.
void test_a_shell_that_strikes_something_holds_the_gun_shut(void)
{
    new_game();
    run("make \"ox [100 100 100 100]  make \"ox se :ox [100 100 100 100]");
    run("make \"oz [100 100 100 100]  make \"oz se :oz [100 100 100 100]");
    camera_at(800, 800, 0);
    foe_at(1, 800, 900, 180);

    // Struck the player: the gun is shut for a few frames afterwards.
    run("make \"hits 0  make \"tk.boom 0  make \"e.cool 0  make \"es.on false  enemy.fires");
    for (int i = 0; i < 8 && truth(":es.on"); i++)
        run("step.eshell");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":hits"), "the shell never arrived");
    TEST_ASSERT_TRUE_MESSAGE(num(":e.cool") > 0, "a shell that hit the player left no explosion behind");

    // Ran out of life instead: nothing is left and the gun is free.
    run("make \"tk.boom 0  make \"e.cool 0  make \"es.on false  enemy.fires");
    run("make \"es.life 1  step.eshell");
    TEST_ASSERT_FALSE(truth(":es.on"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":e.cool"),
                                    "a shell that expired left an explosion behind it");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_file_loads_and_sets_its_tuning);
    RUN_TEST(test_the_whole_field_is_drawable_with_no_cap);
    RUN_TEST(test_the_projection_is_right_at_a_heading_that_is_not_zero);
    RUN_TEST(test_turning_right_sweeps_the_world_to_the_left);
    RUN_TEST(test_an_object_behind_the_camera_is_culled);
    RUN_TEST(test_nothing_is_culled_for_being_close);
    RUN_TEST(test_a_column_inside_the_near_plane_is_floored);
    RUN_TEST(test_the_view_cone_margin_covers_every_object);
    RUN_TEST(test_only_the_view_cone_culls);
    RUN_TEST(test_a_cube_beside_the_tank_is_still_drawn);
    RUN_TEST(test_the_two_projections_agree_on_their_columns);
    RUN_TEST(test_the_floor_bounds_how_big_a_cube_can_get);
    RUN_TEST(test_the_plain_wraps_in_the_arithmetic);
    RUN_TEST(test_the_far_plane_is_inside_the_wrap);
    RUN_TEST(test_driving_across_the_seam_keeps_the_camera_on_the_plain);
    RUN_TEST(test_a_cube_draws_twelve_edges);
    RUN_TEST(test_a_pyramid_draws_eight_edges);
    RUN_TEST(test_the_gunsight_is_a_fixed_overlay);
    RUN_TEST(test_no_part_of_the_gunsight_lies_along_the_horizon);
    RUN_TEST(test_the_resting_sight_is_the_world_and_the_locked_one_is_not);
    RUN_TEST(test_the_locked_sight_turns_its_teeth_toward_the_middle);
    RUN_TEST(test_both_sights_are_symmetric_about_the_aiming_point);
    RUN_TEST(test_the_sight_locks_on_a_bearing_and_not_a_range);
    RUN_TEST(test_what_the_sight_locks_is_inside_its_gap);
    RUN_TEST(test_the_sight_does_not_lock_on_what_is_behind_you);
    RUN_TEST(test_a_dead_enemy_does_not_lock_the_sight);
    RUN_TEST(test_the_frame_draws_the_form_the_bearing_asks_for);
    RUN_TEST(test_the_field_is_on_the_plain_and_clear_of_the_start);
    RUN_TEST(test_nothing_in_view_is_culled_by_distance);
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
    RUN_TEST(test_the_vent_is_the_craters_own_notch);
    RUN_TEST(test_the_volcano_throws_sparks_out_of_its_crater);
    RUN_TEST(test_a_volcano_behind_you_costs_nothing);
    RUN_TEST(test_a_spark_arcs_over_and_dies_on_the_ground);
    RUN_TEST(test_a_spark_cools_as_it_flies);
    RUN_TEST(test_each_key_drives_its_own_tread);
    RUN_TEST(test_one_key_arcs_and_two_keys_pivot);
    RUN_TEST(test_driving_forward_moves_along_the_heading);
    RUN_TEST(test_the_arrows_drive_and_steer);
    RUN_TEST(test_the_arrow_sum_clamps_to_one_tread);
    RUN_TEST(test_each_scheme_ignores_the_other_scheme_s_keys);
    RUN_TEST(test_fire_pause_and_quit_are_the_same_in_both_schemes);
    RUN_TEST(test_the_attract_screen_picks_the_steering);

    RUN_TEST(test_you_can_drive_up_to_an_obstacle_and_not_up_to_two_of_them);
    RUN_TEST(test_you_cannot_drive_close_enough_for_an_obstacle_to_vanish);
    RUN_TEST(test_a_blocked_tank_can_still_turn);
    RUN_TEST(test_a_frame_runs_and_draws_the_scene);
    RUN_TEST(test_a_long_run_of_frames_reclaims);
    RUN_TEST(test_a_paused_frame_neither_drives_nor_draws);
    RUN_TEST(test_quit_ends_the_loop);
    RUN_TEST(test_the_frame_timer_brackets_the_present);
    RUN_TEST(test_the_readout_is_off_in_a_shipped_game);
    RUN_TEST(test_the_frame_does_not_tally_while_the_readout_is_off);
    RUN_TEST(test_d_toggles_the_readout_both_ways);
    RUN_TEST(test_the_readout_is_averaged_over_a_second);
    RUN_TEST(test_the_readout_keeps_a_peak_not_an_average_of_peaks);
    RUN_TEST(test_the_enemy_draws_thirteen_edges);
    RUN_TEST(test_the_gun_points_where_the_enemy_faces);
    RUN_TEST(test_the_turret_sits_on_the_hull_and_inside_it);
    RUN_TEST(test_the_enemy_hull_is_a_square_that_turns);
    RUN_TEST(test_every_part_of_a_tank_lands_on_the_tank);
    RUN_TEST(test_a_shell_is_a_dart_pointing_where_it_flies);
    RUN_TEST(test_ZZDIAG);
    RUN_TEST(test_a_shell_draws_eight_edges);
    RUN_TEST(test_the_enemy_is_drawn_right_into_your_face);
    RUN_TEST(test_the_enemy_turns_towards_the_player);
    RUN_TEST(test_the_enemy_acts_every_frame_and_decides_on_a_counter);
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
    RUN_TEST(test_the_enemy_fires_exactly_along_its_heading);
    RUN_TEST(test_a_still_player_is_hit_and_a_moving_one_is_missed);
    RUN_TEST(test_the_enemys_shell_hits_the_player_and_pauses_the_tank);
    RUN_TEST(test_the_enemys_worst_shot_misses_at_range_and_kills_up_close);
    RUN_TEST(test_a_shell_that_goes_by_you_is_a_shell_that_missed);
    RUN_TEST(test_your_shell_kills_what_the_gunsight_covers);
    RUN_TEST(test_the_players_explosion_draws_its_fragments_and_runs_down);
    RUN_TEST(test_the_wreck_is_the_tank_in_three_pieces_and_runs_down);
    RUN_TEST(test_the_wreck_flies_apart_in_the_world);
    RUN_TEST(test_a_wreck_at_your_feet_stays_on_the_arithmetic);
    RUN_TEST(test_each_kind_comes_apart_into_its_own_shape);
    RUN_TEST(test_a_saucer_explodes_where_it_was_flying_and_falls);
    RUN_TEST(test_the_wreck_is_finished_before_the_next_enemy_spawns);
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
    RUN_TEST(test_the_plain_opens_with_tanks_and_nothing_else);
    RUN_TEST(test_a_saucer_waits_for_two_thousand_and_a_missile_for_twenty);
    RUN_TEST(test_six_missiles_promote_the_tank_to_a_supertank);
    RUN_TEST(test_every_missile_launch_is_counted);
    RUN_TEST(test_a_tank_you_drive_away_from_is_replaced_by_a_missile);
    RUN_TEST(test_a_dodged_missile_is_followed_by_another_until_the_cycle_ends);
    RUN_TEST(test_the_enemy_keeps_score_and_the_difference_is_the_difficulty);
    RUN_TEST(test_a_mild_enemy_drives_somewhere_else_at_full_speed);
    RUN_TEST(test_no_row_is_rewritten_on_the_way_in);
    RUN_TEST(test_seventeen_seconds_makes_a_mild_enemy_aggressive);
    RUN_TEST(test_a_missile_that_gets_past_you_has_been_dodged);
    RUN_TEST(test_a_spawn_is_at_one_of_two_distances);
    RUN_TEST(test_a_new_tank_arrives_pointing_anywhere_and_a_missile_at_you);
    RUN_TEST(test_a_losing_player_gets_the_enemy_in_front_of_them);
    RUN_TEST(test_two_spawns_running_are_not_the_same_place);
    RUN_TEST(test_a_missile_comes_from_the_far_point_and_from_in_front);
    RUN_TEST(test_a_saucer_takes_a_heading_of_its_own);
    RUN_TEST(test_nothing_fires_for_two_seconds_after_a_spawn);
    RUN_TEST(test_the_enemy_is_confused_for_three_seconds_after_you_respawn);
    RUN_TEST(test_the_missiles_final_turn_comes_later_as_the_score_climbs);
    RUN_TEST(test_a_missile_swerves_until_its_final_turn);
    RUN_TEST(test_the_radar_does_not_show_a_saucer);
    RUN_TEST(test_every_kind_sets_a_whole_row);
    RUN_TEST(test_each_kind_is_worth_its_arcade_score);
    RUN_TEST(test_a_supertank_outclasses_a_tank);
    RUN_TEST(test_a_missile_closes_forever_and_never_fires);
    RUN_TEST(test_a_missile_kills_by_arriving_and_dies_of_it);
    RUN_TEST(test_a_missile_that_has_not_arrived_does_nothing);
    RUN_TEST(test_a_missile_draws_twelve_edges);
    RUN_TEST(test_a_missile_flies_at_eye_height);
    RUN_TEST(test_a_saucer_drifts_and_does_not_hunt);
    RUN_TEST(test_a_saucer_flies_over_the_obstacles);
    RUN_TEST(test_a_saucer_leaves_when_its_dwell_runs_out);
    RUN_TEST(test_a_saucer_draws_twelve_edges);
    RUN_TEST(test_a_saucers_outline_does_not_turn_with_its_heading);
    RUN_TEST(test_a_saucer_floats_above_the_horizon);
    RUN_TEST(test_a_saucer_stays_where_the_gunsight_can_reach_it);
    RUN_TEST(test_the_new_models_are_culled_by_the_view_cone);
    RUN_TEST(test_the_frame_draws_the_model_that_matches_the_kind);
    RUN_TEST(test_a_kill_scores_what_the_enemy_is_worth);
    RUN_TEST(test_two_bonus_tanks_a_game_and_never_a_third);
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
    RUN_TEST(test_the_engine_revs_up_and_back_down);
    RUN_TEST(test_the_radar_pings_when_the_sweep_crosses_the_blip);
    RUN_TEST(test_the_enemy_alert_sounds_once_for_each_enemy);
    RUN_TEST(test_the_pokey_pair_carries_one_sound_at_a_time);
    RUN_TEST(test_the_missile_buzzes_and_the_buzz_follows_the_range);
    RUN_TEST(test_running_into_an_obstacle_sounds_once);
    RUN_TEST(test_leaning_on_an_obstacle_loops_the_merp);
    RUN_TEST(test_the_saucer_sweeps_through_the_divisor_and_not_the_scale);
    RUN_TEST(test_the_enemy_alert_is_a_chromatic_run);
    RUN_TEST(test_the_pokey_pair_has_no_release_to_stretch_a_sequence);
    RUN_TEST(test_the_explosions_differ_in_loudness_and_length);
    RUN_TEST(test_the_cannon_is_softer_when_the_enemy_fires);
    RUN_TEST(test_the_game_over_card_plays_the_overture);
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

    RUN_TEST(test_the_wrap_keeps_the_fraction);
    RUN_TEST(test_a_blocked_enemy_backs_out_and_can_leave_a_cube_it_spawned_in);
    RUN_TEST(test_a_missile_hops_the_obstacles_and_a_tank_does_not);
    RUN_TEST(test_the_blip_pings_on_the_sweep_and_then_goes_dark);
    RUN_TEST(test_a_point_blank_tank_cannot_hold_the_player_in_a_permanent_death);
    RUN_TEST(test_the_enemy_does_not_fire_at_a_dying_player);
    RUN_TEST(test_a_shell_in_the_air_cannot_hit_a_dying_player);
    RUN_TEST(test_a_shell_that_strikes_something_holds_the_gun_shut);
    RUN_TEST(test_the_game_fits_the_procedure_table);
    RUN_TEST(test_the_game_fits_the_global_table_with_room_to_spare);
    RUN_TEST(test_every_hot_path_temporary_is_prefixed);
    return UNITY_END();
}
