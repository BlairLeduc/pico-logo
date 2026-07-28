// Tests for the pure-Logo Checkpoint Run game.
//
// The game's encoded map is its only source of truth: collision, AI and the
// drawn road view all read it. These tests therefore check the map against
// itself (reciprocity, connectivity, closed edge) and check the *drawing*
// against the map, so the picture and the logic cannot drift apart.
//
// They also execute every procedure in the file at least once. The parse
// hazards listed in docs/checkpoint-run-design.md section 12 are runtime
// errors that reading does not catch, and the previous suite passed over an
// implementation that could not run a single frame.
#include "test_scaffold.h"
#include "core/repl.h"
#include "core/error.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CHECKRUN_SOURCE
#error "CHECKRUN_SOURCE must be defined"
#endif

#define COLS 32
#define ROWS 40
#define NORTH 1
#define EAST  2
#define SOUTH 4
#define WEST  8

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

// Load the game exactly the way the `load` primitive does: join lines only
// between "to" and "end", and execute every other line on its own.
//
// This deliberately mirrors prim_load in core/primitives_files_load_save.c,
// including its 256-byte line limit. An earlier version of this loader also
// joined bracketed list literals across lines, which made the test harness
// more permissive than the device -- the game passed every test here and
// then failed on `load "games/checkrun` with "I don't know how to 6AAE...".
// If the harness accepts a file, `load` must accept it too.
#define TEST_LOAD_MAX_LINE 256

static void load_checkrun(void)
{
    FILE *f = fopen(CHECKRUN_SOURCE, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, CHECKRUN_SOURCE);
    char line[1024], buf[8192];
    size_t used = 0;
    bool in_def = false;

    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;

        // A line `load` could not read whole would be silently truncated.
        TEST_ASSERT_LESS_THAN_MESSAGE(TEST_LOAD_MAX_LINE, n, line);
        if (n == 0) continue;

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

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

// Read a numeric result. Values fetched out of a list are not necessarily
// stored as a raw float, so go through the printed form rather than reaching
// into the union.
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

// Read a whole world's masks out of Logo into C, so graph properties can be
// checked with a real traversal.
static void read_world(int world, int m[ROWS][COLS])
{
    runf("make \"cr.round %d init.round", world);
    for (int r = 0; r < ROWS; r++) {
        // Every row must still be 32 characters: a row of only digits would
        // have been read as a number and lost its leading zeros.
        char code[64];
        snprintf(code, sizeof(code), "count item %d :cr.map", r + 1);
        TEST_ASSERT_EQUAL_MESSAGE(COLS, (int)num(code), code);
        for (int c = 0; c < COLS; c++)
            m[r][c] = (int)numf("road.at %d %d", c, r);
    }
}

void setUp(void)
{
    test_scaffold_setUp_with_device_and_hardware();
    load_checkrun();
    run("init.game init.round");
}

void tearDown(void) { test_scaffold_tearDown(); }

// ---------------------------------------------------------------------------
// 1. Map invariants
// ---------------------------------------------------------------------------

static void check_world_invariants(int world)
{
    static int m[ROWS][COLS];
    read_world(world, m);

    int road = 0, dead = 0;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int v = m[r][c];
            if (!v) continue;
            road++;
            int exits = __builtin_popcount((unsigned)v);
            if (exits == 1) dead++;

            // Reciprocity: an exit must be matched from the other side.
            if (v & NORTH) {
                TEST_ASSERT_TRUE_MESSAGE(r > 0 && (m[r - 1][c] & SOUTH), "north exit unmatched");
            }
            if (v & EAST) {
                TEST_ASSERT_TRUE_MESSAGE(c < COLS - 1 && (m[r][c + 1] & WEST), "east exit unmatched");
            }
            if (v & SOUTH) {
                TEST_ASSERT_TRUE_MESSAGE(r < ROWS - 1 && (m[r + 1][c] & NORTH), "south exit unmatched");
            }
            if (v & WEST) {
                TEST_ASSERT_TRUE_MESSAGE(c > 0 && (m[r][c - 1] & EAST), "west exit unmatched");
            }

            // Closed outer boundary.
            if (r == 0) TEST_ASSERT_FALSE_MESSAGE(v & NORTH, "exit off the top edge");
            if (r == ROWS - 1) TEST_ASSERT_FALSE_MESSAGE(v & SOUTH, "exit off the bottom edge");
            if (c == 0) TEST_ASSERT_FALSE_MESSAGE(v & WEST, "exit off the left edge");
            if (c == COLS - 1) TEST_ASSERT_FALSE_MESSAGE(v & EAST, "exit off the right edge");
        }
    }
    TEST_ASSERT_GREATER_THAN_MESSAGE(400, road, "implausibly few road tiles");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(4, dead, "too many dead ends");

    // One connected component, by flood fill.
    static bool seen[ROWS][COLS];
    memset(seen, 0, sizeof(seen));
    static int stack[ROWS * COLS][2];
    int top = 0, reached = 0, sr = -1, sc = -1;
    for (int r = 0; r < ROWS && sr < 0; r++)
        for (int c = 0; c < COLS && sr < 0; c++)
            if (m[r][c]) { sr = r; sc = c; }
    stack[top][0] = sr; stack[top][1] = sc; top++;
    seen[sr][sc] = true;
    while (top) {
        top--;
        int r = stack[top][0], c = stack[top][1];
        reached++;
        int dr[4] = {-1, 0, 1, 0}, dc[4] = {0, 1, 0, -1};
        int bit[4] = {NORTH, EAST, SOUTH, WEST};
        for (int k = 0; k < 4; k++) {
            if (!(m[r][c] & bit[k])) continue;
            int nr = r + dr[k], nc = c + dc[k];
            if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) continue;
            if (seen[nr][nc]) continue;
            seen[nr][nc] = true;
            stack[top][0] = nr; stack[top][1] = nc; top++;
        }
    }
    TEST_ASSERT_EQUAL_MESSAGE(road, reached, "road network is not one component");

    // At least three routes across each half boundary.
    int vert = 0, horz = 0;
    for (int r = 0; r < ROWS; r++) if (m[r][15] & EAST) vert++;
    for (int c = 0; c < COLS; c++) if (m[19][c] & SOUTH) horz++;
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(3, vert, "too few vertical sector crossings");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(3, horz, "too few horizontal sector crossings");
}

void test_world_one_is_a_valid_road_graph(void) { check_world_invariants(1); }
void test_world_two_is_a_valid_road_graph(void) { check_world_invariants(2); }

void test_start_and_garages_are_legal(void)
{
    for (int world = 1; world <= 2; world++) {
        runf("make \"cr.round %d init.round", world);
        // The start is a road tile with at least two route choices.
        float sc = num("tile.col :cr.start"), sr = num("tile.row :cr.start");
        TEST_ASSERT_TRUE(numf("road.at %d %d", (int)sc, (int)sr) != 0);
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(2, (int)numf("exit.count %d %d", (int)sc, (int)sr),
                                             "start has fewer than two exits");
        // Garages are on road, and spread over at least two sectors.
        bool sector_seen[4] = {false, false, false, false};
        for (int i = 1; i <= 6; i++) {
            float t = numf("item %d :cr.garage", i);
            int c = (int)numf("tile.col %d", (int)t), r = (int)numf("tile.row %d", (int)t);
            TEST_ASSERT_TRUE_MESSAGE(numf("road.at %d %d", c, r) != 0, "garage off road");
            sector_seen[(int)numf("sector.of %d %d", c, r)] = true;
        }
        int n = 0;
        for (int s = 0; s < 4; s++) if (sector_seen[s]) n++;
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(2, n, "garages not spread across sectors");
    }
}

// ---------------------------------------------------------------------------
// 2. Coordinates and sectors
// ---------------------------------------------------------------------------

void test_tile_and_sector_coordinates(void)
{
    TEST_ASSERT_EQUAL_FLOAT(1, num("tile.index 0 0"));
    TEST_ASSERT_EQUAL_FLOAT(1280, num("tile.index 31 39"));
    TEST_ASSERT_EQUAL_FLOAT(31, num("tile.col 1280"));
    TEST_ASSERT_EQUAL_FLOAT(39, num("tile.row 1280"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("sector.of 0 0"));
    TEST_ASSERT_EQUAL_FLOAT(1, num("sector.of 31 0"));
    TEST_ASSERT_EQUAL_FLOAT(2, num("sector.of 0 39"));
    TEST_ASSERT_EQUAL_FLOAT(3, num("sector.of 31 39"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("sector.base.col 0"));
    TEST_ASSERT_EQUAL_FLOAT(16, num("sector.base.col 3"));
    TEST_ASSERT_EQUAL_FLOAT(20, num("sector.base.row 3"));
}

// The 16x20 tile grid must cover the 256x320 road view exactly, with no
// margin at either end -- this is what lets the drawing test compare stamp
// positions against the map directly.
void test_tiles_cover_the_road_view_exactly(void)
{
    TEST_ASSERT_EQUAL_FLOAT(-156, num("screen.x 0"));
    TEST_ASSERT_EQUAL_FLOAT(84, num("screen.x 15"));
    TEST_ASSERT_EQUAL_FLOAT(156, num("screen.y 0"));
    TEST_ASSERT_EQUAL_FLOAT(-148, num("screen.y 19"));
    // Left edge of column 0 and right edge of column 15, in pixels.
    TEST_ASSERT_EQUAL_FLOAT(0, num("screen.x 0") + 160 - 4);
    TEST_ASSERT_EQUAL_FLOAT(256, num("screen.x 15") + 160 + 12);
    // Top edge of row 0 and bottom edge of row 19.
    TEST_ASSERT_EQUAL_FLOAT(0, 160 - num("screen.y 0") - 4);
    TEST_ASSERT_EQUAL_FLOAT(320, 160 - num("screen.y 19") + 12);
}

void test_directions_and_masks(void)
{
    TEST_ASSERT_EQUAL_FLOAT(1, num("dir.bit 0"));
    TEST_ASSERT_EQUAL_FLOAT(2, num("dir.bit 1"));
    TEST_ASSERT_EQUAL_FLOAT(4, num("dir.bit 2"));
    TEST_ASSERT_EQUAL_FLOAT(8, num("dir.bit 3"));
    TEST_ASSERT_EQUAL_FLOAT(2, num("opposite 0"));
    TEST_ASSERT_EQUAL_FLOAT(3, num("opposite 1"));
    TEST_ASSERT_EQUAL_FLOAT(15, num("hex.value \"F"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("hex.value \"0"));
    TEST_ASSERT_EQUAL_FLOAT(10, num("hex.value \"A"));
    // Off-map tiles are never road, so the world edge stops a car.
    TEST_ASSERT_EQUAL_FLOAT(0, num("road.at -1 5"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("road.at 32 5"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("road.at 5 -1"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("road.at 5 40"));
}

// ---------------------------------------------------------------------------
// 3. The drawing agrees with the map
// ---------------------------------------------------------------------------

// draw.sector must stamp a road block at every road tile of the sector, at
// the coordinates section 2 specifies, and nowhere else. This is the check
// whose absence let the previous version ship a maze picture that had nothing
// to do with its map.
void test_drawn_sector_matches_the_map(void)
{
    run("setup.palette setup.shapes setup.turtles");

    for (int sector = 0; sector < 4; sector++) {
        int base_c = (sector % 2) * 16, base_r = (sector / 2) * 20;
        runf("make \"cr.sector %d", sector);
        mock_device_clear_graphics();
        run("draw.sector");

        // Count road-block stamps landing on each tile of the sector.
        static int hits[20][16];
        memset(hits, 0, sizeof(hits));
        int block_shape = (int)num(":sh.block");
        int road_colour = (int)num(":cr.road.c");

        int total = mock_device_stamp_count();
        TEST_ASSERT_GREATER_THAN_MESSAGE(0, total, "draw.sector stamped nothing");

        for (int i = 0; i < total; i++) {
            const MockStamp *s = mock_device_get_stamp(i);
            if (s->shape != block_shape || s->colour != road_colour) continue;
            TEST_ASSERT_EQUAL_MESSAGE(7, s->turtle, "road drawn by a turtle other than 7");

            // Each 16x16 tile is two 8x16 stamps, at centre -/+ 4.
            float px = s->x + 160.0f, py = 160.0f - s->y;
            TEST_ASSERT_TRUE_MESSAGE(px >= 0 && px < 256, "road stamp outside the road view");
            // A tile's two half-stamps sit at pixel 16*lc and 16*lc+8, and
            // both halves share the row centre at pixel 16*lr+4.
            int lc = (int)(px / 16.0f);
            int lr = (int)((py - 4) / 16.0f);
            TEST_ASSERT_TRUE(lc >= 0 && lc < 16 && lr >= 0 && lr < 20);
            // Confirm it sits on one of the two half positions of that tile.
            float cx = numf("screen.x %d", lc) + 160.0f;
            TEST_ASSERT_TRUE_MESSAGE(px == cx - 4 || px == cx + 4, "stamp not on a tile half");
            hits[lr][lc]++;
        }

        for (int lr = 0; lr < 20; lr++) {
            for (int lc = 0; lc < 16; lc++) {
                bool is_road = numf("road.at %d %d", base_c + lc, base_r + lr) != 0;
                char msg[96];
                snprintf(msg, sizeof(msg), "sector %d tile %d,%d road=%d stamps=%d",
                         sector, lc, lr, (int)is_road, hits[lr][lc]);
                TEST_ASSERT_EQUAL_MESSAGE(is_road ? 2 : 0, hits[lr][lc], msg);
            }
        }
    }
}

void test_drawing_stays_out_of_the_instrument_column(void)
{
    run("setup.palette setup.shapes setup.turtles make \"cr.sector 0");
    mock_device_clear_graphics();
    run("draw.sector");
    int block_shape = (int)num(":sh.block");
    int road_colour = (int)num(":cr.road.c");
    for (int i = 0; i < mock_device_stamp_count(); i++) {
        const MockStamp *s = mock_device_get_stamp(i);
        if (s->shape != block_shape || s->colour != road_colour) continue;
        TEST_ASSERT_TRUE_MESSAGE(s->x + 160.0f < 256.0f, "road stamped over the panel");
    }
}

// ---------------------------------------------------------------------------
// 4. Player driving
// ---------------------------------------------------------------------------

// Put the player on a known four-way junction so turns are unambiguous.
static void player_at_junction(void)
{
    run("make \"cr.round 1 init.round");
    float t = num(":cr.start");
    runf(".setitem 1 :car.col tile.col %d", (int)t);
    runf(".setitem 1 :car.row tile.row %d", (int)t);
    run(".setitem 1 :car.dir 1 .setitem 1 :car.offset 0 make \"cr.buffer -1");
    TEST_ASSERT_EQUAL_FLOAT(15, num("road.at (item 1 :car.col) (item 1 :car.row)"));
}

void test_player_travels_and_carries_the_remainder(void)
{
    player_at_junction();
    float c0 = num("item 1 :car.col");
    run("advance.car 1 60");
    TEST_ASSERT_EQUAL_FLOAT(2.4f, num("item 1 :car.offset"));
    run("advance.car 1 60 advance.car 1 60");
    TEST_ASSERT_EQUAL_FLOAT(7.2f, num("item 1 :car.offset"));
    // The fourth step crosses into the next tile, carrying the remainder.
    run("advance.car 1 60");
    TEST_ASSERT_EQUAL_FLOAT(c0 + 1, num("item 1 :car.col"));
    TEST_ASSERT_EQUAL_FLOAT(9.6f - 16.0f, num("item 1 :car.offset"));
}

void test_reverse_is_immediate_and_negates_progress(void)
{
    player_at_junction();
    run("advance.car 1 60");                 // offset 2.4, heading east
    run("make \"cr.buffer 3 player.turn");   // request west
    TEST_ASSERT_EQUAL_FLOAT(3, num("item 1 :car.dir"));
    TEST_ASSERT_EQUAL_FLOAT(-2.4f, num("item 1 :car.offset"));
    TEST_ASSERT_EQUAL_FLOAT(-1, num(":cr.buffer"));
}

// A perpendicular turn inside the four-pixel window must keep the travel it
// has already made, not discard it.
void test_perpendicular_turn_carries_unused_movement(void)
{
    player_at_junction();
    run("advance.car 1 60");                 // offset 2.4
    run("make \"cr.buffer 2 player.turn");   // request south
    TEST_ASSERT_EQUAL_FLOAT(2, num("item 1 :car.dir"));
    TEST_ASSERT_EQUAL_FLOAT(2.4f, num("item 1 :car.offset"));
}

void test_perpendicular_turn_waits_outside_the_window(void)
{
    player_at_junction();
    run("advance.car 1 60 advance.car 1 60 advance.car 1 60");  // offset 7.2
    run("make \"cr.buffer 2 player.turn");
    TEST_ASSERT_EQUAL_FLOAT(1, num("item 1 :car.dir"));   // still east
    TEST_ASSERT_EQUAL_FLOAT(2, num(":cr.buffer"));        // request retained
}

// The map, not the screen edge, decides where a car may go.
void test_no_turn_through_a_building(void)
{
    run("make \"cr.round 1 init.round");
    // Find a road tile with no north exit and stand on it.
    int fc = -1, fr = -1;
    for (int r = 1; r < ROWS - 1 && fc < 0; r++)
        for (int c = 1; c < COLS - 1 && fc < 0; c++) {
            int m = (int)numf("road.at %d %d", c, r);
            if (m && !(m & NORTH)) { fc = c; fr = r; }
        }
    TEST_ASSERT_TRUE_MESSAGE(fc >= 0, "no tile without a north exit");
    runf(".setitem 1 :car.col %d .setitem 1 :car.row %d", fc, fr);
    run(".setitem 1 :car.dir 1 .setitem 1 :car.offset 0");
    truth("road.open? (item 1 :car.col) (item 1 :car.row) 0", "false");
    run("make \"cr.buffer 0 player.turn");
    TEST_ASSERT_EQUAL_FLOAT(1, num("item 1 :car.dir"));   // refused
}

void test_car_stops_at_a_road_end(void)
{
    run("make \"cr.round 1 init.round");
    int fc = -1, fr = -1;
    for (int r = 1; r < ROWS - 1 && fc < 0; r++)
        for (int c = 1; c < COLS - 1 && fc < 0; c++) {
            int m = (int)numf("road.at %d %d", c, r);
            if (m && !(m & EAST)) { fc = c; fr = r; }
        }
    TEST_ASSERT_TRUE(fc >= 0);
    runf(".setitem 1 :car.col %d .setitem 1 :car.row %d", fc, fr);
    run(".setitem 1 :car.dir 1 .setitem 1 :car.offset 0");
    run("advance.car 1 60 advance.car 1 60");
    TEST_ASSERT_EQUAL_FLOAT(fc, num("item 1 :car.col"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("item 1 :car.offset"));
}

// ---------------------------------------------------------------------------
// 5. Checkpoints, scoring and rounds
// ---------------------------------------------------------------------------

void test_checkpoints_are_ten_distinct_spread_tiles(void)
{
    for (int round = 1; round <= 8; round++) {
        runf("make \"cr.round %d init.round", round);
        TEST_ASSERT_EQUAL_MESSAGE(10, (int)num("count :flag.tile"), "not ten checkpoints");
        int seen_sector[4] = {0, 0, 0, 0};
        float tiles[10];
        for (int i = 1; i <= 10; i++) {
            tiles[i - 1] = numf("item %d :flag.tile", i);
            for (int j = 0; j < i - 1; j++)
                TEST_ASSERT_TRUE_MESSAGE(tiles[j] != tiles[i - 1], "duplicate checkpoint");
            int c = (int)numf("tile.col %d", (int)tiles[i - 1]);
            int r = (int)numf("tile.row %d", (int)tiles[i - 1]);
            TEST_ASSERT_TRUE_MESSAGE(numf("road.at %d %d", c, r) != 0, "checkpoint off road");
            seen_sector[(int)numf("sector.of %d %d", c, r)]++;
        }
        for (int s = 0; s < 4; s++) {
            char msg[64];
            snprintf(msg, sizeof(msg), "round %d sector %d has %d checkpoints", round, s, seen_sector[s]);
            TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(2, seen_sector[s], msg);
        }
    }
}

void test_checkpoint_values_rise_with_collection_order(void)
{
    run("make \"cr.round 1 init.round make \"cr.turbo.i 99");
    run("take.checkpoint 1");
    TEST_ASSERT_EQUAL_FLOAT(100, num(":cr.score"));
    run("take.checkpoint 2");
    TEST_ASSERT_EQUAL_FLOAT(300, num(":cr.score"));   // +200
    run("take.checkpoint 3");
    TEST_ASSERT_EQUAL_FLOAT(600, num(":cr.score"));   // +300
}

// The Turbo checkpoint scores at the multiplier in force when it is taken,
// then raises it: it neither doubles itself nor anything collected earlier.
void test_turbo_multiplier_applies_only_afterwards(void)
{
    run("make \"cr.round 1 init.round make \"cr.turbo.i 2");
    run("take.checkpoint 1");
    TEST_ASSERT_EQUAL_FLOAT(100, num(":cr.score"));
    run("take.checkpoint 2");                          // the Turbo one
    TEST_ASSERT_EQUAL_FLOAT(300, num(":cr.score"));    // +200, not +400
    TEST_ASSERT_EQUAL_FLOAT(2, num(":cr.mult"));
    run("take.checkpoint 3");
    TEST_ASSERT_EQUAL_FLOAT(900, num(":cr.score"));    // +300*2
}

void test_round_clear_awards_the_fuel_bonus(void)
{
    run("make \"cr.round 1 init.round make \"cr.score 0 make \"cr.fuel 100 make \"cr.collected 10");
    run("finish.round");
    TEST_ASSERT_EQUAL_FLOAT(1000, num(":cr.score"));
    TEST_ASSERT_EQUAL_FLOAT(2, num(":cr.round"));
}

void test_extra_life_is_awarded_once(void)
{
    run("init.game make \"cr.score 25000");
    run("award.extra.life");
    TEST_ASSERT_EQUAL_FLOAT(4, num(":cr.lives"));
    run("award.extra.life award.extra.life");
    TEST_ASSERT_EQUAL_FLOAT(4, num(":cr.lives"));   // latched
}

void test_challenge_rounds_are_every_fourth_from_three(void)
{
    const int challenge[] = {3, 7, 11};
    const int normal[] = {1, 2, 4, 5, 6, 8, 9, 10};
    for (size_t i = 0; i < sizeof(challenge) / sizeof(*challenge); i++) {
        runf("make \"cr.round %d set.round.mode", challenge[i]);
        truth(":cr.challenge", "true");
    }
    for (size_t i = 0; i < sizeof(normal) / sizeof(*normal); i++) {
        runf("make \"cr.round %d set.round.mode", normal[i]);
        truth(":cr.challenge", "false");
    }
}

void test_challenge_round_scores_a_flat_bonus_and_no_fuel(void)
{
    run("make \"cr.round 3 init.round make \"cr.score 0 make \"cr.fuel 900 make \"cr.collected 10");
    run("finish.round");
    TEST_ASSERT_EQUAL_FLOAT(5000, num(":cr.score"));
}

// ---------------------------------------------------------------------------
// 6. Fuel and smoke
// ---------------------------------------------------------------------------

void test_fuel_drains_and_clamps_at_zero(void)
{
    run("make \"cr.fuel 3");
    run("step.fuel step.fuel step.fuel step.fuel step.fuel");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":cr.fuel"));
    TEST_ASSERT_EQUAL_FLOAT(30, num("player.speed"));   // slowed, not stopped
}

// A smoke release must never drive fuel negative: if it did, the per-frame
// drain would stop and the zero-fuel slowdown would never trigger.
void test_smoke_cost_cannot_make_fuel_negative(void)
{
    run("make \"cr.fuel 40");
    run("spend.fuel 60");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":cr.fuel"));
    run("step.fuel");
    TEST_ASSERT_EQUAL_FLOAT(0, num(":cr.fuel"));
}

void test_smoke_requires_fuel_and_respects_cooldown(void)
{
    player_at_junction();
    run("make \"cr.frame 100 make \"cr.last.smoke -100 make \"cr.smoke.request \"true");
    run("maybe.smoke");
    TEST_ASSERT_EQUAL_FLOAT(1440, num(":cr.fuel"));
    // Too soon: the cooldown blocks a second release.
    run("make \"cr.smoke.request \"true maybe.smoke");
    TEST_ASSERT_EQUAL_FLOAT(1440, num(":cr.fuel"));
    // After the cooldown it is allowed again.
    run("make \"cr.frame 120 make \"cr.smoke.request \"true maybe.smoke");
    TEST_ASSERT_EQUAL_FLOAT(1380, num(":cr.fuel"));
    // Not enough fuel: refused outright.
    run("make \"cr.fuel 10 make \"cr.frame 200 make \"cr.smoke.request \"true maybe.smoke");
    TEST_ASSERT_EQUAL_FLOAT(10, num(":cr.fuel"));
}

void test_four_smoke_clouds_then_full(void)
{
    player_at_junction();
    run("make \"cr.fuel 1500 make \"cr.frame 0");
    for (int i = 0; i < 4; i++)
        runf("make \"cr.frame %d make \"cr.smoke.request \"true maybe.smoke", i * 10);
    TEST_ASSERT_EQUAL_FLOAT(0, num("free.smoke.slot"));
    // Once the first cloud expires its slot is reusable.
    run("make \"cr.frame 45");
    TEST_ASSERT_EQUAL_FLOAT(1, num("free.smoke.slot"));
}

// Smoke must spin an enemy and then let it recover: without the transition
// out of spin, smoke would delete enemies permanently.
void test_smoke_spins_then_releases_an_enemy(void)
{
    run("make \"cr.round 1 init.round make \"cr.frame 0");
    // Park enemy 2 on a smoke cloud.
    run(".setitem 1 :smoke.tile car.tile 2 .setitem 1 :smoke.until 40");
    run(".setitem 2 :car.state 0 .setitem 2 :car.release 0");
    run("check.smoke.hits");
    TEST_ASSERT_EQUAL_FLOAT(1, num("item 2 :car.state"));
    TEST_ASSERT_GREATER_THAN(0, num("item 2 :car.timer"));
    // A spinning enemy is harmless and does not move.
    run(".setitem 1 :car.col item 2 :car.col .setitem 1 :car.row item 2 :car.row");
    run(".setitem 1 :car.offset 0 .setitem 2 :car.offset 0 make \"cr.crash \"false");
    run("check.car.hits");
    truth(":cr.crash", "false");
    // Run the spin down: state goes 1 -> 2 -> 0.
    for (int i = 0; i < 25; i++) run("step.one.enemy 2");
    TEST_ASSERT_EQUAL_FLOAT(2, num("item 2 :car.state"));
    for (int i = 0; i < 12; i++) run("step.one.enemy 2");
    TEST_ASSERT_EQUAL_FLOAT(0, num("item 2 :car.state"));
}

// ---------------------------------------------------------------------------
// 7. Enemy AI
// ---------------------------------------------------------------------------

void test_enemy_kinds_target_different_things(void)
{
    run("make \"cr.round 1 init.round");
    run(".setitem 1 :car.col 8 .setitem 1 :car.row 10 .setitem 1 :car.dir 1");
    // Hunter aims at the player's tile.
    TEST_ASSERT_EQUAL_FLOAT(0, num("enemy.kind 2"));
    TEST_ASSERT_EQUAL_FLOAT(8, num("target.col 2"));
    TEST_ASSERT_EQUAL_FLOAT(10, num("target.row 2"));
    // Interceptor aims three steps ahead along the player's heading.
    TEST_ASSERT_EQUAL_FLOAT(1, num("enemy.kind 3"));
    TEST_ASSERT_EQUAL_FLOAT(11, num("target.col 3"));
    TEST_ASSERT_EQUAL_FLOAT(10, num("target.row 3"));
    // Collector aims at the checkpoint nearest the player.
    TEST_ASSERT_EQUAL_FLOAT(2, num("enemy.kind 4"));
    int want = (int)num("nearest.flag.tile");
    TEST_ASSERT_EQUAL_FLOAT(numf("tile.col %d", want), num("target.col 4"));
    TEST_ASSERT_EQUAL_FLOAT(numf("tile.row %d", want), num("target.row 4"));
    // That checkpoint really is the closest live one to the player.
    float best = numf("tile.dist2 1 %d", want);
    for (int i = 1; i <= 10; i++) {
        if (numf("item %d :flag.alive", i) != 1) continue;
        float t = numf("item %d :flag.tile", i);
        TEST_ASSERT_TRUE_MESSAGE(numf("tile.dist2 1 %d", (int)t) >= best, "not the nearest checkpoint");
    }
}

// nearest.flag.tile is called from inside choose.enemy.dir's own loop. If it
// wrote shared globals -- as the withdrawn version did -- it would corrupt
// the caller's loop counter and best-so-far.
void test_target_lookup_does_not_disturb_its_caller(void)
{
    run("make \"cr.round 1 init.round");
    // Stand the collector on the start junction, which is known to be road.
    run(".setitem 4 :car.col tile.col :cr.start .setitem 4 :car.row tile.row :cr.start");
    run(".setitem 4 :car.dir 1 .setitem 4 :car.offset 0");
    run("choose.enemy.dir 4");
    float d = num("item 4 :car.dir");
    TEST_ASSERT_TRUE_MESSAGE(d >= 0 && d <= 3, "collector chose an invalid direction");
    truth("road.open? (item 4 :car.col) (item 4 :car.row) (item 4 :car.dir)", "true");
}

void test_enemy_avoids_reversing_unless_cornered(void)
{
    run("make \"cr.round 1 init.round");
    // Stand a hunter on a four-way junction heading east; it must not pick west.
    float t = num(":cr.start");
    runf(".setitem 2 :car.col tile.col %d .setitem 2 :car.row tile.row %d", (int)t, (int)t);
    run(".setitem 2 :car.dir 1 .setitem 2 :car.offset 0");
    run(".setitem 1 :car.col 0 .setitem 1 :car.row 0");   // target far to the north-west
    run("choose.enemy.dir 2");
    TEST_ASSERT_TRUE_MESSAGE(num("item 2 :car.dir") != 3, "enemy reversed at a junction");
}

void test_enemy_population_and_speed_follow_the_round(void)
{
    const int round[]  = {1, 2, 3, 4, 5, 6, 9};
    const int count[]  = {3, 4, 4, 5, 5, 6, 6};
    const int speed[]  = {50, 52, 52, 54, 56, 58, 58};
    for (size_t i = 0; i < sizeof(round) / sizeof(*round); i++) {
        runf("make \"cr.round %d", round[i]);
        TEST_ASSERT_EQUAL_MESSAGE(count[i], (int)num("enemy.count"), "wrong enemy count");
        TEST_ASSERT_EQUAL_MESSAGE(speed[i], (int)num("enemy.speed"), "wrong enemy speed");
    }
}

// In a challenge round the enemies stay in their garages until the fuel runs
// out, then all release together.
void test_challenge_enemies_wait_for_empty_fuel(void)
{
    run("make \"cr.round 3 init.round");
    run("make \"cr.fuel 500 make \"cr.frame 9999");
    truth("enemy.released? 2", "false");
    run("make \"cr.fuel 0");
    truth("enemy.released? 2", "true");
    truth("enemy.released? 7", "true");
}

// ---------------------------------------------------------------------------
// 8. Collisions and round/game state
// ---------------------------------------------------------------------------

void test_player_crashes_into_an_active_enemy(void)
{
    run("make \"cr.round 1 init.round make \"cr.frame 9999 make \"cr.crash \"false");
    run(".setitem 2 :car.state 0");
    run(".setitem 1 :car.col item 2 :car.col .setitem 1 :car.row item 2 :car.row");
    run(".setitem 1 :car.offset 0 .setitem 2 :car.offset 0");
    run("check.car.hits");
    truth(":cr.crash", "true");
}

void test_player_crashes_into_a_rock(void)
{
    run("make \"cr.round 1 init.round make \"cr.crash \"false");
    run(".setitem 1 :car.col tile.col item 1 :rock.tile");
    run(".setitem 1 :car.row tile.row item 1 :rock.tile");
    run(".setitem 1 :car.offset 0");
    run("check.rocks");
    truth(":cr.crash", "true");
}

void test_enemy_hitting_a_rock_spins_instead_of_dying(void)
{
    run("make \"cr.round 1 init.round");
    run(".setitem 2 :car.state 0 .setitem 2 :car.offset 0");
    run(".setitem 2 :car.col tile.col item 1 :rock.tile");
    run(".setitem 2 :car.row tile.row item 1 :rock.tile");
    run("check.enemy.rocks");
    TEST_ASSERT_EQUAL_FLOAT(1, num("item 2 :car.state"));
}

// Score, lives and round belong to the game, not the round: the withdrawn
// version reset them in per-round setup, so no progression could persist.
void test_score_and_lives_survive_a_round_boundary(void)
{
    run("init.game");
    run("make \"cr.score 4321 make \"cr.lives 2 make \"cr.round 4");
    run("init.round");
    TEST_ASSERT_EQUAL_FLOAT(4321, num(":cr.score"));
    TEST_ASSERT_EQUAL_FLOAT(2, num(":cr.lives"));
    TEST_ASSERT_EQUAL_FLOAT(4, num(":cr.round"));
    // Per-round state is reset, though.
    TEST_ASSERT_EQUAL_FLOAT(0, num(":cr.collected"));
    TEST_ASSERT_EQUAL_FLOAT(1500, num(":cr.fuel"));
    TEST_ASSERT_EQUAL_FLOAT(1, num(":cr.mult"));
}

void test_collected_checkpoints_survive_a_sector_redraw(void)
{
    run("make \"cr.round 1 init.round setup.palette setup.shapes setup.turtles");
    run("make \"cr.turbo.i 99 take.checkpoint 1 take.checkpoint 2");
    TEST_ASSERT_EQUAL_FLOAT(0, num("item 1 :flag.alive"));
    run("enter.sector 1 enter.sector 0");
    TEST_ASSERT_EQUAL_FLOAT(0, num("item 1 :flag.alive"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("item 2 :flag.alive"));
    TEST_ASSERT_EQUAL_FLOAT(2, num(":cr.collected"));
}

void test_normal_crash_costs_a_life_but_challenge_crash_does_not(void)
{
    run("make \"cr.round 1 init.round setup.palette setup.shapes setup.turtles");
    run("make \"cr.lives 3");
    run("handle.crash");
    TEST_ASSERT_EQUAL_FLOAT(2, num(":cr.lives"));
    truth(":cr.ended", "false");

    run("make \"cr.round 3 init.round setup.palette setup.shapes setup.turtles");
    run("make \"cr.lives 3");
    run("handle.crash");
    TEST_ASSERT_EQUAL_FLOAT(3, num(":cr.lives"));
    truth(":cr.ended", "true");
}

void test_respawn_returns_cars_to_their_garages(void)
{
    run("make \"cr.round 1 init.round setup.palette setup.shapes setup.turtles");
    run(".setitem 2 :car.col 3 .setitem 2 :car.row 3 .setitem 2 :car.state 1");
    run("respawn");
    TEST_ASSERT_EQUAL_FLOAT(num("tile.col item 2 :car.home"), num("item 2 :car.col"));
    TEST_ASSERT_EQUAL_FLOAT(num("tile.row item 2 :car.home"), num("item 2 :car.row"));
    TEST_ASSERT_EQUAL_FLOAT(0, num("item 2 :car.state"));
    TEST_ASSERT_EQUAL_FLOAT(num("tile.col :cr.start"), num("item 1 :car.col"));
    TEST_ASSERT_EQUAL_FLOAT(1500, num(":cr.fuel"));
}

// ---------------------------------------------------------------------------
// 9. Every procedure runs
// ---------------------------------------------------------------------------

// The parse hazards in design section 12 are runtime errors. A frame that
// never executes a procedure cannot reveal them, so drive the whole loop.
void test_a_full_frame_runs_without_error(void)
{
    run("make \"cr.round 1 init.round setup.palette setup.shapes setup.turtles");
    run("enter.sector sector.of (item 1 :car.col) (item 1 :car.row)");
    run("make \"cr.smoke.request \"true");
    for (int i = 0; i < 30; i++)
        run("play.frame");
    TEST_ASSERT_EQUAL_FLOAT(30, num(":cr.frame"));
}

void test_a_frame_runs_in_every_sector(void)
{
    run("make \"cr.round 1 init.round setup.palette setup.shapes setup.turtles");
    for (int s = 0; s < 4; s++) {
        runf("enter.sector %d", s);
        run("play.frame play.frame");
    }
}

void test_paused_frame_does_not_advance_the_simulation(void)
{
    run("make \"cr.round 1 init.round setup.palette setup.shapes setup.turtles enter.sector 0");
    run("play.frame");
    float frame = num(":cr.frame"), fuel = num(":cr.fuel");
    run("toggle.pause play.frame play.frame");
    TEST_ASSERT_EQUAL_FLOAT(frame, num(":cr.frame"));
    TEST_ASSERT_EQUAL_FLOAT(fuel, num(":cr.fuel"));
    run("toggle.pause play.frame");
    TEST_ASSERT_EQUAL_FLOAT(frame + 1, num(":cr.frame"));
}

void test_remaining_procedures_execute(void)
{
    run("make \"cr.round 1 init.round setup.palette setup.shapes setup.turtles enter.sector 0");
    run("draw.panel draw.hud draw.fuel.gauge draw.message [HI]");
    run("draw.flags draw.rocks draw.smoke draw.radar.all draw.changed.radar");
    run("restore.sector update.visible.turtles show.car 1");
    run("repaint.tile car.tile 1 redraw.tile.contents car.tile 1 draw.one.flag 1");
    run("stamp.tile car.tile 1 :sh.rock :cr.rock.c");
    run("mark.radar 5 5 :cr.flag.c hud.field 150 0 1");
    run("collect.checkpoint check.smoke.hits check.one.enemy.smoke 2");
    run("step.enemies step.smoke step.player update.engine.sound engine.sound \"true");
    run("setup.sound crash.animation");
    run("make \"cr.quit \"true");
    truth("round.over?", "true");
    truth("game.over?", "true");
    TEST_ASSERT_TRUE(num("time.sector") >= 0);
    TEST_ASSERT_TRUE(num("dist2 1 2") >= 0);
    TEST_ASSERT_TRUE(num("tile.dist2 1 car.tile 1") >= 0);
    TEST_ASSERT_TRUE(num("flag.value") >= 0);
    TEST_ASSERT_TRUE(num("flag.radar.colour 1") > 0);
    TEST_ASSERT_TRUE(num("car.radar.colour 1") > 0);
    TEST_ASSERT_TRUE(num("radar.x 0") >= 96);
    TEST_ASSERT_TRUE(num("exit.score 2 1 5 5") >= 0);
    truth("tile.in.sector? car.tile 1", "true");
    truth("road? 0 0", "true");
}

// The radar must stay inside the 64-pixel instrument column: world column 31
// used to land off the right edge of the screen.
void test_radar_fits_the_instrument_column(void)
{
    TEST_ASSERT_EQUAL_FLOAT(96, num("radar.x 0"));
    TEST_ASSERT_EQUAL_FLOAT(158, num("radar.x 31"));
    TEST_ASSERT_TRUE_MESSAGE(num("radar.x 31") + 1 <= 159, "radar runs off the screen");
    TEST_ASSERT_EQUAL_FLOAT(92, num("radar.y 0"));
    TEST_ASSERT_EQUAL_FLOAT(14, num("radar.y 39"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_world_one_is_a_valid_road_graph);
    RUN_TEST(test_world_two_is_a_valid_road_graph);
    RUN_TEST(test_start_and_garages_are_legal);
    RUN_TEST(test_tile_and_sector_coordinates);
    RUN_TEST(test_tiles_cover_the_road_view_exactly);
    RUN_TEST(test_directions_and_masks);
    RUN_TEST(test_drawn_sector_matches_the_map);
    RUN_TEST(test_drawing_stays_out_of_the_instrument_column);
    RUN_TEST(test_player_travels_and_carries_the_remainder);
    RUN_TEST(test_reverse_is_immediate_and_negates_progress);
    RUN_TEST(test_perpendicular_turn_carries_unused_movement);
    RUN_TEST(test_perpendicular_turn_waits_outside_the_window);
    RUN_TEST(test_no_turn_through_a_building);
    RUN_TEST(test_car_stops_at_a_road_end);
    RUN_TEST(test_checkpoints_are_ten_distinct_spread_tiles);
    RUN_TEST(test_checkpoint_values_rise_with_collection_order);
    RUN_TEST(test_turbo_multiplier_applies_only_afterwards);
    RUN_TEST(test_round_clear_awards_the_fuel_bonus);
    RUN_TEST(test_extra_life_is_awarded_once);
    RUN_TEST(test_challenge_rounds_are_every_fourth_from_three);
    RUN_TEST(test_challenge_round_scores_a_flat_bonus_and_no_fuel);
    RUN_TEST(test_fuel_drains_and_clamps_at_zero);
    RUN_TEST(test_smoke_cost_cannot_make_fuel_negative);
    RUN_TEST(test_smoke_requires_fuel_and_respects_cooldown);
    RUN_TEST(test_four_smoke_clouds_then_full);
    RUN_TEST(test_smoke_spins_then_releases_an_enemy);
    RUN_TEST(test_enemy_kinds_target_different_things);
    RUN_TEST(test_target_lookup_does_not_disturb_its_caller);
    RUN_TEST(test_enemy_avoids_reversing_unless_cornered);
    RUN_TEST(test_enemy_population_and_speed_follow_the_round);
    RUN_TEST(test_challenge_enemies_wait_for_empty_fuel);
    RUN_TEST(test_player_crashes_into_an_active_enemy);
    RUN_TEST(test_player_crashes_into_a_rock);
    RUN_TEST(test_enemy_hitting_a_rock_spins_instead_of_dying);
    RUN_TEST(test_score_and_lives_survive_a_round_boundary);
    RUN_TEST(test_collected_checkpoints_survive_a_sector_redraw);
    RUN_TEST(test_normal_crash_costs_a_life_but_challenge_crash_does_not);
    RUN_TEST(test_respawn_returns_cars_to_their_garages);
    RUN_TEST(test_a_full_frame_runs_without_error);
    RUN_TEST(test_a_frame_runs_in_every_sector);
    RUN_TEST(test_paused_frame_does_not_advance_the_simulation);
    RUN_TEST(test_remaining_procedures_execute);
    RUN_TEST(test_radar_fits_the_instrument_column);
    return UNITY_END();
}
