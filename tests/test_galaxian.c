//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the Galaxian game (logo/games/galaxian).
//
//  The game itself is pure Logo; this exercises it two ways:
//   - loading the whole file proves it parses and every procedure the init
//     path touches is defined and runs on the mock device (setup.level does
//     cs/splitscreen/stamping/when-demon registration);
//   - the pure logic helpers (the rank pyramid, the cell<->position formulas,
//     the scoring table, the clamped-turn steering, the hit-test inverse) are
//     checked directly, since those are where the bugs would hide.
//

#include "test_scaffold.h"
#include "mock_device.h"
#include "core/repl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GALAXIAN_SOURCE
#error "GALAXIAN_SOURCE must be defined (path to logo/games/galaxian)"
#endif

// Load the whole game file, defining its procedures and running its top-level
// colour/tuning `make`s. Procedure definitions (`to ... end`) are not handled
// by the bare evaluator, so we buffer them and hand them to
// proc_define_from_text the same way the `load` primitive does; other lines go
// straight to run_string. Fails the test on any error.
static void load_galaxian(void)
{
    FILE *f = fopen(GALAXIAN_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, "cannot open " GALAXIAN_SOURCE);

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
                memcpy(proc + proc_len, "end", 3);
                proc[proc_len + 3] = '\0';
                in_def = false;
                Result r = proc_define_from_text(proc);
                TEST_ASSERT_MESSAGE(r.status != RESULT_ERROR, proc);
            }
            else
            {
                memcpy(proc + proc_len, line, len);
                proc[proc_len + len] = '\n';
                proc_len += len + 1;
            }
            continue;
        }

        Result r = run_string(line);
        TEST_ASSERT_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, line);
    }
    fclose(f);
}

void setUp(void)
{
    // _and_hardware gives a controllable clock so when-demon registration and
    // toot (called by kill.alien) have a backend.
    test_scaffold_setUp_with_device_and_hardware();
    load_galaxian();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

// Assert a Logo expression evaluates to the given number.
static void assert_num(const char *expr, float expected)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(expected, r.value.as.number, expr);
}

// Assert a Logo predicate evaluates to true.
static void assert_true(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(r.value), expr);
}

//==========================================================================
// The file loads and its globals are set
//==========================================================================

void test_file_loads_and_sets_globals(void)
{
    // A representative palette constant and a tuning constant from the top.
    assert_num(":blue.colour", 250);
    assert_num(":flagship.colour", 251);
    assert_num(":diver.speed", 55);
}

//==========================================================================
// Rank pyramid: 20 aliens, corner cells are permanent holes
//==========================================================================

void test_pyramid_occupancy(void)
{
    run_string("make \"conv.rows 4 make \"conv.cols 8");
    // Row 1 (flagships): only cols 4,5 occupied (idx 4,5); 1,6 are holes.
    assert_num("cell.alive 1", 0);
    assert_num("cell.alive 4", 1);
    assert_num("cell.alive 5", 1);
    assert_num("cell.alive 6", 0);
    // Row 2 (red, cols 3-6 -> idx 11-14): idx 9 (col1) is a hole.
    assert_num("cell.alive 9", 0);
    assert_num("cell.alive 11", 1);
    assert_num("cell.alive 14", 1);
    // Row 4 (blue): all 8 cols occupied.
    assert_num("cell.alive 25", 1);
    assert_num("cell.alive 32", 1);
}

void test_pyramid_total_is_20(void)
{
    // Build the real liveness list and count the ones.
    run_string("make \"conv.rows 4 make \"conv.cols 8 make \"aliens make.convoy");
    run_string("make \"n 0 foreach :aliens [[c] make \"n :n + :c]");
    assert_num(":n", 20);   // twenty ones, twelve zero holes
}

//==========================================================================
// Cell <-> row/column mapping (4x8 row-major)
//==========================================================================

void test_row_col_mapping(void)
{
    run_string("make \"conv.cols 8");
    assert_num("alien.row 1", 1);
    assert_num("alien.col 1", 1);
    assert_num("alien.row 8", 1);
    assert_num("alien.col 8", 8);
    assert_num("alien.row 9", 2);
    assert_num("alien.col 9", 1);
    assert_num("alien.row 32", 4);
    assert_num("alien.col 32", 8);
}

//==========================================================================
// Rank colour and scoring
//==========================================================================

void test_rank_colour(void)
{
    assert_num("rank.colour 1", 251);   // flagship
    assert_num("rank.colour 2", 248);   // red
    assert_num("rank.colour 3", 253);   // purple
    assert_num("rank.colour 4", 250);   // blue
}

void test_rank_score(void)
{
    // In convoy (:diving 1)
    assert_num("rank.score 4 1", 30);   // blue
    assert_num("rank.score 3 1", 40);   // purple
    assert_num("rank.score 2 1", 50);   // red
    assert_num("rank.score 1 1", 60);   // flagship
    // Diving (:diving 2) -- doubled, except the flagship is a flat 300
    assert_num("rank.score 4 2", 60);
    assert_num("rank.score 3 2", 80);
    assert_num("rank.score 2 2", 100);
    assert_num("rank.score 1 2", 300);
}

//==========================================================================
// Clamped turning (the whole feel of the game)
//==========================================================================

void test_turn_toward_clamps_and_wraps(void)
{
    // Simple clamp within range
    assert_num("turn.toward 180 240 6", 186);
    assert_num("turn.toward 240 180 6", 234);
    assert_num("turn.toward 0 90 4", 4);
    // No change when already aimed
    assert_num("turn.toward 100 100 4", 100);
    // Takes the short way across the 0/360 seam
    assert_num("turn.toward 10 350 4", 6);     // -20 clamped to -4
    assert_num("turn.toward 350 10 4", 354);   // +20 clamped to +4
}

//==========================================================================
// Shot-vs-convoy hit test (inverse of the cell formula, with tolerance)
//==========================================================================

static void seed_convoy(void)
{
    run_string("make \"conv.rows 4 make \"conv.cols 8 "
               "make \"colgap 20 make \"rowgap 20 "
               "make \"formx -70 make \"formy 140 "
               "make \"aliens make.convoy");
}

void test_locate_alien_exact_and_tolerant(void)
{
    seed_convoy();
    // Row 4 col 1 sits at (-70, 80).
    assert_num("locate.alien -70 80", 25);
    assert_num("locate.alien -64 80", 25);   // 6px off in x, within tolerance
    assert_num("locate.alien -70 88", 25);   // 8px off in y, within tolerance
    // A flagship at (10, 140) -> row1 col5 -> idx 5.
    assert_num("locate.alien 10 140", 5);
}

void test_locate_alien_misses(void)
{
    seed_convoy();
    // Row 1 col 1 position (-70,140) is a permanent hole.
    assert_num("locate.alien -70 140", 0);
    // Well outside the grid.
    assert_num("locate.alien 200 80", 0);
}

//==========================================================================
// End-to-end: level init runs on the mock, and kills / dives keep the
// liveness list and score consistent.
//==========================================================================

void test_setup_level_runs(void)
{
    run_string("make \"level 1 setup.level");
    assert_num(":alive", 20);
    assert_true("32 = count :aliens");   // 4x8 flat liveness list
}

void test_convoy_kill_scores_and_shrinks(void)
{
    run_string("make \"level 1 setup.level make \"score 0");
    // Kill a blue (row 4) convoy alien.
    run_string("kill.alien 32");
    assert_num(":score", 30);
    assert_num(":alive", 19);
    assert_true("0 = item 32 :aliens");
    // Kill a flagship in the convoy.
    run_string("kill.alien 4");
    assert_num(":score", 90);
    assert_num(":alive", 18);
}

void test_dive_detach_and_rejoin(void)
{
    run_string("make \"level 1 setup.level");
    // Detach col-1 blue as a diver on turtle 2: cell goes to liveness 2,
    // :alive is unchanged (a diving alien still counts).
    run_string("launch.diver 2 25 240");
    assert_true("2 = item 25 :aliens");
    assert_num(":alive", 20);
    assert_true("1 = item 1 :diver.phase");   // peel
    // Rejoin restores the cell and idles the diver.
    run_string("rejoin.diver 2");
    assert_true("1 = item 25 :aliens");
    assert_num(":alive", 20);
    assert_true("0 = item 1 :diver.phase");
}

void test_flight_kill_scores_doubled(void)
{
    run_string("make \"level 1 setup.level make \"score 0");
    // A flagship (idx 5) dives, then is shot in flight: flat 300, removed.
    run_string("launch.diver 2 5 180");
    run_string("diver.shot 2");
    assert_num(":score", 300);
    assert_num(":alive", 19);
    assert_true("0 = item 5 :aliens");
    assert_true("0 = item 1 :diver.phase");   // diver idled
}

void test_find_flank_walks_inward(void)
{
    run_string("make \"level 1 setup.level");
    // Front (bottom) rank of each flank column.
    assert_num("find.flank 1 1", 25);          // left  -> row4 col1
    assert_num("find.flank 8 (0 - 1)", 32);    // right -> row4 col8
    // Column 1's only live cell is the row-4 alien; empty it and the scan
    // must step inward to column 2.
    run_string("set.alien 25 0");
    assert_num("find.flank 1 1", 26);          // row4 col2
}

void test_flank_dive_launches_a_diver(void)
{
    run_string("make \"level 1 setup.level");
    // Remove both flagships so try.launch.dive takes the flank path (a
    // flagship dive would otherwise pre-empt it) and force the left side.
    run_string("set.alien 4 0 set.alien 5 0 make \"alive 18");
    run_string("make \"flank.side 0 try.launch.dive");
    assert_true("2 = item 25 :aliens");        // left front alien detached
    assert_true("1 = item 1 :diver.phase");    // turtle 2 now peeling
    assert_num(":alive", 18);                  // a diving alien still counts
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_file_loads_and_sets_globals);
    RUN_TEST(test_pyramid_occupancy);
    RUN_TEST(test_pyramid_total_is_20);
    RUN_TEST(test_row_col_mapping);
    RUN_TEST(test_rank_colour);
    RUN_TEST(test_rank_score);
    RUN_TEST(test_turn_toward_clamps_and_wraps);
    RUN_TEST(test_locate_alien_exact_and_tolerant);
    RUN_TEST(test_locate_alien_misses);
    RUN_TEST(test_setup_level_runs);
    RUN_TEST(test_convoy_kill_scores_and_shrinks);
    RUN_TEST(test_dive_detach_and_rejoin);
    RUN_TEST(test_flight_kill_scores_doubled);
    RUN_TEST(test_find_flank_walks_inward);
    RUN_TEST(test_flank_dive_launches_a_diver);
    return UNITY_END();
}
