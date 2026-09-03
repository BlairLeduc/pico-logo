//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for Dungeons of Daggorath (logo/games/daggorath), P17 M1: the
//  dungeon and the view.  docs/daggorath-design.md section 17 lists what a
//  host test can check without a board -- the maze tables, the transform,
//  the cell walk, the fade table -- and this mirrors that list plus the
//  procedure-table budget (section 14), the same way tests/test_berzerk.c
//  and tests/test_battlezone.c check their own games.
//
//  What a board still has to confirm: the actual redraw timing (that is
//  tests/logo/p17m0, already run at M0) and the turn/move animation read
//  right on a real panel -- neither is a host-observable fact.
//

#include "core/limits.h"
#include "test_scaffold.h"
#include "mock_device.h"
#include "core/repl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DAGGORATH_SOURCE
#error "DAGGORATH_SOURCE must be defined (path to logo/games/daggorath)"
#endif

// Same buffering load as tests/test_p17m0.c / tests/test_berzerk.c: `load`
// hands proc_define_from_text a whole procedure at once, so a plain line
// reader has to buffer "to ... end" blocks and run everything else through
// run_string as it goes.
static void load_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);

    char line[512];
    char proc[LOGO_LOAD_PROC_BUFFER_SIZE];
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
    load_file(DAGGORATH_SOURCE);
    run_string("splitscreen  window");
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

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
// The generated maze block -- section 17's "the maze tables"
//==========================================================================

void test_the_generated_block_is_five_levels_of_the_right_shape(void)
{
    TEST_ASSERT_EQUAL_FLOAT(5, num("count :dagg.mazes"));
    TEST_ASSERT_EQUAL_FLOAT(32, num("count item 1 :dagg.mazes"));
    TEST_ASSERT_EQUAL_FLOAT(32, num("count item 1 (item 1 :dagg.mazes)"));
    TEST_ASSERT_EQUAL_FLOAT(4, num("count :dagg.left"));
    TEST_ASSERT_EQUAL_FLOAT(4, num("count :dagg.forward"));
    TEST_ASSERT_EQUAL_FLOAT(4, num("count :dagg.right"));
}

// FPASAG (forward passage) is the one empty list in the whole file -- an
// open passage dead ahead draws nothing (VARC.ASM's own FPASAG sits right
// on the V$END byte). If the loader ever miscounted runs, this is the
// entry most likely to come back wrong.
void test_fpasag_is_the_empty_list(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0, num("count item 1 :dagg.forward"));
}

// LWALL, VARC.ASM's own transcription (also design section 6.2's worked
// example): one run of four points, ending at (136, 27).
void test_lwall_is_the_rom_shape(void)
{
    run("make \"run item 4 :dagg.left"); // 4th = wall
    TEST_ASSERT_EQUAL_FLOAT(1, num("count :run"));
    run("make \"one item 1 :run");
    TEST_ASSERT_EQUAL_FLOAT(4, num("count item 1 :one")); // 4 ys
    TEST_ASSERT_EQUAL_FLOAT(16, num("item 1 (item 1 :one)"));
    TEST_ASSERT_EQUAL_FLOAT(27, num("item 1 (item 2 :one)"));
    TEST_ASSERT_EQUAL_FLOAT(136, num("item 4 (item 1 :one)"));
    TEST_ASSERT_EQUAL_FLOAT(27, num("item 4 (item 2 :one)"));
}

// The player's own start cell, ONCE.ASM:GAME10 (`LDD #$100B / STD PROW`):
// row 16, column 11, and the one cell in the file the game itself depends
// on being carved. In the true level 1 it is a north-south corridor --
// 0b11001100 = 204: walls east and west, passages north and south, which is
// what level1.svg draws and why `daggorath` can walk forward from it.
void test_the_player_start_cell_is_the_corridor_the_map_draws(void)
{
    TEST_ASSERT_EQUAL_FLOAT(204, num("dagg.cell 0 16 11"));
}

//==========================================================================
// The transform -- section 6.2, board-confirmed at M0 (tests/logo/p17m0)
//==========================================================================

void test_the_transform_matches_the_m0_confirmed_numbers(void)
{
    run("dagg.setscale 1 :dagg.norscl");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.25f, num(":dagg.k"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 160.0f, num(":dagg.kx0"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 160.0f, num(":dagg.c"));
}

//==========================================================================
// The cell -- side-code extraction and stepping (section 6/CD.ASM)
//==========================================================================

// A synthetic cell with all four codes distinct: N=0 E=1 S=2 W=3, packed
// low bits up (0xE4 = 11 10 01 00).
void test_dagg_side_extracts_each_direction(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0, num("dagg.side 228 0"));
    TEST_ASSERT_EQUAL_FLOAT(1, num("dagg.side 228 1"));
    TEST_ASSERT_EQUAL_FLOAT(2, num("dagg.side 228 2"));
    TEST_ASSERT_EQUAL_FLOAT(3, num("dagg.side 228 3"));
}

// STPTAB, CRETUR.ASM: N row-1, E col+1, S row+1, W col-1.
void test_dagg_step_matches_stptab(void)
{
    TEST_ASSERT_EQUAL_FLOAT(9, num("dagg.step.row 10 0"));  // N
    TEST_ASSERT_EQUAL_FLOAT(10, num("dagg.step.col 10 0")); // N: col unchanged
    TEST_ASSERT_EQUAL_FLOAT(11, num("dagg.step.col 10 1")); // E
    TEST_ASSERT_EQUAL_FLOAT(11, num("dagg.step.row 10 2")); // S
    TEST_ASSERT_EQUAL_FLOAT(9, num("dagg.step.col 10 3"));  // W
}

void test_dagg_stepok_rejects_off_grid(void)
{
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string("dagg.stepok -1 5").value));
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string("dagg.stepok 5 32").value));
}

void test_dagg_stepok_rejects_a_cell_outside_the_maze(void)
{
    run("make \"dagg.level 0");
    // (31, 31) is not part of the carved maze (255 = $FF) -- see the
    // generator's own check_maze(): only 500 of 1024 cells are carved.
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string("dagg.stepok 31 31").value));
}

void test_dagg_stepok_accepts_the_real_start_cell(void)
{
    run("make \"dagg.level 0");
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string("dagg.stepok 16 11").value));
}

//==========================================================================
// The cell walk stops at the first non-passage -- section 17, "the cell
// walk". A hand-built one-level maze (not the fixture, for full control):
// walking north from (5,5), the wall sits at (3,5), so the walk should
// draw ranges 0, 1 and 2 and stop -- never reaching dagg.setscale for
// range 3. Checked indirectly: :dagg.k/:dagg.c are globals, still holding
// range 2's values after dagg.redraw returns, since dagg.setscale is never
// called again once the loop stops.
//
// (3,5)'s N side is walled AND (2,5) is $FF (not part of the maze) --
// both, because DGNGEN's Phase II only ever walls a side that faces a
// $FF neighbor or the grid edge (see gen_daggorath.py's carve_maze).
// So in any real maze a wall bit and a $FF neighbor always go together;
// dagg.stepok only checks the target cell's $FF-ness (matching STEPOK,
// CRETUR.ASM, which never inspects wall bits at all), and dagg.redraw's
// stop condition only checks the wall bit (matching VIEW60), so a
// synthetic fixture that set one without the other would make one of the
// two faithful behaviours look like a bug.
//==========================================================================

// Mazes are a list of 32 independent row-lists (see dagg.cell) -- built
// fresh a row at a time so `.setitem` on one row cannot alias another.
static void build_zero_maze(void)
{
    run("make \"rows []");
    for (int r = 0; r < 32; r++)
    {
        run("make \"row []");
        run("repeat 32 [make \"row fput 0 :row]");
        run("make \"rows lput :row :rows");
    }
    run("make \"dagg.mazes (list :rows)");
}

static void set_cell(int row, int col, int value)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), ".setitem %d (item %d :rows) %d",
             col + 1, row + 1, value);
    run(cmd);
}

static void build_synthetic_corridor(void)
{
    build_zero_maze();
    set_cell(3, 5, 3);   // (3,5): N wall, rest passage
    set_cell(2, 5, 255); // (2,5): not part of the maze
    run("make \"dagg.level 0  make \"dagg.row 5  make \"dagg.col 5  make \"dagg.dir 0");
    run("dagg.enter.level 0");
}

void test_the_forward_view_stops_at_a_wall(void)
{
    build_synthetic_corridor();
    run("dagg.redraw :dagg.norscl");
    // range 2's scale: item 3 of NORSCL = 80/128
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (80.0f / 128.0f) * 1.25f, num(":dagg.k"));
}

// And the companion case: an all-passage corridor never finds a wall
// within range 9, so the walk runs all ten ranges and dagg.setscale's last
// call is for range 9.
void test_the_forward_view_runs_all_ten_ranges_when_never_blocked(void)
{
    build_zero_maze(); // every side of every cell open
    run("make \"dagg.level 0  make \"dagg.row 5  make \"dagg.col 5  make \"dagg.dir 0");
    run("dagg.enter.level 0");
    run("dagg.redraw :dagg.norscl");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (2.0f / 128.0f) * 1.25f, num(":dagg.k")); // range 9
}

//==========================================================================
// The colours a level is drawn in -- section 4.3. Palette slot 255 is not
// a colour: it HOLDS the current graphics background (reference appendix
// E), so `setpc 255` paints in the background and draws nothing, and
// `setbg 255` is outside setbg's own 0..254 range and errors. White is
// 254. Both mistakes were in dagg.enter.level (B81): an even level drew
// an invisible picture, an odd level could not be entered at all.
//==========================================================================

void test_enter_level_never_uses_the_background_slot_as_a_colour(void)
{
    for (int level = 0; level < 2; level++)
    {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "dagg.enter.level %d", level);
        Result r = run_string(cmd);
        TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, cmd);
        TEST_ASSERT_TRUE_MESSAGE(num(":dagg.ink") < 255, "ink is the background slot");
        TEST_ASSERT_TRUE_MESSAGE(num(":dagg.bg") < 255, "bg is out of setbg's range");
        TEST_ASSERT_TRUE_MESSAGE(num(":dagg.status.fg") < 255, "status fg is the background slot");
        TEST_ASSERT_TRUE_MESSAGE(num(":dagg.status.bg") < 255, "status bg is the background slot");
        TEST_ASSERT_TRUE_MESSAGE(num(":dagg.ink") != num(":dagg.bg"), "ink is drawn on itself");
    }
}

// The whole point: the picture has to come out in a colour you can see.
void test_the_view_is_drawn_in_something_other_than_the_background(void)
{
    build_synthetic_corridor();
    mock_device_clear_graphics();
    run("dagg.redraw :dagg.norscl");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > 0, "the view drew nothing at all");
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        TEST_ASSERT_TRUE_MESSAGE(mock_device_get_line(i)->colour != 255,
                                 "the view is drawn in the background colour");
    }
}

//==========================================================================
// The forward face (B82) -- section 6's own loop draws "left / forward / right"
// (FLATAB, VIEW20). The wall that stops the walk is the one you are
// looking straight at, so it has to be drawn before the walk stops.
//==========================================================================

void test_a_wall_dead_ahead_is_drawn(void)
{
    build_synthetic_corridor();
    run("make \"dagg.row 4  make \"dagg.col 5  make \"dagg.dir 0"); // (3,5)'s N wall is one step on

    mock_device_clear_graphics();
    run("dagg.draw.side \"f 3"); // FWALL alone, at range 0's scale
    int forward_only = mock_device_line_count();
    TEST_ASSERT_TRUE_MESSAGE(forward_only > 0, "the forward wall list is empty");

    mock_device_clear_graphics();
    run("dagg.redraw :dagg.norscl");
    int with_walk = mock_device_line_count();

    mock_device_clear_graphics();
    run("make \"dagg.forward (list [] [] [] [])"); // the forward face, drawn as nothing
    run("dagg.redraw :dagg.norscl");
    TEST_ASSERT_TRUE_MESSAGE(with_walk > mock_device_line_count(),
                             "the cell walk never draws the forward face");
}

//==========================================================================
// The fade -- section 8, board-confirmed table (also tests/logo/p17m0)
//==========================================================================

void test_dagg_setfade_is_solid_at_full_brightness(void)
{
    run("make \"dagg.light 8");
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string("dagg.setfade 0").value));
    TEST_ASSERT_EQUAL_FLOAT(1, num("pendash"));
}

void test_dagg_setfade_dashes_with_distance(void)
{
    run("make \"dagg.light 8");
    run("dagg.setfade 5"); // a = 8-7-5 = -4 -> dash index 5 -> 9
    TEST_ASSERT_EQUAL_FLOAT(9, num("pendash"));
}

void test_dagg_setfade_draws_nothing_past_the_edge(void)
{
    run("make \"dagg.light 8");
    // a = 8-7-9 = -8 -> invisible (VCTFAD's own "<=-8" cutoff)
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string("dagg.setfade 9").value));
}

//==========================================================================
// MOVE / TURN legality
//==========================================================================

void test_move_forward_is_blocked_by_a_wall(void)
{
    build_synthetic_corridor();
    run("make \"dagg.row 3  make \"dagg.col 5  make \"dagg.dir 0"); // facing the wall
    run("dagg.move \"forward");
    TEST_ASSERT_EQUAL_FLOAT(3, num(":dagg.row")); // unchanged
    TEST_ASSERT_EQUAL_FLOAT(5, num(":dagg.col"));
}

void test_move_forward_succeeds_into_a_passage(void)
{
    build_synthetic_corridor();
    run("dagg.move \"forward");
    TEST_ASSERT_EQUAL_FLOAT(4, num(":dagg.row")); // stepped north
    TEST_ASSERT_EQUAL_FLOAT(5, num(":dagg.col"));
}

void test_turn_left_and_right_wrap_mod_4(void)
{
    build_synthetic_corridor();
    run("dagg.turn \"left");
    TEST_ASSERT_EQUAL_FLOAT(3, num(":dagg.dir")); // N -> W
    run("dagg.turn \"right");
    run("dagg.turn \"right");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":dagg.dir")); // W -> N -> E
}

void test_turn_around_flips_180(void)
{
    build_synthetic_corridor();
    run("dagg.turn \"around");
    TEST_ASSERT_EQUAL_FLOAT(2, num(":dagg.dir")); // N -> S
}

//==========================================================================
// hw.setcpu "fast + restore.clock -- battlezone's own pattern, reused
//==========================================================================

void test_clock_and_restore_round_trip_on_the_mock(void)
{
    // The mock hardware may or may not support hw.setcpu; either way,
    // clock/restore.clock must not error, and must leave things as they
    // were found if the clock could not be read at all.
    run("clock");
    run("restore.clock");
}

//==========================================================================
// The procedure-table budget -- section 14. Same convention as
// tests/test_battlezone.c / tests/test_berzerk.c: a static count of lines
// starting "to " in the source file itself, because nothing hands back the
// live procedure table and the file is what `load` gives a board.
//==========================================================================

void test_the_game_fits_the_procedure_table(void)
{
    FILE *f = fopen(DAGGORATH_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL(f);
    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "to ", 3) == 0)
            count++;
    }
    fclose(f);
    TEST_ASSERT_TRUE_MESSAGE(count < MAX_PROCEDURES, "daggorath is outgrowing the procedure table");
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_generated_block_is_five_levels_of_the_right_shape);
    RUN_TEST(test_fpasag_is_the_empty_list);
    RUN_TEST(test_lwall_is_the_rom_shape);
    RUN_TEST(test_the_player_start_cell_is_the_corridor_the_map_draws);
    RUN_TEST(test_the_transform_matches_the_m0_confirmed_numbers);
    RUN_TEST(test_dagg_side_extracts_each_direction);
    RUN_TEST(test_dagg_step_matches_stptab);
    RUN_TEST(test_dagg_stepok_rejects_off_grid);
    RUN_TEST(test_dagg_stepok_rejects_a_cell_outside_the_maze);
    RUN_TEST(test_dagg_stepok_accepts_the_real_start_cell);
    RUN_TEST(test_the_forward_view_stops_at_a_wall);
    RUN_TEST(test_the_forward_view_runs_all_ten_ranges_when_never_blocked);
    RUN_TEST(test_enter_level_never_uses_the_background_slot_as_a_colour);
    RUN_TEST(test_the_view_is_drawn_in_something_other_than_the_background);
    RUN_TEST(test_a_wall_dead_ahead_is_drawn);
    RUN_TEST(test_dagg_setfade_is_solid_at_full_brightness);
    RUN_TEST(test_dagg_setfade_dashes_with_distance);
    RUN_TEST(test_dagg_setfade_draws_nothing_past_the_edge);
    RUN_TEST(test_move_forward_is_blocked_by_a_wall);
    RUN_TEST(test_move_forward_succeeds_into_a_passage);
    RUN_TEST(test_turn_left_and_right_wrap_mod_4);
    RUN_TEST(test_turn_around_flips_180);
    RUN_TEST(test_clock_and_restore_round_trip_on_the_mock);
    RUN_TEST(test_the_game_fits_the_procedure_table);
    return UNITY_END();
}
