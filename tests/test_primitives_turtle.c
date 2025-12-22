//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for turtle graphics primitives
//

#include "test_scaffold.h"
#include "mock_device.h"
#include <math.h>
#include <string.h>

//==========================================================================
// Test setup/teardown
//==========================================================================

void setUp(void)
{
    test_scaffold_setUp_with_device();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Helper macros
//==========================================================================

#define TOLERANCE 0.001f

#define ASSERT_POSITION(x, y) \
    TEST_ASSERT_TRUE_MESSAGE(mock_device_verify_position(x, y, TOLERANCE), \
        "Position mismatch")

#define ASSERT_HEADING(h) \
    TEST_ASSERT_TRUE_MESSAGE(mock_device_verify_heading(h, TOLERANCE), \
        "Heading mismatch")

//==========================================================================
// Movement Tests
//==========================================================================

void test_forward_moves_turtle(void)
{
    // Turtle starts at (0,0) heading 0 (north)
    Result r = run_string("forward 50");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(0, 50);  // Moved north
}

void test_fd_alias(void)
{
    Result r = run_string("fd 100");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(0, 100);
}

void test_back_moves_turtle_backward(void)
{
    Result r = run_string("back 50");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(0, -50);  // Moved south (backward)
}

void test_bk_alias(void)
{
    Result r = run_string("bk 30");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(0, -30);
}

void test_forward_with_heading(void)
{
    // Turn east (90 degrees) then move forward
    run_string("right 90");
    Result r = run_string("forward 50");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(50, 0);  // Moved east
}

void test_home_resets_position_and_heading(void)
{
    // Move turtle away from home
    run_string("forward 100");
    run_string("right 45");
    
    Result r = run_string("home");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(0, 0);
    ASSERT_HEADING(0);
}

void test_setpos_moves_to_coordinates(void)
{
    Result r = run_string("setpos [50 75]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(50, 75);
}

void test_setpos_negative_coordinates(void)
{
    // Use subtraction to create negative values since [-100 -50] 
    // parses - as subtraction, not negative sign in lists
    run_string("make \"negx (0 - 100)");
    run_string("make \"negy (0 - 50)");
    Result r = run_string("setpos (list :negx :negy)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(-100, -50);
}

void test_setx_changes_only_x(void)
{
    run_string("setpos [10 20]");
    Result r = run_string("setx 100");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(100, 20);  // y unchanged
}

void test_sety_changes_only_y(void)
{
    run_string("setpos [10 20]");
    Result r = run_string("sety 100");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(10, 100);  // x unchanged
}

void test_forward_requires_input(void)
{
    Result r = run_string("forward");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setpos_requires_list(void)
{
    Result r = run_string("setpos 50");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Rotation Tests
//==========================================================================

void test_right_turns_clockwise(void)
{
    Result r = run_string("right 90");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_HEADING(90);
}

void test_rt_alias(void)
{
    Result r = run_string("rt 45");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_HEADING(45);
}

void test_left_turns_counterclockwise(void)
{
    Result r = run_string("left 90");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // 360 - 90 = 270
    ASSERT_HEADING(270);
}

void test_lt_alias(void)
{
    Result r = run_string("lt 45");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_HEADING(315);
}

void test_setheading_sets_absolute_heading(void)
{
    Result r = run_string("setheading 180");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_HEADING(180);
}

void test_seth_alias(void)
{
    Result r = run_string("seth 270");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_HEADING(270);
}

void test_heading_wraps_at_360(void)
{
    run_string("right 400");
    ASSERT_HEADING(40);  // 400 - 360 = 40
}

void test_heading_wraps_negative(void)
{
    run_string("left 100");
    ASSERT_HEADING(260);  // 360 - 100 = 260
}

void test_right_requires_input(void)
{
    Result r = run_string("right");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Query Tests
//==========================================================================

void test_heading_outputs_current_heading(void)
{
    run_string("right 90");
    Result r = run_string("print heading");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "90") != NULL);
}

void test_pos_outputs_position_list(void)
{
    run_string("setpos [30 40]");
    Result r = run_string("print pos");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    const char *output = mock_device_get_output();
    TEST_ASSERT_TRUE(strstr(output, "30") != NULL);
    TEST_ASSERT_TRUE(strstr(output, "40") != NULL);
}

void test_xcor_outputs_x_coordinate(void)
{
    run_string("setpos [50 75]");
    Result r = run_string("print xcor");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "50") != NULL);
}

void test_ycor_outputs_y_coordinate(void)
{
    run_string("setpos [50 75]");
    Result r = run_string("print ycor");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "75") != NULL);
}

void test_towards_north(void)
{
    // Turtle at origin, target directly north
    Result r = run_string("print towards [0 100]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "0") != NULL);
}

void test_towards_east(void)
{
    Result r = run_string("print towards [100 0]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "90") != NULL);
}

void test_towards_south(void)
{
    // Use subtraction since -100 in a list is parsed as subtraction
    run_string("make \"negy (0 - 100)");
    Result r = run_string("print towards (list 0 :negy)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "180") != NULL);
}

void test_towards_west(void)
{
    Result r = run_string("print towards [-100 0]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "270") != NULL);
}

void test_towards_origin_from_north(void)
{
    // Turtle at [0 100], target is origin - should be 180 (south)
    run_string("setpos [0 100]");
    Result r = run_string("print towards [0 0]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "180") != NULL);
}

void test_towards_origin_from_east(void)
{
    // Turtle at [100 0], target is origin - should be 270 (west)
    run_string("setpos [100 0]");
    Result r = run_string("print towards [0 0]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "270") != NULL);
}

void test_towards_origin_from_south(void)
{
    // Turtle at [0 -100], target is origin - should be 0 (north)
    run_string("make \"negy (0 - 100)");
    run_string("setpos (list 0 :negy)");
    Result r = run_string("print towards [0 0]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "0") != NULL);
}

void test_towards_origin_from_west(void)
{
    // Turtle at [-100 0], target is origin - should be 90 (east)
    run_string("setpos [-100 0]");
    Result r = run_string("print towards [0 0]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "90") != NULL);
}

//==========================================================================
// Pen Control Tests
//==========================================================================

void test_pendown_puts_pen_down(void)
{
    run_string("penup");
    Result r = run_string("pendown");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(LOGO_PEN_DOWN, state->turtle.pen_state);
}

void test_pd_alias(void)
{
    run_string("pu");
    Result r = run_string("pd");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(LOGO_PEN_DOWN, state->turtle.pen_state);
}

void test_penup_lifts_pen(void)
{
    Result r = run_string("penup");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(LOGO_PEN_UP, state->turtle.pen_state);
}

void test_pu_alias(void)
{
    Result r = run_string("pu");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(LOGO_PEN_UP, state->turtle.pen_state);
}

void test_pen_outputs_pendown_when_down(void)
{
    run_string("pendown");
    Result r = run_string("print pen");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "pendown") != NULL);
}

void test_pen_outputs_penup_when_up(void)
{
    run_string("penup");
    Result r = run_string("print pen");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "penup") != NULL);
}

void test_setpc_sets_pen_color(void)
{
    Result r = run_string("setpc 7");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(7, state->turtle.pen_colour);
}

void test_setpencolor_alias(void)
{
    Result r = run_string("setpencolor 15");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(15, state->turtle.pen_colour);
}

void test_pencolor_outputs_pen_color(void)
{
    run_string("setpc 12");
    Result r = run_string("print pencolor");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "12") != NULL);
}

void test_pc_alias(void)
{
    run_string("setpc 5");
    Result r = run_string("print pc");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "5") != NULL);
}

void test_setbg_sets_background_color(void)
{
    Result r = run_string("setbg 3");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(3, state->turtle.bg_colour);
}

void test_background_outputs_bg_color(void)
{
    run_string("setbg 8");
    Result r = run_string("print background");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "8") != NULL);
}

void test_bg_alias(void)
{
    run_string("setbg 2");
    Result r = run_string("print bg");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "2") != NULL);
}

void test_penerase_command(void)
{
    Result r = run_string("penerase");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_pe_alias(void)
{
    Result r = run_string("pe");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_penreverse_command(void)
{
    Result r = run_string("penreverse");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_px_alias(void)
{
    Result r = run_string("px");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

//==========================================================================
// Visibility Tests
//==========================================================================

void test_hideturtle_hides_turtle(void)
{
    Result r = run_string("hideturtle");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_FALSE(state->turtle.visible);
}

void test_ht_alias(void)
{
    Result r = run_string("ht");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_FALSE(state->turtle.visible);
}

void test_showturtle_shows_turtle(void)
{
    run_string("hideturtle");
    Result r = run_string("showturtle");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_TRUE(state->turtle.visible);
}

void test_st_alias(void)
{
    run_string("ht");
    Result r = run_string("st");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_TRUE(state->turtle.visible);
}

void test_shownp_true_when_visible(void)
{
    run_string("showturtle");
    Result r = run_string("print shown?");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "true") != NULL);
}

void test_shownp_false_when_hidden(void)
{
    run_string("hideturtle");
    Result r = run_string("print shownp");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "false") != NULL);
}

//==========================================================================
// Screen Tests
//==========================================================================

void test_clearscreen_clears_and_homes(void)
{
    run_string("forward 100");
    run_string("right 45");
    
    Result r = run_string("clearscreen");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    ASSERT_POSITION(0, 0);
    ASSERT_HEADING(0);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_TRUE(state->graphics.cleared);
}

void test_cs_alias(void)
{
    run_string("fd 50");
    run_string("rt 90");
    
    Result r = run_string("cs");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    ASSERT_POSITION(0, 0);
    ASSERT_HEADING(0);
}

void test_clean_clears_without_moving_turtle(void)
{
    run_string("forward 100");
    run_string("right 45");
    
    Result r = run_string("clean");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Turtle should NOT have moved
    ASSERT_POSITION(0, 100);
    ASSERT_HEADING(45);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_TRUE(state->graphics.cleared);
}

//==========================================================================
// Drawing Tests
//==========================================================================

void test_dot_draws_at_position(void)
{
    Result r = run_string("dot [50 75]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    TEST_ASSERT_TRUE(mock_device_has_dot_at(50, 75, TOLERANCE));
}

void test_dotp_true_when_dot_exists(void)
{
    run_string("dot [30 40]");
    Result r = run_string("print dot? [30 40]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "true") != NULL);
}

void test_dotp_false_when_no_dot(void)
{
    Result r = run_string("print dotp [100 100]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "false") != NULL);
}

void test_fill_command(void)
{
    Result r = run_string("fill");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check that fill command was recorded
    const MockCommand *cmd = mock_device_last_command();
    TEST_ASSERT_EQUAL(MOCK_CMD_FILL, cmd->type);
}

void test_dot_requires_list(void)
{
    Result r = run_string("dot 50");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Boundary Mode Tests
//==========================================================================

void test_fence_sets_fence_mode(void)
{
    Result r = run_string("fence");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(MOCK_BOUNDARY_FENCE, state->turtle.boundary_mode);
}

void test_window_sets_window_mode(void)
{
    Result r = run_string("window");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(MOCK_BOUNDARY_WINDOW, state->turtle.boundary_mode);
}

void test_wrap_sets_wrap_mode(void)
{
    Result r = run_string("wrap");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(MOCK_BOUNDARY_WRAP, state->turtle.boundary_mode);
}

void test_fence_prevents_movement_past_boundary(void)
{
    // Set fence mode
    run_string("fence");
    
    // Try to move past the boundary (screen is 320x320, so boundary is at 160)
    Result r = run_string("forward 200");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    
    // Turtle should not have moved
    ASSERT_POSITION(0, 0);
}

void test_fence_allows_movement_within_bounds(void)
{
    // Set fence mode
    run_string("fence");
    
    // Move within bounds
    Result r = run_string("forward 100");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Turtle should have moved
    ASSERT_POSITION(0, 100);
}

void test_window_allows_movement_past_boundary(void)
{
    // Set window mode
    run_string("window");
    
    // Move past the boundary
    Result r = run_string("forward 500");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Turtle should have moved past boundary
    ASSERT_POSITION(0, 500);
}

void test_wrap_wraps_at_boundary(void)
{
    // Default mode is wrap
    run_string("wrap");
    
    // Move past the boundary (screen is 320x320, boundary at +/-160)
    Result r = run_string("forward 200");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Turtle should have wrapped around
    // 200 from center (0,0) heading north goes to y=200
    // Since boundary is at 160, 200 wraps to 200-320 = -120
    ASSERT_POSITION(0, -120);
}

void test_back_in_fence_mode_errors_at_boundary(void)
{
    // Set fence mode
    run_string("fence");
    
    // Try to move backward past the boundary
    Result r = run_string("back 200");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    
    // Turtle should not have moved
    ASSERT_POSITION(0, 0);
}

//==========================================================================
// Line Drawing Tests (pen down movement)
//==========================================================================

void test_forward_with_pendown_draws_line(void)
{
    // Pen is down by default
    Result r = run_string("forward 100");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Should have drawn line from (0,0) to (0,100)
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(0, 0, 0, 100, TOLERANCE));
}

void test_forward_with_penup_no_line(void)
{
    run_string("penup");
    run_string("forward 100");
    
    TEST_ASSERT_EQUAL(0, mock_device_line_count());
}

void test_setpos_with_pendown_draws_line(void)
{
    Result r = run_string("setpos [50 50]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(0, 0, 50, 50, TOLERANCE));
}

void test_back_with_pendown_draws_line(void)
{
    Result r = run_string("back 50");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(0, 0, 0, -50, TOLERANCE));
}

//==========================================================================
// Integration Tests
//==========================================================================

void test_draw_square(void)
{
    run_string("repeat 4 [forward 50 right 90]");
    
    // Should have 4 lines forming a square
    TEST_ASSERT_EQUAL(4, mock_device_line_count());
    
    // Should end up at starting position with same heading
    ASSERT_POSITION(0, 0);
    ASSERT_HEADING(0);
}

void test_draw_triangle(void)
{
    run_string("repeat 3 [forward 50 right 120]");
    
    TEST_ASSERT_EQUAL(3, mock_device_line_count());
    ASSERT_POSITION(0, 0);
    ASSERT_HEADING(0);
}

void test_movement_preserves_heading(void)
{
    run_string("right 45");
    run_string("forward 100");
    ASSERT_HEADING(45);  // Heading unchanged by movement
}

void test_combined_movements(void)
{
    run_string("forward 50");
    run_string("right 90");
    run_string("forward 50");
    
    // Should be at (50, 50)
    ASSERT_POSITION(50, 50);
    ASSERT_HEADING(90);
}

void test_primitives_are_registered(void)
{
    // Test a sample of primitives are registered
    const char *prims[] = {
        "forward", "fd", "back", "bk", "home",
        "left", "lt", "right", "rt", "setheading", "seth",
        "heading", "pos", "xcor", "ycor", "towards",
        "pendown", "pd", "penup", "pu", "pen",
        "setpc", "pencolor", "pc", "setbg", "background", "bg",
        "hideturtle", "ht", "showturtle", "st", "shown?", "shownp",
        "clearscreen", "cs", "clean",
        "dot", "dot?", "dotp", "fill",
        "fence", "window", "wrap",
        "setpalette", "palette", "restorepalette"
    };
    
    for (size_t i = 0; i < sizeof(prims) / sizeof(prims[0]); i++)
    {
        const Primitive *p = primitive_find(prims[i]);
        TEST_ASSERT_NOT_NULL_MESSAGE(p, prims[i]);
    }
}

//==========================================================================
// Palette Tests
//==========================================================================

void test_setpalette_sets_rgb_values(void)
{
    Result r = run_string("setpalette 128 [255 128 64]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(mock_device_verify_palette(128, 255, 128, 64));
}

void test_palette_outputs_rgb_list(void)
{
    // First set a palette value
    run_string("setpalette 200 [100 150 200]");
    
    // Then read it back
    Result r = run_string("palette 200");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    // Extract the list elements
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));
    
    // Check R value
    Node r_node = mem_car(list);
    const char *r_str = mem_word_ptr(r_node);
    TEST_ASSERT_EQUAL_STRING("100", r_str);
    
    // Check G value
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));
    Node g_node = mem_car(list);
    const char *g_str = mem_word_ptr(g_node);
    TEST_ASSERT_EQUAL_STRING("150", g_str);
    
    // Check B value
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));
    Node b_node = mem_car(list);
    const char *b_str = mem_word_ptr(b_node);
    TEST_ASSERT_EQUAL_STRING("200", b_str);
}

void test_restorepalette_resets_palette(void)
{
    // Set a custom palette value
    run_string("setpalette 50 [255 0 0]");
    
    // Restore palette
    Result r = run_string("restorepalette");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check that restore was called
    TEST_ASSERT_TRUE(mock_device_was_restore_palette_called());
}

void test_setpalette_clamps_values(void)
{
    // Test values get clamped to 0-255
    // Note: Can't use negative numbers in list as -50 parses as "- 50"
    Result r = run_string("setpalette 128 [300 0 128]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify via palette primitive to see actual stored values
    r = run_string("palette 128");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    // Extract and verify each component
    Node list = r.value.as.node;
    const char *r_str = mem_word_ptr(mem_car(list));
    TEST_ASSERT_EQUAL_STRING("255", r_str);  // 300 clamped to 255
    
    list = mem_cdr(list);
    const char *g_str = mem_word_ptr(mem_car(list));
    TEST_ASSERT_EQUAL_STRING("0", g_str);    // 0 stays 0
    
    list = mem_cdr(list);
    const char *b_str = mem_word_ptr(mem_car(list));
    TEST_ASSERT_EQUAL_STRING("128", b_str);  // 128 stays 128
}

void test_setpalette_requires_list(void)
{
    Result r = run_string("setpalette 128 \"red");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setpalette_requires_three_elements(void)
{
    // Too few elements
    Result r = run_string("setpalette 128 [255 128]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_palette_validates_slot(void)
{
    // Slot out of range (negative)
    Result r = run_string("palette -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    
    // Slot out of range (too high)
    r = run_string("palette 256");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Movement tests
    RUN_TEST(test_forward_moves_turtle);
    RUN_TEST(test_fd_alias);
    RUN_TEST(test_back_moves_turtle_backward);
    RUN_TEST(test_bk_alias);
    RUN_TEST(test_forward_with_heading);
    RUN_TEST(test_home_resets_position_and_heading);
    RUN_TEST(test_setpos_moves_to_coordinates);
    RUN_TEST(test_setpos_negative_coordinates);
    RUN_TEST(test_setx_changes_only_x);
    RUN_TEST(test_sety_changes_only_y);
    RUN_TEST(test_forward_requires_input);
    RUN_TEST(test_setpos_requires_list);
    
    // Rotation tests
    RUN_TEST(test_right_turns_clockwise);
    RUN_TEST(test_rt_alias);
    RUN_TEST(test_left_turns_counterclockwise);
    RUN_TEST(test_lt_alias);
    RUN_TEST(test_setheading_sets_absolute_heading);
    RUN_TEST(test_seth_alias);
    RUN_TEST(test_heading_wraps_at_360);
    RUN_TEST(test_heading_wraps_negative);
    RUN_TEST(test_right_requires_input);
    
    // Query tests
    RUN_TEST(test_heading_outputs_current_heading);
    RUN_TEST(test_pos_outputs_position_list);
    RUN_TEST(test_xcor_outputs_x_coordinate);
    RUN_TEST(test_ycor_outputs_y_coordinate);
    RUN_TEST(test_towards_north);
    RUN_TEST(test_towards_east);
    RUN_TEST(test_towards_south);
    RUN_TEST(test_towards_west);
    RUN_TEST(test_towards_origin_from_north);
    RUN_TEST(test_towards_origin_from_east);
    RUN_TEST(test_towards_origin_from_south);
    RUN_TEST(test_towards_origin_from_west);
    
    // Pen control tests
    RUN_TEST(test_pendown_puts_pen_down);
    RUN_TEST(test_pd_alias);
    RUN_TEST(test_penup_lifts_pen);
    RUN_TEST(test_pu_alias);
    RUN_TEST(test_pen_outputs_pendown_when_down);
    RUN_TEST(test_pen_outputs_penup_when_up);
    RUN_TEST(test_setpc_sets_pen_color);
    RUN_TEST(test_setpencolor_alias);
    RUN_TEST(test_pencolor_outputs_pen_color);
    RUN_TEST(test_pc_alias);
    RUN_TEST(test_setbg_sets_background_color);
    RUN_TEST(test_background_outputs_bg_color);
    RUN_TEST(test_bg_alias);
    RUN_TEST(test_penerase_command);
    RUN_TEST(test_pe_alias);
    RUN_TEST(test_penreverse_command);
    RUN_TEST(test_px_alias);
    
    // Visibility tests
    RUN_TEST(test_hideturtle_hides_turtle);
    RUN_TEST(test_ht_alias);
    RUN_TEST(test_showturtle_shows_turtle);
    RUN_TEST(test_st_alias);
    RUN_TEST(test_shownp_true_when_visible);
    RUN_TEST(test_shownp_false_when_hidden);
    
    // Screen tests
    RUN_TEST(test_clearscreen_clears_and_homes);
    RUN_TEST(test_cs_alias);
    RUN_TEST(test_clean_clears_without_moving_turtle);
    
    // Drawing tests
    RUN_TEST(test_dot_draws_at_position);
    RUN_TEST(test_dotp_true_when_dot_exists);
    RUN_TEST(test_dotp_false_when_no_dot);
    RUN_TEST(test_fill_command);
    RUN_TEST(test_dot_requires_list);
    
    // Boundary mode tests
    RUN_TEST(test_fence_sets_fence_mode);
    RUN_TEST(test_window_sets_window_mode);
    RUN_TEST(test_wrap_sets_wrap_mode);
    RUN_TEST(test_fence_prevents_movement_past_boundary);
    RUN_TEST(test_fence_allows_movement_within_bounds);
    RUN_TEST(test_window_allows_movement_past_boundary);
    RUN_TEST(test_wrap_wraps_at_boundary);
    RUN_TEST(test_back_in_fence_mode_errors_at_boundary);
    
    // Line drawing tests
    RUN_TEST(test_forward_with_pendown_draws_line);
    RUN_TEST(test_forward_with_penup_no_line);
    RUN_TEST(test_setpos_with_pendown_draws_line);
    RUN_TEST(test_back_with_pendown_draws_line);
    
    // Palette tests
    RUN_TEST(test_setpalette_sets_rgb_values);
    RUN_TEST(test_palette_outputs_rgb_list);
    RUN_TEST(test_restorepalette_resets_palette);
    RUN_TEST(test_setpalette_clamps_values);
    RUN_TEST(test_setpalette_requires_list);
    RUN_TEST(test_setpalette_requires_three_elements);
    RUN_TEST(test_palette_validates_slot);
    
    // Integration tests
    RUN_TEST(test_draw_square);
    RUN_TEST(test_draw_triangle);
    RUN_TEST(test_movement_preserves_heading);
    RUN_TEST(test_combined_movements);
    RUN_TEST(test_primitives_are_registered);
    
    return UNITY_END();
}
