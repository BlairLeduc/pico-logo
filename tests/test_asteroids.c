//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the Asteroids game (logo/games/asteroids), M4: the saucer and
//  the sound, on top of the rocks, the ship, the shots and the lives.
//
//  The game is pure Logo; this exercises it the two ways test_galaxian.c does:
//  loading the whole file proves it parses and that the init path runs on the
//  mock device, and the pure logic (wrap, slot allocation, the split table,
//  the outline walks) is checked directly, since that is where the bugs would
//  hide.
//
//  The frame budget is what every milestone is really about, and no host test
//  can answer it -- that needs logo/tests/p11m4 on a board.  What these tests can hold is
//  everything the budget assumes: that a frame draws the world and nothing
//  else, that the frame loop holds free storage flat, that the outlines carry
//  the segment counts the budget was cut from, and that the split table can
//  never write past the slot count the worst case is bounded by.
//
//  The M0 harness tests are in test_p11rocks.c, a separate binary because
//  both files define `place` and `draw.rock`.
//

#include "test_mock_fs.h"
#include "mock_device.h"
#include "core/repl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ASTEROIDS_SOURCE
#error "ASTEROIDS_SOURCE must be defined (path to logo/games/asteroids)"
#endif

#ifndef P11M4_SOURCE
#error "P11M4_SOURCE must be defined (path to logo/tests/p11m4)"
#endif

// Segments per outline, from the design's section 6.3 table. Statements per
// draw are 15/13/11: four for the prologue, then two per segment less the
// turn after the last one.
#define SEG_LARGE  6
#define SEG_MEDIUM 5
#define SEG_SMALL  4

// The ship (section 6.4): a notched triangle, and the same hull with the
// thrust flame folded into the one closed walk.
#define SEG_SHIP   4
#define SEG_FLAME  6

// The saucer (section 9): dome, rim and hull as one closed walk, the same at
// both sizes -- a small saucer is a scaled shape and not a simpler one.
#define SEG_SAUCER 8

// PicoCalc key codes, as the two shipped shooters use them.
#define KEY_LEFT   "\264"
#define KEY_RIGHT  "\267"
#define KEY_THRUST "\265"
#define KEY_HYPER  "\266"

// Load a whole Logo file, defining its procedures and running its top-level
// tuning `make`s. Procedure definitions are not handled by the bare
// evaluator, so we buffer them and hand them to proc_define_from_text the way
// the `load` primitive does.
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
    // _and_hardware gives a controllable clock, which `sync` and the frame
    // pacing need; the mock filesystem is unused here but keeps the scaffold
    // the same shape as the other game tests.
    test_scaffold_setUp_with_device_and_hardware();
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
    load_file(ASTEROIDS_SOURCE);
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

// An element of a flat list is a word until something does arithmetic on it,
// so read one through a `0 +` the way the game's own comparisons coerce it.
static float item_of(const char *list, int i)
{
    char expr[64];
    snprintf(expr, sizeof(expr), "0 + item %d :%s", i, list);
    return num(expr);
}

// Most tests want a ship that is on the field and hittable. A new ship now
// waits for its spawn point to clear (M4), so getting there means landing it
// rather than clearing an invulnerability countdown.
static void land_the_ship(void)
{
    Result r = run_string("make \"waiting false  make \"ship.rad :ship.rad.hull  "
                          "make \"shipcx :shipx  make \"shipcy :shipy");
    TEST_ASSERT_TRUE(r.status == RESULT_NONE || r.status == RESULT_OK);
}

static void run(const char *input)
{
    Result r = run_string(input);
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, input);
}

// Segments the live rocks should draw between them, straight from `rsize`.
static int expected_segments(void)
{
    int max = (int)num(":max.rocks");
    int total = 0;
    for (int i = 1; i <= max; i++)
    {
        switch ((int)item_of("rsize", i))
        {
        case 3: total += SEG_LARGE;   break;
        case 2: total += SEG_MEDIUM;  break;
        case 1: total += SEG_SMALL;   break;
        default: break;
        }
    }
    return total;
}

//==========================================================================
// The file loads
//==========================================================================

void test_file_loads_and_sets_its_tuning(void)
{
    TEST_ASSERT_EQUAL_FLOAT(12, num(":max.rocks"));
    // Three, so that 3 -> 6 -> 12 fills the slot count exactly (§13).
    TEST_ASSERT_EQUAL_FLOAT(3, num(":start.rocks"));
    TEST_ASSERT_EQUAL_FLOAT(254, num(":rock.colour"));

    // Eight parallel lists, all MAX.ROCKS long. A list edited to a different
    // length is a silent out-of-range read rather than a visible defect.
    const char *lists[] = {"rx", "ry", "rdx", "rdy", "rang", "rspin", "rsize", "rrad"};
    for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "count :%s", lists[i]);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(12, num(expr), lists[i]);
    }

    // `slife` is MAX.SHOTS long for the same reason. The sampled positions are
    // deliberately NOT a list -- the rock pass reads them three times a rock,
    // and an `item` walk costs two and a half times an arithmetic statement.
    TEST_ASSERT_EQUAL_FLOAT(3, num(":max.shots"));
    TEST_ASSERT_EQUAL_FLOAT(3, num("count :slife"));

    // The rate lives in one place, because every per-frame constant is cut
    // from it and `play.level` asks `sync` for it.
    TEST_ASSERT_EQUAL_FLOAT(14, num(":fps"));
}

// A shot lives about 1.2 seconds and a rock drifts about 13.5 steps a second
// however the rate is set -- those are the quantities the player feels, and the
// per-frame constants are only the rate's arithmetic on them. When `fps` moved
// 15 -> 14 every one of them had to move with it, and this is what catches the
// one that gets missed.
void test_the_per_frame_constants_are_cut_from_the_frame_rate(void)
{
    float fps = num(":fps");

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.6f, 13.5f, num(":speed.l") * fps,
                                     "rock drift is no longer 13.5 steps a second");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(10.0f, 240.0f, num(":turn.rate") * fps,
                                     "the ship no longer turns 240 degrees a second");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(3.0f, 67.0f, num(":speed.max") * fps,
                                     "the speed clamp is no longer about 67 steps a second");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 1.2f, num(":shot.life") / fps,
                                     "a shot no longer lives about 1.2 seconds");

    // `spin.max` is the one that was declared and never read until the rate
    // moved and it would have been re-cut with no effect. `spawn.rock` scales
    // its spin from it, so a rock never spins faster than this.
    run("clear.rocks");
    run("repeat :max.rocks [spawn.rock 3]");
    for (int i = 1; i <= 12; i++)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "rock %d spins %g, past spin.max",
                 i, item_of("rspin", i));
        TEST_ASSERT_TRUE_MESSAGE(fabsf(item_of("rspin", i)) <= num(":spin.max") + 0.001f, msg);
    }
}

// The one constant in this game that is not per-frame: `setspeed` is turtle
// steps per SECOND, because the engine flies the shot on wall-clock time. This
// is the hazard P10's log flagged for Galaxian, arriving from the other
// direction, and it has a hard bound rather than a feel.
//
// Collisions are sampled once a frame, so if the shot and the rock CLOSE by
// more than the full width of the rock's box between two samples, the shot
// passes through and is seen on neither side:
//
//     (shot travel + fastest rock) * overrun  <=  2 * smallest half-width
//
// Every term comes from a constant in the game file. The fastest rock is a
// small one from a split of a split: a child leaves at sqrt(boost^2 + kick^2)
// times its parent and the kick reaches 1, so two splits multiply `speed.l` by
// (boost^2 + 1). The overrun allowance covers a frame that misses its budget,
// which moves the shot further rather than less far.
//
// This bound was first written at HALF its true value -- `travel <= rrad` --
// which is safe but blunt, and the `shot.reach` it demanded made the game
// award misses as hits, worst on the smallest rocks. Sized properly the box is
// 10 rather than 12 and no shot speed had to change.
void test_a_shot_cannot_outrun_the_smallest_collision_box(void)
{
    float travel = num(":shot.speed") / num(":fps");
    float boost = num(":split.boost");
    float rock = num(":speed.l") * (boost * boost + 1.0f);   // two splits, full kick
    float closing = (travel + rock) * 1.3f;                  // a 30 % frame overrun
    float box = 2.0f * num("rad.for 1");

    char msg[192];
    snprintf(msg, sizeof(msg),
             "shot and rock close by %.1f steps a frame against a %.0f-step box -- "
             "raise shot.reach or lower shot.speed, or shots will pass through "
             "small rocks", closing, box);
    TEST_ASSERT_TRUE_MESSAGE(closing <= box, msg);
}

// The other side of the same constant, and the one a player notices: a box far
// wider than the rock drawn inside it awards misses as hits. `shot.reach` is a
// flat number added to radii of 22, 14 and 8, so the excess is worst on the
// smallest rock -- which is exactly where it was reported from the board.
void test_the_collision_boxes_are_not_far_wider_than_the_rocks_drawn_in_them(void)
{
    const struct { int size; float drawn; } rocks[] = {{3, 22}, {2, 14}, {1, 8}};
    for (size_t k = 0; k < sizeof(rocks) / sizeof(rocks[0]); k++)
    {
        char expr[32], msg[160];
        snprintf(expr, sizeof(expr), "rad.for %d", rocks[k].size);
        float box = num(expr);
        float excess = (box - rocks[k].drawn) / rocks[k].drawn;
        snprintf(msg, sizeof(msg),
                 "size %d has a %.0f-step box around a %.0f-step outline, %.0f%% over -- "
                 "shots will land on visible misses", rocks[k].size, box,
                 rocks[k].drawn, excess * 100.0f);
        TEST_ASSERT_TRUE_MESSAGE(excess <= 0.30f, msg);
    }
}

// The same test for the box a SHIP is tested against, and it is the constant
// M3's play report sent back: at `ship.rad` 10 the ship died before rocks
// reached it, worst on the medium and small ones -- the identical failure
// `shot.reach` produced at M2, from the identical cause.
//
// The ship is a thin triangle and not a disc: its rear corners are 12.68 steps
// from the centre, its beam is 8.95, and most bearings meet the beam. The rock
// side is its longest spike, because that is the part a player watches come in.
// So the box must not exceed spike + beam -- above that it kills at a distance
// the two drawn shapes could not have closed on any bearing.
//
// It also must not be cut to nothing: below the beam alone, a rock could sit on
// the hull without killing, which reads as badly as the reverse.
void test_the_ship_box_is_not_wider_than_the_shapes_it_is_drawn_from(void)
{
    // Longest vertex radius of each outline, from scripts/gen_rocks.py's walks.
    const struct { int size; float spike; } rocks[] = {{3, 21.68f}, {2, 13.24f}, {1, 7.49f}};
    const float beam = 8.95f;        // ship half-width across the beam
    float ship = num(":ship.rad");

    for (size_t k = 0; k < sizeof(rocks) / sizeof(rocks[0]); k++)
    {
        char expr[32], msg[192];
        snprintf(expr, sizeof(expr), "rad.for %d", rocks[k].size);
        float box = num(expr) + ship;
        float contact = rocks[k].spike + beam;

        snprintf(msg, sizeof(msg),
                 "size %d kills at %.1f steps where the drawn shapes touch at %.1f -- "
                 "lower ship.rad, or the game kills on visible misses",
                 rocks[k].size, box, contact);
        TEST_ASSERT_TRUE_MESSAGE(box <= contact, msg);

        snprintf(msg, sizeof(msg),
                 "size %d kills at %.1f steps, inside the ship's own beam at %.1f -- "
                 "a rock can sit on the hull without killing", rocks[k].size, box, beam);
        TEST_ASSERT_TRUE_MESSAGE(box > beam, msg);
    }
}

//==========================================================================
// wrapc
//==========================================================================

void test_wrapc_wraps_at_both_edges(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0, num("wrapc 0"));
    TEST_ASSERT_EQUAL_FLOAT(100, num("wrapc 100"));
    TEST_ASSERT_EQUAL_FLOAT(-159, num("wrapc 161"));
    TEST_ASSERT_EQUAL_FLOAT(159, num("wrapc -161"));

    // Exactly on the boundary is inside it: the tests are `>` and `<`, so a
    // rock centred on 160 stays there rather than flipping every frame.
    TEST_ASSERT_EQUAL_FLOAT(160, num("wrapc 160"));
    TEST_ASSERT_EQUAL_FLOAT(-160, num("wrapc -160"));
}

// One correction, not a modulo. Beyond a full width it lands out of bounds,
// and that is the documented contract rather than an oversight: a rock moves
// about 0.9 steps a frame, so it can only ever be a step or two outside.
// `wrapc` is on the hottest path in the game and two failed comparisons are
// what it costs; a `modulo` would cost more on every rock on every frame to
// handle a case the physics cannot produce.
void test_wrapc_corrects_once_and_only_once(void)
{
    TEST_ASSERT_EQUAL_FLOAT(180, num("wrapc 500"));
}

//==========================================================================
// The outlines
//==========================================================================

// A rock is authored as radii and converted to a turtle walk by
// scripts/gen_rocks.py, because hand-written turns do not close. Walk each
// outline at the origin and check it arrives back at the vertex it started
// from -- the one property of a pasted-in block of literals that a bad paste
// would break, and an unclosed rock has a gap that reads as broken.
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
    assert_outline_closes("rock.l", SEG_LARGE);
    assert_outline_closes("rock.m", SEG_MEDIUM);
    assert_outline_closes("rock.s", SEG_SMALL);
}

// The ship comes off the same generator and gets the same guarantee. The
// flame version is one closed walk and not a hull plus a separate flame,
// which is what saves a second `pu setx sety seth` on every thrust frame --
// so if it stopped closing, the ship would have a hole in it rather than a
// flame that did not line up.
void test_both_ship_outlines_close_on_themselves(void)
{
    assert_outline_closes("ship", SEG_SHIP);
    assert_outline_closes("ship.flame", SEG_FLAME);
}

// The ship is deliberately about the size of a medium rock. A smaller one was
// tried on a board and rejected: it reads tidier and it makes the game easier,
// because the thing the rocks have to hit is the thing you are steering. What
// this holds is the size class, not the exact walk -- bigger than a small rock
// and under a medium's radius, so the playfield keeps room in it.
void test_the_ship_is_smaller_than_a_large_rock(void)
{
    run("clean  setpc 254  pu setx 0 sety 0 seth 0");
    mock_device_clear_graphics();
    run("ship");

    // How far the hull reaches from the ship's centre, over every vertex.
    float reach = 0.0f;
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        float d = sqrtf(l->x1 * l->x1 + l->y1 * l->y1);
        if (d > reach)
            reach = d;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "the ship reaches %.1f steps from its centre", reach);
    TEST_ASSERT_TRUE_MESSAGE(reach < 14.0f, msg);   // under a medium rock's radius
    TEST_ASSERT_TRUE_MESSAGE(reach > 7.0f, msg);    // and still bigger than a small one
}

// The prologue walks from the rock's stored centre out to its first vertex
// with the pen up, which is what lets the stored position mean the centre.
// If it ever drew, every rock would wear a spoke.
void test_the_walk_out_to_the_first_vertex_does_not_draw(void)
{
    run("clean  setpc 254  pu setx 0 sety 0 seth 0");
    mock_device_clear_graphics();
    run("rock.l");
    TEST_ASSERT_EQUAL_INT(SEG_LARGE, mock_device_line_count());
    // The first vertex is 21.4 steps straight ahead of the centre.
    const MockLine *first = mock_device_get_line(0);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, first->x1);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.4f, first->y1);
}

// One outline per size, reached by a single three-way test. M0 priced the
// nine-outline version's lookup at 370 us a rock, a fifth of what a rock
// costs, which is why there is one per size to reach -- and M2 took the size
// itself as an argument, since the rock pass has already walked `rsize` to find
// out the slot is live and an `item` walk is the most expensive thing in that
// loop that is not a drawing statement.
void test_draw_rock_picks_the_outline_for_the_size(void)
{
    const int sizes[] = {3, 2, 1};
    const int segs[] = {SEG_LARGE, SEG_MEDIUM, SEG_SMALL};
    for (int k = 0; k < 3; k++)
    {
        char cmd[96];
        run("clean  setpc 254  pu setx 0 sety 0 seth 0");
        mock_device_clear_graphics();
        snprintf(cmd, sizeof(cmd), "draw.rock %d", sizes[k]);
        run(cmd);
        snprintf(cmd, sizeof(cmd), "size %d drew %d segments", sizes[k],
                 mock_device_line_count());
        TEST_ASSERT_EQUAL_INT_MESSAGE(segs[k], mock_device_line_count(), cmd);
    }
}

// A rock spins every frame and its angle is never normalised -- one `if` a
// rock a frame is 0.8 ms at twelve rocks, and `seth` is documented to take
// any heading. If it did not, the game would break after a couple of minutes
// rather than at once, which is the worst way for it to break.
void test_a_rock_angle_past_360_still_places(void)
{
    run("init.game  clear.rocks  clear.shots");
    run(".setitem 1 :rsize 3  .setitem 1 :rx 0  .setitem 1 :ry 0");
    run(".setitem 1 :rdx 0  .setitem 1 :rdy 0  .setitem 1 :rspin 0");

    run(".setitem 1 :rang 0");
    mock_device_clear_graphics();
    run("step.draw.all");
    TEST_ASSERT_EQUAL_INT(SEG_LARGE, mock_device_line_count());
    float x0 = mock_device_get_line(0)->x1;
    float y0 = mock_device_get_line(0)->y1;

    run(".setitem 1 :rang 3600");   // ten full turns on
    mock_device_clear_graphics();
    run("step.draw.all");
    TEST_ASSERT_EQUAL_INT(SEG_LARGE, mock_device_line_count());
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.05f, x0, mock_device_get_line(0)->x1,
                                     "a heading past 360 drew the rock somewhere else");
    TEST_ASSERT_FLOAT_WITHIN(0.05f, y0, mock_device_get_line(0)->y1);
}

//==========================================================================
// Slots
//==========================================================================

void test_free_slot_finds_the_first_zero(void)
{
    run("clear.rocks");
    TEST_ASSERT_EQUAL_FLOAT(1, num("free.slot"));
    run(".setitem 1 :rsize 3  .setitem 2 :rsize 3");
    TEST_ASSERT_EQUAL_FLOAT(3, num("free.slot"));
}

// Zero, not an error and not slot 13: the caller's contract is that a rock
// which cannot be placed is simply not created, which is what the split table
// at M2 needs.
void test_a_full_board_has_no_free_slot(void)
{
    run("clear.rocks");
    run("repeat :max.rocks [.setitem repcount :rsize 3]");
    TEST_ASSERT_EQUAL_FLOAT(0, num("free.slot"));
}

void test_spawn_fills_a_slot_inside_the_field(void)
{
    run("clear.rocks");
    run("spawn.rock 3");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":rocks.alive"));
    TEST_ASSERT_EQUAL_FLOAT(3, item_of("rsize", 1));
    // The stored radius is the COLLISION half-width, which is the drawn radius
    // plus `shot.reach` -- one place, `rad.for`, decides it for both the
    // spawner and the split table.
    TEST_ASSERT_EQUAL_FLOAT(num("rad.for 3"), item_of("rrad", 1));

    // Centres always stay in bounds, so setx/sety never asks the turtle to
    // leave the field -- the outline crossing an edge is `wrap`'s job.
    TEST_ASSERT_TRUE(item_of("rx", 1) >= -160 && item_of("rx", 1) < 160);
    TEST_ASSERT_TRUE(item_of("ry", 1) >= -160 && item_of("ry", 1) < 160);

    // Speed comes from an angle, so no rock is ever left nearly stationary.
    float dx = item_of("rdx", 1), dy = item_of("rdy", 1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, num(":speed.l"), sqrtf(dx * dx + dy * dy));
}

void test_spawning_onto_a_full_board_creates_nothing(void)
{
    run("clear.rocks");
    run("repeat :max.rocks [spawn.rock 3]");
    TEST_ASSERT_EQUAL_FLOAT(12, num(":rocks.alive"));
    run("spawn.rock 3");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(12, num(":rocks.alive"),
                                    "a rock was created with no slot to hold it");
}

//==========================================================================
// Motion
//==========================================================================

// The rock pass spells `wrapc` out rather than calling it -- a user procedure
// call plus an `output` on top of two comparisons, 24 times a frame, measured
// 1.4 ms. The ship still calls the procedure, so the rule exists twice and this
// drives the copy: all four edges, and the middle where neither test fires.
void test_a_rock_leaving_the_field_comes_back_on_the_far_side(void)
{
    const struct { float x, y, dx, dy, wx, wy; } cases[] = {
        {159.5f,     0, 2,  0, -158.5f,     0},   // off the right
        {-159.5f,    0, -2, 0,  158.5f,     0},   // off the left
        {0,      159.5f, 0,  2,       0, -158.5f},// off the top
        {0,     -159.5f, 0, -2,       0,  158.5f},// off the bottom
        {10,         20, 1,  1,      11,      21},// nowhere near an edge
    };
    for (size_t k = 0; k < sizeof(cases) / sizeof(cases[0]); k++)
    {
        char cmd[224];
        run("init.game  clear.rocks  clear.shots");
        snprintf(cmd, sizeof(cmd),
                 ".setitem 1 :rsize 3  .setitem 1 :rx %g  .setitem 1 :ry %g "
                 ".setitem 1 :rdx %g  .setitem 1 :rdy %g "
                 ".setitem 1 :rspin 0  .setitem 1 :rang 0",
                 cases[k].x, cases[k].y, cases[k].dx, cases[k].dy);
        run(cmd);
        run("step.draw.all");

        char msg[128];
        snprintf(msg, sizeof(msg), "case %zu: the rock pass wrapped x to %g, not %g",
                 k, item_of("rx", 1), cases[k].wx);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, cases[k].wx, item_of("rx", 1), msg);
        snprintf(msg, sizeof(msg), "case %zu: the rock pass wrapped y to %g, not %g",
                 k, item_of("ry", 1), cases[k].wy);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, cases[k].wy, item_of("ry", 1), msg);

        // And it agrees with the procedure the ship still uses.
        snprintf(cmd, sizeof(cmd), "wrapc %g", cases[k].x + cases[k].dx);
        snprintf(msg, sizeof(msg), "case %zu: the inlined wrap and `wrapc` disagree", k);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, num(cmd), item_of("rx", 1), msg);
    }
}

void test_the_rock_pass_moves_only_live_rocks(void)
{
    run("init.game  clear.rocks  clear.shots");
    run(".setitem 2 :rsize 3  .setitem 2 :rx 0  .setitem 2 :rdx 1");
    run(".setitem 3 :rx 0  .setitem 3 :rdx 1");   // slot 3 stays free
    run("step.draw.all");
    TEST_ASSERT_EQUAL_FLOAT(1, item_of("rx", 2));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, item_of("rx", 3), "a free slot was stepped");
}

//==========================================================================
// The frame
//==========================================================================

static void setup_with(int rocks)
{
    char cmd[64];
    run("init.game");
    snprintf(cmd, sizeof(cmd), "make \"level.rocks %d", rocks);
    run(cmd);
    run("setup.level");
}

void test_setup_level_puts_the_asked_for_rocks_on_the_board(void)
{
    setup_with(9);
    TEST_ASSERT_EQUAL_FLOAT(9, num(":rocks.alive"));

    // And a second level does not inherit the first one's rocks.
    setup_with(3);
    TEST_ASSERT_EQUAL_FLOAT(3, num(":rocks.alive"));
}

// The board never holds more than MAX.ROCKS however many a level asks for --
// the ceiling that makes the frame budget's worst case a real bound.
void test_setup_level_never_exceeds_the_slot_count(void)
{
    setup_with(30);
    TEST_ASSERT_EQUAL_FLOAT(12, num(":rocks.alive"));
}

// The frame clears and redraws, so what reaches the canvas each frame is
// exactly the live rocks and nothing else. Under erase-in-place this was the
// file's most valuable test, because stale state showed up as leftover pixels
// and as nothing else; here there is no stale state to get wrong and it is a
// regression guard on the drawing pass.
void test_a_frame_draws_the_world_and_nothing_else(void)
{
    setup_with(6);
    // A level starts the ship in its respawn grace, where it blinks (M3) and
    // cannot be hit. Settle it: what is under test is that a frame draws the
    // world, and the blink has a test of its own. `ship.rad` goes to zero for
    // the same reason the timing harness zeroes it -- rocks are spawned at
    // random, so a ship that can be hit makes this test's segment count depend
    // on where they landed.
    land_the_ship();
    run("make \"ship.rad 0");
    // The ship too, and with no thrust key pressed it is always the plain
    // hull -- so a flame appearing on a quiet frame fails here.
    int expected = expected_segments() + SEG_SHIP;
    TEST_ASSERT_EQUAL_INT(6 * SEG_LARGE + SEG_SHIP, expected);

    for (int frame = 0; frame < 5; frame++)
    {
        mock_device_clear_graphics();
        run("play.frame");
        char msg[96];
        snprintf(msg, sizeof(msg), "frame %d drew %d segments, expected %d",
                 frame, mock_device_line_count(), expected);
        TEST_ASSERT_EQUAL_INT_MESSAGE(expected, mock_device_line_count(), msg);
    }
}

void test_every_rock_is_drawn_with_a_one_pixel_pen(void)
{
    // A wide pen's round caps spill outside the stroke and, in wrap mode,
    // across the screen edge -- the effect that made an early present-cost
    // harness read every frame as a full screen.
    setup_with(6);
    mock_device_clear_graphics();
    run("play.frame");
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, mock_device_get_line(i)->pen_size,
                                      "a rock was drawn with a pen wider than one pixel");
    }
}

// An Asteroids frame is NOT free, and this is the test that found it out.
//
// The other three games mutate their lists in place and measure zero cells a
// frame, and the design took that for a rule. It is not one. `.setitem` of a
// *number* interns it as a word atom (`member_value_to_node`,
// core/primitives_words_lists.c), so every rock's new x, y and angle mints an
// atom -- 36 a frame at twelve rocks, ~9,000 between reclaims. The three
// shipped games measure zero because the values they store come back out of
// other lists already interned, or from a handful of distinct constants;
// continuous physics has neither property.
//
// So the contract is a steady state rather than a zero, and it is what
// `reclaim` is for -- in this game it is load-bearing rather than a
// precaution. Soaked over 2,000 frames the working set settles near 2,950
// cells and stays there; what would fail here is *growth*.
// The test that was missing, and the one that would have caught the crash.
//
// Storage flatness (below) is not the property that matters, because nothing
// in this interpreter collects on demand: `alloc_cell` and `mem_atom`
// (core/memory.c) report out of space rather than collecting and retrying. So
// what the game must respect is a *deadline* -- how long the frame loop can
// run before it needs a recycle -- and `reclaim.every` has to sit well inside
// it.
//
// B25. A DEADLINE MEASURED ON A QUIET FRAME IS NOT THE GAME'S DEADLINE, and
// this test is the second version of that lesson. The first one disabled
// `reclaim`, ran the loop until it died, and asked for an interval eight times
// inside the number it got -- but the frame it ran had drifting rocks and
// nothing else in it. That frame spends 9 cells and 45 atom bytes. A frame in
// a GAME -- a saucer up, shots in the air, rocks splitting -- spends about 91
// bytes, and dies in 89 frames rather than 365. So the old 25-frame interval
// was a 3.6x margin wearing a 26x label, and the board ate the difference.
//
// The frame this measures is therefore the expensive one, and it stays the
// expensive one: `fire` every frame keeps three shots live and the respawn
// keeps a saucer crossing.
void test_the_reclaim_interval_stays_inside_the_busy_frame_budget(void)
{
    setup_with(12);
    land_the_ship();
    run("recycle");
    proc_define_from_text("to reclaim\nend");   // nothing collects now

    int deadline = 0;
    for (; deadline < 4000; deadline++)
    {
        run_string("fire");
        run_string("if 0 = :sau.on [spawn.saucer]");
        if (run_string("play.frame").status == RESULT_ERROR)
            break;
    }
    TEST_ASSERT_TRUE_MESSAGE(deadline < 4000,
                             "the frame loop no longer runs out of storage -- "
                             "re-derive this test, the interpreter changed");

    // A margin of 8x, so the interval survives a board whose workspace leaves
    // the atom region a quarter of the room this host gives it.
    int interval = (int)num(":reclaim.every");
    char msg[160];
    snprintf(msg, sizeof(msg),
             "reclaim every %d frames against a %d-frame busy budget -- less than 8x margin",
             interval, deadline);
    TEST_ASSERT_TRUE_MESSAGE(interval * 8 < deadline, msg);
}

// The other half of B25, and the half a host cannot measure by itself: the
// board that died had less room than the host that signed the interval off.
// The ballast list is that board -- it holds free storage down where a recycle
// cannot lift it, so every frame has to fit in what is left.
void test_the_frame_loop_survives_a_squeezed_workspace(void)
{
    setup_with(12);
    land_the_ship();
    run("recycle");

    // Eat ATOM bytes -- the resource this game actually runs out of, and the
    // one no Logo primitive reports -- until a fifth of them are left. The
    // ballast is live, so a recycle cannot give them back.
    size_t room = mem_free_atoms();
    run("make \"ballast []");
    while (mem_free_atoms() > room / 5)
    {
        if (run_string("repeat 100 [make \"ballast fput (random 100000) :ballast]").status
            == RESULT_ERROR)
            break;
    }
    size_t squeezed = mem_free_atoms();
    TEST_ASSERT_TRUE_MESSAGE(squeezed < room / 4, "the ballast did not squeeze the atom region");

    // A busy game in what is left.
    for (int f = 0; f < 2000; f++)
    {
        run_string("fire");
        run_string("if 0 = :sau.on [spawn.saucer]");
        if (run_string("play.frame").status == RESULT_ERROR)
        {
            char msg[160];
            snprintf(msg, sizeof(msg),
                     "the frame loop ran out of storage after %d frames with %d atom bytes free",
                     f, (int)squeezed);
            TEST_FAIL_MESSAGE(msg);
        }
    }
}





// FIVE THOUSAND FRAMES AND NOT ONE THOUSAND, because a single 1000-frame
// window does not measure the frame loop -- it measures where the workspace
// happened to settle. Free cells after `recycle` are the node region's ceiling,
// and that ceiling moves with the atom region under it, so the reading swings
// hundreds of cells between consecutive windows on code that did not change:
// measured over five windows in a row, this game gives -666, +500, +841 and
// +1617 with nothing touched between them. The first window is the worst of
// them, because it is the one still settling after
// `test_the_reclaim_interval_stays_inside_the_busy_frame_budget` ran the workspace to
// exhaustion just before it -- and it moves by ~160 cells for a change as small
// as one more procedure called on a death frame, which is not growth and must
// not read as growth.
//
// Summed over 5000 frames the settling is amortised and the number is stable:
// ~2600 cells, and the same within 6% across three different explosions. A leak
// worth catching is per-frame, so it scales with the window -- one cell a frame
// would be 5000 here and would not fit under any threshold this test could
// still call flat.
void test_the_frame_loop_holds_free_storage_flat(void)
{
    setup_with(12);
    run("repeat 250 [play.frame]");
    run("recycle");
    int settled = (int)num("nodes");

    run("repeat 5000 [play.frame]");
    run("recycle");
    int later = (int)num("nodes");

    char msg[128];
    snprintf(msg, sizeof(msg),
             "free storage fell %d cells over 5000 frames -- the frame loop is growing",
             settled - later);
    TEST_ASSERT_TRUE_MESSAGE(settled - later < 3400, msg);
}

// `reclaim` sits inside the unpaused block. Outside it, a pause landing on a
// multiple of 250 leaves `remainder :frame.count 250` at zero for the whole
// pause and recycles on every paused frame -- the every-frame recycle all
// three shipped games explicitly forbid.
void test_a_paused_frame_neither_steps_nor_recycles(void)
{
    setup_with(6);
    run(".setitem 1 :rx 0  .setitem 1 :rdx 1");
    run("make \"paused true");
    run("make \"frame.count 250");
    run("repeat 200 [make \"junk fput 1 [1 2 3]]");   // ~800 cells of garbage

    // A recycle would hand that garbage back, so free storage would jump. The
    // test is that it does not: a few cells either way is the frame's own
    // noise, 800 is a recycle.
    int before = (int)num("nodes");
    run("repeat 5 [play.frame]");
    TEST_ASSERT_TRUE_MESSAGE((int)num("nodes") - before < 100,
                             "a paused frame recycled -- reclaim is outside the pause");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, item_of("rx", 1), "a paused frame stepped a rock");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(250, num(":frame.count"), "a paused frame counted");
}

//==========================================================================
// The ship
//==========================================================================

// Velocity is state and thrust is an impulse, so the ship keeps its momentum
// with nothing pressed. There is no drag term: the arcade's is very slight and
// leaving it out is one fewer statement a frame.
void test_the_ship_keeps_its_momentum(void)
{
    run("reset.ship  make \"svx 2  make \"svy -3");
    run("step.ship");
    TEST_ASSERT_EQUAL_FLOAT(2, num(":shipx"));
    TEST_ASSERT_EQUAL_FLOAT(-3, num(":shipy"));
    run("step.ship");
    TEST_ASSERT_EQUAL_FLOAT(4, num(":shipx"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":svx"), "the ship slowed down on its own");
}

void test_the_ship_wraps_like_a_rock(void)
{
    run("reset.ship  make \"shipx 159  make \"svx 2");
    run("step.ship");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -159.0f, num(":shipx"));
}

// `sin`/`cos` take degrees and this Logo's heading is clockwise from north,
// which is what `seth` wants -- so `sh` is both the physics heading and the
// drawing heading with no conversion anywhere. A sign error here flies the
// ship backwards, which reads as "feel" and is actually a bug.
void test_thrust_pushes_along_the_heading(void)
{
    run("reset.ship  make \"sh 0  thrust");
    TEST_ASSERT_TRUE_MESSAGE(num(":svy") > 0, "thrust at heading 0 did not push north");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, num(":svx"));

    run("reset.ship  make \"sh 90  thrust");
    TEST_ASSERT_TRUE_MESSAGE(num(":svx") > 0, "thrust at heading 90 did not push east");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, num(":svy"));
}

// The speed clamp is what stops the ship becoming unplayable, and it has to
// hold ON the boundary rather than near it -- thrust in one direction long
// enough and the speed sits exactly on `speed.max`, not one impulse past it.
// Checked on an axis and on a diagonal, because a clamp written per component
// rather than on the magnitude passes the first and fails the second.
void test_the_speed_clamp_holds_at_the_boundary(void)
{
    const char *headings[] = {"90", "45"};
    for (int k = 0; k < 2; k++)
    {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "reset.ship  make \"sh %s  repeat 40 [thrust]", headings[k]);
        run(cmd);
        float vx = num(":svx"), vy = num(":svy");
        char msg[96];
        snprintf(msg, sizeof(msg), "heading %s settled at %.3f, not %.3f",
                 headings[k], sqrtf(vx * vx + vy * vy), num(":speed.max"));
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, num(":speed.max"),
                                         sqrtf(vx * vx + vy * vy), msg);
    }
}

// The heading is never normalised, exactly as a rock's rotation is not: one
// `if` a frame buys nothing, and `seth`, `sin` and `cos` all take any angle.
// If they did not the game would break after a couple of minutes of turning
// rather than at once, which is the worst way for it to break.
void test_a_ship_heading_outside_zero_to_360_still_thrusts_and_draws(void)
{
    run("reset.ship  make \"sh 725  thrust");    // 725 is 5 degrees
    TEST_ASSERT_TRUE_MESSAGE(num(":svy") > 0, "a heading past 360 did not thrust north");

    run("reset.ship  make \"sh -355  thrust");   // -355 is 5 degrees too
    TEST_ASSERT_TRUE_MESSAGE(num(":svy") > 0, "a negative heading did not thrust north");

    // `safe` is cleared because `reset.ship` respawns the ship, and a
    // respawned ship blinks for `safe.frames` (M3). What is under test here is
    // the heading, so settle it first.
    land_the_ship();
    run("make \"thrusting false  make \"frame.count 1  make \"sh 725");
    mock_device_clear_graphics();
    run("draw.ship");
    TEST_ASSERT_EQUAL_INT(SEG_SHIP, mock_device_line_count());
}

// The flame alternates on and off every other frame, as the arcade one does:
// a held thrust key would otherwise draw a steady cone, which reads as a
// nozzle rather than a burn. It is folded into one closed walk with the hull,
// so a thrusting ship costs one dispatch and one placement rather than two.
void test_the_flame_shows_only_when_thrusting_and_only_every_other_frame(void)
{
    run("reset.ship  make \"thrusting false  make \"frame.count 0");
    land_the_ship();
    mock_device_clear_graphics();
    run("draw.ship");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SEG_SHIP, mock_device_line_count(),
                                  "a ship that is not thrusting drew a flame");

    run("make \"thrusting true  make \"frame.count 1");
    mock_device_clear_graphics();
    run("draw.ship");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SEG_FLAME, mock_device_line_count(),
                                  "a thrusting ship drew no flame");

    run("make \"frame.count 2");
    mock_device_clear_graphics();
    run("draw.ship");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SEG_SHIP, mock_device_line_count(),
                                  "the flame did not blink");
}

// 16 degrees a frame is 240 a second at 15 fps, which is what 12 at 20 fps
// was: every per-frame constant in this design was re-cut by a third when the
// rate came down off M0's present measurement (section 18).
void test_the_arrows_turn_the_ship_both_ways(void)
{
    setup_with(3);
    land_the_ship();          // a ship still waiting answers no key at all (B24)
    float turn = num(":turn.rate");
    set_mock_input(KEY_RIGHT);
    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT(turn, num(":sh"));
    set_mock_input(KEY_LEFT);
    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":sh"));
}

// One key a frame means thrust is held only on frames where thrust was the key
// read, so `thrusting` is cleared BEFORE the input guard rather than after it.
// Cleared after, a frame with no key at all would leave the flame lit.
void test_a_frame_with_no_key_puts_the_flame_out(void)
{
    setup_with(3);
    land_the_ship();          // a ship still waiting answers no key at all (B24)
    set_mock_input(KEY_THRUST);
    run("play.frame");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true",
                                     value_to_string(eval_string(":thrusting").value),
                                     "the thrust key did not light the flame");
    run("play.frame");   // nothing queued
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false",
                                     value_to_string(eval_string(":thrusting").value),
                                     "the flame stayed lit with no key pressed");
}

//==========================================================================
// Shots
//==========================================================================

void test_firing_takes_the_lowest_idle_shot(void)
{
    setup_with(3);
    TEST_ASSERT_EQUAL_FLOAT(1, num("free.shot"));
    run("fire");
    TEST_ASSERT_EQUAL_FLOAT(num(":shot.life"), item_of("slife", 1));
    TEST_ASSERT_EQUAL_FLOAT(2, num("free.shot"));

    run("fire  fire");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("free.shot"), "a fourth shot slot appeared");

    // A fourth shot is simply not fired -- the same rule a rock with no slot
    // follows, and not a write past the end of the list.
    run("fire");
    TEST_ASSERT_EQUAL_FLOAT(3, num("count :slife"));
}

// A shot lives `shot.life` frames and then hides its turtle and stops it. One
// left gliding with its turtle shown would cross the screen for ever and still
// be tested against every rock on every frame.
void test_a_shot_expires_and_stops_its_turtle(void)
{
    setup_with(3);
    run("fire");
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string("ask 1 [shown?]").value));

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "repeat %d [step.shots]", (int)num(":shot.life"));
    run(cmd);
    TEST_ASSERT_EQUAL_FLOAT(0, item_of("slife", 1));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false",
                                     value_to_string(eval_string("ask 1 [shown?]").value),
                                     "an expired shot is still on the screen");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("ask 1 [speed]"),
                                    "an expired shot is still gliding");
}

// The engine flies the shot and Logo only reads the position back, which is
// the whole reason shots are turtles: `setspeed` is wall-clock, so the mock's
// clock has to move for the shot to move. That is also why the tunnelling
// bound above is stated against a frame that overruns rather than a nominal
// one -- a slow frame moves a shot further, not less far.
void test_a_shot_flies_on_its_own_and_its_position_is_read_back(void)
{
    setup_with(3);
    run("reset.ship  make \"sh 0  fire  step.shots");
    float y0 = num(":s1y");

    set_mock_ticks(mock_ticks_value + 100);   // 100 ms at 200 steps/s is 20 steps
    run("step.shots");
    float y1 = num(":s1y");

    char msg[112];
    snprintf(msg, sizeof(msg), "the shot moved %.1f steps in 100 ms, expected about 20",
             y1 - y0);
    TEST_ASSERT_TRUE_MESSAGE(y1 - y0 > 10.0f, msg);
}

//==========================================================================
// Collisions, splitting and scoring
//==========================================================================

// A square test, not a circle: one statement instead of a squared-distance
// expression, and against a jagged rock whose outline is nowhere near its
// bounding circle the extra reach in the corners is not perceptible. The edge
// is OUTSIDE here, because the tests are `>` on the radius rather than `<` --
// half a step either way on a box that is already four steps generous.
//
// `shot.on` outputs which shot hit, not whether one did, because the rock pass
// has to kill the shot it was hit by.
void test_shot_on_finds_the_shot_inside_the_square(void)
{
    run("clear.shots");
    run("make \"s2x 100  make \"s2y -50");

    TEST_ASSERT_EQUAL_FLOAT(2, num("shot.on 100 -50 10"));
    TEST_ASSERT_EQUAL_FLOAT(2, num("shot.on 109.9 -50 10"));
    TEST_ASSERT_EQUAL_FLOAT(2, num("shot.on 100 -59.9 10"));
    TEST_ASSERT_EQUAL_FLOAT(2, num("shot.on 109.9 -59.9 10"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("shot.on 110.5 -50 10"), "x outside still hit");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("shot.on 100 -60.5 10"), "y outside still hit");
}

// An idle shot is parked off the field rather than guarded by an `if`, which
// is what makes a pair one comparison instead of two. If parking ever stopped
// happening, a dead shot would keep killing rocks from wherever it died.
void test_a_parked_shot_hits_nothing_anywhere_on_the_field(void)
{
    run("clear.shots");
    for (int x = -160; x <= 160; x += 40)
    {
        char expr[64], msg[96];
        snprintf(expr, sizeof(expr), "shot.on %d %d 26", x, x);
        snprintf(msg, sizeof(msg), "a parked shot hit a rock at %d, %d", x, x);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(expr), msg);
    }
}

// Large -> two medium, medium -> two small, small -> nothing.
void test_the_split_table(void)
{
    run("init.game  clear.rocks");
    run(".setitem 1 :rsize 3  .setitem 1 :rrad rad.for 3  make \"rocks.alive 1");
    run(".setitem 1 :rx 20  .setitem 1 :ry -30  .setitem 1 :rdx 1  .setitem 1 :rdy 0");
    run("split.rock 1 true");

    TEST_ASSERT_EQUAL_FLOAT(2, num(":rocks.alive"));
    TEST_ASSERT_EQUAL_FLOAT(2, item_of("rsize", 1));
    TEST_ASSERT_EQUAL_FLOAT(2, item_of("rsize", 2));
    TEST_ASSERT_EQUAL_FLOAT(num("rad.for 2"), item_of("rrad", 1));

    // Both children start where the parent died, carrying its velocity boosted.
    TEST_ASSERT_EQUAL_FLOAT(20, item_of("rx", 1));
    TEST_ASSERT_EQUAL_FLOAT(-30, item_of("ry", 2));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, num(":split.boost"), item_of("rdx", 1));

    // The kick is perpendicular to the parent's velocity with opposite signs,
    // so the two children always separate. A kick drawn independently for each
    // could come out near zero for both and leave them travelling together,
    // which reads as one rock that got smaller rather than as a split.
    TEST_ASSERT_TRUE_MESSAGE(item_of("rdy", 1) * item_of("rdy", 2) < 0,
                             "the two children did not separate");

    run("clear.rocks");
    run(".setitem 1 :rsize 2  make \"rocks.alive 1  split.rock 1 true");
    TEST_ASSERT_EQUAL_FLOAT(2, num(":rocks.alive"));
    TEST_ASSERT_EQUAL_FLOAT(1, item_of("rsize", 1));

    run("clear.rocks");
    run(".setitem 1 :rsize 1  make \"rocks.alive 1  split.rock 1 true");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":rocks.alive"), "a small rock split");
}

// A split fills as many free slots as there are: two if two are free, one if
// one is, none if the board is full -- and a child that cannot be placed is
// simply not created. That rule is what makes the frame budget's worst case a
// real bound rather than an estimate, so what matters is that the count never
// goes above MAX.ROCKS.
void test_a_split_fills_the_slots_it_finds_and_no_more(void)
{
    run("init.game  clear.rocks");
    run("repeat :max.rocks [.setitem repcount :rsize 3]");
    run("make \"rocks.alive :max.rocks");
    run(".setitem 1 :rdx 1  .setitem 1 :rdy 0");
    run("split.rock 1 true");   // frees exactly one slot, so exactly one child fits

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(12, num(":rocks.alive"),
                                    "a split overran the slot count");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(12, num("count :rsize"),
                                    "a split wrote past the end of a list");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("free.slot"), "the board is not full");
}

// Three larges shot all the way down is 3 -> 6 -> 12, which fills MAX.ROCKS
// exactly. That arithmetic is the whole reason a level starts with three rocks
// and not the arcade's four: four would want sixteen, and the split cap would
// start eating children on an ordinary level rather than only on a
// deliberately awkward one.
void test_three_larges_split_all_the_way_down_into_exactly_twelve_slots(void)
{
    setup_with(3);
    // Every large first, then every medium -- the order that needs the most
    // slots at once.
    for (int size = 3; size >= 2; size--)
    {
        for (int i = 1; i <= 12; i++)
        {
            if ((int)item_of("rsize", i) != size)
                continue;
            char cmd[48];
            snprintf(cmd, sizeof(cmd), "split.rock %d true", i);
            run(cmd);
        }
    }

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(12, num(":rocks.alive"), "3 -> 6 -> 12 did not fit");
    for (int i = 1; i <= 12; i++)
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "slot %d holds size %d, not a small rock",
                 i, (int)item_of("rsize", i));
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, item_of("rsize", i), msg);
    }

    // The arcade table over what was actually killed: three larges at 20 and
    // the six mediums they became at 50. The twelve smalls are still on the
    // board, which is the point of the test -- shooting them too would score
    // another 1,200 and leave nothing to count slots with.
    TEST_ASSERT_EQUAL_FLOAT(3 * 20 + 6 * 50, num(":score"));
}

void test_the_score_table(void)
{
    run("init.game");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":score"));
    run("score.rock 3");
    TEST_ASSERT_EQUAL_FLOAT(20, num(":score"));
    run("score.rock 2");
    TEST_ASSERT_EQUAL_FLOAT(70, num(":score"));
    run("score.rock 1");
    TEST_ASSERT_EQUAL_FLOAT(170, num(":score"));
}

void test_a_shot_on_a_rock_splits_it_scores_it_and_is_consumed(void)
{
    run("init.game  clear.rocks  clear.shots");
    run(".setitem 1 :rsize 3  .setitem 1 :rrad rad.for 3  make \"rocks.alive 1");
    run(".setitem 1 :rx 50  .setitem 1 :ry 50  .setitem 1 :rdx 0  .setitem 1 :rdy 0");
    run(".setitem 1 :slife :shot.life  make \"s1x 50  make \"s1y 50");

    mock_device_clear_graphics();
    run("step.draw.all");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":rocks.alive"), "the large rock did not split");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(20, num(":score"), "the kill did not score");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, item_of("slife", 1), "the shot was not consumed");

    // The rock that died was not drawn -- what a separate collision pass
    // running before the drawing pass used to buy. Only the two children that
    // landed in slots above it are, and both are mediums.
    TEST_ASSERT_EQUAL_INT_MESSAGE(SEG_MEDIUM, mock_device_line_count(),
                                  "the dead rock was drawn, or a child was missed");
}

// The shot that kills a rock is killed inside the rock pass, so it has to stop
// hitting things for the rest of that same pass -- otherwise one shot clears a
// diagonal of the board in a frame. Parking its x at 9999 is what does it.
void test_one_shot_kills_one_rock_per_frame(void)
{
    run("init.game  clear.rocks  clear.shots");
    for (int i = 1; i <= 3; i++)
    {
        char cmd[144];
        snprintf(cmd, sizeof(cmd),
                 ".setitem %d :rsize 1  .setitem %d :rrad rad.for 1 "
                 ".setitem %d :rx 50  .setitem %d :ry 50", i, i, i, i);
        run(cmd);
    }
    run("make \"rocks.alive 3");
    run(".setitem 1 :slife :shot.life  make \"s1x 50  make \"s1y 50");
    run("step.draw.all");

    // Three small rocks stacked on the shot; exactly one dies (smalls do not
    // split), so two are left.
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":rocks.alive"),
                                    "one shot killed more than one rock in a frame");
}

// A dead shot is parked rather than guarded, so nothing in the rock pass has
// to test whether it is alive. If parking stopped happening, a rock would die
// to a shot that expired frames ago, from wherever it expired.
void test_an_idle_shot_hits_nothing(void)
{
    run("init.game  clear.rocks  clear.shots");
    run(".setitem 1 :rsize 3  .setitem 1 :rrad rad.for 3  make \"rocks.alive 1");
    run(".setitem 1 :rx 50  .setitem 1 :ry 50");
    run("kill.shot 1");
    run("step.draw.all");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":rocks.alive"),
                                    "an expired shot killed a rock");
}

//==========================================================================
// Dying, lives and levels
//==========================================================================

// A ship at rest with a rock on top of it. `ship.rad` is added to the rock's
// radius in the rock pass, so this is the one collision in the game whose box
// is not `rrad` alone.
static void ship_under_a_rock(void)
{
    run("init.game  clear.rocks  clear.shots");
    run(".setitem 1 :rsize 3  .setitem 1 :rrad rad.for 3  make \"rocks.alive 1");
    run(".setitem 1 :rx 0  .setitem 1 :ry 0  .setitem 1 :rdx 0  .setitem 1 :rdy 0");
    // Out of the respawn grace and hittable, which is `step.ship`'s job.
    land_the_ship();
    run("step.ship");
}

void test_a_rock_on_the_ship_kills_it(void)
{
    ship_under_a_rock();
    TEST_ASSERT_EQUAL_FLOAT(3, num(":lives"));

    mock_device_clear_graphics();
    run("step.draw.all");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"), "a rock on the ship did not kill it");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":death.frames"), num(":dying"),
                                    "the explosion did not start");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9999, num(":shipcx"),
                                    "a dead ship was left where a rock could hit it again");
    // The rock is not consumed by killing the ship -- it is still drawn, and
    // still there.
    TEST_ASSERT_EQUAL_INT_MESSAGE(SEG_LARGE, mock_device_line_count(),
                                  "the rock that killed the ship was not drawn");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":rocks.alive"));
}

// The ship is parked the moment it dies, exactly as a spent shot is, so the
// rocks after it in the same pass test against 9999 and miss. Without that, a
// board of twelve rocks sitting on the ship would take twelve lives in one
// frame and end the game from full.
void test_one_frame_takes_only_one_life(void)
{
    run("init.game  clear.rocks  clear.shots");
    for (int i = 1; i <= 12; i++)
    {
        char cmd[144];
        snprintf(cmd, sizeof(cmd),
                 ".setitem %d :rsize 1  .setitem %d :rrad rad.for 1  "
                 ".setitem %d :rx 0  .setitem %d :ry 0  "
                 ".setitem %d :rdx 0  .setitem %d :rdy 0", i, i, i, i, i, i);
        run(cmd);
    }
    land_the_ship();
    run("make \"rocks.alive 12  step.ship");
    run("step.draw.all");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"),
                                    "one frame took more than one life");
}

// A new ship waits for its spawn point to be clear, which is the arcade's rule
// and replaces M3's invulnerability. The scan it needs is the ship test the
// rock pass already runs: the waiting ship is parked ON the spawn point with a
// wide box, and `ship.hit` answers "not clear yet" instead of taking a life.
void test_a_waiting_ship_does_not_appear_until_the_space_is_clear(void)
{
    ship_under_a_rock();
    run("respawn");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":waiting").value),
                                     "respawn did not put the ship in a wait");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":clear.rad"), num(":ship.rad"),
                                    "a waiting ship is not testing the wide box");

    // Twenty frames with a rock parked on the spawn point: no life is lost and
    // no ship appears.
    for (int i = 0; i < 20; i++)
    {
        run("step.wait  step.draw.all");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":lives"),
                                        "waiting for a clear space cost a life");
        TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":waiting").value),
                                         "the ship appeared with a rock on the spawn point");
    }

    // And it is not drawn while it waits.
    mock_device_clear_graphics();
    run("draw.ship");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(), "a waiting ship was drawn");

    // Move the rock away and it lands -- one frame for the pass to find the
    // field clear, one for the check that reads it.
    run(".setitem 1 :rx 150  .setitem 1 :ry 150");
    run("step.wait  step.draw.all");
    run("step.wait");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", value_to_string(eval_string(":waiting").value),
                                     "the ship never landed on a clear field");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":ship.rad.hull"), num(":ship.rad"),
                                    "a landed ship is still testing the wide box");

    // Landed, it is hittable again: bring the rock back and it dies.
    run(".setitem 1 :rx 0  .setitem 1 :ry 0");
    run("step.ship  step.draw.all");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"), "a landed ship could not be hit");
}

// B24. "Wait for a clear space" with no bound on it is a hang wearing a rule's
// clothes. The box a new ship waits for is `clear.rad` plus the rock's own
// radius -- 50 steps for a large rock -- and rocks cross at 0.96 steps a frame,
// so ONE rock drifting through the middle can hold the spawn point for over a
// hundred frames. Measured on the host before the cap: a mean wait of 10 frames,
// a worst of 127, and one respawn in ten over two seconds. Reported from a board
// as a ship that stayed hidden for several seconds after a respawn.
//
// The cap is affordable because the clear box is 20 steps wider than the box
// that kills: a rock still inside it when the cap expires is, almost always,
// not yet touching the hull. This is the extreme case -- a rock parked dead
// centre, which nothing in play can hold there -- so it lands and dies, and
// even that is better than an empty screen with no way out.
void test_a_respawn_wait_gives_up_and_lands_the_ship(void)
{
    ship_under_a_rock();
    run("respawn");
    int cap = (int)num(":wait.max");

    int f = 0;
    while (strcmp(value_to_string(eval_string(":waiting").value), "true") == 0 && f < cap * 4)
    {
        run("step.wait  step.draw.all");
        f++;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "the ship waited %d frames against a cap of %d", f, cap);
    TEST_ASSERT_TRUE_MESSAGE(f <= cap + 1, msg);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":ship.rad.hull"), num(":ship.rad"),
                                    "the ship landed still testing the wide box");
}

// B24. A waiting ship is parked on the spawn point and drawn nowhere, and the
// controls were live for the whole wait -- so a player could turn and thrust a
// ship they could not see, and `fire` put shots on the screen out of an empty
// spawn point. That is what the board reported: firing while the ship stayed
// hidden. Hyperspace was the worst of the four, because it writes `shipx`/
// `shipy` while the clear-check goes on reading the spawn point.
void test_a_waiting_ship_answers_no_key_but_pause_and_quit(void)
{
    setup_with(3);
    run("respawn");

    set_mock_input(KEY_RIGHT);
    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sh"), "a waiting ship steered");

    run("respawn");
    set_mock_input(KEY_THRUST);
    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":svx"), "a waiting ship thrusted");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":svy"), "a waiting ship thrusted");

    run("respawn");
    set_mock_input(" ");
    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, item_of("slife", 1), "a waiting ship fired");

    run("respawn");
    set_mock_input(KEY_HYPER);
    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":shipx"), "a waiting ship jumped");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":shipy"), "a waiting ship jumped");

    // Pause and quit still answer, as they do through a death.
    run("respawn");
    set_mock_input("p");
    run("play.frame");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":paused").value),
                                     "a waiting ship could not be paused");
    set_mock_input("p");
    run("play.frame");
    set_mock_input("q");
    run("play.frame");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":quit").value),
                                     "a waiting ship could not be quit");
}

// The wide box is what makes the wait mean something, and it is held against
// the shapes rather than assumed: it must be well past the ship's own box, and
// not so wide that a busy field never clears.
void test_the_clear_radius_is_wider_than_the_ship_it_protects(void)
{
    TEST_ASSERT_TRUE_MESSAGE(num(":clear.rad") > num(":ship.rad.hull") * 2,
                             "the clear radius is barely wider than the ship's own box");
    TEST_ASSERT_TRUE_MESSAGE(num("rad.for 3") + num(":clear.rad") < 80,
                             "a respawn waits for more space than a busy field ever has");
}

// A death is a countdown inside the frame loop and not a `wait`: the rocks keep
// drifting under the explosion, which is what the arcade does, and the player
// can still pause or quit through it.
void test_the_explosion_counts_down_and_the_ship_comes_back(void)
{
    ship_under_a_rock();
    run(".setitem 1 :rdx 1");           // and it drifts off while the ship burns
    run("make \"shipx 40  make \"shipy -40  make \"svx 3  make \"svy 3");
    run("ship.hit");

    int frames = (int)num(":death.frames");
    for (int i = 1; i < frames; i++)
    {
        run("play.frame");
        char msg[96];
        snprintf(msg, sizeof(msg), "frame %d of the explosion", i);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(frames - i, num(":dying"), msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(40, num(":shipx"), "a dying ship moved");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"), "the explosion took a second life");
    }

    // The last frame of the countdown is the one that brings the ship back:
    // centre of the field, stopped, facing north, and waiting for the space to
    // clear before it appears.
    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":dying"), "the explosion did not end");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":shipx"), "the ship did not come back to the centre");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":svx"), "the ship came back still moving");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sh"), "the ship came back on its old heading");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":waiting").value),
                                     "the ship came back without waiting for a clear space");
    TEST_ASSERT_TRUE_MESSAGE(item_of("rx", 1) > 0, "the rocks stood still through the death");
}

// The last life ends the level rather than respawning into an unplayable game,
// and it ends it from inside the frame -- `play.level` reads `over` and stops.
void test_the_last_life_ends_the_level(void)
{
    run("init.game  clear.rocks  clear.shots");
    land_the_ship();
    run("make \"lives 1  make \"over false  ship.hit");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":lives"));

    run("repeat :death.frames [play.frame]");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":over").value),
                                     "the last life did not end the level");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9999, num(":shipcx"),
                                    "a game that is over put the ship back");
}

// A dying ship is four fragments and not a ship: the wreck is the four
// segments of `ship`'s own walk, drifting apart. Four lines a death frame, and
// four is also what the intact hull draws -- so the count alone cannot tell
// them apart, and the next test is the one that pins the shape.
void test_a_dying_ship_draws_fragments_and_not_a_ship(void)
{
    run("init.game  clear.rocks  clear.shots");
    land_the_ship();
    run("make \"dying 1");
    mock_device_clear_graphics();
    run("draw.ship");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SEG_SHIP, mock_device_line_count(),
                                  "a dying ship did not draw four fragments");

    // A fragment is one stroke and nothing else: the two legs that carry it out
    // from the centre are walked with the pen up, so a fifth line here means a
    // fragment is trailing a tail back to the ship.
    run("make \"dying :death.frames - 1  make \"shipx 0  make \"shipy 0");
    mock_device_clear_graphics();
    run("draw.ship");
    TEST_ASSERT_EQUAL_INT(SEG_SHIP, mock_device_line_count());
}

// The fragments ARE the ship's segments, and the test says so by re-deriving
// them from it: at `dying = death.frames` the drift is zero, so the explosion
// must land exactly on the outline `ship` draws. That is what holds the four
// hand-computed bearings in `draw.boom` to the walk they came from -- a wrong
// constant is a fragment that does not start where the ship's edge was.
void test_the_fragments_are_the_ship_segments(void)
{
    run("init.game  clear.rocks  clear.shots");
    land_the_ship();
    run("make \"shipx 0  make \"shipy 0  make \"sh 35");

    MockLine hull[SEG_SHIP];
    mock_device_clear_graphics();
    run("pu setx :shipx sety :shipy seth :sh  ship");
    TEST_ASSERT_EQUAL_INT(SEG_SHIP, mock_device_line_count());
    for (int i = 0; i < SEG_SHIP; i++)
        hull[i] = *mock_device_get_line(i);

    run("make \"dying :death.frames");
    mock_device_clear_graphics();
    run("draw.ship");
    TEST_ASSERT_EQUAL_INT(SEG_SHIP, mock_device_line_count());

    for (int i = 0; i < SEG_SHIP; i++)
    {
        const MockLine *f = mock_device_get_line(i);
        char msg[64];
        snprintf(msg, sizeof(msg), "fragment %d is not the ship's segment %d", i, i);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.2f, hull[i].x1, f->x1, msg);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.2f, hull[i].y1, f->y1, msg);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.2f, hull[i].x2, f->x2, msg);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.2f, hull[i].y2, f->y2, msg);
    }

    // And they float outward as the countdown falls: each fragment's middle
    // leaves the ship's centre at `boom.drift` a frame, along its own bearing,
    // so the distance from the centre grows by exactly that between frames and
    // the fragment's length never changes -- it drifts, it does not stretch.
    for (int step = 1; step < 4; step++)
    {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "make \"dying :death.frames - %d", step);
        run(cmd);
        mock_device_clear_graphics();
        run("draw.ship");

        for (int i = 0; i < SEG_SHIP; i++)
        {
            const MockLine *f = mock_device_get_line(i);
            float hmx = (hull[i].x1 + hull[i].x2) / 2, hmy = (hull[i].y1 + hull[i].y2) / 2;
            float fmx = (f->x1 + f->x2) / 2, fmy = (f->y1 + f->y2) / 2;
            float drift = sqrtf((fmx - hmx) * (fmx - hmx) + (fmy - hmy) * (fmy - hmy));
            char msg[64];
            snprintf(msg, sizeof(msg), "fragment %d at death frame %d", i, step);
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.3f, num(":boom.drift") * step, drift, msg);

            float hlen = hypotf(hull[i].x2 - hull[i].x1, hull[i].y2 - hull[i].y1);
            float flen = hypotf(f->x2 - f->x1, f->y2 - f->y1);
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.2f, hlen, flen, msg);
        }
    }
}

void test_hyperspace_moves_the_ship_and_stops_it(void)
{
    run("init.game  clear.rocks  clear.shots");
    land_the_ship();
    run("make \"shipx 100  make \"shipy 100  make \"svx 3  make \"svy -2");
    run("make \"dying 0  hyperspace");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":svx"), "a jump kept the ship's velocity");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":svy"), "a jump kept the ship's velocity");
    TEST_ASSERT_TRUE_MESSAGE(num("abs :shipx") <= 160 && num("abs :shipy") <= 160,
                             "a jump put the ship outside the field");
}

// The risk is a flat one in `hyper.risk` rather than the arcade's
// velocity-dependent formula, so it can be counted. 200 jumps at one in eight
// is 25 deaths; the bounds are wide enough that only a broken chance fails.
//
// `rerandom` with a seed is what makes this testable at all: the mock device's
// hardware RNG returns a constant, so unseeded `random 8` is the same number
// 200 times and no chance in this game can be observed on the host.
void test_hyperspace_sometimes_ends_badly(void)
{
    run("init.game  clear.rocks  clear.shots");
    land_the_ship();
    run("(rerandom 1)");
    int deaths = 0;
    for (int i = 0; i < 200; i++)
    {
        run("make \"dying 0  make \"lives 3  hyperspace");
        if (num(":dying") > 0)
            deaths++;
    }
    char msg[96];
    snprintf(msg, sizeof(msg), "%d deaths in 200 jumps at one in %d",
             deaths, (int)num(":hyper.risk"));
    TEST_ASSERT_TRUE_MESSAGE(deaths > 5 && deaths < 70, msg);
}

// Every point in the game arrives through `add.score`, so the extra ship is
// tested in one place -- and against a moving threshold rather than a
// remainder, because a score can step over a boundary rather than land on it.
void test_an_extra_ship_every_ten_thousand_points(void)
{
    run("init.game");
    run("add.score 9950");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":lives"), "an extra ship arrived early");

    run("add.score 100");            // steps over 10,000 rather than landing on it
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, num(":lives"), "no extra ship at 10,000");

    run("add.score 100");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, num(":lives"), "a second extra ship at the same threshold");

    run("add.score 9900");           // 20,050
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5, num(":lives"), "no extra ship at 20,000");
}

// The HUD refresh belongs with the value that moved. `split.rock` used to do it
// -- from M2, when the HUD carried the live rock count and a kill changed it --
// and the HUD carries the level now, so a kill is a *score* event. Raised on
// PR #145: any future scorer that did not know the convention would leave the
// HUD stale, and M4's saucer is exactly that caller.
void test_scoring_repaints_the_hud_wherever_the_points_come_from(void)
{
    run("init.game  make \"level 1  make \"lives 3");

    run("make \"hud.text [stale]");
    run("add.score 50");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("[SCORE 50 WAVE 1 \x10\x10\x10]",
                                     value_to_string(eval_string(":hud.text").value),
                                     "add.score left the HUD stale");

    // An extra ship moves `lives`, which is on the same line.
    run("make \"hud.text [stale]  add.score 9950");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("[SCORE 10000 WAVE 1 \x10\x10\x10\x10]",
                                     value_to_string(eval_string(":hud.text").value),
                                     "an extra ship did not reach the HUD");

    // And a kill still repaints, through `score.rock` -- exactly once, not
    // once here and once in `split.rock`.
    run("clear.rocks  clear.shots");
    run(".setitem 1 :rsize 1  .setitem 1 :rrad rad.for 1  make \"rocks.alive 1");
    run("make \"hud.text [stale]");
    run("split.rock 1 true");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("[SCORE 10100 WAVE 1 \x10\x10\x10\x10]",
                                     value_to_string(eval_string(":hud.text").value),
                                     "a kill left the HUD stale");
}

// One more large rock a level, to a ceiling: five larges split into ten mediums
// and the eleventh has nowhere to go, so the cap bites at the top level and not
// below it.
void test_a_level_advance_adds_a_rock_up_to_the_ceiling(void)
{
    run("init.game");
    TEST_ASSERT_EQUAL_FLOAT(3, num(":level.rocks"));
    run("next.level");
    TEST_ASSERT_EQUAL_FLOAT(2, num(":level"));
    TEST_ASSERT_EQUAL_FLOAT(4, num(":level.rocks"));
    run("repeat 6 [next.level]");
    TEST_ASSERT_EQUAL_FLOAT(8, num(":level"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":rocks.top"), num(":level.rocks"),
                                    "a level asked for more larges than the ceiling");
}

// Score, level, and one heart a life -- one `write` and one `sentence`, built
// only where a displayed value moves. The heart is glyph 0x10 of
// devices/logo-font.h, which is why the expectation here is a raw byte: the
// HUD is one word of `char 16`s and not a printable stand-in.
void test_the_hud_carries_the_score_the_level_and_a_heart_a_life(void)
{
    run("init.game  make \"score 240  make \"level 2  make \"lives 3");
    run("refresh.hud");
    TEST_ASSERT_EQUAL_STRING("[SCORE 240 WAVE 2 \x10\x10\x10]",
                             value_to_string(eval_string(":hud.text").value));

    run("make \"lives 1  refresh.hud");
    TEST_ASSERT_EQUAL_STRING("[SCORE 240 WAVE 2 \x10]",
                             value_to_string(eval_string(":hud.text").value));
}

//==========================================================================
// Input and the level loop
//==========================================================================

// P is read outside the paused guard, or a paused game could never read the
// key that unpauses it; every other key has to be turned away while paused,
// which is the defect both shipped shooters had.
void test_pause_answers_p_and_nothing_else(void)
{
    setup_with(3);
    set_mock_input("p");
    run("play.frame");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":paused").value),
                                     "P did not pause");

    set_mock_input("q");
    run("play.frame");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", value_to_string(eval_string(":over").value),
                                     "a paused game answered the quit key");

    set_mock_input("p");
    run("play.frame");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", value_to_string(eval_string(":paused").value),
                                     "P did not lift the pause");
}

// A level that ends has to hand the screen back: leaving it in `sync` mode
// freezes the prompt, since nothing the user types appears until something
// presents.
void test_a_level_ends_on_q_and_puts_the_screen_back(void)
{
    run("init.game");
    run("make \"level.rocks 3");
    // Space then Q: one key a frame, so the player fires and then quits with
    // the shot still in flight.
    set_mock_input(" q");
    run("play.level");
    TEST_ASSERT_EQUAL_STRING("auto", value_to_string(eval_string("refreshmode").value));

    // And it hands the turtles back too. A shot is moved by the ENGINE, so one
    // left with a speed keeps gliding and keeps the demon poll working at the
    // prompt long after the game is over.
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("ask 1 [speed]"),
                                    "a shot was still gliding after the level ended");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false",
                                     value_to_string(eval_string("ask 1 [shown?]").value),
                                     "a shot was left on screen after the level ended");
}

// Clearing the board ends the level, and the check belongs in the loop rather
// than in `play.frame` -- a frame that ended the level would still have to draw
// it. Which of the three endings it was is read back by `one.game`.
void test_a_level_ends_when_the_board_is_clear(void)
{
    run("init.game");
    run("make \"level.rocks 1");
    proc_define_from_text("to spawn.rock :size\nend");   // an empty board
    run("play.level");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":over").value),
                                     "a cleared board did not end the level");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", value_to_string(eval_string(":quit").value),
                                     "a cleared board read as a quit");
    TEST_ASSERT_EQUAL_STRING("auto", value_to_string(eval_string("refreshmode").value));
}

// Clear means clear of SAUCERS too. A saucer still crossing when the last rock
// breaks would otherwise be wiped by the next level's `cs`, taking the points it
// was worth with it, so the wave stays open until it leaves or dies. `poll.input`
// runs first in every frame, which is where the first frame gets its saucer.
void test_a_saucer_holds_the_level_open_until_it_is_gone(void)
{
    run("init.game");
    run("make \"level.rocks 1");
    proc_define_from_text("to spawn.rock :size\nend");   // an empty board
    proc_define_from_text("to poll.input\n"
                          "if :frame.count > 0 [stop]\n"
                          "make \"sau.on 2  make \"sau.x 150  make \"sau.y 0\n"
                          "make \"sau.dx :sau.speed  make \"sau.dy 0\n"
                          "make \"sau.w 0  make \"sau.h 0\n"           // crossing, not hunting
                          "make \"sau.turn.in 999  make \"sau.fire.in 999  make \"warble.in 999\n"
                          "end");
    run("play.level");

    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":over").value),
                                     "the level did not end once the saucer left");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.on"), "the level ended with a saucer still on it");
    TEST_ASSERT_TRUE_MESSAGE(num(":frame.count") > 1,
                             "a cleared board ended the level with a saucer still crossing");
}

// And nothing takes its place: a cleared board holds the countdown where it is,
// so the wave cannot be held open by saucer after saucer, and the gap the next
// wave opens with is the one this wave left.
void test_a_cleared_board_spawns_no_further_saucer(void)
{
    setup_with(3);
    land_the_ship();
    run("make \"rocks.alive 0  make \"sau.on 0  make \"sau.wait 1");

    run("repeat 5 [step.saucer]");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.on"), "a cleared board spawned a saucer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":sau.wait"),
                                    "a cleared board ran the countdown down anyway");

    // One rock left and the countdown runs as it always did.
    run("make \"rocks.alive 1  step.saucer");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.on") > 0, "the countdown stayed held with a rock on the board");
}

// A dying ship steers nothing, fires nothing and cannot hyperspace out of its
// own explosion -- one guard in `poll.input` rather than one in each handler.
// Pause and quit sit ABOVE it deliberately: a death lasts most of a second and
// a player who wants out should not have to wait for the ring.
void test_a_dying_ship_answers_only_pause_and_quit(void)
{
    setup_with(3);
    run("make \"sh 90  make \"dying 5  make \"over false");

    set_mock_input(KEY_LEFT);
    run("poll.input");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(90, num(":sh"), "a dying ship steered");

    set_mock_input(" ");
    run("poll.input");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, item_of("slife", 1), "a dying ship fired");

    set_mock_input("p");
    run("poll.input");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":paused").value),
                                     "a dying game could not be paused");
    run("make \"paused false");

    set_mock_input("q");
    run("poll.input");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":quit").value),
                                     "a dying game could not be quit");
}

// Q means "back to the attract screen", not "the game ended": the difference is
// the game-over card, and `quit` is what tells them apart.
void test_q_quits_the_game_and_not_just_the_level(void)
{
    run("init.game");
    run("make \"level.rocks 3");
    set_mock_input("q");
    run("play.level");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":quit").value),
                                     "Q ended the level without ending the game");
    TEST_ASSERT_TRUE_MESSAGE(num(":rocks.alive") > 0, "the board was cleared, not quit");
}

// The state machine: levels advance while there are ships, the game ends when
// there are none, and only running out is worth a game-over card. `play.level`
// is stubbed to cost a life, so three levels is the whole game.
void test_a_game_plays_levels_until_the_ships_run_out(void)
{
    proc_define_from_text("to play.level\nmake \"lives :lives - 1\nend");
    proc_define_from_text("to attract.screen\nend");
    proc_define_from_text("to show.game.over\nmake \"card true\nend");
    run("make \"card false");
    run("one.game");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":level"), "the game did not advance a level a board");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5, num(":level.rocks"),
                                    "the third level did not ask for five larges");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":card").value),
                                     "running out of ships showed no game-over card");
}

// Q leaves without a card, which is the only difference between quitting and
// losing.
void test_quitting_shows_no_game_over_card(void)
{
    proc_define_from_text("to play.level\nmake \"quit true\nend");
    proc_define_from_text("to attract.screen\nend");
    proc_define_from_text("to show.game.over\nmake \"card true\nend");
    run("make \"card false");
    run("one.game");

    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", value_to_string(eval_string(":card").value),
                                     "quitting showed a game-over card");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":lives"), "quitting cost a ship");
}

// The attract screen carries the score table and the keys, as the two shipped
// shooters' do. It waits on space and nothing else.
void test_the_attract_screen_prints_the_scores_and_the_keys(void)
{
    mock_device_clear_output();
    set_mock_input("xy ");            // two keys it must ignore, then space
    run("attract.screen");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "ASTEROIDS"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Large rock      20"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Small rock     100"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Hyperspace"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Press Space"), screen);
}

void test_game_over_prints_the_final_score(void)
{
    run("init.game  make \"score 1240  make \"level 4");
    mock_device_clear_output();
    run("show.game.over");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "GAME OVER"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "1240"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "WAVE REACHED: 4"), screen);
}

//==========================================================================
// The saucer (M4)
//==========================================================================

// Put a saucer of a wanted size on the board through the game's own spawner,
// so its box, its countdowns and its size all come out exactly as play sets
// them, then park it where the test wants it. `random` on the mock is a
// constant without a seed, so the level is what picks the size: below
// `sau.small.at` the spawner cannot produce a small one at all.
static void saucer_of_size(int size, float x, float y)
{
    run("(rerandom 1)");
    // Size comes off the game-long gap and the score now, as the arcade's does:
    // a wide gap forces a large saucer, 30,000 points forces a small one.
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "make \"sau.gap %s  make \"score %d",
             size == 1 ? ":sau.gap.min" : ":sau.gap.start",
             size == 1 ? 40000 : 0);
    run(cmd);
    for (int tries = 0; tries < 50 && (int)num(":sau.on") != size; tries++)
        run("spawn.saucer");
    TEST_ASSERT_EQUAL_INT_MESSAGE(size, (int)num(":sau.on"),
                                  "the spawner never produced the wanted saucer");
    snprintf(cmd, sizeof(cmd), "make \"sau.x %g  make \"sau.y %g  make \"sau.dx 0  make \"sau.dy 0",
             x, y);
    run(cmd);
}

// The saucers come off the same generator as the rocks and the ship, so they
// get the same guarantee: eight segments that arrive back where they started.
// They are the first shapes whose prologue turns before it walks -- a saucer
// has no vertex on its centreline -- so a broken `lt` in the prologue would
// show up here as a walk that no longer closes.
void test_both_saucer_outlines_close_on_themselves(void)
{
    assert_outline_closes("saucer.l", SEG_SAUCER);
    assert_outline_closes("saucer.s", SEG_SAUCER);
}

// A saucer does not rotate, so `draw.saucer` always places it at heading 0 and
// the two sizes are two outlines rather than one scaled walk.
void test_draw_saucer_picks_the_outline_for_the_size(void)
{
    const int sizes[] = {2, 1};
    for (int k = 0; k < 2; k++)
    {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "make \"sau.on %d  make \"sau.x 0  make \"sau.y 0", sizes[k]);
        run("clean  pu setx 0 sety 0 seth 0");
        run(cmd);
        mock_device_clear_graphics();
        run("draw.saucer");
        snprintf(cmd, sizeof(cmd), "saucer size %d drew %d segments", sizes[k],
                 mock_device_line_count());
        TEST_ASSERT_EQUAL_INT_MESSAGE(SEG_SAUCER, mock_device_line_count(), cmd);
    }
}

// How far the saucer currently drawn reaches from its centre, along each axis.
static void saucer_drawn_extent(float *half_w, float *half_h)
{
    run("clean  pu setx 0 sety 0 seth 0");
    mock_device_clear_graphics();
    run("draw.saucer");
    *half_w = *half_h = 0.0f;
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        if (fabsf(l->x1) > *half_w) *half_w = fabsf(l->x1);
        if (fabsf(l->y1) > *half_h) *half_h = fabsf(l->y1);
    }
}

// The third time this design has had to hold a collision box against the shape
// drawn inside it, and the first time it is a RECTANGLE: a saucer is 32 steps
// wide and 18 tall, so one radius would either award shots that passed well
// over it or let them through its nose. The lesson underneath is M2's and M3's
// -- a flat allowance is proportionally worst on the smallest object -- and the
// smallest object here is the small saucer's height.
void test_the_saucer_boxes_are_not_far_wider_than_the_shapes_drawn_in_them(void)
{
    const int sizes[] = {2, 1};
    for (int k = 0; k < 2; k++)
    {
        saucer_of_size(sizes[k], 0, 0);
        float drawn_w, drawn_h;
        saucer_drawn_extent(&drawn_w, &drawn_h);

        char msg[192];
        float box_w = num(":sau.w"), box_h = num(":sau.h");
        snprintf(msg, sizeof(msg),
                 "size %d has a %.0f-step half-width around a %.1f-step outline",
                 sizes[k], box_w, drawn_w);
        TEST_ASSERT_TRUE_MESSAGE((box_w - drawn_w) / drawn_w <= 0.30f, msg);
        snprintf(msg, sizeof(msg),
                 "size %d has a %.0f-step half-height around a %.1f-step outline",
                 sizes[k], box_h, drawn_h);
        TEST_ASSERT_TRUE_MESSAGE((box_h - drawn_h) / drawn_h <= 0.30f, msg);

        // And not cut below the shape either, or shots would pass through the
        // hull without registering.
        TEST_ASSERT_TRUE_MESSAGE(box_w >= drawn_w, msg);
        TEST_ASSERT_TRUE_MESSAGE(box_h >= drawn_h, msg);

        // The box a SHIP is tested against is a different one -- the saucer's
        // plus the ship's own half-width -- and it gets §8's rule, because this
        // is the constant class that has been too generous twice: it must not
        // kill further out than the two drawn shapes can touch, and must not be
        // cut inside the ship's own beam.
        const float beam = 8.95f;
        float ship_w = box_w + num(":ship.rad");
        float ship_h = box_h + num(":ship.rad");
        snprintf(msg, sizeof(msg),
                 "size %d kills a ship at %.1f x %.1f where the drawn shapes touch "
                 "at %.1f x %.1f", sizes[k], ship_w, ship_h,
                 drawn_w + beam, drawn_h + beam);
        TEST_ASSERT_TRUE_MESSAGE(ship_w <= drawn_w + beam, msg);
        TEST_ASSERT_TRUE_MESSAGE(ship_h <= drawn_h + beam, msg);
        TEST_ASSERT_TRUE_MESSAGE(ship_h > beam, msg);
    }
}

// The same bound as the rocks' (section 7.3), applied to a target that is not
// round -- so it is checked PER AXIS, because that is how the box is tested.
// The vertical one is the tight one and it is why the saucer is 1.8:1 rather
// than the arcade's 2.5:1: a shot travels 11.4 steps a frame, and a saucer flat
// enough to look right can be flown clean through between two samples.
void test_a_shot_cannot_outrun_the_saucer(void)
{
    float travel = num(":shot.speed") / num(":fps");
    saucer_of_size(1, 0, 0);                         // the small one, the tight case

    float across = (travel + num(":sau.speed")) * 1.3f;
    float down = (travel + num(":sau.jink")) * 1.3f;

    char msg[192];
    snprintf(msg, sizeof(msg),
             "a shot and a small saucer close by %.1f steps a frame across a "
             "%.0f-step box -- shots will pass through it", across, 2 * num(":sau.w"));
    TEST_ASSERT_TRUE_MESSAGE(across <= 2 * num(":sau.w"), msg);

    snprintf(msg, sizeof(msg),
             "a shot coming down closes %.1f steps a frame against a %.0f-step box -- "
             "make the saucer taller or slow the shot", down, 2 * num(":sau.h"));
    TEST_ASSERT_TRUE_MESSAGE(down <= 2 * num(":sau.h"), msg);
}

// And the same bound in the other direction, which is why the saucer's shot is
// slower than the player's rather than the same speed: the box it has to hit is
// the SHIP, and a ship is 9 steps across the beam where a large rock's box is
// 24. The ship's own top speed is in the sum because the ship is the thing
// closing.
void test_a_saucer_shot_cannot_outrun_the_ship(void)
{
    float travel = num(":sau.shot.speed") / num(":fps");
    float closing = (travel + num(":speed.max")) * 1.3f;
    float box = 2.0f * num(":sau.shot.rad");

    char msg[192];
    snprintf(msg, sizeof(msg),
             "a saucer shot and the ship close by %.1f steps a frame against a "
             "%.0f-step box -- lower sau.shot.speed or raise sau.shot.rad",
             closing, box);
    TEST_ASSERT_TRUE_MESSAGE(closing <= box, msg);
}

// A saucer arrives on a countdown, from one edge or the other, at a height that
// is clear of the HUD band -- a saucer crossing under the score is a saucer the
// player cannot see.
void test_a_saucer_appears_on_a_countdown_and_enters_from_an_edge(void)
{
    setup_with(3);
    land_the_ship();
    run("make \"ship.rad 0  make \"sau.wait 3");

    run("play.frame  play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.on"), "a saucer arrived before its countdown");

    run("play.frame");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.on") > 0, "the countdown ran out and no saucer came");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(num(":sau.speed") + 0.01f, 160.0f, fabsf(num(":sau.x")),
                                     "a saucer did not enter from an edge");
    TEST_ASSERT_TRUE_MESSAGE(fabsf(num(":sau.y")) <= 130.0f,
                             "a saucer entered under the HUD band");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, num(":sau.speed"), fabsf(num(":sau.dx")),
                                     "a saucer entered at the wrong speed");
}

// It crosses and it LEAVES -- the one object in the game that does not wrap in
// x, because leaving is how its visit ends. Vertically it wraps like everything
// else, so a jink cannot take it off the top of the field for good.
void test_a_saucer_crosses_and_leaves_at_the_far_edge(void)
{
    setup_with(3);
    land_the_ship();
    run("make \"ship.rad 0");
    saucer_of_size(2, -160, 0);
    run("make \"sau.dx :sau.speed  make \"level 4");
    run("make \"sau.w 0  make \"sau.h 0");     // it is crossing, not hunting

    int frames = 0;
    for (; frames < 400 && num(":sau.on") > 0; frames++)
        run("step.saucer");

    char msg[128];
    snprintf(msg, sizeof(msg), "a saucer took %d frames to cross a 320-step field", frames);
    TEST_ASSERT_TRUE_MESSAGE(frames > 150 && frames < 220, msg);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.gap"), num(":sau.wait"),
                                    "leaving did not set the wait for the next one");

    // And the vertical wrap, which a jink needs: pushed off the top it comes
    // back at the bottom rather than sailing away.
    saucer_of_size(2, 0, 158);
    run("make \"sau.dy 5  make \"sau.w 0  make \"sau.h 0");
    run("step.saucer");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.y") < 0, "a saucer did not wrap over the top");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.on") > 0, "a vertical wrap ended the saucer's visit");
}

// The arcade keeps ONE countdown for the whole game: a reload value that starts
// wide and loses `sau.gap.step` every time a saucer spawns, floored. So saucers
// arrive steadily more often the longer a player survives, whatever level they
// are on -- and a new game starts gentle again.
void test_the_gap_between_saucers_shortens_with_every_saucer(void)
{
    run("init.game");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.gap.start"), num(":sau.gap"),
                                    "a new game did not start on the widest gap");

    run("spawn.saucer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.gap.start") - num(":sau.gap.step"),
                                    num(":sau.gap"), "spawning did not shorten the gap");

    // Down to the floor and no further, however long the game runs.
    run("repeat 60 [spawn.saucer]");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.gap.min"), num(":sau.gap"),
                                    "the gap fell through its floor");

    // A level change does not reset it -- the timer belongs to the game.
    run("make \"level.rocks 3  setup.level");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.gap.min"), num(":sau.gap"),
                                    "a new level reset the game-long saucer gap");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.gap"), num(":sau.wait"),
                                    "a new level did not start the countdown at the gap");

    // And a new game does.
    run("init.game");
    TEST_ASSERT_EQUAL_FLOAT(num(":sau.gap.start"), num(":sau.gap"));
}

// `BMI SetScrStatus` in the arcade's routine: while the reload value still has
// bit 7 set -- 128 or more -- every saucer is large, which is the first four of
// a game, because the size is read before the gap is stepped down. Then 30,000
// points makes every saucer small, and in between it is a coin flip. So a player
// on level one meets large saucers only, and the small one has to be earned
// rather than waited for.
void test_the_saucer_size_follows_the_gap_and_the_score(void)
{
    run("init.game  (rerandom 1)");

    // Early game: large only, whatever the level says.
    run("make \"level 9  make \"score 0");
    for (int i = 0; i < 20; i++)
    {
        run("make \"sau.gap :sau.gap.start  spawn.saucer");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":sau.on"),
                                        "a small saucer appeared while the gap was still wide");
    }

    // And the run is four long, not three: `spawn.saucer` reads the size before
    // it steps the gap down, so the fourth spawn still sees 146 - 3*6 = 128.
    run("init.game");
    for (int i = 0; i < 4; i++)
    {
        run("spawn.saucer");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":sau.on"),
                                        "one of the first four saucers of a game was not large");
    }
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(122, num(":sau.gap"),
                                    "four spawns did not step the gap past the large-saucer bound");

    // 30,000 points: small only, however wide the gap.
    run("make \"score :sau.small.score");
    for (int i = 0; i < 20; i++)
    {
        run("make \"sau.gap :sau.gap.min  spawn.saucer");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, num(":sau.on"),
                                        "a large saucer appeared above the small-saucer score");
    }

    // In between: both, roughly evenly.
    run("make \"score 1000");
    int small = 0;
    for (int i = 0; i < 60; i++)
    {
        run("make \"sau.gap :sau.gap.min  spawn.saucer");
        if ((int)num(":sau.on") == 1)
            small++;
    }
    char msg[96];
    snprintf(msg, sizeof(msg), "%d small saucers in 60 spawns between the thresholds", small);
    TEST_ASSERT_TRUE_MESSAGE(small > 10 && small < 50, msg);
}

// The other half of what makes the small saucer frightening: its aim tightens
// as the score climbs, from a wide scatter at nothing to nearly exact past
// `sau.aim.score`, and it never opens up again.
void test_the_small_saucer_aim_tightens_with_the_score(void)
{
    run("init.game  make \"score 0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.aim.wide"), num("aim.spread"),
                                    "a beginner's saucer is not scattering widely");

    run("make \"score :sau.aim.score");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.aim.tight"), num("aim.spread"),
                                    "the saucer never becomes accurate");

    run("make \"score 999999");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.aim.tight"), num("aim.spread"),
                                    "the spread went past its floor");

    // Monotone in between, and always a whole number of degrees, because
    // `random` counts.
    float last = 999;
    for (int score = 0; score <= 40000; score += 2500)
    {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "make \"score %d", score);
        run(cmd);
        float spread = num("aim.spread");
        TEST_ASSERT_TRUE_MESSAGE(spread <= last, "the aim widened as the score rose");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(spread, (float)(int)spread,
                                        "the spread is not a whole number of degrees");
        last = spread;
    }
}

// A shot on a saucer takes the saucer, the score and the shot -- the same three
// things a shot on a rock takes, through the same `add.score`, which is why the
// HUD repaint needed no new caller.
void test_a_shot_kills_a_saucer_scores_it_and_is_consumed(void)
{
    setup_with(0);
    land_the_ship();
    run("make \"ship.rad 0  make \"shipcx 9999");
    saucer_of_size(2, 40, 40);
    run(".setitem 1 :slife 5  make \"s1x 40  make \"s1y 44");
    run("make \"score 0");

    run("step.saucer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.on"), "a shot inside the box missed");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.score.l"), num(":score"),
                                    "a large saucer paid the wrong score");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, item_of("slife", 1), "the shot was not consumed");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9999, num(":s1x"), "the spent shot was not parked");

    // The small one is worth five times as much, and it is the same path.
    saucer_of_size(1, 40, 40);
    run(".setitem 1 :slife 5  make \"s1x 40  make \"s1y 44  make \"score 0");
    run("step.saucer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.score.s"), num(":score"),
                                    "a small saucer paid the wrong score");
}

// Flying into a saucer costs a life and takes the saucer with it -- which is
// the player's way round, and the reason the shot test runs before the ship
// test inside `step.saucer`.
void test_a_saucer_on_the_ship_kills_the_ship(void)
{
    setup_with(0);
    land_the_ship();
    run("make \"dying 0  make \"lives 3");
    run("make \"shipx 0  make \"shipy 0  make \"shipcx 0  make \"shipcy 0");
    saucer_of_size(2, 5, 0);

    run("step.saucer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"), "a saucer on the ship cost no life");
    TEST_ASSERT_TRUE_MESSAGE(num(":dying") > 0, "the ship did not explode");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.on"), "the saucer survived the collision");

    // And a ship inside its respawn grace is parked at 9999, so a saucer
    // sitting exactly on it cannot take a life -- the same idiom the rocks meet.
    run("make \"dying 0  make \"lives 3  respawn");
    saucer_of_size(2, 0, 0);
    run("step.saucer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":lives"), "a saucer killed a ship in its grace");
}

// The saucer's shot is flown by the engine like the player's, so all Logo does
// is age it, read it back and test it against the ship -- one pair, once a
// frame, rather than anything in the rock pass.
void test_a_saucer_shot_kills_the_ship(void)
{
    setup_with(0);
    land_the_ship();
    run("make \"dying 0  make \"lives 3");
    run("make \"shipx 0  make \"shipy 0  make \"shipcx 0  make \"shipcy 0");
    run("make \"sau.shot.in 5");
    run("ask 4 [pu setx 4 sety 0 setspeed 0]");

    run("step.sau.shot");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"), "a saucer shot on the ship cost no life");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9999, num(":sau.shot.x"),
                                    "the shot that killed the ship was not parked");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.shot.in"), "the spent shot is still alive");

    // Out of the box it does nothing, and a ship in its grace cannot be hit at
    // all -- `shipcx` is 9999 there, so the first comparison turns it away.
    run("make \"dying 0  make \"lives 3  respawn  make \"sau.shot.in 5");
    run("ask 4 [pu setx 0 sety 0 setspeed 0]");
    run("step.sau.shot");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":lives"),
                                    "a saucer shot killed a ship inside its grace");
}

// A saucer shot that flies at the ship in a REAL frame loop must kill it. The
// static test above places the shot on the ship and calls `step.sau.shot`; this
// one fires from across the field, advances the mock clock as `setspeed` does
// in play, and asks whether the ship ever dies. Reported from a board: it never
// did.
void test_a_saucer_shot_fired_across_the_field_kills_the_ship(void)
{
    setup_with(0);                       // an empty board: nothing to absorb it
    land_the_ship();
    run("make \"shipx 0  make \"shipy 0  make \"svx 0  make \"svy 0");
    run("make \"shipcx 0  make \"shipcy 0  make \"lives 3  make \"dying 0");
    saucer_of_size(1, -120, 0);          // the small one aims
    run("make \"sau.dx 0  make \"sau.dy 0  make \"score 40000");
    run("saucer.fires");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.shot.in") > 0, "the saucer fired nothing");

    int frames = 0;
    for (; frames < (int)num(":sau.shot.life") + 2; frames++)
    {
        set_mock_ticks(mock_ticks_value + (uint32_t)(1000.0f / num(":fps")));
        run("play.frame");
        if (num(":lives") < 3)
            break;
    }

    char msg[192];
    snprintf(msg, sizeof(msg),
             "a saucer shot fired straight at the ship from 120 steps away never hit it "
             "in %d frames -- shot at %g,%g, ship at %g,%g, box %g",
             frames, num(":sau.shot.x"), num(":sau.shot.y"),
             num(":shipcx"), num(":shipcy"), num(":sau.shot.rad"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"), msg);
}

// Reported from a board: saucer shots "never kill the player -- just pass right
// through". They do pass right through, and the reason is geometry rather than
// timing. The ship is a triangle 24 steps long: its nose reaches 12 steps from
// the centre and its rear corners 12.7, while its BEAM is only 9. `sau.shot.rad`
// was 9 -- the beam -- so a shot crossing the nose or the tail was 10 to 12
// steps out, visibly inside the drawn hull and outside the box that decides.
//
// This is the mirror of the mistake this design made twice in the other
// direction (`shot.reach` at M2, `ship.rad` at M3, both too generous). A box
// taken from the narrowest measurement of a shape reads as the game cheating
// just as surely as one taken from the widest.
void test_a_saucer_shot_through_the_ships_nose_kills_it(void)
{
    setup_with(0);
    land_the_ship();
    run("make \"shipx 0  make \"shipy 0  make \"sh 0  make \"svx 0  make \"svy 0");
    run("make \"shipcx 0  make \"shipcy 0  make \"dying 0");

    // Every point the drawn hull actually occupies, from the outline itself.
    run("clean  setpc 254  pu setx 0 sety 0 seth 0");
    mock_device_clear_graphics();
    run("ship");
    int segments = mock_device_line_count();
    TEST_ASSERT_EQUAL_INT(SEG_SHIP, segments);

    for (int i = 0; i < segments; i++)
    {
        const MockLine *l = mock_device_get_line(i);
        char msg[160];
        // A shot sitting exactly on a vertex of the drawn ship must kill it:
        // the player can see it there.
        run("make \"lives 3  make \"dying 0  make \"shipcx 0  make \"shipcy 0");
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "make \"sau.shot.in 5  make \"sau.shot.x %g  make \"sau.shot.y %g",
                 l->x1, l->y1);
        run(cmd);
        run("ask 4 [pu setx :sau.shot.x sety :sau.shot.y setspeed 0]");
        run("step.sau.shot");

        snprintf(msg, sizeof(msg),
                 "a saucer shot on the hull at %.1f,%.1f (%.1f steps out) did not kill the ship "
                 "-- sau.shot.rad is %g", l->x1, l->y1,
                 sqrtf(l->x1 * l->x1 + l->y1 * l->y1), num(":sau.shot.rad"));
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"), msg);
    }
}

// And the other side of it, because this design has been burned there twice: the
// box must not reach past the hull it stands for. A shot two steps clear of the
// ship's longest point must miss.
void test_a_saucer_shot_clear_of_the_ship_misses(void)
{
    setup_with(0);
    land_the_ship();
    run("make \"shipx 0  make \"shipy 0  make \"shipcx 0  make \"shipcy 0  make \"dying 0");
    run("make \"lives 3");

    const float corner = 12.73f;        // the ship's longest reach, at the rear corners
    char cmd[160];
    snprintf(cmd, sizeof(cmd), "make \"sau.shot.in 5  make \"sau.shot.x %g  make \"sau.shot.y 0",
             corner + 3.0f);
    run(cmd);
    run("ask 4 [pu setx :sau.shot.x sety :sau.shot.y setspeed 0]");
    run("step.sau.shot");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":lives"),
                                    "a saucer shot clear of the whole ship still killed it");
}

// It expires on its own count, and stops and hides its turtle when it does --
// a turtle left with a speed keeps gliding after the game hands the screen back.
void test_a_saucer_shot_expires_and_stops_its_turtle(void)
{
    setup_with(0);
    run("make \"shipcx 9999  make \"sau.shot.in 3");
    run("ask 4 [pu setx 100 sety 100 st setspeed 60]");

    run("step.sau.shot  step.sau.shot");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.shot.in") > 0, "the shot expired early");

    run("step.sau.shot");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.shot.in"), "the shot outlived its count");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("ask 4 [speed]"), "an expired shot is still flying");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", value_to_string(eval_string("ask 4 [shown?]").value),
                                     "an expired shot is still on screen");
}

// A shot in the air outlives the saucer that fired it. It is stepped from
// `step.shots` and not from `step.saucer` for exactly this reason: the engine is
// flying it, and nothing about its saucer leaving or being shot should make it
// vanish in mid-flight.
void test_a_saucer_shot_outlives_the_saucer_that_fired_it(void)
{
    setup_with(0);
    land_the_ship();
    run("make \"ship.rad 0  make \"shipcx 9999");
    saucer_of_size(2, 40, 40);
    run("saucer.fires");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.shot.in") > 0, "the saucer fired nothing");

    run("kill.saucer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.on"), "the saucer survived");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.shot.in") > 0,
                             "killing the saucer took its shot out of the air");

    run("step.shots");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.shot.in") > 0, "the orphaned shot stopped being stepped");
}

// The large saucer fires anywhere; the small one fires at where the ship IS,
// which is what makes it the dangerous one. `arctan` gives the bearing and
// Logo's heading is clockwise from north, so the conversion is one statement --
// and if it were wrong the small saucer would fire at a mirror image of the
// ship, which is exactly the kind of error a test has to catch rather than an
// eye.
void test_the_small_saucer_aims_at_the_ship_and_the_large_one_does_not(void)
{
    setup_with(0);
    land_the_ship();
    run("(rerandom 1)  make \"ship.rad 0  make \"shipcx 9999");
    run("make \"shipx 100  make \"shipy 0");        // due east of a saucer at the origin

    saucer_of_size(1, 0, 0);
    for (int i = 0; i < 20; i++)
    {
        run("saucer.fires");
        float h = num("ask 4 [heading]");
        char msg[128];
        snprintf(msg, sizeof(msg), "a small saucer fired on heading %g, not near 90", h);
        TEST_ASSERT_TRUE_MESSAGE(fabsf(h - 90.0f) <= num("aim.spread") + 0.5f, msg);
    }

    // The large one is not aimed, so twenty shots spread out. One heading being
    // near the ship is chance; all twenty would mean the sizes had swapped.
    saucer_of_size(2, 0, 0);
    int away = 0;
    for (int i = 0; i < 20; i++)
    {
        run("saucer.fires");
        if (fabsf(num("ask 4 [heading]") - 90.0f) > num("aim.spread") + 0.5f)
            away++;
    }
    TEST_ASSERT_TRUE_MESSAGE(away > 10, "a large saucer fired at the ship every time");
}

// B22. A ship that is exploding or waiting to appear is NOT a target, which is
// the arcade's arrangement for a structural reason: there, a destroyed ship is
// a deactivated object with no position at all until a new one is created at
// the centre, so the aiming routine has nothing to read.
//
// This port has a position to read, and that is the whole bug. `respawn` parks
// the waiting ship ON the spawn point so the rock pass doubles as the
// clear-check, and `saucer.fires` reads the DRAWN position -- so the small
// saucer spent the death and the wait walking tightly-aimed shots into the
// exact point the player was about to materialise at, stationary, with the fire
// gap (17 frames) longer than the explosion (10) so the next one landed just
// after the ship did. Reported from a board as dying to the small saucer over
// and over with no chance to escape.
//
// The count is what this has to test rather than a single shot: the spread is
// 4 degrees at this score, so one unaimed shot lands near the ship often enough
// to pass by luck.
static int shots_near_north(int n)
{
    int near = 0;
    for (int i = 0; i < n; i++)
    {
        run("saucer.fires");
        float h = num("ask 4 [heading]");
        float spread = num("aim.spread") + 0.5f;
        if (fabsf(h) <= spread || fabsf(h - 360.0f) <= spread)
            near++;
    }
    return near;
}

void test_a_saucer_does_not_range_on_a_ship_that_is_not_there(void)
{
    setup_with(0);

    // The control: a live ship due north of the saucer is aimed at every time.
    land_the_ship();
    run("(rerandom 1)  make \"shipx 0  make \"shipy 100");
    saucer_of_size(1, 0, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, shots_near_north(20),
                                  "a small saucer did not aim at a live ship");

    // Waiting: parked on the spawn point, due north of the saucer. An aimed
    // saucer would be ranging on the point the player is about to appear at.
    run("respawn");
    run("make \"sau.x 0  make \"sau.y -100");
    TEST_ASSERT_TRUE_MESSAGE(shots_near_north(20) < 10,
                             "a small saucer ranged on the spawn point during a respawn wait");

    // Dying: the same, while the explosion is still counting down.
    run("land.ship  make \"shipx 0  make \"shipy 100  make \"dying :death.frames");
    TEST_ASSERT_TRUE_MESSAGE(shots_near_north(20) < 10,
                             "a small saucer ranged on a ship that was still exploding");
}

// B23. `step.saucer`'s ship test runs `ship.hit` and then `kill.saucer`. During
// a respawn wait `ship.hit` returns early -- it sets `blocked` and takes no
// life -- but `kill.saucer` ran anyway, so a saucer that crossed an EMPTY spawn
// point was destroyed and paid the player 200 or 1000 points for it. With
// `ship.rad` widened to `clear.rad` for the wait, the box that handed out those
// points was 26 steps bigger than the ship's, too.
//
// The saucer is supposed to block the respawn there, which is the design's own
// rule -- "an area with a saucer crossing it is not clear" -- not die on it.
void test_a_saucer_over_the_spawn_point_blocks_the_respawn_without_dying(void)
{
    setup_with(0);
    saucer_of_size(2, 0, 0);                  // sitting right on the spawn point
    run("respawn");                           // waiting, parked on it, wide box
    run("make \"score 0  make \"blocked false");

    run("step.saucer");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":sau.on"),
                                    "the saucer died on an empty spawn point");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":score"),
                                    "an empty spawn point paid the player for a saucer");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":blocked").value),
                                     "a saucer over the spawn point did not block the respawn");

    // And the wait really does hold: `step.wait` reads last frame's answer.
    run("step.wait");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":waiting").value),
                                     "the ship landed under a saucer");
}

// The frame draws the world, the ship AND the saucer, and the guard is in the
// frame rather than in `draw.saucer` -- so a frame with no saucer pays one
// comparison and not a call.
void test_a_frame_with_a_saucer_up_draws_it(void)
{
    setup_with(6);
    land_the_ship();
    run("make \"ship.rad 0");
    saucer_of_size(2, 120, 120);
    run("make \"sau.w 0  make \"sau.h 0");   // nothing to collide with mid-test

    mock_device_clear_graphics();
    run("play.frame");
    TEST_ASSERT_EQUAL_INT_MESSAGE(6 * SEG_LARGE + SEG_SHIP + SEG_SAUCER,
                                  mock_device_line_count(),
                                  "a frame with a saucer up did not draw it");

    run("make \"sau.on 0");
    mock_device_clear_graphics();
    run("play.frame");
    TEST_ASSERT_EQUAL_INT_MESSAGE(6 * SEG_LARGE + SEG_SHIP, mock_device_line_count(),
                                  "a frame with no saucer drew one anyway");
}

// A level starts with no saucer and nothing of the last one's in the air. The
// shot matters more than the saucer: a turtle left with a speed is moved by the
// ENGINE, so it would still be gliding across the next level's screen.
void test_a_level_starts_with_no_saucer_and_nothing_in_the_air(void)
{
    setup_with(3);
    saucer_of_size(2, 0, 0);
    run("saucer.fires");

    run("make \"level.rocks 3  setup.level");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.on"), "a saucer survived a level change");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.gap"), num(":sau.wait"),
                                    "a new level did not start the countdown at the game-long gap");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.shot.in"), "a saucer shot survived a level change");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("ask 4 [speed]"),
                                    "a saucer shot was left gliding into the next level");
}

//==========================================================================
// The pause, the saucer's shot on the rocks, and the saucer on the rocks
//==========================================================================

// A shot is flown by the ENGINE, not by this game: `setspeed` moves it on
// wall-clock time whether or not `play.frame` steps anything. So a pause that
// only stops calling the frame body leaves every shot in the air still
// travelling, and a player who pauses to think comes back to a shot that
// crossed the field without them. Reported from a board and logged as B17.
//
// Driven off the mock clock, because that is what `setspeed` reads.
void test_a_pause_holds_the_shots_where_they_are(void)
{
    setup_with(0);
    land_the_ship();
    run("make \"shipx 0  make \"shipy 0  make \"sh 0");
    run("fire");
    run("make \"sau.shot.in 20");
    run("ask 4 [pu setx -100 sety 0 seth 90 st setspeed :sau.shot.speed]");

    // Let both fly for a moment so they are genuinely in motion.
    set_mock_ticks(mock_ticks_value + 200);
    run("ask 1 [make \"probe1 ycor]  ask 4 [make \"probe4 xcor]");
    float player_before = num(":probe1"), saucer_before = num(":probe4");
    TEST_ASSERT_TRUE_MESSAGE(player_before > 0, "the player's shot never left the ship");

    set_mock_input("p");
    run("play.frame");
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string(":paused").value));

    // Half a second of wall clock with the game paused, and several frames.
    run("ask 1 [make \"probe1 ycor]  ask 4 [make \"probe4 xcor]");
    player_before = num(":probe1");
    saucer_before = num(":probe4");
    set_mock_ticks(mock_ticks_value + 500);
    run("repeat 5 [play.frame]");
    run("ask 1 [make \"probe1 ycor]  ask 4 [make \"probe4 xcor]");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(player_before, num(":probe1"),
                                    "a paused game let the player's shot keep flying");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(saucer_before, num(":probe4"),
                                    "a paused game let the saucer's shot keep flying");

    // And unpausing puts them back in the air at the speed they had.
    set_mock_input("p");
    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":shot.speed"), num("ask 1 [speed]"),
                                    "the player's shot did not resume");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.shot.speed"), num("ask 4 [speed]"),
                                    "the saucer's shot did not resume");

    // A spent shot is not resurrected by unpausing: `thaw` resumes what was
    // moving, and a stopped turtle stays stopped.
    run("clear.shots  make \"paused false  toggle.pause  toggle.pause");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("ask 1 [speed]"),
                                    "unpausing restarted a spent shot");
}

// The arcade lets the saucer's shot break rocks, and the player gets nothing
// for them: the board you were about to be paid for is eaten while you dodge.
// It is folded into `shot.on` rather than tested separately, so it costs one
// comparison a rock and no branch.
void test_a_saucer_shot_breaks_a_rock_and_pays_nobody(void)
{
    setup_with(0);
    land_the_ship();
    run("make \"shipcx 9999  make \"score 0");
    run(".setitem 1 :rsize 3  .setitem 1 :rrad rad.for 3  make \"rocks.alive 1");
    run(".setitem 1 :rx 0  .setitem 1 :ry 0  .setitem 1 :rdx 0  .setitem 1 :rdy 0");
    run("make \"sau.shot.in 10  make \"sau.shot.x 2  make \"sau.shot.y 2");

    run("step.draw.all");
    // The parent's slot is freed before the children are made, so slot 1 holds a
    // child now -- what says the large rock died is the size and the count.
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, item_of("rsize", 1),
                                    "the saucer's shot missed the rock");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":rocks.alive"), "the large rock did not split");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":score"),
                                    "the player was paid for a rock the saucer shot");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.shot.in"),
                                    "the saucer's shot survived the rock it broke");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9999, num(":sau.shot.x"), "the spent shot was not parked");

    // A player's shot on the same rock still pays.
    run("clear.rocks  make \"score 0");
    run(".setitem 1 :rsize 3  .setitem 1 :rrad rad.for 3  make \"rocks.alive 1");
    run(".setitem 1 :rx 0  .setitem 1 :ry 0  .setitem 1 :rdx 0  .setitem 1 :rdy 0");
    run(".setitem 1 :slife 5  make \"s1x 2  make \"s1y 2");
    run("step.draw.all");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(20, num(":score"), "a player's shot stopped paying");
}

// Flying into a rock kills the saucer and breaks the rock, and neither of them
// pays the player -- nobody aimed it.
void test_a_saucer_that_flies_into_a_rock_dies_with_it(void)
{
    setup_with(0);
    land_the_ship();
    run("make \"shipcx 9999  make \"score 0");
    saucer_of_size(2, 0, 0);
    run(".setitem 1 :rsize 2  .setitem 1 :rrad rad.for 2  make \"rocks.alive 1");
    run(".setitem 1 :rx 0  .setitem 1 :ry 0  .setitem 1 :rdx 0  .setitem 1 :rdy 0");

    run("step.draw.all");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.on"), "the saucer survived flying into a rock");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1, item_of("rsize", 1), "the rock survived the saucer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":rocks.alive"), "the rock did not split");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":score"),
                                    "the player was paid for a saucer that killed itself");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9999, num(":sau.x"), "the dead saucer was not parked");

    // Shooting it still pays, which is the whole difference.
    run("clear.rocks  make \"score 0");
    saucer_of_size(2, 40, 40);
    run(".setitem 1 :slife 5  make \"s1x 40  make \"s1y 44");
    run("step.saucer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":sau.score.l"), num(":score"),
                                    "shooting a saucer stopped paying");
}

// A saucer that does not exist is parked at 9999, exactly as a spent shot is,
// because every rock is tested against it. Parked at the origin instead -- which
// is where the state block used to leave it -- a rock drifting through the
// middle of the field would explode against a saucer that was not there.
void test_a_saucer_that_is_not_there_hits_nothing(void)
{
    setup_with(0);
    land_the_ship();
    run("make \"shipcx 9999  make \"sau.on 0  make \"sau.w 0  make \"sau.h 0");
    run(".setitem 1 :rsize 3  .setitem 1 :rrad rad.for 3  make \"rocks.alive 1");
    run(".setitem 1 :rx 0  .setitem 1 :ry 0  .setitem 1 :rdx 0  .setitem 1 :rdy 0");

    run("step.draw.all");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, item_of("rsize", 1),
                                    "a rock at the origin hit a saucer that was not there");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("shot.on 0 0 26"),
                                    "an absent saucer answered a collision test");
}

//==========================================================================
// The sound (M4)
//==========================================================================

// Read the mock's gate log from a mark, counting notes on one voice. Every
// `sound` here names a PAIR, so a note shows up twice -- once per ear -- and
// counting one ear counts notes.
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

// Four pairs, set once, and the arrangement has one rule that is not a matter
// of taste: voices 3 and 7 are the NOISE voices and the rest are tone voices,
// and a noise waveform on a tone voice is an error rather than a shrug. The
// design's section 11 asked for white noise on [2 6] -- which cannot be done --
// so the thrust rumble is a narrow pulse instead, and the noise pair is left to
// the explosions, which must never be silenced by a held thrust key.
void test_the_timbres_are_set_once_and_match_the_voice_kinds(void)
{
    run("setup.sound");
    const MockDeviceState *st = mock_device_get_state();

    // A square and not a triangle, and that is a hardware finding rather than a
    // taste: a triangle's harmonics fall as 1/n^2, so a low triangle on the
    // PicoCalc's speaker is only its onset click (B21).
    TEST_ASSERT_EQUAL_INT_MESSAGE(SOUND_WAVE_SQUARE, st->sound.wave[0].wave, "the heartbeat");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SOUND_WAVE_SAWTOOTH, st->sound.wave[1].wave, "fire and the saucer");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SOUND_WAVE_PULSE, st->sound.wave[2].wave, "the thrust rumble");
    TEST_ASSERT_EQUAL_INT_MESSAGE(SOUND_WAVE_WHITE, st->sound.wave[3].wave, "the explosions");

    for (int v = 0; v < 4; v++)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "voice %d and %d are not the same timbre", v, v + 4);
        TEST_ASSERT_EQUAL_INT_MESSAGE(st->sound.wave[v].wave, st->sound.wave[v + 4].wave, msg);

        snprintf(msg, sizeof(msg), "voice %d has a noise wave on a tone voice", v);
        if (v != 3)
            TEST_ASSERT_TRUE_MESSAGE(st->sound.wave[v].wave < SOUND_WAVE_WHITE, msg);
    }
}

// The heartbeat is the point of the sound, and it is a TEMPO: two notes
// alternating, with the gap between them set by the live rock count, so a board
// thinning out speeds up on its own. Count the notes a hundred frames make at
// each end of a level.
void test_the_heartbeat_speeds_up_as_the_board_thins(void)
{
    run("init.game  setup.sound  make \"beat.in 1  make \"rocks.alive 12");
    int mark = mock_sound_gate_count();
    run("repeat 100 [heartbeat]");
    int full = notes_on(0, mark);

    run("make \"beat.in 1  make \"rocks.alive 1");
    mark = mock_sound_gate_count();
    run("repeat 100 [heartbeat]");
    int nearly_clear = notes_on(0, mark);

    char msg[128];
    snprintf(msg, sizeof(msg), "%d beats in 100 frames at twelve rocks, %d at one",
             full, nearly_clear);
    TEST_ASSERT_TRUE_MESSAGE(full > 0, msg);
    TEST_ASSERT_TRUE_MESSAGE(nearly_clear > full + 5, msg);

    // Two notes, not one: a single repeated note reads as a metronome rather
    // than a heartbeat.
    run("make \"beat.in 1");
    run("heartbeat");
    uint32_t first = last_freq_on(0);
    run("make \"beat.in 1  heartbeat");
    TEST_ASSERT_TRUE_MESSAGE(last_freq_on(0) != first, "the heartbeat is one note, not two");
}

// The arcade's other pressure, and the one this game did not have at M4: the
// beat quickens the longer a wave lasts, whether the player is clearing it or
// hiding from it. The wave clock is `frame.count`, which `setup.level` zeroes,
// so the count is held still here and only the clock moved -- a board of the
// same size beats faster deeper into the wave.
void test_the_heartbeat_speeds_up_as_the_wave_wears_on(void)
{
    run("init.game  setup.sound  make \"rocks.alive 8");

    run("make \"frame.count 0  make \"beat.in 1");
    int mark = mock_sound_gate_count();
    run("repeat 100 [heartbeat]");
    int early = notes_on(0, mark);

    run("make \"frame.count 700  make \"beat.in 1");
    mark = mock_sound_gate_count();
    run("repeat 100 [heartbeat]");
    int late = notes_on(0, mark);

    char msg[128];
    snprintf(msg, sizeof(msg), "%d beats in 100 frames at the start of the wave, %d 700 frames in",
             early, late);
    TEST_ASSERT_TRUE_MESSAGE(early > 0, msg);
    TEST_ASSERT_TRUE_MESSAGE(late > early, msg);

    // The floor holds, and it is not tidiness: a beat is a 110 ms note with a
    // 45 ms release on it, so beats closer than `beat.min` frames would run
    // together into one tone and lose the tempo.
    run("make \"rocks.alive 0  make \"frame.count 100000  make \"beat.in 1  heartbeat");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":beat.min"), num(":beat.in"),
                                    "the heartbeat ran past its floor");
    TEST_ASSERT_TRUE_MESSAGE(num(":beat.in") > 0, "the heartbeat's period reached zero");

    // A new wave starts the clock again, so the beat is not still at the floor
    // from the last one.
    run("make \"level.rocks 3  setup.level");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":frame.count"),
                                    "a new wave did not restart the heartbeat's clock");
}

// What the rest of the game sounds like, checked where each noise is made
// rather than through a frame -- the frame's job is only to call these.
void test_the_game_makes_its_noises(void)
{
    setup_with(3);
    land_the_ship();
    run("setup.sound  make \"dying 0  make \"lives 3");

    // Firing zaps, and a fourth shot that is not fired makes no noise either.
    run("clear.shots");
    int mark = mock_sound_gate_count();
    run("fire");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, notes_on(1, mark), "firing made no noise");
    run("clear.shots  repeat :max.shots [fire]");
    mark = mock_sound_gate_count();
    run("fire");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(1, mark), "a shot that was not fired made a noise");

    // A rock's death says its size: a large one is a low crump and a small one
    // a sharp tick, so the split table is audible.
    uint32_t pitch[4] = {0};
    for (int s = 3; s >= 1; s--)
    {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "rock.boom %d", s);
        run(cmd);
        pitch[s] = last_freq_on(3);
    }
    TEST_ASSERT_TRUE_MESSAGE(pitch[3] < pitch[2] && pitch[2] < pitch[1],
                             "a rock's death does not rise in pitch as the rock shrinks");

    // A death is one long, low, loud note on the noise pair.
    mark = mock_sound_gate_count();
    run("ship.hit");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, notes_on(3, mark), "the ship died silently");
}

// The rumble sounds on the frames thrust is held and on no others, which falls
// out of where the call sits: inside `thrust`, which `poll.input` reaches only
// on a frame where thrust was the key read.
void test_the_thrust_rumble_sounds_only_while_it_is_held(void)
{
    setup_with(3);
    land_the_ship();
    run("setup.sound  make \"dying 0");

    int mark = mock_sound_gate_count();
    set_mock_input(KEY_THRUST);
    run("play.frame");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, notes_on(2, mark), "a thrusting frame was silent");

    mark = mock_sound_gate_count();
    run("play.frame");                     // no key at all
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(2, mark), "the rumble kept sounding after release");
}

// The saucer warbles while it is on screen and stops when it is not, which is
// the player's warning that one is there. It shares the fire pair, so a frame
// the player fires on wins the voice -- that is the whole cost of the sharing
// and it is one frame long.
void test_the_saucer_warbles_only_while_it_is_up(void)
{
    setup_with(3);
    run("setup.sound  make \"safe 0  make \"ship.rad 0");
    saucer_of_size(2, 0, 120);
    run("make \"sau.w 0  make \"sau.h 0  make \"sau.dx 0  make \"sau.fire.in 999");

    int mark = mock_sound_gate_count();
    run("repeat 30 [step.saucer]");
    int warbles = notes_on(1, mark);
    char msg[96];
    snprintf(msg, sizeof(msg), "%d warbles in 30 frames with a saucer up", warbles);
    TEST_ASSERT_TRUE_MESSAGE(warbles > 5, msg);

    run("leave.saucer");
    mark = mock_sound_gate_count();
    run("repeat 30 [step.saucer]");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(1, mark),
                                  "the warble went on after the saucer left");
}

// The extra ship's alarm is a fixed burst played out by the frame loop, not a
// note made where the ship is awarded -- and it borrows the heartbeat's pair,
// so the interesting assertion is that the beat stays out of its way rather
// than interleaving with it for a second.
void test_an_extra_ship_sounds_an_alarm_the_heartbeat_makes_room_for(void)
{
    run("init.game  setup.sound  make \"rocks.alive 12  make \"beat.in 1");

    int mark = mock_sound_gate_count();
    run("make \"score 9900  add.score 100");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, num(":lives"), "no extra ship to sound an alarm for");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, notes_on(0, mark), "`add.score` made the noise itself");

    // Every note of the burst, and nothing else on the pair while it runs: the
    // beat was due on the very next frame and `add.score` pushed it past the end.
    run("make \"n (:extra.beeps * :extra.gap) - 1  repeat :n [heartbeat  extra.alarm]");
    char msg[96];
    snprintf(msg, sizeof(msg), "%d notes on the alarm's pair, expected %d",
             notes_on(0, mark), (int)num(":extra.beeps"));
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)num(":extra.beeps"), notes_on(0, mark), msg);

    // Two notes and not one, so it reads as an alarm rather than a tone, and
    // both sit above everything else in the game (nothing else is over 1100 Hz).
    run("make \"extra.left 4  make \"extra.in 1  extra.alarm");
    uint32_t first = last_freq_on(0);
    run("extra.alarm  extra.alarm");
    TEST_ASSERT_TRUE_MESSAGE(last_freq_on(0) != first, "the alarm is one note, not two");
    TEST_ASSERT_TRUE_MESSAGE(first > 1100 && last_freq_on(0) > 1100,
                             "the alarm does not sit above the rest of the game");

    // It ends by itself, and the beat comes back with no flag to clear.
    run("make \"extra.left 0  make \"beat.in 1");
    mark = mock_sound_gate_count();
    run("repeat 30 [heartbeat  extra.alarm]");
    TEST_ASSERT_TRUE_MESSAGE(notes_on(0, mark) > 0, "the heartbeat never came back");

    // A level end already silenced the voices, so an alarm still owed notes must
    // not resume into a board it did not belong to.
    run("make \"extra.left 5  make \"level.rocks 1  setup.level");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":extra.left"),
                                    "an alarm survived into the next level");
}

// A level that ends has to silence the voices as well as stop the turtles: the
// PSG keeps sounding on its own, so a thrust rumble or a warble left gated on
// would follow the player back to the attract screen. `stopsound` keeps the
// timbres, which is why `setup.sound` runs once a game and not once a level.
void test_a_level_end_silences_every_voice(void)
{
    run("init.game  setup.sound  make \"level.rocks 3");
    const MockDeviceState *st = mock_device_get_state();
    int before = st->sound.stop_count;

    set_mock_input(" q");
    run("play.level");
    TEST_ASSERT_TRUE_MESSAGE(st->sound.stop_count > before,
                             "a level ended with the voices still sounding");

    // And the timbres survive it, or every level after the first would play in
    // the engine's default voice. Checked on the zap's WAVE and the heartbeat's
    // ENVELOPE, because the heartbeat's own wave is a square and so is the
    // default -- an assertion on that one would pass either way.
    TEST_ASSERT_EQUAL_INT_MESSAGE(SOUND_WAVE_SAWTOOTH, st->sound.wave[1].wave,
                                  "stopsound cleared the timbres");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(25, st->sound.env[0].attack,
                                     "stopsound cleared the envelopes");
}

//==========================================================================
// The hardware harnesses
//==========================================================================

// The harness spells `play.frame` out again minus its `sync`, because the
// present has to be timed on its own and `sync` is the last thing the frame
// does. That duplication is the whole risk in it: a harness frame that drifts
// from the game measures a game nobody plays. Drive both from the same state
// and require the same drawing, the same physics and the same shot bookkeeping.
//
// `p11m1` used to sit alongside this and is gone: fusing the three passes into
// one removed `step.all`, `draw.all` and `place`, which that script called by
// name, so it could no longer run against this game at all. Its numbers live in
// the design's section 12 table. `p11rocks` survives because it defines its own
// drawing and measures a question -- how a frame gets erased -- that nothing
// else reproduces.
void test_the_harness_frame_matches_the_game_frame(void)
{
    load_file(P11M4_SOURCE);

    // The rocks are kept clear of the origin, where an unfired shot turtle
    // sits: a hit here would split a rock with `random` velocities and the two
    // runs would diverge on a number neither is testing.
    const char *state =
        "init.game  make \"paused false  clear.rocks  clear.shots  reset.ship "
        "make \"waiting false  make \"ship.rad :ship.rad.hull "
        "make \"hud.text [SCORE 0 ROCKS 2]  make \"frame.count 0 "
        "make \"shipx 5  make \"shipy -5  make \"svx 1  make \"svy 2  make \"sh 40 "
        ".setitem 1 :rsize 3  .setitem 1 :rrad 27  .setitem 1 :rx 60  .setitem 1 :ry 20 "
        ".setitem 1 :rdx 1.5  .setitem 1 :rdy -0.5  .setitem 1 :rang 30  .setitem 1 :rspin 2 "
        ".setitem 2 :rsize 1  .setitem 2 :rrad 13  .setitem 2 :rx -140  .setitem 2 :ry 155 "
        ".setitem 2 :rdx -0.5  .setitem 2 :rdy 1.5  .setitem 2 :rang 200  .setitem 2 :rspin -1 "
        ".setitem 1 :slife 5  make \"s1x 120  make \"s1y -120";

    run(state);
    mock_device_clear_graphics();
    run("play.frame");
    int game_segments = mock_device_line_count();
    float game_x = item_of("rx", 1), game_a = item_of("rang", 2);
    float game_shipx = num(":shipx");
    float game_life = item_of("slife", 1);
    TEST_ASSERT_EQUAL_INT(SEG_LARGE + SEG_SMALL + SEG_SHIP, game_segments);

    run(state);
    mock_device_clear_graphics();
    run("frame.body");
    TEST_ASSERT_EQUAL_INT_MESSAGE(game_segments, mock_device_line_count(),
                                  "the harness frame does not draw what the game frame draws");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(game_x, item_of("rx", 1),
                                    "the harness frame does not step what the game frame steps");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(game_a, item_of("rang", 2),
                                    "the harness frame does not spin what the game frame spins");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(game_shipx, num(":shipx"),
                                    "the harness frame does not fly the ship the game flies");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(game_life, item_of("slife", 1),
                                    "the harness frame does not age the shots the game ages");

    // And a death frame, because that is the branch M3 added to the frame: a
    // dying ship counts its explosion down where a live one steps. A harness
    // that kept calling `step.ship` would time a ship the game is not flying.
    const char *dying = "make \"dying 5  make \"shipx 20  make \"shipy 20  make \"svx 4";
    run(state);
    run(dying);
    mock_device_clear_graphics();
    run("play.frame");
    int game_death_segments = mock_device_line_count();
    float game_dying = num(":dying"), game_death_x = num(":shipx");

    run(state);
    run(dying);
    mock_device_clear_graphics();
    run("frame.body");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(game_dying, num(":dying"),
                                    "the harness frame does not count the explosion down");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(game_death_x, num(":shipx"),
                                    "the harness frame flew a ship that is exploding");
    TEST_ASSERT_EQUAL_INT_MESSAGE(game_death_segments, mock_device_line_count(),
                                  "the harness frame does not draw the explosion the game draws");

    // And a saucer frame, because that is what M4 added: a harness that stepped
    // the saucer but did not draw it -- or drew it unconditionally -- would time
    // a frame the game never runs, and the saucer point is the whole reason this
    // run exists.
    const char *flying =
        "make \"sau.on 2  make \"sau.x -40  make \"sau.y 30  make \"sau.dx 1.8 "
        "make \"sau.dy 0  make \"sau.w 0  make \"sau.h 0 "
        "make \"sau.turn.in 5  make \"sau.fire.in 5  make \"warble.in 1";
    run(state);
    run(flying);
    mock_device_clear_graphics();
    run("play.frame");
    int game_saucer_segments = mock_device_line_count();
    float game_saucer_x = num(":sau.x");
    TEST_ASSERT_EQUAL_INT(SEG_LARGE + SEG_SMALL + SEG_SHIP + SEG_SAUCER, game_saucer_segments);

    run(state);
    run(flying);
    mock_device_clear_graphics();
    run("frame.body");
    TEST_ASSERT_EQUAL_INT_MESSAGE(game_saucer_segments, mock_device_line_count(),
                                  "the harness frame does not draw the saucer the game draws");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(game_saucer_x, num(":sau.x"),
                                    "the harness frame does not fly the saucer the game flies");
}

void test_p11m4_script_runs(void)
{
    load_file(P11M4_SOURCE);
    run("make \"p11m4.frames 3");
    mock_device_clear_output();
    run("p11m4");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the rock pass"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "one shot.on"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "one thrust"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "nodes at start"), screen);
    // M4's whole question is what a saucer costs while it is up, so the run is
    // worthless if the report does not carry the fourth point and the delta.
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "WITH a saucer"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the saucer costs"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "one saucer outline"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "one sound"), screen);

    MockFile *report = mock_fs_get_file("p11m4.txt", false);
    TEST_ASSERT_NOT_NULL_MESSAGE(report, "p11m4.txt was not written");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->data, "budget at 14 fps"), report->data);
}

void test_the_m4_harness_measures_the_rock_counts_it_reports(void)
{
    load_file(P11M4_SOURCE);
    run("make \"p11m4.frames 2");
    run("p11m4");

    // Four points now: three without a saucer and a fourth with one, at the
    // same twelve rocks, so the two are subtractable.
    const float wanted[] = {6, 9, 12, 12};
    for (int k = 0; k < 4; k++)
    {
        char expr[64], msg[112];
        snprintf(expr, sizeof(expr), "0 + item %d :p11m4.rocks", k + 1);
        snprintf(msg, sizeof(msg), "point %d timed %d rocks, not %d",
                 k + 1, (int)num(expr), (int)wanted[k]);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(wanted[k], num(expr), msg);
    }
}

// The harness measures the WORST case, which needs two things held that a
// played game does not hold by itself: three shots live on every frame, and a
// rock count that does not drain. It gets both without redefining anything in
// the game -- `arm.shots` tops the life up, and zeroing every `rrad` means no
// shot can connect, so nothing splits and nothing is consumed.
void test_the_m4_harness_holds_three_shots_live_and_the_board_still(void)
{
    load_file(P11M4_SOURCE);
    run("init.game  make \"level.rocks 6  setup.level");
    run("repeat :max.rocks [.setitem repcount :rrad 0]");
    run("launch.shots");

    for (int i = 1; i <= 3; i++)
        TEST_ASSERT_TRUE_MESSAGE(item_of("slife", i) > 0, "launch.shots left a shot idle");

    // Well past `shot.life`, which is 18 frames.
    run("repeat 40 [arm.shots  frame.body]");
    for (int i = 1; i <= 3; i++)
        TEST_ASSERT_TRUE_MESSAGE(item_of("slife", i) > 0, "a shot expired during the run");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(6, num(":rocks.alive"),
                                    "the board drained during the run");
}

// The three plain points have to be plain, and nothing about them says so on
// the screen: `setup.level` leaves the appearance countdown at `sau.first`,
// which is 140 frames against a 300-frame point, so a saucer would walk into
// the back half of every one of them and average itself into a figure that is
// supposed to be without one. `measure` parks the wait; this is what stops that
// being quietly removed.
void test_the_m4_harness_keeps_the_saucer_off_the_plain_points(void)
{
    load_file(P11M4_SOURCE);
    run("make \"p11m4.frames 3");

    run("measure 1 6 false");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sau.on"),
                                    "a saucer appeared during a no-saucer point");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.wait") > 1000,
                             "the appearance countdown was not parked");

    // And the fourth point holds one up for every frame of it, which is what
    // makes the two subtractable. A real saucer crosses the field in 178
    // frames, so without `hold.saucer` a long point would time an empty screen
    // for its tail.
    run("make \"p11m4.frames 200");
    run("measure 4 12 true");
    TEST_ASSERT_TRUE_MESSAGE(num(":sau.on") > 0,
                             "the saucer left during the saucer point");

    // Its boxes are zeroed the way the rocks' are, so the run is a lower bound
    // and can never turn into a death that times a different frame.
    TEST_ASSERT_EQUAL_FLOAT(0, num(":sau.w"));
    TEST_ASSERT_EQUAL_FLOAT(0, num(":sau.h"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":lives"), "the harness lost a life");
}

//==========================================================================



int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_file_loads_and_sets_its_tuning);
    RUN_TEST(test_the_per_frame_constants_are_cut_from_the_frame_rate);
    RUN_TEST(test_a_shot_cannot_outrun_the_smallest_collision_box);
    RUN_TEST(test_the_collision_boxes_are_not_far_wider_than_the_rocks_drawn_in_them);
    RUN_TEST(test_the_ship_box_is_not_wider_than_the_shapes_it_is_drawn_from);
    RUN_TEST(test_wrapc_wraps_at_both_edges);
    RUN_TEST(test_wrapc_corrects_once_and_only_once);
    RUN_TEST(test_every_outline_closes_on_itself);
    RUN_TEST(test_both_ship_outlines_close_on_themselves);
    RUN_TEST(test_the_ship_is_smaller_than_a_large_rock);
    RUN_TEST(test_the_walk_out_to_the_first_vertex_does_not_draw);
    RUN_TEST(test_draw_rock_picks_the_outline_for_the_size);
    RUN_TEST(test_a_rock_angle_past_360_still_places);
    RUN_TEST(test_free_slot_finds_the_first_zero);
    RUN_TEST(test_a_full_board_has_no_free_slot);
    RUN_TEST(test_spawn_fills_a_slot_inside_the_field);
    RUN_TEST(test_spawning_onto_a_full_board_creates_nothing);
    RUN_TEST(test_a_rock_leaving_the_field_comes_back_on_the_far_side);
    RUN_TEST(test_the_rock_pass_moves_only_live_rocks);
    RUN_TEST(test_setup_level_puts_the_asked_for_rocks_on_the_board);
    RUN_TEST(test_setup_level_never_exceeds_the_slot_count);
    RUN_TEST(test_a_frame_draws_the_world_and_nothing_else);
    RUN_TEST(test_every_rock_is_drawn_with_a_one_pixel_pen);
    RUN_TEST(test_the_reclaim_interval_stays_inside_the_busy_frame_budget);
    RUN_TEST(test_the_frame_loop_survives_a_squeezed_workspace);
    RUN_TEST(test_the_frame_loop_holds_free_storage_flat);
    RUN_TEST(test_a_paused_frame_neither_steps_nor_recycles);
    RUN_TEST(test_the_ship_keeps_its_momentum);
    RUN_TEST(test_the_ship_wraps_like_a_rock);
    RUN_TEST(test_thrust_pushes_along_the_heading);
    RUN_TEST(test_the_speed_clamp_holds_at_the_boundary);
    RUN_TEST(test_a_ship_heading_outside_zero_to_360_still_thrusts_and_draws);
    RUN_TEST(test_the_flame_shows_only_when_thrusting_and_only_every_other_frame);
    RUN_TEST(test_the_arrows_turn_the_ship_both_ways);
    RUN_TEST(test_a_frame_with_no_key_puts_the_flame_out);
    RUN_TEST(test_firing_takes_the_lowest_idle_shot);
    RUN_TEST(test_a_shot_expires_and_stops_its_turtle);
    RUN_TEST(test_a_shot_flies_on_its_own_and_its_position_is_read_back);
    RUN_TEST(test_shot_on_finds_the_shot_inside_the_square);
    RUN_TEST(test_a_parked_shot_hits_nothing_anywhere_on_the_field);
    RUN_TEST(test_the_split_table);
    RUN_TEST(test_a_split_fills_the_slots_it_finds_and_no_more);
    RUN_TEST(test_three_larges_split_all_the_way_down_into_exactly_twelve_slots);
    RUN_TEST(test_the_score_table);
    RUN_TEST(test_a_shot_on_a_rock_splits_it_scores_it_and_is_consumed);
    RUN_TEST(test_one_shot_kills_one_rock_per_frame);
    RUN_TEST(test_an_idle_shot_hits_nothing);
    RUN_TEST(test_a_rock_on_the_ship_kills_it);
    RUN_TEST(test_one_frame_takes_only_one_life);
    RUN_TEST(test_a_waiting_ship_does_not_appear_until_the_space_is_clear);
    RUN_TEST(test_a_respawn_wait_gives_up_and_lands_the_ship);
    RUN_TEST(test_a_waiting_ship_answers_no_key_but_pause_and_quit);
    RUN_TEST(test_the_clear_radius_is_wider_than_the_ship_it_protects);
    RUN_TEST(test_the_explosion_counts_down_and_the_ship_comes_back);
    RUN_TEST(test_the_last_life_ends_the_level);
    RUN_TEST(test_a_dying_ship_draws_fragments_and_not_a_ship);
    RUN_TEST(test_the_fragments_are_the_ship_segments);
    RUN_TEST(test_hyperspace_moves_the_ship_and_stops_it);
    RUN_TEST(test_hyperspace_sometimes_ends_badly);
    RUN_TEST(test_an_extra_ship_every_ten_thousand_points);
    RUN_TEST(test_scoring_repaints_the_hud_wherever_the_points_come_from);
    RUN_TEST(test_a_level_advance_adds_a_rock_up_to_the_ceiling);
    RUN_TEST(test_the_hud_carries_the_score_the_level_and_a_heart_a_life);
    RUN_TEST(test_pause_answers_p_and_nothing_else);
    RUN_TEST(test_a_dying_ship_answers_only_pause_and_quit);
    RUN_TEST(test_q_quits_the_game_and_not_just_the_level);
    RUN_TEST(test_a_game_plays_levels_until_the_ships_run_out);
    RUN_TEST(test_quitting_shows_no_game_over_card);
    RUN_TEST(test_the_attract_screen_prints_the_scores_and_the_keys);
    RUN_TEST(test_game_over_prints_the_final_score);
    RUN_TEST(test_a_level_ends_on_q_and_puts_the_screen_back);
    RUN_TEST(test_a_level_ends_when_the_board_is_clear);
    RUN_TEST(test_a_saucer_holds_the_level_open_until_it_is_gone);
    RUN_TEST(test_a_cleared_board_spawns_no_further_saucer);
    RUN_TEST(test_both_saucer_outlines_close_on_themselves);
    RUN_TEST(test_draw_saucer_picks_the_outline_for_the_size);
    RUN_TEST(test_the_saucer_boxes_are_not_far_wider_than_the_shapes_drawn_in_them);
    RUN_TEST(test_a_shot_cannot_outrun_the_saucer);
    RUN_TEST(test_a_saucer_shot_cannot_outrun_the_ship);
    RUN_TEST(test_a_saucer_appears_on_a_countdown_and_enters_from_an_edge);
    RUN_TEST(test_a_saucer_crosses_and_leaves_at_the_far_edge);
    RUN_TEST(test_the_gap_between_saucers_shortens_with_every_saucer);
    RUN_TEST(test_the_saucer_size_follows_the_gap_and_the_score);
    RUN_TEST(test_the_small_saucer_aim_tightens_with_the_score);
    RUN_TEST(test_a_shot_kills_a_saucer_scores_it_and_is_consumed);
    RUN_TEST(test_a_saucer_on_the_ship_kills_the_ship);
    RUN_TEST(test_a_saucer_shot_kills_the_ship);
    RUN_TEST(test_a_saucer_shot_fired_across_the_field_kills_the_ship);
    RUN_TEST(test_a_saucer_shot_through_the_ships_nose_kills_it);
    RUN_TEST(test_a_saucer_shot_clear_of_the_ship_misses);
    RUN_TEST(test_a_saucer_shot_expires_and_stops_its_turtle);
    RUN_TEST(test_a_saucer_shot_outlives_the_saucer_that_fired_it);
    RUN_TEST(test_the_small_saucer_aims_at_the_ship_and_the_large_one_does_not);
    RUN_TEST(test_a_saucer_does_not_range_on_a_ship_that_is_not_there);
    RUN_TEST(test_a_saucer_over_the_spawn_point_blocks_the_respawn_without_dying);
    RUN_TEST(test_a_frame_with_a_saucer_up_draws_it);
    RUN_TEST(test_a_level_starts_with_no_saucer_and_nothing_in_the_air);
    RUN_TEST(test_a_pause_holds_the_shots_where_they_are);
    RUN_TEST(test_a_saucer_shot_breaks_a_rock_and_pays_nobody);
    RUN_TEST(test_a_saucer_that_flies_into_a_rock_dies_with_it);
    RUN_TEST(test_a_saucer_that_is_not_there_hits_nothing);
    RUN_TEST(test_the_timbres_are_set_once_and_match_the_voice_kinds);
    RUN_TEST(test_the_heartbeat_speeds_up_as_the_board_thins);
    RUN_TEST(test_the_heartbeat_speeds_up_as_the_wave_wears_on);
    RUN_TEST(test_the_game_makes_its_noises);
    RUN_TEST(test_the_thrust_rumble_sounds_only_while_it_is_held);
    RUN_TEST(test_the_saucer_warbles_only_while_it_is_up);
    RUN_TEST(test_an_extra_ship_sounds_an_alarm_the_heartbeat_makes_room_for);
    RUN_TEST(test_a_level_end_silences_every_voice);
    RUN_TEST(test_the_harness_frame_matches_the_game_frame);
    RUN_TEST(test_p11m4_script_runs);
    RUN_TEST(test_the_m4_harness_measures_the_rock_counts_it_reports);
    RUN_TEST(test_the_m4_harness_holds_three_shots_live_and_the_board_still);
    RUN_TEST(test_the_m4_harness_keeps_the_saucer_off_the_plain_points);
    return UNITY_END();
}
