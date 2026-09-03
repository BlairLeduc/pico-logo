//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for Dungeons of Daggorath (logo/games/daggorath), P17 M1 (the
//  dungeon and the view) and M2 (the command line and the clock).
//  docs/daggorath-design.md section 17 lists what a host test can check
//  without a board -- the maze tables, the transform, the cell walk, the
//  fade table, the heart, the text widths -- and this mirrors that list
//  plus the procedure-table budget (section 14), the same way
//  tests/test_berzerk.c and tests/test_battlezone.c check their own games.
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

// Push a command line through the real HUMAN path -- one character at a
// time into dagg.key/dagg.human, then a carriage return -- so a test of a
// command is also a test of the echo, the line buffer and the parser.
// This is how the game itself gets its input (dagg.play's `dagg.human
// dagg.key rc`), and there is no other way in.
static void type_line(const char *text)
{
    char cmd[96];
    for (const char *p = text; *p; p++)
    {
        snprintf(cmd, sizeof(cmd), "dagg.human dagg.key char %d", (int)(unsigned char)*p);
        run(cmd);
    }
    run("dagg.human dagg.key char 13");
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

// SFAD10's `DECB / CMPA #-7 / BLE SFAD30` puts VCTFAD at $FF -- total
// darkness -- at MINUS SEVEN, so the last range that draws anything is the
// one at -6.  M1 read the cutoff as one step further out and gave the dash
// table an eighth entry of 65 to fill the gap; `BITMSK+8` indexed by A
// never reaches that entry, because -7 has already branched away.
void test_dagg_setfade_draws_nothing_past_the_edge(void)
{
    run("make \"dagg.light 8");
    // a = 8-7-8 = -7 -> total darkness, the first range that draws nothing
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string("dagg.setfade 8").value));
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string("dagg.setfade 9").value));
}

// The six periods are BITMSK's BIT0..BIT5 plus one, because VECTOR does
// `INC VCTFAD` before it loads FADCNT from it and `DEC VCTFAD` on the way
// out.  There is no period of 65 anywhere in the ROM.
void test_the_fade_periods_are_bitmsk_plus_one(void)
{
    run("make \"dagg.light 8");
    static const int expected[] = {1, 1, 2, 3, 5, 9, 17, 33};
    for (int range = 0; range < 8; range++)
    {
        char cmd[64], msg[64];
        snprintf(cmd, sizeof(cmd), "dagg.setfade %d", range);
        snprintf(msg, sizeof(msg), "range %d", range);
        TEST_ASSERT_EQUAL_STRING_MESSAGE("true", value_to_string(eval_string(cmd).value), msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)expected[range], num("pendash"), msg);
    }
    TEST_ASSERT_EQUAL_FLOAT(7, num("count :dagg.dash"));
}

//==========================================================================
// MOVE / TURN legality
//==========================================================================

void test_move_forward_is_blocked_by_a_wall(void)
{
    build_synthetic_corridor();
    run("make \"dagg.row 3  make \"dagg.col 5  make \"dagg.dir 0"); // facing the wall
    type_line("MOVE");
    TEST_ASSERT_EQUAL_FLOAT(3, num(":dagg.row")); // unchanged
    TEST_ASSERT_EQUAL_FLOAT(5, num(":dagg.col"));
}

void test_move_forward_succeeds_into_a_passage(void)
{
    build_synthetic_corridor();
    type_line("MOVE");
    TEST_ASSERT_EQUAL_FLOAT(4, num(":dagg.row")); // stepped north
    TEST_ASSERT_EQUAL_FLOAT(5, num(":dagg.col"));
}

void test_turn_left_and_right_wrap_mod_4(void)
{
    build_synthetic_corridor();
    type_line("TURN LEFT");
    TEST_ASSERT_EQUAL_FLOAT(3, num(":dagg.dir")); // N -> W
    type_line("TURN RIGHT");
    type_line("TURN RIGHT");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":dagg.dir")); // W -> N -> E
}

void test_turn_around_flips_180(void)
{
    build_synthetic_corridor();
    type_line("TURN AROUND");
    TEST_ASSERT_EQUAL_FLOAT(2, num(":dagg.dir")); // N -> S
}


//==========================================================================
// The turn animation -- PTURN.ASM:TURN00/LRTURN/RLTURN.  Reported from a
// Pico Plus 2 W 2026-09-03: no animation at all, and the horizontal lines
// of the view came back with dots knocked out of them.  Both are the same
// bug.  TURN00 does four things and M1 did one of them: it sets the scale
// to 1:1, sets range 0's fade, ERASES THE CURRENT SCREEN, and draws two
// full-width horizontal lines.  The sweep plays on that picture; the view
// for the new direction is built invisibly (PREVU/PUPSUB) and shown
// afterwards by PTUR90's `DEC UPDATE / SYNC`.
//
// Sweeping over the finished view instead means each `pe` stroke gouges a
// full-height stripe out of it -- which is exactly what a long horizontal
// line with dots missing looks like, and what makes the short distant
// lines disappear outright.
//==========================================================================

void test_the_turn_sweep_blanks_the_screen_and_draws_turn00s_two_lines(void)
{
    build_synthetic_corridor();
    run("dagg.redraw :dagg.norscl"); // put a view up for it to throw away
    mock_device_clear_graphics();
    run("dagg.turnsweep \"rl");

    TEST_ASSERT_TRUE_MESSAGE(mock_device_get_state()->graphics.cleared,
                             "TURN00's ZFLIP never happened -- the sweep is over the view");
    // LINES, PTURN.ASM: CoCo rows 16 and 136 across the full 256, through
    // the 1:1 transform (k 1.25, kx0 160, c 160).
    TEST_ASSERT_TRUE_MESSAGE(mock_device_has_line_from_to(-160, 140, 158.75, 140, 1.0f),
                             "the top horizontal line is missing");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_has_line_from_to(-160, -10, 158.75, -10, 1.0f),
                             "the bottom horizontal line is missing");
}

// LRTU10 loads D with 8, not 0, and adds 32 while the high byte stays
// zero: 8, 40 ... 232.  M1 started the left-to-right sweep at the screen
// edge, which is a ninth position the ROM never draws.
void test_the_left_to_right_sweep_starts_at_eight(void)
{
    build_synthetic_corridor();
    mock_device_clear_graphics();
    run("dagg.turnsweep \"lr");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_has_line_from_to(-150, 138.75, -150, -8.75, 0.5f),
                             "the first stroke is not at CoCo column 8");
    TEST_ASSERT_FALSE_MESSAGE(mock_device_has_line_from_to(-160, 138.75, -160, -8.75, 0.5f),
                              "the sweep still starts at column 0");
    // RLTU10 runs 248 down to 24, so its first stroke is not the mirror of
    // this one -- the two sweeps are not symmetric and the ROM's are not.
    mock_device_clear_graphics();
    run("dagg.turnsweep \"rl");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_has_line_from_to(150, 138.75, 150, -8.75, 0.5f),
                             "the right-to-left sweep does not start at CoCo column 248");
}

// The gate on the reported bug: after a TURN the screen holds the new view
// and nothing else.  A Logo `clean` resets the mock's line record, so what
// is recorded after the command is exactly what survived the last `clean`
// -- and if the sweep ran after the redraw, its sixteen strokes would be
// in there too.
void test_a_turn_ends_on_the_new_view_and_not_on_the_sweep(void)
{
    build_synthetic_corridor();
    mock_device_clear_graphics();
    type_line("TURN RIGHT");
    int after_turn = mock_device_line_count();

    // The same view, drawn on its own now that the direction has changed.
    mock_device_clear_graphics();
    run("dagg.redraw :dagg.norscl");
    TEST_ASSERT_EQUAL_INT_MESSAGE(mock_device_line_count(), after_turn,
                                  "the sweep drew over the finished view");
}

// PMOV30/PMOV40: PSTEP builds the new view, the sweep plays, and only then
// does PMOV90 show it -- and a blocked sidestep skips the sweep entirely
// (`BNE PMOV90`) while still redrawing.
void test_a_sidestep_animates_before_it_shows_and_not_after(void)
{
    build_synthetic_corridor();
    mock_device_clear_graphics();
    type_line("MOVE RIGHT");
    int after_move = mock_device_line_count();

    mock_device_clear_graphics();
    run("dagg.redraw :dagg.norscl");
    TEST_ASSERT_EQUAL_INT_MESSAGE(mock_device_line_count(), after_move,
                                  "the sidestep sweep drew over the finished view");
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
// M2 -- the heart.  Design section 17's "the heart": HUPDAT's formula, the
// faint threshold and the wake threshold.
//
// The oracle is HUPD20 itself rather than the closed form the Logo uses,
// so the two have to agree by arithmetic and not by transcription: the
// ROM divides by repeated subtraction and bumps the count BEFORE testing
// for the borrow, which makes J one larger than the ROM's own comment
// (J = 64P/(P+2D) - 19) says.  46 jiffies at full health, not 45.
//==========================================================================

static int rom_heartr(int power, int damage)
{
    long numerator = 64L * power;
    long denominator = power + 2L * damage;
    long count = 0;
    while (numerator >= 0)
    {
        numerator -= denominator;
        count++;
    }
    return (int)(count - 19);
}

static float heartr_at(int damage)
{
    char cmd[96];
    snprintf(cmd, sizeof(cmd), "make \"dagg.pdam %d  dagg.hupdat", damage);
    run(cmd);
    return num(":dagg.heartr");
}

void test_the_heart_rate_tracks_hupdats_own_division(void)
{
    build_synthetic_corridor();
    // Fainting and death are separate tests; hold them off by staying
    // inside the band where neither fires (damage 0..142, see below).
    static const int ramp[] = {0, 1, 10, 25, 50, 80, 100, 120, 142};
    for (size_t i = 0; i < sizeof(ramp) / sizeof(ramp[0]); i++)
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "damage %d", ramp[i]);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)rom_heartr(160, ramp[i]),
                                        heartr_at(ramp[i]), msg);
    }
}

// 160 power and no damage is 46 jiffies -- 766 ms, 78 beats a minute.
// Design section 9.4 says 45/750/80, paraphrasing the ROM's comment rather
// than its code; where the two disagree the ROM is right (section 1).
void test_the_starting_heart_rate_is_the_roms_forty_six(void)
{
    build_synthetic_corridor();
    TEST_ASSERT_EQUAL_FLOAT(46, heartr_at(0));
    TEST_ASSERT_EQUAL_FLOAT(46, (float)rom_heartr(160, 0));
}

// HUPD30's threshold is `CMPA #3 / BGT HUPD90` -- faint at three jiffies or
// fewer, which at 160 power is 153 damage: seven points short of the death
// at 160, so at full power you black out just before you die.
void test_the_faint_threshold_is_three_jiffies(void)
{
    build_synthetic_corridor();
    TEST_ASSERT_EQUAL_FLOAT(4, heartr_at(152));
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string(":dagg.faint").value));

    TEST_ASSERT_EQUAL_FLOAT(3, heartr_at(153));
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string(":dagg.faint").value));
    // HUPD32: the screen ends up black and the keyboard buffer is thrown away
    TEST_ASSERT_TRUE_MESSAGE(num(":dagg.light") < -7, "the light never went out");
}

// HUPD40's threshold is `CMPA #4 / BLE HUPD90` -- come round ABOVE four,
// so the two thresholds are not the same number and there is a band you
// stay out cold in.  Waking walks the light back one step further than
// fainting walked it down (HUPD42 increments, then tests): OLIGHT + 1.
void test_the_wake_threshold_is_four_jiffies_and_is_not_the_faint_one(void)
{
    build_synthetic_corridor();
    run("make \"dagg.pdam 153  dagg.hupdat");
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string(":dagg.faint").value));

    heartr_at(146); // heart rate 4 -- not enough to get up
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string(":dagg.faint").value));

    heartr_at(142); // heart rate 5 -- up you get
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string(":dagg.faint").value));
    TEST_ASSERT_EQUAL_FLOAT(9, num(":dagg.light")); // OLIGHT + 1, HUPD42's own
}

// HUPD90: `CMPX PDAM / BLO DEATH`, damage strictly past power.
void test_death_is_damage_past_power(void)
{
    build_synthetic_corridor();
    run("make \"dagg.faint \"true"); // skip the faint set piece, test the death
    run("make \"dagg.pdam 160  dagg.hupdat");
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string(":dagg.over").value));

    mock_device_clear_output();
    run("make \"dagg.pdam 161  dagg.hupdat");
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string(":dagg.over").value));
    TEST_ASSERT_NOT_NULL(strstr(mock_device_get_output(), "YET ANOTHER DOES NOT RETURN..."));
}

// HSLOW, COMPLR.ASM: ASRD6 on a negated damage floors, so the recovery is
// ceil(damage/64) and one point of damage still heals.  It reschedules
// itself HEARTR jiffies out, reading HEARTR after HUPDAT rather than
// before.
void test_damage_recovery_is_a_ceilinged_sixty_fourth(void)
{
    build_synthetic_corridor();
    run("make \"dagg.pdam 128  dagg.hslow");
    TEST_ASSERT_EQUAL_FLOAT(126, num(":dagg.pdam"));

    run("make \"dagg.pdam 1  dagg.hslow");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.pdam"));

    run("make \"dagg.pdam 0  dagg.hslow");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.pdam"));
}

// PMOV90: (weight / 8) + 3 on every accepted MOVE, blocked or not.  POBJWT
// is 35 until M3 puts something in the bag, so the step costs 7.
void test_every_move_costs_the_pmov90_energy(void)
{
    build_synthetic_corridor();
    run("make \"dagg.pdam 0");
    type_line("MOVE");
    TEST_ASSERT_EQUAL_FLOAT(7, num(":dagg.pdam"));

    run("make \"dagg.row 3  make \"dagg.col 5  make \"dagg.dir 0"); // into the wall
    type_line("MOVE");
    TEST_ASSERT_EQUAL_FLOAT(14, num(":dagg.pdam")); // charged anyway
}

//==========================================================================
// M2 -- the heart is a turtle (design section 4.1a), not a drawing: two
// costumes over the picture, so the redraw's `clean` never has to repair
// it.  The glyphs are SWCHAR.ASM:SPCTAB's own bits.
//==========================================================================

void test_the_heart_costumes_are_the_spctab_glyphs(void)
{
    build_synthetic_corridor();
    run("dagg.setup.heart");

    uint8_t w = 0, h = 0;
    const uint8_t *small = mock_device_get_shape(1, &w, &h);
    TEST_ASSERT_NOT_NULL(small);
    TEST_ASSERT_EQUAL_UINT8(16, w); // two 8-pixel character cells
    TEST_ASSERT_EQUAL_UINT8(8, h);

    // Small heart, scan line 1: %00000000 %10100000 -- the two lobes at
    // columns 8 and 10, transparent everywhere else.  254 is `fe`, the
    // wearing turtle's own pen colour, which is what makes one pair of
    // costumes serve both polarities of section 4.3.
    for (int col = 0; col < 16; col++)
    {
        bool ink = (col == 8 || col == 10);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(ink ? 254 : 255, small[1 * 16 + col],
                                        "small heart, scan line 1");
    }

    const uint8_t *large = mock_device_get_shape(2, &w, &h);
    TEST_ASSERT_NOT_NULL(large);
    // Large heart, scan line 2: %00000011 %11111000 -- columns 6 through 12.
    for (int col = 0; col < 16; col++)
    {
        bool ink = (col >= 6 && col <= 12);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(ink ? 254 : 255, large[2 * 16 + col],
                                        "large heart, scan line 2");
    }
}

// CLK30 toggles rather than cycling, so the drawn period is two beats, and
// the next beat is scheduled HEARTR jiffies out (COMMON.ASM reloads HEARTC
// from HEARTR at the beat, which is the same thing counted the other way).
void test_a_beat_toggles_the_costume_and_schedules_the_next(void)
{
    build_synthetic_corridor();
    run("dagg.setup.heart");
    run("make \"dagg.heartr 46  make \"dagg.now 1000");

    run("dagg.beat");
    TEST_ASSERT_EQUAL_FLOAT(2, num("ask 1 [shape]"));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1000.0f + 46.0f * 16.667f, num(":dagg.heart.due"));

    run("make \"dagg.now 2000  dagg.beat");
    TEST_ASSERT_EQUAL_FLOAT(1, num("ask 1 [shape]"));
}

// The level flip is a `setpc` on turtle 1 and not a second pair of shapes
// -- NLVL50 inverts the status row with the view (design section 4.3).
void test_the_heart_flips_polarity_with_the_level(void)
{
    build_synthetic_corridor();
    run("dagg.setup.heart");
    run("dagg.enter.level 0");
    TEST_ASSERT_EQUAL_FLOAT(num(":dagg.status.fg"), num("ask 1 [pc]"));
    run("dagg.enter.level 1");
    TEST_ASSERT_EQUAL_FLOAT(num(":dagg.status.fg"), num("ask 1 [pc]"));
}

//==========================================================================
// M2 -- the parser (PARSER.ASM/TOKEN.ASM).  A token is matched as a PREFIX
// and two matches are an error, not a preference.  A token is a list of
// one-character words, which is exactly what a Logo list literal of single
// letters is.
//==========================================================================

void test_a_command_matches_on_any_unambiguous_prefix(void)
{
    TEST_ASSERT_EQUAL_FLOAT(8, num("dagg.parse [M] :dagg.cmdtab"));      // MOVE
    TEST_ASSERT_EQUAL_FLOAT(8, num("dagg.parse [M O] :dagg.cmdtab"));
    TEST_ASSERT_EQUAL_FLOAT(8, num("dagg.parse [M O V E] :dagg.cmdtab"));
    TEST_ASSERT_EQUAL_FLOAT(1, num("dagg.parse [A] :dagg.cmdtab"));      // ATTACK
    TEST_ASSERT_EQUAL_FLOAT(4, num("dagg.parse [E X] :dagg.cmdtab"));    // EXAMINE
}

// PARS20's `TST PARFLG / BNE PARS90`: a second match is NEGONE, and there
// is no rule that prefers the shorter word or the earlier row.
void test_two_matches_are_an_error_not_a_preference(void)
{
    TEST_ASSERT_EQUAL_FLOAT(-1, num("dagg.parse [Z] :dagg.cmdtab"));
    TEST_ASSERT_EQUAL_FLOAT(14, num("dagg.parse [Z L] :dagg.cmdtab"));
    TEST_ASSERT_EQUAL_FLOAT(15, num("dagg.parse [Z S] :dagg.cmdtab"));
}

// The four-letter names in DTABAS.ASM's CMDXXX macro (ATTK, INCN, REVE,
// ZSAV) are assembler symbols.  CMDTAB holds the full words and PARS12
// matches prefixes, so ATTK is not a command -- design section 15's
// "four-letter abbreviations" reads the macro rather than the table.
void test_the_four_letter_symbols_are_not_what_the_parser_matches(void)
{
    TEST_ASSERT_EQUAL_FLOAT(-1, num("dagg.parse [A T T K] :dagg.cmdtab"));
    TEST_ASSERT_EQUAL_FLOAT(-1, num("dagg.parse [I N C N] :dagg.cmdtab"));
    TEST_ASSERT_EQUAL_FLOAT(1, num("dagg.parse [A T T A] :dagg.cmdtab"));
}

// PARS92 stores D = 0 for a null token and PARS90 stores NEGONE for a
// failure, which is why HMAN50 tests BEQ and BPL separately and PMOVE
// tests BLT and BGT.
void test_a_null_token_and_a_failure_are_different_answers(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0, num("dagg.parse [] :dagg.cmdtab"));
    TEST_ASSERT_EQUAL_FLOAT(-1, num("dagg.parse [Q] :dagg.cmdtab"));
    TEST_ASSERT_EQUAL_FLOAT(-1, num("dagg.parse [M O V I E] :dagg.cmdtab"));
}

// FULFLG, which INCANT reads (M3) and nothing else does.
void test_fulflg_is_set_only_by_a_whole_word(void)
{
    run("ignore dagg.parse [M O V E] :dagg.cmdtab");
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string(":dagg.fulflg").value));
    run("ignore dagg.parse [M O V] :dagg.cmdtab");
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(eval_string(":dagg.fulflg").value));
}

// T.BAK's string is BACK, not BACKWARD (TOKEN.ASM) -- so the word the
// CoCo manual prints as MOVE BACKWARD is not what DIRTAB holds, and the
// prefix rule is what makes both spellings of the abbreviation work.
void test_dirtab_holds_back_and_not_backward(void)
{
    TEST_ASSERT_EQUAL_FLOAT(3, num("dagg.parse [B] :dagg.dirtab"));
    TEST_ASSERT_EQUAL_FLOAT(3, num("dagg.parse [B A C K] :dagg.dirtab"));
    TEST_ASSERT_EQUAL_FLOAT(-1, num("dagg.parse [B A C K W A R D] :dagg.dirtab"));
}

//==========================================================================
// M2 -- HUMAN (HUMAN.ASM): the line buffer, the echo and the dispatch.
//==========================================================================

void test_a_command_reaches_its_handler_through_the_typed_line(void)
{
    build_synthetic_corridor();
    type_line("M"); // one letter is a whole command
    TEST_ASSERT_EQUAL_FLOAT(4, num(":dagg.row"));

    build_synthetic_corridor();
    type_line("  move  "); // leading, trailing and doubled spaces are eaten
    TEST_ASSERT_EQUAL_FLOAT(4, num(":dagg.row"));
}

// HMAN40: backspace takes the last character back out of LINBUF, and does
// nothing at all at the start of the buffer.
void test_backspace_unbuffers_and_stops_at_the_start(void)
{
    build_synthetic_corridor();
    run("dagg.human dagg.key char 77");  // M
    run("dagg.human dagg.key char 88");  // X
    TEST_ASSERT_EQUAL_FLOAT(2, num("count :dagg.linbuf"));
    run("dagg.human dagg.key char 8");
    TEST_ASSERT_EQUAL_FLOAT(1, num("count :dagg.linbuf"));
    run("dagg.human dagg.key char 8");
    run("dagg.human dagg.key char 8");
    TEST_ASSERT_EQUAL_FLOAT(0, num("count :dagg.linbuf"));
}

// PLAY10: anything that is not A-Z, a space, a return or a backspace
// becomes a space (the CLRB fallthrough), and lower case is folded up.
void test_the_key_conversion_is_play10s(void)
{
    TEST_ASSERT_EQUAL_STRING("M", value_to_string(eval_string("dagg.key char 109").value));
    TEST_ASSERT_EQUAL_STRING("cr", value_to_string(eval_string("dagg.key char 13").value));
    TEST_ASSERT_EQUAL_STRING("bs", value_to_string(eval_string("dagg.key char 8").value));
    TEST_ASSERT_EQUAL_STRING("esc", value_to_string(eval_string("dagg.key char 27").value));
    TEST_ASSERT_EQUAL_FLOAT(32, num("ascii dagg.key char 55")); // '7'
    TEST_ASSERT_EQUAL_FLOAT(32, num("ascii dagg.key char 44")); // ','
}

// HMAN20's `CMPU #LINEND / BNE HMAN99` falls into the carriage-return path
// when the 32-byte buffer fills, so a 32-character line submits itself.
void test_a_full_line_buffer_submits_itself(void)
{
    build_synthetic_corridor();
    for (int i = 0; i < 31; i++)
    {
        run("dagg.human dagg.key char 88"); // X, 31 of them
    }
    TEST_ASSERT_EQUAL_FLOAT(31, num("count :dagg.linbuf"));
    run("dagg.human dagg.key char 88"); // the 32nd
    TEST_ASSERT_EQUAL_FLOAT(0, num("count :dagg.linbuf"));
}

// PLAY10 eats characters while FAINT is set -- you cannot type your way
// out of being unconscious.
void test_typing_does_nothing_while_unconscious(void)
{
    build_synthetic_corridor();
    run("make \"dagg.faint \"true");
    run("dagg.human dagg.key char 77");
    TEST_ASSERT_EQUAL_FLOAT(0, num("count :dagg.linbuf"));
}

// ESC is this port's one addition to PLAY10 -- the ROM's death loop is
// `BRA *` and there is no other way out of dagg.play.
void test_escape_ends_the_game_loop(void)
{
    build_synthetic_corridor();
    run("make \"dagg.over \"false");
    run("dagg.human dagg.key char 27");
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(eval_string(":dagg.over").value));
}

//==========================================================================
// M2 -- the text (design section 17): every status and message line's
// rendered width, measured by `type` against the mock with the output
// cleared, because nothing wraps at 40 columns (section 4.1c).
//==========================================================================

static size_t rendered_width(const char *expression)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "type %s", expression);
    mock_device_clear_output();
    run(cmd);
    return strlen(mock_device_get_output());
}

void test_the_status_line_is_exactly_forty_columns(void)
{
    build_synthetic_corridor();
    TEST_ASSERT_EQUAL_UINT(40, rendered_width("dagg.status.line"));
}

void test_every_message_line_fits_the_forty_column_screen(void)
{
    build_synthetic_corridor();
    TEST_ASSERT_TRUE(rendered_width("[YET ANOTHER DOES NOT RETURN...]") <= 39);
    TEST_ASSERT_TRUE(rendered_width("[NOT YET]") <= 40);

    mock_device_clear_output();
    run("dagg.cmderr");
    TEST_ASSERT_EQUAL_STRING("??", mock_device_get_output());
}

// STATUX runs before PUPDAT and the redraw's `clean` is the only eraser
// there is (section 4.1b), so the status line has to be part of the
// redraw and not something drawn once and left.
void test_the_status_line_survives_a_redraw(void)
{
    build_synthetic_corridor();
    run("dagg.redraw :dagg.norscl");

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_TRUE_MESSAGE(state->label.count > 0, "the redraw wrote no status line");
    TEST_ASSERT_EQUAL_UINT(40, strlen(state->label.last_text));
    // The opposite polarity to the view above it -- P18 M1's third argument
    TEST_ASSERT_EQUAL_FLOAT(num(":dagg.status.fg"), (float)state->label.last_colour);
    TEST_ASSERT_EQUAL_FLOAT(num(":dagg.status.bg"), (float)state->label.last_background);
    // Column 0 of the 40, and the character row the status band starts on
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -160.0f, state->label.last_x);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -55.0f, state->label.last_y);
}

// It is the bar and not just the glyphs: STATUX inverts the whole status
// row, so the spaces between the two hand names are filled cells.
void test_the_status_bar_is_opaque_across_its_whole_width(void)
{
    build_synthetic_corridor();
    run("dagg.redraw :dagg.norscl");
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_TRUE_MESSAGE(state->label.last_background >= 0,
                             "the status line is transparent, so there is no bar");
    TEST_ASSERT_NOT_EQUAL(state->label.last_colour, (uint16_t)state->label.last_background);

    // NLVL50 sets VDGINV to -(level & 1) and STATUX takes its complement,
    // so the bar is the opposite polarity to the view on every level and
    // both polarities of it are the same two colours the other way up.
    run("dagg.enter.level 0");
    float even_fg = num(":dagg.status.fg"), even_bg = num(":dagg.status.bg");
    run("dagg.enter.level 1");
    TEST_ASSERT_EQUAL_FLOAT(even_bg, num(":dagg.status.fg"));
    TEST_ASSERT_EQUAL_FLOAT(even_fg, num(":dagg.status.bg"));
}

//==========================================================================
// M2 -- the scheduler (design section 5).  One pass over the timers that
// exist; `dagg.heart.tick` is the IRQ's half, which a long blocking effect
// keeps calling so the beat keeps time through it (section 9.4).
//==========================================================================

void test_the_tick_beats_only_when_the_beat_is_due(void)
{
    build_synthetic_corridor();
    run("dagg.setup.heart");
    set_mock_ticks(10000);
    run("make \"dagg.heartr 46");
    run("make \"dagg.heart.due 20000  make \"dagg.hslow.due 20000");
    run("dagg.tick");
    TEST_ASSERT_EQUAL_FLOAT(1, num("ask 1 [shape]")); // nothing was due

    run("make \"dagg.heart.due 10000  dagg.tick"); // due exactly now
    TEST_ASSERT_EQUAL_FLOAT(2, num("ask 1 [shape]"));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 10000.0f + 46.0f * 16.667f, num(":dagg.heart.due"));
}

void test_the_recovery_task_reschedules_itself_at_the_heart_rate(void)
{
    build_synthetic_corridor();
    run("dagg.setup.heart");
    set_mock_ticks(50000);
    run("make \"dagg.pdam 64  make \"dagg.hslow.due 50000");
    run("make \"dagg.heart.due 999999"); // the beat is not what is being tested
    run("dagg.tick");
    TEST_ASSERT_EQUAL_FLOAT(63, num(":dagg.pdam"));
    // HSLOW reads HEARTR after HUPDAT, not before -- the recovery slows
    // down as the damage that caused it comes off.
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 50000.0f + num(":dagg.heartr") * 16.667f,
                             num(":dagg.hslow.due"));
}

//==========================================================================
// M2 -- the budgets (design section 17).  A warm redraw spends nothing:
// the status line is built out of `word` and `dagg.spaces` a redraw at a
// time, and every intermediate it makes is the SAME word as last time, so
// the word table stops growing after the first one.  This is the gate on
// that decision -- a status line that varied a character a redraw would
// intern a new word every frame and this test is how that shows up.
//==========================================================================

// Running one instruction list costs a node or two of its own, whatever is
// inside it, so the measurement is taken at two loop lengths and compared:
// if the redraw allocated, ten times as many of them would cost ten times
// as much.  Each string is run twice because the FIRST run of a given one
// also mints its own words.
static float nodes_for_redraws(int count)
{
    char cmd[192];
    snprintf(cmd, sizeof(cmd),
             "make \"n0 nodes  repeat %d [dagg.redraw :dagg.norscl]  make \"n1 nodes",
             count);
    run(cmd);
    run(cmd);
    return num(":n0") - num(":n1");
}

void test_a_warm_redraw_spends_no_nodes_and_no_atoms(void)
{
    build_synthetic_corridor();
    run("dagg.setup.heart");
    run("make \"n0 0  make \"n1 0  make \"a0 0  make \"a1 0");
    run("repeat 5 [dagg.redraw :dagg.norscl]");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(nodes_for_redraws(100), nodes_for_redraws(1000),
                                    "the redraw conses, and ten times as many cost ten times as much");

    run("make \"a0 atoms  repeat 1000 [dagg.redraw :dagg.norscl]  make \"a1 atoms");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":a0"), num(":a1"),
                                    "the redraw interned a word -- the status line is not warm");
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
    RUN_TEST(test_the_fade_periods_are_bitmsk_plus_one);
    RUN_TEST(test_the_turn_sweep_blanks_the_screen_and_draws_turn00s_two_lines);
    RUN_TEST(test_the_left_to_right_sweep_starts_at_eight);
    RUN_TEST(test_a_turn_ends_on_the_new_view_and_not_on_the_sweep);
    RUN_TEST(test_a_sidestep_animates_before_it_shows_and_not_after);
    RUN_TEST(test_move_forward_is_blocked_by_a_wall);
    RUN_TEST(test_move_forward_succeeds_into_a_passage);
    RUN_TEST(test_turn_left_and_right_wrap_mod_4);
    RUN_TEST(test_turn_around_flips_180);
    RUN_TEST(test_the_heart_rate_tracks_hupdats_own_division);
    RUN_TEST(test_the_starting_heart_rate_is_the_roms_forty_six);
    RUN_TEST(test_the_faint_threshold_is_three_jiffies);
    RUN_TEST(test_the_wake_threshold_is_four_jiffies_and_is_not_the_faint_one);
    RUN_TEST(test_death_is_damage_past_power);
    RUN_TEST(test_damage_recovery_is_a_ceilinged_sixty_fourth);
    RUN_TEST(test_every_move_costs_the_pmov90_energy);
    RUN_TEST(test_the_heart_costumes_are_the_spctab_glyphs);
    RUN_TEST(test_a_beat_toggles_the_costume_and_schedules_the_next);
    RUN_TEST(test_the_heart_flips_polarity_with_the_level);
    RUN_TEST(test_a_command_matches_on_any_unambiguous_prefix);
    RUN_TEST(test_two_matches_are_an_error_not_a_preference);
    RUN_TEST(test_the_four_letter_symbols_are_not_what_the_parser_matches);
    RUN_TEST(test_a_null_token_and_a_failure_are_different_answers);
    RUN_TEST(test_fulflg_is_set_only_by_a_whole_word);
    RUN_TEST(test_dirtab_holds_back_and_not_backward);
    RUN_TEST(test_a_command_reaches_its_handler_through_the_typed_line);
    RUN_TEST(test_backspace_unbuffers_and_stops_at_the_start);
    RUN_TEST(test_the_key_conversion_is_play10s);
    RUN_TEST(test_a_full_line_buffer_submits_itself);
    RUN_TEST(test_typing_does_nothing_while_unconscious);
    RUN_TEST(test_escape_ends_the_game_loop);
    RUN_TEST(test_the_status_line_is_exactly_forty_columns);
    RUN_TEST(test_every_message_line_fits_the_forty_column_screen);
    RUN_TEST(test_the_status_line_survives_a_redraw);
    RUN_TEST(test_the_status_bar_is_opaque_across_its_whole_width);
    RUN_TEST(test_the_tick_beats_only_when_the_beat_is_due);
    RUN_TEST(test_the_recovery_task_reschedules_itself_at_the_heart_rate);
    RUN_TEST(test_clock_and_restore_round_trip_on_the_mock);
    RUN_TEST(test_a_warm_redraw_spends_no_nodes_and_no_atoms);
    RUN_TEST(test_the_game_fits_the_procedure_table);
    return UNITY_END();
}
