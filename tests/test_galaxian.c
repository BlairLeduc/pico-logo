//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the Galaxian game (logo/games/galaxian).
//
//  The game itself is pure Logo; this exercises it two ways:
//   - loading the whole file proves it parses and every procedure the init
//     path touches is defined and runs on the mock device (setup.level does
//     cs/fullscreen/stamping/when-demon registration);
//   - the pure logic helpers (the rank pyramid, the cell<->position formulas,
//     the scoring table, the clamped-turn steering, the hit-test inverse) are
//     checked directly, since those are where the bugs would hide.
//

#include "test_mock_fs.h"
#include "mock_device.h"
#include "core/repl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GALAXIAN_SOURCE
#error "GALAXIAN_SOURCE must be defined (path to logo/games/galaxian)"
#endif

#ifndef P10GAMES_SOURCE
#error "P10GAMES_SOURCE must be defined (path to logo/tests/p10games)"
#endif

// Load a whole Logo file, defining its procedures and running its top-level
// colour/tuning `make`s. Procedure definitions (`to ... end`) are not handled
// by the bare evaluator, so we buffer them and hand them to
// proc_define_from_text the same way the `load` primitive does; other lines go
// straight to run_string. Fails the test on any error.
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
    // _and_hardware gives a controllable clock so when-demon registration and
    // sound (called by kill.alien) have a backend; the mock filesystem is here
    // for the timing script, which writes its report to a file as well as the
    // screen (numbers on the PicoCalc's display cannot be copied off it).
    test_scaffold_setUp_with_device_and_hardware();
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
    load_file(GALAXIAN_SOURCE);
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

// Evaluate a Logo expression and return its number.
static float num(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    return r.value.as.number;
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
               "make \"formx -70 make \"formy 130 "
               "make \"aliens make.convoy");
}

void test_locate_alien_exact_and_tolerant(void)
{
    seed_convoy();
    // Row 4 col 1 sits at (-70, 70).
    assert_num("locate.alien -70 70", 25);
    assert_num("locate.alien -64 70", 25);   // 6px off in x, within tolerance
    assert_num("locate.alien -70 78", 25);   // 8px off in y, within tolerance
    // A flagship at (10, 130) -> row1 col5 -> idx 5.
    assert_num("locate.alien 10 130", 5);
}

void test_locate_alien_misses(void)
{
    seed_convoy();
    // Row 1 col 1 position (-70,130) is a permanent hole.
    assert_num("locate.alien -70 130", 0);
    // Well outside the grid.
    assert_num("locate.alien 200 70", 0);
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

// The per-diver lists have to be genuinely new each level.  As literals they
// were part of setup.level's own body, so the frame loop's .setitems marked
// them for good and a level begun while a diver was airborne started with
// that diver still counted as flying: all.divers.idle? stayed false and no
// new dive could launch.
void test_setup_level_gives_the_divers_fresh_state(void)
{
    run_string("make \"level 1 setup.level");
    run_string(".setitem 1 :diver.phase 2 .setitem 1 :diver.timer 9 "
               ".setitem 1 :diver.cell 7");

    run_string("setup.level");
    assert_true("0 = item 1 :diver.phase");
    assert_true("0 = item 1 :diver.timer");
    assert_true("0 = item 1 :diver.cell");
    assert_true("all.divers.idle?");
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

void test_diver_breaks_away_near_bottom(void)
{
    run_string("make \"level 1 setup.level");
    // Detach a diver on turtle 2 and force it into the attack phase, low on
    // the screen (below :break.y) and just to the right of the player.
    run_string("launch.diver 2 25 240");
    run_string(".setitem 1 :diver.phase 2 .setitem 1 :diver.timer :attack.frames");
    run_string("tell 2 setx 40 sety -125 seth 200 tell 0");
    // Homing on the player (at 0,-145, down-left of the diver) would raise the
    // heading toward ~243; breaking away straight down lowers it toward 180.
    run_string("steer.divers");
    assert_true("(ask 2 [heading]) < 200");
}

//==========================================================================
// The frame: one callable procedure, and a level that can run for minutes
//==========================================================================

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

// Put the game where play.frame can be called: level built, refresh manual so
// the sync ending a frame presents and returns instead of pacing to 25 fps.
static void start_level(void)
{
    run_string("make \"level 1 make \"score 0 make \"lives 3 setup.level");
    run_string("setrefresh \"manual");
}

void test_play_frame_is_the_loop_body(void)
{
    start_level();
    run_string("repeat 5 [play.frame]");
    assert_num(":frame.count", 5);
    // A paused frame still polls, draws the HUD and presents, but runs no
    // simulation -- so the frame counter (and everything clocked off it) stops.
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
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Galaxian"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "nodes at end"), screen);

    // And the same report reached the file, which is the copy that leaves the
    // board -- a screenful of numbers on the PicoCalc cannot be typed out.
    MockFile *report = mock_fs_get_file("p10games.txt", false);
    TEST_ASSERT_NOT_NULL_MESSAGE(report, "p10games.txt was not written");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->data, "frame mean"), report->data);
}

// The attract screen is where a player is told how to play and what a rank is
// worth, so the instructions and the score table have to reach the screen, and
// the four numbers in the table have to be the ones rank.score pays out.
void test_attract_screen_shows_instructions_and_scores(void)
{
    mock_device_clear_output();
    set_mock_input(" "); // wait.for.space reads one character and returns
    Result r = run_string("attract.screen");
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                             error_format(r));

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "GALAXIAN"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Flagship"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Purple"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "ARROWS Move"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "SPACE Fire"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "P Pause"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Press Space"), screen);

    // The legend's four convoy values, its "worth double" line and its diving
    // flagship are all rank.score, so the table cannot drift from the payout.
    assert_num("rank.score 1 1", 60);
    assert_num("rank.score 2 1", 50);
    assert_num("rank.score 3 1", 40);
    assert_num("rank.score 4 1", 30);
    assert_num("rank.score 2 2", 100);
    assert_num("rank.score 1 2", 300);
}

// The eight collision pairs fill MAX_DEMONS exactly, so the game can only
// arm them if it owns the whole table. Nothing disarms a demon on the way
// out of a program, so a workspace that ran anything using `when` first --
// Invaders, most obviously -- leaves the table part full and arm.demons runs
// out of slots part way through, after the convoy is already on the screen.
void test_arm_demons_takes_a_table_an_earlier_program_left_behind(void)
{
    Result r = run_string("when [1 = 2] [stop]"); // a demon from an earlier game
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                             error_format(r));

    r = run_string("setup.level");
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                             error_format(r));

    // And the stale demon is gone, not merely displaced: its action belongs to
    // a program whose globals this one does not have.
    mock_device_clear_output();
    run_string("demons");
    TEST_ASSERT_NULL_MESSAGE(strstr(mock_device_get_output(), "1 = 2"),
                             mock_device_get_output());
}

// A pause has to hold the player as well as the aliens. `freeze` suspends
// demons and autonomous turtle motion, which is what stops the convoy, but
// steering and firing are neither -- they are poll.input writing to the
// turtle directly, and poll.input runs outside the paused guard so the pause
// key can still be read.
void test_pause_holds_the_ship_as_well_as_the_aliens(void)
{
    start_level();
    run_string("toggle.pause");
    assert_true(":paused");

    float x = num("ask 0 [xcor]");
    set_mock_input("\264"); // left arrow (180)
    run_string("play.frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(x, num("ask 0 [xcor]"),
                                    "the ship steered while the game was paused");

    set_mock_input(" "); // fire
    run_string("play.frame");
    assert_true("not (ask 1 [shown?])");

    // ...but P itself still has to reach the game, or the pause could never
    // be lifted, and the ship has to steer again once it is.
    set_mock_input("p");
    run_string("play.frame");
    assert_true("not :paused");

    set_mock_input("\264");
    run_string("play.frame");
    TEST_ASSERT_NOT_EQUAL_MESSAGE((int)x, (int)num("ask 0 [xcor]"),
                                  "the ship did not steer after the pause was lifted");
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_file_loads_and_sets_globals);
    RUN_TEST(test_attract_screen_shows_instructions_and_scores);
    RUN_TEST(test_arm_demons_takes_a_table_an_earlier_program_left_behind);
    RUN_TEST(test_pause_holds_the_ship_as_well_as_the_aliens);
    RUN_TEST(test_pyramid_occupancy);
    RUN_TEST(test_pyramid_total_is_20);
    RUN_TEST(test_row_col_mapping);
    RUN_TEST(test_rank_colour);
    RUN_TEST(test_rank_score);
    RUN_TEST(test_turn_toward_clamps_and_wraps);
    RUN_TEST(test_locate_alien_exact_and_tolerant);
    RUN_TEST(test_locate_alien_misses);
    RUN_TEST(test_setup_level_runs);
    RUN_TEST(test_setup_level_gives_the_divers_fresh_state);
    RUN_TEST(test_convoy_kill_scores_and_shrinks);
    RUN_TEST(test_dive_detach_and_rejoin);
    RUN_TEST(test_flight_kill_scores_doubled);
    RUN_TEST(test_find_flank_walks_inward);
    RUN_TEST(test_flank_dive_launches_a_diver);
    RUN_TEST(test_diver_breaks_away_near_bottom);
    RUN_TEST(test_play_frame_is_the_loop_body);
    RUN_TEST(test_an_idle_frame_allocates_nothing);
    RUN_TEST(test_scoring_frames_are_reclaimed);
    RUN_TEST(test_a_paused_frame_never_recycles);
    RUN_TEST(test_p10games_script_runs);
    return UNITY_END();
}
