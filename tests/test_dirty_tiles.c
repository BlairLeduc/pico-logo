//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the tile-based dirty-region tracker
//  (devices/picocalc/dirty_tiles.c). Verifies tile quantization, span
//  union within a row, clamping, wrap-mode rect splitting, and span
//  iteration -- the properties the screen driver's flush loop relies on.
//

#include "unity.h"
#include "dirty_tiles.h"

static DirtyTiles dt;

void setUp(void)
{
    dirty_tiles_clear(&dt);
}

void tearDown(void) {}

// Collect all spans into arrays; returns the count.
static int collect_spans(int x0[], int y0[], int x1[], int y1[], int max)
{
    int row = 0, n = 0;
    while (n < max && dirty_tiles_next_span(&dt, &row,
                                            &x0[n], &y0[n], &x1[n], &y1[n]))
    {
        n++;
    }
    return n;
}

void test_initially_clean(void)
{
    TEST_ASSERT_FALSE(dirty_tiles_any(&dt));
    int row = 0, a, b, c, d;
    TEST_ASSERT_FALSE(dirty_tiles_next_span(&dt, &row, &a, &b, &c, &d));
}

void test_small_rect_quantizes_to_one_tile(void)
{
    // Pixels 17..18 x 5..6 live entirely in tile (1, 0)
    dirty_tiles_mark_rect(&dt, 17, 5, 18, 6);
    TEST_ASSERT_TRUE(dirty_tiles_any(&dt));

    int x0[4], y0[4], x1[4], y1[4];
    int n = collect_spans(x0, y0, x1, y1, 4);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(16, x0[0]);
    TEST_ASSERT_EQUAL_INT(31, x1[0]);
    TEST_ASSERT_EQUAL_INT(0, y0[0]);
    TEST_ASSERT_EQUAL_INT(15, y1[0]);
}

void test_rect_spanning_tiles_covers_them_all(void)
{
    // 15..16 crosses the tile-column boundary; 15..16 crosses the row one
    dirty_tiles_mark_rect(&dt, 15, 15, 16, 16);

    int x0[4], y0[4], x1[4], y1[4];
    int n = collect_spans(x0, y0, x1, y1, 4);
    TEST_ASSERT_EQUAL_INT(2, n); // two tile rows
    TEST_ASSERT_EQUAL_INT(0, x0[0]);
    TEST_ASSERT_EQUAL_INT(31, x1[0]);
    TEST_ASSERT_EQUAL_INT(0, y0[0]);
    TEST_ASSERT_EQUAL_INT(15, y1[0]);
    TEST_ASSERT_EQUAL_INT(16, y0[1]);
    TEST_ASSERT_EQUAL_INT(31, y1[1]);
}

void test_two_rects_same_row_union_the_span(void)
{
    dirty_tiles_mark_rect(&dt, 0, 0, 3, 3);     // tile col 0
    dirty_tiles_mark_rect(&dt, 300, 0, 310, 3); // tile cols 18..19

    int x0[4], y0[4], x1[4], y1[4];
    int n = collect_spans(x0, y0, x1, y1, 4);
    TEST_ASSERT_EQUAL_INT(1, n);
    // Span over-approximates: whole row width
    TEST_ASSERT_EQUAL_INT(0, x0[0]);
    TEST_ASSERT_EQUAL_INT(319, x1[0]);
}

void test_rects_in_different_rows_stay_separate(void)
{
    dirty_tiles_mark_rect(&dt, 0, 0, 3, 3);       // tile row 0, col 0
    dirty_tiles_mark_rect(&dt, 304, 304, 310, 310); // tile row 19, col 19

    int x0[4], y0[4], x1[4], y1[4];
    int n = collect_spans(x0, y0, x1, y1, 4);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_INT(0, x0[0]);
    TEST_ASSERT_EQUAL_INT(15, x1[0]);
    TEST_ASSERT_EQUAL_INT(304, x0[1]);
    TEST_ASSERT_EQUAL_INT(319, x1[1]);
    TEST_ASSERT_EQUAL_INT(304, y0[1]);
}

void test_clamping_out_of_bounds(void)
{
    dirty_tiles_mark_rect(&dt, -10, -10, 5, 5); // clamps to 0..5
    dirty_tiles_mark_rect(&dt, 315, 315, 400, 400); // clamps to 315..319

    int x0[4], y0[4], x1[4], y1[4];
    int n = collect_spans(x0, y0, x1, y1, 4);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_INT(0, x0[0]);
    TEST_ASSERT_EQUAL_INT(15, x1[0]);
    TEST_ASSERT_EQUAL_INT(304, x0[1]);
}

void test_fully_out_of_bounds_marks_nothing(void)
{
    dirty_tiles_mark_rect(&dt, 320, 0, 400, 10);
    dirty_tiles_mark_rect(&dt, 0, -20, 10, -1);
    dirty_tiles_mark_rect(&dt, 10, 10, 5, 5); // inverted = empty
    TEST_ASSERT_FALSE(dirty_tiles_any(&dt));
}

void test_mark_all_covers_everything(void)
{
    dirty_tiles_mark_all(&dt);

    int x0[24], y0[24], x1[24], y1[24];
    int n = collect_spans(x0, y0, x1, y1, 24);
    TEST_ASSERT_EQUAL_INT(DIRTY_TILE_ROWS, n);
    for (int i = 0; i < n; i++)
    {
        TEST_ASSERT_EQUAL_INT(0, x0[i]);
        TEST_ASSERT_EQUAL_INT(319, x1[i]);
    }
}

void test_clear_resets(void)
{
    dirty_tiles_mark_all(&dt);
    dirty_tiles_clear(&dt);
    TEST_ASSERT_FALSE(dirty_tiles_any(&dt));
}

void test_wrap_rect_splits_across_right_edge(void)
{
    // 16x16 sprite at x=312: covers x 312..319 and x 0..7, one tile row
    dirty_tiles_mark_rect_wrap(&dt, 312, 0, 16, 16);

    int x0[4], y0[4], x1[4], y1[4];
    int n = collect_spans(x0, y0, x1, y1, 4);
    // One span per tile row, unioned: covers full width (cols 0 and 19)
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT(0, x0[0]);
    TEST_ASSERT_EQUAL_INT(319, x1[0]);
}

void test_wrap_rect_splits_across_bottom_edge(void)
{
    // Sprite at y=312: rows 312..319 (tile row 19) and 0..7 (tile row 0)
    dirty_tiles_mark_rect_wrap(&dt, 0, 312, 16, 16);

    int x0[4], y0[4], x1[4], y1[4];
    int n = collect_spans(x0, y0, x1, y1, 4);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_INT(0, y0[0]);
    TEST_ASSERT_EQUAL_INT(15, y1[0]);
    TEST_ASSERT_EQUAL_INT(304, y0[1]);
    TEST_ASSERT_EQUAL_INT(319, y1[1]);
}

void test_wrap_rect_negative_origin(void)
{
    // Sprite half off the top-left corner wraps to all four corners
    dirty_tiles_mark_rect_wrap(&dt, -8, -8, 16, 16);

    int x0[4], y0[4], x1[4], y1[4];
    int n = collect_spans(x0, y0, x1, y1, 4);
    TEST_ASSERT_EQUAL_INT(2, n); // tile rows 0 and 19
    // Both rows span cols 0 and 19 (unioned to full width)
    TEST_ASSERT_EQUAL_INT(0, x0[0]);
    TEST_ASSERT_EQUAL_INT(319, x1[0]);
    TEST_ASSERT_EQUAL_INT(0, x0[1]);
    TEST_ASSERT_EQUAL_INT(319, x1[1]);
    TEST_ASSERT_EQUAL_INT(0, y0[0]);
    TEST_ASSERT_EQUAL_INT(304, y0[1]);
}

void test_wrap_rect_on_screen_no_split(void)
{
    dirty_tiles_mark_rect_wrap(&dt, 100, 100, 16, 16);

    int x0[4], y0[4], x1[4], y1[4];
    int n = collect_spans(x0, y0, x1, y1, 4);
    TEST_ASSERT_EQUAL_INT(2, n); // y 100..115 crosses tile rows 6 and 7
    TEST_ASSERT_EQUAL_INT(96, x0[0]);
    TEST_ASSERT_EQUAL_INT(127, x1[0]); // x 100..115 crosses cols 6 and 7
}

void test_wrap_rect_oversized_covers_screen(void)
{
    dirty_tiles_mark_rect_wrap(&dt, 50, 50, 400, 400);

    int x0[24], y0[24], x1[24], y1[24];
    int n = collect_spans(x0, y0, x1, y1, 24);
    TEST_ASSERT_EQUAL_INT(DIRTY_TILE_ROWS, n);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initially_clean);
    RUN_TEST(test_small_rect_quantizes_to_one_tile);
    RUN_TEST(test_rect_spanning_tiles_covers_them_all);
    RUN_TEST(test_two_rects_same_row_union_the_span);
    RUN_TEST(test_rects_in_different_rows_stay_separate);
    RUN_TEST(test_clamping_out_of_bounds);
    RUN_TEST(test_fully_out_of_bounds_marks_nothing);
    RUN_TEST(test_mark_all_covers_everything);
    RUN_TEST(test_clear_resets);
    RUN_TEST(test_wrap_rect_splits_across_right_edge);
    RUN_TEST(test_wrap_rect_splits_across_bottom_edge);
    RUN_TEST(test_wrap_rect_negative_origin);
    RUN_TEST(test_wrap_rect_on_screen_no_split);
    RUN_TEST(test_wrap_rect_oversized_covers_screen);
    return UNITY_END();
}
