//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for The Snake Temple (logo/games/temple).
//
//  The game is pure Logo, and it is the one game here that draws with nothing
//  but `setcursor` and `type`. It is exercised the way test_invaders.c
//  exercises Space Invaders:
//   - loading the whole file proves it parses and that no procedure name
//     collides with a primitive (an earlier draft called its loop `play`,
//     which is the sound primitive, and the definition was silently refused);
//   - the maze generator is checked as data -- the border ring, the pillar
//     lattice and the contents of the open floor -- because every later rule
//     ("you cannot walk into wall", "the lantern never reads off the edge")
//     rests on the shape it produces;
//   - the rules that change hit points are called directly, since a random
//     bite cannot be asserted through the loop;
//   - the loop itself is driven with held keys through the mock keyboard, and
//     run long enough to prove `tidy.up` really gives its list cells back.
//

#include "test_mock_fs.h"
#include "mock_device.h"
#include "core/repl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TEMPLE_SOURCE
#error "TEMPLE_SOURCE must be defined (path to logo/games/temple)"
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

// Switch `random` to a chosen reproducible sequence.
static void seed(int n)
{
    char expr[32];
    snprintf(expr, sizeof expr, "(rerandom %d)", n);
    Result r = run_string(expr);
    TEST_ASSERT_TRUE(r.status == RESULT_NONE || r.status == RESULT_OK);
}

void setUp(void)
{
    // _and_hardware gives the clock `wait` needs and the backend `sound`
    // wants; the game touches no files, but the scaffold wires storage anyway.
    test_scaffold_setUp_with_device_and_hardware();
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
    load_file(TEMPLE_SOURCE);

    // The mock's hardware random source is the constant 42, which would make
    // every roll in the generator identical -- one character over the whole
    // floor, and `open.cell` throwing the same dart for ever. `rerandom`
    // switches `random` to the seeded PCG, which is both varied and
    // repeatable, so a failure here can be reproduced exactly.
    seed(1);
}

void tearDown(void)
{
    logo_io_close_all(&mock_io);
    test_scaffold_tearDown();
}

static void run(const char *input)
{
    Result r = run_string(input);
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK,
                             error_format(r));
}

// Read a number out of Logo through its printed form rather than straight off
// `value.as.number`. A Logo value that came from `first` of a list is a word
// whose characters happen to spell a number -- arithmetic coerces it, so the
// game does not care, but reading the union directly would hand back 0.
static float num(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);

    const char *text = value_to_string(r.value);
    char *end = NULL;
    float v = strtof(text, &end);
    TEST_ASSERT_TRUE_MESSAGE(end != text && *end == '\0', expr);
    return v;
}

static void assert_num(const char *expr, float expected)
{
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(expected, num(expr), expr);
}

static const char *str(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    return value_to_string(r.value);
}

static void assert_str(const char *expr, const char *expected)
{
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, str(expr), expr);
}

static void assert_true(const char *expr)
{
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", str(expr), expr);
}

static void assert_false(const char *expr)
{
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", str(expr), expr);
}

// The game is turn based and reads a character stream, so a test scripts the
// keystrokes rather than holding keys down. The arrows have no printable
// character; they arrive as codes 180-183, written here as raw bytes.
#define KEY_LEFT  180
#define KEY_RIGHT 183
#define KEY_UP    181
#define KEY_DOWN  182
#define KEY_SPACE  32
#define KEY_QUIT  113

#define K_LEFT  "\xB4"
#define K_UP    "\xB5"
#define K_DOWN  "\xB6"
#define K_RIGHT "\xB7"

// Hand one key to the step rule directly, the way `crawl` hands it the result
// of `ascii readchar`.
static void step_with(int code)
{
    char expr[32];
    snprintf(expr, sizeof expr, "take.a.step %d", code);
    run(expr);
}

// Read one cell of the generated maze.
static const char *cell(int x, int y)
{
    static char expr[64];
    snprintf(expr, sizeof expr, "cell %d %d", x, y);
    return str(expr);
}

// The wall is a space and the floor a DEL, neither of which can be written as
// a quoted word in Logo source -- `" ` is the empty word -- so a cell is always
// set by its character code.
static void set_cell(int x, int y, const char *what)
{
    char expr[64];
    snprintf(expr, sizeof expr, "setcell %d %d char %d", x, y, (unsigned char)*what);
    run(expr);
}

// Put Bocco at a known spot with a known neighbourhood, so a test can decide
// what he walks into instead of hoping the generator produced it.
static void stand_at(int x, int y)
{
    char expr[96];
    snprintf(expr, sizeof expr, "make \"hero.x %d  make \"hero.y %d", x, y);
    run(expr);
    set_cell(x, y, "\x7F");
}


//==========================================================================
// The file itself
//==========================================================================

// Every top-level `make` ran, which is also the check that the file parsed all
// the way to the bottom.
void test_the_file_loads_and_sets_its_constants(void)
{
    assert_num(":cols", 39);
    assert_num(":rows", 27);
    assert_num(":start.hp", 8);
    assert_num(":bite.max", 4);
    assert_num(":flask.heal", 2);
    assert_str(":wall", " ");
    assert_str(":floor", "\x7f");
    assert_str(":snake", "S");
    assert_str(":flask", "!");
    assert_str(":chest", "$");
    assert_str(":hero", "@");
}

// A procedure whose name is already a primitive is refused at definition time
// and the game then dies at the call. `play` is the sound primitive and was
// this program's first name for its loop, so the loop is named here on purpose.
void test_the_game_defines_its_own_procedures(void)
{
    assert_true("defined? \"crawl");
    assert_true("defined? \"temple");
    assert_true("defined? \"take.a.step");
    assert_false("primitive? \"crawl");
}


//==========================================================================
// The maze as data
//==========================================================================

// The border ring is what lets every other rule skip its bounds check: the
// lantern reads x-1..x+1 without clamping, and a move tests only for wall.
void test_the_maze_is_ringed_by_wall(void)
{
    run("build.maze");
    for (int x = 1; x <= 39; x++)
    {
        TEST_ASSERT_EQUAL_STRING_MESSAGE(" ", cell(x, 1), "top border");
        TEST_ASSERT_EQUAL_STRING_MESSAGE(" ", cell(x, 27), "bottom border");
    }
    for (int y = 1; y <= 27; y++)
    {
        TEST_ASSERT_EQUAL_STRING_MESSAGE(" ", cell(1, y), "left border");
        TEST_ASSERT_EQUAL_STRING_MESSAGE(" ", cell(39, y), "right border");
    }
}

// Every row is its own list. Sharing one would make `setcell` change the whole
// maze at once -- the failure that `floor.row` building a fresh list guards.
void test_each_row_is_a_separate_list(void)
{
    run("build.maze");
    set_cell(5, 5, "$");
    TEST_ASSERT_EQUAL_STRING("$", cell(5, 5));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("$", cell(5, 5), "written cell reads back");
    // The same column on every other row must be untouched.
    for (int y = 2; y <= 26; y++)
    {
        if (y == 5)
            continue;
        TEST_ASSERT_NOT_EQUAL_MESSAGE(0, strcmp(cell(5, y), "$"), "rows share structure");
    }
}

// The pillar lattice: odd coordinates from 3 up are always wall.
void test_pillars_stand_on_every_other_cell(void)
{
    run("build.maze");
    for (int y = 3; y <= 25; y += 2)
        for (int x = 3; x <= 37; x += 2)
            TEST_ASSERT_EQUAL_STRING_MESSAGE(" ", cell(x, y), "pillar missing");
}

// Nothing but floor, snake, flask or wall ever reaches the interior, which is
// what `ink.of` and `meet` are written against.
void test_the_open_floor_holds_only_snakes_flasks_and_dust(void)
{
    run("build.maze");
    int snakes = 0, flasks = 0, dust = 0;
    for (int y = 2; y <= 26; y++)
    {
        for (int x = 2; x <= 38; x++)
        {
            const char *c = cell(x, y);
            if (!strcmp(c, " "))
                continue;
            else if (!strcmp(c, "S"))
                snakes++;
            else if (!strcmp(c, "!"))
                flasks++;
            else if (!strcmp(c, "\x7F"))
                dust++;
            else
                TEST_FAIL_MESSAGE("unexpected character on the floor");
        }
    }
    // Roughly a third each, as in the original. The bounds are wide enough
    // that a fair generator will not trip them and a broken one will.
    int open = snakes + flasks + dust;
    TEST_ASSERT_GREATER_THAN_MESSAGE(300, open, "the labyrinth is almost solid");
    TEST_ASSERT_GREATER_THAN_MESSAGE(open / 6, snakes, "too few snakes");
    TEST_ASSERT_GREATER_THAN_MESSAGE(open / 6, flasks, "too few flasks");
    TEST_ASSERT_LESS_THAN_MESSAGE(open / 2, snakes, "too many snakes");
    TEST_ASSERT_LESS_THAN_MESSAGE(open / 2, flasks, "too many flasks");
}

// A chest inside a pillar could never be reached, which the original's
// unconditional poke allowed and this port does not.
void test_the_chest_is_hidden_on_open_floor(void)
{
    for (int i = 0; i < 20; i++)
    {
        seed(i + 1);
        run("build.maze  hide.the.chest");
        assert_str("cell :chest.x :chest.y", "$");
        assert_false("wall? :chest.x :chest.y");
    }
}

// Bocco starts on cleared floor, never in a wall and never on the chest.
void test_bocco_starts_on_cleared_floor_away_from_the_chest(void)
{
    for (int i = 0; i < 20; i++)
    {
        seed(i + 1);
        run("build.maze  hide.the.chest  place.hero");
        assert_str("cell :hero.x :hero.y", "\x7F");
        assert_false("and (equal? :hero.x :chest.x) (equal? :hero.y :chest.y)");
        // Standing on floor means the lantern's 3x3 is always inside the maze.
        TEST_ASSERT_GREATER_OR_EQUAL(2, (int)num(":hero.x"));
        TEST_ASSERT_LESS_OR_EQUAL(38, (int)num(":hero.x"));
        TEST_ASSERT_GREATER_OR_EQUAL(2, (int)num(":hero.y"));
        TEST_ASSERT_LESS_OR_EQUAL(26, (int)num(":hero.y"));
    }
}


//==========================================================================
// The rules
//==========================================================================

// A bite is 1 to 4, so it is checked as a range over many rolls rather than a
// value, and both ends of the range must actually come up.
void test_a_snake_bites_for_one_to_four(void)
{
    bool saw_one = false, saw_four = false;
    for (int i = 0; i < 200; i++)
    {
        run("make \"hp 100  meet :snake");
        int lost = 100 - (int)num(":hp");
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(1, lost, "a bite must cost something");
        TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(4, lost, "a bite must not cost more than 4");
        if (lost == 1)
            saw_one = true;
        if (lost == 4)
            saw_four = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(saw_one, "the cheapest bite never came up");
    TEST_ASSERT_TRUE_MESSAGE(saw_four, "the worst bite never came up");
}

void test_a_flask_heals_two(void)
{
    run("make \"hp 5  meet :flask");
    assert_num(":hp", 7);
}

void test_dust_costs_nothing(void)
{
    run("make \"hp 5  make \"won false  meet :floor");
    assert_num(":hp", 5);
    assert_false(":won");
}

void test_the_chest_wins(void)
{
    run("make \"hp 5  make \"won false  meet :chest");
    assert_true(":won");
    assert_num(":hp", 5);
}

// Entering a square empties it: a snake that has struck is gone and a drained
// flask with it, so walking back over the same square is free.
void test_entering_a_square_empties_it(void)
{
    run("build.maze");
    stand_at(10, 10);
    set_cell(11, 10, "S");
    run("make \"hp 20  enter 11 10");

    assert_num(":hero.x", 11);
    assert_num(":hero.y", 10);
    assert_str("cell 11 10", "\x7F");
    TEST_ASSERT_LESS_THAN_MESSAGE(20, (int)num(":hp"), "the snake did not bite");

    // Back over it, and this time it costs nothing.
    float hp = num(":hp");
    run("enter 10 10");
    assert_num(":hp", hp);
}


//==========================================================================
// The controls
//==========================================================================

// One arrow, one square.
void test_an_arrow_moves_one_square(void)
{
    run("build.maze  make \"hp 8  make \"won false  make \"quit false");
    stand_at(10, 10);
    set_cell(11, 10, "\x7F");

    step_with(KEY_RIGHT);
    assert_num(":hero.x", 11);
    assert_num(":hero.y", 10);
}

// This is the defect that moved the game off key state: a tap lasts longer
// than one pass of a loop, so `keydown?` reported the same press two or three
// times and Bocco walked two or three squares for one press. Reading the
// character stream makes the count exact -- three characters, three squares,
// however long each key was held down.
void test_one_key_is_one_step(void)
{
    run("new.game");
    stand_at(10, 10);
    for (int x = 11; x <= 13; x++)
        set_cell(x, 10, "\x7F");

    set_mock_input(K_RIGHT K_RIGHT K_RIGHT "q");
    run("crawl");

    assert_num(":hero.x", 13);
    assert_num(":hero.y", 10);
    assert_true(":quit");
}

// A key that is not an arrow costs the turn and nothing else -- including the
// space that started the game, if one is still in the stream.
void test_a_key_that_is_not_an_arrow_does_nothing(void)
{
    run("build.maze  make \"hp 8  make \"won false  make \"quit false");
    stand_at(10, 10);

    step_with(KEY_SPACE);
    assert_num(":hero.x", 10);
    assert_num(":hero.y", 10);
    assert_false(":quit");
    assert_num(":hp", 8);
}

// Walls stop a step outright.
void test_a_wall_refuses_the_step(void)
{
    run("build.maze  make \"hp 8  make \"won false  make \"quit false");
    stand_at(10, 10);
    set_cell(11, 10, " ");

    step_with(KEY_RIGHT);
    assert_num(":hero.x", 10);
    assert_num(":hero.y", 10);
}

// Q ends the crawl without pretending the snakes got him, in either case.
void test_q_gives_up_without_dying(void)
{
    run("build.maze  make \"hp 8  make \"won false  make \"quit false");
    stand_at(10, 10);

    step_with(KEY_QUIT);
    assert_true(":quit");
    assert_num(":hp", 8);
    assert_true("game.over?");

    mock_device_clear_output();
    run("final.word");
    TEST_ASSERT_NOT_NULL(strstr(mock_device_get_output(), "empty-handed"));

    run("make \"quit false");
    step_with(81);                   // shifted Q
    assert_true(":quit");
}

// Anything typed at the title screen is thrown away on the way into the game,
// so a player who mashed space does not spend those keys as moves.
void test_keys_typed_before_the_game_are_dropped(void)
{
    set_mock_input("   " K_RIGHT);
    run("drain.keys");
    assert_false("key?");
}


//==========================================================================
// The loop
//==========================================================================

void test_the_crawl_ends_when_the_hit_points_run_out(void)
{
    run("new.game  make \"hp 0");
    assert_true("game.over?");
    run("crawl");            // must return at once rather than spin
    assert_false(":won");

    mock_device_clear_output();
    run("final.word");
    TEST_ASSERT_NOT_NULL(strstr(mock_device_get_output(), "The snakes have Bocco"));
}

void test_the_crawl_ends_on_the_chest(void)
{
    run("new.game  make \"won true");
    assert_true("game.over?");
    run("crawl");

    mock_device_clear_output();
    run("final.word");
    TEST_ASSERT_NOT_NULL(strstr(mock_device_get_output(),
                                "escapes with the chest"));
}

// A long crawl is the only place a slow leak of list cells would show. Each
// step builds a `list` per `setcursor` and per `settextcolor` and a word for
// the hit-point line; `tidy.up` hands them back every hundred steps.
void test_a_long_crawl_gives_its_list_cells_back(void)
{
    run("new.game  make \"hp 1000");
    // Walk into a wall for ever: the drawing and the allocation happen either
    // way, so this measures the loop without the maze deciding when it ends.
    stand_at(10, 10);
    set_cell(11, 10, "\x7F");
    set_cell(9, 10, "\x7F");

    run("recycle");
    float before = num("nodes");

    for (int i = 0; i < 400; i++)
    {
        step_with(i % 2 ? KEY_LEFT : KEY_RIGHT);
        run("tidy.up");
        // Keep the two squares walkable so the walk never stalls on a snake
        // it has already eaten -- the point here is the allocation, not the maze.
        set_cell(11, 10, "\x7F");
        set_cell(9, 10, "\x7F");
    }

    run("recycle");
    float after = num("nodes");
    TEST_ASSERT_TRUE_MESSAGE(after > before - 200,
                             "400 steps leaked list cells that recycle could not reclaim");
}


//==========================================================================
// Drawing
//==========================================================================

// The lantern paints the nine squares around Bocco and nothing else -- the
// dark labyrinth is the whole game, so a lantern that lit the maze would not
// be a cosmetic defect.
void test_the_lantern_draws_nine_squares(void)
{
    run("build.maze");
    for (int y = 9; y <= 11; y++)
        for (int x = 9; x <= 11; x++)
            set_cell(x, y, "\x7F");

    mock_device_clear_output();
    run("light.around 10 10");

    const char *screen = mock_device_get_output();
    int dots = 0;
    for (const char *p = screen; *p; p++)
        if (*p == '\x7F')
            dots++;
    TEST_ASSERT_EQUAL_INT_MESSAGE(9, dots, screen);

    // and it left the cursor on the last of the nine (screen coords are one
    // less than maze coords).
    const MockDeviceState *s = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT(10, s->text.cursor_col);
    TEST_ASSERT_EQUAL_INT(10, s->text.cursor_row);
}

// The hit-point line is rewritten in place, so it has to blank what a longer
// number left behind: 10 falling to 9 must not read as "90".
void test_the_status_line_erases_the_number_it_replaces(void)
{
    run("make \"hp 10  draw.hud");
    mock_device_clear_output();
    run("make \"hp 9  draw.hud");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "HP: 9"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "HP: 9  "), screen);
}

void test_the_title_screen_names_the_game_and_the_keys(void)
{
    mock_device_clear_output();
    set_mock_input("x ");        // a key it must ignore, then space
    run("title.screen");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "THE SNAKE TEMPLE"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "RAX"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "ARROWS"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Press SPACE to begin"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "bites 1 to 4"), screen);
}

// The game owns the whole text screen, so it has to ask for it; started from
// the graphics screen it would otherwise draw where nobody can see.
void test_the_title_screen_takes_the_text_screen(void)
{
    run("fullscreen");
    set_mock_input(" ");
    run("title.screen");
    TEST_ASSERT_EQUAL_INT(MOCK_SCREEN_TEXT, mock_device_get_state()->screen_mode);
}

//==========================================================================
// Fitting the screen
//
// The text screen is 40 columns. A line one character too long does not wrap,
// it runs off the edge, and nothing in the drawing code can notice. Both
// screens outside the game keep their text in lists (`story`, `key.rows`), so
// the width of every one of them can be measured before it is ever drawn.
//==========================================================================

#define SCREEN_COLS 40

// What `type` of this expression actually puts on the screen, in characters.
// Measured rather than derived: `type` of a list drops the outer brackets and
// joins with single spaces, which is exactly the rendering being budgeted.
static int printed_width(const char *expr)
{
    char cmd[128];
    snprintf(cmd, sizeof cmd, "type %s", expr);
    mock_device_clear_output();
    run(cmd);
    return (int)strlen(mock_device_get_output());
}

// Assert what `expr` prints still fits when drawn starting at `col`.
static void assert_fits(const char *expr, int col)
{
    int w = printed_width(expr);
    char msg[192];
    snprintf(msg, sizeof msg, "%s is %d wide at column %d (limit %d)",
             expr, w, col, SCREEN_COLS - col);
    TEST_ASSERT_TRUE_MESSAGE(col + w <= SCREEN_COLS, msg);
}

// Generating the maze walks every one of a thousand cells through a list, and
// on the device that is about a second. Without a word from the program the
// title screen just sits there and the player thinks the key was missed --
// which is exactly what happened. Clearing the title and saying so is the fix.
void test_new_game_says_it_is_building(void)
{
    mock_device_clear_output();
    run("new.game");
    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "Building the temple"), screen);
}

// A dot per row of the two loops that cross the whole grid, as the Oric
// original printed one per cell. The row of them is written with `type`, which
// does not wrap, so the count is bound to the maze height and has to be
// checked against the screen: a taller maze would run the dots off the edge.
void test_the_building_dots_fit_the_screen(void)
{
    mock_device_clear_output();
    run("build.maze");                 // draws nothing else, so every dot is a tick

    int dots = 0;
    for (const char *p = mock_device_get_output(); *p; p++)
        if (*p == '.')
            dots++;

    TEST_ASSERT_GREATER_THAN_MESSAGE(0, dots, "generation showed no progress at all");

    int col = (int)num(":tick.col");
    char msg[96];
    snprintf(msg, sizeof msg, "%d dots from column %d overruns %d",
             dots, col, SCREEN_COLS);
    TEST_ASSERT_TRUE_MESSAGE(col + dots <= SCREEN_COLS, msg);
}

void test_the_story_fits_the_screen(void)
{
    int n = (int)num("count story");
    TEST_ASSERT_GREATER_THAN(0, n);
    for (int i = 1; i <= n; i++)
    {
        char expr[64];
        snprintf(expr, sizeof expr, "item %d story", i);
        assert_fits(expr, 2);            // print.lines story 2 8
    }
}

void test_the_key_fits_the_screen(void)
{
    int n = (int)num("count key.rows");
    TEST_ASSERT_GREATER_THAN(0, n);
    for (int i = 1; i <= n; i++)
    {
        char expr[64];
        snprintf(expr, sizeof expr, "first item %d key.rows", i);
        assert_fits(expr, 6);            // the character column
        snprintf(expr, sizeof expr, "butfirst item %d key.rows", i);
        assert_fits(expr, 9);            // the text column
    }
}

// The three closing messages are drawn at column 0 on their own line.
void test_the_last_word_fits_the_screen(void)
{
    const char *endings[] = {"won.the.chest", "lost.to.the.snakes", "walked.away"};
    for (size_t i = 0; i < sizeof endings / sizeof endings[0]; i++)
    {
        mock_device_clear_output();
        run(endings[i]);
        size_t w = strlen(mock_device_get_output());
        TEST_ASSERT_TRUE_MESSAGE(w <= SCREEN_COLS, endings[i]);
    }
}

void test_the_status_line_fits_the_screen(void)
{
    // Three digits of hit points is the widest the line can get.
    run("make \"hp 100");
    mock_device_clear_output();
    run("draw.hud");
    TEST_ASSERT_TRUE(strlen(mock_device_get_output()) <= SCREEN_COLS);
}

// The key spells out the tuning constants, so it can go stale the moment one
// of them is changed. Nothing in the drawing catches that; this does.
// `--` inside a list literal lexes as two words, and `type` then puts a space
// between them and draws "- -". It reads fine in the source and wrong on the
// screen, which is exactly the kind of defect that survives a code review.
void test_the_key_uses_a_dash_that_survives_the_lexer(void)
{
    int n = (int)num("count key.rows");
    for (int i = 1; i <= n; i++)
    {
        char expr[64];
        snprintf(expr, sizeof expr, "butfirst item %d key.rows", i);
        mock_device_clear_output();
        char cmd[96];
        snprintf(cmd, sizeof cmd, "type %s", expr);
        run(cmd);
        TEST_ASSERT_NULL_MESSAGE(strstr(mock_device_get_output(), "- -"),
                                 mock_device_get_output());
    }
}

void test_the_key_tells_the_truth_about_the_tuning(void)
{
    char want[32];
    const char *screen;

    mock_device_clear_output();
    run("legend 14");
    screen = mock_device_get_output();

    snprintf(want, sizeof want, "bites 1 to %d", (int)num(":bite.max"));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, want), screen);

    snprintf(want, sizeof want, "heals %d", (int)num(":flask.heal"));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, want), screen);

    snprintf(want, sizeof want, "starts with %d hp", (int)num(":start.hp"));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, want), screen);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_file_loads_and_sets_its_constants);
    RUN_TEST(test_the_game_defines_its_own_procedures);
    RUN_TEST(test_the_maze_is_ringed_by_wall);
    RUN_TEST(test_each_row_is_a_separate_list);
    RUN_TEST(test_pillars_stand_on_every_other_cell);
    RUN_TEST(test_the_open_floor_holds_only_snakes_flasks_and_dust);
    RUN_TEST(test_the_chest_is_hidden_on_open_floor);
    RUN_TEST(test_bocco_starts_on_cleared_floor_away_from_the_chest);
    RUN_TEST(test_a_snake_bites_for_one_to_four);
    RUN_TEST(test_a_flask_heals_two);
    RUN_TEST(test_dust_costs_nothing);
    RUN_TEST(test_the_chest_wins);
    RUN_TEST(test_entering_a_square_empties_it);
    RUN_TEST(test_an_arrow_moves_one_square);
    RUN_TEST(test_one_key_is_one_step);
    RUN_TEST(test_a_key_that_is_not_an_arrow_does_nothing);
    RUN_TEST(test_a_wall_refuses_the_step);
    RUN_TEST(test_q_gives_up_without_dying);
    RUN_TEST(test_keys_typed_before_the_game_are_dropped);
    RUN_TEST(test_the_crawl_ends_when_the_hit_points_run_out);
    RUN_TEST(test_the_crawl_ends_on_the_chest);
    RUN_TEST(test_a_long_crawl_gives_its_list_cells_back);
    RUN_TEST(test_the_lantern_draws_nine_squares);
    RUN_TEST(test_the_status_line_erases_the_number_it_replaces);
    RUN_TEST(test_the_title_screen_names_the_game_and_the_keys);
    RUN_TEST(test_the_title_screen_takes_the_text_screen);
    RUN_TEST(test_new_game_says_it_is_building);
    RUN_TEST(test_the_building_dots_fit_the_screen);
    RUN_TEST(test_the_story_fits_the_screen);
    RUN_TEST(test_the_key_fits_the_screen);
    RUN_TEST(test_the_last_word_fits_the_screen);
    RUN_TEST(test_the_status_line_fits_the_screen);
    RUN_TEST(test_the_key_uses_a_dash_that_survives_the_lexer);
    RUN_TEST(test_the_key_tells_the_truth_about_the_tuning);
    return UNITY_END();
}
