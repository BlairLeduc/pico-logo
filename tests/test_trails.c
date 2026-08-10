// Tests for the pure-Logo Turtle Trails game.
//
// The encoded map is the game's only source of truth: movement, bug
// decisions, painting and the board on screen all read it. These tests
// therefore check the encoded map against itself (symmetry, connectivity, a
// sealed nest, no dead ends), check that the C tile map the game builds from
// it agrees cell for cell, and check the baked board against that map, so the
// picture and the logic cannot drift apart.
//
// They also execute every rule procedure at least once. The parse hazards
// listed at the top of logo/games/trails are runtime errors that reading does
// not catch, so a suite that only inspected data would pass over an
// implementation that could not run a single frame.
#include "test_scaffold.h"
#include "core/repl.h"
#include "core/error.h"
#include "core/procedures.h"
#include "core/variables.h"
#include "core/limits.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TRAILS_SOURCE
#error "TRAILS_SOURCE must be defined"
#endif

#define COLS 28
#define ROWS 36

// Tile codes, as documented in docs/turtle-trails-design.md section 5.2.
#define T_DEAD    0
#define T_EMPTY   1
#define T_PAINT   2
#define T_BLOSSOM 3
#define T_TUNNEL  4
#define T_NEST    5
#define T_DOOR    6

// Bank slots, as laid out at the top of logo/games/trails. A map cell holds
// one of these: the picture and the rule at once.
#define S_HEDGE   1
#define S_NEST   17
#define S_PATH   18
#define S_SPECK  19
#define S_BLOSS  20

// The board's offset inside the whole-screen map, and the screen rectangle it
// therefore occupies: 28x36 cells of 8 pixels centred on 320x320.
#define MAP_DC 6
#define MAP_DR 2
#define MAP_KEEP 40   // row offset of the derived board, below the screen
#define BOARD_X0 (MAP_DC * 8)
#define BOARD_Y0 (MAP_DR * 8)

// Directions, which are also the tie-break order.
#define D_UP 1
#define D_LEFT 2
#define D_DOWN 3
#define D_RIGHT 4

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

// Load the game exactly the way the `load` primitive does: join lines only
// between "to" and "end", and execute every other line on its own. This
// mirrors prim_load in core/primitives_files_load_save.c, including its
// 256-byte line limit, so a file this harness accepts `load` must accept too.
#define TEST_LOAD_MAX_LINE 256

static void load_logo(const char *path)
{
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
    char line[1024], buf[8192];
    size_t used = 0;
    bool in_def = false;

    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;

        // A line `load` could not read whole would be silently truncated.
        TEST_ASSERT_LESS_THAN_MESSAGE(TEST_LOAD_MAX_LINE, n, line);
        if (n == 0) continue;

        // A `;` inside a bracketed list starts a comment and swallows the
        // rest of the line -- and, in a procedure, everything after it up to
        // `end`. It raises no error: the procedure simply loses its tail, so
        // only running the exact path shows it. Reject it at load instead.
        int depth = 0;
        for (size_t i = 0; i < n; i++) {
            if (line[i] == '[') depth++;
            else if (line[i] == ']') depth--;
            else if (line[i] == ';' && depth > 0)
                TEST_FAIL_MESSAGE(line);
        }

        if (!in_def && repl_line_starts_with_to(line)) {
            in_def = true;
            used = 0;
        }

        if (in_def) {
            if (repl_line_is_end(line)) {
                memcpy(buf + used, "end", 4);
                in_def = false;
                Result r = proc_define_from_text(buf);
                TEST_ASSERT_NOT_EQUAL_MESSAGE(RESULT_ERROR, r.status, buf);
                used = 0;
            } else {
                TEST_ASSERT_LESS_THAN(sizeof(buf) - n - 2, used);
                memcpy(buf + used, line, n);
                used += n;
                buf[used++] = '\n';
            }
            continue;
        }

        Result r = run_string(line);
        TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, line);
    }
    TEST_ASSERT_FALSE_MESSAGE(in_def, "file ends inside a procedure definition");
    fclose(f);
}

static void load_trails(void) { load_logo(TRAILS_SOURCE); }

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

// Read a numeric result. Values fetched out of a list are not necessarily
// stored as a raw float, so go through the printed form.
static float num(const char *code)
{
    Result r = eval_string(code);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, error_format(r));
    const char *s = value_to_string(r.value);
    char *end = NULL;
    float v = strtof(s, &end);
    TEST_ASSERT_TRUE_MESSAGE(end && end != s, code);
    return v;
}

static void run(const char *code)
{
    Result r = run_string(code);
    TEST_ASSERT_TRUE_MESSAGE(r.status == RESULT_NONE || r.status == RESULT_OK, error_format(r));
}

static void truth(const char *code, const char *want)
{
    Result r = eval_string(code);
    TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, error_format(r));
    TEST_ASSERT_EQUAL_STRING_MESSAGE(want, value_to_string(r.value), code);
}

static void runf(const char *fmt, ...)
{
    char code[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(code, sizeof(code), fmt, ap);
    va_end(ap);
    run(code);
}

static float numf(const char *fmt, ...)
{
    char code[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(code, sizeof(code), fmt, ap);
    va_end(ap);
    return num(code);
}

// Place one actor. Index 1 is the turtle and 2..5 the bugs, matching the
// game's turtle-number-plus-one convention.
static void put_actor(int i, int col, int row, int dir, float off, int state)
{
    runf(".setitem %d :a.col %d", i, col);
    runf(".setitem %d :a.row %d", i, row);
    runf(".setitem %d :a.dir %d", i, dir);
    runf(".setitem %d :a.off %g", i, off);
    runf(".setitem %d :a.state %d", i, state);
    runf(".setitem %d :a.next 0", i);
    runf(".setitem %d :a.rev 0", i);
    // The frame snapshots the tile before movement, so an actor just placed
    // has not moved: the swap half of the collision test must see no exchange
    // until this actor is stepped.
    runf(".setitem %d :a.was %d", i, col);
    runf(".setitem %d :a.was %d", i + 5, row);
}

static int actor(const char *field, int i)
{
    return (int)numf("item %d :a.%s", i, field);
}

// Read the encoded map into C so graph properties can be checked with a real
// traversal rather than by trusting the game's own helpers. The encoded rows
// are the source data; what the runtime map holds is a bank slot, and
// test_the_built_map_agrees_with_the_encoding ties the two together.
static void read_map(int m[ROWS][COLS])
{
    for (int r = 0; r < ROWS; r++) {
        char code[64];
        snprintf(code, sizeof(code), "item %d :tt.map", r + 1);
        Result res = eval_string(code);
        TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, res.status, error_format(res));
        const char *w = value_to_string(res.value);
        TEST_ASSERT_EQUAL_INT_MESSAGE(COLS, (int)strlen(w), "an encoded row is not 28 characters");
        for (int c = 0; c < COLS; c++) {
            TEST_ASSERT_TRUE_MESSAGE(w[c] >= 'A' && w[c] <= 'G', "unknown tile letter");
            m[r][c] = w[c] - 'A';
        }
    }
}

// Read the C tile map the game actually plays from, in board coordinates.
static void read_slots(int s[ROWS][COLS])
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            s[r][c] = (int)numf("tile.at %d %d", c + 1, r + 1);
}

static bool walkable(int code)
{
    return code >= T_EMPTY && code <= T_TUNNEL;
}

static const int DC[5] = {0, 0, -1, 0, 1};
static const int DR[5] = {0, -1, 0, 1, 0};

static bool tunnel_row(int row1)  // 1-based
{
    return row1 == 18 || row1 == 27;
}

// Step one tile in C, honouring the tunnel wrap, in 1-based map coordinates.
// Returns false when the step leaves the board.
static bool step_tile(int c, int r, int d, int *nc, int *nr)
{
    int x = c + DC[d], y = r + DR[d];
    if (x < 1) { if (!tunnel_row(r)) return false; x = COLS; }
    if (x > COLS) { if (!tunnel_row(r)) return false; x = 1; }
    if (y < 1 || y > ROWS) return false;
    *nc = x;
    *nr = y;
    return true;
}

void setUp(void)
{
    test_scaffold_setUp_with_device_and_hardware();
    load_trails();
    run("setup.palette setup.shapes setup.turtles setup.tiles setup.sound");
    run("init.game setup.level");
}

void tearDown(void) { test_scaffold_tearDown(); }

// ---------------------------------------------------------------------------
// 1. Map invariants
// ---------------------------------------------------------------------------

static void test_map_shape_and_encoding(void)
{
    TEST_ASSERT_EQUAL_INT(ROWS, (int)num("count :tt.map"));
    for (int r = 1; r <= ROWS; r++) {
        // A row of only digits would have been read as a number and lost its
        // leading zeros, which is why the encoding uses letters.
        TEST_ASSERT_EQUAL_INT_MESSAGE(COLS, (int)numf("count item %d :tt.map", r),
                                      "encoded row is not 28 characters");
    }
    // The map is the whole 320x320 screen, because a bake always starts at
    // the top left corner of the graphics area; the board sits inside it.
    TEST_ASSERT_EQUAL_INT(MAP_DC, (int)num(":sl.dc"));
    TEST_ASSERT_EQUAL_INT(MAP_DR, (int)num(":sl.dr"));
    TEST_ASSERT_EQUAL_INT(MAP_KEEP, (int)num(":sl.keep"));
    // tile.at spells the offsets out for speed, so pin the two spellings
    // against each other at both corners of the board.
    TEST_ASSERT_EQUAL_INT((int)numf("tile %d %d", 1 + MAP_DC, 1 + MAP_DR),
                          (int)num("tile.at 1 1"));
    TEST_ASSERT_EQUAL_INT((int)numf("tile %d %d", COLS + MAP_DC, ROWS + MAP_DR),
                          (int)num("tile.at 28 36"));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)numf("tile 1 1"), "the map margin is not empty");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)numf("tile 40 40"), "the map margin is not empty");
    // A step off the board has to land on that margin, which is what lets
    // tile.at drop its bounds tests.
    TEST_ASSERT_EQUAL_INT(0, (int)num("tile.at 0 1"));
    TEST_ASSERT_EQUAL_INT(0, (int)num("tile.at 29 36"));
    TEST_ASSERT_EQUAL_INT(0, (int)num("tile.at 1 0"));
    TEST_ASSERT_EQUAL_INT(0, (int)num("tile.at 28 37"));
}

// The C map is built from the encoded rows, and everything downstream -- the
// rules and the picture alike -- reads only the map. So each cell's slot must
// carry its letter's meaning, and carry the variant its neighbours ask for.
static void test_the_built_map_agrees_with_the_encoding(void)
{
    static int m[ROWS][COLS];
    static int s[ROWS][COLS];
    read_map(m);
    read_slots(s);

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            char msg[64];
            snprintf(msg, sizeof(msg), "col %d row %d", c + 1, r + 1);
            int slot = s[r][c];

            if (!walkable(m[r][c])) {
                // The nest floor and its door are solid hedge to look at but
                // a number bugs may cross. Dead space is the hedge the maze
                // is drawn from, and carries the variant its neighbours ask
                // for: the mask counts the sides it does *not* have to stand
                // back from, which are the hedge ones, the nest (hedge to
                // look at) and the margin off the board.
                if (m[r][c] != T_DEAD) {
                    TEST_ASSERT_EQUAL_INT_MESSAGE(S_NEST, slot, msg);
                    continue;
                }

                int mask = 0;
                for (int d = 1; d <= 4; d++) {
                    int nc = c + 1 + DC[d], nr = r + 1 + DR[d];
                    bool hedge = nc < 1 || nc > COLS || nr < 1 || nr > ROWS ||
                                 !walkable(m[nr - 1][nc - 1]);
                    if (!hedge) continue;
                    mask |= (d == D_UP) ? 8 : (d == D_LEFT) ? 4 : (d == D_DOWN) ? 2 : 1;
                }
                TEST_ASSERT_EQUAL_INT_MESSAGE(S_HEDGE + mask, slot, msg);
                TEST_ASSERT_TRUE_MESSAGE(slot < S_NEST, msg);
                continue;
            }

            // A path cell is the background and what sits on it: no variant,
            // because no hedge pixel lives in a path tile any more.
            int want = m[r][c] == T_PAINT ? S_SPECK : m[r][c] == T_BLOSSOM ? S_BLOSS : S_PATH;
            TEST_ASSERT_EQUAL_INT_MESSAGE(want, slot, msg);
            TEST_ASSERT_TRUE_MESSAGE(slot > S_NEST, msg);
        }
    }
}

// The board is derived once into map rows below the screen and copied down at
// every level start, so the copy has to be exact -- and the kept rows must
// survive a level being played, or the next one starts half painted.
static void test_the_kept_board_is_restored_at_every_level(void)
{
    static int s[ROWS][COLS];
    read_slots(s);

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            TEST_ASSERT_EQUAL_INT_MESSAGE(
                s[r][c], (int)numf("tile %d %d", c + 1 + MAP_DC, r + 1 + MAP_KEEP),
                "the kept board disagrees with the live one");

    // Paint a tile, then start the next level: the kept copy is untouched and
    // the live board comes back whole.
    int left = (int)num(":tt.left");
    put_actor(1, 2, 5, D_DOWN, 0, 0);
    run("paint.tile");
    TEST_ASSERT_EQUAL_INT(left - 1, (int)num(":tt.left"));
    TEST_ASSERT_NOT_EQUAL(s[4][1], (int)num("tile.at 2 5"));

    run("make \"tt.level (:tt.level + 1) setup.level");
    TEST_ASSERT_EQUAL_INT_MESSAGE(left, (int)num(":tt.left"), "tt.left was not restored");
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            TEST_ASSERT_EQUAL_INT_MESSAGE((int)numf("tile.at %d %d", c + 1, r + 1), s[r][c],
                                          "a level start did not restore the board");
}

static void test_decoded_counts_match_the_encoded_words(void)
{
    static int m[ROWS][COLS];
    read_map(m);

    int paint = 0, blossom = 0, nest = 0, door = 0, tunnel = 0;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            switch (m[r][c]) {
            case T_PAINT: paint++; break;
            case T_BLOSSOM: blossom++; break;
            case T_NEST: nest++; break;
            case T_DOOR: door++; break;
            case T_TUNNEL: tunnel++; break;
            default: break;
            }
        }
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, blossom, "there must be four power blossoms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, door, "the nest has one two-tile door");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, nest, "the nest has a floor");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, tunnel, "the tunnels have floor tiles");

    // The paintable total is what the level counts down, and the design wants
    // it near the classic pacing.
    int paintable = paint + blossom;
    TEST_ASSERT_EQUAL_INT_MESSAGE(paintable, (int)num(":tt.left"),
                                  "tiles.left disagrees with the decoded map");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(230, paintable, "too few paintable tiles");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(250, paintable, "too many paintable tiles");
}

static void test_map_is_left_right_symmetric(void)
{
    static int m[ROWS][COLS];
    read_map(m);
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            TEST_ASSERT_EQUAL_INT_MESSAGE(m[r][c], m[r][COLS - 1 - c], "map is not symmetric");
}

// One connected path network covering every paintable tile: if any code-2 or
// code-3 tile were cut off, the level could never be cleared.
static void test_paths_form_one_connected_network(void)
{
    static int m[ROWS][COLS];
    read_map(m);

    static bool seen[ROWS][COLS];
    memset(seen, 0, sizeof(seen));
    static int stack[ROWS * COLS][2];
    int top = 0, walk = 0;

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (walkable(m[r][c])) walk++;

    int sc = (int)num(":tt.start.col"), sr = (int)num(":tt.start.row");
    stack[top][0] = sc; stack[top][1] = sr; top++;
    seen[sr - 1][sc - 1] = true;
    int reached = 1;

    while (top) {
        top--;
        int c = stack[top][0], r = stack[top][1];
        for (int d = 1; d <= 4; d++) {
            int nc, nr;
            if (!step_tile(c, r, d, &nc, &nr)) continue;
            if (!walkable(m[nr - 1][nc - 1])) continue;
            if (seen[nr - 1][nc - 1]) continue;
            seen[nr - 1][nc - 1] = true;
            reached++;
            stack[top][0] = nc; stack[top][1] = nr; top++;
        }
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(walk, reached, "the path network is not one component");
}

// Every corridor tile needs at least two exits. A one-exit tile is a pocket
// the turtle can be cornered in with no escape, and the board renderer also
// relies on it: a walkable cell with no walkable neighbour would be drawn
// with the bare mask-0 tile, which is solid hedge.
static void test_no_dead_ends(void)
{
    static int m[ROWS][COLS];
    read_map(m);
    for (int r = 1; r <= ROWS; r++) {
        for (int c = 1; c <= COLS; c++) {
            if (!walkable(m[r - 1][c - 1])) continue;
            int exits = 0, hrun = 0, vrun = 0;
            for (int d = 1; d <= 4; d++) {
                int nc, nr;
                if (step_tile(c, r, d, &nc, &nr) && walkable(m[nr - 1][nc - 1])) {
                    exits++;
                    if (d == D_LEFT || d == D_RIGHT) hrun++;
                    else vrun++;
                }
            }
            char msg[64];
            snprintf(msg, sizeof(msg), "col %d row %d has %d exits", c, r, exits);
            TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(2, exits, msg);
            TEST_ASSERT_TRUE_MESSAGE(hrun > 0 || vrun > 0, msg);
        }
    }
}

// The nest must be sealed except for its door, or bugs would not be confined
// and the turtle could hide inside it.
static void test_nest_is_reachable_only_through_the_door(void)
{
    static int m[ROWS][COLS];
    read_map(m);
    int door_col = (int)num(":tt.door.col"), door_row = (int)num(":tt.door.row");

    for (int r = 1; r <= ROWS; r++) {
        for (int c = 1; c <= COLS; c++) {
            if (m[r - 1][c - 1] != T_NEST) continue;
            for (int d = 1; d <= 4; d++) {
                int nc, nr;
                if (!step_tile(c, r, d, &nc, &nr)) continue;
                int v = m[nr - 1][nc - 1];
                TEST_ASSERT_TRUE_MESSAGE(v == T_NEST || v == T_DOOR || v == T_DEAD,
                                         "nest floor touches open path");
            }
        }
    }
    // The door sits above nest floor and below open path, so a bug leaves
    // upward and drops back in downward.
    TEST_ASSERT_EQUAL_INT(T_DOOR, m[door_row - 1][door_col - 1]);
    TEST_ASSERT_EQUAL_INT(T_DOOR, m[door_row - 1][door_col]);
    TEST_ASSERT_EQUAL_INT(T_NEST, m[door_row][door_col - 1]);
    TEST_ASSERT_TRUE(walkable(m[door_row - 2][door_col - 1]));

    // An ordinary path move may not cross the door or the floor.
    truth("open? :tt.door.col (:tt.door.row - 1) 3", "false");
    truth("nest.open? :tt.door.col (:tt.door.row - 1) 3", "true");
}

static void test_two_tunnels_on_different_rows_wrap(void)
{
    static int m[ROWS][COLS];
    read_map(m);

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, (int)num("count :tt.tunnels"), "there must be two tunnels");
    int a = (int)num("item 1 :tt.tunnels"), b = (int)num("item 2 :tt.tunnels");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(a, b, "the tunnels must sit on different rows");

    for (int r = 1; r <= ROWS; r++) {
        bool edges = m[r - 1][0] == T_TUNNEL && m[r - 1][COLS - 1] == T_TUNNEL;
        TEST_ASSERT_EQUAL_INT_MESSAGE(tunnel_row(r), edges, "tunnel edges disagree with the table");
        char code[64];
        snprintf(code, sizeof(code), "tunnel.row? %d", r);
        truth(code, tunnel_row(r) ? "true" : "false");
    }
    // Column 1 and column 28 are neighbours on a tunnel row, and nowhere else.
    TEST_ASSERT_EQUAL_INT(COLS, (int)numf("next.col 1 %d 2", a));
    TEST_ASSERT_EQUAL_INT(1, (int)numf("next.col %d %d 4", COLS, a));
    TEST_ASSERT_EQUAL_INT(COLS, (int)numf("next.col 1 %d 2", b));
    TEST_ASSERT_EQUAL_INT(0, num("next.col 1 6 2"));
    TEST_ASSERT_EQUAL_INT(0, numf("next.col %d 6 4", COLS));
}

static void test_named_tiles_sit_on_the_right_codes(void)
{
    static int m[ROWS][COLS];
    read_map(m);

    int sc = (int)num(":tt.start.col"), sr = (int)num(":tt.start.row");
    TEST_ASSERT_EQUAL_INT_MESSAGE(T_EMPTY, m[sr - 1][sc - 1], "the start tile carries no paint");

    int gc = (int)num(":tt.gate.col"), gr = (int)num(":tt.gate.row");
    TEST_ASSERT_TRUE_MESSAGE(walkable(m[gr - 1][gc - 1]), "the garden gate is not on a path");
    TEST_ASSERT_TRUE_MESSAGE(gr > (int)num(":tt.door.row"), "the gate must sit below the nest");

    // Dart waits above the door; the other three begin on nest floor.
    TEST_ASSERT_TRUE(walkable(m[(int)num("item 2 :tt.home.row") - 1][(int)num("item 2 :tt.home.col") - 1]));
    for (int i = 3; i <= 5; i++) {
        int hc = (int)numf("item %d :tt.home.col", i);
        int hr = (int)numf("item %d :tt.home.row", i);
        TEST_ASSERT_EQUAL_INT_MESSAGE(T_NEST, m[hr - 1][hc - 1], "a bug starts off the nest floor");
    }
    // Patrol targets sit in dead space beyond the corners, so a patrolling bug
    // orbits its corner instead of arriving and stopping.
    for (int i = 2; i <= 5; i++) {
        int cc = (int)numf("item %d :tt.corner.col", i);
        int cr = (int)numf("item %d :tt.corner.row", i);
        TEST_ASSERT_FALSE_MESSAGE(walkable(m[cr - 1][cc - 1]), "a patrol target is on a path");
    }
}

// The calm tiles stop a hunting bug camping the corridor above the nest, but
// they must not cut the maze in half for bugs: every path tile has to stay
// reachable under the restriction.
static void test_calm_tiles_do_not_strand_bugs(void)
{
    static int m[ROWS][COLS];
    read_map(m);
    TEST_ASSERT_EQUAL_INT(2, (int)num("count :tt.calm.rows"));
    TEST_ASSERT_EQUAL_INT(2, (int)num("count :tt.calm.cols"));

    int calm_r[2] = {(int)num("item 1 :tt.calm.rows"), (int)num("item 2 :tt.calm.rows")};
    int calm_c[2] = {(int)num("item 1 :tt.calm.cols"), (int)num("item 2 :tt.calm.cols")};

    static bool seen[ROWS][COLS];
    memset(seen, 0, sizeof(seen));
    static int stack[ROWS * COLS][2];
    int top = 0, walk = 0, reached = 1;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (walkable(m[r][c])) walk++;

    int sc = (int)num("item 2 :tt.home.col"), sr = (int)num("item 2 :tt.home.row");
    stack[top][0] = sc; stack[top][1] = sr; top++;
    seen[sr - 1][sc - 1] = true;

    while (top) {
        top--;
        int c = stack[top][0], r = stack[top][1];
        for (int d = 1; d <= 4; d++) {
            bool calm = false;
            for (int k = 0; k < 2; k++)
                for (int j = 0; j < 2; j++)
                    if (r == calm_r[k] && c == calm_c[j]) calm = true;
            if (d == D_UP && calm) continue;
            int nc, nr;
            if (!step_tile(c, r, d, &nc, &nr)) continue;
            if (!walkable(m[nr - 1][nc - 1])) continue;
            if (seen[nr - 1][nc - 1]) continue;
            seen[nr - 1][nc - 1] = true;
            reached++;
            stack[top][0] = nc; stack[top][1] = nr; top++;
        }
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(walk, reached, "calm tiles strand part of the maze from bugs");

    // And the restriction really bites where it is meant to.
    truth("exit.open? 2 13 15 1", "false");
    truth("exit.open? 2 7 15 1", "true");
}

// ---------------------------------------------------------------------------
// 2. Coordinates
// ---------------------------------------------------------------------------

static void test_tile_centres_and_round_trips(void)
{
    // The four corner tile centres put a 224x288 board in the middle of the
    // 320x320 screen: 48px of margin left and right, 16px top and bottom.
    TEST_ASSERT_EQUAL_FLOAT(-108.0f, num("tile.x 1"));
    TEST_ASSERT_EQUAL_FLOAT(108.0f, num("tile.x 28"));
    TEST_ASSERT_EQUAL_FLOAT(140.0f, num("tile.y 1"));
    TEST_ASSERT_EQUAL_FLOAT(-140.0f, num("tile.y 36"));

    for (int c = 1; c <= COLS; c++)
        TEST_ASSERT_EQUAL_FLOAT(-108.0f + 8.0f * (c - 1), numf("tile.x %d", c));
    for (int r = 1; r <= ROWS; r++)
        TEST_ASSERT_EQUAL_FLOAT(140.0f - 8.0f * (r - 1), numf("tile.y %d", r));
}

static void test_direction_deltas_and_opposites(void)
{
    int dc[5] = {0, 0, -1, 0, 1}, dr[5] = {0, -1, 0, 1, 0};
    for (int d = 1; d <= 4; d++) {
        TEST_ASSERT_EQUAL_INT(dc[d], (int)numf("dir.dc %d", d));
        TEST_ASSERT_EQUAL_INT(dr[d], (int)numf("dir.dr %d", d));
        TEST_ASSERT_EQUAL_INT(d, (int)numf("opposite opposite %d", d));
    }
    TEST_ASSERT_EQUAL_INT(D_DOWN, (int)numf("opposite %d", D_UP));
    TEST_ASSERT_EQUAL_INT(D_RIGHT, (int)numf("opposite %d", D_LEFT));
}

// An actor's screen position is its tile centre plus the pixel offset along
// its heading, which is what makes movement identical on every board.
static void test_placement_follows_tile_and_offset(void)
{
    put_actor(1, 10, 24, D_RIGHT, 48, 0);   // three pixels, in sixteenths
    run("place.actor 1");
    const MockTurtleState *t = mock_device_get_turtle(0);
    TEST_ASSERT_EQUAL_FLOAT(num("tile.x 10") + 3.0f, t->x);
    TEST_ASSERT_EQUAL_FLOAT(num("tile.y 24"), t->y);

    put_actor(1, 10, 24, D_UP, 48, 0);
    run("place.actor 1");
    t = mock_device_get_turtle(0);
    TEST_ASSERT_EQUAL_FLOAT(num("tile.x 10"), t->x);
    TEST_ASSERT_EQUAL_FLOAT(num("tile.y 24") + 3.0f, t->y);
}

// Crossing a portal lifts the pen: with it down, the turtle would draw its
// trail straight back across the screen.
static void test_tunnel_translation_lifts_the_pen(void)
{
    int row = (int)num("item 1 :tt.tunnels");
    put_actor(1, 1, row, D_LEFT, 0, 0);
    run("make \"tt.warp \"false");
    run("ask 0 [pd]");
    run("move.actor 1 128");

    TEST_ASSERT_EQUAL_INT_MESSAGE(COLS, actor("col", 1), "the turtle did not wrap");
    truth(":tt.warp", "true");

    int before = mock_device_get_state()->graphics.line_count;
    run("place.all");
    int after = mock_device_get_state()->graphics.line_count;
    TEST_ASSERT_EQUAL_INT_MESSAGE(before, after, "a trail was drawn across the teleport");
    truth(":tt.warp", "false");

    // The pen goes back down, so the trail resumes on the far side.
    TEST_ASSERT_EQUAL_INT(LOGO_PEN_DOWN, mock_device_get_turtle(0)->pen_state);
}

// ---------------------------------------------------------------------------
// 3. Player movement
// ---------------------------------------------------------------------------

// A turn the corridor will not take yet stays buffered until it opens.
static void test_blocked_turn_stays_buffered(void)
{
    // Column 2 row 6 is a vertical corridor with hedge to its right.
    put_actor(1, 2, 6, D_DOWN, 0, 0);
    truth("open? 2 6 4", "false");
    run(".setitem 1 :a.next 4");
    run("try.turn");
    TEST_ASSERT_EQUAL_INT_MESSAGE(D_DOWN, actor("dir", 1), "the turtle turned into a hedge");
    TEST_ASSERT_EQUAL_INT_MESSAGE(D_RIGHT, actor("next", 1), "the request was discarded");

    // Carry it to a tile where the corridor does open, and it takes effect.
    put_actor(1, 2, 9, D_DOWN, 0, 0);
    run(".setitem 1 :a.next 4");
    truth("open? 2 9 4", "true");
    run("try.turn");
    TEST_ASSERT_EQUAL_INT(D_RIGHT, actor("dir", 1));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, actor("next", 1), "the request was not consumed");
}

// Cornering: a turn may start up to four pixels before a centre, so the
// corridor tested is the nearest centre rather than the one last passed.
static void test_early_cornering_uses_the_nearest_centre(void)
{
    // Heading down column 2, the corridor at row 9 opens to the right.
    put_actor(1, 2, 8, D_DOWN, 80, 0);
    run(".setitem 1 :a.next 4");
    run("try.turn");
    TEST_ASSERT_EQUAL_INT_MESSAGE(9, actor("row", 1), "the turn did not start early");
    TEST_ASSERT_EQUAL_INT(D_RIGHT, actor("dir", 1));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, actor("off", 1), "the perpendicular axis did not snap");

    // Three pixels past the centre, the same request still corners, this time
    // around the centre just passed.
    put_actor(1, 2, 9, D_DOWN, 48, 0);
    run(".setitem 1 :a.next 4");
    run("try.turn");
    TEST_ASSERT_EQUAL_INT_MESSAGE(9, actor("row", 1), "the late turn moved to the wrong tile");
    TEST_ASSERT_EQUAL_INT(D_RIGHT, actor("dir", 1));
}

// A reverse is accepted immediately: the actor keeps its place and adopts the
// tile it was heading for.
static void test_reverse_is_immediate(void)
{
    put_actor(1, 10, 24, D_RIGHT, 48, 0);
    run(".setitem 1 :a.next 2");
    run("try.turn");
    TEST_ASSERT_EQUAL_INT(D_LEFT, actor("dir", 1));
    TEST_ASSERT_EQUAL_INT_MESSAGE(11, actor("col", 1), "reverse did not adopt the tile ahead");
    TEST_ASSERT_EQUAL_INT_MESSAGE(80, actor("off", 1), "reverse moved the turtle");

    // At a centre there is no tile ahead to adopt: only the heading flips.
    put_actor(1, 10, 24, D_RIGHT, 0, 0);
    run(".setitem 1 :a.next 2");
    run("try.turn");
    TEST_ASSERT_EQUAL_INT(D_LEFT, actor("dir", 1));
    TEST_ASSERT_EQUAL_INT(10, actor("col", 1));
    TEST_ASSERT_EQUAL_INT(0, actor("off", 1));
}

static void test_wall_stops_the_turtle_at_the_centre(void)
{
    // Row 5 column 2 faces hedge to the left.
    put_actor(1, 2, 5, D_LEFT, 0, 0);
    truth("open? 2 5 2", "false");
    run("move.actor 1 48");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, actor("col", 1), "the turtle walked into a hedge");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, actor("off", 1), "a blocked turtle must wait at the centre");
}

// Fractional progress is kept, so speed does not depend on frame boundaries.
static void test_sub_pixel_progress_carries_across_tiles(void)
{
    // Row 5 is the open corridor along the top of the maze.
    put_actor(1, 10, 5, D_RIGHT, 0, 0);
    run("move.actor 1 80");
    TEST_ASSERT_EQUAL_INT(10, actor("col", 1));
    TEST_ASSERT_EQUAL_INT(80, actor("off", 1));

    run("move.actor 1 80");
    TEST_ASSERT_EQUAL_INT_MESSAGE(11, actor("col", 1), "the turtle did not enter the next tile");
    TEST_ASSERT_EQUAL_INT_MESSAGE(32, actor("off", 1), "the remainder was lost");
}

// A blocked actor stops dead at the centre rather than drifting into hedge.
static void test_blocked_actor_discards_the_frame_step(void)
{
    put_actor(1, 10, 5, D_UP, 0, 0);   // row 4 above is all hedge
    truth("open? 10 5 1", "false");
    run("move.actor 1 48");
    TEST_ASSERT_EQUAL_INT(0, actor("off", 1));
    TEST_ASSERT_EQUAL_INT(5, actor("row", 1));
}

// ---------------------------------------------------------------------------
// 4. Painting and bonuses
// ---------------------------------------------------------------------------

static void test_painting_mutates_the_tile_and_scores(void)
{
    run("make \"tt.score 0");
    int left = (int)num(":tt.left");

    // Column 2 row 5 is an ordinary unpainted path tile. Painting it leaves
    // the bare corridor the speck was sitting on.
    int speck = (int)num("tile.at 2 5");
    TEST_ASSERT_EQUAL_INT_MESSAGE(S_SPECK, speck, "col 2 row 5 carries no speck");
    put_actor(1, 2, 5, D_DOWN, 0, 0);
    run("paint.tile");

    TEST_ASSERT_EQUAL_INT_MESSAGE(S_PATH, (int)num("tile.at 2 5"), "the tile was not painted");
    TEST_ASSERT_EQUAL_INT(10, (int)num(":tt.score"));
    TEST_ASSERT_EQUAL_INT(left - 1, (int)num(":tt.left"));
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, (int)num(":tt.pause"), "a fresh tile withholds one quantum");

    // Painting the same tile again is a no-op: the mutation is in place.
    run("paint.tile");
    TEST_ASSERT_EQUAL_INT(10, (int)num(":tt.score"));
    TEST_ASSERT_EQUAL_INT(left - 1, (int)num(":tt.left"));
}

static void test_blossom_scores_pauses_and_turns_the_tables(void)
{
    run("make \"tt.score 0");
    int bloss = (int)num("tile.at 2 7");
    TEST_ASSERT_EQUAL_INT_MESSAGE(S_BLOSS, bloss, "col 2 row 7 carries no blossom");

    // Two bugs hunting, one still waiting in the nest.
    put_actor(2, 10, 24, D_LEFT, 0, 1);
    put_actor(3, 12, 24, D_LEFT, 0, 1);
    put_actor(4, 12, 18, D_UP, 0, 2);
    put_actor(5, 17, 18, D_UP, 0, 2);

    put_actor(1, 2, 7, D_DOWN, 0, 0);
    run("paint.tile");

    TEST_ASSERT_EQUAL_INT(S_PATH, (int)num("tile.at 2 7"));
    TEST_ASSERT_EQUAL_INT(50, (int)num(":tt.score"));
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, (int)num(":tt.pause"), "a blossom withholds three quanta");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, (int)num(":tt.dizzy"), "the dizzy timer did not start");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":tt.chain"), "the bug chain did not reset");

    // Hunting bugs go dizzy and are made to reverse; nest bugs are immune.
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, actor("state", 2), "a hunting bug did not go dizzy");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, actor("state", 3), "a hunting bug did not go dizzy");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, actor("rev", 2), "no reversal was queued");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, actor("state", 4), "a nest bug was not immune");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, actor("state", 5), "a nest bug was not immune");
}

// The dizzy timer pauses the patrol/hunt schedule and restores hunting when
// it runs out.
static void test_dizzy_timer_pauses_the_schedule_then_restores(void)
{
    put_actor(2, 10, 24, D_LEFT, 0, 1);
    run("make \"tt.dizzy 3 .setitem 2 :a.state 4");
    int phase = (int)num(":tt.phase");
    int timer = (int)num(":tt.mtimer");

    run("step.mode.clock");
    TEST_ASSERT_EQUAL_INT_MESSAGE(timer, (int)num(":tt.mtimer"), "the schedule kept running");
    TEST_ASSERT_EQUAL_INT(phase, (int)num(":tt.phase"));
    TEST_ASSERT_EQUAL_INT(2, (int)num(":tt.dizzy"));

    run("step.mode.clock step.mode.clock");
    TEST_ASSERT_EQUAL_INT(0, (int)num(":tt.dizzy"));
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, actor("state", 2), "the bug stayed dizzy");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, actor("rev", 2), "no reversal followed the restore");

    // With dizzy time over, the schedule advances again.
    run("step.mode.clock");
    TEST_ASSERT_EQUAL_INT(timer - 1, (int)num(":tt.mtimer"));
}

static void test_bonus_appears_at_the_gate_and_times_out(void)
{
    run("make \"tt.painted 0 make \"tt.bonus 0 make \"tt.bonus.done 0");
    run("bonus.check");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":tt.bonus"), "a bonus appeared too early");

    runf("make \"tt.painted %d", (int)num("item 1 :tt.bonus.at"));
    run("bonus.check");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, (int)num(":tt.bonus"), "no bonus at the first threshold");
    TEST_ASSERT_EQUAL_INT(1, (int)num(":tt.bonus.done"));
    TEST_ASSERT_TRUE_MESSAGE(mock_device_get_turtle(5)->visible, "the bonus turtle stayed hidden");
    TEST_ASSERT_EQUAL_FLOAT(num("tile.x :tt.gate.col"), mock_device_get_turtle(5)->x);
    TEST_ASSERT_EQUAL_FLOAT(num("tile.y :tt.gate.row"), mock_device_get_turtle(5)->y);

    // It is bounded near nine seconds, then it goes away on its own.
    int t = (int)num(":tt.bonus");
    TEST_ASSERT_GREATER_OR_EQUAL(200, t);
    TEST_ASSERT_LESS_THAN(251, t);
    run("repeat :tt.bonus [step.bonus]");
    TEST_ASSERT_EQUAL_INT(0, (int)num(":tt.bonus"));
    TEST_ASSERT_FALSE_MESSAGE(mock_device_get_turtle(5)->visible, "the bonus stayed on screen");

    // Only two appear per level.
    runf("make \"tt.painted %d", (int)num("item 2 :tt.bonus.at"));
    run("bonus.check");
    TEST_ASSERT_EQUAL_INT(2, (int)num(":tt.bonus.done"));
    run("make \"tt.bonus 0 make \"tt.painted 999 bonus.check");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":tt.bonus"), "a third bonus appeared");
}

// The bonus is taken by standing on the gate tile, using the same same-tile
// comparison every other collision uses.
static void test_taking_the_bonus_scores_by_level(void)
{
    run("set.profile 3");
    run("make \"tt.score 0 make \"tt.bonus 100");
    put_actor(1, 2, 30, D_LEFT, 0, 0);
    run("take.bonus");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":tt.score"), "the bonus scored from another tile");
    TEST_ASSERT_EQUAL_INT(100, (int)num(":tt.bonus"));

    run(".setitem 1 :a.col :tt.gate.col");
    run(".setitem 1 :a.row :tt.gate.row");
    run("take.bonus");
    TEST_ASSERT_EQUAL_INT_MESSAGE(500, (int)num(":tt.score"), "the level-3 bonus is 500");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":tt.bonus"), "the bonus stayed on the board");
}

static void test_level_completes_when_no_tiles_remain(void)
{
    truth("level.over?", "false");
    run("make \"tt.left 0 make \"tt.cleared \"true");
    truth("level.over?", "true");
}

// ---------------------------------------------------------------------------
// 5. Bug targeting
// ---------------------------------------------------------------------------

// Each personality aims somewhere different, in every turtle heading. The
// classic's facing-up overflow is deliberately not reproduced.
static void test_hunt_targets_in_every_direction(void)
{
    run("make \"tt.mode 1");
    put_actor(2, 5, 5, D_LEFT, 0, 1);   // Dart, also Echo's pivot

    for (int d = 1; d <= 4; d++) {
        put_actor(1, 14, 20, d, 0, 0);
        int pc = 14, pr = 20;
        int dc = DC[d], dr = DR[d];

        TEST_ASSERT_EQUAL_INT_MESSAGE(pc, (int)num("target.col 2"), "Dart must aim at the turtle");
        TEST_ASSERT_EQUAL_INT(pr, (int)num("target.row 2"));

        TEST_ASSERT_EQUAL_INT_MESSAGE(pc + 4 * dc, (int)num("target.col 3"),
                                      "Swoop must aim four tiles ahead");
        TEST_ASSERT_EQUAL_INT(pr + 4 * dr, (int)num("target.row 3"));

        // Echo doubles the vector from Dart to the tile two ahead.
        int px = pc + 2 * dc, py = pr + 2 * dr;
        TEST_ASSERT_EQUAL_INT(2 * px - 5, (int)num("target.col 4"));
        TEST_ASSERT_EQUAL_INT(2 * py - 5, (int)num("target.row 4"));
    }
}

// Moss hunts from afar and turns for home once inside eight tiles.
static void test_moss_switches_at_eight_tiles(void)
{
    run("make \"tt.mode 1");
    put_actor(1, 14, 20, D_UP, 0, 0);

    put_actor(5, 14, 4, D_UP, 0, 1);      // 16 rows away
    truth("moss.far?", "true");
    TEST_ASSERT_EQUAL_INT(14, (int)num("target.col 5"));
    TEST_ASSERT_EQUAL_INT(20, (int)num("target.row 5"));

    put_actor(5, 14, 13, D_UP, 0, 1);     // exactly 7 rows away
    truth("moss.far?", "false");
    TEST_ASSERT_EQUAL_INT((int)num("item 5 :tt.corner.col"), (int)num("target.col 5"));
    TEST_ASSERT_EQUAL_INT((int)num("item 5 :tt.corner.row"), (int)num("target.row 5"));

    put_actor(5, 14, 12, D_UP, 0, 1);     // exactly 8 rows away
    truth("moss.far?", "true");
}

// Patrol sends each bug to its own corner; frenzy makes Dart ignore that.
static void test_patrol_targets_corners_until_dart_goes_frenzied(void)
{
    run("make \"tt.mode 0");
    put_actor(1, 14, 20, D_UP, 0, 0);
    put_actor(2, 10, 10, D_LEFT, 0, 1);
    runf("make \"tt.left %d", (int)num(":tt.frenzy1") + 10);

    TEST_ASSERT_EQUAL_INT((int)num("item 2 :tt.corner.col"), (int)num("target.col 2"));
    truth("frenzy? 2", "false");

    runf("make \"tt.left %d", (int)num(":tt.frenzy1") - 1);
    truth("frenzy? 2", "true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(14, (int)num("target.col 2"),
                                  "a frenzied Dart must hunt during patrol");
    // Only Dart goes frenzied.
    truth("frenzy? 3", "false");
    TEST_ASSERT_EQUAL_INT((int)num("item 3 :tt.corner.col"), (int)num("target.col 3"));
}

// A bug cannot reverse voluntarily, and ties break up, left, down, right.
static void test_exit_choice_excludes_reverse_and_breaks_ties_in_order(void)
{
    // Row 5 is the open top corridor, hedge above and below: from column 10
    // the only exits are left and right, and the target is equidistant.
    put_actor(2, 10, 5, D_RIGHT, 0, 1);
    truth("open? 10 5 1", "false");
    truth("open? 10 5 3", "false");
    TEST_ASSERT_EQUAL_INT_MESSAGE(D_RIGHT, (int)num("best.dir 2 10 5 4 10 5"),
                                  "the bug reversed, or broke the tie out of order");

    // Facing left at the same tile, the reverse is now right, so it must not
    // be chosen even though the scores are identical.
    TEST_ASSERT_EQUAL_INT(D_LEFT, (int)num("best.dir 2 10 5 2 10 5"));

    // Ties break up, left, down, right. Tile (7,15) on the nest ring opens
    // up, down and right, and with the target on the tile itself all three
    // score the same, so up must win.
    truth("open? 7 15 1", "true");
    truth("open? 7 15 3", "true");
    truth("open? 7 15 4", "true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(D_UP, (int)num("best.dir 2 7 15 4 7 15"),
                                  "up must win a tie");

    // With every non-reversing exit blocked, reversing is the last resort:
    // tile (2,4) is hedge with only the path at (2,5) below it.
    TEST_ASSERT_EQUAL_INT_MESSAGE(D_DOWN, (int)num("best.dir 2 2 4 1 2 4"),
                                  "a bug with no forward exit must turn back");
}

// A bug looks one tile ahead, so the exit it commits at a centre is the one it
// chose on the previous tile.
static void test_bug_commits_the_exit_chosen_a_tile_earlier(void)
{
    run("make \"tt.mode 1");
    put_actor(1, 2, 30, D_LEFT, 0, 0);   // turtle in the bottom-left
    put_actor(2, 14, 30, D_LEFT, 0, 1);
    run(".setitem 2 :a.next 0 bug.arrive 2");

    int chosen = actor("next", 2);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, chosen, "no exit was preselected");

    run("move.actor 2 128");
    TEST_ASSERT_EQUAL_INT_MESSAGE(chosen, actor("dir", 2), "the preselected exit was not taken");
}

// A pending mode reversal overrides the no-reverse rule once, at the next
// tile centre.
static void test_mode_change_queues_one_reversal(void)
{
    put_actor(2, 14, 30, D_LEFT, 0, 1);
    put_actor(3, 12, 18, D_UP, 0, 2);
    run("reverse.bugs");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, actor("rev", 2), "a hunting bug was not told to reverse");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, actor("rev", 3), "a nest bug must not be reversed");

    run("move.actor 2 128");
    TEST_ASSERT_EQUAL_INT_MESSAGE(D_RIGHT, actor("dir", 2), "the reversal was not applied");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, actor("rev", 2), "the reversal was not consumed");
}

// ---------------------------------------------------------------------------
// 6. The nest
// ---------------------------------------------------------------------------

static void test_release_counters_free_waiting_bugs(void)
{
    // At a level start the counters are personal.
    run("setup.level");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, actor("state", 3), "Swoop must leave immediately");
    TEST_ASSERT_EQUAL_INT(2, actor("state", 4));
    TEST_ASSERT_EQUAL_INT(2, actor("state", 5));

    runf("make \"tt.painted %d release.check", (int)num("item 4 :tt.release"));
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, actor("state", 4), "Echo's counter did not free it");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, actor("state", 5), "Moss left too early");

    runf("make \"tt.painted %d release.check", (int)num("item 5 :tt.release"));
    TEST_ASSERT_EQUAL_INT(3, actor("state", 5));
}

// After a lost life a global counter replaces the personal ones, so the level
// cannot stall with bugs trapped inside.
static void test_respawn_switches_to_the_global_release_counters(void)
{
    run("respawn");
    TEST_ASSERT_EQUAL_INT(7, (int)num("item 3 :tt.release"));
    TEST_ASSERT_EQUAL_INT(17, (int)num("item 4 :tt.release"));
    TEST_ASSERT_EQUAL_INT(32, (int)num("item 5 :tt.release"));
    TEST_ASSERT_EQUAL_INT(0, (int)num(":tt.painted"));
}

// A lost life always resumes play.  Otherwise a latched game pause leaves
// every bug frozen after respawn, including the nest bugs that should bob.
static void test_respawn_unpauses_the_bug_simulation(void)
{
    run("make \"tt.frame 0 make \"tt.paused \"true make \"tt.lives 3 handle.death");
    truth(":tt.paused", "false");

    TEST_ASSERT_EQUAL_INT(2, actor("state", 4));
    TEST_ASSERT_EQUAL_INT(0, actor("off", 4));
    run("repeat 4 [play.frame]");
    TEST_ASSERT_TRUE_MESSAGE(actor("col", 2) != 14 || actor("off", 2) != 0,
                             "the path bug did not resume moving");
    TEST_ASSERT_EQUAL_INT_MESSAGE(16, actor("off", 4),
                                  "the waiting bug did not resume bobbing");
}

// place.actors has to hand back genuinely fresh state lists.  Written as bare
// literals they belonged to the procedure's own body, so the frame loop's
// .setitems scribbled on them permanently and a respawn rebound the same
// scribbled lists: the bugs stood on the right tiles carrying the previous
// life's states, part-tile offsets and pending reversals, and any left dizzy
// or eyes-only simply froze.
static void test_place_actors_resets_every_state_list(void)
{
    put_actor(2, 9, 9, D_DOWN, 7, 4);
    put_actor(3, 11, 5, D_LEFT, 3, 5);
    put_actor(4, 17, 18, D_RIGHT, 9, 6);
    run(".setitem 4 :a.next 2 .setitem 2 :a.rev 1");

    run("place.actors");

    const int dir[5] = {2, 2, 1, 1, 1};
    const int state[5] = {0, 1, 3, 2, 2};
    for (int i = 1; i <= 5; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(dir[i - 1], actor("dir", i), "stale a.dir");
        TEST_ASSERT_EQUAL_INT_MESSAGE(state[i - 1], actor("state", i), "stale a.state");
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, actor("off", i), "stale a.off");
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, actor("rev", i), "stale a.rev");
    }
    // Bug 1 is the only one given a turn up front, by choose.dir.
    TEST_ASSERT_EQUAL_INT(0, actor("next", 1));
    for (int i = 3; i <= 5; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, actor("next", i), "stale a.next");
    }
}

// A no-painting timer forces the preferred waiting bug out, whatever the
// counters say.
static void test_idle_timer_forces_one_bug_out(void)
{
    put_actor(3, 14, 18, D_UP, 0, 2);
    put_actor(4, 12, 18, D_UP, 0, 2);
    run("make \"tt.idle 0");
    runf("repeat %d [step.nest.clock]", (int)num(":tt.p.idle") - 1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, actor("state", 3), "a bug left before the timer expired");

    run("step.nest.clock");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, actor("state", 3), "the idle timer freed nobody");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, actor("state", 4), "the idle timer freed more than one bug");
}

// Leaving is scripted: slide to the door column, then rise through it.
static void test_leaving_bug_aligns_with_the_door_then_rises(void)
{
    int door = (int)num(":tt.door.col");
    put_actor(4, 12, 18, D_UP, 0, 3);
    TEST_ASSERT_LESS_THAN(door, 12 + 1);

    for (int i = 0; i < 400 && actor("state", 4) == 3; i++) run("step.one.bug 4");

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, actor("state", 4), "the bug never left the nest");
    TEST_ASSERT_LESS_THAN_MESSAGE((int)num(":tt.door.row"), actor("row", 4),
                                  "the bug did not rise above the door");
}

// An eaten bug flies back as wings, drops through the door, regrows and
// leaves again.
static void test_wings_return_regrow_and_leave(void)
{
    run("make \"tt.mode 1");
    put_actor(1, 2, 30, D_LEFT, 0, 0);
    put_actor(3, 10, 24, D_LEFT, 0, 5);

    int guard = 0;
    while (actor("state", 3) == 5 && guard++ < 4000) run("step.one.bug 3");
    TEST_ASSERT_LESS_THAN_MESSAGE(4000, guard, "the wings never found the nest");
    TEST_ASSERT_EQUAL_INT_MESSAGE(6, actor("state", 3), "the wings did not drop in");

    guard = 0;
    while (actor("state", 3) == 6 && guard++ < 400) run("step.one.bug 3");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, actor("state", 3), "the bug did not regrow and leave");

    guard = 0;
    while (actor("state", 3) == 3 && guard++ < 400) run("step.one.bug 3");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, actor("state", 3), "the regrown bug never resumed hunting");
}

// A bug is eaten wherever it happens to be, and eat.bug clears the exit it
// had chosen, so the wings set off on the heading it died on. Nothing is
// committed for the tile it arrives at next, and that heading may face a
// hedge there -- at which point a bug that only chooses on arrival never
// arrives again and flies the maze as wings for the rest of the life. So
// every tile and legal heading has to reach the nest.
static void test_wings_home_from_every_tile_and_heading(void)
{
    static int m[ROWS][COLS];
    read_map(m);
    put_actor(1, 2, 30, D_LEFT, 0, 0);

    for (int r = 1; r <= ROWS; r++) {
        for (int c = 1; c <= COLS; c++) {
            if (!walkable(m[r - 1][c - 1])) continue;
            for (int d = 1; d <= 4; d++) {
                int nc, nr;
                if (!step_tile(c, r, d, &nc, &nr)) continue;
                if (!walkable(m[nr - 1][nc - 1])) continue;

                put_actor(3, c, r, d, 0, 5);
                run("recycle");
                int guard = 0;
                while (actor("state", 3) == 5 && guard++ < 40) run("repeat 40 [step.one.bug 3]");

                char msg[80];
                snprintf(msg, sizeof(msg), "wings from col %d row %d dir %d stopped at col %d row %d",
                         c, r, d, actor("col", 3), actor("row", 3));
                TEST_ASSERT_NOT_EQUAL_MESSAGE(5, actor("state", 3), msg);
            }
        }
    }
}

// The body frames are what put a bug back in its own shape after it has been
// wings -- nothing else ever sets one. They only arrive if the animation
// actually runs, and setanim restarts the animation clock, so dressing a bug
// that is already dressed holds the walk cycle on whatever frame it is on:
// for a regrown bug, the wings, for the rest of the life.
static void test_a_regrown_bug_puts_its_body_back_on(void)
{
    const int wings = (int)num(":sh.wings"), body = (int)num(":sh.bug");
    uint32_t t = 100000;
    set_mock_ticks(t);
    run("dress.bugs");

    put_actor(3, 14, 18, D_UP, 0, 5);
    run("dress.bug 3");
    TEST_ASSERT_EQUAL_INT_MESSAGE(wings, mock_device_get_state()->turtles[2].shape,
                                  "an eaten bug is not wearing the wings");

    // Regrown, and then a second of frames at 25 fps, dressed every frame the
    // way play.frame dresses them.
    runf(".setitem 3 :a.state 1");
    for (int k = 0; k < 25; k++) {
        set_mock_ticks(t += 40);
        run("dress.bugs");
    }

    int shape = mock_device_get_state()->turtles[2].shape;
    char msg[80];
    snprintf(msg, sizeof(msg), "the regrown bug is wearing shape %d", shape);
    TEST_ASSERT_TRUE_MESSAGE(shape == body || shape == body + 1, msg);
}

// ---------------------------------------------------------------------------
// 7. Collisions and scoring
// ---------------------------------------------------------------------------

static void test_collision_outcomes_by_bug_state(void)
{
    put_actor(1, 10, 24, D_LEFT, 0, 0);

    // A hunting bug on the same tile is lethal.
    put_actor(2, 10, 24, D_RIGHT, 0, 1);
    put_actor(3, 5, 5, D_UP, 0, 1);
    put_actor(4, 5, 6, D_UP, 0, 1);
    put_actor(5, 5, 8, D_UP, 0, 1);
    run("make \"tt.dying \"false check.collisions");
    truth(":tt.dying", "true");

    // Bugs in the nest, leaving it, or flying home as wings are harmless.
    int harmless[4] = {2, 3, 5, 6};
    for (int k = 0; k < 4; k++) {
        put_actor(2, 10, 24, D_RIGHT, 0, harmless[k]);
        run("make \"tt.dying \"false check.collisions");
        truth(":tt.dying", "false");
    }
}

// The chain doubles for each dizzy bug eaten under one blossom.
static void test_dizzy_chain_scores_200_400_800_1600(void)
{
    run("make \"tt.score 0 make \"tt.chain 0 make \"tt.dizzy 100");
    put_actor(1, 10, 24, D_LEFT, 0, 0);
    int want[4] = {200, 600, 1400, 3000};

    for (int i = 0; i < 4; i++) {
        put_actor(2 + i, 10, 24, D_RIGHT, 0, 4);
        run("check.collisions");
        TEST_ASSERT_EQUAL_INT_MESSAGE(want[i], (int)num(":tt.score"), "wrong chain score");
        TEST_ASSERT_EQUAL_INT_MESSAGE(5, actor("state", 2 + i), "an eaten bug did not become wings");
        // Move it away so it is not counted twice.
        runf(".setitem %d :a.col 3", 2 + i);
    }
    truth(":tt.dying", "false");
}

// A head-on meeting: two actors closing from adjacent tiles exchange tiles in
// one frame and so never share one. The swap half of the test catches them,
// or a bug walking straight into the turtle would pass through it.
static void test_opposite_direction_tile_swap_collides(void)
{
    run("make \"tt.dying \"false");
    put_actor(1, 10, 30, D_RIGHT, 0, 0);
    put_actor(2, 11, 30, D_LEFT, 0, 1);
    put_actor(3, 5, 5, D_UP, 0, 1);
    put_actor(4, 5, 6, D_UP, 0, 1);
    put_actor(5, 5, 8, D_UP, 0, 1);

    run("move.actor 1 128 move.actor 2 128");
    TEST_ASSERT_EQUAL_INT(11, actor("col", 1));
    TEST_ASSERT_EQUAL_INT(10, actor("col", 2));
    truth("same.tile? 2", "false");   // or the swap half is not what is being tested
    run("check.collisions");
    truth(":tt.dying", "true");
}

// The same crossing while the bug is dizzy eats it, on the same rule.
static void test_dizzy_bug_is_eaten_on_a_tile_swap(void)
{
    run("make \"tt.dying \"false make \"tt.score 0 make \"tt.chain 0");
    put_actor(1, 10, 30, D_RIGHT, 0, 0);
    put_actor(2, 11, 30, D_LEFT, 0, 4);
    put_actor(3, 5, 5, D_UP, 0, 1);
    put_actor(4, 5, 6, D_UP, 0, 1);
    put_actor(5, 5, 8, D_UP, 0, 1);

    run("move.actor 1 128 move.actor 2 128");
    run("check.collisions");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, actor("state", 2), "a dizzy bug crossed the turtle and was not eaten");
    TEST_ASSERT_EQUAL_INT(200, (int)num(":tt.score"));
    truth(":tt.dying", "false");
}

// Only an exchange counts. A bug following the turtle moves onto the tile the
// turtle just left, which is not a crossing and must not kill.
static void test_a_bug_following_the_turtle_does_not_collide(void)
{
    run("make \"tt.dying \"false");
    put_actor(1, 10, 30, D_RIGHT, 0, 0);
    put_actor(2, 9, 30, D_RIGHT, 0, 1);
    put_actor(3, 5, 5, D_UP, 0, 1);
    put_actor(4, 5, 6, D_UP, 0, 1);
    put_actor(5, 5, 8, D_UP, 0, 1);

    run("move.actor 1 128 move.actor 2 128");
    TEST_ASSERT_EQUAL_INT(11, actor("col", 1));
    TEST_ASSERT_EQUAL_INT(10, actor("col", 2));
    run("check.collisions");
    truth(":tt.dying", "false");
}

// The snapshot is taken before anything moves, so a frame that has only just
// placed its actors cannot report a crossing.
static void test_snap_tiles_records_the_pre_movement_tiles(void)
{
    put_actor(1, 10, 30, D_RIGHT, 0, 0);
    put_actor(2, 11, 30, D_LEFT, 0, 1);
    run("snap.tiles");
    truth("swapped.tile? 2", "false");   // nothing has moved yet
    run("move.actor 1 128 move.actor 2 128");
    truth("swapped.tile? 2", "true");
    run("snap.tiles");
    truth("swapped.tile? 2", "false");   // the snapshot follows the actors
}

static void test_extra_life_is_awarded_once(void)
{
    run("make \"tt.score 0 make \"tt.lives 3 make \"tt.extra \"false");
    run("add.score 9990");
    TEST_ASSERT_EQUAL_INT(3, (int)num(":tt.lives"));
    run("add.score 10");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, (int)num(":tt.lives"), "no extra life at 10,000");
    truth(":tt.extra", "true");
    run("add.score 20000");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, (int)num(":tt.lives"), "a second extra life was awarded");
}

static void test_high_score_tracks_the_session_best(void)
{
    run("make \"tt.high 0 make \"tt.score 0 add.score 1234");
    TEST_ASSERT_EQUAL_INT(1234, (int)num(":tt.high"));
    run("make \"tt.score 0 add.score 100");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1234, (int)num(":tt.high"), "the high score went backwards");
}

// ---------------------------------------------------------------------------
// 8. The drawn maze, and the frame loop
// ---------------------------------------------------------------------------

// A hedge tile is a band of :tt.wall pixels run out to each hedge neighbour:
// one stroke per set bit of the mask, out to the neighbour's centre and no
// further, so a face with no stroke stands back from the cell edge and gives
// its pixels to the corridor. The bit order pinned here is the one build.map
// computes with. Two crossing bands miss the cell's four corner pixels, so an
// inner corner -- both of its sides hedge -- takes a dot as well.
static void test_a_hedge_tile_runs_one_band_per_hedge_neighbour(void)
{
    // 8 up, 4 left, 2 down, 1 right, in the order make.hedge draws them.
    const int bit[4] = {8, 4, 2, 1};
    const float dx[4] = {0.0f, -8.0f, 0.0f, 8.0f};
    const float dy[4] = {8.0f, 0.0f, -8.0f, 0.0f};
    // The corners, in the order make.hedge fills them: up-left, up-right,
    // down-left, down-right.
    const int corner[4][2] = {{8, 4}, {8, 1}, {2, 4}, {2, 1}};
    const float cx[4] = {-4.0f, 4.0f, -4.0f, 4.0f};
    const float cy[4] = {4.0f, 4.0f, -4.0f, -4.0f};

    int wall = (int)num(":tt.wall");

    for (int m = 0; m < 16; m++) {
        mock_device_clear_graphics();
        runf("make.hedge %d %d", S_HEDGE + m, m);
        const MockDeviceState *st = mock_device_get_state();

        char msg[64];
        snprintf(msg, sizeof(msg), "mask %d", m);
        int n = 0;
        for (int d = 0; d < 4; d++) {
            if (!(m & bit[d])) continue;
            TEST_ASSERT_GREATER_THAN_MESSAGE(n, st->graphics.line_count, msg);
            const MockLine *l = &st->graphics.lines[n++];
            TEST_ASSERT_EQUAL_INT_MESSAGE(wall, l->pen_size, msg);
            TEST_ASSERT_EQUAL_FLOAT(0.0f, l->x1);
            TEST_ASSERT_EQUAL_FLOAT(0.0f, l->y1);
            TEST_ASSERT_EQUAL_FLOAT(dx[d], l->x2);
            TEST_ASSERT_EQUAL_FLOAT(dy[d], l->y2);
        }
        TEST_ASSERT_EQUAL_INT_MESSAGE(n, st->graphics.line_count, msg);

        // The background patch that clears the cell -- pen 16 covers the
        // whole 8x8 cell including its corners, so no tile needs the screen
        // cleared first -- then the hedge, then a dot per inner corner.
        int k = 0;
        TEST_ASSERT_EQUAL_INT_MESSAGE(16, st->graphics.dots[k++].pen_size, msg);
        TEST_ASSERT_EQUAL_INT_MESSAGE(wall, st->graphics.dots[k++].pen_size, msg);
        for (int i = 0; i < 4; i++) {
            if ((m & corner[i][0]) == 0 || (m & corner[i][1]) == 0) continue;
            TEST_ASSERT_GREATER_THAN_MESSAGE(k, st->graphics.dot_count, msg);
            TEST_ASSERT_EQUAL_INT_MESSAGE(4, st->graphics.dots[k].pen_size, msg);
            TEST_ASSERT_EQUAL_FLOAT(cx[i], st->graphics.dots[k].x);
            TEST_ASSERT_EQUAL_FLOAT(cy[i], st->graphics.dots[k].y);
            k++;
        }
        TEST_ASSERT_EQUAL_INT_MESSAGE(k, st->graphics.dot_count, msg);
    }
}

// The corridor is what the hedge gives up: a cell is 8 pixels, a hedge keeps
// :tt.wall of them on a face it turns toward a corridor, and the corridor
// takes the rest on each side. Two hedges facing each other across a lane is
// the widest a corridor gets.
static void test_the_corridor_is_wider_than_its_cell(void)
{
    int wall = (int)num(":tt.wall");
    TEST_ASSERT_TRUE_MESSAGE(wall > 0 && wall < 8, "the hedge must be thinner than its cell");
    TEST_ASSERT_TRUE_MESSAGE((8 - wall) % 2 == 0, "an odd inset cannot be centred in the cell");
    TEST_ASSERT_EQUAL_INT_MESSAGE(10, 8 + (8 - wall), "the corridor is not 10 pixels wide");
}

// The board is baked rather than carved, so where the map lands on screen is
// the thing to pin: 224x288 pixels centred on 320x320, and nothing outside
// it. The mock does not rasterise the pen, so stage the canvas first -- every
// tile the bank then captures is the same known index, and where that index
// lands is the whole question. It also settles bank coverage: an empty slot
// bakes as background, so a board painted edge to edge in the staged ink
// means every slot the map names really was captured.
static void test_the_bake_puts_the_board_where_the_pen_did(void)
{
    const int ink = 77;
    mock_device_paint_canvas(0, 0, MOCK_SCREEN_WIDTH_PX, MOCK_SCREEN_HEIGHT_PX, (uint8_t)ink);
    run("setup.tiles");        // recapture the bank off the staged canvas
    run("reset.board draw.board");

    int bg = (int)num(":c.bg");
    for (int y = 0; y < MOCK_SCREEN_HEIGHT_PX; y++) {
        for (int x = 0; x < MOCK_SCREEN_WIDTH_PX; x++) {
            bool board = x >= BOARD_X0 && x < BOARD_X0 + COLS * 8 &&
                         y >= BOARD_Y0 && y < BOARD_Y0 + ROWS * 8;
            int want = board ? ink : bg;
            if ((int)mock_device_get_canvas_point(x, y) == want) continue;
            char msg[80];
            snprintf(msg, sizeof(msg), "pixel %d,%d is %d, wanted %d", x, y,
                     mock_device_get_canvas_point(x, y), want);
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

// Repairing one cell has to hit that cell and no other: the blossom erase
// leans on it, and an offset that is wrong by a cell would only show up here
// or on a real screen.
static void test_stamping_one_cell_repairs_exactly_that_cell(void)
{
    const int ink = 77;
    mock_device_paint_canvas(0, 0, MOCK_SCREEN_WIDTH_PX, MOCK_SCREEN_HEIGHT_PX, (uint8_t)ink);
    run("setup.tiles");
    run("reset.board draw.board");

    run("set.tile 3 4 0 stamp.tile 3 4");
    int bg = (int)num(":c.bg");
    int x0 = BOARD_X0 + 2 * 8, y0 = BOARD_Y0 + 3 * 8;
    for (int y = y0 - 1; y < y0 + 9; y++) {
        for (int x = x0 - 1; x < x0 + 9; x++) {
            bool cell = x >= x0 && x < x0 + 8 && y >= y0 && y < y0 + 8;
            char msg[80];
            snprintf(msg, sizeof(msg), "pixel %d,%d", x, y);
            TEST_ASSERT_EQUAL_INT_MESSAGE(cell ? bg : ink,
                                          (int)mock_device_get_canvas_point(x, y), msg);
        }
    }
}

// The cell of the k'th of the fifteen cells READY covers, 1-based board
// coordinates, as ready.cells walks them.
static void ready_cell(int k, int *col, int *row)
{
    *col = 11 + k % 5;
    *row = 20 + k / 5;
}

// READY is written over the corridor below the nest, where the turtle has
// been and left a trail. Neither obvious erase will do: writing the word
// again in the background colour punches it out of the trail, and repainting
// the cells from the map takes the trail with them. The cells are
// photographed and stamped back instead, so what was under the word survives
// it exactly -- and the mock does not rasterise text, so the round trip is
// staged here by scribbling over the cells in the interval the glyphs would
// have occupied.
static void test_ready_puts_back_the_trail_it_covered(void)
{
    const int ink = 77;
    mock_device_paint_canvas(0, 0, MOCK_SCREEN_WIDTH_PX, MOCK_SCREEN_HEIGHT_PX, (uint8_t)ink);
    run("setup.tiles");        // every tile in the bank is now the staged ink
    run("reset.board draw.board");

    // Stand in for the pen trail, a different colour in every cell so a
    // photograph put back in the wrong cell cannot pass.
    for (int k = 0; k < 15; k++) {
        int c, r;
        ready_cell(k, &c, &r);
        mock_device_paint_canvas(BOARD_X0 + (c - 1) * 8, BOARD_Y0 + (r - 1) * 8, 8, 8,
                                 (uint8_t)(40 + k));
    }
    static int slot_before[15];
    for (int k = 0; k < 15; k++) {
        int c, r;
        ready_cell(k, &c, &r);
        slot_before[k] = (int)numf("tile.at %d %d", c, r);
    }

    run("ready.cells \"true");
    // Whatever the glyphs would have put there.
    mock_device_paint_canvas(BOARD_X0 + 10 * 8, BOARD_Y0 + 19 * 8, 5 * 8, 3 * 8, 99);
    run("ready.cells \"false");

    for (int k = 0; k < 15; k++) {
        int c, r;
        ready_cell(k, &c, &r);
        char msg[80];
        snprintf(msg, sizeof(msg), "col %d row %d", c, r);
        TEST_ASSERT_EQUAL_INT_MESSAGE(slot_before[k], (int)numf("tile.at %d %d", c, r),
                                      "a cell was left holding its scratch slot");
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                TEST_ASSERT_EQUAL_INT_MESSAGE(
                    40 + k,
                    (int)mock_device_get_canvas_point(BOARD_X0 + (c - 1) * 8 + x,
                                                      BOARD_Y0 + (r - 1) * 8 + y), msg);
    }

    // And the whole screen comes back through ready.screen untouched, with
    // the word written once and never in the background colour.
    static uint8_t before[MOCK_SCREEN_HEIGHT_PX][MOCK_SCREEN_WIDTH_PX];
    for (int y = 0; y < MOCK_SCREEN_HEIGHT_PX; y++)
        for (int x = 0; x < MOCK_SCREEN_WIDTH_PX; x++)
            before[y][x] = mock_device_get_canvas_point(x, y);

    int labels = mock_device_get_state()->label.count;
    run("ready.screen");
    const MockDeviceState *s = mock_device_get_state();

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, s->label.count - labels, "READY was written more than once");
    TEST_ASSERT_EQUAL_STRING("READY", s->label.last_text);
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)num(":c.text"), s->label.last_colour,
                                  "READY was erased with the background colour");

    // The glyph box, in screen pixels, from where the game actually wrote it:
    // 8 wide and 10 tall a glyph, laid rightwards from the write position and
    // centred on it vertically. The cells photographed have to cover it.
    int gx0 = (int)(s->label.last_x + MOCK_SCREEN_WIDTH_PX / 2);
    int gy0 = (int)(MOCK_SCREEN_HEIGHT_PX / 2 - s->label.last_y) - 5;
    TEST_ASSERT_EQUAL_INT_MESSAGE(BOARD_X0 + 10 * 8, gx0, "READY does not start on column 11");
    TEST_ASSERT_TRUE_MESSAGE(gy0 >= BOARD_Y0 + 19 * 8 && gy0 + 9 < BOARD_Y0 + 22 * 8,
                             "READY does not fall inside rows 20 to 22");

    for (int y = 0; y < MOCK_SCREEN_HEIGHT_PX; y++) {
        for (int x = 0; x < MOCK_SCREEN_WIDTH_PX; x++) {
            char msg[80];
            snprintf(msg, sizeof(msg), "pixel %d,%d", x, y);
            TEST_ASSERT_EQUAL_INT_MESSAGE(before[y][x],
                                          (int)mock_device_get_canvas_point(x, y), msg);
        }
    }
}

// The score line lives in the three rows above the maze and the spare lives
// in the two below it, so neither can overwrite a corridor.
static void test_hud_stays_out_of_the_maze(void)
{
    mock_device_clear_graphics();
    run("draw.labels");
    const MockDeviceState *s = mock_device_get_state();
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, s->label.count, "no labels were drawn");
    // Row 4 is the top hedge, centred on y = 116 and eight pixels tall, so
    // any HUD text must sit above y = 120.
    TEST_ASSERT_TRUE_MESSAGE(s->label.last_y > 120.0f, "the score line overlaps the maze");

    run("make \"tt.lives 3");
    mock_device_clear_graphics();
    run("draw.lives");
    s = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, s->graphics.stamp_count, "one stamp per spare life");
    for (int i = 0; i < s->graphics.stamp_count; i++) {
        // Row 34 is the bottom hedge, centred on y = -140... the lives band
        // is below it, under y = -128.
        TEST_ASSERT_TRUE_MESSAGE(s->graphics.stamps[i].y < -128.0f,
                                 "a life stamp overlaps the maze");
    }

    run("make \"tt.lives 9 draw.lives");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(5, mock_device_get_state()->graphics.stamp_count - 3,
                                      "the lives row must be capped, not run off the board");
}

// Both HUD rows take the old picture away by painting the background over it
// and then stamping the new one, so a band has to cover the whole footprint of
// every stamp it is responsible for -- the full row, not just the count drawn
// this time, or losing a life leaves the one that went behind.
static void assert_band_covers_its_row(const char *fill, const char *drawer, int want)
{
    run(fill);
    mock_device_clear_graphics();
    run(drawer);

    const MockDeviceState *s = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, s->graphics.line_count, "one erase band per redraw");
    TEST_ASSERT_EQUAL_INT_MESSAGE(want, s->graphics.stamp_count, drawer);

    // A pen wider than one pixel stamps a filled disc at every point, so the
    // band reaches half its width past each end and either side of the line.
    const MockLine *band = &s->graphics.lines[0];
    TEST_ASSERT_EQUAL_FLOAT_MESSAGE(band->y1, band->y2, "the band must be level");
    float half = band->pen_size / 2.0f;
    float x1 = band->x1 < band->x2 ? band->x1 : band->x2;
    float x2 = band->x1 < band->x2 ? band->x2 : band->x1;

    for (int i = 0; i < s->graphics.stamp_count; i++) {
        const MockStamp *st = &s->graphics.stamps[i];
        char msg[128];
        snprintf(msg, sizeof(msg), "%s: the stamp at %g,%g is not covered by the band",
                 drawer, (double)st->x, (double)st->y);
        // A shape is 16 pixels square, centred on the turtle.
        TEST_ASSERT_TRUE_MESSAGE(st->x - 8.0f >= x1 - half && st->x + 8.0f <= x2 + half, msg);
        TEST_ASSERT_TRUE_MESSAGE(st->y - 8.0f >= band->y1 - half &&
                                 st->y + 8.0f <= band->y1 + half, msg);
    }
}

#define FILL_LIVES "make \"tt.lives 5"
#define FILL_HISTORY \
    "make \"tt.history [] repeat 7 [make \"tt.history lput (item 1 :tt.shapes) :tt.history]"

static void test_the_hud_bands_erase_the_whole_stamp(void)
{
    assert_band_covers_its_row(FILL_LIVES, "draw.lives", 5);
    assert_band_covers_its_row(FILL_HISTORY, "draw.history", 7);
}

// A stamp is 16 pixels square against an 8-pixel cell, so a HUD row laid out
// from the outermost column hangs half a cell into the board's side wall and
// the band that erases it takes a bite out of that wall. Both rows must keep
// clear of columns 1 and 28 along their whole width.
static void assert_row_clears_the_walls(const char *fill, const char *drawer)
{
    // The walls are one cell wide, centred on the outermost column centres.
    float inside_left = num("tile.x 1") + 4.0f;
    float inside_right = num("tile.x 28") - 4.0f;

    run(fill);
    mock_device_clear_graphics();
    run(drawer);

    const MockDeviceState *s = mock_device_get_state();
    const MockLine *band = &s->graphics.lines[0];
    float half = band->pen_size / 2.0f;
    char msg[128];

    snprintf(msg, sizeof(msg), "%s: the erase band runs into a wall", drawer);
    TEST_ASSERT_TRUE_MESSAGE(band->x1 - half >= inside_left, msg);
    TEST_ASSERT_TRUE_MESSAGE(band->x2 + half <= inside_right, msg);

    for (int i = 0; i < s->graphics.stamp_count; i++) {
        const MockStamp *st = &s->graphics.stamps[i];
        snprintf(msg, sizeof(msg), "%s: the stamp at %g,%g sits in a wall",
                 drawer, (double)st->x, (double)st->y);
        TEST_ASSERT_TRUE_MESSAGE(st->x - 8.0f >= inside_left, msg);
        TEST_ASSERT_TRUE_MESSAGE(st->x + 8.0f <= inside_right, msg);
    }
}

static void test_the_hud_rows_clear_the_board_walls(void)
{
    assert_row_clears_the_walls(FILL_LIVES, "draw.lives");
    assert_row_clears_the_walls(FILL_HISTORY, "draw.history");
}

// A whole frame must run without error, and repeatedly: the parse hazards in
// this file are runtime errors that reading the source does not catch.
static void test_frames_run_and_the_turtle_paints(void)
{
    run("setup.level");
    int left = (int)num(":tt.left");
    run(".setitem 1 :a.next 2");           // head left out of the start tile
    run("repeat 200 [play.frame]");

    TEST_ASSERT_LESS_THAN_MESSAGE(left, (int)num(":tt.left"), "200 frames painted nothing");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, (int)num(":tt.score"), "no score was earned");
    TEST_ASSERT_EQUAL_INT(200, (int)num(":tt.frame"));
}

// Painted paths must survive a lost life: only a new level rebuilds the map.
static void test_respawn_keeps_painted_tiles(void)
{
    run("setup.level");
    TEST_ASSERT_EQUAL_INT(S_SPECK, (int)num("tile.at 2 5"));
    put_actor(1, 2, 5, D_DOWN, 0, 0);
    run("paint.tile");
    int left = (int)num(":tt.left");
    TEST_ASSERT_EQUAL_INT(S_PATH, (int)num("tile.at 2 5"));

    run("respawn");
    TEST_ASSERT_EQUAL_INT_MESSAGE(S_PATH, (int)num("tile.at 2 5"), "respawn repainted the map");
    TEST_ASSERT_EQUAL_INT_MESSAGE(left, (int)num(":tt.left"), "respawn reset the tile count");
    TEST_ASSERT_EQUAL_INT((int)num(":tt.start.col"), actor("col", 1));
    TEST_ASSERT_EQUAL_INT((int)num(":tt.start.row"), actor("row", 1));
}

// Several level rebuilds must not leak: the game recycles the old map before
// decoding the next one, and Logo never collects on its own.
static void test_repeated_level_builds_keep_free_nodes_stable(void)
{
    run("setup.level");
    run("recycle");
    int base = (int)num("nodes");

    for (int i = 0; i < 4; i++) run("make \"tt.level (:tt.level + 1) setup.level");
    run("recycle");
    int after = (int)num("nodes");

    // Some drift is expected from the score words and the bonus history; a
    // leak of a whole map would be about a thousand cells.
    int drift = base - after;
    if (drift < 0) drift = -drift;
    TEST_ASSERT_LESS_THAN_MESSAGE(400, drift, "level rebuilds leak list storage");
}



// Movement is integer, so an ordinary frame costs about one cell -- but Logo
// never collects on its own, and even that drains the pool inside ten minutes
// of play. The frame loop therefore reclaims on a timer, and this pins both
// halves: the cost stays small, and it is garbage the collector can free
// rather than storage the game has retained.
static void test_the_frame_loop_reclaims_what_it_spends(void)
{
    run("setup.level");
    run(".setitem 1 :a.next 2");
    run("repeat 20 [play.frame]");   // settle: the first frames draw the HUD
    run("recycle");
    int before = (int)num("nodes");
    run("repeat 100 [play.frame]");
    int spent = before - (int)num("nodes");
    TEST_ASSERT_LESS_THAN_MESSAGE(250, spent, "a frame costs far more than expected");

    // Nearly all of that must be garbage rather than retained: recycling has
    // to bring free storage back to where it started.
    run("recycle");
    int after = (int)num("nodes");
    TEST_ASSERT_LESS_THAN_MESSAGE(60, before - after,
                                  "100 frames retained storage that recycle cannot free");

    // And the game reclaims on its own, without the test asking, so a level
    // that runs for minutes never reaches `out of space`.
    run("repeat 600 [play.frame]");
    int low = (int)num("nodes");
    TEST_ASSERT_TRUE_MESSAGE(low > 0, "the pool ran dry despite the reclaim timer");
    run("recycle");
    TEST_ASSERT_LESS_THAN_MESSAGE(400, after - (int)num("nodes"),
                             "600 more frames retained storage");
    TEST_ASSERT_TRUE_MESSAGE(low > 0, "the pool ran dry despite the reclaim timer");
}

// A long session must survive: thousands of frames, a lost life and a level
// clear, without an error, without erasing painted tiles, and without the
// free-node count drifting. Logo never collects on its own, so a leak in the
// hot path would show up here as steadily falling free storage.
static void test_state_machine_soak(void)
{
    run("setup.level");
    run("recycle");
    int base_nodes = (int)num("nodes");
    int start_left = (int)num(":tt.left");

    // Steer the turtle down any open corridor, then let it run. Choosing
    // between batches keeps the round trips into Logo cheap.
    unsigned seed = 12345;
    for (int batch = 0; batch < 80; batch++) {
        int col = actor("col", 1), row = actor("row", 1);
        for (int k = 0; k < 4; k++) {
            seed = seed * 1103515245u + 12345u;
            int d = 1 + (int)((seed >> 16) & 3u);
            char code[64];
            snprintf(code, sizeof(code), "open? %d %d %d", col, row, d);
            Result r = eval_string(code);
            TEST_ASSERT_EQUAL_MESSAGE(RESULT_OK, r.status, error_format(r));
            if (strcmp(value_to_string(r.value), "true") == 0) {
                runf(".setitem 1 :a.next %d", d);
                break;
            }
        }
        run("repeat 25 [play.frame]");
    }

    TEST_ASSERT_EQUAL_INT(2000, (int)num(":tt.frame"));
    TEST_ASSERT_LESS_THAN_MESSAGE(start_left - 40, (int)num(":tt.left"),
                                  "the turtle barely painted anything in 2,000 frames");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, (int)num(":tt.score"), "no score in a long run");

    // The bugs must have got out of the nest and be hunting by now.
    int hunting = 0;
    for (int i = 2; i <= 5; i++)
        if (actor("state", i) != 2) hunting++;
    TEST_ASSERT_GREATER_THAN_MESSAGE(2, hunting, "bugs are still stuck in the nest");

    // A lost life keeps the painted map and returns the turtle to its start.
    int left_before = (int)num(":tt.left");
    run("make \"tt.lives 3 handle.death");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, (int)num(":tt.lives"), "a life was not spent");
    TEST_ASSERT_EQUAL_INT_MESSAGE(left_before, (int)num(":tt.left"),
                                  "the death sequence repainted the map");
    TEST_ASSERT_EQUAL_INT((int)num(":tt.start.col"), actor("col", 1));

    // Clearing a level advances it and records the bonus in the history.
    int level = (int)num(":tt.level");
    run("make \"tt.left 0 handle.level.clear");
    TEST_ASSERT_EQUAL_INT_MESSAGE(level + 1, (int)num(":tt.level"), "the level did not advance");
    TEST_ASSERT_EQUAL_INT(1, (int)num("count :tt.history"));
    truth(":tt.cleared", "true");

    // And the next level builds cleanly on top of all of that.
    run("setup.level");
    run("repeat 100 [play.frame]");
    run("recycle");
    int drift = base_nodes - (int)num("nodes");
    if (drift < 0) drift = -drift;
    TEST_ASSERT_LESS_THAN_MESSAGE(400, drift, "a long session leaks list storage");
}

static void test_level_profiles_escalate(void)
{
    run("set.profile 1");
    float base_bug = num(":tt.bstep");
    int base_dizzy = (int)num(":tt.p.dizzy");
    TEST_ASSERT_EQUAL_INT(100, (int)num(":tt.bonus.score"));
    TEST_ASSERT_EQUAL_INT(1, (int)num(":tt.bonus.shape"));

    run("set.profile 9");
    // Unity's GREATER_THAN is integral, and these speeds differ by fractions
    // of a pixel per frame, so compare them as floats.
    TEST_ASSERT_TRUE_MESSAGE(num(":tt.bstep") > base_bug, "bugs did not speed up");
    TEST_ASSERT_LESS_THAN_MESSAGE(base_dizzy, (int)num(":tt.p.dizzy"), "dizzy time did not shorten");
    TEST_ASSERT_EQUAL_INT(2000, (int)num(":tt.bonus.score"));

    run("set.profile 13");
    TEST_ASSERT_EQUAL_INT(5000, (int)num(":tt.bonus.score"));
    TEST_ASSERT_EQUAL_INT(8, (int)num(":tt.bonus.shape"));

    // The bonus shape table has one bitmap per named shape.
    TEST_ASSERT_EQUAL_INT(8, (int)num("count :tt.shapes"));
    for (int i = 1; i <= 8; i++)
        TEST_ASSERT_EQUAL_INT_MESSAGE(16, (int)numf("count item %d :tt.shapes", i),
                                      "a bonus bitmap is not 16 rows");
}

// An ordinary bug must stay slower than the turtle, or the maze is
// unescapable; only a frenzied Dart is allowed to outrun it, which is the
// whole point of frenzy. Every actor must also cover less than a tile per
// frame, which is what keeps exactly one tile-centre event per tile.
static void test_speeds_are_sane_at_25_fps(void)
{
    for (int level = 1; level <= 16; level++) {
        runf("set.profile %d", level);
        float p = num(":tt.pstep"), b = num(":tt.bstep");
        float d = num(":tt.dstep"), t = num(":tt.tstep");
        float w = num(":tt.wstep"), f = num(":tt.fstep");
        char msg[64];
        snprintf(msg, sizeof(msg), "level %d", level);

        TEST_ASSERT_TRUE_MESSAGE(p > 0.0f, msg);
        TEST_ASSERT_TRUE_MESSAGE(b < p, "an ordinary bug must not outrun the turtle");
        TEST_ASSERT_TRUE_MESSAGE(f > b, "a frenzied Dart must be faster than an ordinary bug");
        TEST_ASSERT_TRUE_MESSAGE(d < b, "dizzy bugs must be slow");
        TEST_ASSERT_TRUE_MESSAGE(t < b, "tunnels must slow bugs");
        TEST_ASSERT_TRUE_MESSAGE(w > b, "wings must be quick to get home");
        // Speeds are whole sixteenths of a pixel: fractions would allocate.
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(p, (float)(int)p, "the turtle speed is not a whole unit");
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(b, (float)(int)b, "the bug speed is not a whole unit");
        // Nothing may cross a whole 128-unit tile inside one 40 ms frame.
        TEST_ASSERT_TRUE_MESSAGE(p < 128.0f && f < 128.0f && w < 128.0f, msg);
    }
}

// The P9 M0 instrumentation (docs/tilemap-scrolling-design.md 3.1) is taken on
// hardware, where a script that fails half way through wastes the session, so
// it has to run end to end here first. It is a separate file loaded on top of
// the game, as p10prof is, so its parse hazards are runtime errors nothing
// else would catch until it ran on a board.
static void test_p9m0_instrumentation_runs(void)
{
    int before = proc_count(true);
    load_logo(P9TRAILS_SOURCE);
    int after = proc_count(true);
    char msg[96];
    snprintf(msg, sizeof(msg), "trails+p9trails define %d of %d procedures",
             after, MAX_PROCEDURES);
    TEST_ASSERT_LESS_THAN_MESSAGE(MAX_PROCEDURES - 8, after, msg);
    TEST_ASSERT_GREATER_THAN(before, after);

    run("setup.level");
    TEST_ASSERT_TRUE(num("time.board") >= 0);
    TEST_ASSERT_TRUE(num("time.bake") >= 0);
    TEST_ASSERT_TRUE(num("time.frame") >= 0);
    run("p9m0.trails");
}

// The frame profiler is a separate file loaded on top of the game, so its
// parse hazards are runtime errors nothing else would catch until it ran on a
// board. Run it whole, at a handful of frames rather than 200.
static void test_p10prof_profiler_runs(void)
{
    // The two files together must leave real room in the procedure table:
    // MAX_PROCEDURES is a hard 128, and a board whose workspace is not empty
    // is the likeliest way this ever fails in the field.
    int before = proc_count(true);
    load_logo(P10PROF_SOURCE);
    int after = proc_count(true);
    char msg[96];
    snprintf(msg, sizeof(msg), "trails+p10prof define %d of %d procedures",
             after, MAX_PROCEDURES);
    TEST_ASSERT_LESS_THAN_MESSAGE(MAX_PROCEDURES - 8, after, msg);
    TEST_ASSERT_GREATER_THAN(before, after);

    run("make \"p10prof.n 5");
    run("p10prof");

    // The other 128-wide table, and the one that actually broke first: the
    // game alone holds ~94 globals, and the profiler's marks and sums are
    // named globals too. Counted after the run, because most of them are
    // only created when the frame first executes.
    int globals = var_global_count(true);
    snprintf(msg, sizeof(msg), "trails+p10prof hold %d of %d globals",
             globals, MAX_GLOBAL_VARIABLES);
    TEST_ASSERT_LESS_THAN_MESSAGE(MAX_GLOBAL_VARIABLES - 8, globals, msg);

    // Every slot has to have been tallied, and the parts have to make up the
    // whole: a mark left out of p10prof.tally would go unnoticed otherwise.
    TEST_ASSERT_EQUAL_INT(13, (int)num("count :p10prof.s"));
    for (int i = 1; i <= 13; i++)
        TEST_ASSERT_TRUE_MESSAGE(numf("item %d :p10prof.s", i) >= 0,
                                 "a slot was not tallied");
    float sum = 0;
    for (int i = 1; i <= 13; i++) sum += numf("item %d :p10prof.s", i);
    TEST_ASSERT_EQUAL_FLOAT(num("(:k13 - :k0)"), sum);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_map_shape_and_encoding);
    RUN_TEST(test_the_built_map_agrees_with_the_encoding);
    RUN_TEST(test_the_kept_board_is_restored_at_every_level);
    RUN_TEST(test_decoded_counts_match_the_encoded_words);
    RUN_TEST(test_map_is_left_right_symmetric);
    RUN_TEST(test_paths_form_one_connected_network);
    RUN_TEST(test_no_dead_ends);
    RUN_TEST(test_nest_is_reachable_only_through_the_door);
    RUN_TEST(test_two_tunnels_on_different_rows_wrap);
    RUN_TEST(test_named_tiles_sit_on_the_right_codes);
    RUN_TEST(test_calm_tiles_do_not_strand_bugs);

    RUN_TEST(test_tile_centres_and_round_trips);
    RUN_TEST(test_direction_deltas_and_opposites);
    RUN_TEST(test_placement_follows_tile_and_offset);
    RUN_TEST(test_tunnel_translation_lifts_the_pen);

    RUN_TEST(test_blocked_turn_stays_buffered);
    RUN_TEST(test_early_cornering_uses_the_nearest_centre);
    RUN_TEST(test_reverse_is_immediate);
    RUN_TEST(test_wall_stops_the_turtle_at_the_centre);
    RUN_TEST(test_sub_pixel_progress_carries_across_tiles);
    RUN_TEST(test_blocked_actor_discards_the_frame_step);

    RUN_TEST(test_painting_mutates_the_tile_and_scores);
    RUN_TEST(test_blossom_scores_pauses_and_turns_the_tables);
    RUN_TEST(test_dizzy_timer_pauses_the_schedule_then_restores);
    RUN_TEST(test_bonus_appears_at_the_gate_and_times_out);
    RUN_TEST(test_taking_the_bonus_scores_by_level);
    RUN_TEST(test_level_completes_when_no_tiles_remain);

    RUN_TEST(test_hunt_targets_in_every_direction);
    RUN_TEST(test_moss_switches_at_eight_tiles);
    RUN_TEST(test_patrol_targets_corners_until_dart_goes_frenzied);
    RUN_TEST(test_exit_choice_excludes_reverse_and_breaks_ties_in_order);
    RUN_TEST(test_bug_commits_the_exit_chosen_a_tile_earlier);
    RUN_TEST(test_mode_change_queues_one_reversal);

    RUN_TEST(test_release_counters_free_waiting_bugs);
    RUN_TEST(test_respawn_switches_to_the_global_release_counters);
    RUN_TEST(test_respawn_unpauses_the_bug_simulation);
    RUN_TEST(test_place_actors_resets_every_state_list);
    RUN_TEST(test_idle_timer_forces_one_bug_out);
    RUN_TEST(test_leaving_bug_aligns_with_the_door_then_rises);
    RUN_TEST(test_wings_return_regrow_and_leave);
    RUN_TEST(test_wings_home_from_every_tile_and_heading);
    RUN_TEST(test_a_regrown_bug_puts_its_body_back_on);

    RUN_TEST(test_collision_outcomes_by_bug_state);
    RUN_TEST(test_dizzy_chain_scores_200_400_800_1600);
    RUN_TEST(test_opposite_direction_tile_swap_collides);
    RUN_TEST(test_dizzy_bug_is_eaten_on_a_tile_swap);
    RUN_TEST(test_a_bug_following_the_turtle_does_not_collide);
    RUN_TEST(test_snap_tiles_records_the_pre_movement_tiles);
    RUN_TEST(test_extra_life_is_awarded_once);
    RUN_TEST(test_high_score_tracks_the_session_best);

    RUN_TEST(test_a_hedge_tile_runs_one_band_per_hedge_neighbour);
    RUN_TEST(test_the_corridor_is_wider_than_its_cell);
    RUN_TEST(test_the_bake_puts_the_board_where_the_pen_did);
    RUN_TEST(test_stamping_one_cell_repairs_exactly_that_cell);
    RUN_TEST(test_ready_puts_back_the_trail_it_covered);
    RUN_TEST(test_hud_stays_out_of_the_maze);
    RUN_TEST(test_the_hud_bands_erase_the_whole_stamp);
    RUN_TEST(test_the_hud_rows_clear_the_board_walls);
    RUN_TEST(test_frames_run_and_the_turtle_paints);
    RUN_TEST(test_respawn_keeps_painted_tiles);
    RUN_TEST(test_repeated_level_builds_keep_free_nodes_stable);
    RUN_TEST(test_the_frame_loop_reclaims_what_it_spends);
    RUN_TEST(test_state_machine_soak);
    RUN_TEST(test_level_profiles_escalate);
    RUN_TEST(test_speeds_are_sane_at_25_fps);
    RUN_TEST(test_p9m0_instrumentation_runs);
    RUN_TEST(test_p10prof_profiler_runs);

    return UNITY_END();
}
