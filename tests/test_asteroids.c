//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the Asteroids game (logo/games/asteroids), M2: the ship, the
//  shots, splitting and scoring.
//
//  The game is pure Logo; this exercises it the two ways test_galaxian.c does:
//  loading the whole file proves it parses and that the init path runs on the
//  mock device, and the pure logic (wrap, slot allocation, the split table,
//  the outline walks) is checked directly, since that is where the bugs would
//  hide.
//
//  The frame budget is what M2 is really about, and no host test can answer it
//  -- that needs logo/tests/p11m3 on a board.  What these tests can hold is
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

#ifndef P11M3_SOURCE
#error "P11M3_SOURCE must be defined (path to logo/tests/p11m3)"
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

// PicoCalc key codes, as the two shipped shooters use them.
#define KEY_LEFT   "\264"
#define KEY_RIGHT  "\267"
#define KEY_THRUST "\265"

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
    run("make \"safe 0  make \"ship.rad 0");
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
// Measure the deadline rather than assume it: disable `reclaim` and run until
// the loop dies. It survives ~649 frames at twelve rocks on the host. The
// original interval of 250 was copied from Galaxian, whose frame spends
// nothing, and left a 2.6x margin -- which held here and did not hold on a
// board, where a fuller workspace puts the node region's floor lower and
// squeezes the shared atom ceiling with it.
void test_the_reclaim_interval_stays_inside_the_atom_budget(void)
{
    setup_with(12);
    run("recycle");
    proc_define_from_text("to reclaim\nend");   // nothing collects now

    int deadline = 0;
    for (; deadline < 4000; deadline++)
    {
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
             "reclaim every %d frames against a %d-frame budget -- less than 8x margin",
             interval, deadline);
    TEST_ASSERT_TRUE_MESSAGE(interval * 8 < deadline, msg);
}

void test_the_frame_loop_holds_free_storage_flat(void)
{
    setup_with(12);
    run("repeat 250 [play.frame]");
    run("recycle");
    int settled = (int)num("nodes");

    run("repeat 1000 [play.frame]");
    run("recycle");
    int later = (int)num("nodes");

    char msg[128];
    snprintf(msg, sizeof(msg),
             "free storage fell %d cells over 1000 frames -- the frame loop is growing",
             settled - later);
    TEST_ASSERT_TRUE_MESSAGE(settled - later < 400, msg);
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
    run("make \"thrusting false  make \"frame.count 1  make \"sh 725  make \"safe 0");
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
    run("reset.ship  make \"thrusting false  make \"frame.count 0  make \"safe 0");
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
    run("split.rock 1");

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
    run(".setitem 1 :rsize 2  make \"rocks.alive 1  split.rock 1");
    TEST_ASSERT_EQUAL_FLOAT(2, num(":rocks.alive"));
    TEST_ASSERT_EQUAL_FLOAT(1, item_of("rsize", 1));

    run("clear.rocks");
    run(".setitem 1 :rsize 1  make \"rocks.alive 1  split.rock 1");
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
    run("split.rock 1");   // frees exactly one slot, so exactly one child fits

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
            snprintf(cmd, sizeof(cmd), "split.rock %d", i);
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
    run("make \"safe 0  step.ship");
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
    run("make \"rocks.alive 12  make \"safe 0  step.ship");
    run("step.draw.all");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"),
                                    "one frame took more than one life");
}

// The respawn grace is invulnerability, and it is spelled as a parked ship
// rather than as a guard in the rock pass -- so what proves it is that the
// collision position never leaves 9999 while it lasts.
void test_a_ship_in_its_respawn_grace_cannot_be_hit(void)
{
    ship_under_a_rock();
    run("respawn");
    TEST_ASSERT_TRUE(num(":safe") > 0);

    // Every frame of the grace, with the rock sitting on the ship's centre.
    int grace = (int)num(":safe.frames");
    for (int i = 0; i < grace; i++)
    {
        run("step.ship  step.draw.all");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3, num(":lives"),
                                        "a rock hit a ship inside its respawn grace");
    }

    // And it ends: one more frame makes the ship hittable again, and the rock
    // it has been sitting under kills it.
    run("step.ship  step.draw.all");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2, num(":lives"),
                                    "the respawn grace never ended");
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
    // centre of the field, stopped, facing north, and in its grace.
    run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":dying"), "the explosion did not end");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":shipx"), "the ship did not come back to the centre");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":svx"), "the ship came back still moving");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":sh"), "the ship came back on its old heading");
    TEST_ASSERT_EQUAL_FLOAT(num(":safe.frames"), num(":safe"));
    TEST_ASSERT_TRUE_MESSAGE(item_of("rx", 1) > 0, "the rocks stood still through the death");
}

// The last life ends the level rather than respawning into an unplayable game,
// and it ends it from inside the frame -- `play.level` reads `over` and stops.
void test_the_last_life_ends_the_level(void)
{
    run("init.game  clear.rocks  clear.shots");
    run("make \"lives 1  make \"over false  ship.hit");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":lives"));

    run("repeat :death.frames [play.frame]");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(":over").value),
                                     "the last life did not end the level");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9999, num(":shipcx"),
                                    "a game that is over put the ship back");
}

// A dying ship is a ring and not a ship. `arc` sweeps four degrees a segment,
// so a full circle is 90 strokes inside one primitive call -- the reason an
// explosion is affordable at all where a fragment system was not.
void test_a_dying_ship_draws_a_ring_and_not_a_ship(void)
{
    run("init.game  clear.rocks  clear.shots  make \"safe 0");
    run("make \"dying 1");
    mock_device_clear_graphics();
    run("draw.ship");
    TEST_ASSERT_EQUAL_INT_MESSAGE(90, mock_device_line_count(),
                                  "a dying ship did not draw one full ring");

    // And the ring grows as the countdown falls: the first death frame is the
    // smallest, the last is the widest. The sweep starts at the turtle's
    // heading, which `draw.boom` sets to north, so the first stroke begins one
    // radius above the ship and its y is the radius.
    run("make \"dying :death.frames - 1");
    mock_device_clear_graphics();
    run("draw.ship");
    float first = mock_device_get_line(0)->y1;
    TEST_ASSERT_EQUAL_FLOAT(num(":boom.grow"), first);

    run("make \"dying 1");
    mock_device_clear_graphics();
    run("draw.ship");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":boom.grow * (:death.frames - 1)"),
                                    mock_device_get_line(0)->y1,
                                    "the explosion ring did not expand with the countdown");
}

// The blink is what tells a player which ship is theirs and that it is still
// safe, and it costs one `remainder` on the frames it hides.
void test_a_ship_in_its_respawn_grace_blinks(void)
{
    run("init.game  clear.rocks  clear.shots  respawn");
    int shown = 0, hidden = 0;
    for (int i = 0; i < 8; i++)
    {
        mock_device_clear_graphics();
        run("draw.ship");
        if (mock_device_line_count() == 0)
            hidden++;
        else
            shown++;
        run("step.ship");
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, shown, "the ship did not blink through its grace");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, hidden, "the ship did not blink through its grace");
}

// Hyperspace is the panic button and it is meant to cost something: no
// velocity on the far side, and one jump in `hyper.risk` arrives inside a rock.
void test_hyperspace_moves_the_ship_and_stops_it(void)
{
    run("init.game  clear.rocks  clear.shots  make \"safe 0");
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
    TEST_ASSERT_EQUAL_STRING_MESSAGE("[SCORE 50 LEVEL 1 \x10\x10\x10]",
                                     value_to_string(eval_string(":hud.text").value),
                                     "add.score left the HUD stale");

    // An extra ship moves `lives`, which is on the same line.
    run("make \"hud.text [stale]  add.score 9950");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("[SCORE 10000 LEVEL 1 \x10\x10\x10\x10]",
                                     value_to_string(eval_string(":hud.text").value),
                                     "an extra ship did not reach the HUD");

    // And a kill still repaints, through `score.rock` -- exactly once, not
    // once here and once in `split.rock`.
    run("clear.rocks  clear.shots");
    run(".setitem 1 :rsize 1  .setitem 1 :rrad rad.for 1  make \"rocks.alive 1");
    run("make \"hud.text [stale]");
    run("split.rock 1");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("[SCORE 10100 LEVEL 1 \x10\x10\x10\x10]",
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
    TEST_ASSERT_EQUAL_STRING("[SCORE 240 LEVEL 2 \x10\x10\x10]",
                             value_to_string(eval_string(":hud.text").value));

    run("make \"lives 1  refresh.hud");
    TEST_ASSERT_EQUAL_STRING("[SCORE 240 LEVEL 2 \x10]",
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
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Large rock     20"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Small rock    100"), screen);
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
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "LEVEL REACHED: 4"), screen);
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
    load_file(P11M3_SOURCE);

    // The rocks are kept clear of the origin, where an unfired shot turtle
    // sits: a hit here would split a rock with `random` velocities and the two
    // runs would diverge on a number neither is testing.
    const char *state =
        "make \"paused false  clear.rocks  clear.shots  reset.ship "
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
}

void test_p11m3_script_runs(void)
{
    load_file(P11M3_SOURCE);
    run("make \"p11m3.frames 3");
    mock_device_clear_output();
    run("p11m3");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the rock pass"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "one shot.on"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "one thrust"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "nodes at start"), screen);

    MockFile *report = mock_fs_get_file("p11m3.txt", false);
    TEST_ASSERT_NOT_NULL_MESSAGE(report, "p11m3.txt was not written");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->data, "budget at 14 fps"), report->data);
}

void test_the_m3_harness_measures_the_rock_counts_it_reports(void)
{
    load_file(P11M3_SOURCE);
    run("make \"p11m3.frames 2");
    run("p11m3");

    const float wanted[] = {6, 9, 12};
    for (int k = 0; k < 3; k++)
    {
        char expr[64], msg[112];
        snprintf(expr, sizeof(expr), "0 + item %d :p11m3.rocks", k + 1);
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
void test_the_m3_harness_holds_three_shots_live_and_the_board_still(void)
{
    load_file(P11M3_SOURCE);
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
    RUN_TEST(test_the_reclaim_interval_stays_inside_the_atom_budget);
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
    RUN_TEST(test_a_ship_in_its_respawn_grace_cannot_be_hit);
    RUN_TEST(test_the_explosion_counts_down_and_the_ship_comes_back);
    RUN_TEST(test_the_last_life_ends_the_level);
    RUN_TEST(test_a_dying_ship_draws_a_ring_and_not_a_ship);
    RUN_TEST(test_a_ship_in_its_respawn_grace_blinks);
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
    RUN_TEST(test_the_harness_frame_matches_the_game_frame);
    RUN_TEST(test_p11m3_script_runs);
    RUN_TEST(test_the_m3_harness_measures_the_rock_counts_it_reports);
    RUN_TEST(test_the_m3_harness_holds_three_shots_live_and_the_board_still);
    return UNITY_END();
}
