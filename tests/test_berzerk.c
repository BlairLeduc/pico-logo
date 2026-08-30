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

    // And each interior run is a cell dimension, drawn from an intersection:
    // 48 steps across, 68 down, or the 52 that the leftmost column is wide
    // (B67 -- the playfield is 244 as 52 + 4 x 48, and a run west out of the
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
// (B67).  `test_the_drawn_walls_agree_with_the_wall_masks` stands ten steps off
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

// THE END OF A WALL IS STILL A WALL, and a crossing test cannot see it (B66).
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
            run("draw.walls");
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

// THE ERASE HAS TO COVER THE PIXELS THE STAMP ACTUALLY WROTE, and B64 is what
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
            "the eraser is not pen 3, so it is not a square brush (B64)");
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
                         "he leaves a trail (B64)",
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

    int stamps = 0;
    for (int i = 0; i < 15; i++)
    {
        mock_device_clear_graphics();
        frame();
        stamps += mock_device_stamp_count();
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
// is, which is what M0's unbounded `r.time` counter was.  So: walk him for a
// thousand frames in every direction, through rooms and into walls, and then
// walk him for another thousand.  The second thousand must spend nothing at
// all.
void test_the_word_table_converges(void)
{
    static const int ways[8][2] = {
        { K_RIGHT, 0 }, { K_RIGHT, K_UP }, { K_UP, 0 }, { K_LEFT, K_UP },
        { K_LEFT, 0 }, { K_LEFT, K_DOWN }, { K_DOWN, 0 }, { K_RIGHT, K_DOWN },
    };
    run("setrefresh \"manual");
    in_room(9, 9);
    run("reset.man");

    float atoms_at_first_thousand = 0.0f;
    for (int i = 0; i < 2000; i++)
    {
        if (i == 1000)
            atoms_at_first_thousand = num("atoms");
        const int *way = ways[(i / 11) % 8];
        press(way[0]);
        if (way[1]) press(way[1]);
        frame();
        release(way[0]);
        if (way[1]) release(way[1]);
    }

    char msg[160];
    snprintf(msg, sizeof(msg),
             "the second thousand frames spent %g word-table bytes, so the set "
             "of numbers the man can hold is not closed",
             (double)(atoms_at_first_thousand - num("atoms")));
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(atoms_at_first_thousand, num("atoms"), msg);
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

    const MockDeviceState *st = mock_device_get_state();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("auto", word_of("refreshmode"),
        "the game left the display in sync refresh");
    TEST_ASSERT_EQUAL_INT_MESSAGE(MOCK_SCREEN_TEXT, st->screen_mode,
        "the game left the split screen up");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_get_turtle(0)->shape,
        "the game left the turtle wearing the man");
    TEST_ASSERT_TRUE_MESSAGE(st->turtle.visible, "the game left the turtle hidden");
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
    RUN_TEST(test_the_game_sets_up_in_window_mode_and_flips);
    RUN_TEST(test_the_game_is_inside_the_procedure_ceiling);
    return UNITY_END();
}
