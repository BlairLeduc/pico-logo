//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the Berzerk M0 timing harness (tests/logo/p15m0).
//
//  M0 is the gate in docs/berzerk-design.md section 19, and it has NOT run.
//  The design's whole budget is Battlezone's board figures divided by a
//  measured interpretation ratio, and this is the script that goes to a board
//  to find out which of those numbers are wrong.  Nothing here checks a
//  timing -- the host is far faster than the target and `ticks` has
//  millisecond resolution, so every figure the harness produces reads as zero
//  here.  What these tests check is that the script is worth carrying to a
//  board at all.
//
//  Four things are worth pinning even in a timing script:
//
//    * The GENERATOR has to be right, and reproducible.  Section 6.1's whole
//      claim is that a room is a pure function of its coordinates -- walk out
//      and back and it is the same room -- and that claim is free to make and
//      silently false to get wrong.  It is checked here rather than on a
//      board because a board cannot see it at all.
//    * `IQ` has to clear the right bits.  A wall test that lets a robot
//      through is invisible in a still scene and is the whole of section
//      6.3's replacement for the cabinet's hardware collision bit.
//    * THE SCENE HAS TO HOLD STILL.  The erase-in-place figure is only
//      comparable with the clear-and-redraw one if the two passes cover the
//      same pixels; a figure that moved between them leaves residue, the
//      residue widens the tile-row spans, and Q1 comes back biased against
//      the strategy section 3 already predicts will lose.  Three tests below
//      exist only for this.
//    * It has to run end to end, with the report reaching the file.  A script
//      that dies half way through wastes a hardware session (the p9m0
//      convention), and numbers on the PicoCalc's display cannot be copied
//      anywhere.
//

#include "test_mock_fs.h"
#include "test_scaffold.h"
#include "mock_device.h"
#include "core/repl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef P15M0_SOURCE
#error "P15M0_SOURCE must be defined (path to tests/logo/p15m0)"
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
    // _and_hardware gives a clock for `ticks`; the mock filesystem is here for
    // the report, which goes to a file as well as the screen because numbers
    // on the PicoCalc's display cannot be copied off it.
    test_scaffold_setUp_with_device_and_hardware();
    mock_fs_reset();
    logo_storage_init(&mock_storage, &mock_storage_ops);
    logo_io_init(&mock_io, mock_device_get_console(), &mock_storage, &mock_hardware);
    primitives_set_io(&mock_io);
    load_file(P15M0_SOURCE);
    // The harness sets `window` at load; nothing here may quietly undo it.
    run_string("window");
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

static const char *word(const char *expr)
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

// Every line the mock recorded, as a flat array, so two drawing passes can be
// compared point for point.
static int snapshot_lines(MockLine out[], int cap)
{
    int n = mock_device_line_count();
    TEST_ASSERT_LESS_OR_EQUAL_INT_MESSAGE(cap, n, "line snapshot overflowed");
    for (int i = 0; i < n; i++)
        out[i] = *mock_device_get_line(i);
    return n;
}

//==========================================================================
// The room, which is the half a board cannot see
//==========================================================================

// The generator at $2678, written a second time: a 16-bit LCG whose output is
// the high byte. Having it here means the harness's copy is checked against
// an independent one rather than against itself, which is the only way a
// transposed constant gets caught -- the maze would still look like a maze.
static unsigned lcg_next(unsigned seed)
{
    return ((7u * seed + 0x3153u) & 0xFFFFu) >> 8;
}

static unsigned lcg_step(unsigned seed)
{
    return (7u * seed + 0x3153u) & 0xFFFFu;
}

// `rnd` has to be THIS generator and not `random`: section 6.1's reproducible
// room is the whole reason the maze needs no storage, and `rerandom` reseeds a
// different stream that is not indexable by room coordinate.
void test_rnd_is_the_roms_own_generator(void)
{
    run("make \"seed 0");
    unsigned seed = 0;
    for (int i = 0; i < 200; i++)
    {
        unsigned expected = lcg_next(seed);
        seed = lcg_step(seed);
        char msg[96];
        snprintf(msg, sizeof(msg), "draw %d diverged from the ROM's LCG", i);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)expected, num("rnd"), msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)seed, num(":seed"), msg);
    }
}

// RNG_SEED := ROOM_X + 256.ROOM_Y at the top of every room build ($2540).
void test_the_seed_is_the_rooms_own_coordinates(void)
{
    run("make \"room.x 9  make \"room.y 4  make \"seed 0");
    // `setup.room` consumes eight draws, so the seed is read before it runs.
    run("make \"seed :room.x + (256 * :room.y)");
    TEST_ASSERT_EQUAL_FLOAT(9.0f + 256.0f * 4.0f, num(":seed"));
}

// Section 6.1's whole claim: `RNG_SEED := ROOM_X + 256.ROOM_Y` at every room
// build, so the maze is a pure function of where you are.  Walk right, walk
// back, and it is the same room -- which is what makes an infinite maze cost
// two globals and no storage.  This is the one test that proves it.
void test_a_room_is_a_function_of_its_coordinates(void)
{
    float first[15], elsewhere[15], again[15];

    run("make \"room.x 0  make \"room.y 0  setup.room");
    read_cell(first);

    run("make \"room.x 3  make \"room.y 7  setup.room");
    read_cell(elsewhere);

    run("make \"room.x 0  make \"room.y 0  setup.room");
    read_cell(again);

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
}

// And the segments reproduce with the masks, because they come out of the same
// stream: a room whose masks matched but whose drawn walls did not would be a
// room robots pathed through differently from the one on the screen.
void test_the_drawn_segments_reproduce_with_the_masks(void)
{
    float x[8], h[8], l[8];

    run("make \"room.x 5  make \"room.y 2  setup.room");
    for (int i = 0; i < 8; i++)
    {
        char e[32];
        snprintf(e, sizeof(e), "item %d :seg.x", i + 1);  x[i] = num(e);
        snprintf(e, sizeof(e), "item %d :seg.h", i + 1);  h[i] = num(e);
        snprintf(e, sizeof(e), "item %d :seg.l", i + 1);  l[i] = num(e);
    }

    run("make \"room.x 9  make \"room.y 9  setup.room");
    run("make \"room.x 5  make \"room.y 2  setup.room");

    for (int i = 0; i < 8; i++)
    {
        char e[32], msg[96];
        snprintf(msg, sizeof(msg), "segment %d did not reproduce", i + 1);
        snprintf(e, sizeof(e), "item %d :seg.x", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(x[i], num(e), msg);
        snprintf(e, sizeof(e), "item %d :seg.h", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(h[i], num(e), msg);
        snprintf(e, sizeof(e), "item %d :seg.l", i + 1);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(l[i], num(e), msg);
    }
}

// Section 6.2: CREATE_ROOM is called twice, once per interior row boundary,
// and each call steps x by 48 while x < 220 -- so the eight segments start on
// the eight interior grid intersections and nowhere else.  In turtle steps
// those are x = -70, -22, 26, 74 and y = 74, 6.
//
// Checked over sixteen rooms, because a generator that put a segment in the
// wrong place only for one value of `rand & 3` would pass a single room.
void test_the_eight_segments_start_on_the_eight_intersections(void)
{
    static const float xs[4] = { -70.0f, -22.0f, 26.0f, 74.0f };

    for (int ry = 0; ry < 4; ry++)
    {
        for (int rx = 0; rx < 4; rx++)
        {
            char cmd[80];
            snprintf(cmd, sizeof(cmd),
                     "make \"room.x %d  make \"room.y %d  setup.room", rx, ry);
            run(cmd);

            for (int k = 0; k < 8; k++)
            {
                char e[32], msg[128];
                snprintf(e, sizeof(e), "item %d :seg.x", k + 1);
                float sx = num(e);
                snprintf(e, sizeof(e), "item %d :seg.y", k + 1);
                float sy = num(e);

                snprintf(msg, sizeof(msg),
                         "room %d,%d segment %d started at %g,%g", rx, ry, k + 1,
                         (double)sx, (double)sy);
                TEST_ASSERT_EQUAL_FLOAT_MESSAGE(xs[k % 4], sx, msg);
                TEST_ASSERT_EQUAL_FLOAT_MESSAGE(k < 4 ? 74.0f : 6.0f, sy, msg);
            }
        }
    }
}

// The four choices, one at a time, against section 6.2's table.  `room.seg` is
// driven directly with a seed chosen so `rnd and 3` lands on each of them, and
// the mask the choice is supposed to set is the assertion.
//
// The mapping is the `set` instructions at $2617-$264B against ix+0, ix+1,
// ix+5 and ix+6 -- an index into the 5-wide array where +1 is the next column
// and +5 the next row.
void test_each_of_the_four_choices_sets_the_bits_the_rom_sets(void)
{
    // Intersection (c=2, r=1): above-left is cell 2, above-right 3,
    // below-left 7, below-right 8.
    struct { int choice; int cell; int bit; const char *what; } expect[] = {
        { 2, 3, 8, "horizontal right: BOTTOM of above-right" },
        { 2, 8, 4, "horizontal right: TOP of below-right" },
        { 3, 2, 8, "horizontal left: BOTTOM of above-left" },
        { 3, 7, 4, "horizontal left: TOP of below-left" },
        { 0, 2, 2, "vertical above: RIGHT of above-left" },
        { 0, 3, 1, "vertical above: LEFT of above-right" },
        { 1, 7, 2, "vertical below: RIGHT of below-left" },
        { 1, 8, 1, "vertical below: LEFT of below-right" },
    };

    for (size_t i = 0; i < sizeof(expect) / sizeof(expect[0]); i++)
    {
        // A seed whose next draw lands on this choice, found with the C copy
        // of the generator above rather than by asking the harness -- so the
        // test is not using the thing it is checking to set itself up.
        int seed = 0;
        for (; seed < 65536; seed++)
            if ((int)(lcg_next(seed) & 3) == expect[i].choice)
                break;
        TEST_ASSERT_LESS_THAN_INT_MESSAGE(65536, seed, "no seed produced this choice");

        char cmd[128];
        snprintf(cmd, sizeof(cmd),
                 "make \"cell (list 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)  "
                 "make \"seed %d  room.seg 1 2 1", seed);
        run(cmd);

        char e[32];
        snprintf(e, sizeof(e), "item %d :cell", expect[i].cell);
        int mask = (int)num(e);
        TEST_ASSERT_TRUE_MESSAGE((mask & expect[i].bit) == expect[i].bit, expect[i].what);
    }
}

// The template at $268C, which the eight segments then set their bits into.
// The border cells are walled on all four outer sides EVEN WHERE THE EXITS
// ARE -- cell (row 1, col 0) carries a LEFT wall although the left doorway is
// exactly there. That is not a bug: the table is what robots consult, and
// robots never leave the room.
void test_the_wall_template_walls_the_border_including_the_doorways(void)
{
    run("make \"room.x 0  make \"room.y 0  make \"seed 0");
    run("make \"cell (list 5 4 4 4 6 1 0 0 0 2 9 8 8 8 10)");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, num("bitand 1 item 6 :cell"),
        "the left-middle cell lost its LEFT wall, which the doorway is in");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f, num("bitand 2 item 10 :cell"),
        "the right-middle cell lost its RIGHT wall");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4.0f, num("bitand 4 item 3 :cell"),
        "the top-middle cell lost its TOP wall");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(8.0f, num("bitand 8 item 13 :cell"),
        "the bottom-middle cell lost its BOTTOM wall");
}

// WALLINDEX ($1CE7).  Column boundaries at turtle x = -70, -22, 26, 74 and row
// boundaries at y = 74, 6, with the corners of the playfield in the corner
// cells.  The clamps matter as much as the arithmetic: `iq` probes points four
// steps outside the robot, which at a wall are outside the playfield.
void test_a_point_maps_to_the_cell_the_rom_would_pick(void)
{
    struct { float x, y; int cell; const char *what; } cases[] = {
        { -122.0f,  142.0f,  1, "top-left corner" },
        {  122.0f,  142.0f,  5, "top-right corner" },
        { -122.0f,  -62.0f, 11, "bottom-left corner" },
        {  122.0f,  -62.0f, 15, "bottom-right corner" },
        {    0.0f,   40.0f,  8, "the middle of the room" },
        {  -71.0f,   75.0f,  1, "one step inside the first boundary" },
        {  -70.0f,   74.0f,  7, "exactly on both boundaries" },
        { -300.0f,  400.0f,  1, "far outside, clamped into the corner" },
        {  300.0f, -400.0f, 15, "far outside the other way" },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        char e[64];
        snprintf(e, sizeof(e), "cell.at %g %g", (double)cases[i].x, (double)cases[i].y);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)cases[i].cell, num(e), cases[i].what);
    }
}

//==========================================================================
// SEEK and IQ
//==========================================================================

// SEEK ($23EF) is the whole AI: LEFT 1, RIGHT 2, UP 4, DOWN 8, and the
// arcade's `player.y + 2` is `-2` here because turtle y runs the other way.
// Robots walk straight at you; their entire tactical repertoire is walking
// into things.
void test_seek_is_the_four_durl_bits(void)
{
    run("make \"p.x 0  make \"p.y 40");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(2.0f, num("seek -50 38"), "player to the right");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, num("seek 50 38"), "player to the left");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(4.0f, num("seek 0 -20"), "player above");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(8.0f, num("seek 0 100"), "player below");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(6.0f, num("seek -50 -20"), "player up and right");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(9.0f, num("seek 50 100"), "player down and left");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num("seek 0 38"),
        "a robot on the player's own point still asked to move");
}

// IQ ($1C6E), in the direction that matters: a robot against a wall may not
// walk into it.  The mask table is section 6.3's replacement for the cabinet's
// hardware pixel-intercept bit, so this is the game's entire wall collision
// system and it is four statements.
void test_iq_clears_a_direction_whose_edge_is_walled(void)
{
    // A robot in the middle cell (index 8) with the player somewhere else, so
    // the $1C92 shortcut does not fire, and every edge of that cell walled.
    run("make \"p.x -110  make \"p.y 130");
    run("make \"cell (list 0 0 0 0 0 0 0 15 0 0 0 0 0 0 0)");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num("iq 0 40 15"),
        "every edge walled and IQ still allowed a direction");

    run("make \"cell (list 0 0 0 0 0 0 0 8 0 0 0 0 0 0 0)");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(7.0f, num("iq 0 40 15"),
        "a BOTTOM wall must clear DOWN and nothing else");

    run("make \"cell (list 0 0 0 0 0 0 0 4 0 0 0 0 0 0 0)");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(11.0f, num("iq 0 40 15"),
        "a TOP wall must clear UP and nothing else");

    run("make \"cell (list 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0)");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(13.0f, num("iq 0 40 15"),
        "a RIGHT wall must clear RIGHT and nothing else");

    run("make \"cell (list 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0)");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(14.0f, num("iq 0 40 15"),
        "a LEFT wall must clear LEFT and nothing else");
}

// The other direction, stated positively: a robot in open ground keeps every
// bit it asked for.  An `iq` that quietly cleared bits would make robots
// stand still and would read on a board as the AI being cheap.
void test_iq_in_open_ground_keeps_every_bit(void)
{
    run("make \"p.x -110  make \"p.y 130");
    run("make \"cell (list 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)");
    TEST_ASSERT_EQUAL_FLOAT(15.0f, num("iq 0 40 15"));
    TEST_ASSERT_EQUAL_FLOAT(6.0f, num("iq 0 40 6"));
}

// And the shortcut at $1C92, which is the cheap path this loop takes most
// often when the player is being chased into a corner: a robot in the
// player's own cell is not probed at all.
void test_a_robot_in_the_players_cell_is_not_probed(void)
{
    run("make \"p.x 0  make \"p.y 40");
    run("make \"cell (list 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15)");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(15.0f, num("iq 4 44 15"),
        "the same-cell shortcut probed anyway");
}

// SHOOT ($287F): three windows, and the case that refuses.  The 2600 manual's
// "robots cannot shoot on the diagonal" is right about the 2600 and wrong
// about the arcade -- the third window is right there.
void test_the_three_firing_windows_at_their_boundaries(void)
{
    run("make \"p.x 0  make \"p.y 0");

    // Vertical window: dx = p.x - rx in -2 .. +5.
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word("fires? -4 200"), "dx = 4, inside");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word("fires? 1 200"), "dx = -1, inside");
    // Horizontal window: dy = ry - p.y in -4 .. +6.
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word("fires? 200 5"), "dy = 5, inside");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word("fires? 200 -3"), "dy = -3, inside");
    // Diagonal window: abs(dy) - abs(dx) in -10 .. +5.
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word("fires? 100 100"), "on the diagonal");
    // And the refusal, which is the reason the procedure exists.
    TEST_ASSERT_EQUAL_STRING_MESSAGE("false", word("fires? 200 20"),
        "a robot well off every window fired anyway");
}

//==========================================================================
// The models
//==========================================================================

// The sprites are the disassembly's, verbatim. Transcribing sixty bytes by
// hand is exactly the kind of thing that goes wrong silently -- a robot with a
// mistyped row still looks like a robot -- so the invariants the ROM's own
// format guarantees are checked instead of the bytes being trusted.
void test_the_robot_sprites_are_the_roms_own_bytes(void)
{
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5, num("count :rob.bits"),
        "there are five facing groups at $1000-$1030");

    for (int g = 1; g <= 5; g++)
    {
        char e[48], msg[128];
        snprintf(e, sizeof(e), "count item %d :rob.bits", g);
        snprintf(msg, sizeof(msg), "facing group %d is not twelve rows", g);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(12.0f, num(e), msg);

        // Every byte is a byte. A transcription that dropped a digit or ran
        // two together lands outside 0..255 far more often than not.
        for (int r = 1; r <= 12; r++)
        {
            snprintf(e, sizeof(e), "item %d item %d :rob.bits", r, g);
            float b = num(e);
            snprintf(msg, sizeof(msg), "group %d row %d is %g", g, r, (double)b);
            TEST_ASSERT_TRUE_MESSAGE(b >= 0.0f && b <= 255.0f, msg);
        }

        // The shoulders ($xx02 = FF) and the head ($xx01 = 3C) are common to
        // all five frames in the ROM; the eye row and the feet are what differ.
        snprintf(e, sizeof(e), "item 1 item %d :rob.bits", g);
        snprintf(msg, sizeof(msg), "group %d has the wrong head row", g);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(60.0f, num(e), msg);
        snprintf(e, sizeof(e), "item 3 item %d :rob.bits", g);
        snprintf(msg, sizeof(msg), "group %d has the wrong shoulder row", g);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(255.0f, num(e), msg);
    }

    // The eye row carries the facing -- 66 centre, 7E up, 1E left, 78 right --
    // so no two facing groups may share one, or two directions would be
    // indistinguishable on screen. ($1139, the "down" frame, keeps the centred
    // eyes and differs in its feet, so it is excluded from the comparison.)
    static const int eyes[5] = { 102, 126, 102, 30, 120 };
    for (int g = 1; g <= 5; g++)
    {
        char e[48], msg[96];
        snprintf(e, sizeof(e), "item 2 item %d :rob.bits", g);
        snprintf(msg, sizeof(msg), "facing group %d has the wrong eye row", g);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE((float)eyes[g - 1], num(e), msg);
    }

    // The man is $10BF: one byte wide, sixteen rows.
    TEST_ASSERT_EQUAL_FLOAT(16, num("count :man.bits"));
}

// The renderer has to put the bitmap where `snapsh` will look for it, and at
// the cabinet's size. This is the test that would catch a row-order flip or an
// off-by-one in the row walk, neither of which changes any count.
void test_the_renderer_draws_the_sprite_at_the_cabinets_size(void)
{
    run("clean");
    mock_device_clear_graphics();
    run("render.sprite :rob.b.1 0 0");

    float lo_x = 1e9f, hi_x = -1e9f, lo_y = 1e9f, hi_y = -1e9f;
    for (int k = 0; k < mock_device_line_count(); k++)
    {
        const MockLine *l = mock_device_get_line(k);
        float xs[2] = { l->x1, l->x2 }, ys[2] = { l->y1, l->y2 };
        for (int q = 0; q < 2; q++)
        {
            if (xs[q] < lo_x) lo_x = xs[q];
            if (xs[q] > hi_x) hi_x = xs[q];
            if (ys[q] < lo_y) lo_y = ys[q];
            if (ys[q] > hi_y) hi_y = ys[q];
        }
    }

    char msg[160];
    snprintf(msg, sizeof(msg), "the sprite rendered x %g..%g y %g..%g",
             (double)lo_x, (double)hi_x, (double)lo_y, (double)hi_y);
    // Eight pixels wide from the origin, eleven rows down (row 12 is blank in
    // the standing frame), which is the cabinet's 8 x 11 at 1:1.
    TEST_ASSERT_TRUE_MESSAGE(lo_x >= -0.5f && hi_x <= 8.5f, msg);
    TEST_ASSERT_TRUE_MESSAGE(lo_y >= -10.5f && hi_y <= 0.5f, msg);
    TEST_ASSERT_TRUE_MESSAGE(hi_x - lo_x > 6.5f, msg);
    TEST_ASSERT_TRUE_MESSAGE(hi_y - lo_y > 9.5f, msg);
}

// `putsh` would have been the obvious way to carry a ROM bitmap and it is the
// wrong one: it doubles every pixel horizontally on the way in
// (`turtle_put_shape_data`), so the cabinet's 8-wide robot renders 16 wide in a
// 48-wide cell, which changes dodging and robot-versus-wall collisions. This
// test does not exercise `putsh`; it pins the decision that follows from it --
// the capture is 8 wide, and a capture that quietly grew would take the
// playfield off 1:1 without any count changing.
void test_the_costumes_are_captured_at_the_cabinets_width(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room");
    run("cache.models");

    const MockDeviceState *st = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, st->costume.last_snap_w,
        "the costume is not eight pixels wide, so the playfield is not 1:1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(16, st->costume.last_snap_h,
        "the man is not sixteen rows");
}

// Section 15.4's avoidance, checked rather than assumed: one list of five
// procedure names indexed by facing group and reached with `run`, no `if`
// chain.  Asteroids measured its nine-way `if` dispatch at 360-398 us a rock a
// pass, which at eleven robots would be 2 ms the budget does not have.
//
// A list of LISTS, so `run item :g :r.models` allocates nothing.
void test_the_model_dispatch_is_five_lists_reached_by_facing_group(void)
{
    TEST_ASSERT_EQUAL_FLOAT(5, num("count :r.models"));
    TEST_ASSERT_EQUAL_FLOAT(16, num("count :d.grp"));
    for (int i = 1; i <= 5; i++)
    {
        char e[48], msg[96];
        snprintf(e, sizeof(e), "count item %d :r.models", i);
        snprintf(msg, sizeof(msg), "model %d is not a one-word list", i);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, num(e), msg);
    }
    // Every facing group in the table is one of the five.
    for (int m = 0; m < 16; m++)
    {
        char e[48], msg[96];
        snprintf(e, sizeof(e), "item %d :d.grp", m + 1);
        float g = num(e);
        snprintf(msg, sizeof(msg), "direction mask %d dispatches to group %g", m, (double)g);
        TEST_ASSERT_TRUE_MESSAGE(g >= 1.0f && g <= 5.0f, msg);
    }
}

// The direction tables are read by index every frame, so a table edited
// unevenly is a silent out-of-range read rather than a visible defect. And
// their contents have to agree: `d.h` must point the way `d.dx`/`d.dy` move.
void test_the_direction_tables_agree_with_each_other(void)
{
    TEST_ASSERT_EQUAL_FLOAT(16, num("count :d.dx"));
    TEST_ASSERT_EQUAL_FLOAT(16, num("count :d.dy"));
    TEST_ASSERT_EQUAL_FLOAT(16, num("count :d.h"));

    for (int m = 0; m < 16; m++)
    {
        char e[48], msg[128];
        snprintf(e, sizeof(e), "item %d :d.dx", m + 1);
        float dx = num(e);
        snprintf(e, sizeof(e), "item %d :d.dy", m + 1);
        float dy = num(e);
        snprintf(e, sizeof(e), "item %d :d.h", m + 1);
        float h = num(e);

        snprintf(msg, sizeof(msg), "mask %d moves %g,%g but heads %g",
                 m, (double)dx, (double)dy, (double)h);
        if (dx == 0.0f && dy == 0.0f)
            continue;                       // degenerate masks keep heading 0
        // Turtle heading 0 is north and 90 is east, so sin/cos of the heading
        // must have the sign of the step.
        float sx = sinf(h * 3.14159265f / 180.0f);
        float sy = cosf(h * 3.14159265f / 180.0f);
        TEST_ASSERT_TRUE_MESSAGE(dx * sx >= -0.001f, msg);
        TEST_ASSERT_TRUE_MESSAGE(dy * sy >= -0.001f, msg);
        TEST_ASSERT_TRUE_MESSAGE(fabsf(dx) + fabsf(dy) > 0.0f, msg);
    }
}

// The eleven spawn cells, transcribed from the Vectrex's own table: the 5 x 3
// grid less the four doorway cells, which is why there are exactly eleven
// robots and not fifteen.
void test_the_spawn_table_is_the_eleven_cells_that_are_not_doorways(void)
{
    TEST_ASSERT_EQUAL_FLOAT(11, num("count :rob.sx"));
    TEST_ASSERT_EQUAL_FLOAT(11, num("count :rob.sy"));

    // No entry is a doorway cell: 3 (top middle), 6 (left middle),
    // 10 (right middle) or 13 (bottom middle).
    for (int i = 1; i <= 11; i++)
    {
        char e[64], msg[96];
        snprintf(e, sizeof(e), "cell.at (item %d :rob.sx) (item %d :rob.sy)", i, i);
        int c = (int)num(e);
        snprintf(msg, sizeof(msg), "spawn %d is in cell %d, which is a doorway", i, c);
        TEST_ASSERT_TRUE_MESSAGE(c != 3 && c != 6 && c != 10 && c != 13, msg);
    }
    // And all eleven are different cells.
    for (int i = 1; i <= 11; i++)
    {
        for (int j = i + 1; j <= 11; j++)
        {
            char a[64], b[64], msg[96];
            snprintf(a, sizeof(a), "cell.at (item %d :rob.sx) (item %d :rob.sy)", i, i);
            snprintf(b, sizeof(b), "cell.at (item %d :rob.sx) (item %d :rob.sy)", j, j);
            snprintf(msg, sizeof(msg), "spawns %d and %d share a cell", i, j);
            TEST_ASSERT_TRUE_MESSAGE((int)num(a) != (int)num(b), msg);
        }
    }
}

// The border is one closed circuit with the pen toggling -- twelve statements
// and one `setpos`.  If it does not close, one side of the room is missing,
// which reads as a rendering bug rather than as a bad table.
void test_the_border_is_a_closed_circuit_with_four_doorways(void)
{
    run("clean");
    mock_device_clear_graphics();
    run("draw.border");

    // Eight runs: two either side of each of the four doorways.
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, mock_device_line_count(),
        "the border is not eight runs, so a doorway is missing or doubled");

    // It closes: the turtle ends where it started.
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -122.0f, num("xcor"));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 142.0f, num("ycor"));

    // And every stroke is on the playfield boundary, never across it.
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

//==========================================================================
// The stamped alternative (§7.6's question, priced rather than decided)
//==========================================================================

// One costume per facing group plus the man, captured from the pen models
// themselves. That is what makes this a render cache rather than a redesign:
// the `rob.*` procedures stay the artwork and `snapsh` picks up what they
// draw.
void test_the_cache_captures_one_costume_per_model(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room");
    int before = mock_device_get_state()->costume.snap_count;
    run("cache.models");
    int captured = mock_device_get_state()->costume.snap_count - before;

    char msg[96];
    snprintf(msg, sizeof(msg), "captured %d costumes, not six", captured);
    TEST_ASSERT_EQUAL_INT_MESSAGE(6, captured, msg);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("true", word(":p15.cached"),
        "the cache did not mark itself built");
}

// MAX_TURTLES is 8 and the room holds eleven robots, a man, Otto and seven
// bolts, so one turtle per robot was never available. `stamp` is the mechanism
// and this is what it costs in marks: one stamp and no strokes, where the pen
// figures this file used to draw cost eleven strokes and 2.00 ms.
void test_a_stamped_robot_is_one_stamp_and_no_strokes(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("cache.models");

    run("clean");
    mock_device_clear_graphics();
    run("draw.robot 1");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, mock_device_stamp_count(),
        "a robot is not exactly one stamp");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_line_count(),
        "a robot still drew strokes");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_dot_count(),
        "a robot drew a dot, and a dot conses the list it is given");
}

// A costume is centred on the turtle at BOTH ends -- capture and stamp -- while
// a robot's stored position is the sprite's top-left corner, the arcade's, and
// what `iq` is cut against. The offset that reconciles them is the whole
// difference between a figure that lands right and one displaced by half a
// sprite, and no count changes either way.
//
// An 8 x 12 sprite drawn from (x, y) downward has its centre at
// (x + 3.5, y - 5.5), which is what `cache.models` captures around.
void test_a_stamped_robot_lands_half_a_sprite_from_its_stored_corner(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("frame.inplace 11");
    run("cache.models");

    for (int i = 1; i <= 11; i++)
    {
        char e[48], msg[160];
        snprintf(e, sizeof(e), "item %d :r.x", i);
        float rx = num(e);
        snprintf(e, sizeof(e), "item %d :r.y", i);
        float ry = num(e);

        run("clean");
        mock_device_clear_graphics();
        snprintf(e, sizeof(e), "draw.robot %d", i);
        run(e);

        TEST_ASSERT_EQUAL_INT(1, mock_device_stamp_count());
        const MockStamp *st = mock_device_get_stamp(0);
        snprintf(msg, sizeof(msg),
                 "robot %d stored at %g,%g stamped at %g,%g", i,
                 (double)rx, (double)ry, (double)st->x, (double)st->y);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, rx + 3.5f, st->x, msg);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, ry - 5.5f, st->y, msg);
    }
}

// And the facing group picks the slot: a robot walking left must not stamp the
// costume captured from the robot walking right. The slot is the same `d.grp`
// index the `run` dispatch uses, so the two paths cannot disagree about which
// model a direction means.
void test_a_stamped_robot_wears_the_costume_for_its_facing(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("cache.models");

    for (int mask = 0; mask < 16; mask++)
    {
        char cmd[96], msg[128];
        snprintf(cmd, sizeof(cmd), ".setitem 1 :r.dir %d", mask);
        run(cmd);

        snprintf(cmd, sizeof(cmd), "item %d :d.grp", mask + 1);
        int group = (int)num(cmd);

        run("clean");
        mock_device_clear_graphics();
        run("draw.robot 1");

        snprintf(msg, sizeof(msg),
                 "direction mask %d is facing group %d but stamped slot %d",
                 mask, group, mock_device_get_stamp(0)->shape);
        TEST_ASSERT_EQUAL_INT_MESSAGE(group, mock_device_get_stamp(0)->shape, msg);
    }
    run(".setitem 1 :r.dir 0");
}

// A stamped frame draws every figure the pen frame does, and the bolts stay
// strokes: §7.2's bolt IS a line segment, and its drawn length grows to
// MaxLength, so a fixed costume is the wrong shape for it twice over.
void test_the_stamped_frame_stamps_the_figures_and_still_draws_the_bolts(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("frame.clear 11");
    run("cache.models");

    mock_device_clear_graphics();
    run("frame.clear 11");

    TEST_ASSERT_EQUAL_INT_MESSAGE(12, mock_device_stamp_count(),
        "expected eleven robots and the man as stamps");
    // The walls (8 border runs + 8 interior), Otto and seven bolts are still
    // strokes, so the stamped frame is far from strokeless.
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() > 16,
        "the walls and bolts stopped being drawn");
}

// Erasing a stamp is not a pen colour -- a costume carries its own pixels --
// so the in-place variant covers each figure with a wide background stroke.
// If that ever stopped covering the sprite the erase would leave a comb of
// residue, which is the same hazard the pen path has and the same fix.
void test_the_stamped_in_place_erase_covers_every_figure(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("frame.clear 11");
    run("cache.models");

    mock_device_clear_graphics();
    run("erase.actors 11");

    // One stroke a robot, plus the man, plus Otto, plus the seven bolts.
    TEST_ASSERT_TRUE_MESSAGE(mock_device_line_count() >= 13,
        "the erase pass does not cover one stroke per figure");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_stamp_count(),
        "the erase pass stamped, which cannot erase anything");

    // And it puts the pen back: a frame that left pen size at 8 would draw
    // every wall and bolt after it eight pixels wide.
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, num("pensize"),
        "the erase pass left the pen wide");
}

//==========================================================================
// The frame
//==========================================================================

// `frame.clear` and `frame.split` have to stay in step.  The first is what the
// body, min and max columns report and the second carries the phase timers
// that split logic from drawing; the difference between them is reported as
// the instrumentation cost, which is a lie the moment they draw different
// things.
void test_the_timed_and_untimed_frames_draw_the_same_thing(void)
{
    static MockLine a[512], b[512];

    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("cache.models");
    run("frame.inplace 11");          // settle `r.dir`

    mock_device_clear_graphics();
    run("frame.inplace 11");
    int na = snapshot_lines(a, 512);
    int da = mock_device_dot_count();

    mock_device_clear_graphics();
    run("frame.split 11");
    int nb = snapshot_lines(b, 512);
    int db = mock_device_dot_count();

    TEST_ASSERT_EQUAL_INT_MESSAGE(na, nb, "the instrumented frame drew a different scene");
    TEST_ASSERT_EQUAL_INT(da, db);
    for (int i = 0; i < na; i++)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "line %d differs between the two frames", i);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(a[i].x1, b[i].x1, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(a[i].y1, b[i].y1, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(a[i].x2, b[i].x2, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(a[i].y2, b[i].y2, msg);
    }
}

// THE SCENE HOLDS STILL, and this is the test the whole erase comparison
// rests on.  Erase-in-place draws the figures in the background colour and
// then in the foreground; if anything moved between one frame and the next,
// the erase takes away the wrong pixels, the residue widens the tile-row
// spans, and Q1 comes back biased against the strategy section 3 already
// predicts will lose.
//
// Three things had to be held for this to pass and each was a real leak: the
// bolt advance, Otto's bounce counter, and the robot directions before the
// first erase.
void test_two_consecutive_frames_draw_identical_geometry(void)
{
    static MockLine a[512], b[512];

    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("frame.clear 11");

    mock_device_clear_graphics();
    run("frame.clear 11");
    int na = snapshot_lines(a, 512);

    mock_device_clear_graphics();
    run("frame.clear 11");
    int nb = snapshot_lines(b, 512);

    TEST_ASSERT_EQUAL_INT_MESSAGE(na, nb, "the scene moved between two frames");
    for (int i = 0; i < na; i++)
    {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "line %d moved from %g,%g-%g,%g to %g,%g-%g,%g", i,
                 (double)a[i].x1, (double)a[i].y1, (double)a[i].x2, (double)a[i].y2,
                 (double)b[i].x1, (double)b[i].y1, (double)b[i].x2, (double)b[i].y2);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(a[i].x1, b[i].x1, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(a[i].y1, b[i].y1, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(a[i].x2, b[i].x2, msg);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(a[i].y2, b[i].y2, msg);
    }
}

// Erase-in-place is now one STROKE pass to cover the figures and one STAMP
// pass to put them back, and it must never redraw the walls -- if it did it
// would be clear-and-redraw with extra steps and Q1 would compare a strategy
// with itself.
void test_erase_in_place_covers_the_figures_and_leaves_the_walls_alone(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("cache.models");
    run("frame.inplace 11");

    mock_device_clear_graphics();
    run("frame.inplace 11");

    TEST_ASSERT_EQUAL_INT_MESSAGE(12, mock_device_stamp_count(),
        "the in-place frame is not exactly one stamp pass over the figures");

    // Asserted against the walls' own signature rather than a line count: a
    // border run lies on |x| = 122 or on y = 142 / -62, and nothing else in
    // the frame goes near those.
    for (int k = 0; k < mock_device_line_count(); k++)
    {
        const MockLine *l = mock_device_get_line(k);
        bool on_border =
            (fabsf(fabsf(l->x1) - 122.0f) < 0.5f && fabsf(fabsf(l->x2) - 122.0f) < 0.5f) ||
            (fabsf(l->y1 - 142.0f) < 0.5f && fabsf(l->y2 - 142.0f) < 0.5f) ||
            (fabsf(l->y1 + 62.0f) < 0.5f && fabsf(l->y2 + 62.0f) < 0.5f);
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "the in-place frame redrew a wall: %g,%g to %g,%g",
                 (double)l->x1, (double)l->y1, (double)l->x2, (double)l->y2);
        TEST_ASSERT_FALSE_MESSAGE(on_border, msg);
    }
}

// THE FRAME MUST NOT ALLOCATE, and this is the test the first board run
// needed and did not have: `p15m0` died with `out of space` inside `rob.left`
// rather than reporting a number, which costs a hardware session.
//
// Two things were consing, and the second is the interesting one:
//
//   * The eyes were two `dot`s a robot, and `dot` takes a LIST -- 24 cells a
//     drawing pass, 48 in an erase-in-place frame. They are strokes now.
//   * `.setitem` of a value the list has not held before allocates a cell,
//     and `r.time` was an unbounded counter written eleven times a frame.
//     Writing the SAME value back costs nothing, so bounding the counter --
//     which is what the cabinet's own 60 Hz TIME does anyway -- fixes it.
//
// Neither is visible in a timing column; both empty the pool.
void test_the_frame_allocates_nothing(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("frame.clear 11");        // warm up: the first frame mints its values

    run("recycle  repeat 100 [frame.clear 11]");   // warm: intern the timer cycle
    run("make \"probe nodes");
    run("repeat 200 [frame.clear 11]  repeat 200 [frame.inplace 11]");
    float consumed = num(":probe") - num("nodes");

    char msg[160];
    snprintf(msg, sizeof(msg),
             "400 frames consumed %g cells; at that rate a full harness run "
             "empties the pool and the board reports `out of space`",
             (double)consumed);
    TEST_ASSERT_TRUE_MESSAGE(consumed < 200.0f, msg);
}

// And the harness's own answer to the same question, which is the one the
// report carries off the board. It reads both resources because B25's finding
// is that they fail apart: that frame loop died with 21,000 free nodes and 20
// free bytes of word table, so a harness watching `nodes` watches the wrong
// one.
void test_the_harness_measures_both_resources_and_reports_zero(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("alloc.per.frame");

    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":p15.alloc.n"),
        "100 warm frames consed, so a long board run ends in `out of space`");
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, num(":p15.alloc.a"),
        "100 warm frames spent word-table bytes, which is what B25 died of");
}

// The word table is the resource, and the way to prove a frame does not spend
// it is to show the cost is a WARM-UP and not a rate. The robot timers cycle
// through 60 values, each interned once; a cold hundred frames charges those
// and a warm hundred charges nothing, for as long as you care to run it.
void test_the_frames_word_table_cost_is_warm_up_and_not_a_rate(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("recycle  repeat 50 [frame.clear 11]  repeat 50 [frame.inplace 11]");

    run("make \"probe atoms");
    run("repeat 400 [frame.clear 11]");
    float first = num(":probe") - num("atoms");

    run("make \"probe atoms");
    run("repeat 400 [frame.inplace 11]");
    float second = num(":probe") - num("atoms");

    char msg[160];
    snprintf(msg, sizeof(msg),
             "400 warm frames spent %g word bytes after %g in the run before, "
             "so it is a rate and not a settling cost",
             (double)second, (double)first);
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(0.0f, second, msg);
}

// The budget is `c + m.n` and the report prints the slope, so the frame has to
// actually grow by one robot at a time -- one stamp each now, where the pen
// figures grew by eleven strokes.
void test_the_frame_grows_by_one_robot_at_a_time(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("cache.models");
    run("frame.inplace 11");

    mock_device_clear_graphics();
    run("frame.inplace 1");
    int one = mock_device_stamp_count();

    mock_device_clear_graphics();
    run("frame.inplace 11");
    int eleven = mock_device_stamp_count();

    TEST_ASSERT_EQUAL_INT_MESSAGE(10, eleven - one,
        "ten more robots did not cost ten more stamps");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, mock_device_dot_count(),
        "the frame draws a dot somewhere, and a dot conses the list");
}

// The dynamic-scope hazard, which cost P13 M0 its most important column: a
// `make` inside a procedure the frame calls finds the CALLER's local and
// writes the measure accumulator instead of the global. `local` protects a
// name from the world; it does not protect the world from a name.
void test_the_frame_does_not_write_the_measure_accumulators(void)
{
    static const char *accumulators[] = {
        "m.b", "m.l", "m.d", "m.r", "m.one", "m.t0", "m.lo", "m.hi", "m.ins", "m.n"
    };

    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");

    for (size_t i = 0; i < sizeof(accumulators) / sizeof(accumulators[0]); i++)
    {
        char cmd[192], msg[128];
        snprintf(cmd, sizeof(cmd),
                 "make \"%s 12345  frame.clear 11  frame.inplace 11  frame.split 11  "
                 "make \"probe :%s", accumulators[i], accumulators[i]);
        run(cmd);
        snprintf(msg, sizeof(msg), "a frame wrote the accumulator `%s`", accumulators[i]);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(12345.0f, num(":probe"), msg);
    }
}

//==========================================================================
// The board furniture
//==========================================================================

// The harness may be pointed at a clock, and it must report the one the
// hardware actually took rather than the one it was asked for -- a board that
// refused the change would otherwise read as an overclock that bought
// nothing. Section 15.5 makes 300 MHz a precondition, so this is the column
// the whole gate is read against.
void test_the_harness_reports_the_clock_it_actually_ran_at(void)
{
    run("make \"p15m0.frames 2");

    run("make \"p15m0.cpu \"fast");
    run("p15m0");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("fast", word(":p15m0.cpu.ran"),
        "asked for fast and the mock can make it");

    run("make \"p15m0.cpu \"normal");
    run("p15m0");
    TEST_ASSERT_EQUAL_STRING("normal", word(":p15m0.cpu.ran"));
    run("make \"p15m0.cpu \"same");
}

// `same` means "leave the board wherever it was", which is what every run
// before that feature existed did.
void test_the_same_clock_leaves_the_board_alone(void)
{
    run("make \"p15m0.frames 2");
    run("make \"p15m0.cpu \"fast");
    run("p15m0");

    run("make \"p15m0.cpu \"same");
    run("p15m0");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("fast", word(":p15m0.cpu.ran"),
        "`same` retuned the board instead of leaving it");

    run("hw.setcpu \"normal");
    run("make \"p15m0.cpu \"same");
}

void test_the_harness_reads_the_temperature_either_side(void)
{
    set_mock_temperature(true, 31.5f);
    run("make \"p15m0.frames 2");
    run("p15m0");

    TEST_ASSERT_EQUAL_FLOAT(31.5f, num(":p15m0.temp0"));
    TEST_ASSERT_EQUAL_FLOAT(31.5f, num(":p15m0.temp1"));
}

// A board with no sensor, or one that cannot retune, still runs the harness
// and reports 0 for what it could not read. A timing script that dies on its
// last line has wasted the session.
void test_the_harness_survives_a_board_with_no_sensor_or_clock(void)
{
    set_mock_temperature(false, 0.0f);
    set_mock_cpu_khz(false, 150000u);
    run("make \"p15m0.frames 2  make \"p15m0.cpu \"fast");

    mock_device_clear_output();
    run("p15m0");

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(mock_device_get_output(), "the gate"),
                                 mock_device_get_output());
    TEST_ASSERT_EQUAL_STRING("unknown", word(":p15m0.cpu.ran"));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, num(":p15m0.temp0"));

    set_mock_temperature(true, 25.0f);
    set_mock_cpu_khz(true, 150000u);
    run("make \"p15m0.cpu \"same");
}

//==========================================================================
// The script
//==========================================================================

// It must run end to end before it is worth carrying to a board: a script that
// dies half way through wastes a hardware session, and its own numbers are
// only readable if the report reaches the file.
void test_p15m0_script_runs(void)
{
    run("make \"p15m0.frames 2");
    mock_device_clear_output();
    run("p15m0");

    const char *screen = mock_device_get_output();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "100 warm frames spend"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the calibration loops"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "what the present costs"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the two erase strategies"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the pieces"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the series"), screen);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(screen, "the gate"), screen);

    // And the same report reached the file, which is the copy that leaves the
    // board -- a screenful of numbers on the PicoCalc cannot be typed out.
    MockFile *report = mock_fs_get_file("p15m0.txt", false);
    TEST_ASSERT_NOT_NULL_MESSAGE(report, "p15m0.txt was not written");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->data, "the gate"), report->data);
}

// A `;` starts a comment wherever it appears -- inside a list literal too --
// so one in a `pr` line swallows the rest of the procedure body into that
// line's list, and every report line after it is lost.  It costs nothing to
// look like a working script: the sections before the semicolon still print,
// which is exactly why the substring checks above did not catch it.
//
// The signature is one absurdly long line where the remaining twenty should
// be, so that is what is asserted.
void test_the_report_is_lines_and_not_one_swallowed_body(void)
{
    run("make \"p15m0.frames 2");
    run("p15m0");

    MockFile *report = mock_fs_get_file("p15m0.txt", false);
    TEST_ASSERT_NOT_NULL(report);

    int lines = 0;
    const char *p = report->data;
    while (*p)
    {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len > 160)
        {
            char msg[240];
            snprintf(msg, sizeof(msg),
                     "report line %d is %zu characters, so a `pr` line swallowed "
                     "the rest of the body: %.120s", lines, len, p);
            TEST_FAIL_MESSAGE(msg);
        }
        lines++;
        if (!nl)
            break;
        p = nl + 1;
    }
    TEST_ASSERT_TRUE_MESSAGE(lines >= 45, "the report is shorter than its own sections");

    // And the last section actually arrived, which is what a swallowed line
    // takes away.
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(report->data, "word bytes free"), report->data);
}

// The report has to say the logic ran. Every position in the timing path is
// computed and thrown away so the scene holds still (see the harness header),
// and without these counters a reader cannot tell that from the logic having
// been optimised out of the measurement altogether.
void test_the_report_shows_the_logic_was_not_skipped(void)
{
    run("make \"p15m0.frames 2");
    run("p15m0");

    TEST_ASSERT_TRUE_MESSAGE(num(":p15.moved") > 0.0f,
        "no robot ever computed a direction, so the logic pass measured nothing");
    TEST_ASSERT_TRUE_MESSAGE(num(":p15.probed") > 0.0f,
        "no bolt/robot pair got past the cheap gate, so section 15.4's number "
        "was measured on the trivial path only");
}

// The gate is a 300 MHz number and the harness defaults to leaving the board
// where it found it, so the first board run was taken at `normal` -- and the
// report then compared every figure against a `fast` prediction without ever
// saying the comparison did not apply. A run at the wrong clock must say so.
void test_a_run_at_the_wrong_clock_says_it_is_not_the_gate(void)
{
    run("make \"p15m0.frames 2");

    run("make \"p15m0.cpu \"normal");
    mock_device_clear_output();
    run("p15m0");
    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr(mock_device_get_output(), "NOT AT 300 MHz"),
        "a run at normal did not say it was not the gate");

    run("make \"p15m0.cpu \"fast");
    mock_device_clear_output();
    run("p15m0");
    TEST_ASSERT_NULL_MESSAGE(
        strstr(mock_device_get_output(), "NOT AT 300 MHz"),
        "a run at fast disclaimed itself");
    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr(mock_device_get_output(), "50 ms gate"),
        "a run at fast did not reach a verdict");

    run("hw.setcpu \"normal  make \"p15m0.cpu \"same");
}

// Q3 is the control on §15.1's unit table and the unit is a FULL-CANVAS
// present -- what P13 M0 measured at 19.62/18.70. When the series was pointed
// at the chosen in-place frame this procedure was switched over with it, and
// it then timed a handful of dirty rows: the 2026-08-29 board run came back
// with an 8.45 ms splitscreen present, a *fullscreen* figure BELOW it, and a
// negative split saving. Nothing on the host noticed, because the mock does
// not model dirty regions.
//
// What the host CAN see is which scene was drawn, and that is enough: a
// full-canvas present has to be timed over a frame that redrew the walls, and
// the in-place frame never does.
void test_the_present_control_times_a_full_canvas(void)
{
    run("make \"room.x 0  make \"room.y 0  setup.room  place.robots 11  place.bolts 7");
    run("cache.models");

    mock_device_clear_graphics();
    run("ignore time.present");

    bool saw_a_wall = false;
    for (int k = 0; k < mock_device_line_count(); k++)
    {
        const MockLine *l = mock_device_get_line(k);
        if (fabsf(l->y1 - 142.0f) < 0.5f && fabsf(l->y2 - 142.0f) < 0.5f)
            saw_a_wall = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(saw_a_wall,
        "time.present timed a frame that did not redraw the walls, so it is "
        "measuring a partial present and not §15.1's unit");
}

// A timing script that leaves the screen in manual refresh hands the prompt
// back frozen: nothing the user types afterwards appears until something
// presents. It is also how a board session gets thrown away.
void test_the_script_puts_the_screen_back(void)
{
    run("make \"p15m0.frames 2");
    run("p15m0");
    TEST_ASSERT_EQUAL_STRING("auto", value_to_string(eval_string("refreshmode").value));
}

// Section 18's first ceiling, named rather than discovered late: MAX_PROCEDURES
// is a hard limit (128 when this was written, 192 since P18 M0), Battlezone
// defines exactly 128, and the failure mode is silent -- the LAST definition in
// the file goes missing. The game's budget is 100; the harness has to leave room
// to be loaded beside nothing at all.
void test_the_harness_is_inside_the_procedure_ceiling(void)
{
    // Counted out of the file rather than out of the workspace: there is no
    // operation that hands back the procedure table, and the file is what
    // `load` will be given on the board.
    FILE *f = fopen(P15M0_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL(f);
    char line[512];
    int defs = 0;
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "to ", 3) == 0)
            defs++;
    fclose(f);

    char msg[128];
    snprintf(msg, sizeof(msg),
             "the harness defines %d procedures, over the game's 100 budget", defs);
    TEST_ASSERT_TRUE_MESSAGE(defs <= 100, msg);
}

//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rnd_is_the_roms_own_generator);
    RUN_TEST(test_the_seed_is_the_rooms_own_coordinates);
    RUN_TEST(test_a_room_is_a_function_of_its_coordinates);
    RUN_TEST(test_the_drawn_segments_reproduce_with_the_masks);
    RUN_TEST(test_the_eight_segments_start_on_the_eight_intersections);
    RUN_TEST(test_each_of_the_four_choices_sets_the_bits_the_rom_sets);
    RUN_TEST(test_the_wall_template_walls_the_border_including_the_doorways);
    RUN_TEST(test_a_point_maps_to_the_cell_the_rom_would_pick);
    RUN_TEST(test_seek_is_the_four_durl_bits);
    RUN_TEST(test_iq_clears_a_direction_whose_edge_is_walled);
    RUN_TEST(test_iq_in_open_ground_keeps_every_bit);
    RUN_TEST(test_a_robot_in_the_players_cell_is_not_probed);
    RUN_TEST(test_the_three_firing_windows_at_their_boundaries);
    RUN_TEST(test_the_robot_sprites_are_the_roms_own_bytes);
    RUN_TEST(test_the_renderer_draws_the_sprite_at_the_cabinets_size);
    RUN_TEST(test_the_costumes_are_captured_at_the_cabinets_width);
    RUN_TEST(test_the_model_dispatch_is_five_lists_reached_by_facing_group);
    RUN_TEST(test_the_direction_tables_agree_with_each_other);
    RUN_TEST(test_the_spawn_table_is_the_eleven_cells_that_are_not_doorways);
    RUN_TEST(test_the_border_is_a_closed_circuit_with_four_doorways);
    RUN_TEST(test_the_cache_captures_one_costume_per_model);
    RUN_TEST(test_a_stamped_robot_is_one_stamp_and_no_strokes);
    RUN_TEST(test_a_stamped_robot_lands_half_a_sprite_from_its_stored_corner);
    RUN_TEST(test_a_stamped_robot_wears_the_costume_for_its_facing);
    RUN_TEST(test_the_stamped_frame_stamps_the_figures_and_still_draws_the_bolts);
    RUN_TEST(test_the_stamped_in_place_erase_covers_every_figure);
    RUN_TEST(test_the_timed_and_untimed_frames_draw_the_same_thing);
    RUN_TEST(test_two_consecutive_frames_draw_identical_geometry);
    RUN_TEST(test_erase_in_place_covers_the_figures_and_leaves_the_walls_alone);
    RUN_TEST(test_the_frame_allocates_nothing);
    RUN_TEST(test_the_harness_measures_both_resources_and_reports_zero);
    RUN_TEST(test_the_frames_word_table_cost_is_warm_up_and_not_a_rate);
    RUN_TEST(test_the_frame_grows_by_one_robot_at_a_time);
    RUN_TEST(test_the_frame_does_not_write_the_measure_accumulators);
    RUN_TEST(test_the_harness_reports_the_clock_it_actually_ran_at);
    RUN_TEST(test_the_same_clock_leaves_the_board_alone);
    RUN_TEST(test_the_harness_reads_the_temperature_either_side);
    RUN_TEST(test_the_harness_survives_a_board_with_no_sensor_or_clock);
    RUN_TEST(test_p15m0_script_runs);
    RUN_TEST(test_the_report_is_lines_and_not_one_swallowed_body);
    RUN_TEST(test_the_report_shows_the_logic_was_not_skipped);
    RUN_TEST(test_the_present_control_times_a_full_canvas);
    RUN_TEST(test_a_run_at_the_wrong_clock_says_it_is_not_the_gate);
    RUN_TEST(test_the_script_puts_the_screen_back);
    RUN_TEST(test_the_harness_is_inside_the_procedure_ceiling);
    return UNITY_END();
}
