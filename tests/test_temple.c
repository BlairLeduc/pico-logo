// Tests for the pure-Logo Snake Temple game (logo/games/temple), a port of
// RAX's 2022 BASIC 10Liner for the Oric Atmos.
//
// The tile map is the game's only source of truth: walls, floor, flasks, the
// chest and the snakes are all map cells, and movement, pickups, bites and
// the picture read those same cells. These tests therefore check the carved
// labyrinth as a graph -- with a real flood fill in C, not by trusting the
// game's own helpers -- and then check that every rule that reads or writes a
// cell leaves the map in a state the next rule can believe.
//
// The temple is dark, and darkness is the one thing the map cannot show: an
// unrevealed cell and a revealed one hold the same slot, and differ only in
// what reached the canvas. Those tests therefore assert on pixels, against a
// bank staged with flat colours (the mock does not rasterise the pen, so the
// game's own art is indistinguishable on it).
//
// They also execute every rule procedure at least once. The parse hazards
// listed at the top of logo/games/temple are runtime errors that reading does
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

#ifndef TEMPLE_SOURCE
#error "TEMPLE_SOURCE must be defined"
#endif

// The map is the whole 320x320 screen in 8 px cells. The labyrinth fills
// columns 1..39 and rows 1..35; rows 36..40 are the HUD.
#define MAP_COLS 40
#define MAP_ROWS 40
#define MAZE_COLS 39
#define MAZE_ROWS 35

// Chambers sit at even map coordinates: (2i, 2j) for i in 1..19, j in 1..17.
#define CHAMBERS_X 19
#define CHAMBERS_Y 17

// Bank slots, as laid out at the top of logo/games/temple. Everything a
// walker may stand on is above S_BOCCO -- a snake included, because paying
// its toll is a move the player is allowed to make.
#define S_NOTHING 0
#define S_WALL    1
#define S_HEART   2
#define S_BOCCO   3
#define S_FLOOR   4
#define S_FLASK   5
#define S_CHEST   6
#define S_SNAKE   7

#define SNAKES 20
#define FLASKS 10
#define MAX_HEALTH 10

// The longest line `load` can read whole; a longer one would be truncated in
// silence, so the loader below rejects it instead.
#define TEST_LOAD_MAX_LINE 256

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

// Same hygiene as the Turtle Trails loader: a line `load` could not read
// whole would be silently truncated, and a `;` inside a bracketed list
// silently swallows the rest of a procedure. Both are rejected here rather
// than left to show up as a missing tail at run time.
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

        TEST_ASSERT_LESS_THAN_MESSAGE(TEST_LOAD_MAX_LINE, n, line);
        if (n == 0) continue;

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

// The map the game actually plays from, 1-based, as the interpreter sees it.
static int slot_at(int col, int row)
{
    return (int)numf("tile %d %d", col, row);
}

static void read_map(int m[MAP_ROWS + 1][MAP_COLS + 1])
{
    for (int r = 1; r <= MAP_ROWS; r++)
        for (int c = 1; c <= MAP_COLS; c++)
            m[r][c] = slot_at(c, r);
}

static bool walkable(int slot) { return slot > S_BOCCO; }

static const int DC[5] = {0, 0, -1, 0, 1};
static const int DR[5] = {0, -1, 0, 1, 0};

static void put_bocco(int col, int row)
{
    runf("make \"st.col %d  make \"st.row %d", col, row);
}

// ---------------------------------------------------------------------------
// Seeing what reached the screen
// ---------------------------------------------------------------------------

// A slot's staged colour. Flat and distinguishable, so one pixel identifies
// which tile was stamped in a cell.
#define TILE_COLOUR(slot) ((uint8_t)(100 + (slot)))

// Painted over the whole canvas before a reveal test. It is not any tile's
// colour and not the background, so a cell still showing it was never
// stamped at all -- which is exactly what "unrevealed" means here.
#define UNLIT 200

// The turtle boots at Logo (0,0), the middle of the canvas, and `snaptile`
// captures the 8x8 square around it.
#define CAPTURE_X (MOCK_SCREEN_WIDTH_PX / 2 - 4)
#define CAPTURE_Y (MOCK_SCREEN_HEIGHT_PX / 2 - 4)

// Replace the game's pen-drawn art with flat colours. The mock does not
// rasterise the pen, so every tile setup.tiles captures is indistinguishable
// on it; staging the bank is what makes a pixel assertion mean something.
static void stage_bank(void)
{
    run("ask 7 [pu setpos [0 0]]");
    for (int slot = S_WALL; slot <= S_SNAKE; slot++) {
        mock_device_paint_canvas(CAPTURE_X, CAPTURE_Y, 8, 8, TILE_COLOUR(slot));
        runf("ask 7 [snaptile %d]", slot);
    }
}

static void darken(void)
{
    mock_device_paint_canvas(0, 0, MOCK_SCREEN_WIDTH_PX, MOCK_SCREEN_HEIGHT_PX, UNLIT);
}

// The centre pixel of a cell. Staged tiles are flat, so one pixel is enough.
static uint8_t cell_pixel(int col, int row)
{
    return mock_device_get_canvas_point((col - 1) * 8 + 4, (row - 1) * 8 + 4);
}

static bool lit(int col, int row) { return cell_pixel(col, row) != UNLIT; }

// A revealed cell shows the tile its map slot names.
static void assert_shows_its_slot(int col, int row)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "cell (%d,%d) does not show its own tile", col, row);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(TILE_COLOUR(slot_at(col, row)), cell_pixel(col, row), msg);
}

void setUp(void)
{
    test_scaffold_setUp_with_device_and_hardware();
    load_logo(TEMPLE_SOURCE);
    // A fixed sequence, so a failure can be reproduced and a maze property
    // is checked against a real maze rather than a lucky one.
    run("(rerandom 20220301)");
    run("setup.tiles");
    run("setup.temple");
}

void tearDown(void) { test_scaffold_tearDown(); }

// ---------------------------------------------------------------------------
// 1. Geometry and directions
// ---------------------------------------------------------------------------

// The 40x40 map of 8 px cells must cover the 320x320 screen exactly: cell 1
// centred at -156 and cell 40 at +156 puts the outer edges on -160 and +160.
void test_the_map_covers_the_screen_exactly(void)
{
    TEST_ASSERT_EQUAL_FLOAT(-156.0f, numf("tile.x 1"));
    TEST_ASSERT_EQUAL_FLOAT(156.0f, numf("tile.x %d", MAP_COLS));
    TEST_ASSERT_EQUAL_FLOAT(156.0f, numf("tile.y 1"));
    TEST_ASSERT_EQUAL_FLOAT(-156.0f, numf("tile.y %d", MAP_ROWS));
    // Adjacent cells are one tile apart, and row 1 is the top.
    TEST_ASSERT_EQUAL_FLOAT(8.0f, numf("(tile.x 3) - (tile.x 2)"));
    TEST_ASSERT_EQUAL_FLOAT(-8.0f, numf("(tile.y 3) - (tile.y 2)"));
}

void test_direction_deltas_and_opposites(void)
{
    const int op[5] = {0, 3, 4, 1, 2};
    for (int d = 1; d <= 4; d++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(DC[d], (int)numf("dir.dc %d", d), "dir.dc");
        TEST_ASSERT_EQUAL_INT_MESSAGE(DR[d], (int)numf("dir.dr %d", d), "dir.dr");
        TEST_ASSERT_EQUAL_INT_MESSAGE(op[d], (int)numf("opposite %d", d), "opposite");
    }
}

// The slot order is load-bearing: `walkable?` is one comparison, so anything
// that can be stood on must sort above Bocco's own slot. A snake is walkable
// -- the player is allowed to pay the toll and walk through it.
void test_a_snake_is_walkable_and_stone_is_not(void)
{
    truth("walkable? 0", "false");
    truth("walkable? :sl.wall", "false");
    truth("walkable? :sl.heart", "false");
    truth("walkable? :sl.bocco", "false");
    truth("walkable? :sl.floor", "true");
    truth("walkable? :sl.flask", "true");
    truth("walkable? :sl.chest", "true");
    truth("walkable? :sl.snake", "true");
}

// ---------------------------------------------------------------------------
// 2. The labyrinth
// ---------------------------------------------------------------------------

// Movement does no bounds testing at all -- it is the solid ring of wall that
// keeps a walker on the map, and it is also what keeps the 3x3 block the
// reveal stamps inside the map. If the ring were ever breached, a step could
// index outside it.
void test_the_labyrinth_is_ringed_by_solid_wall(void)
{
    for (int r = 1; r <= MAZE_ROWS; r++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(S_WALL, slot_at(1, r), "west wall");
        TEST_ASSERT_EQUAL_INT_MESSAGE(S_WALL, slot_at(MAZE_COLS, r), "east wall");
    }
    for (int c = 1; c <= MAZE_COLS; c++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(S_WALL, slot_at(c, 1), "north wall");
        TEST_ASSERT_EQUAL_INT_MESSAGE(S_WALL, slot_at(c, MAZE_ROWS), "south wall");
    }
    for (int r = 1; r <= MAP_ROWS; r++)
        TEST_ASSERT_EQUAL_INT_MESSAGE(S_WALL, slot_at(MAP_COLS, r), "east margin");
}

// The HUD sits below the labyrinth's south wall on cells that are background
// or hearts. Nothing there is walkable, so a heart can never be picked up.
void test_the_hud_rows_are_not_walkable(void)
{
    for (int r = MAZE_ROWS + 1; r <= MAP_ROWS; r++)
        for (int c = 1; c < MAP_COLS; c++)
            TEST_ASSERT_FALSE_MESSAGE(walkable(slot_at(c, r)), "a HUD cell is walkable");
}

// Every chamber is carved. A depth-first carve that left one unvisited would
// strand whatever got placed in it -- including, one game in three hundred,
// the chest.
void test_every_chamber_is_carved(void)
{
    for (int j = 1; j <= CHAMBERS_Y; j++)
        for (int i = 1; i <= CHAMBERS_X; i++)
            TEST_ASSERT_TRUE_MESSAGE(walkable(slot_at(2 * i, 2 * j)), "an uncarved chamber");
}

// The one property the game depends on and cannot check for itself: from
// Bocco's corner every chamber, and so the chest and every flask, is
// reachable. Flood fill in C over the real map.
void test_the_labyrinth_is_one_connected_network(void)
{
    int m[MAP_ROWS + 1][MAP_COLS + 1];
    read_map(m);

    static bool seen[MAP_ROWS + 1][MAP_COLS + 1];
    memset(seen, 0, sizeof(seen));

    int stack[MAP_ROWS * MAP_COLS][2];
    int top = 0;
    stack[top][0] = 2;
    stack[top][1] = 2;
    top++;
    seen[2][2] = true;

    while (top > 0) {
        top--;
        int c = stack[top][0], r = stack[top][1];
        for (int d = 1; d <= 4; d++) {
            int nc = c + DC[d], nr = r + DR[d];
            if (nc < 1 || nc > MAP_COLS || nr < 1 || nr > MAP_ROWS) continue;
            if (seen[nr][nc] || !walkable(m[nr][nc])) continue;
            seen[nr][nc] = true;
            stack[top][0] = nc;
            stack[top][1] = nr;
            top++;
        }
    }

    for (int j = 1; j <= CHAMBERS_Y; j++)
        for (int i = 1; i <= CHAMBERS_X; i++)
            TEST_ASSERT_TRUE_MESSAGE(seen[2 * j][2 * i], "a chamber is walled off");
}

// The loop openings must land on the walls *between* chambers and nowhere
// else. A cell with both coordinates odd is a wall junction; opening one
// would start dissolving the labyrinth into open rooms.
void test_no_wall_junction_is_ever_opened(void)
{
    for (int r = 1; r <= MAZE_ROWS; r += 2)
        for (int c = 1; c <= MAZE_COLS; c += 2)
            TEST_ASSERT_EQUAL_INT_MESSAGE(S_WALL, slot_at(c, r), "an opened wall junction");
}

// Loops give Bocco somewhere to go instead of paying a snake's toll, so
// there must be more open cells than a spanning tree has -- but only a few
// more, or a temple full of snakes would cost nothing to cross.
void test_a_few_loops_are_opened_and_no_more(void)
{
    int m[MAP_ROWS + 1][MAP_COLS + 1];
    read_map(m);

    // A perfect maze on N chambers has exactly N-1 walls knocked out.
    int gaps = 0;
    for (int r = 2; r < MAZE_ROWS; r++)
        for (int c = 2; c < MAZE_COLS; c++)
            if (((c % 2) + (r % 2)) == 1 && walkable(m[r][c])) gaps++;

    const int tree = CHAMBERS_X * CHAMBERS_Y - 1;
    TEST_ASSERT_GREATER_THAN_MESSAGE(tree, gaps, "open.loops opened no new routes");
    TEST_ASSERT_LESS_THAN_MESSAGE(tree + 16, gaps, "the labyrinth is dissolving into rooms");
}

// The carve is reproducible: the same seed must give the same labyrinth, or
// a reported game could never be replayed.
void test_the_same_seed_carves_the_same_labyrinth(void)
{
    int a[MAP_ROWS + 1][MAP_COLS + 1];
    int b[MAP_ROWS + 1][MAP_COLS + 1];

    run("(rerandom 4242) setup.temple");
    read_map(a);
    run("(rerandom 4242) setup.temple");
    read_map(b);

    for (int r = 1; r <= MAP_ROWS; r++)
        for (int c = 1; c <= MAP_COLS; c++)
            TEST_ASSERT_EQUAL_INT_MESSAGE(a[r][c], b[r][c], "the same seed carved a different maze");
}

// ---------------------------------------------------------------------------
// 3. Stocking the temple
// ---------------------------------------------------------------------------

static int count_slot(int want)
{
    int m[MAP_ROWS + 1][MAP_COLS + 1];
    read_map(m);
    int n = 0;
    for (int r = 1; r <= MAZE_ROWS; r++)
        for (int c = 1; c <= MAZE_COLS; c++)
            if (m[r][c] == want) n++;
    return n;
}

// Everything placed goes in a chamber -- never in the gap between two -- so
// nothing is hidden inside a wall join.
static void assert_all_in_chambers(int want, int away, const char *what)
{
    int m[MAP_ROWS + 1][MAP_COLS + 1];
    read_map(m);
    for (int r = 1; r <= MAZE_ROWS; r++)
        for (int c = 1; c <= MAZE_COLS; c++)
            if (m[r][c] == want) {
                TEST_ASSERT_EQUAL_INT_MESSAGE(0, c % 2, what);
                TEST_ASSERT_EQUAL_INT_MESSAGE(0, r % 2, what);
                TEST_ASSERT_GREATER_THAN_MESSAGE(away, c + r, what);
            }
}

// One chest, in a chamber, at least twenty chambers from Bocco -- the walk
// there is the game.
void test_one_chest_is_placed_far_from_bocco(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, count_slot(S_CHEST), "there is not exactly one chest");
    assert_all_in_chambers(S_CHEST, 42, "the chest is misplaced");
}

void test_flasks_are_placed_in_chambers_away_from_the_start(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(FLASKS, count_slot(S_FLASK), "wrong number of flasks");
    assert_all_in_chambers(S_FLASK, 4, "a flask is misplaced");
}

// Snakes are coiled in chambers Bocco can reach but not next to him -- a
// bite in the first second would be the original's rules applied unfairly.
void test_snakes_are_coiled_in_chambers_at_a_distance(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(SNAKES, count_slot(S_SNAKE), "wrong number of snakes");
    assert_all_in_chambers(S_SNAKE, 14, "a snake is misplaced");
}

// Nothing may be stocked on top of anything else: `free.chamber` only ever
// offers bare floor, so the counts above must all survive together.
void test_nothing_is_stocked_on_top_of_anything_else(void)
{
    TEST_ASSERT_EQUAL_INT(1, count_slot(S_CHEST));
    TEST_ASSERT_EQUAL_INT(FLASKS, count_slot(S_FLASK));
    TEST_ASSERT_EQUAL_INT(SNAKES, count_slot(S_SNAKE));
}

// ---------------------------------------------------------------------------
// 4. The dark
// ---------------------------------------------------------------------------

// Nothing is drawn until Bocco is beside it. This is the whole premise, and
// it cannot be seen in the map -- an unrevealed cell and a revealed one hold
// the same slot -- so it is asserted on the canvas.
void test_the_temple_starts_dark(void)
{
    stage_bank();
    run("setup.temple");
    darken();
    run("reveal");

    // Only the 3x3 around Bocco's corner is lit.
    for (int r = 1; r <= 3; r++)
        for (int c = 1; c <= 3; c++)
            TEST_ASSERT_TRUE_MESSAGE(lit(c, r), "the block around Bocco is not lit");

    TEST_ASSERT_FALSE_MESSAGE(lit(4, 2), "the cell beyond the block is lit");
    TEST_ASSERT_FALSE_MESSAGE(lit(2, 4), "the cell beyond the block is lit");
    TEST_ASSERT_FALSE_MESSAGE(lit(20, 20), "the far side of the temple is lit");
    TEST_ASSERT_FALSE_MESSAGE(lit(MAZE_COLS - 1, MAZE_ROWS - 1), "the far corner is lit");
}

// A revealed cell shows what the map says is there -- wall, floor, a flask,
// a snake -- so the picture and the rules cannot disagree about the temple.
void test_a_revealed_cell_shows_its_own_tile(void)
{
    stage_bank();
    run("setup.temple");
    darken();
    put_bocco(10, 10);
    run("reveal");

    for (int r = 9; r <= 11; r++)
        for (int c = 9; c <= 11; c++)
            assert_shows_its_slot(c, r);
}

// Walking is what uncovers the temple: a step lights the block around where
// Bocco lands, and nothing further.
void test_a_step_reveals_the_block_around_bocco(void)
{
    stage_bank();
    run("setup.temple");
    // A known corridor east from the start.
    for (int c = 2; c <= 6; c++) runf("settile %d 2 %d", c, S_FLOOR);
    darken();
    put_bocco(2, 2);
    run("make \"st.want 4");
    run("step.bocco");

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, (int)num(":st.col"), "the step did not happen");
    for (int r = 1; r <= 3; r++)
        for (int c = 2; c <= 4; c++)
            TEST_ASSERT_TRUE_MESSAGE(lit(c, r), "the block around the landing is not lit");
    TEST_ASSERT_FALSE_MESSAGE(lit(5, 2), "the step lit further than the block");
}

// Ground already walked stays visible, so the picture accumulates into the
// map Bocco has explored rather than a torch moving through the dark.
void test_revealed_ground_stays_revealed(void)
{
    stage_bank();
    run("setup.temple");
    for (int c = 2; c <= 8; c++) runf("settile %d 2 %d", c, S_FLOOR);
    darken();
    put_bocco(2, 2);
    // As a game starts: the corner is revealed before the first step.
    run("reveal");
    run("make \"st.want 4");
    run("step.bocco  step.bocco  step.bocco");

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, (int)num(":st.col"), "three steps did not happen");
    // The first block is four cells behind him and still lit.
    TEST_ASSERT_TRUE_MESSAGE(lit(1, 1), "the ground first revealed went dark again");
    TEST_ASSERT_TRUE_MESSAGE(lit(2, 2), "the ground first revealed went dark again");
    TEST_ASSERT_TRUE_MESSAGE(lit(6, 2), "the newest block is not lit");
    TEST_ASSERT_FALSE_MESSAGE(lit(7, 2), "the walk lit further than the block");
}

// A blocked step reveals nothing new: Bocco did not move, so there is nothing
// beside him he has not already seen.
void test_a_blocked_step_reveals_nothing(void)
{
    stage_bank();
    run("setup.temple");
    darken();
    put_bocco(2, 2);
    run("make \"st.want 1");  // north, into the border wall
    run("step.bocco");

    TEST_ASSERT_FALSE_MESSAGE(lit(2, 2), "a blocked step drew something");
}

// ---------------------------------------------------------------------------
// 5. Bocco in the map, and out of it
// ---------------------------------------------------------------------------

// Bocco is the one thing not in the map, so his cell lends him its slot for
// a single stamp and takes it straight back. If it did not, a flask he was
// standing on would be gone from the map and `tile` would answer with him.
void test_drawing_bocco_leaves_the_world_alone(void)
{
    int before[MAP_ROWS + 1][MAP_COLS + 1];
    int after[MAP_ROWS + 1][MAP_COLS + 1];
    read_map(before);

    run("draw.bocco");
    run("draw.bocco");
    read_map(after);

    for (int r = 1; r <= MAP_ROWS; r++)
        for (int c = 1; c <= MAP_COLS; c++)
            TEST_ASSERT_EQUAL_INT_MESSAGE(before[r][c], after[r][c], "drawing changed the world");
}

void test_drawing_bocco_over_a_flask_keeps_the_flask(void)
{
    runf("settile 4 2 %d", S_FLASK);
    put_bocco(4, 2);
    run("draw.bocco");
    TEST_ASSERT_EQUAL_INT(S_FLASK, slot_at(4, 2));
}

// He is drawn on top of the cell he stands on, so he is never lost under the
// tile the reveal has just stamped there.
void test_bocco_is_drawn_over_the_revealed_cell(void)
{
    stage_bank();
    run("setup.temple");
    darken();
    put_bocco(10, 10);
    run("reveal  draw.bocco");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(TILE_COLOUR(S_BOCCO), cell_pixel(10, 10),
                                    "Bocco is under the tile he is standing on");
}

// ---------------------------------------------------------------------------
// 6. Movement
// ---------------------------------------------------------------------------

void test_bocco_walks_into_open_floor(void)
{
    runf("settile 3 2 %d  settile 4 2 %d", S_FLOOR, S_FLOOR);
    put_bocco(2, 2);
    run("make \"st.want 4");
    run("step.bocco");
    TEST_ASSERT_EQUAL_INT(3, (int)num(":st.col"));
    TEST_ASSERT_EQUAL_INT(2, (int)num(":st.row"));
    run("step.bocco");
    TEST_ASSERT_EQUAL_INT(4, (int)num(":st.col"));
}

// A wall stops him and clears the intent, so he stands still instead of
// grinding against the stone until another arrow arrives.
void test_a_wall_stops_bocco_and_clears_the_intent(void)
{
    put_bocco(2, 2);
    run("make \"st.want 1");
    run("step.bocco");
    TEST_ASSERT_EQUAL_INT(2, (int)num(":st.col"));
    TEST_ASSERT_EQUAL_INT(2, (int)num(":st.row"));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":st.want"), "the blocked intent was kept");
}

void test_no_intent_means_no_step(void)
{
    put_bocco(2, 2);
    run("make \"st.want 0");
    run("step.bocco");
    TEST_ASSERT_EQUAL_INT(2, (int)num(":st.col"));
    TEST_ASSERT_EQUAL_INT(2, (int)num(":st.row"));
}

// ---------------------------------------------------------------------------
// 7. The published rules
// ---------------------------------------------------------------------------

// "Each flask you find restores 2 health points."
void test_a_flask_restores_two_health_and_is_consumed(void)
{
    runf("settile 4 2 %d", S_FLASK);
    put_bocco(4, 2);
    run("make \"st.health 5");
    run("enter.cell");
    TEST_ASSERT_EQUAL_INT(7, (int)num(":st.health"));
    TEST_ASSERT_EQUAL_INT_MESSAGE(S_FLOOR, slot_at(4, 2), "the flask was not consumed");
}

// Health is a ten-heart bar, so a flask taken at full health cannot overflow
// it -- the HUD only has ten hearts to draw.
void test_a_flask_cannot_push_health_over_the_maximum(void)
{
    runf("settile 4 2 %d", S_FLASK);
    put_bocco(4, 2);
    run("make \"st.health 9");
    run("enter.cell");
    TEST_ASSERT_EQUAL_INT(MAX_HEALTH, (int)num(":st.health"));
}

// "A snake inflicts damage on 1 to 4 health points." Walk onto one many
// times and check the cost never leaves that range -- and that both ends of
// it occur, so a bite that always cost the same would fail too.
void test_a_snake_costs_between_one_and_four_health(void)
{
    bool seen[5] = {false, false, false, false, false};
    put_bocco(10, 10);

    for (int trial = 0; trial < 200; trial++) {
        runf("settile 10 10 %d", S_SNAKE);
        run("make \"st.health 100");
        run("enter.cell");
        int loss = 100 - (int)num(":st.health");
        TEST_ASSERT_TRUE_MESSAGE(loss >= 1 && loss <= 4, "a bite cost outside 1..4");
        seen[loss] = true;
    }
    for (int d = 1; d <= 4; d++)
        TEST_ASSERT_TRUE_MESSAGE(seen[d], "a damage value in 1..4 never came up");
}

// The snakes do not move, and a bite does not drive one off: it stays coiled
// where it lies, so the toll is paid again by anyone who comes back this way.
void test_a_snake_stays_coiled_after_biting(void)
{
    runf("settile 10 10 %d", S_SNAKE);
    put_bocco(10, 10);
    run("make \"st.health 100");
    run("enter.cell");
    TEST_ASSERT_EQUAL_INT_MESSAGE(S_SNAKE, slot_at(10, 10), "the snake left after biting");

    int after_first = (int)num(":st.health");
    run("enter.cell");
    TEST_ASSERT_LESS_THAN_MESSAGE(after_first, (int)num(":st.health"),
                                  "coming back this way cost nothing");
}

// Walking through a snake is a move the player is allowed to make -- the
// toll is the point, not a wall.
void test_bocco_can_walk_through_a_snake(void)
{
    runf("settile 3 2 %d", S_SNAKE);
    put_bocco(2, 2);
    run("make \"st.health 10  make \"st.want 4");
    run("step.bocco");
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, (int)num(":st.col"), "a snake blocked the corridor");
    TEST_ASSERT_LESS_THAN_MESSAGE(10, (int)num(":st.health"), "walking onto a snake cost nothing");
}

// "Among the labyrinth of tunnels is a treasure chest that you must find."
void test_reaching_the_chest_wins(void)
{
    runf("settile 4 2 %d", S_CHEST);
    put_bocco(4, 2);
    run("make \"st.won \"false");
    run("enter.cell");
    truth(":st.won", "true");
    truth("game.over?", "true");
}

void test_plain_floor_changes_nothing(void)
{
    runf("settile 4 2 %d", S_FLOOR);
    put_bocco(4, 2);
    run("make \"st.health 5  make \"st.won \"false");
    run("enter.cell");
    TEST_ASSERT_EQUAL_INT(5, (int)num(":st.health"));
    truth(":st.won", "false");
}

// Running out of health ends the game; so does giving up.
void test_the_game_ends_on_death_and_on_quitting(void)
{
    run("make \"st.won \"false  make \"st.quit \"false  make \"st.health 1");
    truth("game.over?", "false");
    run("make \"st.health 0");
    truth("game.over?", "true");
    run("make \"st.health 5  make \"st.quit \"true");
    truth("game.over?", "true");
}

// ---------------------------------------------------------------------------
// 8. Input
// ---------------------------------------------------------------------------

// The four arrows, as the PicoCalc keyboard sends them.
void test_arrows_set_the_direction(void)
{
    const char *keys[4] = {"\xB5", "\xB4", "\xB6", "\xB7"};  // up, left, down, right
    for (int d = 1; d <= 4; d++) {
        run("make \"st.want 0  make \"st.paused \"false");
        mock_device_set_input(keys[d - 1]);
        run("poll.input");
        TEST_ASSERT_EQUAL_INT_MESSAGE(d, (int)num(":st.want"), "an arrow did not steer");
    }
}

// The queue is drained and the most recent arrow wins, so a burst of key
// repeat cannot leave Bocco walking the way he was steered three frames ago.
void test_the_last_arrow_in_the_queue_wins(void)
{
    run("make \"st.want 0  make \"st.paused \"false");
    mock_device_set_input("\xB5\xB4\xB7");  // up, left, right
    run("poll.input");
    TEST_ASSERT_EQUAL_INT(4, (int)num(":st.want"));
}

// A paused game must still answer its own unpause key -- and nothing else.
// Reading arrows while paused would let a player line Bocco up for free (the
// defect fixed in Galaxian and Invaders on 2026-08-10).
void test_a_paused_game_reads_the_pause_key_and_no_arrows(void)
{
    run("make \"st.want 0  make \"st.paused \"false");
    mock_device_set_input("p\xB7");  // pause, then right
    run("poll.input");
    truth(":st.paused", "true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)num(":st.want"), "a paused game steered");

    mock_device_set_input("p");
    run("poll.input");
    truth(":st.paused", "false");

    mock_device_set_input("\xB7");
    run("poll.input");
    TEST_ASSERT_EQUAL_INT_MESSAGE(4, (int)num(":st.want"), "steering did not come back");
}

void test_q_gives_up(void)
{
    run("make \"st.quit \"false  make \"st.paused \"false");
    mock_device_set_input("q");
    run("poll.input");
    truth(":st.quit", "true");
}

// ---------------------------------------------------------------------------
// 9. The heads-up display
// ---------------------------------------------------------------------------

// One heart per health point, the rest cleared to background -- and the row
// is only redrawn when the number changes.
void test_the_hud_draws_one_heart_per_health_point(void)
{
    run("make \"st.hud -1  make \"st.health 4");
    run("update.hud");
    for (int i = 1; i <= MAX_HEALTH; i++) {
        int want = (i > 4) ? S_NOTHING : S_HEART;
        TEST_ASSERT_EQUAL_INT_MESSAGE(want, slot_at(8 + i, 38), "wrong heart count");
    }

    run("make \"st.health 9");
    run("update.hud");
    for (int i = 1; i <= MAX_HEALTH; i++) {
        int want = (i > 9) ? S_NOTHING : S_HEART;
        TEST_ASSERT_EQUAL_INT_MESSAGE(want, slot_at(8 + i, 38), "the bar did not grow back");
    }
}

void test_the_hud_is_not_redrawn_when_health_is_unchanged(void)
{
    run("make \"st.hud -1  make \"st.health 6");
    run("update.hud");
    // Vandalise a heart cell; an unchanged health must leave it alone.
    run("settile 9 38 0");
    run("update.hud");
    TEST_ASSERT_EQUAL_INT_MESSAGE(S_NOTHING, slot_at(9, 38), "the HUD redrew for nothing");

    run("make \"st.health 5");
    run("update.hud");
    TEST_ASSERT_EQUAL_INT_MESSAGE(S_HEART, slot_at(9, 38), "the HUD did not redraw on a change");
}

// ---------------------------------------------------------------------------
// 10. The frame
// ---------------------------------------------------------------------------

// The whole frame has to run: the parse hazards at the top of the game file
// are runtime errors that reading the source does not catch, so every path
// through play.frame is exercised here for as long as a game lasts.
void test_the_frame_runs_and_reclaims(void)
{
    run("make \"st.want 4");
    for (int f = 0; f < 300; f++) run("play.frame");
    TEST_ASSERT_EQUAL_INT_MESSAGE(300, (int)num(":st.frame"), "the frame counter stalled");
    TEST_ASSERT_TRUE_MESSAGE(walkable(slot_at((int)num(":st.col"), (int)num(":st.row"))),
                             "Bocco ended a frame inside stone");
}

// A paused frame advances nothing at all.
void test_a_paused_frame_advances_nothing(void)
{
    run("make \"st.paused \"true  make \"st.frame 17");
    put_bocco(2, 2);
    run("play.frame");
    TEST_ASSERT_EQUAL_INT(17, (int)num(":st.frame"));
    TEST_ASSERT_EQUAL_INT(2, (int)num(":st.col"));
    TEST_ASSERT_EQUAL_INT(2, (int)num(":st.row"));
}

// A fresh game puts everything back, so a second game is not the first one's
// leftovers.
void test_a_new_game_resets_the_state(void)
{
    run("make \"st.health 1  make \"st.won \"true  make \"st.quit \"true");
    run("make \"st.frame 900  make \"st.want 3");
    run("setup.temple");
    TEST_ASSERT_EQUAL_INT(MAX_HEALTH, (int)num(":st.health"));
    TEST_ASSERT_EQUAL_INT(0, (int)num(":st.frame"));
    TEST_ASSERT_EQUAL_INT(0, (int)num(":st.want"));
    TEST_ASSERT_EQUAL_INT(2, (int)num(":st.col"));
    TEST_ASSERT_EQUAL_INT(2, (int)num(":st.row"));
    truth(":st.won", "false");
    truth(":st.quit", "false");
}

// ---------------------------------------------------------------------------
// 11. Screens
// ---------------------------------------------------------------------------

// The laid-out screens are all `setcursor` plus vertical-bar quoted words,
// and a mistake in one of those is a runtime error that reading the source
// does not catch. Each screen ends by waiting for space, so feeding a space
// is what lets the test run the whole procedure.
void test_the_title_screen_draws(void)
{
    mock_device_set_input(" ");
    run("title.screen");
}

void test_both_end_screens_draw(void)
{
    run("make \"st.quit \"false  make \"st.won \"true");
    mock_device_set_input(" ");
    run("end.screen");

    run("make \"st.won \"false");
    mock_device_set_input(" ");
    run("end.screen");
}

// Giving up goes straight back to the title without a banner, so a player who
// quit is not made to press space at a screen they did not ask for.
void test_giving_up_shows_no_end_screen(void)
{
    run("make \"st.quit \"true  make \"st.won \"false");
    mock_device_set_input("");
    run("end.screen");
}

// The whole path, once: title screen, a game set up and played, and the exit.
// `q` is queued behind the space that starts the game, so the first frame
// reads it and the game ends there.
void test_a_whole_game_runs_from_the_title_screen(void)
{
    mock_device_set_input(" q");
    run("title.screen");
    run("one.game");
    truth(":st.quit", "true");
}

// ---------------------------------------------------------------------------
// 12. Workspace budget
// ---------------------------------------------------------------------------

// The workspace has to leave room for a profiler or another program to load
// on top of the game, which is what B14 was about. This game is far smaller
// than Turtle Trails; pin that it stays that way.
void test_the_game_leaves_the_workspace_room_to_spare(void)
{
    char msg[96];
    int used = var_global_count(true);
    snprintf(msg, sizeof(msg), "the game holds %d of %d globals", used, MAX_GLOBAL_VARIABLES);
    TEST_ASSERT_LESS_THAN_MESSAGE(MAX_GLOBAL_VARIABLES / 2, used, msg);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_the_map_covers_the_screen_exactly);
    RUN_TEST(test_direction_deltas_and_opposites);
    RUN_TEST(test_a_snake_is_walkable_and_stone_is_not);

    RUN_TEST(test_the_labyrinth_is_ringed_by_solid_wall);
    RUN_TEST(test_the_hud_rows_are_not_walkable);
    RUN_TEST(test_every_chamber_is_carved);
    RUN_TEST(test_the_labyrinth_is_one_connected_network);
    RUN_TEST(test_no_wall_junction_is_ever_opened);
    RUN_TEST(test_a_few_loops_are_opened_and_no_more);
    RUN_TEST(test_the_same_seed_carves_the_same_labyrinth);

    RUN_TEST(test_one_chest_is_placed_far_from_bocco);
    RUN_TEST(test_flasks_are_placed_in_chambers_away_from_the_start);
    RUN_TEST(test_snakes_are_coiled_in_chambers_at_a_distance);
    RUN_TEST(test_nothing_is_stocked_on_top_of_anything_else);

    RUN_TEST(test_the_temple_starts_dark);
    RUN_TEST(test_a_revealed_cell_shows_its_own_tile);
    RUN_TEST(test_a_step_reveals_the_block_around_bocco);
    RUN_TEST(test_revealed_ground_stays_revealed);
    RUN_TEST(test_a_blocked_step_reveals_nothing);

    RUN_TEST(test_drawing_bocco_leaves_the_world_alone);
    RUN_TEST(test_drawing_bocco_over_a_flask_keeps_the_flask);
    RUN_TEST(test_bocco_is_drawn_over_the_revealed_cell);

    RUN_TEST(test_bocco_walks_into_open_floor);
    RUN_TEST(test_a_wall_stops_bocco_and_clears_the_intent);
    RUN_TEST(test_no_intent_means_no_step);

    RUN_TEST(test_a_flask_restores_two_health_and_is_consumed);
    RUN_TEST(test_a_flask_cannot_push_health_over_the_maximum);
    RUN_TEST(test_a_snake_costs_between_one_and_four_health);
    RUN_TEST(test_a_snake_stays_coiled_after_biting);
    RUN_TEST(test_bocco_can_walk_through_a_snake);
    RUN_TEST(test_reaching_the_chest_wins);
    RUN_TEST(test_plain_floor_changes_nothing);
    RUN_TEST(test_the_game_ends_on_death_and_on_quitting);

    RUN_TEST(test_arrows_set_the_direction);
    RUN_TEST(test_the_last_arrow_in_the_queue_wins);
    RUN_TEST(test_a_paused_game_reads_the_pause_key_and_no_arrows);
    RUN_TEST(test_q_gives_up);

    RUN_TEST(test_the_hud_draws_one_heart_per_health_point);
    RUN_TEST(test_the_hud_is_not_redrawn_when_health_is_unchanged);

    RUN_TEST(test_the_frame_runs_and_reclaims);
    RUN_TEST(test_a_paused_frame_advances_nothing);
    RUN_TEST(test_a_new_game_resets_the_state);

    RUN_TEST(test_the_title_screen_draws);
    RUN_TEST(test_both_end_screens_draw);
    RUN_TEST(test_giving_up_shows_no_end_screen);
    RUN_TEST(test_a_whole_game_runs_from_the_title_screen);

    RUN_TEST(test_the_game_leaves_the_workspace_room_to_spare);

    return UNITY_END();
}
