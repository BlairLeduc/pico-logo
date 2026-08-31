//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for Berzerk (P15) M1 and M2 -- the room, and the man.  See
//  docs/berzerk-design.md sections 6, 7.1, 7.4, 8 and 19.
//
//  Nothing here checks a timing.  The host is 150-180x faster than the target
//  and `ticks` has millisecond resolution, so the figures the game puts on the
//  screen read as zero here; what a room and a frame cost is measured on a
//  board (docs/measurements/p15m0-bitmap-fast-pico2w-2026-08-29.md, and M1's
//  30 ms room change).
//
//  WHAT M1'S GATE ASKED is "walk out of a room and back into it and it is the
//  same room", and that is a claim about a pure function, which is exactly the
//  half a board cannot check.  A person walking out and back sees a maze that
//  LOOKS the same; these tests read the fifteen wall masks and the eight
//  segments back out and compare them number for number.
//
//  WHAT M2'S GATE ASKS is three sentences -- walls kill, doors work, and the
//  eight directions read right on the keyboard -- and all three are things a
//  person closes with their fingers on a board.  What the host adds is the
//  half that is invisible there:
//
//    * The room reproduces.  Section 6.1's whole claim, and the reason an
//      infinite maze costs two globals and no storage.
//    * THE DRAWN WALLS AGREE WITH THE WALL MASKS.  Section 5 has two
//      coordinate conventions (the disassembly's `WALLINDEX` takes H as x, and
//      its wall drawer takes H as the vertical axis), so a segment's geometry
//      and the bits it sets are computed down two different paths from the
//      same `rand & 3`.  If they ever disagree, the room draws correctly and
//      M3's robots path through a wall you can see.
//    * The coordinates wrap at 256 (section 22, Q3, taken at M1).
//    * THE MAN'S OUTLINE NEVER TOUCHES A WALL, which is the invariant M2's
//      erase-in-place stands on: the eraser covers exactly his 8 x 16 and the
//      walls are drawn once a room, so a man who can overlap ink is a man who
//      rubs holes in the maze.  Section 8.3's plus-or-minus four cannot hold
//      it and his own outline can.
//    * The four doors work, each carrying the other axis across, and the
//      border kills him everywhere else.
//    * The eight directions, including the four the DURL mask makes by adding
//      two arrows, and the arcade's one clever control at $1EEF.
//    * A frame spends no cells and no word-table bytes.  M0's first board run
//      died of exactly that (section 18, B52) and the host can see it.
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
#include "core/error.h"
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
    if (r.status == RESULT_NONE || r.status == RESULT_OK)
        return;
    // Say what the interpreter said, not just what was asked: a game this size
    // fails in one procedure and the line that called it is rarely the line
    // that is wrong.
    char msg[512];
    snprintf(msg, sizeof(msg), "%s -- %s", input, error_message(result_get_error_code(r)));
    TEST_FAIL_MESSAGE(msg);
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

// The keys, as `keydown?` names them.  The arrows have no printable
// character, so they arrive as codes of their own.
#define K_LEFT  180
#define K_UP    181
#define K_DOWN  182
#define K_RIGHT 183
#define K_ESC   177
#define K_SPACE 32
#define K_PAUSE 122

// The game reads key STATE from M2 on, so a test holds keys down rather than
// queueing characters.  A press also registers as a hit for the next
// `pollkeys`, which is how the driver reports one.
static void press(int key_code)   { set_mock_key_down(key_code, true); }
static void release(int key_code) { set_mock_key_down(key_code, false); }

// Put the man somewhere, facing nowhere, alive.
static void man_at(float x, float y)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "make \"p.x %g  make \"p.y %g  make \"p.dir 0  "
             "make \"p.dying 0  make \"p.face 90  make \"p.step 1",
             (double)x, (double)y);
    run(cmd);
}

// One pass of the game's own frame, with the keyboard in whatever state the
// test left it.  `play.frame` is the whole loop body, so nothing here reaches
// past what the game does on a board.
static void frame(void)
{
    run("play.frame");
}

// The man's outline, which is his contact box less the one step of margin:
// x from p.x to p.x + 8, y from p.y - 16 to p.y.
static void man_box(float box[4])
{
    box[0] = num(":p.x");
    box[1] = num(":p.x") + 8.0f;
    box[2] = num(":p.y") - 16.0f;
    box[3] = num(":p.y");
}

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
    run("draw.border 0 0");

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
    run("draw.walls 0 0");

    TEST_ASSERT_EQUAL_INT_MESSAGE(16, mock_device_line_count(),
        "a room is not sixteen runs");

    // And each interior run is a cell dimension, drawn from an intersection:
    // 48 steps across, 68 down, or the 52 that the leftmost column is wide
    // (B70 -- the playfield is 244 as 52 + 4 x 48, and a run west out of the
    // first intersection has to reach the wall).  Never anything between.
    for (int i = 8; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        float len = fabsf(l->x1 - l->x2) + fabsf(l->y1 - l->y2);
        char msg[128];
        snprintf(msg, sizeof(msg), "interior run %d is %g steps long", i - 7, (double)len);
        TEST_ASSERT_TRUE_MESSAGE(fabsf(len - 48.0f) < 0.5f || fabsf(len - 52.0f) < 0.5f ||
                                 fabsf(len - 68.0f) < 0.5f, msg);
    }
}

// A WALL RUNS FROM ONE GRID LINE TO THE NEXT, and nothing was checking it
// (B70).  `test_the_drawn_walls_agree_with_the_wall_masks` stands ten steps off
// each segment's MIDDLE, so a run that is the right shape in the right place
// and simply stops short passes it -- and one of them does.  The leftmost
// column is **52** steps wide, not 48: the playfield is 244 across as
// 52 + 4 x 48 (section 5, and the border drawer already knows it -- its top run
// is 100 + 48 + 96 rather than a symmetric 98 + 48 + 98).  A westward run of 48
// from the first intersection therefore ends four steps short of the left wall.
//
// The cabinet does not show this because its border wall is a FOUR-PIXEL
// SPRITE: `$2620` steps x back 48 and lays twelve 4-pixel sprites, covering
// arcade x 8 to 55, and the left wall occupies 4 to 7 -- contiguous.  Our
// border is a one-pixel pen line, so the span it has to reach is 52.  The
// right-hand side needs no such adjustment and has never been wrong: an
// eastward run from the last intersection ends exactly on the right wall.
void test_every_wall_runs_from_one_grid_line_to_the_next(void)
{
    // The lines a wall may start or end on: the four column boundaries plus
    // the two side walls, and the two row boundaries plus the top and bottom.
    static const float cols[6] = { -122.0f, -70.0f, -22.0f, 26.0f, 74.0f, 122.0f };
    static const float rows[4] = { 142.0f, 74.0f, 6.0f, -62.0f };

    for (int ry = 0; ry < 4; ry++)
        for (int rx = 0; rx < 4; rx++)
        {
            in_room(rx, ry);
            Segment s[8];
            read_segments(s);

            for (int k = 0; k < 8; k++)
            {
                bool horizontal = fabsf(s[k].h - 90.0f) < 0.5f || fabsf(s[k].h - 270.0f) < 0.5f;
                float dir = (fabsf(s[k].h - 270.0f) < 0.5f || fabsf(s[k].h - 180.0f) < 0.5f)
                                ? -1.0f : 1.0f;
                const float *lines = horizontal ? cols : rows;
                int n = horizontal ? 6 : 4;
                float from = horizontal ? s[k].x : s[k].y;
                float to = from + dir * s[k].l;

                bool from_ok = false, to_ok = false;
                for (int i = 0; i < n; i++)
                {
                    if (fabsf(from - lines[i]) < 0.5f) from_ok = true;
                    if (fabsf(to - lines[i]) < 0.5f) to_ok = true;
                }

                char msg[192];
                snprintf(msg, sizeof(msg),
                         "room %d,%d segment %d is %s, runs %g to %g and stops between "
                         "grid lines -- there is a gap at one end of it",
                         rx, ry, k + 1, horizontal ? "horizontal" : "vertical",
                         (double)from, (double)to);
                TEST_ASSERT_TRUE_MESSAGE(from_ok, msg);
                TEST_ASSERT_TRUE_MESSAGE(to_ok, msg);
            }
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
// and back" from an impression into a reading, and from M3 they are what the
// robots consult.
//
// THEY ARE DRAWN ON THE FRAME AFTER THE DOORWAY, NOT IN IT.  Text is not part
// of the graphics `sync`: `screen_putc` sends every character straight to the
// panel (`lcd_putc_attr` plus two cursor calls, over SPI), so the readout
// costs about a third of a millisecond a character wherever it runs.  Forty-
// five of them on the frame that also generates a room, clears the canvas,
// draws sixteen walls and presents the whole screen is what made a doorway a
// dropped frame on a board.  So `show.room` only marks them due and the next
// frame draws them -- 50 ms later, which nobody can see.
void test_a_doorway_defers_the_masks_to_the_next_frame(void)
{
    run("setrefresh \"manual");
    in_room(0, 0);
    man_at(-5, 45);
    run("make \"masks.due false");

    mock_device_clear_output();
    run("show.room");
    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "ROOM"), screen);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word_of(":masks.due"),
        "the room change did not ask for the masks");

    // Fifteen numbers are not on the screen yet: the room line is all of it.
    TEST_ASSERT_TRUE_MESSAGE(strlen(screen) < 32,
        "the doorway drew the masks in the frame it changed room in");

    // The next frame draws them, in the order the table holds them, and stops
    // asking.
    float masks[15];
    read_cell(masks);
    mock_device_clear_output();
    frame();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word_of(":masks.due"),
        "the masks were drawn twice");

    screen = mock_device_get_output();
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

    // And the frame after that draws nothing of them, so the readout is one
    // frame's cost and not a standing charge.
    mock_device_clear_output();
    frame();
    TEST_ASSERT_TRUE_MESSAGE(strlen(mock_device_get_output()) == 0,
        "the masks are redrawn every frame");
}

// ONE TEXT JOB A FRAME, and it is the rule the deferral above depends on.
// Text is not batched by `sync`, so the masks (45 characters) and the timing
// rows (48) landing in the same frame is two frames' worth of panel time in
// one -- and on a board that frame would be the new worst, worse than the
// doorway the deferral was built to fix.  So the masks take the frame after a
// doorway and the timing rows take the next free one.
void test_a_frame_writes_at_most_one_block_of_text(void)
{
    run("setrefresh \"manual");
    in_room(0, 0);
    man_at(-5, 45);

    // Force the collision the rule exists for: masks due AND the second's beat
    // falling on the same frame.
    run("make \"masks.due true  make \"frames 19");
    mock_device_clear_output();
    frame();
    const char *screen = mock_device_get_output();
    TEST_ASSERT_NULL_MESSAGE(strstr(screen, "FRAME"),
        "the masks and the timing rows were written in the same frame");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word_of(":masks.due"),
        "the masks did not take that frame");

    // And the beat is not owed forever: the next multiple of the rate takes it.
    run("make \"frames 19");
    mock_device_clear_output();
    frame();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(mock_device_get_output(), "FRAME"),
        "the timing rows never came back");
}

// THE WORST FRAME IS MEASURED BEFORE `sync`, not across it.  A frame that fits
// its budget is padded by `sync` to exactly the period, so a worst taken after
// it can only ever say 50 -- a board read `WORST` 51 at `fast`, which is one
// millisecond of clock granularity over the period and means only that nothing
// overran.  The body says how much of the 50 was used, so the margin is
// visible and an overrun still reads above 50.  Battlezone splits `body.ms`
// from `frame.ms` for the same reason.
void test_the_worst_frame_is_the_body_and_not_the_padded_period(void)
{
    run("setrefresh \"manual");
    in_room(9, 9);
    man_at(-5, 45);
    run("make \"worst.fr 0  make \"body.ms 0  make \"frame.ms 0");
    frame();

    TEST_ASSERT_TRUE_MESSAGE(num(":worst.fr") <= num(":body.ms"),
        "the worst frame is taken across `sync` and is therefore the period");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(num(":body.ms"), num(":worst.fr"),
        "the worst frame did not follow the body");
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
// The man is four costumes and not sixteen (design section 7.4)
//==========================================================================

// THE CEILING THE BITMAP DECISION INTRODUCED.  `COSTUME_SLOTS` is 15 and the
// cabinet's man alone is sixteen sprites -- standing, two walk cycles of three
// and eight shooting poses -- before a robot, an explosion or Otto.  M2's
// answer is `setrot "flip`: the ROM's own left-facing frames ($1391, $13A3,
// $13B5) are hand-mirrored copies of its right-facing ones, so the engine can
// make them and the slots go to the robots instead.
//
// Four captured here, and each 8 by 16 -- `snapsh` and not `putsh`, because
// `putsh` doubles every pixel horizontally on the way in and a 16-wide man
// takes the playfield off section 5's 1:1.
void test_the_man_is_four_costumes_at_the_cabinets_size(void)
{
    int before = mock_device_get_state()->costume.snap_count;
    run("setrefresh \"manual  cache.man");
    const MockDeviceState *st = mock_device_get_state();

    char msg[96];
    snprintf(msg, sizeof(msg), "captured %d costumes, not four",
             st->costume.snap_count - before);
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, st->costume.snap_count - before, msg);

    TEST_ASSERT_EQUAL_INT_MESSAGE(8, st->costume.last_snap_w,
        "the man is not eight pixels wide, so the playfield is not 1:1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(16, st->costume.last_snap_h,
        "the man is not sixteen rows tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, st->costume.last_snap_slot,
        "the four costumes are not slots 1 to 4");
}

// THE RENDERER HAS TO PUT THE BITMAP WHERE `snapsh` WILL LOOK FOR IT.  M0 lost
// a day to models that drew the right number of segments in the wrong-sized
// box, and a costume captured around the wrong centre lands half a body from
// where the walls test him.  So: render one sprite at a known corner and check
// that every stroke is inside its 8 by 16, which is what the capture offset of
// (x + 3.5, y - 7.5) assumes.
void test_the_renderer_fills_the_cabinets_8_by_16(void)
{
    run("clean");
    mock_device_clear_graphics();
    run("render.sprite :mb1 0 0");

    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > 0, "the man rendered nothing");
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        char msg[128];
        snprintf(msg, sizeof(msg), "stroke %d runs %g,%g to %g,%g", i,
                 (double)l->x1, (double)l->y1, (double)l->x2, (double)l->y2);
        TEST_ASSERT_TRUE_MESSAGE(l->x1 >= -0.01f && l->x2 <= 8.01f, msg);
        TEST_ASSERT_TRUE_MESSAGE(l->y1 <= 0.01f && l->y1 >= -15.01f, msg);
        TEST_ASSERT_TRUE_MESSAGE(l->y2 <= 0.01f && l->y2 >= -15.01f, msg);
    }
}

// The whole point of the flip: walking east and walking west wear the SAME
// slot and differ only in the heading.  A second costume here would be a slot
// M3's robots need.
void test_both_sides_of_the_man_are_one_costume(void)
{
    in_room(0, 0);
    run("setrot \"flip");

    man_at(-5, 45);
    press(K_RIGHT);
    frame();
    release(K_RIGHT);
    int east_slot = mock_device_get_stamp(mock_device_stamp_count() - 1)->shape;
    float east_h = num("heading");

    man_at(-5, 45);
    press(K_LEFT);
    frame();
    release(K_LEFT);
    int west_slot = mock_device_get_stamp(mock_device_stamp_count() - 1)->shape;
    float west_h = num("heading");

    TEST_ASSERT_EQUAL_INT_MESSAGE(east_slot, west_slot,
        "the man walking west wears a costume of his own");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(90.0f, east_h, "walking east is not heading 90");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(270.0f, west_h, "walking west is not heading 270");
}

// A costume is centred on the turtle at both ends -- capture and stamp -- while
// the man's stored position is his sprite's TOP-LEFT corner, which is the
// arcade's and what every box in the file is cut against.  An 8 x 16 sprite
// drawn from (x, y) downward has its centre at (x + 3.5, y - 7.5); getting
// that offset wrong draws him half a body from where the walls test him.
void test_the_man_stamps_half_a_sprite_from_his_stored_corner(void)
{
    in_room(0, 0);
    man_at(-5, 45);
    mock_device_clear_graphics();
    run("draw.man");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, mock_device_stamp_count(),
        "the man is not exactly one stamp");
    const MockStamp *st = mock_device_get_stamp(0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.5f, st->x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 37.5f, st->y);
}

// Standing is its own frame in the ROM ($1046) and the walk is the four-entry
// cycle A B C B at $104B, whose distinct frames are $10AD, $109B and $1089.
// A cycle that ran 2 3 4 2 3 4 would still animate and would not be the
// cabinet's gait.
void test_the_walk_is_the_roms_a_b_c_b_and_standing_is_its_own_frame(void)
{
    in_room(0, 0);
    man_at(-5, 45);

    mock_device_clear_graphics();
    run("draw.man");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, mock_device_get_stamp(0)->shape,
        "a standing man is not slot 1");

    static const int cycle[8] = { 3, 4, 3, 2, 3, 4, 3, 2 };
    press(K_RIGHT);
    for (int i = 0; i < 8; i++)
    {
        man_at(-5, 45);
        run("make \"p.step 1");
        // Advance the cycle by i+1 steps, then draw.
        for (int k = 0; k <= i; k++)
            run("make \"p.dir 2  step.man");
        mock_device_clear_graphics();
        run("draw.man");

        char msg[96];
        snprintf(msg, sizeof(msg), "walk step %d wore slot %d, not %d",
                 i + 1, mock_device_get_stamp(0)->shape, cycle[i]);
        TEST_ASSERT_EQUAL_INT_MESSAGE(cycle[i], mock_device_get_stamp(0)->shape, msg);
    }
    release(K_RIGHT);
}

//==========================================================================
// The gate, part one: the eight directions read right on the keyboard
//==========================================================================

// The arcade's four DURL bits are a MASK, so two arrows held at once are a
// diagonal and the table lookup adds them for nothing.  Turtle y runs the
// other way from the cabinet's, so UP is +1 here -- getting that sign
// backwards is invisible in one room and wrong in every maze.
//
// The step is 1.5, which is section 8.1's arithmetic: the player's TPRIME is 2
// ($2004), so he moves a pixel every other tick at 60 Hz, and one of our
// frames at 20 fps is three of the cabinet's.  A diagonal is 1.5 in EACH axis
// and therefore faster, which is what the cabinet does too.
void test_the_eight_directions_read_right_on_the_keyboard(void)
{
    static const struct {
        int a, b; float dx, dy; float face; const char *way;
    } ways[] = {
        { K_LEFT,  0,       -1.5f,  0.0f, 270.0f, "left" },
        { K_RIGHT, 0,        1.5f,  0.0f,  90.0f, "right" },
        { K_UP,    0,        0.0f,  1.5f,  90.0f, "up" },
        { K_DOWN,  0,        0.0f, -1.5f,  90.0f, "down" },
        { K_LEFT,  K_UP,    -1.5f,  1.5f, 270.0f, "up and left" },
        { K_RIGHT, K_UP,     1.5f,  1.5f,  90.0f, "up and right" },
        { K_LEFT,  K_DOWN,  -1.5f, -1.5f, 270.0f, "down and left" },
        { K_RIGHT, K_DOWN,   1.5f, -1.5f,  90.0f, "down and right" },
    };

    in_room(0, 0);
    for (size_t i = 0; i < sizeof(ways) / sizeof(ways[0]); i++)
    {
        man_at(-5, 45);
        press(ways[i].a);
        if (ways[i].b) press(ways[i].b);
        frame();
        release(ways[i].a);
        if (ways[i].b) release(ways[i].b);

        char msg[128];
        snprintf(msg, sizeof(msg), "%s moved to %g,%g", ways[i].way,
                 (double)num(":p.x"), (double)num(":p.y"));
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, -5.0f + ways[i].dx, num(":p.x"), msg);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 45.0f + ways[i].dy, num(":p.y"), msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(ways[i].face, num(":p.face"), ways[i].way);
    }
}

// The four masks that name no axis are the arcade's own answers at $2042 and
// not padding: LEFT + RIGHT is "no move", and the man stands.
void test_opposite_arrows_stand_him_still(void)
{
    in_room(0, 0);
    man_at(-5, 45);
    press(K_LEFT);
    press(K_RIGHT);
    frame();
    release(K_LEFT);
    release(K_RIGHT);

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-5.0f, num(":p.x"), "left and right together moved him");
    TEST_ASSERT_EQUAL_FLOAT(45.0f, num(":p.y"));
}

// THE ARCADE'S ONE CLEVER CONTROL, from the 2600 manual and visible at $1EEF:
// "if you depress the fire button while moving the Joystick, your man will
// stand and fire lasers in any direction you move the Joystick."  The fire
// test comes BEFORE the movement test and FIRE ($1F33) zeroes both velocities.
// The bolt is M4's; what M2 owes it is `p.fire` and a man who does not walk.
void test_space_with_a_direction_stands_him_still_and_aims(void)
{
    in_room(0, 0);
    man_at(-5, 45);
    press(K_SPACE);
    press(K_UP);
    frame();

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-5.0f, num(":p.x"), "he walked while firing");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(45.0f, num(":p.y"), "he walked while firing");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4.0f, num(":p.fire"), "SPACE plus up did not aim up");

    // And the direction still turns him, so the aim is visible before M4's
    // bolt exists.
    press(K_LEFT);
    frame();
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(270.0f, num(":p.face"), "aiming left did not turn him");
    release(K_SPACE);
    release(K_UP);
    release(K_LEFT);
}

//==========================================================================
// The gate, part two: walls kill
//==========================================================================

// An INTERIOR wall, through the mask table -- which cell am I in, is the edge
// I am crossing walled (section 6.3), four statements and no pixels.  The room
// is chosen by reading the table rather than by hand, because the maze is a
// function of the coordinates and a hand-picked room would be a hand-picked
// generator too.
void test_an_interior_wall_kills_him(void)
{
    int found = -1;
    for (int r = 0; r < 64 && found < 0; r++)
    {
        in_room(r, 0);
        // Cell (row 1, column 1) is index 7, and bit 4 is its TOP.
        if ((mask_at(-46, 40) & 4) != 0)
            found = r;
    }
    TEST_ASSERT_TRUE_MESSAGE(found >= 0, "no room in 64 walls the top of cell (1,1)");

    in_room(found, 0);
    man_at(-50, 40);
    press(K_UP);
    for (int i = 0; i < 40 && num(":p.dying") == 0.0f; i++)
        frame();
    release(K_UP);

    TEST_ASSERT_TRUE_MESSAGE(num(":p.dying") > 0.0f,
        "he walked through a wall the mask table says is there");
    TEST_ASSERT_TRUE_MESSAGE(num(":p.y") + 1.0f <= 74.0f,
        "he died on the far side of the wall, so the test was crossed too late");
}

// Mask of the cell at (row, column), through the game's own `cell.at`.
static int mask_of(int row, int col)
{
    return mask_at((float)(-94 + 48 * col), (float)(108 - 68 * row));
}

// THE END OF A WALL IS STILL A WALL, and a crossing test cannot see it (B69).
// A wall test asked as "which cell am I in, is the edge I am crossing walled"
// only fires when the man crosses the boundary the wall LIES ON -- so walking
// PARALLEL to a wall and stepping into the cell at its end crosses nothing,
// and he ends up standing on the drawn wall, alive.  Reported from a board:
// "if the wall is vertical man can walk on the wall vertically from the
// start/end without dying."
//
// The cabinet has no such hole because it has no cells: it XORs the sprite
// into video RAM and a wall pixel under a man pixel raises the intercept bit.
// So the port's question has to be OCCUPANCY -- may his box be here -- and not
// crossing.
//
// The room is chosen by reading the table rather than by hand: one where the
// vertical wall at x = -70 exists in row 0 and NOT in row 1, so the wall ends
// at the row boundary, and where no horizontal wall runs along that boundary
// to kill him for the wrong reason.
void test_the_end_of_a_vertical_wall_kills_him(void)
{
    int found = -1;
    for (int r = 0; r < 200 && found < 0; r++)
    {
        in_room(r, 0);
        if ((mask_of(0, 0) & 2) && !(mask_of(1, 0) & 2) &&
            !(mask_of(0, 0) & 8) && !(mask_of(0, 1) & 8))
            found = r;
    }
    TEST_ASSERT_TRUE_MESSAGE(found >= 0, "no room in 200 ends a vertical wall at x = -70");

    in_room(found, 0);
    // Astride the wall's line, one row below where the wall starts.
    man_at(-74, 50);
    press(K_UP);
    for (int i = 0; i < 40 && num(":p.dying") == 0.0f; i++)
        frame();
    release(K_UP);

    char msg[160];
    snprintf(msg, sizeof(msg),
             "in room %d he walked up to y=%g astride the wall at x=-70 and lived",
             found, (double)num(":p.y"));
    TEST_ASSERT_TRUE_MESSAGE(num(":p.dying") > 0.0f, msg);
    TEST_ASSERT_TRUE_MESSAGE(num(":p.y") + 1.0f <= 74.0f,
        "he died only after his box was already over the wall");
}

// The same hole in the other axis: a horizontal wall that ends at a column
// boundary, walked into along its own line.
void test_the_end_of_a_horizontal_wall_kills_him(void)
{
    int found = -1;
    for (int r = 0; r < 200 && found < 0; r++)
    {
        in_room(r, 0);
        if ((mask_of(0, 1) & 8) && !(mask_of(0, 2) & 8) &&
            !(mask_of(0, 1) & 2) && !(mask_of(1, 1) & 2))
            found = r;
    }
    TEST_ASSERT_TRUE_MESSAGE(found >= 0, "no room in 200 ends a horizontal wall at y = 74");

    in_room(found, 0);
    // Astride the wall's line, one column to the right of where it starts.
    man_at(0, 80);
    press(K_LEFT);
    for (int i = 0; i < 40 && num(":p.dying") == 0.0f; i++)
        frame();
    release(K_LEFT);

    char msg[160];
    snprintf(msg, sizeof(msg),
             "in room %d he walked left to x=%g astride the wall at y=74 and lived",
             found, (double)num(":p.x"));
    TEST_ASSERT_TRUE_MESSAGE(num(":p.dying") > 0.0f, msg);
    TEST_ASSERT_TRUE_MESSAGE(num(":p.x") - 1.0f >= -22.0f,
        "he died only after his box was already over the wall");
}

// THE BORDER IS A POSITION TEST and not a table lookup, because `cell.at`
// clamps to the grid: every point left of the playfield is in column 0, so no
// crossing ever happens there.  That is right rather than a gap -- the mask
// table carries the border walls at all four doorways, because it is what
// ROBOTS consult and robots never leave the room.  Reading it for the player
// would wall him into a room with four visible doors.
void test_the_border_kills_him_where_there_is_no_doorway(void)
{
    in_room(0, 0);
    // Column 3 of the top row: inside the playfield, nowhere near the top
    // doorway, which is column 2 only.
    man_at(40, 130);
    press(K_UP);
    for (int i = 0; i < 40 && num(":p.dying") == 0.0f; i++)
        frame();
    release(K_UP);

    TEST_ASSERT_TRUE_MESSAGE(num(":p.dying") > 0.0f, "he walked out through the top wall");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":room.y"),
        "walking into the top wall changed the room");
}

// THE INVARIANT THE ERASE STANDS ON.  The walls are drawn once a room and the
// man is rubbed out every frame with an 8-wide background stroke that covers
// exactly his 8 x 16, so a man whose outline can overlap ink is a man who eats
// holes in the maze -- and nothing puts them back until the next doorway.
// Section 8.3's plus-or-minus four lets him stand four steps inside a
// horizontal wall; his own outline, grown one step, cannot.
//
// Driven the way a player drives it: eight directions in turn, through rooms,
// against the segments the game actually DREW rather than against the table.
// That distinction is the doorways -- his box passes the border's x and y and
// must, because the drawn wall has a hole there.
void test_the_mans_outline_never_touches_a_wall(void)
{
    static const int ways[8][2] = {
        { K_RIGHT, 0 }, { K_RIGHT, K_UP }, { K_UP, 0 }, { K_LEFT, K_UP },
        { K_LEFT, 0 }, { K_LEFT, K_DOWN }, { K_DOWN, 0 }, { K_RIGHT, K_DOWN },
    };
    typedef struct { float x1, y1, x2, y2; } Wall;
    Wall walls[32];
    int nw = 0;

    run("setrefresh \"manual");
    in_room(7, 11);
    run("reset.man");
    float room_x = num(":room.x"), room_y = num(":room.y");

    for (int i = 0; i < 480; i++)
    {
        if (nw == 0 || room_x != num(":room.x") || room_y != num(":room.y"))
        {
            room_x = num(":room.x");
            room_y = num(":room.y");
            run("clean");
            mock_device_clear_graphics();
            run("draw.walls 0 0");
            nw = mock_device_line_count();
            TEST_ASSERT_EQUAL_INT(16, nw);
            for (int w = 0; w < nw; w++)
            {
                const MockLine *l = mock_device_get_line(w);
                walls[w].x1 = fminf(l->x1, l->x2);
                walls[w].x2 = fmaxf(l->x1, l->x2);
                walls[w].y1 = fminf(l->y1, l->y2);
                walls[w].y2 = fmaxf(l->y1, l->y2);
            }
        }

        const int *way = ways[(i / 15) % 8];
        press(way[0]);
        if (way[1]) press(way[1]);
        frame();
        release(way[0]);
        if (way[1]) release(way[1]);

        float box[4];
        man_box(box);
        for (int w = 0; w < nw; w++)
        {
            bool hit = box[0] <= walls[w].x2 && walls[w].x1 <= box[1] &&
                       box[2] <= walls[w].y2 && walls[w].y1 <= box[3];
            char msg[192];
            snprintf(msg, sizeof(msg),
                     "frame %d in room %g,%g: his outline %g..%g by %g..%g "
                     "overlaps the wall %g,%g to %g,%g",
                     i, (double)room_x, (double)room_y,
                     (double)box[0], (double)box[1], (double)box[2], (double)box[3],
                     (double)walls[w].x1, (double)walls[w].y1,
                     (double)walls[w].x2, (double)walls[w].y2);
            TEST_ASSERT_FALSE_MESSAGE(hit, msg);
        }
    }
}

// THE ERASE HAS TO COVER THE PIXELS THE STAMP ACTUALLY WROTE, and B67 is what
// happens when it nearly does.  The game shipped one pen-8 stroke down the
// man's spine, inset by what section 7.1 calls the cap radius -- and a wide
// pen in this interpreter is a filled DISC, so the cap is a semicircle that
// pinches to a single pixel rather than a square extension.  Seventeen of his
// 128 pixels were never erased, his top four rows and his bottom three, and a
// board reported him dragging a trail in every direction.
//
// So the eraser is three strokes of PEN 3, the one wide pen that is an exact
// square (radius 1.5, extent 1, corner (1,1) = 2 against 2.25).
// tests/test_screen_pen.c pins that against the real rasteriser; what is
// checked here is that the game aims them at the right pixels.
//
// AND AT HALF POSITIONS TOO.  He moves 1.5 steps a frame, so half his
// positions are half-pixels, and `stamp` puts the costume at
// `round(centre) - w/2` while a stroke rounds its own endpoints -- two
// different roundings of the same number.  `draw.man` records the rounded
// position it drew at and the erase reads that back, which is what makes the
// two agree; a test that only stood him on whole steps would not see it.

// Turtle to screen, the way picocalc_console.c does it: x + WIDTH/2, and y
// flipped about HEIGHT/2.
#define SCR_X(tx) ((int)((tx) + 160.0f + 0.5f))
#define SCR_Y(ty) ((int)(-(ty) + 120.0f + 0.5f))

// Screen pixels the recorded erase strokes painted.  Only axis-aligned pen-3
// strokes are understood, and both facts are asserted: the square rule is
// valid for pen 3 and for no other width.
static void erase_coverage(bool out[240][320])
{
    memset(out, 0, sizeof(bool) * 240 * 320);
    for (int i = 0; i < mock_device_line_count(); i++)
    {
        const MockLine *l = mock_device_get_line(i);
        TEST_ASSERT_EQUAL_INT_MESSAGE(3, l->pen_size,
            "the eraser is not pen 3, so it is not a square brush (B67)");
        int x1 = SCR_X(l->x1), x2 = SCR_X(l->x2);
        int y1 = SCR_Y(l->y1), y2 = SCR_Y(l->y2);
        TEST_ASSERT_TRUE_MESSAGE(x1 == x2 || y1 == y2, "the erase stroke is not axis-aligned");
        for (int y = (y1 < y2 ? y1 : y2); y <= (y1 < y2 ? y2 : y1); y++)
            for (int x = (x1 < x2 ? x1 : x2); x <= (x1 < x2 ? x2 : x1); x++)
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                        if (x + ox >= 0 && x + ox < 320 && y + oy >= 0 && y + oy < 240)
                            out[y + oy][x + ox] = true;
    }
}

void test_the_erase_covers_every_pixel_the_stamp_wrote(void)
{
    static const float where[6][2] = {
        { -5.0f, 45.0f }, { -5.5f, 45.0f }, { -5.0f, 44.5f },
        { -5.5f, 44.5f }, { 40.5f, -20.5f }, { -96.0f, 42.0f },
    };
    static bool covered[240][320];

    in_room(0, 0);
    for (size_t w = 0; w < sizeof(where) / sizeof(where[0]); w++)
    {
        man_at(where[w][0], where[w][1]);

        // Draw him, then erase him, exactly as a frame does.
        mock_device_clear_graphics();
        run("draw.man");
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, mock_device_stamp_count(), "the man is not one stamp");
        const MockStamp *st = mock_device_get_stamp(0);
        // `stamp` puts the costume's top-left at round(centre) - size/2.
        int sx0 = SCR_X(st->x) - 4, sy0 = SCR_Y(st->y) - 8;

        mock_device_clear_graphics();
        run("erase.man");
        TEST_ASSERT_EQUAL_INT_MESSAGE(3, mock_device_line_count(),
            "the erase is not three strokes");
        erase_coverage(covered);

        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 8; x++)
            {
                char msg[160];
                snprintf(msg, sizeof(msg),
                         "at %g,%g the man's pixel %d,%d survives the erase -- "
                         "he leaves a trail (B67)",
                         (double)where[w][0], (double)where[w][1], x, y);
                TEST_ASSERT_TRUE_MESSAGE(covered[sy0 + y][sx0 + x], msg);
            }
    }
}

// AND NOT ONE PIXEL MORE.  The walls are drawn once a room, so an eraser that
// spills does not smudge -- it eats a hole nothing repaints until the next
// doorway.  Three pen-3 strokes are an exact rectangle, so this is a stronger
// guarantee than the one-step margin section 8.3 carries: the eraser cannot
// reach a wall even if the man is standing against one.
void test_the_erase_touches_nothing_outside_the_man(void)
{
    static bool covered[240][320];

    in_room(0, 0);
    man_at(-5, 45);
    mock_device_clear_graphics();
    run("draw.man");
    const MockStamp *st = mock_device_get_stamp(0);
    int sx0 = SCR_X(st->x) - 4, sy0 = SCR_Y(st->y) - 8;

    mock_device_clear_graphics();
    run("erase.man");
    erase_coverage(covered);

    for (int y = sy0 - 3; y < sy0 + 19; y++)
        for (int x = sx0 - 3; x < sx0 + 11; x++)
        {
            if (x >= sx0 && x < sx0 + 8 && y >= sy0 && y < sy0 + 16)
                continue;
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "the eraser painted %d,%d, which is outside the man and may be a wall",
                     x - sx0, y - sy0);
            TEST_ASSERT_FALSE_MESSAGE(covered[y][x], msg);
        }

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, (int)num("pensize"),
        "the eraser left the pen three wide, so the next wall is a slab");
}

//==========================================================================
// The gate, part three: doors work
//==========================================================================

// The exits are a position test at $2157 and the constants transfer verbatim
// through section 5's two lines.  THE OTHER AXIS IS PRESERVED, so you come in
// where you went out -- and he re-enters at the arcade's own coordinate, not
// at the doorway's near edge.
void test_the_four_doors_work(void)
{
    static const struct {
        float x, y; int key; float dx, dy; float in_x, in_y; const char *way;
    } doors[] = {
        { -96,  42, K_LEFT,  -1,  0,  104,   42, "the left door" },
        { 100,  42, K_RIGHT,  1,  0, -118,   42, "the right door" },
        {   0, 130, K_UP,     0, -1,    0,  -43, "the top door" },
        {   0, -30, K_DOWN,   0,  1,    0,  136, "the bottom door" },
    };

    for (size_t i = 0; i < sizeof(doors) / sizeof(doors[0]); i++)
    {
        in_room(9, 9);
        man_at(doors[i].x, doors[i].y);
        press(doors[i].key);
        for (int f = 0; f < 60 && num(":room.x") == 9.0f && num(":room.y") == 9.0f; f++)
            frame();
        release(doors[i].key);

        char msg[128];
        snprintf(msg, sizeof(msg), "%s did not change the room", doors[i].way);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9.0f + doors[i].dx, num(":room.x"), msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9.0f + doors[i].dy, num(":room.y"), msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":p.dying"), "a doorway killed him");

        snprintf(msg, sizeof(msg), "%s put him back at %g,%g", doors[i].way,
                 (double)num(":p.x"), (double)num(":p.y"));
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, doors[i].in_x, num(":p.x"), msg);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, doors[i].in_y, num(":p.y"), msg);
    }
}

// The room is built INSIDE the frame he walks in -- M1 measured a room change
// at 30 ms at `fast` against section 15.3's 50 ms frame, so there is no need
// to spread the build over two frames.  The proof is that the frame he leaves
// in ends with the new room's sixteen runs on the canvas and not with the old
// room's.
void test_a_doorway_rebuilds_the_room_inside_the_frame(void)
{
    run("setrefresh \"manual");
    in_room(9, 9);
    man_at(-129, 42);
    press(K_LEFT);
    mock_device_clear_graphics();
    frame();
    release(K_LEFT);

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(8.0f, num(":room.x"), "he did not leave");
    TEST_ASSERT_EQUAL_INT_MESSAGE(16, mock_device_line_count(),
        "the frame he left in did not draw the new room");
}

// ERASE IN PLACE (section 3, settled once the figures became stamps): an
// ordinary frame draws no wall at all.  The maze is static and there is one
// moving figure, so clear-and-redraw would repaint sixteen runs to erase eight
// steps of man -- and would present the whole canvas to do it.
void test_an_ordinary_frame_does_not_redraw_the_maze(void)
{
    run("setrefresh \"manual");
    in_room(9, 9);
    man_at(-5, 45);
    press(K_RIGHT);
    mock_device_clear_graphics();
    frame();
    release(K_RIGHT);

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, mock_device_line_count(),
        "the frame drew more than the three erase strokes");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, mock_device_stamp_count(),
        "the frame stamped more than the man");
}

//==========================================================================
// The death, and the frame itself
//==========================================================================

// The arcade holds for 45 ticks after the electrocution ($1FB6), which is
// fifteen frames at 20 fps, and he flickers rather than wobbling because
// section 7.6's `PlayerFriedScales` wants five scales and `setmag` has two.
// That is M7's, with the rest of the deaths.  Lives are M6's, so he comes back
// where a new game starts him: DEFAULT_PLAYER_STATE ($187F) is arcade
// (30, 100), which is turtle (-96, 42).
void test_a_death_holds_for_the_arcades_45_ticks_and_then_respawns(void)
{
    in_room(0, 0);
    man_at(40, 130);
    press(K_UP);
    for (int i = 0; i < 40 && num(":p.dying") == 0.0f; i++)
        frame();
    release(K_UP);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(15.0f, num(":p.dying"),
        "the pause is not the arcade's 45 ticks at 20 fps");

    // HIS OWN COSTUMES AND NOT EVERY STAMP.  Slots 1 to 4 are the man and
    // 10 to 13 are the robots, and from M3 the last frame of a death rebuilds
    // the room -- so counting stamps would count the crowd that came back
    // with him.
    int stamps = 0;
    for (int i = 0; i < 15; i++)
    {
        mock_device_clear_graphics();
        frame();
        for (int s = 0; s < mock_device_stamp_count(); s++)
            if (mock_device_get_stamp(s)->shape <= 4)
                stamps++;
    }
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":p.dying"), "the death did not end");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, -96.0f, num(":p.x"), "he came back somewhere else");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 42.0f, num(":p.y"), "he came back somewhere else");

    // He flickers: stamped on some of those frames and not on all of them.
    TEST_ASSERT_TRUE_MESSAGE(stamps > 0 && stamps < 15,
        "a dying man is either always drawn or never drawn, so he does not flicker");
}

// SECTION 18'S FOURTH CEILING, and the one M0 found on a board rather than in
// a budget: nothing in this interpreter collects on demand, so a frame that
// spends storage has a fuse on it.  `dot`, `setpos [x y]`, `list` and `se` all
// cons a cell, and `.setitem` of a number the workspace has not held before
// interns a word (B52).  M0's harness did both and died `out of space in
// rob.left` on its first board run.
//
// Warm first: the frame's first pass mints whatever words it is going to mint.
// Nineteen frames is deliberately one short of the readout's period --
// `show.timing` DOES spend, about fifteen cells, and hands them straight back
// with `recycle` -- so what is measured here is the frame the game spends
// nineteen twentieths of its life in.
void test_an_ordinary_frame_spends_no_cells(void)
{
    run("setrefresh \"manual");
    in_room(9, 9);
    man_at(-5, 45);
    press(K_RIGHT);
    for (int i = 0; i < 60; i++)  // warm
        frame();

    run("make \"frames 0");
    float nodes0 = num("nodes");
    for (int i = 0; i < 19; i++)
        frame();
    release(K_RIGHT);

    char msg[128];
    snprintf(msg, sizeof(msg), "nineteen frames spent %g cells",
             (double)(nodes0 - num("nodes")));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(nodes0, num("nodes"), msg);
}

// AND THE WORD TABLE CONVERGES, which is the half B25 says kills a frame loop:
// M0's harness died with 21,000 free cells and 20 free bytes of word table, so
// a test that watches `nodes` alone watches the half that is fine.
//
// The claim is bounded, not zero-per-frame, and the difference is the whole
// point.  A frame that spends a handful of bytes and settles is fine; one that
// spends a handful and keeps spending has a fuse on it however long the fuse
// is, which is what M0's unbounded `r.time` counter was.  So: walk him in
// every direction, through rooms and into walls, for four thousand frames, and
// read the free word table at every thousand.
//
// IT WOBBLES RATHER THAN SETTLING FROM M3, and the wobble is `recycle` rather
// than a leak.  M2 could assert exact equality because a death reset him in
// place; from M3 a death REBUILDS THE ROOM (the arcade's own behaviour, and the
// only thing that stops him respawning inside the robot that killed him), so a
// death frame spends what a doorway spends and hands it back the way the
// readout does.  A single reading taken mid-transient sits up to ~600 bytes
// below the quiescent level, which is why this compares PEAKS across halves of
// the run: a peak is a reading taken after a `recycle` and before the next
// spend, and it is the level the table actually settles at.  Twenty thousand
// frames of this hold ~25,180 either way.  What no wobble can hide is a trend:
// one word a frame is four thousand words over this run, against a 256-BYTE
// bound.
void test_the_word_table_converges(void)
{
    static const int ways[8][2] = {
        { K_RIGHT, 0 }, { K_RIGHT, K_UP }, { K_UP, 0 }, { K_LEFT, K_UP },
        { K_LEFT, 0 }, { K_LEFT, K_DOWN }, { K_DOWN, 0 }, { K_RIGHT, K_DOWN },
    };
    run("setrefresh \"manual");
    in_room(9, 9);
    run("reset.man");

    float early = 0.0f, late = 0.0f;
    for (int i = 0; i < 8000; i++)
    {
        if (i % 500 == 0)
        {
            float a = num("atoms");
            if (i >= 500  && i < 4000 && a > early) early = a;
            if (i >= 4000 && a > late)              late  = a;
        }
        const int *way = ways[(i / 11) % 8];
        press(way[0]);
        if (way[1]) press(way[1]);
        frame();
        release(way[0]);
        if (way[1]) release(way[1]);
    }

    char msg[224];
    snprintf(msg, sizeof(msg),
             "the free word table peaked at %g in the second half against %g in "
             "the first, so the set of numbers the game can hold is not closed",
             (double)late, (double)early);
    TEST_ASSERT_TRUE_MESSAGE(late > early - 256.0f, msg);
}

//==========================================================================
// The keyboard, and the session
//==========================================================================

// Z pauses, and a paused frame moves nothing.  `poll.input` runs OUTSIDE the
// paused guard, or a paused game could never read the key that unpauses it --
// which is the defect both shipped shooters had.  `keyhit?` and not
// `keydown?`, or holding Z would toggle the pause twenty times a second.
void test_pause_stops_the_man_and_can_be_lifted(void)
{
    in_room(0, 0);
    man_at(-5, 45);
    set_mock_key_tap(K_PAUSE);
    frame();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word_of(":paused"), "Z did not pause");

    press(K_RIGHT);
    frame();
    frame();
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-5.0f, num(":p.x"), "a paused man walked");

    set_mock_key_tap(K_PAUSE);
    frame();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word_of(":paused"), "Z did not unpause");
    frame();
    release(K_RIGHT);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, -3.5f, num(":p.x"), "he did not start again");
}

// ESC is the way out, and it is the same key it is in Battlezone and in M1,
// because a key you press without thinking should not move between games.
// `keydown?` and not `keyhit?`: the loop's first act is to poll, so a key
// already down when the game starts has had its hit consumed, and quit is the
// one control where answering twice costs nothing.
void test_esc_leaves_the_game(void)
{
    in_room(0, 0);
    man_at(-5, 45);
    run("make \"leaving false");
    press(K_ESC);
    press(K_RIGHT);
    frame();
    release(K_ESC);
    release(K_RIGHT);

    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word_of(":leaving"), "ESC did not end the game");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-5.0f, num(":p.x"),
        "the frame ESC arrived in still walked him");
}

// THE CADENCE IS SEEDED OUTSIDE THE LOOP, and the reason is a measurement
// rather than a hitch.  `frame_sync_wait_ms` has no baseline on its first call,
// so it seeds at `now` and puts the first boundary one period out -- core's own
// `test_first_wait_seeds_full_period` says exactly that.  A frame timed across
// its own `sync` therefore reads its work plus a whole period, and a board read
// `WORST` at 98 ms at `fast` and 118 at `normal` because of it: both were the
// FIRST frame, 48 and 68 ms of real work with the 50 ms seed on top.
//
// One `sync` before the loop absorbs the seed where nothing is timing it -- and
// puts the room on the screen at once rather than a frame later.
void test_the_frame_cadence_is_seeded_before_the_loop(void)
{
    press(K_ESC);
    run("init.game  (setrefresh \"sync :fps)");
    int before = mock_device_get_state()->refresh_now_count;
    run("play.game");
    release(K_ESC);

    // ESC leaves in the first frame's `poll.input`, before that frame draws
    // anything -- so `play.game` presenting more than that frame's own `sync`
    // is the seed, taken where nothing is timing it.
    int presents = mock_device_get_state()->refresh_now_count - before;
    char msg[128];
    snprintf(msg, sizeof(msg),
             "play.game presented %d times; the seed before the loop is missing",
             presents);
    TEST_ASSERT_TRUE_MESSAGE(presents >= 2, msg);

    // And the first room's masks are drawn in `init.game`, not deferred into
    // the frame that is also presenting a whole canvas for the first time.
    // Deferring is for doorways, which happen while the loop is running.
    run("init.game");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word_of(":masks.due"),
        "the first frame has to draw the masks as well as present the room");
}

// The whole entry point, driven to its exit.  It has to put the screen back
// the way it found it -- `setrefresh "auto` above all, because a session left
// in sync mode looks like a broken interpreter to whoever types the next
// command, and so does a turtle still wearing a costume and still flipping
// with its heading.
void test_berzerk_puts_the_screen_back(void)
{
    press(K_ESC);
    run("berzerk");
    release(K_ESC);

    // IT HAS TO HAVE PLAYED.  From M3 a board that will not take the fast
    // clock is refused (§15.5), and a refusal skips `play.game` — which leaves
    // every assertion below true for the wrong reason.  Thirteen costumes is
    // `init.game` having run: four of the man walking, four of the robot, and
    // M4's five shooting poses (§7.4's allocation, now spent but for two).
    const MockDeviceState *st = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT_MESSAGE(13, st->costume.snap_count,
        "the game never started, so this proved nothing about putting it back");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("auto", word_of("refreshmode"),
        "the game left the display in sync refresh");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MOCK_SCREEN_TEXT, st->screen_mode,
        "the game left the split screen up");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_get_turtle(0)->shape,
        "the game left the turtle wearing the man");
    TEST_ASSERT_TRUE_MESSAGE(st->turtle.visible, "the game left the turtle hidden");

    // And the clock, which is the one thing it changed about the machine
    // rather than about the screen (B50).
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(LOGO_CPU_KHZ_NORMAL, mock_cpu_khz,
        "the game left the board overclocked");
}

// Running out of men is worth a card and ESC is not, which is Battlezone's
// split.  The attract screen is still M6's (§21, risk 6); what M4 owes is that
// the game ENDS, because the difficulty ramp is bounded by nothing else.
//
// The two halves are checked apart: `test_three_deaths_end_the_game_...` drives
// the frame loop to the flag, and this drives the card off it.  A whole
// three-life game cannot be run through `berzerk` here, because `play.game`
// paces on `sync` and the mock clock only moves when a test moves it.
void test_the_last_life_is_worth_a_card_and_esc_is_not(void)
{
    run("setrefresh \"manual  init.game  make \"score 750");
    mock_device_clear_output();
    run("show.game.over");
    const char *card = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(card, "GAME OVER"), "the card does not say so");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(card, "750"), "the card does not carry the score");

    // ESC quits without one, and a fresh `berzerk` is a fresh game.
    press(K_ESC);
    mock_device_clear_output();
    run("berzerk");
    release(K_ESC);
    TEST_ASSERT_NULL_MESSAGE(strstr(mock_device_get_output(), "GAME OVER"),
        "quitting showed a game-over card");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3.0f, num(":lives"),
        "the new game did not restore the lives");
}

// WINDOW, not the default `wrap`.  M1 set it and nothing needed it; M2 cannot
// do without it, because the man walks four steps past the playfield to leave
// through a doorway and his probes hang a step further -- a wrapped probe
// lands on the far side of the screen.  P13 M0 was caught by exactly this.
void test_the_game_sets_up_in_window_mode_and_flips(void)
{
    run("wrap  setrot \"fixed");
    run("init.game");

    const MockDeviceState *st = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT_MESSAGE(MOCK_BOUNDARY_WINDOW, st->turtle.boundary_mode,
        "the game did not set `window`");
    TEST_ASSERT_EQUAL_INT_MESSAGE(LOGO_ROT_FLIP, mock_device_get_turtle(0)->rot_style,
        "the game did not set `setrot \"flip`, so the man has no left-hand side");
    TEST_ASSERT_FALSE_MESSAGE(st->turtle.visible,
        "the turtle is showing, so the man is drawn twice");
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
// M3: the robots (design section 9)
//==========================================================================

// Empty every slot, so a test that wants two robots gets two and not two plus
// whatever the last room build left behind.
static void no_robots(void)
{
    run("repeat 11 [.setitem repcount :r.state 0]  make \"rob.live 0");
}

// Empty every slot.  A test that wants one bolt gets one, and `bolt.live` is
// the gate the whole pass hangs off, so it has to agree with the slots.
static void no_bolts(void)
{
    run("repeat 7 [.setitem repcount :b.dir 0  .setitem repcount :b.len 0]  "
        "make \"bolt.live 0");
}

// One robot, placed by hand with a cold wall cache.
static void robot_at(int i, float x, float y, int dir, int state)
{
    char cmd[320];
    snprintf(cmd, sizeof(cmd),
             ".setitem %d :r.x %g  .setitem %d :r.y %g  "
             ".setitem %d :r.dir %d  .setitem %d :r.state %d  "
             ".setitem %d :r.time 0  .setitem %d :r.tl 0  "
             ".setitem %d :r.br 0  .setitem %d :r.blk 15",
             i, (double)x, i, (double)y, i, dir, i, state, i, i, i, i);
    run(cmd);
}

// `iq` reads `p.cell`, which the frame hoists once in `logic.robots`.  A test
// that calls `iq` on its own has to hoist it too.
static void hoist_player_cell(void)
{
    run("make \"p.cell cell.at :p.x :p.y");
}

// The fifteen masks, written straight in, so a wall test does not depend on
// which room the generator happens to make.
static void set_cells(const int m[15])
{
    char cmd[256];
    int n = snprintf(cmd, sizeof(cmd), "make \"cell (list");
    for (int i = 0; i < 15; i++)
        n += snprintf(cmd + n, sizeof(cmd) - (size_t)n, " %d", m[i]);
    snprintf(cmd + n, sizeof(cmd) - (size_t)n, ")");
    run(cmd);
}

static int robot_state(int i)
{
    char e[48];
    snprintf(e, sizeof(e), "item %d :r.state", i);
    return (int)num(e);
}

static float robot_x(int i)
{
    char e[48];
    snprintf(e, sizeof(e), "item %d :r.x", i);
    return num(e);
}

static float robot_y(int i)
{
    char e[48];
    snprintf(e, sizeof(e), "item %d :r.y", i);
    return num(e);
}

// SEEK ($23EF) is the whole AI: two subtractions and four comparisons, and its
// output is the DURL mask -- LEFT 1, RIGHT 2, UP 4, DOWN 8 -- with the two
// halves added, which is what makes a diagonal.  Turtle y runs the other way
// from the cabinet's, so UP is the larger y here.
void test_a_robot_walks_straight_at_the_man(void)
{
    static const struct { float dx, dy; int want; } way[] = {
        { -20.0f,  0.0f, 2 },        // west of him: walk east
        {  20.0f,  0.0f, 1 },        // east of him: walk west
        {   0.0f, 40.0f, 8 },        // above him: walk down
        {   0.0f,-40.0f, 4 },        // below him: walk up
        { -20.0f, 40.0f, 10 },       // above and west
        {  20.0f, 40.0f, 9 },        // above and east
        { -20.0f,-40.0f, 6 },        // below and west
        {  20.0f,-40.0f, 5 },        // below and east
    };
    man_at(0, 0);
    for (size_t k = 0; k < sizeof(way) / sizeof(way[0]); k++)
    {
        char e[96], msg[128];
        // The robot sits at the man's own y less two, which is where seek's
        // vertical comparison is neutral -- see the test below.
        snprintf(e, sizeof(e), "seek %g %g",
                 (double)way[k].dx, (double)(-2.0f + way[k].dy));
        snprintf(msg, sizeof(msg), "a robot %g,%g from the man sought %g",
                 (double)way[k].dx, (double)way[k].dy, (double)num(e));
        // Read the direction the man is in, not the direction the robot is.
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)way[k].want, num(e), msg);
    }
}

// `dy := (player.y + 2) - robot.y` is the arcade compensating for the player
// being taller than a robot, and it is the one constant in SEEK that is not
// zero.  Negated for turtle y, the neutral row is TWO BELOW his stored corner:
// a robot exactly level with him is told to walk DOWN.
void test_seek_aims_two_below_the_mans_corner(void)
{
    man_at(0, 0);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num("seek 0 -2"),
        "the neutral row is not two below the man's stored corner");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(8.0f, num("seek 0 -1"),
        "a robot one step above the neutral row is not sent down");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4.0f, num("seek 0 -3"),
        "a robot one step below the neutral row is not sent up");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(8.0f, num("seek 0 0"),
        "a robot level with the man is not sent down, so the +2 is missing");
}

// `IQ` ($1C6E) clears any desired direction whose edge is walled.  Cell 1 is
// row 0 column 0 of the bare template and carries LEFT | TOP, so a robot in
// the middle of it may go right and down and nothing else.
void test_iq_clears_the_directions_a_wall_forbids(void)
{
    run("make \"cell wall.template");
    no_robots();
    man_at(2, 40);                  // cell 8, so the shortcut cannot fire
    hoist_player_cell();
    robot_at(1, -96, 108, 0, 1);    // the middle of cell 1

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, num("iq 1 -96 108 15"),
        "a robot in the top-left cell was not stopped by its LEFT and TOP walls");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f, num("iq 1 -96 108 3"),
        "iq cleared a bit the robot never asked for");
}

// AND IT CLEARS NOTHING IN OPEN GROUND.  Cell 8 is the middle of the bare
// template and is walled on no side at all.
void test_iq_leaves_an_open_cell_alone(void)
{
    run("make \"cell wall.template");
    no_robots();
    man_at(-96, 108);               // cell 1, so the shortcut cannot fire
    hoist_player_cell();
    robot_at(1, 2, 40, 0, 1);

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(15.0f, num("iq 1 2 40 15"),
        "a robot in open ground lost a direction to a wall that is not there");
}

// THE $1C92 SHORTCUT: a robot in the player's own cell is not probed at all.
// It is the ROM's, it is deliberately permissive, and design section 9.3 says
// leave it -- it is the only way a robot can reach a wall (see below).
void test_a_robot_in_the_players_cell_is_not_probed(void)
{
    run("make \"cell wall.template");
    no_robots();
    man_at(-96, 108);               // cell 1, walled LEFT and TOP
    hoist_player_cell();
    robot_at(1, -90, 104, 0, 1);    // cell 1 as well

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(15.0f, num("iq 1 -90 104 15"),
        "the robot sharing the player's cell was probed anyway");
}

// THE SAVING THIS MILESTONE OWES THE BUDGET (design section 6.3): `cell.at` is
// ten arithmetic statements and the straight port calls it six times a robot a
// frame.  The blocked mask is cached and re-probed only when a corner crosses
// a cell boundary, so what this test does is poison the cache and watch `iq`
// hand the poison back -- which is the only way to see from outside that it
// did not probe.
void test_iq_reuses_its_answer_until_a_corner_crosses(void)
{
    run("make \"cell wall.template");
    no_robots();
    man_at(2, 40);
    hoist_player_cell();
    robot_at(1, -96, 108, 0, 1);

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(10.0f, num("iq 1 -96 108 15"), "the first probe");

    // A mask no wall could produce, written where the cache keeps its answer.
    run(".setitem 1 :r.blk 5");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5.0f, num("iq 1 -93 105 15"),
        "the robot moved three steps inside its cell and paid for a fresh probe");

    // ...and a real crossing throws it away.  Cell 6 (row 1, column 0) carries
    // LEFT alone, so the answer there is different from cell 1's.
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(14.0f, num("iq 1 -96 40 15"),
        "the robot crossed into another cell and kept the old answer");
}

//==========================================================================
// The count cycle (design section 9.1)
//==========================================================================

// $2117 is a rejection sampler over eleven slots and $434A's threshold has
// 0x60 added to it as BCD every room, so the count is a FIVE-ROOM CYCLE and
// not a ramp: 60, 20, 80, 40, 00 and round again.  The expected counts are
// 11 x (256 - threshold) / 256.
void test_the_robot_count_is_a_five_room_cycle(void)
{
    // The USED order, which is the stored order stepped once: $434A starts at
    // $60 from DEFAULT_PLAYER_STATE and $20D8 adds $60 before $2117 places
    // anything, so the first room a player ever sees is the 87.5 % one.
    static const struct { int seed, threshold; float expected; } cycle[5] = {
        { 1,  32, 9.625f }, { 2, 128, 5.5f }, { 3, 64, 8.25f },
        { 4,   0, 11.0f  }, { 5,  96, 6.875f },
    };
    for (int t = 0; t < 5; t++)
    {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "make \"rob.ti %d  place.robots", cycle[t].seed);

        float total = 0.0f;
        const int rooms = 240;
        int used = 0;
        for (int r = 0; r < rooms; r++)
        {
            in_room(r % 16, r / 16);
            run(cmd);
            used = (int)num(":rob.ti");
            total += num(":rob.live");
        }

        char e[64], msg[160];
        snprintf(e, sizeof(e), "item %d :rob.thr", used);
        snprintf(msg, sizeof(msg),
                 "the room after cycle position %d used threshold %g, not the ROM's %d",
                 cycle[t].seed, (double)num(e), cycle[t].threshold);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)cycle[t].threshold, num(e), msg);

        float mean = total / (float)rooms;
        snprintf(msg, sizeof(msg),
                 "threshold %d seated %g robots a room, not about %g",
                 cycle[t].threshold, (double)mean, (double)cycle[t].expected);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.6f, cycle[t].expected, mean, msg);
    }
}

// The last position of the cycle is threshold zero, which the sampler can
// never refuse: eleven robots, every room, which is the room the frame budget
// is written against.
void test_the_full_room_is_always_eleven(void)
{
    for (int r = 0; r < 20; r++)
    {
        in_room(r, 3);
        run("make \"rob.ti 4  place.robots");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(11.0f, num(":rob.live"),
            "threshold zero refused a slot it cannot refuse");
    }
}

// THE COUNTERS COUNT ROOM BUILDS AND THE FIRST ROOM IS ONE OF THEM, which is
// the correction the arcade disassembly forced.  $209D is "initialise a new
// game room", $20D8 (the threshold) and $20E1 (ROBOT_SPEED) are the first
// things in it and $2117 (the placement) is the last, and DEFAULT_PLAYER_STATE
// ($187F) starts the pair at $60 and 5.  So THE FIRST ROOM A PLAYER EVER SEES
// runs at threshold $20 -- 87.5 %, 9.6 robots -- and ROBOT_SPEED 4.
//
// And the ramp is far steeper than "robots begin slower than you": the FOURTH
// room is already at the floor of 1, three pixels a frame against the player's
// 1.5, for the rest of the game.
void test_the_first_room_is_the_second_of_the_cycle(void)
{
    run("setrefresh \"manual  init.game");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f, num(":rob.ti"),
        "the first room did not use $20, the second threshold of the stored cycle");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(32.0f, num("item :rob.ti :rob.thr"),
        "the first room's threshold is not the ROM's $20");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4.0f, num(":rob.tp"),
        "the first room's robots did not start at ROBOT_SPEED 4");

    run("go.room 1 0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3.0f, num(":rob.ti"), "a doorway did not advance the cycle");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3.0f, num(":rob.tp"), "a doorway did not speed the robots up");

    run("go.room 1 0  go.room 1 0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, num(":rob.tp"),
        "ROBOT_SPEED did not reach its floor by the fourth room");

    run("go.room 1 0");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, num(":rob.ti"),
        "the cycle did not come round after five builds");
    for (int i = 0; i < 6; i++)
        run("go.room 0 1");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, num(":rob.tp"),
        "ROBOT_SPEED went below its floor");
}

// The maze is a pure function of where you are and THE CROWD IS NOT, which is
// the cabinet's own doing: the threshold moved on the doorway.  What is still
// a function of the room is the crowd GIVEN the threshold, because the sampler
// draws from the room's own stream -- so this test pins both halves.
void test_the_crowd_reproduces_for_a_room_and_a_threshold(void)
{
    float first_x[11], first_y[11];
    int   first_state[11];

    in_room(4, 9);
    run("make \"rob.ti 2  place.robots");
    for (int i = 0; i < 11; i++)
    {
        first_state[i] = robot_state(i + 1);
        first_x[i] = robot_x(i + 1);
        first_y[i] = robot_y(i + 1);
    }

    in_room(11, 2);
    run("make \"rob.ti 4  place.robots");

    in_room(4, 9);
    run("make \"rob.ti 2  place.robots");
    for (int i = 0; i < 11; i++)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "slot %d did not reproduce", i + 1);
        TEST_ASSERT_EQUAL_INT_MESSAGE(first_state[i], robot_state(i + 1), msg);
        if (first_state[i] == 0)
            continue;
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(first_x[i], robot_x(i + 1), msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(first_y[i], robot_y(i + 1), msg);
    }

    // And a different threshold in the same room is a different crowd.
    in_room(4, 9);
    run("make \"rob.ti 3  place.robots");
    bool differs = false;
    for (int i = 0; i < 11; i++)
        if (first_state[i] != robot_state(i + 1) ||
            fabsf(first_x[i] - robot_x(i + 1)) > 0.5f)
            differs = true;
    TEST_ASSERT_TRUE_MESSAGE(differs,
        "the crowd did not change with the threshold, so a room could be cleared "
        "by walking out of it and back");
}

// NO ROBOT SPAWNS TOUCHING A WALL, and that is arithmetic rather than luck --
// the jitter is -16 to +15 about a cell centre, his box is 10 x 14, and a cell
// is 48 x 68.  If it ever were luck, a full room would open with free points
// in it.
void test_no_robot_spawns_touching_a_wall(void)
{
    for (int r = 0; r < 60; r++)
    {
        in_room(r % 8, r / 8);
        run("make \"rob.ti 4  place.robots");
        for (int i = 1; i <= 11; i++)
        {
            char e[96], msg[160];
            snprintf(e, sizeof(e), "on.wall? (item %d :r.x) (item %d :r.y) 12", i, i);
            snprintf(msg, sizeof(msg),
                     "robot %d spawned on a wall in room %d,%d", i, r % 8, r / 8);
            TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word_of(e), msg);
        }
    }
}

//==========================================================================
// The move (design section 9.3's ROBOT_SPEED)
//==========================================================================

// ROBOT_SPEED ($20E1) is a TPRIME: the object steps one pixel each time its
// counter reaches zero and the counter reloads.  One of our frames is three of
// the cabinet's ticks, so a TPRIME of 5 is three pixels every five frames and
// a TPRIME of 1 is three a frame -- twice the player's rate, which is what a
// fifth room feels like.
//
// AND THE PIXELS ARE WHOLE ONES, which is not decoration: `.setitem` of a
// number the workspace has not held before interns a word (B52), so eleven
// robots writing fractional coordinates into two lists would mint two words a
// robot a frame for as long as the game ran.
// Put `n` robots in open ground in a line due west of the man, run the real
// logic pass for `frames` frames, and hand back how far the first of them
// travelled.
//
// THE LINE IS THE POINT.  They sit at the man's own seek row (`p.y - 2`, which
// is $23EF's compensation for his being taller than a robot), so every one of
// them walks due EAST and one step is one pixel of x.  Eleven apart is clear of
// `hit.robots`' eight, and they hold that spacing because they all move at the
// same rate -- spread them in y instead and they converge on his row, collide,
// and the measurement is of a crowd that killed itself.
static int crowd_travel(int n, int tp, int frames)
{
    no_robots();
    no_bolts();
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    char cmd[224];
    snprintf(cmd, sizeof(cmd),
             "make \"p.x 110  make \"p.y 40  make \"p.dying 0  "
             "make \"rob.bolts 0  make \"rob.wait 9999  "
             "make \"rob.tp %d  make \"rob.vecs %d  make \"rob.live %d",
             tp, n, n);
    run(cmd);
    for (int i = 1; i <= n; i++)
        robot_at(i, -120.0f + 11.0f * (float)(i - 1), 38.0f, 0, 1);
    for (int f = 0; f < frames; f++)
        run("logic.robots");

    char msg[96];
    snprintf(msg, sizeof(msg), "the crowd of %d lost one during the measurement", n);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)n, num(":rob.vecs"), msg);
    return (int)(robot_x(1) + 120.0f);
}

// ROBOT_SPEED IS NOT THE WHOLE PERIOD: THE CROWD TAKES TURNS.  B75, and the
// number that found it came off a recording of the cabinet -- "at the start of
// the game, the robots move about once per second" -- against a port that was
// moving them fifteen pixels a second.
//
// `BOTTOM_OF_SCREEN_INTERRUPT` ($26D9) runs once a video frame and moves
// exactly two things: the man, through MAN_PTR, and ONE vector through V.PTR.
// Then $2701-$270C walks V.PTR on to the next vector.  So the man moves every
// interrupt and a robot moves only when his turn comes round, and the period is
//
//     (vectors in the list) x ROBOT_SPEED  interrupts per pixel.
//
// The man is exempt because he is not in that list: $1FD6 allocates his vector
// inline with a NULL link word where $200E links a robot's into the chain.
//
// The start of a game is about ten robots at ROBOT_SPEED 4 -- forty interrupts,
// two thirds of a second a pixel -- which is what the board was watching.
void test_a_robot_takes_about_a_second_a_pixel_at_the_start_of_a_game(void)
{
    // Three seconds of the opening room.
    int px = crowd_travel(10, 4, 60);
    float seconds_per_pixel = 3.0f / (float)px;

    char msg[160];
    snprintf(msg, sizeof(msg),
             "ten robots at ROBOT_SPEED 4 moved %d pixels in three seconds, "
             "which is %g seconds a pixel and not about one",
             px, (double)seconds_per_pixel);
    TEST_ASSERT_TRUE_MESSAGE(seconds_per_pixel > 0.5f && seconds_per_pixel < 1.2f, msg);
}

// AND KILLING ROBOTS SPEEDS UP THE SURVIVORS, because they are the
// denominator.  Eleven robots share the room's step rate and the last one has
// all of it -- which is the thing everybody remembers about the end of a room,
// and it is not a rule anybody wrote, it is the round robin.
void test_killing_robots_speeds_up_the_ones_left(void)
{
    int crowd = crowd_travel(10, 4, 60);
    int alone = crowd_travel(1, 4, 60);

    char msg[160];
    snprintf(msg, sizeof(msg),
             "one robot travelled %d pixels where ten each travelled %d, "
             "so thinning the room did not speed up what is left of it",
             alone, crowd);
    TEST_ASSERT_TRUE_MESSAGE(alone >= crowd * 8, msg);
}

// AND A ROBOT IS NEVER FASTER THAN THE MAN UNTIL HE IS THE LAST ONE, which is
// the correction to §8.1 and §9.1: they say robots run at twice the player's
// rate from the fourth room on, and that is true of a room with ONE robot left
// in it and of nothing else.  The man is 1.5 steps a frame.
void test_a_robot_only_outruns_the_man_as_the_last_one_left(void)
{
    // Two robots at the floor: one pixel every two interrupts, which is the
    // man's own rate.
    int two = crowd_travel(2, 1, 60);
    TEST_ASSERT_EQUAL_INT_MESSAGE(90, two,
        "two robots at ROBOT_SPEED 1 are not exactly the man's speed");

    // One: twice it, and only here.
    int one = crowd_travel(1, 1, 60);
    TEST_ASSERT_EQUAL_INT_MESSAGE(180, one,
        "the last robot at ROBOT_SPEED 1 is not twice the man's speed");
}

// THE AGGREGATE IS THE CHECK ON THE WHOLE MODEL.  The cabinet moves at most one
// robot per interrupt, so a room produces 60/ROBOT_SPEED robot-steps a second
// however many robots are standing in it.  Here each robot steps
// 3/(vectors x tp) a frame and there are `vectors` of them, so the room
// produces 3/tp a frame -- the same number.  If the total moved with the crowd
// size, the per-robot rate would be wrong even where it looked right.
void test_the_rooms_step_rate_does_not_depend_on_how_many_robots_are_in_it(void)
{
    static const int crowd[] = { 2, 5, 10 };
    for (size_t k = 0; k < sizeof(crowd) / sizeof(crowd[0]); k++)
    {
        int each = crowd_travel(crowd[k], 3, 60);
        int total = each * crowd[k];
        char msg[176];
        snprintf(msg, sizeof(msg),
                 "%d robots at ROBOT_SPEED 3 took %d steps between them in 60 "
                 "frames, where the interrupt allows 60",
                 crowd[k], total);
        // Three ticks a frame over a period of 3n, n robots: sixty steps
        // between them, whatever n is.
        TEST_ASSERT_EQUAL_INT_MESSAGE(60, total, msg);
    }
}

// A ROBOT NEVER LEAVES THE ROOM, and it costs no border test: every outer cell
// is walled on its outer side even at the doorways (design section 6.3), which
// is what the mask table carries them for.  The player needs his own position
// test because he is the only thing that goes through a door.
void test_a_robot_never_leaves_the_room(void)
{
    run("setrefresh \"manual");
    in_room(5, 5);
    run("make \"rob.ti 4  place.robots  make \"rob.tp 1");
    man_at(-96, 108);

    for (int f = 0; f < 400; f++)
    {
        // Walk the man round the four corners so every robot is drawn towards
        // an outer wall in turn.
        static const float corner[4][2] = {
            { -110.0f, 130.0f }, { 106.0f, 130.0f }, { 106.0f, -30.0f }, { -110.0f, -30.0f },
        };
        const float *c = corner[(f / 40) % 4];
        char cmd[96];
        snprintf(cmd, sizeof(cmd), "make \"p.x %g  make \"p.y %g", (double)c[0], (double)c[1]);
        run(cmd);
        run("logic.robots");

        for (int i = 1; i <= 11; i++)
        {
            if (robot_state(i) != 1)
                continue;
            char msg[160];
            snprintf(msg, sizeof(msg), "robot %d reached %g,%g on frame %d",
                     i, (double)robot_x(i), (double)robot_y(i), f);
            TEST_ASSERT_TRUE_MESSAGE(robot_x(i) > -123.0f && robot_x(i) < 115.0f, msg);
            TEST_ASSERT_TRUE_MESSAGE(robot_y(i) < 143.0f && robot_y(i) > -47.0f, msg);
        }
    }
}

//==========================================================================
// The deaths
//==========================================================================

// ROBOTS KILLING ROBOTS is the fourth sentence of M3's gate.  The arcade got
// it free -- it XORs sprites into video RAM and the intercept bit does not care
// whose pixels met -- so here it is the one loop in the milestone with a
// quadratic in it, and BOTH robots die, which is what the 2600 manual teaches
// as strategy.
void test_two_robots_that_meet_both_die(void)
{
    run("make \"cell wall.template");
    no_robots();
    man_at(2, 40);
    hoist_player_cell();
    robot_at(1, -20, 40, 0, 1);
    robot_at(2, -16, 36, 0, 1);     // four steps away in each axis: overlapping
    robot_at(3, 60, 40, 0, 1);      // far off, and it must survive
    run("make \"rob.live 3");

    run("hit.robots");

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, robot_state(1), "the first robot did not die");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, robot_state(2), "the second robot did not die");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, robot_state(3), "a robot on the far side of the room died");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, num(":rob.live"), "the live count is wrong");
}

// The gate in front of the pair loop is one `abs` on y, and a pair that clears
// it on y and not on x must not die of it.
void test_robots_level_with_each_other_do_not_touch(void)
{
    run("make \"cell wall.template");
    no_robots();
    man_at(2, 40);
    hoist_player_cell();
    robot_at(1, -20, 40, 0, 1);
    robot_at(2, -12, 40, 0, 1);     // exactly eight apart: edge to edge, no overlap
    run("make \"rob.live 2");

    run("hit.robots");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, robot_state(1), "robots eight steps apart collided");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, robot_state(2), "robots eight steps apart collided");
}

// THE CORNER SUICIDE, and it has exactly one way to happen: `iq`'s $1C92
// shortcut.  Everywhere else the probe box contains the swept collision box --
// the arcade's -4/+12/+15 offsets are the sprite grown by one frame's travel --
// so the direction is cleared before the step that would land on ink.  A robot
// in the player's own cell is not probed, walks, and dies of it.
//
// The wall here is the one between cells 8 and 9, set from both sides the way
// `room.seg` sets them.  The man stands east of the robot inside cell 8, so
// seek says RIGHT and nothing probes it.
void test_a_robot_in_the_players_cell_walks_into_a_wall_and_dies(void)
{
    int m[15] = { 0 };
    m[7] = 2;                        // cell 8: a wall on its RIGHT
    m[8] = 1;                        // cell 9: the same wall from the other side
    set_cells(m);

    no_robots();
    man_at(25, 40);                  // cell 8, and far enough east not to be touched
    hoist_player_cell();
    robot_at(1, 0, 38, 0, 1);        // cell 8 as well
    run("make \"rob.live 1  make \"rob.tp 1");

    for (int f = 0; f < 6; f++)
        run("step.robot 1");

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, robot_state(1),
        "the robot walked through the wall between cells 8 and 9");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":rob.live"), "the live count is wrong");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":p.dying"),
        "the wall death did not stop the robot's turn, so it killed the man too");
}

// AND HE KILLS THE MAN BY ARRIVING, which is the asymmetry the cabinet has:
// the intercept is checked against the player's sprite and it is the player
// who is destroyed.
void test_a_robot_that_reaches_the_man_kills_him(void)
{
    run("make \"cell wall.template");
    no_robots();
    man_at(2, 40);
    hoist_player_cell();
    robot_at(1, -8, 38, 0, 1);
    run("make \"rob.live 1  make \"rob.tp 1");

    run("step.robot 1");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(15.0f, num(":p.dying"),
        "a robot walked into the man and he lived");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, robot_state(1),
        "the robot died of touching the man, which the cabinet does not do");
}

// The explosion is four frames, which is design section 7.6's count, and then
// the slot is empty.  It costs no costume slot -- $103B's four frames would be
// four of the fifteen and the fifteen are spoken for -- so it is the Vectrex's
// random strokes.
void test_the_explosion_is_four_frames_and_then_the_slot_is_empty(void)
{
    no_robots();
    robot_at(1, 0, 40, 0, 1);
    run("make \"rob.live 1  rob.dies 1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, robot_state(1), "a dying robot did not enter the explosion");

    int drawn = 0;
    for (int f = 0; f < 6; f++)
    {
        mock_device_clear_graphics();
        run("draw.robots");
        if (mock_device_line_count() > 0)
            drawn++;
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_stamp_count(),
            "a dying robot was stamped as a live one");
        run("logic.robots");
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, drawn, "the explosion is not four frames");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, robot_state(1), "the slot did not come free");
}

// AND IT STAYS INSIDE HIS OWN 8 x 12, which is the other half of the same
// decision.  A cloud that spreads needs an eraser that spreads with it, and
// the only wide pen that does not spill is pen 3 -- a wider one is a filled
// disc whose round caps would eat the wall he died against (B67).  Inside his
// own box, his own eraser is the only one it ever needs.
// SIXTY EXPLOSIONS AND NOT ONE, which is B71 and is the whole of why this test
// passed while a board watched robots leave a pixel behind.  The assertion was
// right; the sample was four frames of six strokes, against a corner that needs
// `random 6` and `random 4` to come up together -- one stroke in twenty-four.
// A stroke starts somewhere in the box and steps ONE PIXEL in one of four
// directions, so the start has to be inset by one on the side the step can
// leave from, and it was not: from x + 0 a westward stroke reached x - 1, a
// column `erase.robots` does not cover.
void test_the_explosion_stays_inside_the_robots_own_box(void)
{
    // AND THE RANDOM SOURCE HAS TO MOVE.  `mock_random` is a constant 42, so
    // `random 6` is always 0 and `random 4` always 2 -- one of the twenty-four
    // strokes this procedure can draw, and not the one that leaves the box.
    set_mock_random_walking(true);
    for (int e = 0; e < 60; e++)
    {
    no_robots();
    robot_at(1, -30, 44, 0, 1);
    run("rob.dies 1");

    for (int f = 0; f < 4; f++)
    {
        mock_device_clear_graphics();
        run("draw.robots");
        TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > 0, "the explosion drew nothing");
        for (int i = 0; i < mock_device_line_count(); i++)
        {
            const MockLine *l = mock_device_get_line(i);
            char msg[160];
            snprintf(msg, sizeof(msg),
                     "explosion stroke %d runs %g,%g to %g,%g, outside the 8 x 12 "
                     "its own eraser covers", i,
                     (double)l->x1, (double)l->y1, (double)l->x2, (double)l->y2);
            TEST_ASSERT_TRUE_MESSAGE(l->x1 >= -30.01f && l->x1 <= -22.99f, msg);
            TEST_ASSERT_TRUE_MESSAGE(l->x2 >= -30.01f && l->x2 <= -22.99f, msg);
            TEST_ASSERT_TRUE_MESSAGE(l->y1 <= 44.01f && l->y1 >= 32.99f, msg);
            TEST_ASSERT_TRUE_MESSAGE(l->y2 <= 44.01f && l->y2 >= 32.99f, msg);
        }
        run("step.boom 1");
    }
    }
}

//==========================================================================
// The robots' two marks
//==========================================================================

// Four costumes and not five, at the cabinet's own 8 x 12.  Slots 10 to 13 are
// what the table beside `cache.man` reserved for them, and the fifth facing is
// the flip.
void test_the_robot_is_four_costumes_at_the_cabinets_size(void)
{
    int before = mock_device_get_state()->costume.snap_count;
    run("setrefresh \"manual  cache.robots");
    const MockDeviceState *st = mock_device_get_state();

    TEST_ASSERT_EQUAL_INT_MESSAGE(4, st->costume.snap_count - before,
        "the robot is not four costumes");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, st->costume.last_snap_w,
        "the robot is not eight pixels wide, so the playfield is not 1:1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(12, st->costume.last_snap_h,
        "the robot is not twelve rows tall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(13, st->costume.last_snap_slot,
        "the four robot costumes are not slots 10 to 13");
}

// $1155 (walking left) IS $112C (walking right) MIRRORED, which is the claim
// the whole slot budget rests on, and it is checkable rather than believable:
// every row of one is the bit-reversal of the same row of the other.
void test_the_roms_left_facing_robot_is_its_right_one_mirrored(void)
{
    static const int right[12] = { 60, 120, 255, 189, 189, 189, 60, 36, 36, 36, 54, 0 };
    static const int left[12]  = { 60,  30, 255, 189, 189, 189, 60, 36, 36, 36, 108, 0 };

    for (int r = 0; r < 12; r++)
    {
        int mirrored = 0;
        for (int b = 0; b < 8; b++)
            if (right[r] & (1 << b))
                mirrored |= 1 << (7 - b);
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "row %d of $112C mirrors to %d, and $1155 has %d -- the flip is "
                 "not the ROM's left-facing robot", r, mirrored, left[r]);
        TEST_ASSERT_EQUAL_INT_MESSAGE(left[r], mirrored, msg);

        char e[48];
        snprintf(e, sizeof(e), "item %d :rb4", r + 1);
        snprintf(msg, sizeof(msg), "row %d of the game's own $112C is wrong", r);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)right[r], num(e), msg);
    }
}

// The whole point of the flip: a robot walking west wears the same slot as one
// walking east and differs only in the heading.  A second costume here would
// be a slot M4's shooting poses need.
void test_both_sides_of_a_robot_are_one_costume(void)
{
    no_robots();
    run("setrot \"flip");
    robot_at(1, 0, 40, 2, 1);       // RIGHT
    mock_device_clear_graphics();
    run("draw.robots");
    int east_slot = mock_device_get_stamp(0)->shape;
    float east_h = num("heading");

    robot_at(1, 0, 40, 1, 1);       // LEFT
    mock_device_clear_graphics();
    run("draw.robots");
    int west_slot = mock_device_get_stamp(0)->shape;
    float west_h = num("heading");

    TEST_ASSERT_EQUAL_INT_MESSAGE(13, east_slot, "walking east is not slot 13");
    TEST_ASSERT_EQUAL_INT_MESSAGE(east_slot, west_slot,
        "the robot walking west wears a costume of his own");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(90.0f, east_h, "walking east is not heading 90");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(270.0f, west_h, "walking west is not heading 270");
}

// The five facing groups of the ROM's sprite table ($252D) reached off the
// DURL mask: standing, up, down, and the two horizontals that share a slot.
// Every diagonal wears its horizontal, which is what the $1013 and $1027
// groups do.
void test_every_direction_wears_the_roms_own_facing(void)
{
    static const struct { int dir, slot; float face; } way[] = {
        {  0, 10,  90.0f },  // standing
        {  4, 11,  90.0f },  // up
        {  8, 12,  90.0f },  // down
        {  2, 13,  90.0f },  // right
        {  1, 13, 270.0f },  // left
        {  6, 13,  90.0f },  // up and right
        {  5, 13, 270.0f },  // up and left
        { 10, 13,  90.0f },  // down and right
        {  9, 13, 270.0f },  // down and left
        {  3, 10,  90.0f },  // left and right at once: the arcade's standing default
        { 12, 10,  90.0f },  // up and down at once
    };
    no_robots();
    run("setrot \"flip");
    for (size_t k = 0; k < sizeof(way) / sizeof(way[0]); k++)
    {
        robot_at(1, 0, 40, way[k].dir, 1);
        mock_device_clear_graphics();
        run("draw.robots");
        char msg[128];
        snprintf(msg, sizeof(msg), "direction %d wore slot %d at heading %g",
                 way[k].dir, mock_device_get_stamp(0)->shape, (double)num("heading"));
        TEST_ASSERT_EQUAL_INT_MESSAGE(way[k].slot, mock_device_get_stamp(0)->shape, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(way[k].face, num("heading"), msg);
    }
}

// A costume is centred on the turtle at both ends, and a robot's stored
// position is his sprite's TOP-LEFT corner -- an 8 x 12 sprite drawn from
// (x, y) downward has its centre at (x + 3.5, y - 5.5), where the 8 x 16 man's
// is at (x + 3.5, y - 7.5).  Getting it wrong draws him half a body from where
// the walls test him.
void test_a_robot_stamps_half_a_sprite_from_his_stored_corner(void)
{
    no_robots();
    robot_at(1, -40, 60, 2, 1);
    mock_device_clear_graphics();
    run("draw.robots");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, mock_device_stamp_count(), "the robot is not one stamp");
    const MockStamp *st = mock_device_get_stamp(0);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, -36.5f, st->x, "the stamp is not half a sprite east");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 54.5f, st->y, "the stamp is not half a sprite south");
}

// The eraser, and it is `erase.man`'s arithmetic at twelve rows: pen 3 is the
// one wide pen that is a square, so three strokes at x + 1, x + 4 and x + 6
// running from y - 1 down nine cover the 8 x 12 exactly.  A pen 8 stroke down
// the spine would leave the corners behind and the robot would drag a trail in
// every direction, which is what a board said about the man (B67).
void test_the_erase_covers_every_pixel_a_robot_stamped(void)
{
    static bool covered[240][320];

    no_robots();
    robot_at(1, -40, 60, 2, 1);
    mock_device_clear_graphics();
    run("draw.robots");
    const MockStamp *st = mock_device_get_stamp(0);
    int sx0 = SCR_X(st->x) - 4, sy0 = SCR_Y(st->y) - 6;

    mock_device_clear_graphics();
    run("erase.robots");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, mock_device_line_count(),
        "one robot is not three erase strokes");
    erase_coverage(covered);

    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 8; x++)
        {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "the robot's pixel %d,%d survives the erase -- he leaves a trail", x, y);
            TEST_ASSERT_TRUE_MESSAGE(covered[sy0 + y][sx0 + x], msg);
        }

    // AND NOT ONE PIXEL MORE.  The walls are drawn once a room, so an eraser
    // that spills eats a hole nothing repaints until the next doorway.
    for (int y = sy0 - 3; y < sy0 + 15; y++)
        for (int x = sx0 - 3; x < sx0 + 11; x++)
        {
            if (x >= sx0 && x < sx0 + 8 && y >= sy0 && y < sy0 + 12)
                continue;
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "the robot eraser painted %d,%d, which is outside him and may be a wall",
                     x - sx0, y - sy0);
            TEST_ASSERT_FALSE_MESSAGE(covered[y][x], msg);
        }

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, (int)num("pensize"),
        "the eraser left the pen three wide, so the next wall is a slab");
}

// A DYING ROBOT IS ERASED TOO, because his explosion is drawn inside his own
// box and nothing else is going to reach it.
void test_a_dying_robot_is_still_erased(void)
{
    no_robots();
    robot_at(1, -40, 60, 2, 1);
    run("rob.dies 1");
    mock_device_clear_graphics();
    run("erase.robots");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, mock_device_line_count(),
        "a dying robot was left on the screen");
}

//==========================================================================
// The frame, with a room full of them
//==========================================================================

// Design section 18's fourth ceiling, at eleven robots rather than at one man:
// nothing in this interpreter collects on demand, so a frame that spends
// storage has a fuse on it.  M0's harness died `out of space in rob.left` on
// its first board run with exactly this loop in it.
//
// MEASURED AS A TREND AND NOT AS A QUIET WINDOW, which is what M3 learned by
// trying the quiet window first.  `mem_free_nodes()` is `free_count +
// (node_bottom - atom_next) / 4` (core/memory.c): **the cell pool and the word
// table are one arena growing from opposite ends**, so a robot walking into a
// coordinate the workspace has not held reads as a spent NODE, and `recycle`
// hands both back at once on the readout's own cadence.  Over nineteen frames
// neither counter means what its name says.  M2's `test_an_ordinary_frame_
// spends_no_cells` can still assert exact equality because one man's positions
// are a small set that closes in a few dozen frames; eleven robots roaming a
// 244 x 204 playfield are ~450 numbers an axis plus 352 spawn jitters, and the
// man moves the target every frame.
//
// What the ceiling actually needs is that the workspace comes back to where it
// started, and that is a trend over thousands of frames.  Peaks are compared
// because a peak is a reading taken after a `recycle` and before the next
// spend -- the level the arena settles at.  A leak of one cell a frame is six
// thousand over this run, against a bound of 256.
void test_a_full_room_of_robots_leaves_the_workspace_where_it_found_it(void)
{
    static const int ways[8][2] = {
        { K_RIGHT, 0 }, { K_RIGHT, K_UP }, { K_UP, 0 }, { K_LEFT, K_UP },
        { K_LEFT, 0 }, { K_LEFT, K_DOWN }, { K_DOWN, 0 }, { K_RIGHT, K_DOWN },
    };
    run("setrefresh \"manual  init.game");
    in_room(9, 9);
    run("make \"rob.ti 4  place.robots  make \"rob.tp 1  reset.man");

    float n_early = 0.0f, n_late = 0.0f, a_early = 0.0f, a_late = 0.0f;
    for (int i = 0; i < 6000; i++)
    {
        if (i % 500 == 0)
        {
            float n = num("nodes"), a = num("atoms");
            if (i >= 500 && i < 3000) { if (n > n_early) n_early = n; if (a > a_early) a_early = a; }
            if (i >= 3000)            { if (n > n_late)  n_late  = n; if (a > a_late)  a_late  = a; }
        }
        const int *way = ways[(i / 11) % 8];
        press(way[0]);
        if (way[1]) press(way[1]);
        frame();
        release(way[0]);
        if (way[1]) release(way[1]);
    }

    char msg[240];
    snprintf(msg, sizeof(msg),
             "the free cell pool peaked at %g in the second half against %g in "
             "the first, so a frame with eleven robots in it spends cells",
             (double)n_late, (double)n_early);
    TEST_ASSERT_TRUE_MESSAGE(n_late > n_early - 256.0f, msg);

    snprintf(msg, sizeof(msg),
             "the free word table peaked at %g in the second half against %g in "
             "the first, so a robot's coordinates are not a closed set (B52)",
             (double)a_late, (double)a_early);
    TEST_ASSERT_TRUE_MESSAGE(a_late > a_early - 256.0f, msg);
}

void test_a_doorway_brings_a_new_crowd(void)
{
    run("setrefresh \"manual  init.game");
    run("make \"room.x 3  make \"room.y 4  make \"rob.ti 4  draw.room");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(11.0f, num(":rob.live"),
        "the first room was built without robots in it");

    man_at(-96, 42);
    mock_device_clear_graphics();
    frame();
    TEST_ASSERT_EQUAL_INT_MESSAGE(12, mock_device_stamp_count(),
        "an ordinary frame did not stamp eleven robots and a man");
}


// A DEATH SENDS HIM TO ANOTHER ROOM, which M2 did not need and M3 cannot do
// without: he respawns in the left doorway cell and the thing that killed him
// is standing there.  The crowd comes back on the eleven spawn cells -- the
// fifteen less the four doorway cells -- so the cell he arrives in is the one
// cell no robot starts in.
void test_a_death_sends_him_to_another_room_and_costs_him_a_life(void)
{
    run("setrefresh \"manual  init.game");
    run("make \"room.x 6  make \"room.y 2  make \"rob.ti 4  "
        "make \"rob.tp 4  draw.room");
    man_at(2, 40);

    // Kill him, and move the crowd well away from where they started, so a
    // room that did NOT rebuild would leave them where the test put them.
    run("repeat 11 [.setitem repcount :r.x 60  .setitem repcount :r.y 40]");
    run("man.dies");
    for (int f = 0; f < 16; f++)
        frame();

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":p.dying"), "he never came back");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-96.0f, num(":p.x"), "he did not respawn at the doorway");
    TEST_ASSERT_TRUE_MESSAGE(num(":rob.live") > 0.0f, "the crowd did not come back");

    // ANOTHER ROOM, WHICH M3 GOT WRONG AND M4 READ OUT OF $1806 -- reversed once
    // on a board's report and put back by watching the cabinet (2026-08-31).
    // The per-life entry builds a room from the current coordinates and then
    // sets ROOM_X and ROOM_Y from `call RANDOM` ($1821, low byte to ROOM_X) --
    // and since $209D does not return until you die, what it writes is where
    // the NEXT life starts.  So a death does not put you back in the maze that
    // killed you.  It is deterministic rather than arbitrary: the room he lands
    // in is a function of the stream the room he died in left behind.
    TEST_ASSERT_TRUE_MESSAGE(num(":room.x") != 6.0f || num(":room.y") != 2.0f,
        "the death put him back in the room that had just killed him");
    TEST_ASSERT_TRUE_MESSAGE(num(":room.x") >= 0.0f && num(":room.x") <= 255.0f,
        "the new room's x is not a byte");
    TEST_ASSERT_TRUE_MESSAGE(num(":room.y") >= 0.0f && num(":room.y") <= 255.0f,
        "the new room's y is not a byte");

    // AND IT COSTS HIM A LIFE AND SOME DIFFICULTY.  A death is a room build,
    // so $20D8 and $20E1 run and the cabinet charges you for dying; the three
    // lives are what stop that running away.
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f, num(":lives"), "the death was free");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, num(":rob.ti"),
        "a death did not advance the threshold, but it re-enters the same room init");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f, num(":rob.tp"),
        "a death did not speed the robots up");

    // Nobody is standing on him, which is the whole point.  The spawn cells are
    // the fifteen less the four doorway cells and he comes back in a doorway
    // cell, so this is arithmetic rather than luck.
    for (int i = 1; i <= 11; i++)
    {
        if (robot_state(i) != 1)
            continue;
        char msg[160];
        snprintf(msg, sizeof(msg), "robot %d respawned within reach of him at %g,%g",
                 i, (double)robot_x(i), (double)robot_y(i));
        TEST_ASSERT_TRUE_MESSAGE(fabsf(robot_x(i) - num(":p.x")) >= 8.0f ||
                                 robot_y(i) - 12.0f >= num(":p.y") ||
                                 num(":p.y") - 16.0f >= robot_y(i), msg);
    }
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":p.dying"),
        "he was killed again the frame he came back");
}

// THREE LIVES AND THEN THE GAME ENDS, which is the whole reason the ramp is
// survivable.  $209D advances the robot threshold and decrements ROBOT_SPEED,
// and BOTH a doorway ($2209 ends `jp $20D7`, which falls into that block) and a
// death ($1806 -> $181E) reach it -- so the game gets harder every room and
// every death, ROBOT_SPEED runs 4, 3, 2, 1 over the first four builds and stays
// at 1, where a robot moves twice the player's speed.  In the cabinet that is
// bounded by the game ENDING and DEFAULT_PLAYER_STATE being copied back; here
// it was bounded by nothing at all, and a board reported the obvious symptom:
// the game gets very hard very quickly.
void test_three_deaths_end_the_game_and_a_new_one_starts_over(void)
{
    run("setrefresh \"manual  init.game");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3.0f, num(":lives"), "a game does not start with three");

    for (int life = 3; life >= 1; life--)
    {
        run("make \"p.dying 0  man.dies");
        for (int f = 0; f < 16; f++)
            frame();
        char msg[96];
        snprintf(msg, sizeof(msg), "after %d death(s) he has %g lives",
                 4 - life, (double)num(":lives"));
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)(life - 1), num(":lives"), msg);
    }

    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word_of(":over"),
        "the third death did not end the game");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word_of(":leaving"),
        "the game over did not stop the frame loop");

    // And a new game puts the whole ramp back where $187F puts it: threshold,
    // robot speed, score and lives.  Without this a session only ever gets
    // harder, which is not a difficulty curve.
    run("make \"score 4321  init.game");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(3.0f, num(":lives"), "the new game did not restore the lives");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":score"), "the new game kept the score");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word_of(":over"), "the new game started over");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4.0f, num(":rob.tp"),
        "the new game kept the robots at the speed the last one reached");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f, num(":rob.ti"),
        "the new game kept the last one's threshold");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":rob.bolts"),
        "the new game kept the last one's robot bolts");
}

// The ramp itself, read straight: ROBOT_SPEED starts at 5 ($187F byte 8) and
// $20E1 decrements it at the top of every room build with a floor of 1, and the
// threshold at $434A starts at $60 and has $60 added as BCD -- so the FIRST
// room a player sees is the 87.5 % one and the FOURTH is already at the floor.
// This is the cabinet, and it is a great deal steeper than "robots begin slower
// than you"; what it is not is unbounded.
void test_the_ramp_reaches_the_floor_in_four_room_builds(void)
{
    static const int speed[6] = { 4, 3, 2, 1, 1, 1 };
    run("setrefresh \"manual  init.game");
    for (int room = 0; room < 6; room++)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "after %d room build(s) ROBOT_SPEED is %g",
                 room + 1, (double)num(":rob.tp"));
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)speed[room], num(":rob.tp"), msg);
        run("go.room 1 0");
    }
}


// A FRAME THAT BUILDS A ROOM WRITES NO TEXT, which is M3's widening of M2's
// "one text job a frame" and came off a board: `WORST` read 100 ms at `fast`
// where an ordinary frame was 53 and ROOM was 23.  M2's rule serialised the
// two text jobs against each other and stopped there, because a room build was
// then 11 ms; `place.robots` put it at 23, which makes it a third job of the
// same size -- and `show.text` used to run at the top of the frame, where
// nothing yet knows a doorway is coming.
//
// So it runs at the END of the body now, and a build claims the frame.  The
// masks were already deferred and stay deferred; what is new is that the
// once-a-second readout gives way too.
void test_a_frame_that_builds_a_room_writes_no_other_text(void)
{
    run("setrefresh \"manual  init.game");
    man_at(119, 42);                 // one step from the right-hand doorway
    press(K_RIGHT);

    // A game starts in a random room ($17D6), so the door is read as a step
    // rather than as a coordinate.
    float was = num(":room.x");
    // Force the collision: the second's beat falls on the frame he leaves in.
    run("make \"frames 19  make \"masks.due false");
    mock_device_clear_output();
    frame();
    release(K_RIGHT);
    const char *screen = mock_device_get_output();

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(fmodf(was + 1.0f, 256.0f), num(":room.x"),
        "he did not go through the door");
    TEST_ASSERT_NULL_MESSAGE(strstr(screen, "FRAME"),
        "the timing rows were written in the frame that built a room");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word_of(":room.built"),
        "the build flag was not cleared, so the next frame is silent too");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word_of(":masks.due"),
        "the doorway's masks were not deferred");

    // And nothing is owed forever: the masks take the next frame and the
    // timing rows the one after their next beat.
    mock_device_clear_output();
    frame();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word_of(":masks.due"),
        "the masks never came back");

    run("make \"frames 19");
    mock_device_clear_output();
    frame();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(mock_device_get_output(), "FRAME"),
        "the timing rows never came back");
}


//==========================================================================
// The clock is a precondition (design section 15.5)
//==========================================================================

// §15.5 called 300 MHz a precondition rather than a preference, and M3's board
// reading made it a measurement: at `fast` an ordinary frame with eleven robots
// is 53 ms, and at `normal` the body roughly doubles and there is nothing to
// play.  So the game asks for the clock, READS IT BACK, and does not start
// without it -- Battlezone's `clock`/`restore.clock` pair, which is the same
// problem with a different frame budget on it.
void test_the_game_asks_for_the_fast_clock_and_reads_it_back(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word_of("clock"),
        "the game would not play on a board that took the clock");
    TEST_ASSERT_EQUAL_STRING("fast", word_of(":cpu.at"));

    // Asked for on the HARDWARE and not just recorded: `hw.cpu` reads the board
    // back, so a `cpu.at` of "fast without the clock having moved would mean
    // the read was answering from memory.
    TEST_ASSERT_EQUAL_UINT32(LOGO_CPU_KHZ_FAST, mock_cpu_khz);
}

// B50: a game that leaves the board overclocked has changed the machine and not
// just played on it, and on a board with PSRAM that is not merely impolite --
// the QMI's timing is computed once at boot against the clock running then.
// It restores what it FOUND, so a session that was already fast stays fast.
void test_the_game_gives_the_clock_back_when_it_exits(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
    run("ignore clock");
    TEST_ASSERT_EQUAL_UINT32(LOGO_CPU_KHZ_FAST, mock_cpu_khz);
    run("restore.clock");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(LOGO_CPU_KHZ_NORMAL, mock_cpu_khz,
        "the game left the board overclocked");

    set_mock_cpu_khz(true, LOGO_CPU_KHZ_FAST);
    run("ignore clock");
    run("restore.clock");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(LOGO_CPU_KHZ_FAST, mock_cpu_khz,
        "the game undid a clock it did not set");

    // A board with no settable clock has no `hw.cpu` at all, which is what the
    // `catch`es are for: nothing to put back, and it must not error.
    set_mock_cpu_khz(false, LOGO_CPU_KHZ_NORMAL);
    run("ignore clock");
    run("restore.clock");
    TEST_ASSERT_EQUAL_STRING("unknown", word_of(":cpu.was"));
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
}

// AND A BOARD THAT WILL NOT OVERCLOCK IS TOLD WHY, which is the whole point of
// bringing this forward from M6.  "Unplayable" with no explanation is the worst
// of the three outcomes; the attract screen §21 risk 6 asks for is still M6's,
// and what M3 owes is the sentence.
void test_a_board_that_will_not_overclock_is_told_why_and_does_not_play(void)
{
    set_mock_cpu_khz(false, LOGO_CPU_KHZ_NORMAL);
    int costumes = mock_device_get_state()->costume.snap_count;
    mock_device_clear_output();

    run("berzerk");

    TEST_ASSERT_EQUAL_INT_MESSAGE(costumes, mock_device_get_state()->costume.snap_count,
        "the game started on a board that cannot run it");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(mock_device_get_output(), "300 MHz"),
        "the board was refused without being told why");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MOCK_SCREEN_TEXT,
        mock_device_get_state()->screen_mode, "the refusal left the split screen up");
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
}


// THE SPAWN TABLE IS THE ARCADE'S $23A0 AND NOT THE VECTREX PORT'S GLOSS OF IT,
// which is where M0 got it and where two errors came in and survived to M3.
//
//   * They are (x, y) pairs, not (y, x).  $23B8 does `push bc / pop de` then
//     `ld (ix+7),d` for P.X and `ld (ix+9),e` for P.Y, so the FIRST byte is x.
//     The check is that the second byte takes only three values -- 12, 80, 150
//     -- which is the three rows of a 5 x 3 grid.
//   * The jitter is UNSIGNED.  $2130-$213C is `RANDOM / and $1F / add a,b` in
//     each axis: 0 to 31 ADDED.  The table holds the top-left of a spawn band,
//     four pixels into its cell; it does not hold cell centres, and subtracting
//     16 from it (which is what this game used to do) puts robots half a cell
//     from where the cabinet starts them.
//
// Eleven bands on the fifteen cells less the four doorway cells, which is why
// there are exactly eleven robots.
void test_the_spawn_bands_are_the_cabinets_own(void)
{
    // Each band, as turtle coordinates: x + 0..31 and y - 0..31.
    static const float band[11][2] = {
        {  80.0f,  -8.0f }, {  34.0f, -8.0f }, { -62.0f,  -8.0f }, { -114.0f,  -8.0f },
        {  32.0f,  62.0f }, { -14.0f, 62.0f }, { -62.0f,  62.0f },
        {  80.0f, 130.0f }, {  34.0f, 130.0f }, { -62.0f, 130.0f }, { -114.0f, 130.0f },
    };
    for (int i = 0; i < 11; i++)
    {
        char e[64], msg[160];
        snprintf(e, sizeof(e), "item %d :rob.sx", i + 1);
        snprintf(msg, sizeof(msg), "spawn band %d is not at the ROM's x", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(band[i][0], num(e), msg);
        snprintf(e, sizeof(e), "item %d :rob.sy", i + 1);
        snprintf(msg, sizeof(msg), "spawn band %d is not at the ROM's y", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(band[i][1], num(e), msg);
    }

    // And the jitter lands inside the band, above and left of nothing: over
    // enough rooms every robot is at or after its band's corner, and within 31.
    bool saw_corner_x = false, saw_far_x = false;
    for (int r = 0; r < 80; r++)
    {
        in_room(r % 8, r / 8);
        run("make \"rob.ti 4  place.robots");
        for (int i = 1; i <= 11; i++)
        {
            float dx = robot_x(i) - band[i - 1][0];
            float dy = band[i - 1][1] - robot_y(i);
            char msg[192];
            snprintf(msg, sizeof(msg),
                     "robot %d landed %g,%g from its band, which is not `random and 31` added",
                     i, (double)dx, (double)dy);
            TEST_ASSERT_TRUE_MESSAGE(dx >= 0.0f && dx <= 31.0f, msg);
            TEST_ASSERT_TRUE_MESSAGE(dy >= 0.0f && dy <= 31.0f, msg);
            if (dx < 2.0f)  saw_corner_x = true;
            if (dx > 29.0f) saw_far_x = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(saw_corner_x && saw_far_x,
        "the jitter never reached both ends of its 32-step band");
}

//==========================================================================

//==========================================================================
// M4: the bolts (design section 10)
//==========================================================================

// One bolt, placed by hand with its wall cache primed the way `fire.bolt`
// primes it -- the cell of its own head, so the first frame does not report a
// crossing that never happened.
static void bolt_at(int i, int dir, float x, float y, int len, int own)
{
    char cmd[400];
    snprintf(cmd, sizeof(cmd),
             ".setitem %d :b.dir %d  .setitem %d :b.x %g  .setitem %d :b.y %g  "
             ".setitem %d :b.len %d  .setitem %d :b.own %d  "
             ".setitem %d :b.cell cell.at %g %g  "
             "make \"bolt.live :bolt.live + 1",
             i, dir, i, (double)x, i, (double)y,
             i, len, i, own,
             i, (double)x, (double)y);
    run(cmd);
}

static int bolt_dir(int i) { char e[32]; snprintf(e, sizeof(e), "item %d :b.dir", i); return (int)num(e); }
static int bolt_len(int i) { char e[32]; snprintf(e, sizeof(e), "item %d :b.len", i); return (int)num(e); }
static float bolt_x(int i) { char e[32]; snprintf(e, sizeof(e), "item %d :b.x", i); return num(e); }
static float bolt_y(int i) { char e[32]; snprintf(e, sizeof(e), "item %d :b.y", i); return num(e); }

// SHOOT ($287F) is asked directly: the seek direction it is handed comes from
// $241B and is the RAW one, before `iq` has cleared anything, so a test has to
// hoist it the way `step.robot` does.
static const char *robot_fires(float px, float py, float rx, float ry)
{
    char cmd[220];
    snprintf(cmd, sizeof(cmd),
             "make \"p.x %g  make \"p.y %g  make \"rob.bolts 5  "
             "make \"s.k seek %g %g",
             (double)px, (double)py, (double)rx, (double)ry);
    run(cmd);
    char e[96];
    snprintf(e, sizeof(e), "robot.fires 1 %g %g", (double)rx, (double)ry);
    return word_of(e);
}

// SHOOT refuses entirely unless one of three windows matches, and the windows
// are read off the compares rather than off the prose: `cp -2 / jp nc` with
// `cp 6 / jr c` is dx in -2..5 INCLUSIVE, `cp -4` with `cp 7` is dy in -4..6,
// and `cp $F6` with `cp 6` on abs(dy) - abs(dx) is -10..5.
//
// The robot sits at the origin throughout and the man is moved to put the
// deltas on their boundaries, with the other two windows held wide open --
// which is the only way to test a window that is third in a chain of three.
//
// The 2600 manual's "unlike you, robots cannot shoot on the diagonal" is right
// about the 2600 and wrong about the arcade.  The third window is right there.
void test_the_three_firing_windows_at_their_boundaries(void)
{
    static const struct { float px, py; bool fires; const char *what; } way[] = {
        //  dx = p.x, dy = 2 - p.y, with the robot at the origin.
        {   5.0f, -98.0f, true,  "dx = 5, the top of the vertical window" },
        {  -2.0f, -98.0f, true,  "dx = -2, the bottom of it" },
        {   6.0f, -98.0f, false, "dx = 6, one past the vertical window" },
        {  -3.0f, -98.0f, false, "dx = -3, one under it" },
        { 100.0f,  -4.0f, true,  "dy = 6, the top of the horizontal window" },
        { 100.0f,   6.0f, true,  "dy = -4, the bottom of it" },
        { 100.0f,  -5.0f, false, "dy = 7, one past the horizontal window" },
        { 100.0f,   7.0f, false, "dy = -5, one under it" },
        {  20.0f, -23.0f, true,  "abs(dy) - abs(dx) = 5, the top of the diagonal window" },
        {  20.0f,  -8.0f, true,  "abs(dy) - abs(dx) = -10, the bottom of it" },
        {  20.0f, -24.0f, false, "abs(dy) - abs(dx) = 6, one past the diagonal window" },
        {  20.0f,  -7.0f, false, "abs(dy) - abs(dx) = -11, one under it" },
        {  20.0f, -98.0f, false, "well off every window" },
    };

    for (size_t k = 0; k < sizeof(way) / sizeof(way[0]); k++)
    {
        no_bolts();
        const char *got = robot_fires(way[k].px, way[k].py, 0.0f, 0.0f);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(way[k].fires ? "true" : "false", got, way[k].what);
    }
}

// The direction is SEEK's, MASKED: a vertical window keeps the up/down bits
// ($28D8's `and $0C`), a horizontal one the left/right bits ($28D2's
// `and $03`), and a diagonal keeps both.  So a robot nearly in your column
// shoots straight down it rather than at you.
void test_a_robots_shot_is_seeks_direction_masked_to_the_window(void)
{
    no_bolts();
    robot_fires(5.0f, -98.0f, 0.0f, 0.0f);       // vertical window
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, bolt_dir(3),
        "a shot down the column kept its sideways bit");

    no_bolts();
    robot_fires(100.0f, -4.0f, 0.0f, 0.0f);      // horizontal window
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, bolt_dir(3),
        "a shot along the row kept its vertical bit");

    no_bolts();
    robot_fires(20.0f, -18.0f, 0.0f, 0.0f);      // diagonal window
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, bolt_dir(3),
        "a diagonal shot did not keep both bits");
}

// The bolt goes in the FIRST free robot slot of the RBOLTS the difficulty
// table allows ($289F scans exactly that many), and slots 1 and 2 are the
// player's and are never scanned.
void test_a_robot_takes_the_first_free_robot_slot_and_never_the_players(void)
{
    no_bolts();
    run("make \"rob.wait 0");
    robot_fires(100.0f, -4.0f, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(1), "a robot took a player slot");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(2), "a robot took a player slot");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, bolt_dir(3), "the robot's bolt is not in slot 3");

    // Every robot slot full and the shot is refused rather than dropped on a
    // player bolt.
    no_bolts();
    run("repeat 5 [.setitem repcount + 2 :b.dir 2]  make \"bolt.live 5");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", robot_fires(100.0f, -4.0f, 0.0f, 0.0f),
        "a robot fired with every robot slot in use");
}

// RWAIT ($434D) IS SHARED BY THE ROOM and not held per robot: SHOOT's first
// act is to read the timer at TIMER_LIST_PTR+1 and return if it is running,
// and $2931 reloads it from RWAIT after any robot fires.  So a room of robots
// lined up on you produces ONE bolt, not eleven -- which is also what makes
// the fire test one comparison a frame instead of eleven.
void test_only_one_robot_fires_per_holdoff(void)
{
    no_robots();
    no_bolts();
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    for (int i = 1; i <= 5; i++)
        robot_at(i, -60.0f + 4.0f * (float)i, 40.0f, 0, 1);
    run("make \"p.x 100  make \"p.y 44  make \"rob.bolts 5  make \"rob.wait 0  "
        "make \"rob.wait0 70");
    run("logic.robots");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, (int)num(":bolt.live"),
        "a room of robots all fired in the same frame");
    TEST_ASSERT_EQUAL_INT_MESSAGE(70, (int)num(":rob.wait"),
        "the shot did not reload the room's holdoff");

    // And nothing fires while it runs.
    run("logic.robots");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, (int)num(":bolt.live"),
        "a robot fired while the holdoff was still running");
}

// $28EF zeroes both of the firing robot's velocities, with the best comment in
// the disassembly beside it: "if you didn't do this, the robot could walk into
// the bolt it's just fired and blow itself up."  Both S.TAB pattern pointers
// are $1000, the STANDING group, so he also stops looking where he was going.
void test_a_firing_robot_stops_dead_and_stands(void)
{
    no_robots();
    no_bolts();
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    robot_at(1, 0.0f, 40.0f, 0, 1);
    run("make \"p.x 100  make \"p.y 44  make \"rob.bolts 5  make \"rob.wait 0  "
        "make \"rob.wait0 70  make \"rob.tp 1  .setitem 1 :r.time 0");
    run("logic.robots");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, robot_x(1),
        "the robot walked in the frame he fired in");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num("item 1 :r.dir"),
        "the firing robot is not wearing the standing group");
}

// SR.TAB ($2067): the pose the man wears, where his bolt starts and which way
// it goes, all off the DURL bits through DIRECTION_OFFSET_TABLE.  Every number
// here is the ROM's, with the y column negated as every y in this file is.
void test_space_and_a_direction_starts_the_roms_own_bolt(void)
{
    static const struct { int a, b; int dir; float dx, dy; const char *way; } way[] = {
        { K_RIGHT, 0,      2,  7.0f, -3.0f, "right" },
        { K_LEFT,  0,      1,  0.0f, -3.0f, "left" },
        { K_UP,    0,      4,  7.0f, -2.0f, "up" },
        { K_DOWN,  0,      8,  6.0f, -7.0f, "down" },
        { K_UP,    K_RIGHT, 6, 7.0f, -1.0f, "up and right" },
        { K_UP,    K_LEFT,  5, 0.0f,  0.0f, "up and left" },
        { K_DOWN,  K_RIGHT, 10, 6.0f, -6.0f, "down and right" },
        { K_DOWN,  K_LEFT,  9, 0.0f, -6.0f, "down and left" },
    };

    in_room(0, 0);
    for (size_t k = 0; k < sizeof(way) / sizeof(way[0]); k++)
    {
        no_bolts();
        man_at(-40.0f, 40.0f);
        run("make \"p.shoot 0");
        press(K_SPACE);
        press(way[k].a);
        if (way[k].b) press(way[k].b);
        run("poll.input  fire.man");
        release(K_SPACE);
        release(way[k].a);
        if (way[k].b) release(way[k].b);

        char msg[160];
        snprintf(msg, sizeof(msg), "shooting %s took the wrong direction", way[k].way);
        TEST_ASSERT_EQUAL_INT_MESSAGE(way[k].dir, bolt_dir(1), msg);
        snprintf(msg, sizeof(msg), "shooting %s started the bolt in the wrong place", way[k].way);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-40.0f + way[k].dx, bolt_x(1), msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(40.0f + way[k].dy, bolt_y(1), msg);
    }
}

// TRY_FIRE ($1F01) takes the first of the two player slots that is free and
// refuses if neither is, and $1F8A holds the trigger for 12 ticks -- four of
// our frames, counted down three a frame, so the fifth frame is the next shot.
// Holding the key re-arms it on exactly the frame it expires, which is why the
// pose does not blink between shots.
void test_the_man_holds_two_bolts_and_reloads_for_twelve_ticks(void)
{
    in_room(0, 0);
    no_bolts();
    no_robots();
    man_at(-40.0f, 40.0f);
    run("make \"p.shoot 0");
    press(K_SPACE);
    press(K_UP);

    run("poll.input  fire.man");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, bolt_dir(1), "the first shot did not go out");
    TEST_ASSERT_EQUAL_INT_MESSAGE(12, (int)num(":p.shoot"), "the reload is not the ROM's 12");

    for (int f = 1; f <= 3; f++)
    {
        run("poll.input  fire.man");
        char msg[96];
        snprintf(msg, sizeof(msg), "a second bolt went out %d frame(s) into the reload", f);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(2), msg);
    }
    run("poll.input  fire.man");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, bolt_dir(2), "the reload never expired");

    // Both slots are now in use, so the next expiry has nowhere to put a bolt.
    for (int f = 0; f < 4; f++)
        run("poll.input  fire.man");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, (int)num(":bolt.live"),
        "a third player bolt went out with both slots in use");

    release(K_SPACE);
    release(K_UP);
}

// THE ARRAY IS PROCESSED THREE TIMES A TICK AND NOT TWICE, which is M4's
// correction to section 10.2 and the one mechanism that section marked as
// inferred.  HANDLE_PLAYER_BOLTS is `ld b,2 / call`, then `(mod+2) and 7 /
// call`, and then `ld b,7` FALLING THROUGH into the same loop -- so a slot
// gets 1 + (i <= 2) + (i <= (mod+2) and 7) passes.  One pixel a pass at 60 Hz
// is three of our frames.
//
// Player bolts are therefore nine steps a frame at every difficulty, and robot
// bolts three at mod 0 and six at mod 5.  Section 10.2 expected parity at
// mod 5; there is no difficulty at which they are equal.
void test_a_player_bolt_is_three_pixels_a_tick_and_a_robot_bolt_one(void)
{
    static const int mod0[7] = { 9, 9, 3, 3, 3, 3, 3 };
    static const int mod5[7] = { 9, 9, 6, 6, 6, 6, 6 };

    run("make \"score 0  place.bolts");
    for (int i = 1; i <= 7; i++)
    {
        char e[32], msg[128];
        snprintf(e, sizeof(e), "item %d :b.spd", i);
        snprintf(msg, sizeof(msg), "at mod 0 slot %d moves %g steps a frame", i, (double)num(e));
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)mod0[i - 1], num(e), msg);
    }

    // mod 1 is the last two rows of the below-10,000 table; mod 5 is the top of
    // the second one, which M6 owns -- set it directly, since what is being
    // pinned is the pass arithmetic and not the table.
    run("make \"bolt.hi bitand (5 + 2) 7  "
        "repeat 7 [make \"k.i repcount  make \"k.n 3  "
        "if not :k.i > 2 [make \"k.n :k.n + 3]  "
        "if not :k.i > :bolt.hi [make \"k.n :k.n + 3]  "
        ".setitem :k.i :b.spd :k.n]");
    for (int i = 1; i <= 7; i++)
    {
        char e[32], msg[128];
        snprintf(e, sizeof(e), "item %d :b.spd", i);
        snprintf(msg, sizeof(msg), "at mod 5 slot %d moves %g steps a frame", i, (double)num(e));
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)mod5[i - 1], num(e), msg);
    }

    // And the step is actually taken.
    run("make \"score 0  place.bolts");
    no_bolts();
    no_robots();
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    bolt_at(1, 2, -40.0f, 40.0f, 0, 0);
    bolt_at(3, 2, -40.0f, 20.0f, 0, 0);
    run("step.bolts");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-31.0f, bolt_x(1), "the player's bolt is not nine steps a frame");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-37.0f, bolt_x(3), "a robot's bolt is not three steps a frame");
}

// Length grows one a pass with the head and stops at MaxLength -- 8 for the
// player ($1F7C) and 5 for a robot ($2921).  A player bolt is at full length
// after one tick, which is why nine steps a frame leaves no gap behind it:
// the new tail lands one pixel past the old head.
void test_a_bolt_grows_to_the_roms_own_length(void)
{
    no_bolts();
    no_robots();
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    run("make \"score 0  place.bolts");
    bolt_at(1, 4, -40.0f, -40.0f, 0, 0);
    bolt_at(3, 4, 0.0f, -40.0f, 0, 0);

    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, bolt_len(1), "the player's bolt is not eight long");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, bolt_len(3), "a robot's bolt grew faster than it moved");
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, bolt_len(1), "the player's bolt grew past eight");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, bolt_len(3), "a robot's bolt is not five long");
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, bolt_len(3), "a robot's bolt grew past five");
}

// A bolt is a one-pixel stroke drawn from its HEAD backwards along its own
// direction, and a diagonal one is `len` pixels in BOTH axes -- so the stroke
// is len * sqrt 2 turtle steps long.  Drawing it `len` long would put the tail
// at 71 % of where it belongs and leave the head where the collision test is
// not looking.
void test_a_diagonal_bolt_is_drawn_its_full_length(void)
{
    no_bolts();
    bolt_at(1, 2, 0.0f, 40.0f, 8, 0);       // right
    bolt_at(2, 10, 0.0f, 0.0f, 8, 0);       // down and right
    mock_device_clear_graphics();
    run("mark.bolts :wall.pc");

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, mock_device_line_count(), "a bolt is not one stroke");
    const MockLine *a = mock_device_get_line(0);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.0f, a->x1, "the head moved");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, -8.0f, a->x2, "the tail is not eight steps behind the head");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 40.0f, a->y2, "a horizontal bolt is not level");

    const MockLine *b = mock_device_get_line(1);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.02f, -8.0f, b->x2,
        "the diagonal bolt is not eight pixels wide, so it was drawn `len` rather than len * sqrt 2");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.02f, 8.0f, b->y2,
        "the diagonal bolt is not eight pixels tall");
}

// A bolt hits the cell edge the wall is drawn on, and the test is a CROSSING
// rather than the occupancy the man needs (section 8.3, B69): he has extent
// and can stand beside a wall he never crossed, a bolt is one pixel and cannot.
// One `cell.at` a bolt a frame, cached the way `iq` caches its probe, with the
// crossed edge coming out of the index arithmetic.
void test_a_bolt_dies_on_a_wall(void)
{
    //                     cell 1..5     6  7  8..10      11..15
    set_cells((const int[15]){ 0,0,0,0,0,  2, 1, 0,0,0,  0,0,0,0,0 });
    no_bolts();
    no_robots();
    run("make \"score 0  place.bolts");

    // Cell 6 is walled on its RIGHT, at x = -70.  Nine steps a frame from -80
    // lands on -71 and then crosses.
    bolt_at(1, 2, -80.0f, 40.0f, 8, 0);
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, bolt_dir(1), "the bolt died before it reached the wall");
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(1), "the bolt went through a drawn wall");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_len(1),
        "a dead bolt kept its length, so the next erase rubs a stroke nobody drew");

    // And an unwalled edge is crossed without comment.
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    no_bolts();
    bolt_at(1, 2, -80.0f, 40.0f, 8, 0);
    run("step.bolts  step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, bolt_dir(1), "the bolt died on an edge with no wall on it");
}

// A BOLT TRAVELLING LEFT OR UP THAT LANDS EXACTLY ON THE WALL LINE is the case
// the probe offset is for.  `cell.at` puts a boundary pixel in the cell to its
// right and the one below it, so a leftward bolt that stops ON x = -70 is
// still in the cell it started in and no crossing is seen -- it would be drawn
// over the wall and rub a hole in it on the next frame's erase.
void test_a_bolt_that_lands_on_the_wall_line_dies_on_it(void)
{
    set_cells((const int[15]){ 0,0,0,0,0,  2, 1, 0,0,0,  0,0,0,0,0 });
    no_bolts();
    no_robots();
    run("make \"score 0  place.bolts");

    // Cell 7 is walled on its LEFT, at x = -70.  From -61, three steps a frame
    // lands on -64, then -67, then exactly -70.
    bolt_at(3, 1, -61.0f, 40.0f, 5, 0);
    run("step.bolts  step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, bolt_dir(3), "the bolt died short of the wall");
    run("step.bolts");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-70.0f, bolt_x(3), "the bolt is not on the wall line");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(3),
        "the bolt stopped ON the wall and lived, so the next erase eats a hole in it");
}

// The playfield rectangle is the border wall and CHECK_IF_BOLT_OFFSCREEN
// ($157E) in one test: the border is drawn ON x = -122 and 122 and y = -62 and
// 142, so a bolt that reaches the line has hit it.
void test_a_bolt_dies_at_the_border(void)
{
    static const struct { int dir; float x, y; const char *way; } way[] = {
        { 2, 115.0f,  40.0f, "east" },
        { 1, -115.0f, 40.0f, "west" },
        { 4, -40.0f,  135.0f, "north" },
        { 8, -40.0f, -55.0f, "south" },
    };
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    no_robots();
    run("make \"score 0  place.bolts");

    for (size_t k = 0; k < sizeof(way) / sizeof(way[0]); k++)
    {
        no_bolts();
        bolt_at(1, way[k].dir, way[k].x, way[k].y, 8, 0);
        run("step.bolts");
        char msg[128];
        snprintf(msg, sizeof(msg), "a bolt flew out of the playfield to the %s", way[k].way);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(1), msg);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":bolt.live"), msg);
    }
}

// A BOLT CHECKS THE PLAYER FIRST AND THEN EVERY ROBOT ($15A4, $15AB), which is
// what makes a robot's stray shot kill another robot and pay you for it.  That
// is a rule and not an accident, and section 12 keeps it: fifty points a robot
// however he died ($2480).
void test_a_bolt_kills_a_robot_and_pays_fifty(void)
{
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    no_robots();
    no_bolts();
    run("make \"score 0  place.bolts  make \"p.dying 0  make \"p.x -110  make \"p.y 120");
    robot_at(1, 0.0f, 35.0f, 0, 1);

    // The player's own bolt, walking up to him nine steps a frame: the first
    // step ends at -2, one pixel short of his box, and the second sweeps him.
    bolt_at(1, 2, -11.0f, 30.0f, 8, 0);
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, robot_state(1), "the bolt killed him two pixels short of his box");
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, robot_state(1), "the bolt went straight through him");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(1), "the bolt lived through the robot it killed");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(50.0f, num(":score"), "a robot is not fifty points");

    // A ROBOT'S bolt kills another robot the same way, and pays you the same.
    no_robots();
    no_bolts();
    run("make \"score 0");
    robot_at(1, 0.0f, 35.0f, 0, 1);
    bolt_at(3, 2, -2.0f, 30.0f, 5, 7);
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, robot_state(1), "a robot's stray shot did not kill a robot");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(50.0f, num(":score"),
        "a robot killed by a robot did not pay");
}

// And the other way round: a robot's bolt kills the man, and it is the only
// kind that can.
void test_a_robots_bolt_kills_the_man(void)
{
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    no_robots();
    no_bolts();
    run("make \"score 0  place.bolts");
    man_at(0.0f, 40.0f);
    bolt_at(3, 2, -2.0f, 30.0f, 5, 7);
    run("step.bolts");

    TEST_ASSERT_TRUE_MESSAGE(num(":p.dying") > 0.0f, "a robot's bolt did not kill the man");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(3), "the bolt lived through the man it killed");
}

// A BOLT DOES NOT HIT WHOEVER FIRED IT, and that is a divergence worth naming
// (section 17).  The cabinet does not need the rule -- it erases the player's
// sprite before it processes bolts ($26E6 ahead of $26F4), so his own bolt has
// no pixels of his to intercept, and it stops a firing robot dead so he cannot
// walk into his.  Neither survives a nine-pixel step out of a spawn point that
// is ON the firer's own box: SR.TAB starts the man's bolt at his x + 7 of 8.
void test_a_bolt_does_not_hit_whoever_fired_it(void)
{
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    no_robots();
    no_bolts();
    run("make \"score 0  place.bolts");
    man_at(0.0f, 40.0f);
    robot_at(4, 0.0f, 0.0f, 0, 1);

    bolt_at(1, 2, 7.0f, 37.0f, 0, 0);         // the man's own, out of SR.TAB
    bolt_at(3, 8, 7.0f, -6.0f, 0, 4);         // robot 4's own, out of S.TAB
    run("step.bolts");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":p.dying"), "the man shot himself");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, robot_state(4), "a robot shot itself");

    // And the exclusion is the FIRER's and not everybody's: the same bolt
    // kills the robot standing beside him.
    no_bolts();
    robot_at(5, 0.0f, 40.0f, 0, 1);
    bolt_at(1, 2, -2.0f, 35.0f, 8, 0);
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, robot_state(5), "the exclusion took everybody with it");
}

// THE GATE IS AN OVER-ESTIMATE ON A DIAGONAL and the exact test is what stops
// it costing a robot his life.  The two `abs` comparisons are satisfied on
// each axis independently, where a 45-degree line ties them together, so a
// robot in the corner of a diagonal bolt's bounding box passes the gate and
// must survive.  Nine pixels of travel and an 8 x 12 box is a big enough
// corner to be worth the parameter interval.
void test_a_diagonal_bolt_misses_the_corner_of_its_own_bounding_box(void)
{
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    no_robots();
    no_bolts();
    run("make \"score 0  place.bolts  make \"p.x -110  make \"p.y 120  make \"p.dying 0");

    // The bolt runs (0,0) to (9,-9); the robot's box is x -8..0, y -21..-9,
    // which is the far corner of the same bounding box and nowhere near the line.
    robot_at(1, -8.0f, -9.0f, 0, 1);
    // And one squarely on it.
    robot_at(2, 4.0f, -2.0f, 0, 1);

    bolt_at(1, 10, 0.0f, 0.0f, 8, 0);
    run("step.bolts");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, robot_state(1),
        "a diagonal bolt killed a robot in the corner of its bounding box");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, robot_state(2),
        "a diagonal bolt missed a robot standing on it");
}

// Section 13's below-10,000 table ($3794), at every threshold and either side
// of it.  RBOLTS is 0 below 300 points, which is the manuals' "robots don't
// shoot in the first maze", and the difficulty RESETS at 7,500 -- five bolts
// drops back to one, with the modifier raised, which is the 2600 manual's
// "their firing speeds remain equal to your man's" in the arcade's currency.
//
// RWAIT is set by the table and then decremented by ten, floor ten ($20FA's
// `cp $14`), which does not accumulate because the table is read again every
// room: the table sets the level and the room walks it down one step.
void test_the_difficulty_table_at_every_threshold(void)
{
    static const struct { int score, bolts, mod, wait; } band[] = {
        {     0, 0, 0, 70 }, {   299, 0, 0, 70 },
        {   300, 1, 0, 70 }, {  1499, 1, 0, 70 },
        {  1500, 2, 0, 10 }, {  2999, 2, 0, 10 },
        {  3000, 3, 0, 10 }, {  4499, 3, 0, 10 },
        {  4500, 4, 0, 10 }, {  5999, 4, 0, 10 },
        {  6000, 5, 0, 15 }, {  7499, 5, 0, 15 },
        {  7500, 1, 1, 50 }, {  8999, 1, 1, 50 },
        {  9000, 1, 1, 40 }, { 99999, 1, 1, 40 },
    };

    for (size_t k = 0; k < sizeof(band) / sizeof(band[0]); k++)
    {
        char cmd[64], msg[160];
        snprintf(cmd, sizeof(cmd), "make \"score %d  place.bolts", band[k].score);
        run(cmd);

        snprintf(msg, sizeof(msg), "at %d points the room allows %g robot bolts",
                 band[k].score, (double)num(":rob.bolts"));
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)band[k].bolts, num(":rob.bolts"), msg);

        snprintf(msg, sizeof(msg), "at %d points the bolt slot modifier is %g",
                 band[k].score, (double)(num(":bolt.hi") - 2.0f));
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)((band[k].mod + 2) & 7), num(":bolt.hi"), msg);

        snprintf(msg, sizeof(msg), "at %d points the firing holdoff is %g",
                 band[k].score, (double)num(":rob.wait0"));
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)band[k].wait, num(":rob.wait0"), msg);
    }
}

// "Robots don't shoot in the first maze", read end to end rather than off the
// table: a room's worth of robots lined up on the man, the holdoff expired,
// and no bolt because RBOLTS is zero.
void test_robots_do_not_shoot_in_the_first_maze(void)
{
    no_robots();
    no_bolts();
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    run("make \"score 0  place.bolts  make \"rob.wait 0");
    for (int i = 1; i <= 5; i++)
        robot_at(i, -60.0f + 4.0f * (float)i, 40.0f, 0, 1);
    run("make \"p.x 100  make \"p.y 44");
    for (int f = 0; f < 20; f++)
        run("logic.robots");

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":bolt.live"),
        "a robot fired below 300 points");
}

// A room build clears the bolts, because $209D is where the difficulty table
// is read and the screen is cleared: a doorway does not carry last room's
// shots into the next one, and neither does a death.
void test_a_room_change_clears_the_bolts(void)
{
    in_room(0, 0);
    no_bolts();
    bolt_at(1, 2, -40.0f, 40.0f, 8, 0);
    bolt_at(3, 1, 40.0f, 20.0f, 5, 4);
    run("setrefresh \"manual  go.room 1 0");

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":bolt.live"), "a bolt crossed a doorway");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(1), "a bolt crossed a doorway");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_len(3),
        "a bolt kept its length across a room build, so the next erase draws on the new maze");
}

// THE ROM'S EIGHT SHOOT SPRITES ARE NOT FOUR MIRRORED PAIRS, which is what
// section 7.4 says and what the fifteen slots were balanced on.  Reversed byte
// for byte only ONE pair is exact, and the obvious pairing of the rest is
// wrong -- $132B (down-right) is a plain body with one arm and $134D
// (down-left) carries the DOWN sprite's two shoulder rows, so they are
// different drawings.
//
// The count survives because the pairs are different ones: DOWN mirrors to
// down-left in two rows and up-right to up-left in three, both inside the
// licence section 7.4 already took for the walk cycle.  Down-right and up
// pair with nothing and wear their own slot.  Five slots, eight poses.
void test_the_roms_shoot_sprites_are_not_the_pairs_the_design_named(void)
{
    static const int ur[15] = { 24, 25,  4, 28, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 28 };
    static const int rt[15] = { 24, 24,  0, 31, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 28 };
    static const int dr[15] = { 24, 24,  0, 24, 24, 28, 26, 24, 24, 24, 24, 24, 24, 24, 28 };
    static const int dn[15] = { 24, 24,  0, 60, 60, 58, 58, 58, 24, 24, 24, 24, 24, 24, 28 };
    static const int dl[15] = { 24, 24,  0, 60, 60, 92,156, 28, 24, 24, 24, 24, 24, 24, 56 };
    static const int lf[15] = { 24, 24,  0,248, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 56 };
    static const int ul[15] = {152, 88, 32, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 56 };

    static const struct { const int *a, *b; int rows; const char *what; } pair[] = {
        { rt, lf, 0, "$131A mirrors to $135E exactly, so right and left are one slot" },
        { dn, dl, 2, "$133C mirrors to $134D in all but two rows" },
        { ur, ul, 3, "$1309 mirrors to $136F in all but three rows" },
        { dr, dl, 5, "$132B and $134D are not each other's mirror at all" },
    };

    for (size_t k = 0; k < sizeof(pair) / sizeof(pair[0]); k++)
    {
        int differ = 0;
        for (int r = 0; r < 15; r++)
        {
            int m = 0;
            for (int b = 0; b < 8; b++)
                if (pair[k].a[r] & (1 << b))
                    m |= 1 << (7 - b);
            if (m != pair[k].b[r])
                differ++;
        }
        char msg[192];
        snprintf(msg, sizeof(msg), "%s -- %d rows differ, not %d",
                 pair[k].what, differ, pair[k].rows);
        TEST_ASSERT_EQUAL_INT_MESSAGE(pair[k].rows, differ, msg);
    }

    // And the five the game holds are the ROM's own five, padded to sixteen
    // rows so every costume the man has is one size and one erase rectangle.
    static const int *held[5] = { ur, rt, dr, dn, NULL };
    static const int up[15] = { 24, 24, 0, 29, 27, 25, 24, 24, 24, 24, 24, 24, 24, 24, 56 };
    held[4] = up;
    for (int i = 0; i < 5; i++)
        for (int r = 0; r < 16; r++)
        {
            char e[32], msg[128];
            snprintf(e, sizeof(e), "item %d :sb%d", r + 1, i + 1);
            snprintf(msg, sizeof(msg), "row %d of shoot sprite %d is wrong", r, i + 1);
            TEST_ASSERT_EQUAL_FLOAT_MESSAGE(r < 15 ? (float)held[i][r] : 0.0f, num(e), msg);
        }
}

// Five slots and thirteen of fifteen spent, which is section 18's third
// ceiling closed rather than deferred.
void test_the_shooting_poses_are_five_costumes_at_the_mans_size(void)
{
    int before = mock_device_get_state()->costume.snap_count;
    run("setrefresh \"manual  cache.shoot");
    const MockDeviceState *st = mock_device_get_state();

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, st->costume.snap_count - before,
        "the shooting poses are not five costumes");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, st->costume.last_snap_w, "a shooting pose is not eight wide");
    TEST_ASSERT_EQUAL_INT_MESSAGE(16, st->costume.last_snap_h,
        "a shooting pose is not sixteen rows, so the man has two erase rectangles");
    TEST_ASSERT_EQUAL_INT_MESSAGE(9, st->costume.last_snap_slot,
        "the shooting poses are not slots 5 to 9");
}

// Every direction the trigger can point, off DIRECTION_OFFSET_TABLE: the eight
// real ones and the four "defaults" the table gives for two opposite arrows.
// Three of the eight are somebody else mirrored, which is the whole of why
// five slots hold eight poses.
void test_every_shot_direction_wears_the_roms_own_pose(void)
{
    static const struct { int fire, slot; float face; const char *way; } way[] = {
        {  6, 5,  90.0f, "up and right" },
        {  2, 6,  90.0f, "right" },
        { 10, 7,  90.0f, "down and right" },
        {  8, 8,  90.0f, "down" },
        {  9, 8, 270.0f, "down and left, which is DOWN flipped" },
        {  1, 6, 270.0f, "left, which is RIGHT flipped" },
        {  5, 5, 270.0f, "up and left, which is UP-RIGHT flipped" },
        {  4, 9,  90.0f, "up" },
        {  7, 9,  90.0f, "up with both sideways arrows: the table's up default" },
        { 11, 8,  90.0f, "down with both: the down default" },
        { 13, 6, 270.0f, "left with both vertical arrows: the left default" },
        { 14, 6,  90.0f, "right with both: the right default" },
    };

    in_room(0, 0);
    run("setrot \"flip");
    man_at(-40.0f, 40.0f);
    for (size_t k = 0; k < sizeof(way) / sizeof(way[0]); k++)
    {
        char cmd[96];
        snprintf(cmd, sizeof(cmd), "make \"p.fire %d  make \"p.shoot 12", way[k].fire);
        run(cmd);
        mock_device_clear_graphics();
        run("draw.man");

        char msg[160];
        snprintf(msg, sizeof(msg), "shooting %s wore slot %d at heading %g",
                 way[k].way, mock_device_get_stamp(0)->shape, (double)num("heading"));
        TEST_ASSERT_EQUAL_INT_MESSAGE(way[k].slot, mock_device_get_stamp(0)->shape, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(way[k].face, num("heading"), msg);
    }

    // And two opposite arrows alone point nowhere, so the pose is standing and
    // no bolt goes out -- $2042's entry 0.
    no_bolts();
    run("make \"p.fire 3  make \"p.shoot 0  make \"p.fires \"true  fire.man");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":bolt.live"),
        "the man fired with the stick in a direction the table calls neutral");
}

// A WALL STOPS A BOLT WHERE IT IS, NOT AT THE END OF THE STEP -- B72, reported
// from a board in two shapes: "if a robot is standing on the other side of a
// wall, I can kill it", and "I shot diagonally at the point of the wall, the
// wall stopped it but I killed the robots diagonally on the other side".
//
// Both are the same defect.  The first build tested actors over the WHOLE nine
// pixels of a step and then asked about the wall, on the grounds that a robot
// standing against a wall is the case that matters -- and it is, which is why
// wall-first was not the answer either.  The answer is to find where along the
// step the wall is and shorten the swept segment to end there, which keeps the
// robot in front and drops the one behind.
void test_a_bolt_does_not_kill_through_a_wall(void)
{
    // A wall on the boundary at x = -70: cell 6's RIGHT and cell 7's LEFT.
    set_cells((const int[15]){ 0,0,0,0,0,  2, 1, 0,0,0,  0,0,0,0,0 });
    no_robots();
    no_bolts();
    run("make \"score 0  place.bolts  make \"p.x -110  make \"p.y 120  make \"p.dying 0");

    // Nine steps from -74 would end at -65, four pixels into a robot standing
    // just past the wall.
    robot_at(1, -68.0f, 40.0f, 0, 1);
    bolt_at(1, 2, -74.0f, 35.0f, 8, 0);
    run("step.bolts");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, robot_state(1),
        "the bolt killed a robot on the far side of a wall (B72)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(1), "the bolt went through the wall");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(-70.0f, bolt_x(1),
        "the bolt did not stop on the wall pixel");
}

// And the case wall-first would have broken: the robot IN FRONT of the wall is
// inside the shortened segment and still dies.
void test_a_bolt_stopped_by_a_wall_still_kills_what_is_in_front_of_it(void)
{
    set_cells((const int[15]){ 0,0,0,0,0,  2, 1, 0,0,0,  0,0,0,0,0 });
    no_robots();
    no_bolts();
    run("make \"score 0  place.bolts  make \"p.x -110  make \"p.y 120  make \"p.dying 0");

    robot_at(1, -78.0f, 40.0f, 0, 1);
    bolt_at(1, 2, -74.0f, 35.0f, 8, 0);
    run("step.bolts");

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, robot_state(1),
        "the wall swallowed a shot the player could see connecting");
}

// The diagonal shape of B72, and it is the one that says the clip is a
// DISTANCE and not a coordinate: the bolt crosses the wall's column part way
// through its step, so everything past that point on the line has to go with
// it -- including a robot the unclipped segment reaches two pixels later.
//
// The far robot is slot 1 and the near one slot 2, so the far one is tested
// FIRST: on the shipped code it died and the bolt stopped there, which left
// the near robot alive.  Both halves fail before the fix.
void test_a_diagonal_bolt_stopped_by_a_wall_kills_nothing_past_it(void)
{
    set_cells((const int[15]){ 0,0,0,0,0,  2, 1, 0,0,0,  0,0,0,0,0 });
    no_robots();
    no_bolts();
    run("make \"score 0  place.bolts  make \"p.x -110  make \"p.y 120  make \"p.dying 0");

    robot_at(1, -69.0f, 44.0f, 0, 1);      // past the wall, on the line
    robot_at(2, -78.0f, 40.0f, 0, 1);      // in front of it, also on the line
    bolt_at(1, 6, -76.0f, 30.0f, 8, 0);    // up and right, nine steps
    run("step.bolts");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, robot_state(1),
        "a diagonal bolt killed past the point of the wall that stopped it (B72)");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, robot_state(2),
        "the clip took the robot in front of the wall with it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(1), "the bolt lived through the wall");
}

// A BOLT FIRED ALONG A WALL RUNS DOWN THE LINE IT IS DRAWN ON -- B73, reported
// from a board as "if I shoot edge on a wall the shot will erase the wall (does
// not stop)".  A crossing test asks whether the head went THROUGH a wall line,
// which is everything a bolt fired across the room can do and nothing a bolt
// fired along one does: it crosses nothing, so nothing stopped it, and it was
// drawn over the wall every frame and the next frame's erase took the wall away
// with it.
//
// So the head is also asked whether it is ON ink, which for something one pixel
// wide is exact and is two `modulo`s -- every wall pixel in the room is named
// exactly once by "my own left boundary" or "my own top boundary".
void test_a_bolt_fired_along_a_wall_dies_on_it(void)
{
    no_robots();
    run("make \"score 0  place.bolts  make \"p.x -110  make \"p.y 120  make \"p.dying 0");

    // Horizontal: the wall on the boundary at y = 74, which is cell 6's TOP.
    set_cells((const int[15]){ 0,0,0,0,0,  4, 0, 0,0,0,  0,0,0,0,0 });
    no_bolts();
    bolt_at(1, 2, -100.0f, 74.0f, 8, 0);
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(1),
        "a bolt ran along a horizontal wall, erasing it as it went (B73)");

    // Vertical: the wall on the boundary at x = -70, which is cell 7's LEFT.
    set_cells((const int[15]){ 0,0,0,0,0,  0, 1, 0,0,0,  0,0,0,0,0 });
    no_bolts();
    bolt_at(1, 8, -70.0f, 60.0f, 8, 0);
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, bolt_dir(1),
        "a bolt ran down a vertical wall, erasing it as it went (B73)");
}

// AND NOT ONE PIXEL EAGER, which is the other half of an occupancy test.  A
// bolt one step off the wall line must live, or a room becomes unshootable
// along every corridor; and the playfield's own left edge is NOT an interior
// wall -- column 0's left boundary is x = -118 while the border is drawn at
// -122, and the mask table carries a LEFT bit there for the robots that consult
// it (§6.3).  `bolt.out?` owns the border; this must not.
void test_a_bolt_beside_a_wall_and_at_the_grids_edge_lives(void)
{
    no_robots();
    run("make \"score 0  place.bolts  make \"p.x -110  make \"p.y 120  make \"p.dying 0");

    set_cells((const int[15]){ 0,0,0,0,0,  4, 0, 0,0,0,  0,0,0,0,0 });
    no_bolts();
    bolt_at(1, 2, -100.0f, 73.0f, 8, 0);       // one step below the wall line
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, bolt_dir(1),
        "a bolt died beside a wall it was never on");

    // Column 0 carries a LEFT bit for the border, drawn four steps further out.
    set_cells((const int[15]){ 1,0,0,0,0,  1, 0, 0,0,0,  1,0,0,0,0 });
    no_bolts();
    bolt_at(1, 8, -118.0f, 40.0f, 8, 0);
    run("step.bolts");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, bolt_dir(1),
        "a bolt died on the grid's left boundary, where no wall is drawn");
}

// A bolt is a one-pixel stroke and its erase is the same stroke in the
// background colour, so it puts back exactly what it took -- no cap arithmetic
// and no B67.  What has to hold is that it retraces the stroke it DREW: the
// frame erases before it steps, so a bolt that has moved must not be erased at
// its new head.
void test_the_erase_retraces_the_stroke_the_bolt_drew(void)
{
    set_cells((const int[15]){ 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 });
    no_robots();
    no_bolts();
    run("make \"score 0  place.bolts  make \"p.x -110  make \"p.y 120  make \"p.dying 0");
    bolt_at(1, 2, -40.0f, 40.0f, 8, 0);

    mock_device_clear_graphics();
    run("mark.bolts :wall.pc");
    const MockLine *drew = mock_device_get_line(0);
    float x1 = drew->x1, y1 = drew->y1, x2 = drew->x2, y2 = drew->y2;
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, drew->pen_size, "a bolt is not a one-pixel stroke");

    mock_device_clear_graphics();
    run("mark.bolts :bg.pc");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, mock_device_line_count(), "the erase is not one stroke");
    const MockLine *rub = mock_device_get_line(0);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, x1, rub->x1, "the erase starts somewhere else");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, y1, rub->y1, "the erase starts somewhere else");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, x2, rub->x2, "the erase ends somewhere else");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, y2, rub->y2, "the erase ends somewhere else");

    // A dead bolt has nothing left on the screen, and a slot that kept its
    // length would rub out a stroke nobody drew.
    run("step.bolts");
    run("bolt.dies 1");
    mock_device_clear_graphics();
    run("mark.bolts :bg.pc");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(),
        "a dead bolt was erased at a head it never reached");
}

// Section 18's fourth ceiling, with the trigger held down.  A bolt writes six
// numbers into six lists every frame it lives and its head is on whole pixels
// for exactly this reason (B52): `.setitem` of a number the workspace has not
// held before interns a word, and the man's own x is a half-pixel every other
// frame.  `fire.bolt` rounds, and every step after it is whole.
void test_a_frame_with_bolts_in_the_air_spends_no_cells(void)
{
    run("setrefresh \"manual");
    in_room(9, 9);
    man_at(-5, 45);
    run("make \"score 6000  place.bolts");
    press(K_RIGHT);
    press(K_SPACE);
    for (int i = 0; i < 200; i++)   // warm: every slot used, every number minted
        frame();

    float nodes0 = num("nodes");
    for (int i = 0; i < 40; i++)
        frame();
    release(K_RIGHT);
    release(K_SPACE);

    char msg[160];
    snprintf(msg, sizeof(msg), "forty frames of shooting spent %g cells",
             (double)(nodes0 - num("nodes")));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(nodes0, num("nodes"), msg);
}

//==========================================================================
// The scroll between rooms (design section 17, and question 4 with it)
//==========================================================================

// WHAT THE CABINET DOES.  SCROLL_LEFT ($2274) is `ld a,$20` and thirty-two
// one-byte `ldir`s of the whole screen image; SCROLL_UP ($21E6) is twenty-
// seven $0100-byte ones.  Either way the bitmap moves EIGHT PIXELS A STEP for
// one full screen.  Our canvas is 320 x 240 where the cabinet's is 256 x 216,
// so the step is the thing kept and the count falls out of it: 40 across and
// 30 down.
//
// AND NOTHING SLIDES IN BEHIND IT, which is the finding that made the scroll
// affordable here at all.  All four routines zero the rows the move vacated
// ("remove junk left after scroll") and only THEN does $2209 seed the new room
// and fall into $20D7 to draw it.  So a step draws ONE room -- the one being
// left -- and section 17's price for the cut, "the room has to be drawn twice
// during it", was double what it costs.
//
// Nothing here is a timing, and the pause least of all: a step ends on `sync`,
// so the transition is thirty or forty FRAME PERIODS -- 1.50 s down and 2.00 s
// across at 20 fps, against the cabinet's own 1.53 and 1.92 -- and the mock
// clock does not advance over a sleep, so only a board can read that back.
// What the host can say is that there are the right number of steps, that the
// maze on the last one is gone from the presented band, and that it is still
// the room he is leaving.

static int refreshes(void)
{
    return mock_device_get_state()->refresh_now_count;
}

void test_the_maze_leaves_by_a_whole_screen_eight_pixels_at_a_time(void)
{
    // `bound` and `sign`: where the whole picture has to be by the last step.
    // The presented band is turtle x in [-160, 160] and y in [-79, 160]
    // (design section 4), and the room is 244 x 204 about (0, +40).
    static const struct
    {
        const char *way; int steps; float dx, dy, bound; int sign;
    } trips[] = {
        { "slide.room 1 0",  40, -320.0f,    0.0f, -160.0f, -1 },
        { "slide.room -1 0", 40,  320.0f,    0.0f,  160.0f, +1 },
        { "slide.room 0 1",  30,    0.0f,  240.0f,  160.0f, +1 },
        { "slide.room 0 -1", 30,    0.0f, -240.0f,  -79.0f, -1 },
    };

    for (size_t t = 0; t < sizeof(trips) / sizeof(trips[0]); t++)
    {
        // The room as it stands, which is what has to be sliding.
        in_room(7, 2);
        run("clean");
        mock_device_clear_graphics();
        run("draw.walls 0 0");
        TEST_ASSERT_EQUAL_INT_MESSAGE(16, mock_device_line_count(),
            "a room is not sixteen runs");
        MockLine home[16];
        for (int i = 0; i < 16; i++)
            home[i] = *mock_device_get_line(i);

        int before = refreshes();
        run(trips[t].way);

        char msg[192];
        snprintf(msg, sizeof(msg), "%s presented %d times, not %d",
                 trips[t].way, refreshes() - before, trips[t].steps);
        TEST_ASSERT_EQUAL_INT_MESSAGE(trips[t].steps, refreshes() - before, msg);

        // Every step begins with `clean`, which truncates the mock's record,
        // so what is left is the LAST step and nothing else.
        snprintf(msg, sizeof(msg), "%s: the last step is not one room", trips[t].way);
        TEST_ASSERT_EQUAL_INT_MESSAGE(16, mock_device_line_count(), msg);

        for (int i = 0; i < 16; i++)
        {
            const MockLine *l = mock_device_get_line(i);
            snprintf(msg, sizeof(msg),
                     "%s: run %d ended at %g,%g to %g,%g and not the room "
                     "displaced by %g,%g", trips[t].way, i,
                     (double)l->x1, (double)l->y1, (double)l->x2, (double)l->y2,
                     (double)trips[t].dx, (double)trips[t].dy);
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, home[i].x1 + trips[t].dx, l->x1, msg);
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, home[i].y1 + trips[t].dy, l->y1, msg);
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, home[i].x2 + trips[t].dx, l->x2, msg);
            TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, home[i].y2 + trips[t].dy, l->y2, msg);

            // OUT OF VIEW, which is the whole claim: a travel of one room
            // would leave the far wall of the old maze standing in the band.
            float a = trips[t].dx != 0.0f ? l->x1 : l->y1;
            float b = trips[t].dx != 0.0f ? l->x2 : l->y2;
            snprintf(msg, sizeof(msg),
                     "%s: run %d is still inside the band at %g,%g", trips[t].way,
                     i, (double)a, (double)b);
            if (trips[t].sign > 0)
                TEST_ASSERT_TRUE_MESSAGE(a > trips[t].bound && b > trips[t].bound, msg);
            else
                TEST_ASSERT_TRUE_MESSAGE(a < trips[t].bound && b < trips[t].bound, msg);
        }
    }
}

// $2209 SEEDS THE NEW ROOM AFTER THE SCROLL, not before it, so what slides out
// is the maze he walked in -- and the room ahead does not exist while it does.
// Here that is the order in `go.room`: slide, then move the coordinates, then
// build.  It is also why the slide needs no copy of anything.
void test_the_slide_shows_the_room_it_is_leaving_and_not_the_one_ahead(void)
{
    in_room(4, 9);
    float here[15], after[15];
    Segment segs0[8], segs1[8];
    read_cell(here);
    read_segments(segs0);

    run("slide.room 1 0");

    read_cell(after);
    read_segments(segs1);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4.0f, num(":room.x"), "the slide moved the room");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9.0f, num(":room.y"), "the slide moved the room");
    for (int i = 0; i < 15; i++)
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(here[i], after[i],
            "the slide generated the next room under itself");
    for (int k = 0; k < 8; k++)
    {
        TEST_ASSERT_EQUAL_FLOAT(segs0[k].x, segs1[k].x);
        TEST_ASSERT_EQUAL_FLOAT(segs0[k].y, segs1[k].y);
        TEST_ASSERT_EQUAL_FLOAT(segs0[k].h, segs1[k].h);
        TEST_ASSERT_EQUAL_FLOAT(segs0[k].l, segs1[k].l);
    }
}

// ONLY A DOORWAY SCROLLS.  $21CF -- the doorway -- reaches the scroll and then
// falls into the room build; the per-life entry at $1806 calls $209D directly,
// so a death gets the build and none of the travel.  It changes the room
// ([B74](bugs.md)) without ever being a journey.
//
// Every step ends on `sync` and so does the frame, and `sync` presents in any
// refresh mode -- so an ordinary frame reads 1 present and a frame that walked
// out of the left doorway reads 41.  `slid` is the other
// half of it: a frame that scrolled is a deliberate hold rather than a frame
// that overran, so it is kept out of WORST (what it costs is a board reading).
void test_a_doorway_scrolls_and_a_death_does_not(void)
{
    run("setrefresh \"manual  init.game");
    run("make \"room.x 3  make \"room.y 4  draw.room");

    // Nobody to interrupt the walk: an unlucky robot at the doorway would
    // make this a death test rather than a doorway one.
    run("repeat 11 [.setitem repcount :r.state 0]  make \"rob.live 0");

    man_at(-129, 42);
    int before = refreshes();
    press(K_LEFT);
    frame();
    release(K_LEFT);

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f, num(":room.x"),
        "he did not walk out of the left doorway");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word_of(":slid"),
        "the frame that scrolled is not marked as one, so WORST is charged for it");
    char msg[160];
    snprintf(msg, sizeof(msg), "a doorway presented %d times, not 40 and a sync",
             refreshes() - before);
    TEST_ASSERT_EQUAL_INT_MESSAGE(41, refreshes() - before, msg);

    // And an ordinary frame is its own `sync` and nothing else.
    before = refreshes();
    frame();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, refreshes() - before,
        "an ordinary frame presented more than once");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word_of(":slid"),
        "an ordinary frame is marked as a scroll");

    // A death rebuilds without scrolling: fifteen frames of electrocution and
    // the build at the end of them, and not one present beyond their own.  It
    // still lands in a NEW room -- that is B74 -- and the point here is that it
    // gets there without the forty presents a doorway costs.
    run("make \"room.x 3  make \"room.y 4  draw.room  man.dies");
    before = refreshes();
    for (int f = 0; f < 16; f++)
        frame();
    snprintf(msg, sizeof(msg), "a death presented %d times over sixteen frames",
             refreshes() - before);
    TEST_ASSERT_EQUAL_INT_MESSAGE(16, refreshes() - before, msg);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word_of(":slid"),
        "the death scrolled");
}

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
    RUN_TEST(test_every_wall_runs_from_one_grid_line_to_the_next);
    RUN_TEST(test_a_room_change_clears_the_last_room);
    RUN_TEST(test_a_doorway_defers_the_masks_to_the_next_frame);
    RUN_TEST(test_a_frame_writes_at_most_one_block_of_text);
    RUN_TEST(test_the_worst_frame_is_the_body_and_not_the_padded_period);
    RUN_TEST(test_the_worst_transition_is_kept);
    RUN_TEST(test_the_man_is_four_costumes_at_the_cabinets_size);
    RUN_TEST(test_the_renderer_fills_the_cabinets_8_by_16);
    RUN_TEST(test_both_sides_of_the_man_are_one_costume);
    RUN_TEST(test_the_man_stamps_half_a_sprite_from_his_stored_corner);
    RUN_TEST(test_the_walk_is_the_roms_a_b_c_b_and_standing_is_its_own_frame);
    RUN_TEST(test_the_eight_directions_read_right_on_the_keyboard);
    RUN_TEST(test_opposite_arrows_stand_him_still);
    RUN_TEST(test_space_with_a_direction_stands_him_still_and_aims);
    RUN_TEST(test_an_interior_wall_kills_him);
    RUN_TEST(test_the_end_of_a_vertical_wall_kills_him);
    RUN_TEST(test_the_end_of_a_horizontal_wall_kills_him);
    RUN_TEST(test_the_border_kills_him_where_there_is_no_doorway);
    RUN_TEST(test_the_mans_outline_never_touches_a_wall);
    RUN_TEST(test_the_erase_covers_every_pixel_the_stamp_wrote);
    RUN_TEST(test_the_erase_touches_nothing_outside_the_man);
    RUN_TEST(test_the_four_doors_work);
    RUN_TEST(test_a_doorway_rebuilds_the_room_inside_the_frame);
    RUN_TEST(test_an_ordinary_frame_does_not_redraw_the_maze);
    RUN_TEST(test_a_death_holds_for_the_arcades_45_ticks_and_then_respawns);
    RUN_TEST(test_an_ordinary_frame_spends_no_cells);
    RUN_TEST(test_the_word_table_converges);
    RUN_TEST(test_pause_stops_the_man_and_can_be_lifted);
    RUN_TEST(test_esc_leaves_the_game);
    RUN_TEST(test_the_frame_cadence_is_seeded_before_the_loop);
    RUN_TEST(test_berzerk_puts_the_screen_back);
    RUN_TEST(test_the_last_life_is_worth_a_card_and_esc_is_not);
    RUN_TEST(test_the_game_sets_up_in_window_mode_and_flips);
    RUN_TEST(test_the_game_is_inside_the_procedure_ceiling);

    RUN_TEST(test_a_robot_walks_straight_at_the_man);
    RUN_TEST(test_seek_aims_two_below_the_mans_corner);
    RUN_TEST(test_iq_clears_the_directions_a_wall_forbids);
    RUN_TEST(test_iq_leaves_an_open_cell_alone);
    RUN_TEST(test_a_robot_in_the_players_cell_is_not_probed);
    RUN_TEST(test_iq_reuses_its_answer_until_a_corner_crosses);
    RUN_TEST(test_the_robot_count_is_a_five_room_cycle);
    RUN_TEST(test_the_full_room_is_always_eleven);
    RUN_TEST(test_the_first_room_is_the_second_of_the_cycle);
    RUN_TEST(test_the_crowd_reproduces_for_a_room_and_a_threshold);
    RUN_TEST(test_the_spawn_bands_are_the_cabinets_own);
    RUN_TEST(test_no_robot_spawns_touching_a_wall);
    RUN_TEST(test_a_robot_takes_about_a_second_a_pixel_at_the_start_of_a_game);
    RUN_TEST(test_killing_robots_speeds_up_the_ones_left);
    RUN_TEST(test_a_robot_only_outruns_the_man_as_the_last_one_left);
    RUN_TEST(test_the_rooms_step_rate_does_not_depend_on_how_many_robots_are_in_it);
    RUN_TEST(test_a_robot_never_leaves_the_room);
    RUN_TEST(test_two_robots_that_meet_both_die);
    RUN_TEST(test_robots_level_with_each_other_do_not_touch);
    RUN_TEST(test_a_robot_in_the_players_cell_walks_into_a_wall_and_dies);
    RUN_TEST(test_a_robot_that_reaches_the_man_kills_him);
    RUN_TEST(test_the_explosion_is_four_frames_and_then_the_slot_is_empty);
    RUN_TEST(test_the_explosion_stays_inside_the_robots_own_box);
    RUN_TEST(test_the_robot_is_four_costumes_at_the_cabinets_size);
    RUN_TEST(test_the_roms_left_facing_robot_is_its_right_one_mirrored);
    RUN_TEST(test_both_sides_of_a_robot_are_one_costume);
    RUN_TEST(test_every_direction_wears_the_roms_own_facing);
    RUN_TEST(test_a_robot_stamps_half_a_sprite_from_his_stored_corner);
    RUN_TEST(test_the_erase_covers_every_pixel_a_robot_stamped);
    RUN_TEST(test_a_dying_robot_is_still_erased);
    RUN_TEST(test_a_full_room_of_robots_leaves_the_workspace_where_it_found_it);
    RUN_TEST(test_a_doorway_brings_a_new_crowd);
    RUN_TEST(test_a_death_sends_him_to_another_room_and_costs_him_a_life);
    RUN_TEST(test_three_deaths_end_the_game_and_a_new_one_starts_over);
    RUN_TEST(test_the_ramp_reaches_the_floor_in_four_room_builds);
    RUN_TEST(test_a_frame_that_builds_a_room_writes_no_other_text);
    RUN_TEST(test_the_game_asks_for_the_fast_clock_and_reads_it_back);
    RUN_TEST(test_the_game_gives_the_clock_back_when_it_exits);
    RUN_TEST(test_a_board_that_will_not_overclock_is_told_why_and_does_not_play);
    RUN_TEST(test_the_three_firing_windows_at_their_boundaries);
    RUN_TEST(test_a_robots_shot_is_seeks_direction_masked_to_the_window);
    RUN_TEST(test_a_robot_takes_the_first_free_robot_slot_and_never_the_players);
    RUN_TEST(test_only_one_robot_fires_per_holdoff);
    RUN_TEST(test_a_firing_robot_stops_dead_and_stands);
    RUN_TEST(test_space_and_a_direction_starts_the_roms_own_bolt);
    RUN_TEST(test_the_man_holds_two_bolts_and_reloads_for_twelve_ticks);
    RUN_TEST(test_a_player_bolt_is_three_pixels_a_tick_and_a_robot_bolt_one);
    RUN_TEST(test_a_bolt_grows_to_the_roms_own_length);
    RUN_TEST(test_a_diagonal_bolt_is_drawn_its_full_length);
    RUN_TEST(test_a_bolt_dies_on_a_wall);
    RUN_TEST(test_a_bolt_that_lands_on_the_wall_line_dies_on_it);
    RUN_TEST(test_a_bolt_dies_at_the_border);
    RUN_TEST(test_a_bolt_kills_a_robot_and_pays_fifty);
    RUN_TEST(test_a_robots_bolt_kills_the_man);
    RUN_TEST(test_a_bolt_does_not_hit_whoever_fired_it);
    RUN_TEST(test_a_diagonal_bolt_misses_the_corner_of_its_own_bounding_box);
    RUN_TEST(test_the_difficulty_table_at_every_threshold);
    RUN_TEST(test_robots_do_not_shoot_in_the_first_maze);
    RUN_TEST(test_a_room_change_clears_the_bolts);
    RUN_TEST(test_the_roms_shoot_sprites_are_not_the_pairs_the_design_named);
    RUN_TEST(test_the_shooting_poses_are_five_costumes_at_the_mans_size);
    RUN_TEST(test_every_shot_direction_wears_the_roms_own_pose);
    RUN_TEST(test_a_bolt_does_not_kill_through_a_wall);
    RUN_TEST(test_a_bolt_stopped_by_a_wall_still_kills_what_is_in_front_of_it);
    RUN_TEST(test_a_diagonal_bolt_stopped_by_a_wall_kills_nothing_past_it);
    RUN_TEST(test_a_bolt_fired_along_a_wall_dies_on_it);
    RUN_TEST(test_a_bolt_beside_a_wall_and_at_the_grids_edge_lives);
    RUN_TEST(test_the_erase_retraces_the_stroke_the_bolt_drew);
    RUN_TEST(test_a_frame_with_bolts_in_the_air_spends_no_cells);
    RUN_TEST(test_the_maze_leaves_by_a_whole_screen_eight_pixels_at_a_time);
    RUN_TEST(test_the_slide_shows_the_room_it_is_leaving_and_not_the_one_ahead);
    RUN_TEST(test_a_doorway_scrolls_and_a_death_does_not);

    return UNITY_END();
}
