//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the Space Invaders game (logo/games/invaders).
//
//  The game is pure Logo; this exercises it the way test_galaxian.c does:
//   - loading the whole file proves it parses and that every procedure the
//     init path touches is defined and runs on the mock device (setup.level
//     does cs/fullscreen/stamping/when-demon registration);
//   - the pure logic helpers (the cell<->position formulas, the incrementally
//     tracked living edges, the hit-test inverse) are checked directly;
//   - the frame is called as the game calls it, for as long as a level lasts,
//     since that is where a slow leak of list storage would end the program.
//

#include "test_mock_fs.h"
#include "mock_device.h"
#include "core/repl.h"
#include <stdio.h>
#include <string.h>

#ifndef INVADERS_SOURCE
#error "INVADERS_SOURCE must be defined (path to logo/games/invaders)"
#endif

#ifndef P10GAMES_SOURCE
#error "P10GAMES_SOURCE must be defined (path to logo/tests/p10games)"
#endif

// Load a whole Logo file, defining its procedures and running its top-level
// `make`s, the way the `load` primitive does. Fails the test on any error.
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
                memcpy(proc + proc_len, "end", 4);
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
    // _and_hardware gives a controllable clock, so when-demon registration and
    // sound (called by kill.alien) have a backend; the mock filesystem is here
    // for the timing script, which writes its report to a file as well as the
    // screen (numbers on the PicoCalc's display cannot be copied off it).
    test_scaffold_setUp_with_device_and_hardware();
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
    load_file(INVADERS_SOURCE);
}

void tearDown(void)
{
    logo_io_close_all(&mock_io);
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

// Evaluate a Logo expression and return its number.
static float num(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    return r.value.as.number;
}


// Run frames one at a time with the clock advancing a frame's worth each, so
// the collision demons really poll (DEMON_POLL_MS is 20) and the turtles really
// glide. `repeat 100 [play.frame]` with a stopped clock would suppress every
// poll after the first and measure the game with its collisions switched off.
static void run_frames(int frames)
{
    for (int i = 0; i < frames; i++)
    {
        set_mock_ticks(mock_ticks_value + 40);
        Result r = run_string("play.frame");
        TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                                 error_format(r));
    }
}

// The same, with the score changed before each frame so draw.hud repaints --
// the game's one allocating path, and the reason play.frame reclaims.
static void run_scoring_frames(int frames)
{
    for (int i = 0; i < frames; i++)
    {
        run_string("make \"score :score + 10");
        set_mock_ticks(mock_ticks_value + 40);
        Result r = run_string("play.frame");
        TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                                 error_format(r));
    }
}

// Build a level: 5x11 formation, four shields, demons armed.
static void start_level(void)
{
    run_string("make \"level 1 make \"score 0 make \"lives 3 setup.level");
    run_string("setrefresh \"manual");
}

//==========================================================================
// The file loads and its globals are set
//==========================================================================

void test_file_loads_and_sets_globals(void)
{
    assert_num(":alien.colour", 249);
    assert_num(":shield.colour", 252);
    assert_num(":cannon.colour", 254);
}

//==========================================================================
// Cell <-> row/column mapping (5x11 row-major) and cell positions
//==========================================================================

void test_row_col_mapping(void)
{
    run_string("make \"alien.cols 11");
    assert_num("alien.row 1", 1);
    assert_num("alien.col 1", 1);
    assert_num("alien.row 11", 1);
    assert_num("alien.col 11", 11);
    assert_num("alien.row 12", 2);
    assert_num("alien.col 12", 1);
    assert_num("alien.row 55", 5);
    assert_num("alien.col 55", 11);
}

void test_cell_positions_and_bottom_row(void)
{
    start_level();
    assert_num("alien.cell.x 1", -100);
    assert_num("alien.cell.x 2", -80);
    assert_num("alien.cell.y 1", 120);
    assert_num("alien.cell.y 2", 98);
    // The invasion test reads the lowest LIVING row, not the grid's bottom.
    assert_num("alien.bottom.y", 120 - 4 * 22);
    run_string("make \"form.bot 3");
    assert_num("alien.bottom.y", 120 - 2 * 22);
}

//==========================================================================
// Kills: liveness, score, and the incrementally tracked living edges
//==========================================================================

void test_setup_level_runs(void)
{
    start_level();
    assert_num(":alive", 55);
    assert_true("55 = count :aliens");
    assert_num(":form.lo", 1);
    assert_num(":form.hi", 11);
    assert_num(":form.bot", 5);
}

void test_kill_scores_and_shrinks_edges(void)
{
    start_level();
    assert_num(":score", 0);
    // Empty column 1 (rows 1..5 are idx 1, 12, 23, 34, 45): the left living
    // edge must walk in to column 2, and nothing else moves.
    run_string("kill.alien 1 kill.alien 12 kill.alien 23 kill.alien 34 kill.alien 45");
    assert_num(":score", 50);
    assert_num(":alive", 50);
    assert_num(":form.lo", 2);
    assert_num(":form.hi", 11);
    // Empty the bottom row (idx 45..55, one already dead): the living bottom
    // row rises, so the formation may descend further before it invades.
    run_string("make \"i 46 repeat 10 [kill.alien :i make \"i :i + 1]");
    assert_num(":form.bot", 4);
}

//==========================================================================
// Shot-vs-formation hit test (inverse of the cell formula, with tolerance)
//==========================================================================

void test_locate_alien_exact_and_tolerant(void)
{
    start_level();
    assert_num("locate.alien -100 120", 1);    // row 1, col 1
    assert_num("locate.alien -94 120", 1);     // 6px off in x, within tolerance
    assert_num("locate.alien -100 128", 1);    // 8px off in y, within tolerance
    assert_num("locate.alien -80 98", 13);     // row 2, col 2
}

void test_locate_alien_misses(void)
{
    start_level();
    run_string("kill.alien 1");
    assert_num("locate.alien -100 120", 0);    // dead cell
    assert_num("locate.alien 200 120", 0);     // off the grid
    // The 10-unit tolerance covers a whole 20-unit colgap, so no x can fall
    // between two columns; the 22-unit rowgap does leave a two-unit band.
    assert_num("locate.alien -100 109", 0);
}

//==========================================================================
// The frame: one callable procedure, and a level that can run for minutes
//==========================================================================

void test_play_frame_is_the_loop_body(void)
{
    start_level();
    run_string("repeat 5 [play.frame]");
    assert_num(":frame.count", 5);
    // A paused frame still polls, draws the HUD and presents, but runs no
    // simulation -- so the frame counter (and the march clocked off it) stops.
    run_string("make \"paused true repeat 5 [play.frame]");
    assert_num(":frame.count", 5);
}

// A frame with nothing to redraw allocates nothing: every actor list is
// mutated in place, and unlike Turtle Trails -- whose fractional-pixel
// arithmetic costs about five cells a frame -- neither of these games computes
// anything per frame that needs a cons cell. That is the property keeping a
// level alive, so it is worth a guard.
void test_an_idle_frame_allocates_nothing(void)
{
    start_level();
    run_frames(20);                         // settle: the first frames draw the HUD
    run_string("recycle");
    int before = (int)num("nodes");
    run_frames(100);
    TEST_ASSERT_EQUAL_INT_MESSAGE(before, (int)num("nodes"),
                                  "a frame with nothing to redraw now costs storage");
}

// Scoring does allocate. draw.hud builds `sentence [SCORE:] :s` for each of its
// three fields and does it twice (erase the old line, draw the new), about 14
// cells a repaint -- and a repaint follows every kill. Logo frees none of it on
// its own, so a long session slides towards `out of space`; the reclaim timer
// in play.frame is what holds free storage flat.
void test_scoring_frames_are_reclaimed(void)
{
    start_level();
    run_frames(20);
    run_string("recycle");
    int before = (int)num("nodes");

    run_scoring_frames(100);
    int spent = before - (int)num("nodes");
    TEST_ASSERT_TRUE_MESSAGE(spent > 0,
                             "the HUD repaint costs nothing now -- has draw.hud changed?");

    // All of it is garbage rather than retention: recycle brings it back.
    run_string("recycle");
    TEST_ASSERT_INT_WITHIN_MESSAGE(20, before, (int)num("nodes"),
                                   "scoring frames retained storage recycle cannot free");

    // And the game reclaims on its own, without the test asking: over 600 more
    // scoring frames free storage must stay within one 250-frame window of
    // where it started, rather than sliding the whole way down.
    int start = (int)num("nodes");
    run_scoring_frames(600);
    int now = (int)num("nodes");
    TEST_ASSERT_TRUE_MESSAGE(now > 0, "the pool ran dry despite the reclaim timer");
    TEST_ASSERT_TRUE_MESSAGE(start - now < (250 * spent / 100) + 200,
                             "free storage slid: the reclaim timer is not keeping up");
}

// A paused frame must not recycle. The frame counter stops while paused, so a
// pause landing on a multiple of 250 leaves `remainder :frame.count 250` at
// zero for as long as the pause lasts -- and `reclaim` sat outside the paused
// block, so it recycled on every one of those frames. Garbage left lying
// around is the probe: a recycle would hand it back and free `nodes` would
// jump.
void test_a_paused_frame_never_recycles(void)
{
    start_level();
    run_frames(20);
    run_string("make \"frame.count 250 make \"paused true");
    run_string("repeat 200 [make \"junk fput 1 [1 2 3]]");   // ~800 cells of garbage

    int before = (int)num("nodes");
    run_frames(5);
    TEST_ASSERT_EQUAL_INT_MESSAGE(before, (int)num("nodes"),
                                  "a paused frame recycled -- reclaim is outside the pause");
}

// The hardware timing script must run end to end on the mock, so a script that
// fails half way through cannot waste a board session (the p9m0 convention).
void test_p10games_script_runs(void)
{
    load_file(P10GAMES_SOURCE);
    mock_device_clear_output();
    Result r = run_string("p10games");
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                             error_format(r));
    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "frame mean"), screen);
    // The report names the game it measured: the file appends, so a run has to
    // be tellable from the one before it.
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Space Invaders"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "nodes at end"), screen);

    // And the same report reached the file, which is the copy that leaves the
    // board -- a screenful of numbers on the PicoCalc cannot be typed out.
    MockFile *report = mock_fs_get_file("p10games.txt", false);
    TEST_ASSERT_NOT_NULL_MESSAGE(report, "p10games.txt was not written");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->data, "frame mean"), report->data);
}

// The attract screen is where a player is told how to play and what a target
// is worth, so the instructions and the score table have to reach the screen,
// and the numbers in the table have to be the ones the game awards.
void test_attract_screen_shows_instructions_and_scores(void)
{
    mock_device_clear_output();
    set_mock_input(" "); // wait.for.space reads one character and returns
    Result r = run_string("attract.screen");
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                             error_format(r));

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "SPACE INVADERS"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Alien"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Saucer"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "ARROWS Move"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "SPACE Fire"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "P Pause"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Press Space"), screen);

    // The two numbers the legend prints are the two the game pays out: an
    // alien through kill.alien, the saucer through ufo.hit.
    start_level();
    run_string("make \"score 0  kill.alien 1");
    assert_num(":score", 10);
    run_string("make \"score 0  ufo.hit");
    assert_num(":score", 100);
}

// Nothing disarms a demon on the way out of a program, so a workspace that
// ran Galaxian first (it fills all eight slots) leaves no room for these five
// and arm.demons runs out of slots part way through setup.level.
void test_arm_demons_takes_a_table_an_earlier_program_left_behind(void)
{
    // Four foreign demons: with the five below that is nine, past MAX_DEMONS.
    run_string("when [1 = 2] [stop]");
    run_string("when [1 = 3] [stop]");
    run_string("when [1 = 4] [stop]");
    run_string("when [1 = 5] [stop]");

    Result r = run_string("setup.level");
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                             error_format(r));

    mock_device_clear_output();
    run_string("demons");
    TEST_ASSERT_NULL_MESSAGE(strstr(mock_device_get_output(), "1 = 2"),
                             mock_device_get_output());
}

// A pause has to hold the cannon as well as the formation. `freeze` suspends
// demons and autonomous turtle motion, which is what stops the march, but
// steering and firing are neither -- they are poll.input writing to the
// turtle directly, and poll.input runs outside the paused guard so the pause
// key can still be read.
void test_pause_holds_the_cannon_as_well_as_the_formation(void)
{
    start_level();
    run_string("toggle.pause");
    assert_true(":paused");

    float x = num("ask 0 [xcor]");
    set_mock_input("\264"); // left arrow (180)
    run_string("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(x, num("ask 0 [xcor]"),
                                    "the cannon steered while the game was paused");

    set_mock_input(" "); // fire
    run_string("play.frame");
    assert_true("not (ask 1 [shown?])");

    // ...but P itself still has to reach the game, or the pause could never
    // be lifted, and the cannon has to steer again once it is.
    set_mock_input("p");
    run_string("play.frame");
    assert_true("not :paused");

    set_mock_input("\264");
    run_string("play.frame");
    TEST_ASSERT_NOT_EQUAL_FLOAT_MESSAGE(x, num("ask 0 [xcor]"),
                                        "the cannon did not steer after the pause was lifted");
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_file_loads_and_sets_globals);
    RUN_TEST(test_attract_screen_shows_instructions_and_scores);
    RUN_TEST(test_arm_demons_takes_a_table_an_earlier_program_left_behind);
    RUN_TEST(test_pause_holds_the_cannon_as_well_as_the_formation);
    RUN_TEST(test_row_col_mapping);
    RUN_TEST(test_cell_positions_and_bottom_row);
    RUN_TEST(test_setup_level_runs);
    RUN_TEST(test_kill_scores_and_shrinks_edges);
    RUN_TEST(test_locate_alien_exact_and_tolerant);
    RUN_TEST(test_locate_alien_misses);
    RUN_TEST(test_play_frame_is_the_loop_body);
    RUN_TEST(test_an_idle_frame_allocates_nothing);
    RUN_TEST(test_scoring_frames_are_reclaimed);
    RUN_TEST(test_a_paused_frame_never_recycles);
    RUN_TEST(test_p10games_script_runs);
    return UNITY_END();
}
