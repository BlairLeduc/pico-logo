//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Refresh policy of the PicoCalc screen driver: what reaches the panel
//  before the program says refresh, and what only reaches the canvas.
//
//  Compiles devices/picocalc/screen.c on the host against tests/fake_lcd.c,
//  which records the panel writes (see fake_lcd.h).
//

#include <string.h>

#include "unity.h"
#include "fake_lcd.h"
#include "screen.h"

#define DRAWN (12) // A palette index nothing else in these tests uses

void setUp(void)
{
    // screen.c holds its state statically; put it back to a known one.
    screen_gfx_set_refresh_auto(true);
    screen_set_mode(SCREEN_MODE_TXT);
    screen_gfx_clear();
    screen_set_mode(SCREEN_MODE_GFX);
    fake_lcd_reset();
}

void tearDown(void)
{
    screen_gfx_set_refresh_auto(true);
    screen_set_mode(SCREEN_MODE_TXT);
}

// Is the whole graphics area of the panel the background colour?
static bool panel_is_background(int height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < SCREEN_WIDTH; x++)
        {
            if (fake_lcd_panel_point(x, y) != GFX_DEFAULT_BACKGROUND)
            {
                return false;
            }
        }
    }
    return true;
}

// Put a mark on the canvas and present it, so the panel holds drawing that a
// later clear has something to wipe.
static void draw_and_present(void)
{
    screen_gfx_set_point(10.0f, 20.0f, DRAWN);
    screen_gfx_present();
    TEST_ASSERT_EQUAL_UINT8(DRAWN, fake_lcd_panel_point(10, 20));
}

// B16: in manual refresh mode `clean`/`cs` must not write through to the
// panel — the screen goes black ahead of the refresh that was meant to
// present the drawing replacing it.
void test_manual_clean_does_not_reach_the_panel(void)
{
    draw_and_present();
    screen_gfx_set_refresh_auto(false);

    screen_gfx_clear();

    TEST_ASSERT_EQUAL_INT(0, fake_lcd_clear_count());
    TEST_ASSERT_EQUAL_UINT8(DRAWN, fake_lcd_panel_point(10, 20));
    // The canvas is wiped even though the panel is not
    TEST_ASSERT_EQUAL_UINT8(GFX_DEFAULT_BACKGROUND, screen_gfx_frame()[20 * SCREEN_WIDTH + 10]);
}

// The wipe is deferred, not dropped: the next present carries it.
void test_manual_clean_is_presented_by_the_next_refresh(void)
{
    draw_and_present();
    screen_gfx_set_refresh_auto(false);

    screen_gfx_clear();
    screen_gfx_present();

    TEST_ASSERT_EQUAL_INT(0, fake_lcd_clear_count());
    TEST_ASSERT_TRUE(panel_is_background(SCREEN_HEIGHT));
}

// Split mode wipes the graphics area with a rectangle; same rule applies.
void test_manual_clean_in_split_mode_does_not_reach_the_panel(void)
{
    screen_set_mode(SCREEN_MODE_SPLIT);
    draw_and_present();
    screen_gfx_set_refresh_auto(false);

    screen_gfx_clear();

    TEST_ASSERT_EQUAL_INT(0, fake_lcd_rectangle_count());
    TEST_ASSERT_EQUAL_UINT8(DRAWN, fake_lcd_panel_point(10, 20));

    screen_gfx_present();

    TEST_ASSERT_TRUE(panel_is_background(SCREEN_SPLIT_GFX_HEIGHT));
    // The text area below the split is not the graphics clear's to touch
    TEST_ASSERT_EQUAL_UINT8(FAKE_LCD_UNWRITTEN,
                            fake_lcd_panel_point(0, SCREEN_SPLIT_GFX_HEIGHT));
}

// Auto mode keeps the write-through: filling the panel directly costs less
// than composing 320 rows back through the blit pipeline, and it leaves the
// canvas and panel in sync, so nothing is left for the next present.
void test_auto_clean_fills_the_panel_directly(void)
{
    draw_and_present();

    screen_gfx_clear();

    TEST_ASSERT_EQUAL_INT(1, fake_lcd_clear_count());
    TEST_ASSERT_TRUE(panel_is_background(SCREEN_HEIGHT));

    int rows_before = fake_lcd_blit_row_count();
    screen_gfx_present();
    TEST_ASSERT_EQUAL_INT(rows_before, fake_lcd_blit_row_count());
}

void test_auto_clean_in_split_mode_fills_the_graphics_area_directly(void)
{
    screen_set_mode(SCREEN_MODE_SPLIT);
    draw_and_present();

    screen_gfx_clear();

    TEST_ASSERT_EQUAL_INT(1, fake_lcd_rectangle_count());
    TEST_ASSERT_TRUE(panel_is_background(SCREEN_SPLIT_GFX_HEIGHT));

    int rows_before = fake_lcd_blit_row_count();
    screen_gfx_present();
    TEST_ASSERT_EQUAL_INT(rows_before, fake_lcd_blit_row_count());
}

// A clear-and-redraw frame is the point of the fix: the panel must go from
// the old frame to the new one, never through black.
void test_a_manual_clear_and_redraw_frame_never_shows_black(void)
{
    draw_and_present();
    screen_gfx_set_refresh_auto(false);

    screen_gfx_clear();
    screen_gfx_set_point(30.0f, 40.0f, DRAWN);

    // Mid-frame: the panel still holds the previous frame, untouched — the
    // old mark is still there and the new one has not appeared yet
    TEST_ASSERT_EQUAL_UINT8(DRAWN, fake_lcd_panel_point(10, 20));
    TEST_ASSERT_NOT_EQUAL_UINT8(DRAWN, fake_lcd_panel_point(30, 40));

    screen_gfx_present();

    TEST_ASSERT_EQUAL_UINT8(GFX_DEFAULT_BACKGROUND, fake_lcd_panel_point(10, 20));
    TEST_ASSERT_EQUAL_UINT8(DRAWN, fake_lcd_panel_point(30, 40));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_manual_clean_does_not_reach_the_panel);
    RUN_TEST(test_manual_clean_is_presented_by_the_next_refresh);
    RUN_TEST(test_manual_clean_in_split_mode_does_not_reach_the_panel);
    RUN_TEST(test_auto_clean_fills_the_panel_directly);
    RUN_TEST(test_auto_clean_in_split_mode_fills_the_graphics_area_directly);
    RUN_TEST(test_a_manual_clear_and_redraw_frame_never_shows_black);
    return UNITY_END();
}
