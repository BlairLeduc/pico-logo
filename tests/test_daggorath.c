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
#include "core/variables.h"
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
    // M3 made the light real: RLIGHT/MLIGHT come from a burning torch and
    // the player's own light is zero, so a fixture with no torch draws a
    // black screen.  dagg.redraw does not recompute them (PUPSUB does, and
    // HUPD30 depends on it not), so setting them here is what the ROM's
    // "typical dungeon level 1 or 2" comment on the VIEWER inputs means.
    run("make \"dagg.light 8  make \"dagg.mlight 8");
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

// B87 -- the command line draws its own cursor, because the one
// devices/picocalc/input.c turns on lives only inside its line reader and
// dagg.play never enters it (it polls with `key?`/`rc`, because this is a
// typing game that must not block).  Asserted as the raw byte stream the
// mock records, which is the right level: emitting the sequence is the
// game's job and turning it into a picture is the device's -- and our
// backspace is DESTRUCTIVE (screen.c:screen_txt_putc steps back and clears
// the cell it lands on) where the CoCo's I.BS was a pure cursor move,
// which is exactly why the ROM's own bytes could not be copied across.
void test_the_command_line_carries_its_own_cursor(void)
{
    build_synthetic_corridor();

    // M$PROM1 is `FCB I.CR,I.DOT` falling into M$CURS -- one string
    mock_device_clear_output();
    run("dagg.prompt");
    TEST_ASSERT_EQUAL_STRING("\n._", mock_device_get_output());

    // HMAN20: rub the cursor out, echo the character, put the cursor back
    mock_device_clear_output();
    run("dagg.human dagg.key char 77"); // M
    TEST_ASSERT_EQUAL_STRING("\bM_", mock_device_get_output());

    // M$ERAS: the cursor, then the character, then the cursor where the
    // character was
    mock_device_clear_output();
    run("dagg.human dagg.key char 8");
    TEST_ASSERT_EQUAL_STRING("\b\b_", mock_device_get_output());

    // ...and a backspace on an empty line emits nothing at all, so it can
    // never eat the prompt's own period
    mock_device_clear_output();
    run("dagg.human dagg.key char 8");
    TEST_ASSERT_EQUAL_STRING("", mock_device_get_output());

    // HMAN30 erases the cursor before the command prints anything where it
    // was standing -- and then the next prompt puts a fresh one up
    run("make \"dagg.linbuf []");
    mock_device_clear_output();
    run("dagg.enter");
    TEST_ASSERT_EQUAL_STRING("\b\n._", mock_device_get_output());
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
// M3 -- objects (design section 15).  Everything below reads out of the
// generated block, which is itself read out of TOKEN.ASM and DTABAS.ASM by
// scripts/gen_daggorath.py -- so these constants are section 10.2 of the
// design, transcribed by hand, checked against the ROM's own tables.  A
// generator bug is a failing test here rather than a wrong game.
//==========================================================================

static const char *text(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    return value_to_string(r.value);
}

// The whole of ONCE.ASM's start-of-game: the dungeon's loot from OMXTAB,
// then GAMDAT's two objects into the player's bag.  It is `daggorath`
// without the screen, the clock and the input loop.
static void start_game(void)
{
    build_synthetic_corridor();
    run("make \"dagg.ppow 160  make \"dagg.pdam 0  make \"dagg.objwt 35");
    run("make \"dagg.plhand 0  make \"dagg.prhand 0  make \"dagg.ptorch 0");
    run("make \"dagg.bag []  make \"dagg.floor []  make \"dagg.dspmod 0");
    run("make \"dagg.prlite 0  make \"dagg.prmlite 0  make \"dagg.heartf \"true");
    run("dagg.makeobjects");
    run("dagg.givebag");
}

static float field(const char *list, int i)
{
    char expr[96];
    snprintf(expr, sizeof(expr), "item %d :%s", i, list);
    return num(expr);
}

// docs/daggorath-design.md section 10.2, in ADJTAB order -- which is the
// order P.OCTYP indexes, so the row number here IS the object type.
struct object_row
{
    const char *word;
    int cls, reveal, magoff, physoff, level, count;
};

static const struct object_row SECTION_10_2[] = {
    {"SUPREME", 1, 255, 0, 5, 4, 1},
    {"JOULE", 1, 170, 0, 5, 3, 1},
    {"ELVISH", 4, 150, 64, 64, 3, 1},
    {"MITHRIL", 3, 140, 13, 26, 3, 2},
    {"SEER", 2, 130, 0, 5, 2, 3},
    {"THEWS", 0, 70, 0, 5, 2, 3},
    // The design's own table calls this one HOTH.  It is not what the
    // player types -- see the next test.
    {"RIME", 1, 52, 0, 5, 1, 1},
    {"VISION", 2, 50, 0, 5, 1, 3},
    {"ABYE", 0, 48, 0, 5, 1, 6},
    {"HALE", 0, 40, 0, 5, 1, 4},
    {"SOLAR", 5, 70, 0, 5, 1, 4},
    {"BRONZE", 3, 25, 0, 26, 1, 6},
    {"VULCAN", 1, 13, 0, 5, 0, 1},
    {"IRON", 4, 13, 0, 40, 0, 4},
    {"LUNAR", 5, 25, 0, 5, 0, 8},
    {"PINE", 5, 5, 0, 5, 0, 8},
    {"LEATHER", 3, 5, 0, 10, 0, 3},
    {"WOODEN", 4, 5, 0, 16, 0, 4},
};

void test_the_object_tables_are_section_10_2(void)
{
    TEST_ASSERT_EQUAL_FLOAT(25, num("count :dagg.adjtab"));
    TEST_ASSERT_EQUAL_FLOAT(25, num("count :dagg.odb"));
    TEST_ASSERT_EQUAL_FLOAT(25, num("count :dagg.xxx"));
    TEST_ASSERT_EQUAL_FLOAT(18, num("count :dagg.omx"));

    for (int i = 0; i < 18; i++)
    {
        const struct object_row *w = &SECTION_10_2[i];
        char expr[96];
        snprintf(expr, sizeof(expr), "first item %d :dagg.adjtab", i + 1);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(w->word, text(expr), w->word);
        snprintf(expr, sizeof(expr), "item 2 (item %d :dagg.adjtab)", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(w->cls, num(expr), w->word);
        snprintf(expr, sizeof(expr), "item 1 (item %d :dagg.odb)", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(w->cls, num(expr), w->word);
        snprintf(expr, sizeof(expr), "item 2 (item %d :dagg.odb)", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(w->reveal, num(expr), w->word);
        snprintf(expr, sizeof(expr), "item 3 (item %d :dagg.odb)", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(w->magoff, num(expr), w->word);
        snprintf(expr, sizeof(expr), "item 4 (item %d :dagg.odb)", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(w->physoff, num(expr), w->word);
        snprintf(expr, sizeof(expr), "item 1 (item %d :dagg.omx)", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(w->level, num(expr), w->word);
        snprintf(expr, sizeof(expr), "item 2 (item %d :dagg.omx)", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(w->count, num(expr), w->word);
    }
}

// The third time this port has been caught reading a macro instead of the
// table it generates (LVLTAB at M1, CMDTAB at M2).  DTABAS.ASM's OBJXXX
// macro names this object HOTH and the design's section 10.2 copied that;
// TOKEN.ASM's ADJTAB, which is what PARSER matches and OBJNAM prints,
// holds RIME.  HOTH is not a word this game knows.
void test_the_ring_the_macro_names_is_not_the_ring_the_player_types(void)
{
    TEST_ASSERT_EQUAL_STRING("RIME", text("first item 7 :dagg.adjtab"));
    // 7 rows of DIRTAB-style tokens, and HOTH matches none of them
    TEST_ASSERT_EQUAL_FLOAT(-1, num("dagg.parse [H O T H] :dagg.adjtab"));
    TEST_ASSERT_EQUAL_FLOAT(7, num("dagg.parse [R I M E] :dagg.adjtab"));
}

void test_the_special_objects_follow_the_eighteen(void)
{
    const char *specials[] = {"FINAL", "ENERGY", "ICE", "FIRE", "GOLD", "EMPTY", "DEAD"};
    for (int i = 0; i < 7; i++)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "first item %d :dagg.adjtab", 19 + i);
        TEST_ASSERT_EQUAL_STRING(specials[i], text(expr));
    }
    // The three attack rings are 255/255 and always hit; FINAL is 0/0 and
    // ends the game instead (PATTK.ASM, PINCAN.ASM:WINNER -- M5).
    TEST_ASSERT_EQUAL_FLOAT(255, num("item 3 (item 20 :dagg.odb)"));
    TEST_ASSERT_EQUAL_FLOAT(255, num("item 4 (item 20 :dagg.odb)"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("item 3 (item 19 :dagg.odb)"));
}

// GENXXX's own weights, by class, and the six object outlines VOBJ.ASM
// draws on the floor -- indexed by class, not type, so one torch outline
// serves all four torches.
void test_the_weights_and_outlines_are_by_class(void)
{
    const char *names[] = {"FLASK", "RING", "SCROLL", "SHIELD", "SWORD", "TORCH"};
    const int weights[] = {5, 1, 10, 25, 25, 10};
    for (int i = 0; i < 6; i++)
    {
        char expr[64];
        snprintf(expr, sizeof(expr), "first item %d :dagg.gentab", i + 1);
        TEST_ASSERT_EQUAL_STRING(names[i], text(expr));
        TEST_ASSERT_EQUAL_FLOAT(weights[i], field("dagg.objwgt", i + 1));
    }
    TEST_ASSERT_EQUAL_FLOAT(6, num("count :dagg.fobj"));
    for (int i = 0; i < 6; i++)
        TEST_ASSERT_TRUE_MESSAGE(num("count item 1 :dagg.fobj") > 0, names[i]);
    // FSWORD is the only object list with a pen lift in it (V$NEW between
    // the blade and the hand guard); the other five are one closed run.
    TEST_ASSERT_EQUAL_FLOAT(2, num("count item 5 :dagg.fobj"));
    // FSHIEL's SVORG is not a pen lift, so its six points are one run
    TEST_ASSERT_EQUAL_FLOAT(1, num("count item 4 :dagg.fobj"));
    TEST_ASSERT_EQUAL_FLOAT(6, num("count item 1 (item 1 (item 4 :dagg.fobj))"));
}

//==========================================================================
// Birth and the bag -- ONCE.ASM:CINI40/GAME30, OBIRTH.ASM.
//==========================================================================

void test_every_object_in_the_dungeon_is_created_and_creature_owned(void)
{
    build_synthetic_corridor();
    run("dagg.makeobjects");
    TEST_ASSERT_EQUAL_FLOAT(63, num(":dagg.ocbptr"));
    for (int i = 1; i <= 63; i++)
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-1, field("dagg.ocown", i),
                                        "an object was born unowned");
    // Nothing is lying on the floor at the start of a game (section 7.3)
    TEST_ASSERT_EQUAL_FLOAT(0, num("count :dagg.floor"));
    // ...and there is room for GAMDAT's two on top, exactly
    run("dagg.givebag");
    TEST_ASSERT_EQUAL_FLOAT(65, num(":dagg.ocbptr"));
    TEST_ASSERT_EQUAL_FLOAT(65, num(":dagg.ocbmax"));
}

// CINI44 walks DOWN from the object's first level and wraps, and it allows
// a level 5 the dungeon does not have -- design section 19, decision 4.
void test_the_distribution_walks_down_and_wraps_past_level_five(void)
{
    build_synthetic_corridor();
    run("dagg.makeobjects");
    // ABYE flasks: six of them starting at level 1, so 1 2 3 4 5 1
    const int want[] = {1, 2, 3, 4, 5, 1};
    for (int i = 0; i < 6; i++)
        TEST_ASSERT_EQUAL_FLOAT(want[i], field("dagg.oclvl", 16 + i));
}

void test_the_player_starts_with_a_wooden_sword_and_a_pine_torch(void)
{
    start_game();
    TEST_ASSERT_EQUAL_FLOAT(2, num("count :dagg.bag"));
    TEST_ASSERT_EQUAL_STRING("WOODEN SWORD", text("dagg.objnam item 1 :dagg.bag"));
    TEST_ASSERT_EQUAL_STRING("PINE TORCH", text("dagg.objnam item 2 :dagg.bag"));
    // POBJWT's own `FDB 30+5` is those two: a 25 sword and a 10 torch
    TEST_ASSERT_EQUAL_FLOAT(25 + 10, num(":dagg.objwt"));
    // GAME30 clears P.OCREV, so both are revealed from the start
    TEST_ASSERT_EQUAL_FLOAT(0, num("item (item 1 :dagg.bag) :dagg.ocrev"));
    // and nothing is burning
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.ptorch"));
}

// GAMDAT is a table and not a constant -- ONCE.ASM has two of them, one
// for a game and one for the attract mode (DEMDAT: iron sword, pine torch,
// leather shield), and GAME20 picks between them with a pointer.  So it is
// the seam a board uses to reach an object M3 otherwise cannot: nothing in
// the dungeon is pickable until creatures exist to drop it (section 7.3).
//
// `make "dagg.gamdat [12 15]` before `daggorath` is a Vulcan ring and a
// pine torch, which is what INCANT needs -- PINCAN reads P.OCXXX+1 and
// never looks at P.OCREV, so GAME30's `CLR P.OCREV,X` does not spoil it.
// REVEAL is not reachable this way and stays gated on M4, because GAME30
// reveals everything it hands you.
void test_gamdat_is_the_seam_a_board_uses_to_reach_a_ring(void)
{
    build_synthetic_corridor();
    run("make \"dagg.plhand 0  make \"dagg.prhand 0  make \"dagg.ptorch 0");
    run("make \"dagg.bag []  make \"dagg.floor []  make \"dagg.dspmod 0");
    run("make \"dagg.gamdat [12 15]"); // VULCAN ring, PINE torch
    run("dagg.makeobjects  dagg.givebag");

    TEST_ASSERT_EQUAL_FLOAT(2, num("count :dagg.bag"));
    TEST_ASSERT_EQUAL_STRING("VULCAN RING", text("dagg.objnam item 1 :dagg.bag"));
    // Revealed, as GAME30 leaves everything it gives -- and the ring is
    // still incantable, because the charge and the target type live in
    // P.OCXXX, which the reveal does not touch.
    TEST_ASSERT_EQUAL_FLOAT(0, num("item (item 1 :dagg.bag) :dagg.ocrev"));
    TEST_ASSERT_EQUAL_FLOAT(21, num("item (item 1 :dagg.bag) :dagg.ocx1"));

    type_line("PULL LEFT RING");
    type_line("INCANT FIRE");
    TEST_ASSERT_EQUAL_STRING("FIRE RING", text("dagg.objnam :dagg.plhand"));

    // ...and it does not reach REVEAL, which is the honest half of this:
    // there is nothing in the bag left to reveal.
    mock_device_clear_output();
    type_line("REVEAL LEFT");
    TEST_ASSERT_EQUAL_STRING("FIRE RING", text("dagg.objnam :dagg.plhand"));
}

//==========================================================================
// Names and REVEAL -- STATUS.ASM:OBJNAM, OBIRTH.ASM:GENVAL, PREVEA.ASM.
//==========================================================================

void test_an_unrevealed_object_shows_only_its_generic_name(void)
{
    start_game();
    run("make \"dagg.ocbptr 0  make \"i dagg.obirth 3 0"); // MITHRIL
    TEST_ASSERT_EQUAL_STRING("SHIELD", text("dagg.objnam :i"));
    run(".setitem :i :dagg.ocrev 0");
    TEST_ASSERT_EQUAL_STRING("MITHRIL SHIELD", text("dagg.objnam :i"));
    TEST_ASSERT_EQUAL_STRING("EMPTY", text("dagg.objnam 0"));
}

// GENVAL: a new shield, sword or torch wears the LEATHER, WOODEN or PINE
// numbers until it is revealed, keeping only its own reveal requirement.
// A Mithril shield really does fight like a leather one until you know
// what it is.
void test_an_unrevealed_shield_wears_the_leather_shields_numbers(void)
{
    start_game();
    run("make \"dagg.ocbptr 0  make \"i dagg.obirth 3 0"); // MITHRIL
    TEST_ASSERT_EQUAL_FLOAT(140, num("item :i :dagg.ocrev"));   // its own
    TEST_ASSERT_EQUAL_FLOAT(0, num("item :i :dagg.ocmgo"));     // LEATHER's
    TEST_ASSERT_EQUAL_FLOAT(10, num("item :i :dagg.ocpho"));    // LEATHER's
    TEST_ASSERT_EQUAL_FLOAT(108, num("item :i :dagg.ocx0"));    // LEATHER's filters
    TEST_ASSERT_EQUAL_FLOAT(128, num("item :i :dagg.ocx1"));
    // ...and a flask keeps its own from birth (GENVAL is -1 for a flask):
    // you cannot see what it is, but drinking it does what it does.
    run("make \"j dagg.obirth 5 0"); // THEWS
    TEST_ASSERT_EQUAL_FLOAT(70, num("item :j :dagg.ocrev"));
    TEST_ASSERT_EQUAL_STRING("FLASK", text("dagg.objnam :j"));
}

void test_reveal_needs_twenty_five_power_a_point_and_gives_the_numbers_back(void)
{
    start_game();
    run("make \"dagg.ocbptr 0  make \"i dagg.obirth 3 0"); // MITHRIL, reveal 140
    run("make \"dagg.plhand :i");

    run("make \"dagg.ppow 3499  make \"dagg.tokens dagg.split [L E F T]");
    run("make \"dagg.linptr 1  dagg.reveal");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(140, num("item :i :dagg.ocrev"),
                                    "revealed one power short of 25 x 140");

    run("make \"dagg.ppow 3500  make \"dagg.tokens dagg.split [L E F T]");
    run("make \"dagg.linptr 1  dagg.reveal");
    TEST_ASSERT_EQUAL_FLOAT(0, num("item :i :dagg.ocrev"));
    TEST_ASSERT_EQUAL_STRING("MITHRIL SHIELD", text("dagg.objnam :i"));
    TEST_ASSERT_EQUAL_FLOAT(13, num("item :i :dagg.ocmgo"));
    TEST_ASSERT_EQUAL_FLOAT(26, num("item :i :dagg.ocpho"));
    TEST_ASSERT_EQUAL_FLOAT(64, num("item :i :dagg.ocx0"));
    TEST_ASSERT_EQUAL_FLOAT(64, num("item :i :dagg.ocx1"));
}

// The M3 gate, first half: "the section 10.2 table round-trips -- every
// object can be found, revealed, named and used."  Every one of the
// twenty-five types, born, revealed and named.
void test_every_object_can_be_born_revealed_and_named(void)
{
    start_game();
    for (int typ = 0; typ < 25; typ++)
    {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "make \"dagg.ocbptr 0  make \"i dagg.obirth %d 0"
                 "  make \"dagg.plhand :i  make \"dagg.ppow 30000"
                 "  make \"dagg.tokens dagg.split [L E F T]  make \"dagg.linptr 1"
                 "  dagg.reveal",
                 typ);
        run(cmd);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("item :i :dagg.ocrev"),
                                        "an object could not be revealed");
        snprintf(cmd, sizeof(cmd), "first item %d :dagg.adjtab", typ + 1);
        const char *adjective = text(cmd);
        char want[64];
        snprintf(cmd, sizeof(cmd), "first item (1 + item :i :dagg.occls) :dagg.gentab");
        snprintf(want, sizeof(want), "%s %s", adjective, text(cmd));
        TEST_ASSERT_EQUAL_STRING(want, text("dagg.objnam :i"));
        // and the name fits a hand: the longest is fourteen characters
        TEST_ASSERT_TRUE(strlen(want) <= 14);
    }
}

//==========================================================================
// GET, DROP, STOW, PULL -- PGET.ASM, and the floor OFIND reads.
//==========================================================================

// The command line is the only way in, exactly as it is for MOVE and TURN.
void test_get_and_drop_move_the_weight_and_the_floor(void)
{
    start_game();
    type_line("PULL RIGHT TORCH");
    TEST_ASSERT_EQUAL_STRING("PINE TORCH", text("dagg.objnam :dagg.prhand"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(35, num(":dagg.objwt"),
                                    "PULL changed the weight, and it is still carried");

    type_line("DROP RIGHT");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.prhand"));
    TEST_ASSERT_EQUAL_FLOAT(25, num(":dagg.objwt"));
    TEST_ASSERT_EQUAL_FLOAT(1, num("count :dagg.floor"));
    TEST_ASSERT_EQUAL_FLOAT(5, num("item (item 1 :dagg.floor) :dagg.ocrow"));
    TEST_ASSERT_EQUAL_FLOAT(5, num("item (item 1 :dagg.floor) :dagg.occol"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("item (item 1 :dagg.floor) :dagg.ocown"));

    type_line("GET RIGHT TORCH");
    TEST_ASSERT_EQUAL_STRING("PINE TORCH", text("dagg.objnam :dagg.prhand"));
    TEST_ASSERT_EQUAL_FLOAT(35, num(":dagg.objwt"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("count :dagg.floor"));
}

// OFIND only answers for the cell you are standing on, on this level.
void test_a_dropped_object_stays_where_it_was_dropped(void)
{
    start_game();
    type_line("PULL RIGHT TORCH");
    type_line("DROP RIGHT");
    run("make \"dagg.col 6");
    type_line("GET RIGHT TORCH");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":dagg.prhand"),
                                    "picked up an object from the next cell along");
    run("make \"dagg.col 5  make \"dagg.level 1");
    type_line("GET RIGHT TORCH");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":dagg.prhand"),
                                    "picked up an object from another level");
}

// VIEW52: an object on the floor is drawn twice, once under each light
// channel, and its outline comes from its class.
void test_an_object_on_the_floor_is_drawn_in_the_view(void)
{
    start_game();
    type_line("PULL RIGHT TORCH");
    type_line("DROP RIGHT");
    run("make \"dagg.light 8  make \"dagg.mlight 8");

    mock_device_clear_graphics();
    run("dagg.redraw :dagg.norscl");
    int with_object = mock_device_line_count();

    run("make \"dagg.floor []");
    mock_device_clear_graphics();
    run("dagg.redraw :dagg.norscl");
    TEST_ASSERT_TRUE_MESSAGE(with_object > mock_device_line_count(),
                             "the cell walk never draws the objects on the floor");
    // FTORCH is one run of four points -- three strokes -- and VIEW52 draws
    // it twice, once with MAGFLG set and once without
    TEST_ASSERT_EQUAL_FLOAT(6, with_object - mock_device_line_count());
}

// The sword is the one object list with a PEN LIFT in it -- VOBJ.ASM's
// FSWORD is a blade and then a `V$NEW` and then a hand guard -- so it is
// the only one whose stroke count says the generator's V$NEW decode came
// out right rather than merely plausible.  A board saw this one: `D R`
// put a sword on the ground and it read as a sword.
void test_a_dropped_sword_draws_its_two_runs_and_stops_when_picked_up(void)
{
    start_game();
    type_line("PULL RIGHT SWORD");
    type_line("DROP RIGHT");
    run("make \"dagg.light 8  make \"dagg.mlight 8");

    mock_device_clear_graphics();
    run("dagg.redraw :dagg.norscl");
    int with_sword = mock_device_line_count();

    // GET takes it off the floor, and the next redraw stops drawing it --
    // which is dagg.floor.del and OFIND's own filter, both at once.
    type_line("GET RIGHT SWORD");
    TEST_ASSERT_EQUAL_FLOAT(0, num("count :dagg.floor"));
    // GET redraws through dagg.pupdat, which recomputes the light from the
    // torch -- and there is none burning here, so put the fixture's light
    // back before measuring again.
    run("make \"dagg.light 8  make \"dagg.mlight 8");
    mock_device_clear_graphics();
    run("dagg.redraw :dagg.norscl");
    int without = mock_device_line_count();

    // Two runs of two points is two strokes, and VIEW52 draws the list
    // twice -- so four, where the single-run torch above is six.
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, with_sword - without,
                                    "FSWORD's pen lift did not survive the generator");
}

void test_stow_and_pull_leave_the_weight_alone(void)
{
    start_game();
    type_line("PULL LEFT SWORD");
    float carried = num(":dagg.objwt");
    type_line("STOW LEFT");
    TEST_ASSERT_EQUAL_FLOAT(carried, num(":dagg.objwt"));
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.plhand"));
    // PSTOW0 pushes at the head, so it is back at the front of the bag
    TEST_ASSERT_EQUAL_STRING("WOODEN SWORD", text("dagg.objnam item 1 :dagg.bag"));
}

void test_a_full_hand_refuses_and_an_empty_one_has_nothing_to_give(void)
{
    start_game();
    type_line("PULL RIGHT TORCH");
    mock_device_clear_output();
    type_line("PULL RIGHT SWORD");
    TEST_ASSERT_TRUE_MESSAGE(strstr(mock_device_get_output(), "??") != NULL,
                             "pulled into a hand that was already full");
    TEST_ASSERT_EQUAL_STRING("PINE TORCH", text("dagg.objnam :dagg.prhand"));

    mock_device_clear_output();
    type_line("DROP LEFT");
    TEST_ASSERT_TRUE_MESSAGE(strstr(mock_device_get_output(), "??") != NULL,
                             "dropped something out of an empty hand");
}

//==========================================================================
// PAROBJ -- PARSER.ASM.  <generic>, or <adjective> <generic> with the two
// agreeing on class.
//==========================================================================

// The line buffer is a list of one-character words, so a space in it is a
// `char 32` element and not a gap in a list literal -- which is what
// dagg.split is splitting on.  Built the way dagg.human builds it.
static bool parses(const char *tokens)
{
    char cmd[96];
    run("make \"dagg.linbuf []");
    for (const char *p = tokens; *p; p++)
    {
        snprintf(cmd, sizeof(cmd),
                 "make \"dagg.linbuf lput dagg.key char %d :dagg.linbuf",
                 (int)(unsigned char)*p);
        run(cmd);
    }
    run("make \"dagg.tokens dagg.split :dagg.linbuf  make \"dagg.linptr 1");
    return strcmp(text("dagg.parobj"), "true") == 0;
}

void test_parobj_takes_a_generic_or_an_agreeing_adjective(void)
{
    start_game();
    TEST_ASSERT_TRUE_MESSAGE(parses("TORCH"), "a bare generic");
    TEST_ASSERT_EQUAL_STRING("false", text(":dagg.speflg"));
    TEST_ASSERT_EQUAL_FLOAT(5, num(":dagg.objcls"));

    TEST_ASSERT_TRUE_MESSAGE(parses("PINE TORCH"), "an adjective and its generic");
    TEST_ASSERT_EQUAL_STRING("true", text(":dagg.speflg"));
    TEST_ASSERT_EQUAL_FLOAT(15, num(":dagg.objtyp"));

    // POBJ10's class comparison: the adjective and the generic have to be
    // the same kind of thing, which is why ADJTAB carries a class.
    TEST_ASSERT_FALSE_MESSAGE(parses("PINE SWORD"), "PINE SWORD parsed");
    // A bare adjective fails because PARSER then finds no generic at all
    TEST_ASSERT_FALSE_MESSAGE(parses("PINE"), "a bare adjective parsed");
    TEST_ASSERT_FALSE_MESSAGE(parses(""), "a missing object parsed");
    // Prefixes work here as everywhere else
    TEST_ASSERT_TRUE_MESSAGE(parses("TO"), "a prefix of a generic");
    TEST_ASSERT_TRUE_MESSAGE(parses("MI SH"), "prefixes of both");
}

// A generic name takes the first match in OCB order, which for the bag is
// the order PSTOW0 built it in.
void test_a_generic_name_takes_the_first_of_its_class(void)
{
    start_game();
    type_line("PULL LEFT TORCH");
    TEST_ASSERT_EQUAL_STRING("PINE TORCH", text("dagg.objnam :dagg.plhand"));
    type_line("STOW LEFT");
    // A Solar torch on top of it: a generic PULL now takes the Solar one
    run("make \"dagg.ocbptr 0  make \"i dagg.obirth 10 0  .setitem :i :dagg.ocown 1"
        "  make \"dagg.bag fput :i :dagg.bag  .setitem :i :dagg.ocrev 0");
    type_line("PULL LEFT TORCH");
    TEST_ASSERT_EQUAL_STRING("SOLAR TORCH", text("dagg.objnam :dagg.plhand"));
    type_line("STOW LEFT");
    // ...and a specific one reaches past it
    type_line("PULL LEFT PINE TORCH");
    TEST_ASSERT_EQUAL_STRING("PINE TORCH", text("dagg.objnam :dagg.plhand"));
}

//==========================================================================
// Light and torches -- design section 8.3, PUPDAT.ASM:PSUB10,
// COMPLR.ASM:BURNER.
//==========================================================================

// PRLITE is zero and COMDAT.ASM never sets it, so the dungeon is black
// until something is burning.  This is why the ROM's own attract mode
// opens PULL RIGHT TORCH / USE RIGHT (TOKEN.ASM:AUTTAB).
void test_you_start_in_the_dark_and_a_torch_is_the_only_light(void)
{
    start_game();
    mock_device_clear_graphics();
    run("dagg.pupdat :dagg.norscl");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.light"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, mock_device_line_count(),
                                    "the dungeon was lit with no torch burning");

    type_line("PULL RIGHT TORCH");
    type_line("USE RIGHT");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(7, num(":dagg.light"), "a pine torch is 7 regular");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":dagg.mlight"), "a pine torch is 0 magic");
    // PUSE12 stows the torch it lights, so your hand is free again and
    // the torch is back in the bag, burning
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.prhand"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("count :dagg.floor"));
    TEST_ASSERT_EQUAL_FLOAT(2, num("count :dagg.bag"));

    mock_device_clear_graphics();
    run("dagg.pupdat :dagg.norscl");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > 0, "a lit torch shows nothing");
}

// PPULL clears PTORCH when it takes the burning torch back into a hand,
// which is the only way to put a torch out.  PDROP does not need to,
// because USE stows it and PULL is the only way back out of the bag.
void test_pulling_the_burning_torch_puts_it_out(void)
{
    start_game();
    type_line("PULL RIGHT TORCH");
    type_line("USE RIGHT");
    TEST_ASSERT_TRUE(num(":dagg.ptorch") > 0);
    type_line("PULL RIGHT TORCH");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.ptorch"));
    run("dagg.pupdat :dagg.norscl");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.light"));
}

// The M3 gate, second half.  BURNER's `CMPA #5 / BGT` marks a torch dead
// at FIVE minutes to go, not at zero -- so a fifteen-minute Pine torch is
// called DEAD after ten -- and each light value is clamped down to the
// timer as it falls, so it dims before it dies and goes on dimming for
// the five minutes after.
void test_a_pine_torch_dies_at_five_minutes(void)
{
    start_game();
    type_line("PULL RIGHT TORCH");
    type_line("USE RIGHT");
    run("make \"i :dagg.ptorch");
    TEST_ASSERT_EQUAL_FLOAT(15, num("item :i :dagg.ocx0"));

    // Minutes 1..15.  The light holds at 7 while the timer is above it and
    // then tracks the timer down, so the last two minutes of a live Pine
    // torch are already dimmer than the first eight -- and it goes on
    // dimming for the five minutes it spends called DEAD, because BURNER's
    // only stopping condition is a timer of zero.
    const int light[] = {7, 7, 7, 7, 7, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0};
    for (int minute = 1; minute <= 15; minute++)
    {
        char why[64];
        run("make \"dagg.now :dagg.burner.due  dagg.burner");
        snprintf(why, sizeof(why), "minute %d", minute);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(15 - minute, num("item :i :dagg.ocx0"), why);
        run("dagg.setlight");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(light[minute - 1], num(":dagg.light"), why);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(minute < 10 ? "PINE TORCH" : "DEAD TORCH",
                                         text("dagg.objnam :i"), why);
    }
    // BURN99's `TST P.OCXXX / BEQ` -- a burnt-out torch is left alone
    run("make \"dagg.now :dagg.burner.due  dagg.burner");
    TEST_ASSERT_EQUAL_FLOAT(0, num("item :i :dagg.ocx0"));
}

// Is the arithmetic right?  COMPLR.ASM:BURNER transcribed into C, the way
// test_the_heart_rate_tracks_hupdats_own_division transcribes HUPD20 -- so
// the Logo and the ROM have to agree by arithmetic and not because the same
// person wrote both of them the same way.  Every torch, every minute of its
// life, all three of its bytes and its name.
struct torch_state
{
    int timer, regular, magic;
    bool dead;
};

static void burner_step(struct torch_state *s)
{
    if (s->timer == 0) // BURN99: `LDA P.OCXXX,U / BEQ` -- a burnt-out
        return;        // torch is left alone
    s->timer--;
    if (!(s->timer > 5)) // `CMPA #5 / BGT BURN10`
        s->dead = true;
    if (s->timer < s->regular) // `CMPA P.OCXXX+1,U / BGE BURN20`
        s->regular = s->timer;
    if (s->timer < s->magic) // `CMPA P.OCXXX+2,U / BGE BURN99`
        s->magic = s->timer;
}

void test_every_torch_burns_the_way_burner_says_it_does(void)
{
    // SOLAR, LUNAR, PINE and the DEAD torch: type, and XXXTAB's three bytes
    static const struct
    {
        int type;
        const char *name;
        struct torch_state start;
    } torches[] = {
        {10, "SOLAR", {60, 13, 11, false}},
        {14, "LUNAR", {30, 10, 4, false}},
        {15, "PINE", {15, 7, 0, false}},
        {24, "DEAD", {0, 0, 0, true}},
    };

    for (size_t k = 0; k < sizeof torches / sizeof torches[0]; k++)
    {
        char cmd[192], why[96];
        start_game();
        // Born and then REVEALED -- PREV00's own two steps, the OCBFIL and
        // then the clear.  Marking it revealed without the re-fill would
        // leave it wearing a pine torch's XXXTAB (see the next test).
        snprintf(cmd, sizeof(cmd),
                 "make \"dagg.ocbptr 0  make \"i dagg.obirth %d 0"
                 "  dagg.ocbfil :i (item :i :dagg.octyp)"
                 "  .setitem :i :dagg.ocrev 0  make \"dagg.ptorch :i",
                 torches[k].type);
        run(cmd);
        struct torch_state want = torches[k].start;
        // XXXTAB reached the OCB intact in the first place
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(want.timer, num("item :i :dagg.ocx0"),
                                        torches[k].name);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(want.regular, num("item :i :dagg.ocx1"),
                                        torches[k].name);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(want.magic, num("item :i :dagg.ocx2"),
                                        torches[k].name);

        // One minute past the end of the longest torch there is
        for (int minute = 1; minute <= 61; minute++)
        {
            burner_step(&want);
            run("make \"dagg.now :dagg.burner.due  dagg.burner");
            snprintf(why, sizeof(why), "%s torch, minute %d", torches[k].name, minute);
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(want.timer, num("item :i :dagg.ocx0"), why);
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(want.regular, num("item :i :dagg.ocx1"), why);
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(want.magic, num("item :i :dagg.ocx2"), why);
            char name[32];
            snprintf(name, sizeof(name), "%s TORCH",
                     want.dead ? "DEAD" : torches[k].name);
            TEST_ASSERT_EQUAL_STRING_MESSAGE(name, text("dagg.objnam :i"), why);
            // and the light the viewer actually gets is the sum PSUB10 makes
            run("dagg.setlight");
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(want.regular, num(":dagg.light"), why);
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(want.magic, num(":dagg.mlight"), why);
        }
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, want.timer, torches[k].name);
    }
}

// OBIRTH.ASM:GENVAL puts every new torch in a PINE torch's clothes, and
// for a torch that is not only its light but its LIFETIME: an unrevealed
// Solar torch burns fifteen minutes at 7/0, not sixty at 13/11, and there
// is no way to tell it from the real thing while it does.  PREVEA's
// OCBFIL is what gives the numbers back -- including a full timer, so
// revealing a torch you have already been burning refills it.
void test_an_unrevealed_torch_burns_as_a_pine_torch_until_you_reveal_it(void)
{
    start_game();
    run("make \"dagg.ocbptr 0  make \"i dagg.obirth 10 0"); // SOLAR
    TEST_ASSERT_EQUAL_STRING("TORCH", text("dagg.objnam :i"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(15, num("item :i :dagg.ocx0"), "PINE's timer");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(7, num("item :i :dagg.ocx1"), "PINE's regular light");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num("item :i :dagg.ocx2"), "PINE's magic light");
    // ...and its own reveal requirement is what OBIRTH kept
    TEST_ASSERT_EQUAL_FLOAT(70, num("item :i :dagg.ocrev"));

    // Burn it half way down, then reveal it: PREV00's OCBFIL re-fills all
    // three bytes from SOLAR, so the timer comes back full.
    run("make \"dagg.ptorch :i");
    for (int minute = 1; minute <= 7; minute++)
        run("make \"dagg.now :dagg.burner.due  dagg.burner");
    TEST_ASSERT_EQUAL_FLOAT(8, num("item :i :dagg.ocx0"));

    run("make \"dagg.plhand :i  make \"dagg.ppow 30000");
    run("make \"dagg.tokens dagg.split [L E F T]  make \"dagg.linptr 1  dagg.reveal");
    TEST_ASSERT_EQUAL_STRING("SOLAR TORCH", text("dagg.objnam :i"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(60, num("item :i :dagg.ocx0"),
                                    "REVEAL did not refill the torch");
    TEST_ASSERT_EQUAL_FLOAT(13, num("item :i :dagg.ocx1"));
    TEST_ASSERT_EQUAL_FLOAT(11, num("item :i :dagg.ocx2"));
}

// A torch is called DEAD five minutes before it stops giving light, so the
// three phases do not line up: a Lunar torch still has its full magic 4 for
// two minutes AFTER it is named dead, and PATT22 is already throwing away
// three hits in four by then (`CMPA #T.TOR5`, which reads the NAME and not
// the light).  That gap is the ROM's, not a rounding of it.
void test_a_dead_torch_still_gives_light_and_still_fights_as_darkness(void)
{
    start_game();
    run("make \"dagg.ocbptr 0  make \"i dagg.obirth 14 0"  // LUNAR: 30, 10, 4
        "  dagg.ocbfil :i (item :i :dagg.octyp)"
        "  .setitem :i :dagg.ocrev 0  make \"dagg.ptorch :i");
    for (int minute = 1; minute <= 25; minute++)
        run("make \"dagg.now :dagg.burner.due  dagg.burner");
    // Minute 25: the timer is 5, so it is named dead...
    TEST_ASSERT_EQUAL_FLOAT(5, num("item :i :dagg.ocx0"));
    TEST_ASSERT_EQUAL_STRING("DEAD TORCH", text("dagg.objnam :i"));
    // ...and it is still lighting the dungeon, magic channel at full
    run("dagg.setlight");
    TEST_ASSERT_EQUAL_FLOAT(5, num(":dagg.light"));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4, num(":dagg.mlight"),
                                    "a dead Lunar torch lost its magic light early");
    // Five more minutes and there is nothing left
    for (int minute = 26; minute <= 30; minute++)
        run("make \"dagg.now :dagg.burner.due  dagg.burner");
    run("dagg.setlight");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.light"));
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.mlight"));
}

// SOLAR is the one torch with a magic channel, and design section 8.3's
// "a magical torch shows you secret doors that regular light will not" is
// VIEW22 drawing the secret-door list under MLIGHT and the wall under
// RLIGHT.  Two lists, two numbers, one range.
void test_the_magic_channel_lights_a_secret_door_the_regular_one_does_not(void)
{
    build_synthetic_corridor();
    set_cell(4, 5, 2 + (2 << 6)); // (4,5): N and W secret doors, E/S passage
    run("make \"dagg.row 4  make \"dagg.col 5  make \"dagg.dir 0");

    // Regular light reaches range 0 and magic light does not: the wall
    // draws, the secret door does not.
    run("make \"dagg.light 7  make \"dagg.mlight 0");
    mock_device_clear_graphics();
    run("dagg.redraw :dagg.norscl");
    int regular_only = mock_device_line_count();

    run("make \"dagg.light 7  make \"dagg.mlight 7");
    mock_device_clear_graphics();
    run("dagg.redraw :dagg.norscl");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > regular_only,
                             "magic light showed nothing the regular light did not");
}

//==========================================================================
// USE -- PUSE.ASM.  Every failure past the hand parse is silent.
//==========================================================================

static void put_in_left_hand(int type)
{
    char cmd[160];
    snprintf(cmd, sizeof(cmd),
             "make \"dagg.ocbptr 0  make \"i dagg.obirth %d 0"
             "  .setitem :i :dagg.ocown 1"
             "  .setitem :i :dagg.ocrev 0  make \"dagg.plhand :i",
             type);
    run(cmd);
}

void test_the_three_flasks_do_what_section_10_2_says(void)
{
    start_game();
    put_in_left_hand(5); // THEWS
    type_line("USE LEFT");
    TEST_ASSERT_EQUAL_FLOAT(1160, num(":dagg.ppow"));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("EMPTY FLASK", text("dagg.objnam :dagg.plhand"),
                                     "a used flask keeps its class and loses its name");

    start_game();
    run("make \"dagg.pdam 100");
    put_in_left_hand(9); // HALE
    type_line("USE LEFT");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.pdam"));

    start_game();
    put_in_left_hand(8); // ABYE
    type_line("USE LEFT");
    // SCAL16 is radix-7, so 102/128 of 160 and not 102/256
    TEST_ASSERT_EQUAL_FLOAT(127, num(":dagg.pdam"));
}

// An emptied flask is always revealed (`CLR P.OCREV,U`), which is the game
// telling you what you just drank after it is too late.
void test_an_emptied_flask_is_always_revealed(void)
{
    start_game();
    run("make \"dagg.ocbptr 0  make \"i dagg.obirth 5 0"
        "  .setitem :i :dagg.ocown 1  make \"dagg.plhand :i");
    TEST_ASSERT_EQUAL_STRING("FLASK", text("dagg.objnam :i"));
    type_line("USE LEFT");
    TEST_ASSERT_EQUAL_STRING("EMPTY FLASK", text("dagg.objnam :i"));
}

//==========================================================================
// The map and the two scrolls -- PUSE.ASM:USC100/USC200, MAPPER.ASM,
// design section 13.
//==========================================================================

void test_an_unrevealed_scroll_does_nothing(void)
{
    start_game();
    run("make \"dagg.ocbptr 0  make \"i dagg.obirth 7 0"
        "  .setitem :i :dagg.ocown 1  make \"dagg.plhand :i");
    TEST_ASSERT_TRUE(num("item :i :dagg.ocrev") > 0);
    type_line("USE LEFT");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0, num(":dagg.dspmod"),
                                    "an unrevealed scroll put the map up");
}

void test_a_revealed_scroll_puts_the_map_up_and_stops_the_heart_being_drawn(void)
{
    start_game();
    put_in_left_hand(4); // SEER
    mock_device_clear_graphics();
    type_line("USE LEFT");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":dagg.dspmod"));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", text(":dagg.mapflg"),
                                     "a seer scroll shows walls only");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", text(":dagg.heartf"),
                                     "the heart is still being drawn under the map");
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > 0, "the map drew nothing");
    // MAPP50's "X marks the spot", at the centre of the player's own cell:
    // (5,5) is x = -160 + 50 + 5 and y = 160 - 35 - 3.
    TEST_ASSERT_TRUE_MESSAGE(
        mock_device_has_line_from_to(-108.0f, 120.0f, -102.0f, 124.0f, 0.5f),
        "the map does not mark where you are standing");
    // The map covers the whole band, so there is no status line on it
    const MockDeviceState *state = mock_device_get_state();
    int labels = state->label.count;
    run("dagg.pupdat :dagg.norscl");
    TEST_ASSERT_EQUAL_INT_MESSAGE(labels, mock_device_get_state()->label.count,
                                  "the map wrote a status line over itself");

    // ...and a Vision scroll shows the walls without what is on them
    start_game();
    put_in_left_hand(7); // VISION
    type_line("USE LEFT");
    TEST_ASSERT_EQUAL_STRING("false", text(":dagg.mapflg"));
}

// One stroke per RUN of rock, not one a cell -- design section 13's own
// choice, and the reason dagg.map.row counts with a variable instead of
// `repcount`: `repcount` answers the innermost REPEAT, and inside this
// procedure's `foreach` that is dagg.mapper's own row loop, so every run
// would start and end in the same place.
void test_the_map_draws_one_stroke_a_run_of_rock(void)
{
    build_zero_maze(); // every cell open: no rock, no strokes
    run("make \"dagg.level 0  make \"dagg.row 0  make \"dagg.col 0");
    run("dagg.enter.level 0");
    set_cell(3, 4, 255);
    set_cell(3, 5, 255);
    set_cell(3, 6, 255);
    set_cell(3, 20, 255);

    mock_device_clear_graphics();
    run("setpensize 7  dagg.map.row 3  setpensize 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, mock_device_line_count(),
                                  "a run of three cells is not one stroke");
    // A cell is 10 x 7: columns 4-6 span x -120..-90, and row 3's centre
    // is y = 160 - 21 - 3.
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(-120.0f, 136.0f, -90.0f, 136.0f, 0.5f));
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(40.0f, 136.0f, 50.0f, 136.0f, 0.5f));

    // ...and a row that ends in rock closes its last run at column 31
    mock_device_clear_graphics();
    set_cell(3, 31, 255);
    run("setpensize 7  dagg.map.row 3  setpensize 1");
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(150.0f, 136.0f, 160.0f, 136.0f, 0.5f));
}

// HMAN10: the map comes down on the first keystroke after it went up,
// before that keystroke is even buffered.
void test_the_map_comes_down_on_the_next_keystroke(void)
{
    start_game();
    put_in_left_hand(4);
    type_line("USE LEFT");
    TEST_ASSERT_EQUAL_FLOAT(1, num(":dagg.dspmod"));
    run("dagg.human dagg.key char 76"); // "L"
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.dspmod"));
    TEST_ASSERT_EQUAL_STRING("true", text(":dagg.heartf"));
    // and the L still got buffered
    TEST_ASSERT_EQUAL_FLOAT(1, num("count :dagg.linbuf"));
}

//==========================================================================
// INCANT -- PINCAN.ASM.  The one place in the parser that wants the whole
// word, and the only command that reads both hands.
//==========================================================================

void test_incant_needs_the_whole_word_and_the_right_one(void)
{
    start_game();
    put_in_left_hand(12); // VULCAN -> FIRE
    type_line("INCANT FIR");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("VULCAN RING", text("dagg.objnam :dagg.plhand"),
                                     "a prefix was enough for INCANT");
    type_line("INCANT ICE");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("VULCAN RING", text("dagg.objnam :dagg.plhand"),
                                     "the wrong word transformed the ring");
    type_line("INCANT FIRE");
    TEST_ASSERT_EQUAL_STRING("FIRE RING", text("dagg.objnam :dagg.plhand"));
}

// The transformed ring keeps its three charges (FIRE has no XXXTAB row, so
// OCBFIL leaves P.OCXXX+0 alone) and loses the word that made it, so it
// cannot be done twice.
void test_an_incanted_ring_keeps_its_charges_and_cannot_be_done_twice(void)
{
    start_game();
    put_in_left_hand(12);
    TEST_ASSERT_EQUAL_FLOAT(3, num("item :dagg.plhand :dagg.ocx0"));
    type_line("INCANT FIRE");
    TEST_ASSERT_EQUAL_FLOAT(3, num("item :dagg.plhand :dagg.ocx0"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("item :dagg.plhand :dagg.ocx1"));
    TEST_ASSERT_EQUAL_FLOAT(255, num("item :dagg.plhand :dagg.ocmgo"));
    TEST_ASSERT_EQUAL_FLOAT(255, num("item :dagg.plhand :dagg.ocpho"));
    type_line("INCANT FIRE");
    TEST_ASSERT_EQUAL_STRING("FIRE RING", text("dagg.objnam :dagg.plhand"));
}

void test_incant_reads_both_hands(void)
{
    start_game();
    run("make \"dagg.ocbptr 0  make \"i dagg.obirth 6 0  .setitem :i :dagg.ocown 1"  // RIME -> ICE
        "  .setitem :i :dagg.ocrev 0  make \"dagg.prhand :i");
    type_line("INCANT ICE");
    TEST_ASSERT_EQUAL_STRING("ICE RING", text("dagg.objnam :dagg.prhand"));
}

//==========================================================================
// EXAMINE and LOOK -- PEXAM.ASM, PLOOK.ASM.  DSPMOD is sticky: the listing
// replaces the view and stays until LOOK, which is why LOOK is a command.
//==========================================================================

void test_examine_lists_the_floor_and_the_bag(void)
{
    start_game();
    type_line("PULL RIGHT TORCH");
    type_line("DROP RIGHT");

    int before = mock_device_get_state()->label.count;
    type_line("EXAMINE");
    TEST_ASSERT_EQUAL_FLOAT(2, num(":dagg.dspmod"));
    const MockDeviceState *state = mock_device_get_state();
    // IN THIS ROOM, PINE TORCH, the bar, BACKPACK, WOODEN SWORD, and the
    // status line last
    TEST_ASSERT_EQUAL_INT(6, state->label.count - before);
    TEST_ASSERT_EQUAL_UINT(40, strlen(state->label.last_text));

    // It stays up: another command redraws the listing, not the view
    mock_device_clear_graphics();
    type_line("MOVE");
    TEST_ASSERT_EQUAL_FLOAT(2, num(":dagg.dspmod"));

    type_line("LOOK");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.dspmod"));
}

// The 19 rows carry over from the ROM unchanged; the columns do not.
// EXAMIN's numbers are all fractions of its screen width -- both headers
// centred, the rule the full width, the second entry half way across --
// so they are recomputed for 40 rather than left at 32 with eight columns
// of nothing down the right-hand side.
void test_the_examine_screen_is_laid_out_for_forty_columns(void)
{
    start_game();
    const MockDeviceState *state = mock_device_get_state();

    // The rule is the full width of the screen, starting at column 0
    TEST_ASSERT_EQUAL_UINT(40, strlen(text("dagg.exbar")));
    run("dagg.write.at 4 0 dagg.exbar");
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -160.0f, state->label.last_x);

    // Both headers centred: (40 - 12) / 2 and (40 - 8) / 2
    run("make \"dagg.exrow 0  make \"dagg.excol 0  dagg.examin");
    run("dagg.write.at 0 14 [IN THIS ROOM]");
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -160.0f + 8 * 14, state->label.last_x);
    run("dagg.write.at 0 16 [BACKPACK]");
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -160.0f + 8 * 16, state->label.last_x);

    // Two entries to a line, the second half way across -- and the longest
    // name in the game still ends inside the screen from there.
    run("make \"dagg.exrow 0  make \"dagg.excol 0");
    run("make \"dagg.ocbptr 0  make \"i dagg.obirth 3 0  .setitem :i :dagg.ocrev 0");
    run("dagg.prtobj :i \"false");
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -160.0f, state->label.last_x);
    TEST_ASSERT_EQUAL_FLOAT(20, num(":dagg.excol"));
    run("dagg.prtobj :i \"false");
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, state->label.last_x);
    TEST_ASSERT_EQUAL_STRING("MITHRIL SHIELD", state->label.last_text);
    TEST_ASSERT_TRUE(20 + (int)strlen(state->label.last_text) <= 40);
    // ...and the pair wraps to the next row rather than to a third column
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.excol"));
    TEST_ASSERT_EQUAL_FLOAT(1, num(":dagg.exrow"));
}

// EXAM30 highlights the burning torch in inverse video (`COM P.TXINV,U`),
// which is the same three-argument `write` the status bar uses.
void test_examine_highlights_the_burning_torch(void)
{
    start_game();
    type_line("PULL RIGHT TORCH");
    type_line("USE RIGHT");
    run("make \"dagg.dspmod 2");
    run("dagg.examin");
    // Nothing on the floor, one torch and one sword in the bag: the last
    // thing written is the status line, so count the writes instead.
    TEST_ASSERT_TRUE(mock_device_get_state()->label.count > 0);
    // The torch is the head of the bag (USE stowed it), drawn opaque
    run("make \"dagg.bag (list :dagg.ptorch)  make \"dagg.exrow 0  make \"dagg.excol 0");
    run("dagg.prtobj :dagg.ptorch (:dagg.ptorch = :dagg.ptorch)");
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_STRING("PINE TORCH", state->label.last_text);
    TEST_ASSERT_EQUAL_FLOAT(num(":dagg.bg"), (float)state->label.last_colour);
    TEST_ASSERT_EQUAL_FLOAT(num(":dagg.ink"), (float)state->label.last_background);
}

// The exact sequence a board reported: the torch LEAVES the backpack
// listing when you PULL it and comes back in reverse video when you USE
// it.  Three separate pieces of PGET.ASM/PUSE.ASM agreeing --
// PPULL unlinking it from the bag and clearing PTORCH, PUSE12's PSTOW0
// pushing it back at the HEAD, and EXAM32's `CMPX PTORCH / COM P.TXINV,U`
// picking out that one row.
void test_the_torch_leaves_the_listing_when_pulled_and_returns_lit(void)
{
    start_game();
    run("make \"dagg.dspmod 2");

    // In the bag to start with, and nothing is burning, so the row is
    // transparent.  (The LAST write of a listing is always the status
    // line, which is opaque on every level -- so a row has to be measured
    // as a row.)
    TEST_ASSERT_EQUAL_FLOAT(2, num("count :dagg.bag"));
    run("make \"dagg.exrow 0  make \"dagg.excol 0");
    run("dagg.prtobj (item 2 :dagg.bag) ((item 2 :dagg.bag) = :dagg.ptorch)");
    TEST_ASSERT_EQUAL_STRING("PINE TORCH", mock_device_get_state()->label.last_text);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, mock_device_get_state()->label.last_background,
                                  "an unlit torch was drawn opaque");

    // PULL takes it out of the listing altogether
    type_line("PULL LEFT TORCH");
    TEST_ASSERT_EQUAL_FLOAT(1, num("count :dagg.bag"));
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.ptorch"));
    TEST_ASSERT_EQUAL_STRING("WOODEN SWORD", text("dagg.objnam item 1 :dagg.bag"));

    // USE puts it back -- at the head, because PSTOW0 pushes there -- and
    // lights it
    type_line("USE LEFT");
    TEST_ASSERT_EQUAL_FLOAT(2, num("count :dagg.bag"));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("PINE TORCH", text("dagg.objnam item 1 :dagg.bag"),
                                     "the used torch did not go back on top of the bag");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.plhand"));
    TEST_ASSERT_TRUE(num(":dagg.ptorch") > 0);

    // ...and it is the one row of the listing drawn in reverse video.  The
    // bag is walked head-first, so it is the FIRST backpack entry, and the
    // sword after it goes back to transparent.
    run("make \"dagg.exrow 0  make \"dagg.excol 0");
    run("dagg.prtobj :dagg.ptorch (:dagg.ptorch = :dagg.ptorch)");
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_STRING("PINE TORCH", state->label.last_text);
    TEST_ASSERT_EQUAL_FLOAT(num(":dagg.bg"), (float)state->label.last_colour);
    TEST_ASSERT_EQUAL_FLOAT(num(":dagg.ink"), (float)state->label.last_background);

    run("dagg.prtobj (item 2 :dagg.bag) \"false");
    TEST_ASSERT_EQUAL_STRING("WOODEN SWORD", state->label.last_text);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, state->label.last_background,
                                  "the highlight leaked onto the next entry");
}

//==========================================================================
// The status line, now that there is something to put in it -- STATUX.
//==========================================================================

void test_the_status_line_names_both_hands_and_still_measures_forty(void)
{
    start_game();
    TEST_ASSERT_EQUAL_STRING("EMPTY", text("dagg.objnam :dagg.plhand"));
    TEST_ASSERT_EQUAL_UINT(40, rendered_width("dagg.status.line"));

    // The two longest names in the game, one in each hand: 14 + 14 of 40
    run("make \"dagg.ocbptr 0");
    run("make \"i dagg.obirth 3 0  .setitem :i :dagg.ocrev 0  make \"dagg.plhand :i");
    run("make \"j dagg.obirth 16 0  .setitem :j :dagg.ocrev 0  make \"dagg.prhand :j");
    TEST_ASSERT_EQUAL_UINT(40, rendered_width("dagg.status.line"));
    mock_device_clear_output();
    run("type dagg.status.line");
    TEST_ASSERT_EQUAL_STRING("MITHRIL SHIELD            LEATHER SHIELD",
                             mock_device_get_output());
}

//==========================================================================
// The whole game, started and stopped.  Every other test drives a piece of
// `daggorath` and this is the only one that runs it -- which is where a
// typo in the init sequence would otherwise hide, because nothing else
// calls dagg.makeobjects, dagg.givebag and dagg.setup.heart in order
// against the real maze.  ESC is the one key that ends dagg.play.
//==========================================================================

void test_the_game_starts_and_stops(void)
{
    mock_device_set_input("\x1b");
    run("daggorath");
    TEST_ASSERT_EQUAL_STRING("true", text(":dagg.over"));
    // ONCE.ASM:GAME10's own start, and CINI40 + GAME30's 63 + 2 objects
    TEST_ASSERT_EQUAL_FLOAT(16, num(":dagg.row"));
    TEST_ASSERT_EQUAL_FLOAT(11, num(":dagg.col"));
    TEST_ASSERT_EQUAL_FLOAT(65, num(":dagg.ocbptr"));
    TEST_ASSERT_EQUAL_FLOAT(2, num("count :dagg.bag"));
    TEST_ASSERT_EQUAL_FLOAT(35, num(":dagg.objwt"));
    // and it starts in the dark, with nothing burning
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.ptorch"));
    TEST_ASSERT_EQUAL_FLOAT(0, num(":dagg.light"));
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

static float atoms_for_redraws(int count)
{
    char cmd[192];
    snprintf(cmd, sizeof(cmd),
             "make \"a0 atoms  repeat %d [dagg.redraw :dagg.norscl]  make \"a1 atoms",
             count);
    run(cmd);
    run(cmd);
    return num(":a0") - num(":a1");
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

// M3 put two more walks inside the redraw -- OFIND over the floor and
// SETFAD per vector list -- and neither may allocate either.  OFIND is a
// cursor rather than a list for exactly this reason.
void test_a_warm_redraw_over_an_object_spends_nothing_either(void)
{
    start_game();
    run("make \"dagg.light 8  make \"dagg.mlight 8");
    type_line("PULL RIGHT TORCH");
    type_line("DROP RIGHT");
    TEST_ASSERT_EQUAL_FLOAT(1, num("count :dagg.floor"));

    run("repeat 5 [dagg.redraw :dagg.norscl]");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(nodes_for_redraws(100), nodes_for_redraws(1000),
                                    "the object pass conses");
    // Measured at two lengths for the same reason the nodes are: the first
    // redraw over a given object mints the words for its own dash periods,
    // and that fixed cost would otherwise read as a leak.
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(atoms_for_redraws(100), atoms_for_redraws(1000),
                                    "the object pass interns a word every redraw");
}

//==========================================================================
// The global table -- section 14's second budget. It is 254 slots and the
// design does not expect it to bind (this game has no frame to buy, so
// nothing is in the flat namespace to make it faster), but M3 put twelve
// parallel OCB lists and thirty-odd names into it in one go and a count
// that is never taken is a count that surprises somebody later.
//==========================================================================

void test_the_game_fits_the_global_table(void)
{
    const int at_load = var_global_count(true);
    // Play far enough that every procedure which mints a name has run.
    start_game();
    type_line("PULL RIGHT TORCH");
    type_line("USE RIGHT");
    type_line("EXAMINE");
    type_line("LOOK");
    type_line("MOVE");
    type_line("TURN LEFT");
    put_in_left_hand(4); // a seer scroll: the map
    type_line("USE LEFT");
    run("dagg.human dagg.key char 76");
    run("dagg.burner  dagg.luknew  dagg.tick");

    const int peak = var_global_count(true);
    char msg[192];
    snprintf(msg, sizeof(msg),
             "daggorath peaks at %d globals of %d, leaving %d",
             peak, MAX_GLOBAL_VARIABLES, MAX_GLOBAL_VARIABLES - peak);
    TEST_ASSERT_TRUE_MESSAGE(peak <= MAX_GLOBAL_VARIABLES - 16, msg);
    TEST_ASSERT_TRUE_MESSAGE(peak >= at_load, "globals went down while playing");
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
    RUN_TEST(test_the_command_line_carries_its_own_cursor);
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
    RUN_TEST(test_the_object_tables_are_section_10_2);
    RUN_TEST(test_the_ring_the_macro_names_is_not_the_ring_the_player_types);
    RUN_TEST(test_the_special_objects_follow_the_eighteen);
    RUN_TEST(test_the_weights_and_outlines_are_by_class);
    RUN_TEST(test_every_object_in_the_dungeon_is_created_and_creature_owned);
    RUN_TEST(test_the_distribution_walks_down_and_wraps_past_level_five);
    RUN_TEST(test_the_player_starts_with_a_wooden_sword_and_a_pine_torch);
    RUN_TEST(test_gamdat_is_the_seam_a_board_uses_to_reach_a_ring);
    RUN_TEST(test_an_unrevealed_object_shows_only_its_generic_name);
    RUN_TEST(test_an_unrevealed_shield_wears_the_leather_shields_numbers);
    RUN_TEST(test_reveal_needs_twenty_five_power_a_point_and_gives_the_numbers_back);
    RUN_TEST(test_every_object_can_be_born_revealed_and_named);
    RUN_TEST(test_get_and_drop_move_the_weight_and_the_floor);
    RUN_TEST(test_a_dropped_object_stays_where_it_was_dropped);
    RUN_TEST(test_an_object_on_the_floor_is_drawn_in_the_view);
    RUN_TEST(test_a_dropped_sword_draws_its_two_runs_and_stops_when_picked_up);
    RUN_TEST(test_stow_and_pull_leave_the_weight_alone);
    RUN_TEST(test_a_full_hand_refuses_and_an_empty_one_has_nothing_to_give);
    RUN_TEST(test_parobj_takes_a_generic_or_an_agreeing_adjective);
    RUN_TEST(test_a_generic_name_takes_the_first_of_its_class);
    RUN_TEST(test_you_start_in_the_dark_and_a_torch_is_the_only_light);
    RUN_TEST(test_pulling_the_burning_torch_puts_it_out);
    RUN_TEST(test_a_pine_torch_dies_at_five_minutes);
    RUN_TEST(test_an_unrevealed_torch_burns_as_a_pine_torch_until_you_reveal_it);
    RUN_TEST(test_every_torch_burns_the_way_burner_says_it_does);
    RUN_TEST(test_a_dead_torch_still_gives_light_and_still_fights_as_darkness);
    RUN_TEST(test_the_magic_channel_lights_a_secret_door_the_regular_one_does_not);
    RUN_TEST(test_the_three_flasks_do_what_section_10_2_says);
    RUN_TEST(test_an_emptied_flask_is_always_revealed);
    RUN_TEST(test_an_unrevealed_scroll_does_nothing);
    RUN_TEST(test_a_revealed_scroll_puts_the_map_up_and_stops_the_heart_being_drawn);
    RUN_TEST(test_the_map_draws_one_stroke_a_run_of_rock);
    RUN_TEST(test_the_map_comes_down_on_the_next_keystroke);
    RUN_TEST(test_incant_needs_the_whole_word_and_the_right_one);
    RUN_TEST(test_an_incanted_ring_keeps_its_charges_and_cannot_be_done_twice);
    RUN_TEST(test_incant_reads_both_hands);
    RUN_TEST(test_examine_lists_the_floor_and_the_bag);
    RUN_TEST(test_the_examine_screen_is_laid_out_for_forty_columns);
    RUN_TEST(test_examine_highlights_the_burning_torch);
    RUN_TEST(test_the_torch_leaves_the_listing_when_pulled_and_returns_lit);
    RUN_TEST(test_the_status_line_names_both_hands_and_still_measures_forty);
    RUN_TEST(test_the_game_starts_and_stops);
    RUN_TEST(test_a_warm_redraw_spends_no_nodes_and_no_atoms);
    RUN_TEST(test_a_warm_redraw_over_an_object_spends_nothing_either);
    RUN_TEST(test_the_game_fits_the_global_table);
    RUN_TEST(test_the_game_fits_the_procedure_table);
    return UNITY_END();
}
