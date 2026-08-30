//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for Berzerk (P15) M1 -- the room.  See docs/berzerk-design.md
//  sections 6, 7.1 and 19.
//
//  Nothing here checks a timing.  The host is 150-180x faster than the target
//  and `ticks` has millisecond resolution, so the transition figure the game
//  puts on the screen reads as zero here; what a room costs was measured by
//  M0 on a board (docs/measurements/p15m0-bitmap-fast-pico2w-2026-08-29.md:
//  7.37 ms to generate, 3.34 ms to draw).
//
//  WHAT M1'S GATE ACTUALLY ASKS is "walk out of a room and back into it and it
//  is the same room", and that is a claim about a pure function, which is
//  exactly the half a board cannot check.  A person walking out and back sees
//  a maze that LOOKS the same; these tests read the fifteen wall masks and the
//  eight segments back out and compare them number for number.
//
//  Four of them are the ones this milestone exists for, and each names the
//  failure it is here for:
//
//    * The room reproduces.  Section 6.1's whole claim, and the reason an
//      infinite maze costs two globals and no storage.
//    * THE DRAWN WALLS AGREE WITH THE WALL MASKS.  This is the new one, and it
//      is the check section 6.2 says is worth writing out: section 5 has two
//      coordinate conventions (the disassembly's `WALLINDEX` takes H as x, and
//      its wall drawer takes H as the vertical axis), so a segment's geometry
//      and the bits it sets are computed down two different paths from the
//      same `rand & 3`.  If they ever disagree, the room draws correctly and
//      M3's robots path through a wall you can see -- and no test that checks
//      either half on its own would notice.  So every drawn segment is probed
//      from both sides with `cell.at` and the mask bits are the assertion.
//    * The coordinates wrap at 256 (section 22, Q3, taken at this milestone).
//      Letting them run would alias room (256, 0) onto room (0, 1), because
//      the seed is ROOM_X + 256.ROOM_Y and that is one number.
//    * One press is one room.  The game reads a CHARACTER STREAM here and not
//      key state, which is the defect the Snake Temple lost a milestone to: a
//      tap outlives a pass of a loop that has nothing else to do, so key state
//      reports it three or four times.
//
//  The generator itself, the eight intersections, the four choices and
//  `cell.at`'s clamps are already covered against an independent C copy of the
//  LCG in tests/test_p15m0.c (design section 20), and are not repeated here --
//  except for one short assertion that the game's OWN copy of `rnd` is the
//  same generator, because a transposed constant is precisely what copying a
//  procedure from a harness into a game introduces, and the maze would still
//  look like a maze.
//

#include "test_mock_fs.h"
#include "test_scaffold.h"
#include "mock_device.h"
#include "core/repl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BERZERK_SOURCE
#error "BERZERK_SOURCE must be defined (path to logo/games/berzerk)"
#endif

// Load a whole Logo file, defining its procedures and running its top-level
// `make`s. Procedure definitions are not handled by the bare evaluator, so we
// buffer them and hand them to proc_define_from_text the way `load` does.
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
    // _and_hardware gives a clock for `ticks`, which the transition timing
    // reads either side of a room build.
    test_scaffold_setUp_with_device_and_hardware();
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
    load_file(BERZERK_SOURCE);
}

void tearDown(void)
{
    logo_io_close_all(&mock_io);
    test_scaffold_tearDown();
}

// Evaluate a Logo expression and return its number.
//
// Through `value_to_number` and not `r.value.as.number`, because `item` hands
// back whatever the slot holds and a list element is a word until something
// does arithmetic on it -- reading the union directly gives a pointer
// reinterpreted as a float.
static float num(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    float n = 0.0f;
    TEST_ASSERT_TRUE_MESSAGE(value_to_number(r.value, &n), expr);
    return n;
}

static const char *word_of(const char *expr)
{
    Result r = eval_string(expr);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, expr);
    return value_to_string(r.value);
}

static void run(const char *input)
{
    Result r = run_string(input);
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, input);
}

// Put the game in a named room and generate it.
static void in_room(int x, int y)
{
    char cmd[96];
    snprintf(cmd, sizeof(cmd),
             "make \"room.x %d  make \"room.y %d  setup.room", x, y);
    run(cmd);
}

// The fifteen wall masks, as fifteen floats.
static void read_cell(float out[15])
{
    for (int i = 0; i < 15; i++)
    {
        char expr[32];
        snprintf(expr, sizeof(expr), "item %d :cell", i + 1);
        out[i] = num(expr);
    }
}

// The eight segments: start point, heading, length.
typedef struct { float x, y, h, l; } Segment;

static void read_segments(Segment out[8])
{
    for (int k = 0; k < 8; k++)
    {
        char e[32];
        snprintf(e, sizeof(e), "item %d :seg.x", k + 1);  out[k].x = num(e);
        snprintf(e, sizeof(e), "item %d :seg.y", k + 1);  out[k].y = num(e);
        snprintf(e, sizeof(e), "item %d :seg.h", k + 1);  out[k].h = num(e);
        snprintf(e, sizeof(e), "item %d :seg.l", k + 1);  out[k].l = num(e);
    }
}

// The mask of the cell a point lands in, through the game's own `cell.at`.
static int mask_at(float x, float y)
{
    char e[64];
    snprintf(e, sizeof(e), "item (cell.at %g %g) :cell", (double)x, (double)y);
    return (int)num(e);
}

// The arrows, as `readchar` hands them back.  They have no printable
// character, so they arrive as codes of their own.
#define K_LEFT  "\xB4"
#define K_UP    "\xB5"
#define K_DOWN  "\xB6"
#define K_RIGHT "\xB7"
#define K_ESC   "\xB1"

//==========================================================================
// The generator, once
//==========================================================================

// The LCG at $2678, against a copy written here rather than against the game's
// own.  test_p15m0.c checks this at length; what is checked here is only that
// the procedure the game carries is the same one the harness measured -- a
// transposed constant is what copying a procedure introduces, and a maze built
// from the wrong stream is still a maze.
void test_the_games_generator_is_the_roms_own_lcg(void)
{
    run("make \"seed 0");
    unsigned seed = 0;
    for (int i = 0; i < 64; i++)
    {
        unsigned expected = ((7u * seed + 0x3153u) & 0xFFFFu) >> 8;
        seed = (7u * seed + 0x3153u) & 0xFFFFu;
        char msg[96];
        snprintf(msg, sizeof(msg), "draw %d diverged from the ROM's LCG", i);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)expected, num("rnd"), msg);
    }
}

//==========================================================================
// The gate: the maze is a function of where you are
//==========================================================================

// Section 6.1.  Generate a room, generate a different one, generate the first
// again: the fifteen masks and the eight segments come back identical, and two
// different coordinates do not give the same room.
void test_the_maze_is_a_function_of_the_room_coordinates(void)
{
    float first[15], elsewhere[15], again[15];
    Segment s0[8], s1[8];

    in_room(0, 0);
    read_cell(first);
    read_segments(s0);

    in_room(3, 7);
    read_cell(elsewhere);

    in_room(0, 0);
    read_cell(again);
    read_segments(s1);

    bool differs = false;
    for (int i = 0; i < 15; i++)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "cell %d did not reproduce", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(first[i], again[i], msg);
        if (fabsf(first[i] - elsewhere[i]) > 0.5f)
            differs = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(differs, "two different rooms generated the same maze");

    for (int k = 0; k < 8; k++)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "segment %d did not reproduce", k + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(s0[k].x, s1[k].x, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(s0[k].y, s1[k].y, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(s0[k].h, s1[k].h, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(s0[k].l, s1[k].l, msg);
    }
}

// THE MILESTONE, driven the way a player drives it: out of the room through
// each of the four doorways in turn and back in again.  Nothing is stored
// between the two, so this is the whole of section 6.1's claim exercised
// through the code path that will carry the man in M2.
void test_walking_out_and_back_is_the_same_room(void)
{
    static const struct { const char *out; const char *back; const char *way; } trips[] = {
        { "go.room 1 0",  "go.room -1 0", "right and back" },
        { "go.room -1 0", "go.room 1 0",  "left and back" },
        { "go.room 0 1",  "go.room 0 -1", "down and back" },
        { "go.room 0 -1", "go.room 0 1",  "up and back" },
    };

    for (size_t t = 0; t < sizeof(trips) / sizeof(trips[0]); t++)
    {
        float here[15], there[15], again[15];

        in_room(12, 5);
        read_cell(here);

        run(trips[t].out);
        read_cell(there);

        run(trips[t].back);
        read_cell(again);

        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(12.0f, num(":room.x"), trips[t].way);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5.0f, num(":room.y"), trips[t].way);

        bool differs = false;
        for (int i = 0; i < 15; i++)
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "%s: cell %d came back different",
                     trips[t].way, i + 1);
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(here[i], again[i], msg);
            if (fabsf(here[i] - there[i]) > 0.5f)
                differs = true;
        }
        TEST_ASSERT_TRUE_MESSAGE(differs, "the room next door is the same room");
    }
}

// Section 22, Q3, taken at this milestone: the coordinates wrap at 256,
// because ROOM_X and ROOM_Y are a byte each in the cabinet and the seed is
// ROOM_X + 256.ROOM_Y.  Letting them run would alias room (256, 0) onto room
// (0, 1) -- the same seed, so the same room -- which is a wrap nobody chose.
void test_the_room_coordinates_wrap_at_256(void)
{
    in_room(0, 0);
    run("go.room -1 0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(255.0f, num(":room.x"), "walking left off zero");
    TEST_ASSERT_EQUAL_FLOAT(0.0f, num(":room.y"));

    run("go.room 1 0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":room.x"), "walking right off 255");

    run("go.room 0 -1");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(255.0f, num(":room.y"), "walking up off zero");

    run("go.room 0 1");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":room.y"), "walking down off 255");

    // And the wrapped room is the room it wrapped onto, not merely a legal
    // number: walking left off zero and right again is the room you left.
    float here[15], again[15];
    in_room(0, 4);
    read_cell(here);
    run("go.room -1 0");
    run("go.room 1 0");
    read_cell(again);
    for (int i = 0; i < 15; i++)
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(here[i], again[i], "the wrap changed the room");
}

//==========================================================================
// The check section 6.2 says is worth writing out
//==========================================================================

// THE PICTURE AND THE TABLE ARE COMPUTED DOWN TWO DIFFERENT PATHS from the
// same `rand & 3`, and section 5 warns that the disassembly's two coordinate
// conventions disagree with each other by name: `WALLINDEX` takes H as x, and
// `DRAW_VERTICAL_WALL` takes H as the vertical axis, so "vertical wall" draws
// a horizontal run.  A sign error in either path leaves a room that draws
// perfectly and whose masks describe a different maze -- and from M3 that is a
// robot walking through a wall you can see.
//
// So: take every drawn segment, stand ten steps off each side of its middle,
// and ask `cell.at` what the cell there is walled with.  A horizontal run is
// the BOTTOM of the cell above it and the TOP of the cell below; a vertical
// run is the RIGHT of the cell left of it and the LEFT of the cell right.
//
// Sixteen rooms, because a generator that placed one of the four choices
// wrongly would pass a single room.
void test_the_drawn_walls_agree_with_the_wall_masks(void)
{
    for (int ry = 0; ry < 4; ry++)
    {
        for (int rx = 0; rx < 4; rx++)
        {
            in_room(rx, ry);

            Segment s[8];
            read_segments(s);

            for (int k = 0; k < 8; k++)
            {
                char msg[160];
                bool horizontal = fabsf(s[k].h - 90.0f) < 0.5f || fabsf(s[k].h - 270.0f) < 0.5f;
                float dir = (fabsf(s[k].h - 270.0f) < 0.5f || fabsf(s[k].h - 180.0f) < 0.5f)
                                ? -1.0f : 1.0f;

                if (horizontal)
                {
                    float mx = s[k].x + dir * s[k].l / 2.0f;
                    int above = mask_at(mx, s[k].y + 10.0f);
                    int below = mask_at(mx, s[k].y - 10.0f);

                    snprintf(msg, sizeof(msg),
                             "room %d,%d segment %d runs along y=%g at x=%g "
                             "and the cell above it has no BOTTOM wall (mask %d)",
                             rx, ry, k + 1, (double)s[k].y, (double)mx, above);
                    TEST_ASSERT_TRUE_MESSAGE((above & 8) != 0, msg);

                    snprintf(msg, sizeof(msg),
                             "room %d,%d segment %d runs along y=%g at x=%g "
                             "and the cell below it has no TOP wall (mask %d)",
                             rx, ry, k + 1, (double)s[k].y, (double)mx, below);
                    TEST_ASSERT_TRUE_MESSAGE((below & 4) != 0, msg);
                }
                else
                {
                    float my = s[k].y + dir * s[k].l / 2.0f;
                    int left = mask_at(s[k].x - 10.0f, my);
                    int right = mask_at(s[k].x + 10.0f, my);

                    snprintf(msg, sizeof(msg),
                             "room %d,%d segment %d runs up x=%g at y=%g "
                             "and the cell left of it has no RIGHT wall (mask %d)",
                             rx, ry, k + 1, (double)s[k].x, (double)my, left);
                    TEST_ASSERT_TRUE_MESSAGE((left & 2) != 0, msg);

                    snprintf(msg, sizeof(msg),
                             "room %d,%d segment %d runs up x=%g at y=%g "
                             "and the cell right of it has no LEFT wall (mask %d)",
                             rx, ry, k + 1, (double)s[k].x, (double)my, right);
                    TEST_ASSERT_TRUE_MESSAGE((right & 1) != 0, msg);
                }
            }
        }
    }
}

//==========================================================================
// The walls, drawn
//==========================================================================

// Section 7.1: the border is one closed circuit with the pen toggling, so it
// is eight runs -- two either side of each of four doorways -- and the turtle
// comes back to where it started.  Every stroke is ON the playfield boundary,
// never across it.
void test_the_border_is_a_closed_circuit_with_four_doorways(void)
{
    run("clean");
    mock_device_clear_graphics();
    run("draw.border");

    TEST_ASSERT_EQUAL_INT_MESSAGE(8, mock_device_line_count(),
        "the border is not eight runs, so a doorway is missing or doubled");

    TEST_ASSERT_FLOAT_WITHIN(0.5f, -122.0f, num("xcor"));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 142.0f, num("ycor"));

    for (int i = 0; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        char msg[128];
        snprintf(msg, sizeof(msg), "border run %d is at %g,%g to %g,%g",
                 i, (double)l->x1, (double)l->y1, (double)l->x2, (double)l->y2);
        bool vertical = fabsf(l->x1 - l->x2) < 0.5f && fabsf(fabsf(l->x1) - 122.0f) < 0.5f;
        bool horizontal = fabsf(l->y1 - l->y2) < 0.5f &&
                          (fabsf(l->y1 - 142.0f) < 0.5f || fabsf(l->y1 + 62.0f) < 0.5f);
        TEST_ASSERT_TRUE_MESSAGE(vertical || horizontal, msg);
    }
}

// Sixteen runs at most, and exactly sixteen every time: eight of border and
// eight interior, one per interior grid intersection.  Nothing checks
// connectivity and nothing prevents a sealed pocket -- that is the generator,
// and a room with a missing segment would be a generator that lost a draw.
void test_a_room_is_eight_border_runs_and_eight_interior_ones(void)
{
    in_room(6, 3);
    run("clean");
    mock_device_clear_graphics();
    run("draw.walls");

    TEST_ASSERT_EQUAL_INT_MESSAGE(16, mock_device_line_count(),
        "a room is not sixteen runs");

    // And each interior run is one of the two cell dimensions, drawn from an
    // intersection: 48 steps across or 68 down, never anything between.
    for (int i = 8; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        float len = fabsf(l->x1 - l->x2) + fabsf(l->y1 - l->y2);
        char msg[128];
        snprintf(msg, sizeof(msg), "interior run %d is %g steps long", i - 7, (double)len);
        TEST_ASSERT_TRUE_MESSAGE(fabsf(len - 48.0f) < 0.5f || fabsf(len - 68.0f) < 0.5f, msg);
    }
}

// `draw.room` is what a doorway costs, and it must leave nothing of the room
// behind: `clean` and not `cs`, because `cs` restores automatic refresh and
// would quietly end every measurement this game takes after it.
void test_a_room_change_clears_the_last_room(void)
{
    run("setrefresh \"manual");
    in_room(2, 2);
    run("draw.room");

    mock_device_clear_graphics();
    run("go.room 1 0");

    TEST_ASSERT_EQUAL_INT_MESSAGE(16, mock_device_line_count(),
        "the new room drew over the old one instead of replacing it");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("manual", word_of("refreshmode"),
        "the room change put the refresh back to automatic");
}

//==========================================================================
// The readout under the picture
//==========================================================================

// The fifteen masks go on the screen beside the maze, because two rooms can
// look alike at a glance and their masks cannot.  That is what turns "walk out
// and back" from an impression into a reading.
void test_the_masks_are_on_the_screen_beside_the_maze(void)
{
    in_room(0, 0);
    mock_device_clear_output();
    run("show.room");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "ROOM"), screen);

    // Every mask, in the order the table holds them.
    float masks[15];
    read_cell(masks);
    const char *at = screen;
    for (int i = 0; i < 15; i++)
    {
        char want[8], msg[160];
        snprintf(want, sizeof(want), "%d ", (int)masks[i]);
        const char *found = strstr(at, want);
        snprintf(msg, sizeof(msg), "mask %d (%d) is not on the screen in order: %s",
                 i + 1, (int)masks[i], screen);
        TEST_ASSERT_NOT_NULL_MESSAGE(found, msg);
        at = found + strlen(want);
    }
}

// The worst transition is kept rather than averaged away, because a hitch is
// what a player notices and a mean is what hides it.
void test_the_worst_transition_is_kept(void)
{
    run("make \"worst.ms 0  make \"room.ms 0");
    in_room(0, 0);
    run("make \"worst.ms 40  go.room 1 0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(40.0f, num(":worst.ms"),
        "a quick room change threw the worst one away");

    run("make \"room.ms 0  make \"worst.ms 0  go.room 1 0");
    TEST_ASSERT_TRUE_MESSAGE(num(":worst.ms") >= num(":room.ms"),
        "the worst reading is below the last one");
}

//==========================================================================
// The keyboard
//==========================================================================

// ONE PRESS IS ONE ROOM.  This is the defect that moved the Snake Temple off
// key state: a tap lasts longer than a pass of a loop with nothing else to do,
// so `keydown?` reports the same press three or four times.  Reading the
// character stream makes the count exact -- three characters, three rooms,
// however long each key was held.
void test_one_press_is_one_room(void)
{
    in_room(0, 0);
    run("make \"leaving false");
    set_mock_input(K_RIGHT K_RIGHT K_RIGHT K_ESC);
    run("walk");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3.0f, num(":room.x"), "three presses were not three rooms");
    TEST_ASSERT_EQUAL_FLOAT(0.0f, num(":room.y"));
}

// All four arrows, and the sign on the vertical pair: up is ROOM_Y MINUS one,
// because arcade y runs down the screen and the exit table at $2157 sends
// `y < 2` to ROOM_Y - 1.  Getting that backwards is invisible in one room and
// wrong in every maze.
void test_the_four_arrows_move_the_four_ways(void)
{
    static const struct { const char *keys; float dx, dy; const char *way; } presses[] = {
        { K_RIGHT, 1.0f,  0.0f, "right" },
        { K_LEFT, -1.0f,  0.0f, "left" },
        { K_DOWN,  0.0f,  1.0f, "down" },
        { K_UP,    0.0f, -1.0f, "up" },
    };

    for (size_t i = 0; i < sizeof(presses) / sizeof(presses[0]); i++)
    {
        char input[8];
        in_room(10, 10);
        run("make \"leaving false");
        snprintf(input, sizeof(input), "%s%s", presses[i].keys, K_ESC);
        set_mock_input(input);
        run("walk");

        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f + presses[i].dx, num(":room.x"), presses[i].way);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f + presses[i].dy, num(":room.y"), presses[i].way);
    }
}

// A key that is not an arrow costs nothing at all, including the space bar,
// which fires from M4 and must not move the room now.
void test_a_key_that_is_not_an_arrow_stays_put(void)
{
    in_room(4, 4);
    run("make \"leaving false");
    set_mock_input("x z " K_ESC);
    run("walk");

    TEST_ASSERT_EQUAL_FLOAT(4.0f, num(":room.x"));
    TEST_ASSERT_EQUAL_FLOAT(4.0f, num(":room.y"));
}

// ESC is the way out, and it is the same key it is in Battlezone, because a
// key you press without thinking should not move between games.
void test_esc_leaves_the_walk(void)
{
    in_room(0, 0);
    run("make \"leaving false");
    set_mock_input(K_ESC K_RIGHT);
    run("walk");

    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word_of(":leaving"), "ESC did not end the walk");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":room.x"),
        "a key after ESC was still taken as a move");
}

//==========================================================================
// The session
//==========================================================================

// The whole entry point, driven to its exit.  It has to put the screen back
// the way it found it -- `setrefresh "auto` above all, because a session left
// in manual mode looks like a broken interpreter to whoever types the next
// command.
void test_berzerk_puts_the_screen_back(void)
{
    set_mock_input(K_RIGHT K_LEFT K_ESC);
    run("berzerk");

    TEST_ASSERT_EQUAL_STRING_MESSAGE("auto", word_of("refreshmode"),
        "the game left the display in manual refresh");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MOCK_SCREEN_TEXT, mock_device_get_state()->screen_mode,
        "the game left the split screen up");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":room.x"),
        "out and back did not come back");
}

// WINDOW, not the default `wrap`.  It costs nothing at this milestone -- the
// border is inside the screen either way -- and M2 cannot do without it: the
// man's contact box hangs four steps past him, and at the right-hand wall a
// wrapped probe lands on the far side of the screen.  P13 M0 was caught by
// exactly this.
void test_the_game_draws_in_window_mode(void)
{
    run("wrap");
    set_mock_input(K_ESC);
    run("berzerk");

    TEST_ASSERT_EQUAL_INT_MESSAGE(MOCK_BOUNDARY_WINDOW,
        mock_device_get_state()->turtle.boundary_mode,
        "the game did not set `window`");
}

// Section 18's first ceiling.  MAX_PROCEDURES is 128, Battlezone defines
// exactly 128 and the overflow is SILENT -- the last `to` in the file goes
// missing -- so this game's budget is 100 and the count is named rather than
// discovered at M6.
void test_the_game_is_inside_the_procedure_ceiling(void)
{
    // Counted out of the file rather than out of the workspace: there is no
    // operation that hands back the procedure table, and the file is what
    // `load` will be given on the board.
    FILE *f = fopen(BERZERK_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL(f);
    char line[512];
    int defs = 0;
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "to ", 3) == 0)
            defs++;
    fclose(f);

    char msg[128];
    snprintf(msg, sizeof(msg),
             "the game defines %d procedures, over the 100 budget", defs);
    TEST_ASSERT_TRUE_MESSAGE(defs <= 100, msg);
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_games_generator_is_the_roms_own_lcg);
    RUN_TEST(test_the_maze_is_a_function_of_the_room_coordinates);
    RUN_TEST(test_walking_out_and_back_is_the_same_room);
    RUN_TEST(test_the_room_coordinates_wrap_at_256);
    RUN_TEST(test_the_drawn_walls_agree_with_the_wall_masks);
    RUN_TEST(test_the_border_is_a_closed_circuit_with_four_doorways);
    RUN_TEST(test_a_room_is_eight_border_runs_and_eight_interior_ones);
    RUN_TEST(test_a_room_change_clears_the_last_room);
    RUN_TEST(test_the_masks_are_on_the_screen_beside_the_maze);
    RUN_TEST(test_the_worst_transition_is_kept);
    RUN_TEST(test_one_press_is_one_room);
    RUN_TEST(test_the_four_arrows_move_the_four_ways);
    RUN_TEST(test_a_key_that_is_not_an_arrow_stays_put);
    RUN_TEST(test_esc_leaves_the_walk);
    RUN_TEST(test_berzerk_puts_the_screen_back);
    RUN_TEST(test_the_game_draws_in_window_mode);
    RUN_TEST(test_the_game_is_inside_the_procedure_ceiling);
    return UNITY_END();
}
