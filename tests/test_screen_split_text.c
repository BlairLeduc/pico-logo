//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Where the text screen's lines are in split mode (B49).
//
//  The reference has always said what split mode shows: "you can only see the
//  top 24 lines of the graphics screen (240 turtle units) and the bottom eight
//  lines of the text screen", with `setcursor` addressing lines 0 to 31. So
//  line 26 is the third of the eight visible ones, and `setcursor [0 26]` puts
//  text where you can see it.
//
//  screen.c did something else. The visible eight rows were a window that
//  FLOATED: `start_row` was computed from `text_row`, "the last row written
//  to", which only ever advanced when a newline pushed the cursor past the
//  window's bottom edge. `setcursor` did not move that window and nothing else
//  did either. Two things followed, and a per-frame status line in Battlezone
//  M1 hit both at once:
//
//    * Text positioned above the window went into the buffer and never onto
//      the panel, so it was simply invisible.
//    * Every newline below the window's bottom edge scrolled the panel by one
//      row and dragged the window down by one, whether or not the buffer had
//      anything left to scroll -- so a status line rewritten every frame
//      scrolled the display continuously and never appeared where it was put.
//
//  These tests drive the real screen.c against tests/fake_lcd.c, which now
//  records the text plane, and pin the mapping rather than the symptom: LCD
//  text row R shows buffer line R, for R in 24..31, always.
//

#include "unity.h"
#include "fake_lcd.h"
#include "screen.h"

#include <string.h>

// The eight lines the reference says you can see.
#define FIRST_VISIBLE SCREEN_SPLIT_TXT_ROW
#define LAST_VISIBLE  (SCREEN_ROWS - 1)

void setUp(void)
{
    fake_lcd_reset();
    screen_init();
    screen_set_mode(SCREEN_MODE_SPLIT);
    screen_txt_clear();
    fake_lcd_reset();
    // fake_lcd_reset clears the recorded scroll region along with everything
    // else, so re-arm it the way split mode does.
    screen_set_mode(SCREEN_MODE_TXT);
    screen_set_mode(SCREEN_MODE_SPLIT);
}

void tearDown(void) {}

static void put(const char *s)
{
    screen_txt_puts(s);
}

// Read a whole LCD text row back as a string, trimmed of trailing blanks.
static void row_text(int row, char *out, size_t cap)
{
    size_t n = 0;
    for (int c = 0; c < SCREEN_COLUMNS && n + 1 < cap; c++)
    {
        uint8_t ch = fake_lcd_text_char(c, row);
        out[n++] = (ch == 0) ? ' ' : (char)ch;
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = '\0';
}

//==========================================================================
// The mapping
//==========================================================================

// The whole bug in one test. `setcursor [0 26]` then text, and the text has to
// be on LCD row 26.
void test_text_positioned_in_the_visible_band_lands_on_that_row(void)
{
    screen_txt_set_cursor(0, 26);
    put("POS 800 800");

    char line[64];
    row_text(26, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("POS 800 800", line,
                                     "setcursor to a visible line did not put text on that line");
}

// Every one of the eight, because an off-by-one at either end is exactly the
// shape the floating window had.
void test_every_visible_line_maps_to_itself(void)
{
    for (int row = FIRST_VISIBLE; row <= LAST_VISIBLE; row++)
    {
        screen_txt_set_cursor(0, row);
        char one[2] = { (char)('A' + row - FIRST_VISIBLE), '\0' };
        put(one);
    }

    for (int row = FIRST_VISIBLE; row <= LAST_VISIBLE; row++)
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "line %d is not on LCD row %d", row, row);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)('A' + row - FIRST_VISIBLE),
                                        fake_lcd_text_char(0, row), msg);
    }
}

// The other half: lines above the band are still in the buffer and still not
// on the panel, which is what "the bottom eight lines" means.
void test_a_line_above_the_band_is_not_drawn(void)
{
    // From a clean panel, so that what is asserted is what THIS write drew and
    // not what the mode switch in setUp repainted.
    fake_lcd_reset();
    screen_txt_set_cursor(0, 10);
    put("hidden");

    for (int row = 0; row < SCREEN_ROWS; row++)
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "row %d was drawn", row);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, fake_lcd_text_char(0, row), msg);
    }

    // ...and switching to the full text screen shows it, because the buffer
    // kept it.
    screen_set_mode(SCREEN_MODE_TXT);
    char line[64];
    row_text(10, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("hidden", line);
}

//==========================================================================
// Scrolling
//==========================================================================

// The symptom that was reported: a status line rewritten every frame scrolled
// the display continuously. Nothing here reaches the bottom line, so nothing
// may scroll.
void test_rewriting_a_status_line_never_scrolls(void)
{
    for (int frame = 0; frame < 60; frame++)
    {
        screen_txt_set_cursor(0, 26);
        put("POS 800 800\n");
        screen_txt_set_cursor(0, 27);
        put("HDG 0\n");
    }

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, fake_lcd_scroll_up_count(),
                                  "a status line above the last row scrolled the display");

    char line[64];
    row_text(26, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("POS 800 800", line);
    row_text(27, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("HDG 0", line);
}

// Scrolling still has to happen, and it happens where the text screen ends --
// at line 31 -- not at the top of the visible band.
void test_a_newline_on_the_last_line_scrolls_once(void)
{
    screen_txt_set_cursor(0, LAST_VISIBLE);
    put("bottom");
    TEST_ASSERT_EQUAL_INT(0, fake_lcd_scroll_up_count());

    put("\n");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, fake_lcd_scroll_up_count(),
                                  "a newline on the last line did not scroll");

    // What was on the last line is now on the one above it, on the panel and
    // in the buffer alike.
    char line[64];
    row_text(LAST_VISIBLE - 1, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("bottom", line);

    uint8_t col, row;
    screen_txt_get_cursor(&col, &row);
    TEST_ASSERT_EQUAL_UINT8(0, col);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(LAST_VISIBLE, row, "the cursor left the last line");
}

// A run of output longer than the band scrolls once a line and keeps the last
// seven above a fresh one, which is what a REPL in split mode does all day.
void test_a_long_run_of_output_keeps_the_last_lines(void)
{
    screen_txt_set_cursor(0, LAST_VISIBLE);
    for (int i = 0; i < 20; i++)
    {
        char one[8];
        snprintf(one, sizeof(one), "%d\n", i);
        put(one);
    }

    // Every line was written on the last row and then pushed up one by its own
    // newline, so "19" ends one above the bottom and the band holds 13..19.
    TEST_ASSERT_EQUAL_INT(20, fake_lcd_scroll_up_count());
    for (int i = 0; i < 7; i++)
    {
        char line[64], want[8], msg[64];
        row_text(FIRST_VISIBLE + i, line, sizeof(line));
        snprintf(want, sizeof(want), "%d", 13 + i);
        snprintf(msg, sizeof(msg), "LCD row %d", FIRST_VISIBLE + i);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(want, line, msg);
    }

    // The last row is the fresh one the cursor is sitting on.
    char line[64];
    row_text(LAST_VISIBLE, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("", line);
}

//==========================================================================
// Getting into the band
//==========================================================================

// `ct` in split mode has to home the cursor somewhere you can see it. Row 0
// would be seventeen invisible lines away from the top of the band.
void test_clearing_homes_the_cursor_to_the_top_of_the_band(void)
{
    screen_txt_set_cursor(4, 29);
    put("something");
    screen_txt_clear();

    uint8_t col, row;
    screen_txt_get_cursor(&col, &row);
    TEST_ASSERT_EQUAL_UINT8(0, col);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(FIRST_VISIBLE, row,
                                    "a cleared split screen homed the cursor out of sight");

    put("after");
    char line[64];
    row_text(FIRST_VISIBLE, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("after", line);
}

// And switching in from the full text screen with the cursor above the band
// has to bring the caret somewhere the typing will appear. The text above
// stays in the buffer; only the cursor moves.
void test_entering_split_mode_brings_the_cursor_into_the_band(void)
{
    screen_set_mode(SCREEN_MODE_TXT);
    screen_txt_clear();
    screen_txt_set_cursor(3, 5);

    screen_set_mode(SCREEN_MODE_SPLIT);

    uint8_t col, row;
    screen_txt_get_cursor(&col, &row);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(FIRST_VISIBLE, row,
                                    "the caret stayed above the visible band");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, col, "the column moved as well as the row");
}

// A cursor already inside the band is left exactly where it is, which is the
// case every REPL session is in after its first few lines.
void test_entering_split_mode_leaves_a_visible_cursor_alone(void)
{
    screen_set_mode(SCREEN_MODE_TXT);
    screen_txt_set_cursor(7, 29);
    screen_set_mode(SCREEN_MODE_SPLIT);

    uint8_t col, row;
    screen_txt_get_cursor(&col, &row);
    TEST_ASSERT_EQUAL_UINT8(7, col);
    TEST_ASSERT_EQUAL_UINT8(29, row);
}

//==========================================================================
// Redrawing
//==========================================================================

// screen_txt_update repaints dirty rows, and it has to repaint them onto the
// same rows the direct writes use -- the two paths disagreeing is how half a
// screen ends up one row out.
void test_a_full_repaint_puts_the_band_back_where_it_was(void)
{
    screen_txt_set_cursor(0, 25);
    put("alpha");
    screen_txt_set_cursor(0, 30);
    put("omega");

    fake_lcd_reset();
    screen_set_mode(SCREEN_MODE_TXT);
    screen_set_mode(SCREEN_MODE_SPLIT);   // marks all dirty and repaints

    char line[64];
    row_text(25, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("alpha", line);
    row_text(30, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("omega", line);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_text_positioned_in_the_visible_band_lands_on_that_row);
    RUN_TEST(test_every_visible_line_maps_to_itself);
    RUN_TEST(test_a_line_above_the_band_is_not_drawn);

    RUN_TEST(test_rewriting_a_status_line_never_scrolls);
    RUN_TEST(test_a_newline_on_the_last_line_scrolls_once);
    RUN_TEST(test_a_long_run_of_output_keeps_the_last_lines);

    RUN_TEST(test_clearing_homes_the_cursor_to_the_top_of_the_band);
    RUN_TEST(test_entering_split_mode_brings_the_cursor_into_the_band);
    RUN_TEST(test_entering_split_mode_leaves_a_visible_cursor_alone);

    RUN_TEST(test_a_full_repaint_puts_the_band_back_where_it_was);

    return UNITY_END();
}
