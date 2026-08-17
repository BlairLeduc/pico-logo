//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Where a screen row lands in frame memory when the panel is scrolling.
//
//  The three regions in use: the console scrolls with nothing fixed, split
//  mode fixes the graphics half at the top, and the editor fixes a header row
//  at the top and a footer row at the bottom. The fixed areas are measured
//  against the 480 rows of frame memory rather than the 320 rows on display,
//  which is what makes the editor's bottom fixed area 170 and not 10.
//

#include <stdbool.h>
#include <stdint.h>

#include "unity.h"
#include "lcd.h"

// The editor's region: a header row fixed at the top, a footer row at the
// bottom, and thirty content rows scrolling between them.
#define ED_TOP (10)
#define ED_MEM (300)

void setUp(void) {}
void tearDown(void) {}

// Full-screen text: the whole frame memory scrolls, nothing is fixed.
static void test_full_screen_region(void)
{
    // Unscrolled, every row maps to itself
    TEST_ASSERT_EQUAL_UINT16(0, lcd_scroll_map_row(0, 0, 480, 0));
    TEST_ASSERT_EQUAL_UINT16(310, lcd_scroll_map_row(310, 0, 480, 0));

    // One line down, rows move a glyph further into memory
    TEST_ASSERT_EQUAL_UINT16(10, lcd_scroll_map_row(0, 0, 480, 10));
    TEST_ASSERT_EQUAL_UINT16(320, lcd_scroll_map_row(310, 0, 480, 10));

    // And wrap at the end of frame memory
    TEST_ASSERT_EQUAL_UINT16(0, lcd_scroll_map_row(10, 0, 480, 470));
}

// Split mode: the graphics half is a 240 pixel top fixed area.
static void test_split_region(void)
{
    // The graphics half never moves
    TEST_ASSERT_EQUAL_UINT16(0, lcd_scroll_map_row(0, 240, 240, 10));
    TEST_ASSERT_EQUAL_UINT16(239, lcd_scroll_map_row(239, 240, 240, 10));

    // The text half is an identity map until it scrolls
    TEST_ASSERT_EQUAL_UINT16(240, lcd_scroll_map_row(240, 240, 240, 0));
    TEST_ASSERT_EQUAL_UINT16(310, lcd_scroll_map_row(310, 240, 240, 0));

    // One line down, and a wrap back to the top of the region
    TEST_ASSERT_EQUAL_UINT16(250, lcd_scroll_map_row(240, 240, 240, 10));
    TEST_ASSERT_EQUAL_UINT16(240, lcd_scroll_map_row(250, 240, 240, 230));
}

// The editor: a top fixed area that is not a whole number of scrolling areas,
// so it has to be subtracted before wrapping, and a bottom fixed area that
// puts the footer row outside the region entirely.
static void test_editor_region(void)
{
    // The header never moves
    TEST_ASSERT_EQUAL_UINT16(0, lcd_scroll_map_row(0, ED_TOP, ED_MEM, 20));
    TEST_ASSERT_EQUAL_UINT16(9, lcd_scroll_map_row(9, ED_TOP, ED_MEM, 20));

    // Neither does the footer, whatever the content has scrolled to
    TEST_ASSERT_EQUAL_UINT16(310, lcd_scroll_map_row(310, ED_TOP, ED_MEM, 0));
    TEST_ASSERT_EQUAL_UINT16(310, lcd_scroll_map_row(310, ED_TOP, ED_MEM, 20));
    TEST_ASSERT_EQUAL_UINT16(319, lcd_scroll_map_row(319, ED_TOP, ED_MEM, 290));

    // Unscrolled, the content rows map to themselves
    TEST_ASSERT_EQUAL_UINT16(10, lcd_scroll_map_row(10, ED_TOP, ED_MEM, 0));
    TEST_ASSERT_EQUAL_UINT16(300, lcd_scroll_map_row(300, ED_TOP, ED_MEM, 0));

    // One line down: the top content row follows the offset and the bottom one
    // wraps to the row that just left the top of the region
    TEST_ASSERT_EQUAL_UINT16(20, lcd_scroll_map_row(10, ED_TOP, ED_MEM, 10));
    TEST_ASSERT_EQUAL_UINT16(10, lcd_scroll_map_row(300, ED_TOP, ED_MEM, 10));

    // The last offset before the region wraps
    TEST_ASSERT_EQUAL_UINT16(300, lcd_scroll_map_row(10, ED_TOP, ED_MEM, 290));
}

// Whatever the offset, the content rows have to land on distinct rows of frame
// memory inside the region: a scroll may reorder rows, never lose one.
static void test_editor_region_is_one_to_one(void)
{
    for (uint16_t offset = 0; offset < ED_MEM; offset += 10)
    {
        bool seen[480] = {false};

        for (uint16_t y = ED_TOP; y < ED_TOP + ED_MEM; y += 10)
        {
            uint16_t row = lcd_scroll_map_row(y, ED_TOP, ED_MEM, offset);

            TEST_ASSERT_TRUE(row >= ED_TOP && row < ED_TOP + ED_MEM);
            TEST_ASSERT_FALSE(seen[row]);
            seen[row] = true;
        }
    }
}

// A region of no height maps every row to itself.
static void test_no_scrolling_area(void)
{
    TEST_ASSERT_EQUAL_UINT16(100, lcd_scroll_map_row(100, 0, 0, 20));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_full_screen_region);
    RUN_TEST(test_split_region);
    RUN_TEST(test_editor_region);
    RUN_TEST(test_editor_region_is_one_to_one);
    RUN_TEST(test_no_scrolling_area);
    return UNITY_END();
}
