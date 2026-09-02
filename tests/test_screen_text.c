//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  What `write` puts on the canvas.  Compiles devices/picocalc/screen.c on
//  the host against tests/fake_lcd.c, the same way test_screen_pen.c does,
//  and reads the canvas back with screen_gfx_get_point -- and the PANEL back
//  with fake_lcd_panel_point, which is the only way to see whether the dirty
//  rectangle was right.
//
//  WHY THIS FILE EXISTS.  `write` had exactly one behaviour: light the pixels
//  a glyph is made of and leave the rest of the cell alone.  That made it
//  impossible to draw a filled text cell, and therefore impossible to draw a
//  bar of text in inverse video -- Dungeons of Daggorath's status row is the
//  complement of the view above it (`STATUS.ASM:STATUX`), and nothing in the
//  language could paint the cell.  There was no eraser for text in a picture
//  either: `pe write :old` does not erase, and writing spaces over text
//  erases nothing, because a space lights no pixels at all.
//
//  P18 M1 added the background.  The three claims that matter are pixel
//  claims, so they are made here rather than against the mock: transparent is
//  BYTE-FOR-BYTE what it was, opaque stores every pixel of the cell, and the
//  cells tile with no seam -- the advance is GLYPH_WIDTH and the fill is
//  GLYPH_HEIGHT rows, so a run of them is a solid bar.
//

#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "fake_lcd.h"
#include "screen.h"

#define FG (12)      // A palette index nothing else in these tests uses
#define BG (9)       // The cell fill
#define UNDER (5)    // What was on the canvas before the write

// Where the writes land. Clear of every edge, so nothing is clipped.
#define AT_X (40)
#define AT_Y (60)

void setUp(void)
{
    screen_gfx_set_refresh_auto(true);
    screen_set_mode(SCREEN_MODE_TXT);
    screen_gfx_clear();
    screen_set_mode(SCREEN_MODE_GFX);
    screen_gfx_set_boundary_mode(SCREEN_BOUNDARY_WINDOW);
    fake_lcd_reset();
}

void tearDown(void)
{
    screen_gfx_set_refresh_auto(true);
    screen_set_mode(SCREEN_MODE_TXT);
}

static uint8_t at(int x, int y)
{
    return screen_gfx_get_point((float)x, (float)y);
}

// Paint the region a write is about to land on, so "untouched" is a colour a
// test can name rather than the absence of one.
static void undercoat(int cells)
{
    for (int y = AT_Y - 2; y < AT_Y + GLYPH_HEIGHT + 2; y++)
        for (int x = AT_X - 2; x < AT_X + cells * GLYPH_WIDTH + 2; x++)
            screen_gfx_set_point((float)x, (float)y, UNDER);
}

// Count the pixels of one cell that hold `colour`.
static int cell_count(int cell, uint8_t colour)
{
    int n = 0;
    for (int y = AT_Y; y < AT_Y + GLYPH_HEIGHT; y++)
        for (int x = AT_X + cell * GLYPH_WIDTH; x < AT_X + (cell + 1) * GLYPH_WIDTH; x++)
            if (at(x, y) == colour)
                n++;
    return n;
}

//==========================================================================

// TRANSPARENT IS UNCHANGED, and this is the half of M1 that had to not move.
// Every existing program calls the bare form, and an opaque default would
// punch a rectangle through all of them.
void test_a_transparent_write_leaves_the_rest_of_the_cell_alone(void)
{
    undercoat(1);
    screen_gfx_text(AT_X, AT_Y, "A", FG, -1);

    int lit = cell_count(0, FG);
    TEST_ASSERT_TRUE_MESSAGE(lit > 0, "the glyph lit no pixels at all");
    TEST_ASSERT_EQUAL_INT_MESSAGE(GLYPH_WIDTH * GLYPH_HEIGHT - lit,
        cell_count(0, UNDER),
        "a transparent write disturbed pixels the glyph does not light");
}

// A SPACE IS THE PROOF, because it lights nothing: transparently written it
// is a no-op, which is exactly why writing spaces over text never erased it.
void test_a_transparent_space_erases_nothing(void)
{
    undercoat(1);
    screen_gfx_text(AT_X, AT_Y, " ", FG, -1);

    TEST_ASSERT_EQUAL_INT_MESSAGE(GLYPH_WIDTH * GLYPH_HEIGHT, cell_count(0, UNDER),
        "a transparent space changed the canvas");
}

// OPAQUE STORES THE WHOLE CELL: the glyph in fg, everything else in bg, and
// nothing of what was underneath left anywhere in it.
void test_an_opaque_write_fills_the_whole_cell(void)
{
    undercoat(1);
    screen_gfx_text(AT_X, AT_Y, "A", FG, BG);

    int lit = cell_count(0, FG);
    int filled = cell_count(0, BG);
    TEST_ASSERT_TRUE_MESSAGE(lit > 0, "the glyph lit no pixels at all");
    TEST_ASSERT_TRUE_MESSAGE(filled > 0, "the cell fill painted nothing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(GLYPH_WIDTH * GLYPH_HEIGHT, lit + filled,
        "the cell is not wholly fg or bg -- something underneath showed through");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, cell_count(0, UNDER),
        "an opaque write left a pixel of what was underneath");
}

// AND THAT IS THE ERASER.  `(write :old 255 255)` is fg == bg, which stores a
// solid block: the one thing that could remove text from a picture short of
// `clean`.
void test_an_opaque_write_with_matching_colours_is_a_solid_block(void)
{
    undercoat(1);
    screen_gfx_text(AT_X, AT_Y, "A", BG, BG);

    TEST_ASSERT_EQUAL_INT_MESSAGE(GLYPH_WIDTH * GLYPH_HEIGHT, cell_count(0, BG),
        "fg == bg did not paint a solid cell");
}

// CELLS TILE WITH NO SEAM.  The advance is GLYPH_WIDTH and the fill is
// GLYPH_HEIGHT rows, so a run of opaque cells is one solid bar -- a column of
// undercoat surviving between two cells would be a one-pixel stripe through
// every status line ever drawn with this.
void test_a_run_of_opaque_cells_is_a_solid_bar(void)
{
    const int cells = 4;
    undercoat(cells);
    screen_gfx_text(AT_X, AT_Y, "    ", FG, BG);   // spaces: the bar alone

    for (int y = AT_Y; y < AT_Y + GLYPH_HEIGHT; y++)
    {
        for (int x = AT_X; x < AT_X + cells * GLYPH_WIDTH; x++)
        {
            char msg[96];
            snprintf(msg, sizeof(msg), "the bar has a hole at %d,%d", x - AT_X, y - AT_Y);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(BG, at(x, y), msg);
        }
    }

    // And not one pixel more: the bar is its own cells and nothing beside them.
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(UNDER, at(AT_X - 1, AT_Y),
        "the bar spilled left of the first cell");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(UNDER, at(AT_X + cells * GLYPH_WIDTH, AT_Y),
        "the bar spilled right of the last cell");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(UNDER, at(AT_X, AT_Y - 1),
        "the bar spilled above the cell");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(UNDER, at(AT_X, AT_Y + GLYPH_HEIGHT),
        "the bar spilled below the cell");
}

// THE DIRTY RECTANGLE COVERS THE FILLED CELL, not merely the lit pixels.
// screen_gfx_text always marked the whole cell extent, which was more than
// the transparent path needed and is exactly right for the opaque one -- but
// "exactly right" is a claim about what reaches the LCD, so it is read off
// the panel rather than off the canvas.  An opaque space is the sharpest form
// of it: not one pixel of the cell is lit by a glyph, so a dirty rectangle
// derived from lit pixels would present nothing at all.
void test_an_opaque_cell_reaches_the_panel_whole(void)
{
    screen_gfx_present();          // Start from a panel that matches the canvas
    fake_lcd_reset();

    screen_gfx_text(AT_X, AT_Y, " ", FG, BG);
    screen_gfx_present();

    for (int y = AT_Y; y < AT_Y + GLYPH_HEIGHT; y++)
    {
        for (int x = AT_X; x < AT_X + GLYPH_WIDTH; x++)
        {
            char msg[96];
            snprintf(msg, sizeof(msg), "%d,%d of the cell never reached the panel",
                     x - AT_X, y - AT_Y);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(BG, fake_lcd_panel_point(x, y), msg);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_a_transparent_write_leaves_the_rest_of_the_cell_alone);
    RUN_TEST(test_a_transparent_space_erases_nothing);
    RUN_TEST(test_an_opaque_write_fills_the_whole_cell);
    RUN_TEST(test_an_opaque_write_with_matching_colours_is_a_solid_block);
    RUN_TEST(test_a_run_of_opaque_cells_is_a_solid_bar);
    RUN_TEST(test_an_opaque_cell_reaches_the_panel_whole);

    return UNITY_END();
}
