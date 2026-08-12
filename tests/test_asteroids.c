//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the Asteroids game (logo/games/asteroids), M1: rocks only.
//
//  The game is pure Logo; this exercises it the two ways test_galaxian.c does:
//  loading the whole file proves it parses and that the init path runs on the
//  mock device, and the pure logic (wrap, slot allocation, the outline walks)
//  is checked directly, since that is where the bugs would hide.
//
//  M1's own question is the frame budget, and no host test can answer it --
//  that needs logo/tests/p11m1 on a board.  What these tests can hold is
//  everything the budget assumes: that a frame draws the world and nothing
//  else, that the frame loop holds free storage flat, and that the outlines
//  carry the segment counts the budget was cut from.
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

#ifndef P11M1_SOURCE
#error "P11M1_SOURCE must be defined (path to logo/tests/p11m1)"
#endif

// Segments per outline, from the design's section 6.3 table. Statements per
// draw are 15/13/11: four for the prologue, then two per segment less the
// turn after the last one.
#define SEG_LARGE  6
#define SEG_MEDIUM 5
#define SEG_SMALL  4

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
// costs, which is why there is one per size to reach.
void test_draw_rock_picks_the_outline_for_the_size(void)
{
    const int sizes[] = {3, 2, 1};
    const int segs[] = {SEG_LARGE, SEG_MEDIUM, SEG_SMALL};
    for (int k = 0; k < 3; k++)
    {
        char cmd[96];
        snprintf(cmd, sizeof(cmd), ".setitem 1 :rsize %d", sizes[k]);
        run(cmd);
        run("clean  setpc 254  pu setx 0 sety 0 seth 0");
        mock_device_clear_graphics();
        run("draw.rock 1");
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
    run(".setitem 1 :rx 0  .setitem 1 :ry 0  .setitem 1 :rang 3600  .setitem 1 :rsize 3");
    run("place 1");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_verify_heading(0.0f, 0.5f),
                             "seth did not normalise a heading past 360");
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
    run("spawn.rock 3 22");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":rocks.alive"));
    TEST_ASSERT_EQUAL_FLOAT(3, item_of("rsize", 1));
    TEST_ASSERT_EQUAL_FLOAT(22, item_of("rrad", 1));

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
    run("repeat :max.rocks [spawn.rock 3 22]");
    TEST_ASSERT_EQUAL_FLOAT(12, num(":rocks.alive"));
    run("spawn.rock 3 22");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(12, num(":rocks.alive"),
                                    "a rock was created with no slot to hold it");
}

//==========================================================================
// Motion
//==========================================================================

void test_a_rock_leaving_the_field_comes_back_on_the_far_side(void)
{
    run("clear.rocks");
    run(".setitem 1 :rsize 3  .setitem 1 :rx 159.5  .setitem 1 :ry 0");
    run(".setitem 1 :rdx 2  .setitem 1 :rdy 0  .setitem 1 :rspin 0  .setitem 1 :rang 0");
    run("step.rock 1");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -158.5f, item_of("rx", 1));
}

void test_step_all_moves_only_live_rocks(void)
{
    run("clear.rocks");
    run(".setitem 2 :rsize 3  .setitem 2 :rx 0  .setitem 2 :rdx 1");
    run(".setitem 3 :rx 0  .setitem 3 :rdx 1");   // slot 3 stays free
    run("step.all");
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
    int expected = expected_segments();
    TEST_ASSERT_EQUAL_INT(6 * SEG_LARGE, expected);

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
    set_mock_input("q");
    run("play.level");
    TEST_ASSERT_EQUAL_STRING("auto", value_to_string(eval_string("refreshmode").value));
}

//==========================================================================
// The M1 hardware harness
//==========================================================================

// It must run end to end before it is worth carrying to a board: a script
// that dies half way through wastes a hardware session, and its numbers only
// leave the board through the file.
void test_p11m1_script_runs(void)
{
    load_file(P11M1_SOURCE);
    run("make \"p11m1.frames 3");
    mock_device_clear_output();
    run("p11m1");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "body"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "present"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "nodes at start"), screen);

    MockFile *report = mock_fs_get_file("p11m1.txt", false);
    TEST_ASSERT_NOT_NULL_MESSAGE(report, "p11m1.txt was not written");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->data, "budget at 15 fps"), report->data);
}

// The harness has to measure the board it says it measured, and the first
// hardware run did not: `measure` set `level.rocks` and then called
// `init.game`, which resets it to `start.rocks`, so all three points ran on a
// four-rock board and came back within 0.03 ms of each other. Three identical
// numbers is what gave it away, which is a poor substitute for a test.
void test_the_harness_measures_the_rock_counts_it_reports(void)
{
    load_file(P11M1_SOURCE);
    run("make \"p11m1.frames 2");
    run("p11m1");

    const float wanted[] = {6, 9, 12};
    for (int k = 0; k < 3; k++)
    {
        char expr[64], msg[112];
        snprintf(expr, sizeof(expr), "0 + item %d :p11m1.rocks", k + 1);
        snprintf(msg, sizeof(msg), "point %d timed %d rocks, not %d",
                 k + 1, (int)num(expr), (int)wanted[k]);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(wanted[k], num(expr), msg);
    }
}

// The harness spells `play.frame` out again minus its `sync`, because the
// present has to be timed on its own and `sync` is the last thing the frame
// does. That duplication is the whole risk in it: a harness frame that drifts
// from the game measures a game nobody plays. Drive both from the same state
// and require the same drawing and the same physics.
void test_the_harness_frame_matches_the_game_frame(void)
{
    load_file(P11M1_SOURCE);

    const char *state =
        "make \"paused false  clear.rocks  make \"hud.text [ROCKS 2]  make \"frame.count 0 "
        ".setitem 1 :rsize 3  .setitem 1 :rx 10  .setitem 1 :ry 20 "
        ".setitem 1 :rdx 1.5  .setitem 1 :rdy -0.5  .setitem 1 :rang 30  .setitem 1 :rspin 2 "
        ".setitem 2 :rsize 1  .setitem 2 :rx -140  .setitem 2 :ry 155 "
        ".setitem 2 :rdx -0.5  .setitem 2 :rdy 1.5  .setitem 2 :rang 200  .setitem 2 :rspin -1";

    run(state);
    mock_device_clear_graphics();
    run("play.frame");
    int game_segments = mock_device_line_count();
    float game_x = item_of("rx", 1), game_a = item_of("rang", 2);
    TEST_ASSERT_EQUAL_INT(SEG_LARGE + SEG_SMALL, game_segments);

    run(state);
    mock_device_clear_graphics();
    run("frame.body");
    TEST_ASSERT_EQUAL_INT_MESSAGE(game_segments, mock_device_line_count(),
                                  "the harness frame does not draw what the game frame draws");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(game_x, item_of("rx", 1),
                                    "the harness frame does not step what the game frame steps");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(game_a, item_of("rang", 2),
                                    "the harness frame does not spin what the game frame spins");
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_file_loads_and_sets_its_tuning);
    RUN_TEST(test_wrapc_wraps_at_both_edges);
    RUN_TEST(test_wrapc_corrects_once_and_only_once);
    RUN_TEST(test_every_outline_closes_on_itself);
    RUN_TEST(test_the_walk_out_to_the_first_vertex_does_not_draw);
    RUN_TEST(test_draw_rock_picks_the_outline_for_the_size);
    RUN_TEST(test_a_rock_angle_past_360_still_places);
    RUN_TEST(test_free_slot_finds_the_first_zero);
    RUN_TEST(test_a_full_board_has_no_free_slot);
    RUN_TEST(test_spawn_fills_a_slot_inside_the_field);
    RUN_TEST(test_spawning_onto_a_full_board_creates_nothing);
    RUN_TEST(test_a_rock_leaving_the_field_comes_back_on_the_far_side);
    RUN_TEST(test_step_all_moves_only_live_rocks);
    RUN_TEST(test_setup_level_puts_the_asked_for_rocks_on_the_board);
    RUN_TEST(test_setup_level_never_exceeds_the_slot_count);
    RUN_TEST(test_a_frame_draws_the_world_and_nothing_else);
    RUN_TEST(test_every_rock_is_drawn_with_a_one_pixel_pen);
    RUN_TEST(test_the_reclaim_interval_stays_inside_the_atom_budget);
    RUN_TEST(test_the_frame_loop_holds_free_storage_flat);
    RUN_TEST(test_a_paused_frame_neither_steps_nor_recycles);
    RUN_TEST(test_pause_answers_p_and_nothing_else);
    RUN_TEST(test_a_level_ends_on_q_and_puts_the_screen_back);
    RUN_TEST(test_the_harness_frame_matches_the_game_frame);
    RUN_TEST(test_p11m1_script_runs);
    RUN_TEST(test_the_harness_measures_the_rock_counts_it_reports);
    return UNITY_END();
}
