//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  The shape a wide pen puts on the canvas.  Compiles devices/picocalc/screen.c
//  on the host against tests/fake_lcd.c, the same way test_screen_refresh.c
//  does, and reads the canvas back with screen_gfx_get_point.
//
//  WHY THIS FILE EXISTS.  The reference already says what a wide pen is -- "a
//  pen wider than one pixel draws by stamping a filled disc at each point
//  along the line ... and round ends" -- and Berzerk's design section 7.1
//  describes the same thing loosely, as "round caps that spill outside the
//  stroke".  Read as a warning about overspill that reads as though the cap
//  SQUARES the end off, and it does the opposite: `screen_gfx_line` stamps
//  every offset satisfying `ox*ox + oy*oy <= r*r`, so a stroke is a stadium
//  whose ends pinch to a single pixel.  Prose was never going to settle it;
//  pixels do.
//
//  That cost a board session (B67).  Berzerk erased its man with one pen-8
//  stroke inset by what the design called the cap radius, and the four corners
//  of his 8 x 16 were never erased at all -- 17 of 128 pixels, every frame, so
//  he dragged a trail in every direction.
//
//  So both halves of the fact are pinned here: a wide pen is a disc, and
//  **pen 3 is the one wide pen that is an exact square**, because its radius
//  is 1.5, its extent truncates to 1, and the corner (1, 1) is 2 against 2.25.
//  Anything that needs a stroke to cover a rectangle exactly has to be built
//  out of that one.
//

#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "fake_lcd.h"
#include "screen.h"

#define INK (12) // A palette index nothing else in these tests uses

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

static bool inked(int x, int y)
{
    return screen_gfx_get_point((float)x, (float)y) == INK;
}

// How many pixels wide the brush is, k rows above the top end of a vertical
// stroke: 0 means the stroke does not reach that row at all.
static int cap_width_above(int cx, int top, int k)
{
    int n = 0;
    for (int x = cx - 12; x <= cx + 12; x++)
        if (inked(x, top - k))
            n++;
    return n;
}

//==========================================================================

// A WIDE PEN IS A DISC.  Its cap is a semicircle, not a square extension: an
// 8-wide pen puts nine pixels across the stroke itself, seven one row past the
// end, five three rows past, and exactly one at four rows past.  A caller that
// insets a stroke by "the cap radius" and expects a rectangle gets the corners
// wrong, which is B67.
void test_a_wide_pens_cap_is_round_and_does_not_square_off_the_stroke(void)
{
    const int cx = 100, top = 100, bottom = 120;
    screen_gfx_line((float)cx, (float)top, (float)cx, (float)bottom, INK, false, 8, 1);

    TEST_ASSERT_EQUAL_INT_MESSAGE(9, cap_width_above(cx, top, 0),
        "an 8-wide pen is not nine pixels across its own stroke");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, cap_width_above(cx, top, 4),
        "four rows past the end the cap is not down to a single pixel");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, cap_width_above(cx, top, 5),
        "the cap reaches further than the pen's radius");

    // The claim that matters: the stroke plus its caps is NOT a rectangle.
    // Stand at the corner the design assumed was covered and it is empty.
    TEST_ASSERT_FALSE_MESSAGE(inked(cx - 4, top - 4),
        "the cap filled the corner, so B67's reasoning would have been right");
    TEST_ASSERT_FALSE_MESSAGE(inked(cx + 4, bottom + 4),
        "the cap filled the corner, so B67's reasoning would have been right");
}

// PEN 3 IS THE EXCEPTION AND THE WHOLE OF IT.  Radius 1.5, extent 1, and the
// corner offset (1, 1) is 2 against 2.25 -- so all nine pixels of the 3 x 3
// are in and a stroke of it is an exact rectangle, three wide by its length
// plus two.  This is the only brush anything can build a rectangle out of.
void test_pen_3_is_an_exact_square_brush(void)
{
    const int cx = 100, top = 100, bottom = 113;
    screen_gfx_line((float)cx, (float)top, (float)cx, (float)bottom, INK, false, 3, 1);

    for (int y = top - 1; y <= bottom + 1; y++)
    {
        for (int x = cx - 1; x <= cx + 1; x++)
        {
            char msg[96];
            snprintf(msg, sizeof(msg), "pen 3 left %d,%d unpainted", x, y);
            TEST_ASSERT_TRUE_MESSAGE(inked(x, y), msg);
        }
    }

    // And not one pixel more, so a rectangle built from it cannot spill onto
    // whatever is next to it -- which is what Berzerk's walls need.
    for (int y = top - 3; y <= bottom + 3; y++)
    {
        TEST_ASSERT_FALSE_MESSAGE(inked(cx - 2, y), "pen 3 is wider than three");
        TEST_ASSERT_FALSE_MESSAGE(inked(cx + 2, y), "pen 3 is wider than three");
    }
    TEST_ASSERT_FALSE_MESSAGE(inked(cx, top - 2), "pen 3's cap is longer than one");
    TEST_ASSERT_FALSE_MESSAGE(inked(cx, bottom + 2), "pen 3's cap is longer than one");
}

// Three pen-3 strokes cover an 8 x 16 exactly, which is the geometry Berzerk's
// eraser is built on: columns at +1, +4 and +6 of the box's left edge, run
// from one row inside the top to one row inside the bottom.  Asserted here
// rather than only in the game's own tests, because the game can only see the
// strokes it asked for and this can see the pixels they made.
void test_three_pen_3_strokes_cover_an_8_by_16_exactly(void)
{
    const int x0 = 100, y0 = 100;   // top-left of the box, screen pixels
    static const int at[3] = { 1, 4, 6 };

    for (int i = 0; i < 3; i++)
    {
        float cx = (float)(x0 + at[i]);
        screen_gfx_line(cx, (float)(y0 + 1), cx, (float)(y0 + 14), INK, false, 3, 1);
    }

    for (int y = y0; y < y0 + 16; y++)
        for (int x = x0; x < x0 + 8; x++)
        {
            char msg[96];
            snprintf(msg, sizeof(msg), "the eraser left %d,%d of the box behind",
                     x - x0, y - y0);
            TEST_ASSERT_TRUE_MESSAGE(inked(x, y), msg);
        }

    for (int y = y0 - 2; y < y0 + 18; y++)
        for (int x = x0 - 2; x < x0 + 10; x++)
        {
            if (x >= x0 && x < x0 + 8 && y >= y0 && y < y0 + 16)
                continue;
            char msg[96];
            snprintf(msg, sizeof(msg), "the eraser spilled onto %d,%d, which is a wall",
                     x - x0, y - y0);
            TEST_ASSERT_FALSE_MESSAGE(inked(x, y), msg);
        }
}

//==========================================================================
// P18 M2: the dashed pen.
//
// `setpendash n` puts the pen down on one point in every n along a stroke.
// This is Dungeons of Daggorath's `VECTOR.ASM:VECT30` -- `DEC FADCNT / BNE
// VECT40`, with FADCNT loaded from VCTFAD at the head of each vector -- so
// the counter starts FULL and the first point plotted is index n - 1, not
// index 0.  That off-by-one is the whole of the compatibility claim: get it
// backwards and the fade is a pixel out of phase with the ROM's everywhere.
//
// The spans below are hand-computed from that rule rather than read off the
// implementation.
//==========================================================================

// Count the inked pixels in a box, so "fewer pixels than solid" is a number.
static int inked_in(int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (inked(x, y))
                n++;
    return n;
}

// A DASH OF 1 IS SOLID AND IS THE DEFAULT.  The stroke every existing program
// draws has to be byte-for-byte what it was.
void test_a_dash_of_one_is_a_solid_stroke(void)
{
    const int cx = 100, top = 100, bottom = 120;
    screen_gfx_line((float)cx, (float)top, (float)cx, (float)bottom, INK, false, 1, 1);

    for (int y = top; y <= bottom; y++)
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "a solid pen skipped %d,%d", cx, y);
        TEST_ASSERT_TRUE_MESSAGE(inked(cx, y), msg);
    }
}

// THE HAND-COMPUTED SPAN, on the Y-driving branch.  y = 100 to 120 is 21
// points, indices 0..20.  With a period of 4 the counter reaches zero at
// indices 3, 7, 11, 15 and 19 -- so y = 103, 107, 111, 115, 119, and nothing
// else.  Note that neither end of the stroke is plotted: the ROM does not
// special-case them and nor does this.
void test_a_dashed_vertical_stroke_plots_the_roms_span(void)
{
    const int cx = 100, top = 100, bottom = 120;
    screen_gfx_line((float)cx, (float)top, (float)cx, (float)bottom, INK, false, 1, 4);

    static const int expect[5] = { 103, 107, 111, 115, 119 };
    for (int y = top; y <= bottom; y++)
    {
        bool want = false;
        for (int i = 0; i < 5; i++)
            if (expect[i] == y)
                want = true;

        char msg[80];
        snprintf(msg, sizeof(msg), "dash 4 got %d,%d wrong", cx, y);
        TEST_ASSERT_EQUAL_INT_MESSAGE(want ? 1 : 0, inked(cx, y) ? 1 : 0, msg);
    }
}

// THE SAME RULE ON THE X-DRIVING BRANCH, which is a separate loop in
// screen_gfx_line and could easily have been given the counter and not the
// reset, or neither.
void test_a_dashed_horizontal_stroke_plots_the_roms_span(void)
{
    const int cy = 100, left = 200, right = 220;
    screen_gfx_line((float)left, (float)cy, (float)right, (float)cy, INK, false, 1, 4);

    static const int expect[5] = { 203, 207, 211, 215, 219 };
    for (int x = left; x <= right; x++)
    {
        bool want = false;
        for (int i = 0; i < 5; i++)
            if (expect[i] == x)
                want = true;

        char msg[80];
        snprintf(msg, sizeof(msg), "dash 4 got %d,%d wrong", x, cy);
        TEST_ASSERT_EQUAL_INT_MESSAGE(want ? 1 : 0, inked(x, cy) ? 1 : 0, msg);
    }
}

// THE COUNTER STARTS AGAIN AT EACH STROKE, so a dashed figure is drawn the
// same way whichever order its sides go down -- and, less obviously, so a
// stroke of fewer than `dash` points draws nothing at all.  That is the ROM's
// behaviour and it is what makes VCTFAD == $FF mean "invisible".
void test_a_stroke_shorter_than_the_dash_period_draws_nothing(void)
{
    const int cx = 100, top = 100, bottom = 102;   // three points
    screen_gfx_line((float)cx, (float)top, (float)cx, (float)bottom, INK, false, 1, 8);

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, inked_in(cx - 2, top - 2, cx + 2, bottom + 2),
        "a stroke shorter than its dash period put pixels on the screen");
}

// A DASHED STROKE IS NOT SLOWER THAN THE SOLID ONE IT REPLACES: it stores
// strictly fewer pixels.  That is the whole reason this is worth having --
// dotting a line in Logo costs six statements a dot, and dotting it in the
// rasteriser costs one decrement.
void test_a_dashed_stroke_stores_fewer_pixels_than_a_solid_one(void)
{
    const int cx = 100, top = 100, bottom = 120;

    screen_gfx_line((float)cx, (float)top, (float)cx, (float)bottom, INK, false, 1, 1);
    int solid = inked_in(cx - 1, top - 1, cx + 1, bottom + 1);

    screen_gfx_clear();
    screen_gfx_line((float)cx, (float)top, (float)cx, (float)bottom, INK, false, 1, 4);
    int dashed = inked_in(cx - 1, top - 1, cx + 1, bottom + 1);

    TEST_ASSERT_EQUAL_INT_MESSAGE(21, solid, "the solid stroke is not 21 points long");
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, dashed, "the dashed stroke is not five dots");
}

// AND IT COMPOSES WITH THE PEN SIZE: the gate is around the whole stamp, so a
// wide dashed pen puts its disc down at the same spacing rather than dotting
// the inside of one disc.  Pen 3 is the exact 3 x 3 square (above), so each
// dot of it is nine pixels and they are far enough apart not to touch.
void test_a_wide_dashed_pen_stamps_whole_discs(void)
{
    const int cx = 100, top = 100, bottom = 120;
    screen_gfx_line((float)cx, (float)top, (float)cx, (float)bottom, INK, false, 3, 8);

    // Indices 7 and 15 of 0..20, so y = 107 and 115, each a full 3 x 3.
    static const int centres[2] = { 107, 115 };
    for (int i = 0; i < 2; i++)
        for (int y = centres[i] - 1; y <= centres[i] + 1; y++)
            for (int x = cx - 1; x <= cx + 1; x++)
            {
                char msg[80];
                snprintf(msg, sizeof(msg), "the disc at y=%d is missing %d,%d",
                         centres[i], x, y);
                TEST_ASSERT_TRUE_MESSAGE(inked(x, y), msg);
            }

    TEST_ASSERT_EQUAL_INT_MESSAGE(2 * 9, inked_in(cx - 2, top - 2, cx + 2, bottom + 2),
        "a pen-3 dashed stroke is not exactly two 3 x 3 discs");
}

int main(void)
{
    UNITY_BEGIN();

    // P18 M2: the dashed pen
    RUN_TEST(test_a_dash_of_one_is_a_solid_stroke);
    RUN_TEST(test_a_dashed_vertical_stroke_plots_the_roms_span);
    RUN_TEST(test_a_dashed_horizontal_stroke_plots_the_roms_span);
    RUN_TEST(test_a_stroke_shorter_than_the_dash_period_draws_nothing);
    RUN_TEST(test_a_dashed_stroke_stores_fewer_pixels_than_a_solid_one);
    RUN_TEST(test_a_wide_dashed_pen_stamps_whole_discs);
    RUN_TEST(test_a_wide_pens_cap_is_round_and_does_not_square_off_the_stroke);
    RUN_TEST(test_pen_3_is_an_exact_square_brush);
    RUN_TEST(test_three_pen_3_strokes_cover_an_8_by_16_exactly);
    return UNITY_END();
}
