//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for the mock device used in turtle and text screen testing.
//

#include "unity.h"
#include "mock_device.h"
#include <math.h>

void setUp(void)
{
    mock_device_init();
}

void tearDown(void)
{
}

// ============================================================================
// Initial State Tests
// ============================================================================

void test_initial_turtle_position(void)
{
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, state->turtle.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, state->turtle.y);
}

void test_initial_turtle_heading(void)
{
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, state->turtle.heading);
}

void test_initial_pen_state(void)
{
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_TRUE(state->turtle.pen_down);
    TEST_ASSERT_EQUAL(MOCK_PEN_DOWN, state->turtle.pen_mode);
}

void test_initial_turtle_visibility(void)
{
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_TRUE(state->turtle.visible);
}

void test_initial_boundary_mode(void)
{
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(MOCK_BOUNDARY_WRAP, state->turtle.boundary_mode);
}

void test_initial_text_state(void)
{
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(0, state->text.cursor_col);
    TEST_ASSERT_EQUAL(0, state->text.cursor_row);
    TEST_ASSERT_EQUAL(40, state->text.width);
}

void test_initial_screen_mode(void)
{
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(MOCK_SCREEN_TEXT, state->screen_mode);
}

// ============================================================================
// Turtle Movement Tests
// ============================================================================

void test_turtle_move_forward_north(void)
{
    LogoConsole *console = mock_device_get_console();
    
    // Move forward 100 units while heading north (0 degrees)
    console->turtle->move(100.0f);
    
    // Should be at (0, 100)
    TEST_ASSERT_TRUE(mock_device_verify_position(0.0f, 100.0f, 0.01f));
}

void test_turtle_move_forward_east(void)
{
    LogoConsole *console = mock_device_get_console();
    
    // Turn to heading 90 (east)
    console->turtle->set_heading(90.0f);
    
    // Move forward 50 units
    console->turtle->move(50.0f);
    
    // Should be at (50, 0)
    TEST_ASSERT_TRUE(mock_device_verify_position(50.0f, 0.0f, 0.01f));
}

void test_turtle_move_backward(void)
{
    LogoConsole *console = mock_device_get_console();
    
    // Move backward 50 units while heading north
    console->turtle->move(-50.0f);
    
    // Should be at (0, -50)
    TEST_ASSERT_TRUE(mock_device_verify_position(0.0f, -50.0f, 0.01f));
}

void test_turtle_draw_line_when_pen_down(void)
{
    LogoConsole *console = mock_device_get_console();
    
    // Move forward 100 units with pen down
    console->turtle->move(100.0f);
    
    // Should have recorded a line
    TEST_ASSERT_EQUAL(1, mock_device_line_count());
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(0.0f, 0.0f, 0.0f, 100.0f, 0.01f));
}

void test_turtle_no_line_when_pen_up(void)
{
    LogoConsole *console = mock_device_get_console();
    
    // Lift pen
    console->turtle->set_pen_down(false);
    
    // Move forward
    console->turtle->move(100.0f);
    
    // Should have no lines
    TEST_ASSERT_EQUAL(0, mock_device_line_count());
}

// ============================================================================
// Heading Tests
// ============================================================================

void test_turtle_set_heading(void)
{
    LogoConsole *console = mock_device_get_console();
    
    console->turtle->set_heading(45.0f);
    TEST_ASSERT_TRUE(mock_device_verify_heading(45.0f, 0.01f));
    
    console->turtle->set_heading(180.0f);
    TEST_ASSERT_TRUE(mock_device_verify_heading(180.0f, 0.01f));
    
    console->turtle->set_heading(270.0f);
    TEST_ASSERT_TRUE(mock_device_verify_heading(270.0f, 0.01f));
}

void test_heading_normalization(void)
{
    LogoConsole *console = mock_device_get_console();
    
    // Heading should be normalized to [0, 360)
    console->turtle->set_heading(360.0f);
    TEST_ASSERT_TRUE(mock_device_verify_heading(0.0f, 0.01f));
    
    console->turtle->set_heading(450.0f);
    TEST_ASSERT_TRUE(mock_device_verify_heading(90.0f, 0.01f));
    
    console->turtle->set_heading(-90.0f);
    TEST_ASSERT_TRUE(mock_device_verify_heading(270.0f, 0.01f));
}

// ============================================================================
// Home Command Tests
// ============================================================================

void test_turtle_home(void)
{
    LogoConsole *console = mock_device_get_console();
    
    // Move somewhere else
    console->turtle->move(50.0f);
    console->turtle->set_heading(90.0f);
    
    // Go home
    console->turtle->home();
    
    // Should be at origin with heading 0
    TEST_ASSERT_TRUE(mock_device_verify_position(0.0f, 0.0f, 0.01f));
    TEST_ASSERT_TRUE(mock_device_verify_heading(0.0f, 0.01f));
}

// ============================================================================
// Set Position Tests
// ============================================================================

void test_turtle_set_position(void)
{
    LogoConsole *console = mock_device_get_console();
    
    console->turtle->set_position(100.0f, 50.0f);
    TEST_ASSERT_TRUE(mock_device_verify_position(100.0f, 50.0f, 0.01f));
}

void test_turtle_set_position_draws_line(void)
{
    LogoConsole *console = mock_device_get_console();
    
    console->turtle->set_position(100.0f, 50.0f);
    
    // Should have drawn a line from (0,0) to (100,50)
    TEST_ASSERT_EQUAL(1, mock_device_line_count());
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(0.0f, 0.0f, 100.0f, 50.0f, 0.01f));
}

// ============================================================================
// Pen State Tests
// ============================================================================

void test_pen_down_state(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->turtle->set_pen_down(true);
    TEST_ASSERT_TRUE(state->turtle.pen_down);
    
    console->turtle->set_pen_down(false);
    TEST_ASSERT_FALSE(state->turtle.pen_down);
}

// ============================================================================
// Visibility Tests
// ============================================================================

void test_turtle_visibility(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->turtle->set_visible(false);
    TEST_ASSERT_FALSE(state->turtle.visible);
    
    console->turtle->set_visible(true);
    TEST_ASSERT_TRUE(state->turtle.visible);
}

// ============================================================================
// Colour Tests
// ============================================================================

void test_pen_colour(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->turtle->set_pen_colour(42);
    TEST_ASSERT_EQUAL(42, state->turtle.pen_colour);
    TEST_ASSERT_EQUAL(42, console->turtle->get_pen_colour());
}

void test_background_colour(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->turtle->set_bg_colour(7);
    TEST_ASSERT_EQUAL(7, state->turtle.bg_colour);
    TEST_ASSERT_EQUAL(7, console->turtle->get_bg_colour());
}

// ============================================================================
// Dot Tests
// ============================================================================

void test_draw_dot(void)
{
    LogoConsole *console = mock_device_get_console();
    
    console->turtle->dot(50.0f, 75.0f);
    
    TEST_ASSERT_EQUAL(1, mock_device_dot_count());
    TEST_ASSERT_TRUE(mock_device_has_dot_at(50.0f, 75.0f, 0.01f));
}

void test_dot_at_query(void)
{
    LogoConsole *console = mock_device_get_console();
    
    console->turtle->dot(100.0f, 100.0f);
    
    TEST_ASSERT_TRUE(console->turtle->dot_at(100.0f, 100.0f));
    TEST_ASSERT_FALSE(console->turtle->dot_at(0.0f, 0.0f));
}

// ============================================================================
// Boundary Mode Tests
// ============================================================================

void test_fence_mode_blocks_movement(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->turtle->set_fence();
    
    // Try to move beyond boundary (160 is the edge)
    console->turtle->move(200.0f);
    
    // Should not have moved
    TEST_ASSERT_TRUE(mock_device_verify_position(0.0f, 0.0f, 0.01f));
    TEST_ASSERT_TRUE(state->boundary_error);
}

void test_window_mode_allows_offscreen(void)
{
    LogoConsole *console = mock_device_get_console();
    
    console->turtle->set_window();
    
    // Move beyond boundary
    console->turtle->move(500.0f);
    
    // Should have moved (no restriction)
    TEST_ASSERT_TRUE(mock_device_verify_position(0.0f, 500.0f, 0.01f));
}

void test_wrap_mode_wraps_around(void)
{
    LogoConsole *console = mock_device_get_console();
    
    console->turtle->set_wrap();
    
    // Move beyond boundary (160 is half width, screen is 320 wide)
    console->turtle->move(200.0f);  // Goes to y=200, which wraps
    
    // Should have wrapped (200 - 320 = -120)
    TEST_ASSERT_TRUE(mock_device_verify_position(0.0f, -120.0f, 0.01f));
}

// ============================================================================
// Text Screen Tests
// ============================================================================

void test_text_set_cursor(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->text->set_cursor(10, 5);
    
    TEST_ASSERT_EQUAL(10, state->text.cursor_col);
    TEST_ASSERT_EQUAL(5, state->text.cursor_row);
}

void test_text_get_cursor(void)
{
    LogoConsole *console = mock_device_get_console();
    
    console->text->set_cursor(20, 15);
    
    uint8_t col, row;
    console->text->get_cursor(&col, &row);
    
    TEST_ASSERT_EQUAL(20, col);
    TEST_ASSERT_EQUAL(15, row);
}

void test_text_set_width(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->text->set_width(64);
    TEST_ASSERT_EQUAL(64, state->text.width);
    TEST_ASSERT_EQUAL(64, console->text->get_width());
    
    console->text->set_width(40);
    TEST_ASSERT_EQUAL(40, state->text.width);
}

void test_text_clear(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->text->set_cursor(10, 10);
    console->text->clear();
    
    TEST_ASSERT_TRUE(state->text.cleared);
    TEST_ASSERT_EQUAL(0, state->text.cursor_col);
    TEST_ASSERT_EQUAL(0, state->text.cursor_row);
}

// ============================================================================
// Screen Mode Tests
// ============================================================================

void test_fullscreen_mode(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->screen->fullscreen();
    TEST_ASSERT_EQUAL(MOCK_SCREEN_FULLSCREEN, state->screen_mode);
}

void test_splitscreen_mode(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->screen->splitscreen();
    TEST_ASSERT_EQUAL(MOCK_SCREEN_SPLIT, state->screen_mode);
}

void test_textscreen_mode(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    console->screen->fullscreen();  // First change to something else
    console->screen->textscreen();
    TEST_ASSERT_EQUAL(MOCK_SCREEN_TEXT, state->screen_mode);
}

// ============================================================================
// Command History Tests
// ============================================================================

void test_command_history_records_moves(void)
{
    LogoConsole *console = mock_device_get_console();
    
    mock_device_clear_commands();
    
    console->turtle->move(100.0f);
    
    TEST_ASSERT_EQUAL(1, mock_device_command_count());
    const MockCommand *cmd = mock_device_last_command();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(MOCK_CMD_MOVE, cmd->type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, cmd->params.distance);
}

void test_command_history_records_heading(void)
{
    LogoConsole *console = mock_device_get_console();
    
    mock_device_clear_commands();
    
    console->turtle->set_heading(90.0f);
    
    const MockCommand *cmd = mock_device_last_command();
    TEST_ASSERT_EQUAL(MOCK_CMD_SET_HEADING, cmd->type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, cmd->params.heading);
}

void test_clear_commands(void)
{
    LogoConsole *console = mock_device_get_console();
    
    console->turtle->move(50.0f);
    console->turtle->move(50.0f);
    
    TEST_ASSERT_GREATER_THAN(0, mock_device_command_count());
    
    mock_device_clear_commands();
    
    TEST_ASSERT_EQUAL(0, mock_device_command_count());
}

// ============================================================================
// Reset Tests
// ============================================================================

void test_reset_restores_defaults(void)
{
    LogoConsole *console = mock_device_get_console();
    const MockDeviceState *state = mock_device_get_state();
    
    // Change various things
    console->turtle->move(100.0f);
    console->turtle->set_heading(90.0f);
    console->turtle->set_visible(false);
    console->text->set_cursor(10, 10);
    console->screen->fullscreen();
    
    // Reset
    mock_device_reset();
    
    // Verify defaults restored
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, state->turtle.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, state->turtle.y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, state->turtle.heading);
    TEST_ASSERT_TRUE(state->turtle.visible);
    TEST_ASSERT_TRUE(state->turtle.pen_down);
    TEST_ASSERT_EQUAL(0, state->text.cursor_col);
    TEST_ASSERT_EQUAL(0, state->text.cursor_row);
    TEST_ASSERT_EQUAL(MOCK_SCREEN_TEXT, state->screen_mode);
    TEST_ASSERT_EQUAL(0, mock_device_command_count());
    TEST_ASSERT_EQUAL(0, mock_device_line_count());
    TEST_ASSERT_EQUAL(0, mock_device_dot_count());
}

// ============================================================================
// Console Capability Tests
// ============================================================================

void test_console_has_turtle(void)
{
    LogoConsole *console = mock_device_get_console();
    TEST_ASSERT_TRUE(logo_console_has_turtle(console));
}

void test_console_has_text(void)
{
    LogoConsole *console = mock_device_get_console();
    TEST_ASSERT_TRUE(logo_console_has_text(console));
}

void test_console_has_screen_modes(void)
{
    LogoConsole *console = mock_device_get_console();
    TEST_ASSERT_TRUE(logo_console_has_screen_modes(console));
}

// ============================================================================
// Main
// ============================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Initial state
    RUN_TEST(test_initial_turtle_position);
    RUN_TEST(test_initial_turtle_heading);
    RUN_TEST(test_initial_pen_state);
    RUN_TEST(test_initial_turtle_visibility);
    RUN_TEST(test_initial_boundary_mode);
    RUN_TEST(test_initial_text_state);
    RUN_TEST(test_initial_screen_mode);
    
    // Turtle movement
    RUN_TEST(test_turtle_move_forward_north);
    RUN_TEST(test_turtle_move_forward_east);
    RUN_TEST(test_turtle_move_backward);
    RUN_TEST(test_turtle_draw_line_when_pen_down);
    RUN_TEST(test_turtle_no_line_when_pen_up);
    
    // Heading
    RUN_TEST(test_turtle_set_heading);
    RUN_TEST(test_heading_normalization);
    
    // Home
    RUN_TEST(test_turtle_home);
    
    // Set position
    RUN_TEST(test_turtle_set_position);
    RUN_TEST(test_turtle_set_position_draws_line);
    
    // Pen state
    RUN_TEST(test_pen_down_state);
    
    // Visibility
    RUN_TEST(test_turtle_visibility);
    
    // Colours
    RUN_TEST(test_pen_colour);
    RUN_TEST(test_background_colour);
    
    // Dots
    RUN_TEST(test_draw_dot);
    RUN_TEST(test_dot_at_query);
    
    // Boundary modes
    RUN_TEST(test_fence_mode_blocks_movement);
    RUN_TEST(test_window_mode_allows_offscreen);
    RUN_TEST(test_wrap_mode_wraps_around);
    
    // Text screen
    RUN_TEST(test_text_set_cursor);
    RUN_TEST(test_text_get_cursor);
    RUN_TEST(test_text_set_width);
    RUN_TEST(test_text_clear);
    
    // Screen modes
    RUN_TEST(test_fullscreen_mode);
    RUN_TEST(test_splitscreen_mode);
    RUN_TEST(test_textscreen_mode);
    
    // Command history
    RUN_TEST(test_command_history_records_moves);
    RUN_TEST(test_command_history_records_heading);
    RUN_TEST(test_clear_commands);
    
    // Reset
    RUN_TEST(test_reset_restores_defaults);
    
    // Console capabilities
    RUN_TEST(test_console_has_turtle);
    RUN_TEST(test_console_has_text);
    RUN_TEST(test_console_has_screen_modes);
    
    return UNITY_END();
}
