//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for turtle graphics primitives
//

#include "test_scaffold.h"
#include "mock_device.h"
#include "core/limits.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

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

// The reference has documented `(setpos xcor ycor)` alongside the list form
// all along, and the primitive was registered with arity 1, so the variadic
// spelling answered "setpos doesn't like 50 as input" (B48, docs/bugs.md).
void test_setpos_accepts_two_numbers(void)
{
    Result r = run_string("(setpos 50 75)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(50, 75);
}

// The geometric half of B48, and the reason the list form is not a workaround
// for a program that draws: `setx` then `sety` with the pen down draws an L in
// two segments, because each moves on one axis only. One statement, one
// straight line.
void test_setpos_two_number_form_draws_one_straight_line(void)
{
    run_string("pendown");
    Result r = run_string("(setpos 100 50)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    TEST_ASSERT_EQUAL(1, mock_device_line_count());
    const MockLine *line = mock_device_get_line(0);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, line->x1);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, line->y1);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 100.0f, line->x2);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 50.0f, line->y2);
}

// The memory half of B48, and the reason it was worth fixing rather than
// documenting around. `setpos list :x :y` conses its argument, so a wireframe
// drawing 70 edges a frame mints 140 cells and needs a periodic `recycle` --
// which is a visible hitch.
//
// The test is written as "does the cost grow with the iteration count?"
// rather than "is the cost zero", because `run_string` parses its line into a
// list and that parse is a fixed charge either spelling pays. Only a
// per-iteration allocation scales.
static float nodes_consumed_by(const char *logo)
{
    Result before = run_string("nodes");
    TEST_ASSERT_EQUAL(RESULT_OK, before.status);
    float free_before = 0.0f;
    TEST_ASSERT_TRUE(value_to_number(before.value, &free_before));

    run_string(logo);

    Result after = run_string("nodes");
    TEST_ASSERT_EQUAL(RESULT_OK, after.status);
    float free_after = 0.0f;
    TEST_ASSERT_TRUE(value_to_number(after.value, &free_after));

    return free_before - free_after;
}

void test_setpos_two_number_form_does_not_allocate_per_call(void)
{
    run_string("make \"x 10");
    run_string("make \"y 20");

    float few = nodes_consumed_by("repeat 10 [(setpos :x :y)]");
    float many = nodes_consumed_by("repeat 200 [(setpos :x :y)]");

    // Not equality: the first run also pays to intern the numerals it parses,
    // so it costs a little more than the second. A per-call cons would put
    // `many` some 380 cells above `few` rather than below it.
    TEST_ASSERT_TRUE_MESSAGE(many <= few,
        "the two-input form's cost must not grow with the iteration count");
}

// The contrast that makes the test above mean something: the list form is
// linear in the number of calls, which is the defect B48 describes.
void test_setpos_list_form_allocates_per_call(void)
{
    run_string("make \"x 10");
    run_string("make \"y 20");

    float few = nodes_consumed_by("repeat 10 [setpos list :x :y]");
    float many = nodes_consumed_by("repeat 200 [setpos list :x :y]");

    TEST_ASSERT_TRUE_MESSAGE(many > few + 100.0f,
        "building the argument list should cons once per call");
}

void test_setpos_rejects_too_many_inputs(void)
{
    Result r = run_string("(setpos 1 2 3)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// A list is still a list in the variadic form's presence, and a lone number
// is still an error -- the two-number branch must not make `setpos 50` mean
// "x = 50, y = whatever".
void test_setpos_list_form_survives_the_variadic_form(void)
{
    Result r = run_string("(setpos [50 75])");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(50, 75);
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

void test_heading_outputs_normalized_after_negative(void)
{
    // left 100 from 0 should give 260 (not -100)
    run_string("left 100");
    Result r = run_string("print heading");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Should print 260, not -100
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "260") != NULL);
}

void test_heading_outputs_normalized_after_large_positive(void)
{
    // right 400 should give 40 (not 400)
    run_string("right 400");
    Result r = run_string("print heading");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    // Should print 40, not 400
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "40") != NULL);
}

void test_setheading_negative_normalized(void)
{
    // setheading -90 should give 270
    run_string("setheading -90");
    Result r = run_string("print heading");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "270") != NULL);
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

void test_pensize_defaults_to_one(void)
{
    Result r = run_string("print pensize");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "1") != NULL);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(1, state->turtle.pen_size);
}

void test_setpensize_sets_pen_size(void)
{
    Result r = run_string("setpensize 5");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(5, state->turtle.pen_size);
}

void test_pensize_outputs_current_size(void)
{
    run_string("setpensize 7");
    Result r = run_string("print pensize");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "7") != NULL);
}

void test_setpensize_rounds_to_nearest_pixel(void)
{
    run_string("setpensize 3.6");
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(4, state->turtle.pen_size);
}

void test_setpensize_clamps_to_maximum(void)
{
    run_string("setpensize 1000");
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(MAX_PEN_SIZE, state->turtle.pen_size);
}

void test_setpensize_rejects_non_positive(void)
{
    Result r = run_string("setpensize 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("setpensize -2");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setpensize_records_on_drawn_line(void)
{
    run_string("setpensize 4");
    run_string("pendown");
    Result r = run_string("forward 20");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    TEST_ASSERT_EQUAL(1, mock_device_line_count());
    const MockLine *line = mock_device_get_line(0);
    TEST_ASSERT_EQUAL(4, line->pen_size);
}

// A dot is a stamp of the pen, so a wide pen puts a wide dot. The PicoCalc
// plotted a single pixel whatever the pen size until B11 (docs/bugs.md), and
// the mock recorded no pen size at all, so no test could see it.
void test_setpensize_records_on_a_dot(void)
{
    run_string("setpensize 6");
    Result r = run_string("dot [10 20]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    TEST_ASSERT_EQUAL(1, mock_device_dot_count());
    TEST_ASSERT_EQUAL(6, mock_device_get_dot(0)->pen_size);
}

void test_setbg_sets_background_color(void)
{
    Result r = run_string("setbg 3");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(3, state->turtle.bg_colour);
}

void test_setbg_accepts_zero(void)
{
    Result r = run_string("setbg 0");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(0, state->turtle.bg_colour);
}

void test_setbg_accepts_254(void)
{
    Result r = run_string("setbg 254");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(254, state->turtle.bg_colour);
}

void test_setbg_rejects_255(void)
{
    Result r = run_string("setbg 255");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setbg_rejects_negative(void)
{
    Result r = run_string("setbg -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setbg_rejects_256(void)
{
    Result r = run_string("setbg 256");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
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

void test_clearscreen_does_not_draw_line(void)
{
    // Move turtle away from home
    run_string("forward 100");
    
    // Clear the graphics state (including line count) after the forward drew a line
    mock_device_clear_graphics();
    
    // Clearscreen should not draw a line when homing
    Result r = run_string("clearscreen");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify no lines were drawn
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT(0, state->graphics.line_count);
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

void test_fence_prevents_setpos_past_boundary(void)
{
    run_string("fence");
    
    // Try to setpos past the boundary
    Result r = run_string("setpos [200 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    
    // Turtle should not have moved
    ASSERT_POSITION(0, 0);
}

void test_fence_prevents_setx_past_boundary(void)
{
    run_string("fence");
    
    // Try to setx past the boundary
    Result r = run_string("setx 200");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    
    // Turtle should not have moved
    ASSERT_POSITION(0, 0);
}

void test_fence_prevents_sety_past_boundary(void)
{
    run_string("fence");
    
    // Try to sety past the boundary
    Result r = run_string("sety 200");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    
    // Turtle should not have moved
    ASSERT_POSITION(0, 0);
}

void test_fence_allows_setpos_within_bounds(void)
{
    run_string("fence");
    
    Result r = run_string("setpos [100 50]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(100, 50);
}

void test_window_allows_setpos_past_boundary(void)
{
    run_string("window");
    
    Result r = run_string("setpos [500 300]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(500, 300);
}

void test_wrap_wraps_setpos_at_boundary(void)
{
    run_string("wrap");
    
    // setpos [180 0] from [0,0]: 180 is 20 past the right edge (160)
    // Should wrap to -140 (180 - 320 = -140)
    Result r = run_string("setpos [180 0]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(-140, 0);
}

void test_wrap_setpos_draws_correct_line(void)
{
    // In wrap mode, setpos [180 0] from [0,0] should draw a line
    // that goes right and wraps, not directly to the wrapped position
    run_string("wrap");
    
    Result r = run_string("setpos [180 0]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // The line should be from (0,0) to (180,0) (unwrapped endpoint)
    // NOT from (0,0) to (-140,0) (the wrapped endpoint)
    // The mock records a single line from old to new unwrapped position
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(1, state->graphics.line_count);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, state->graphics.lines[0].x1);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, state->graphics.lines[0].y1);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 180.0f, state->graphics.lines[0].x2);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, state->graphics.lines[0].y2);
    
    // But the turtle position should be wrapped
    ASSERT_POSITION(-140, 0);
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

void test_home_with_pendown_draws_line(void)
{
    // Move turtle away from home without drawing (pen up)
    run_string("penup");
    run_string("setpos [50 50]");
    run_string("pendown");
    
    // Clear any graphics state
    mock_device_clear_graphics();
    
    // Home should draw a line back to origin
    Result r = run_string("home");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(50, 50, 0, 0, TOLERANCE));
}

void test_setx_with_pendown_draws_line(void)
{
    Result r = run_string("setx 75");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Should draw horizontal line from (0,0) to (75,0)
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(0, 0, 75, 0, TOLERANCE));
}

void test_sety_with_pendown_draws_line(void)
{
    Result r = run_string("sety 75");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Should draw vertical line from (0,0) to (0,75)
    TEST_ASSERT_TRUE(mock_device_has_line_from_to(0, 0, 0, 75, TOLERANCE));
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
    // Set a custom palette value (slot 50 default is palette_24bit[50])
    run_string("setpalette 50 [255 0 0]");
    
    // Verify the custom value was set
    Result r = run_string("palette 50");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("255", mem_word_ptr(mem_car(list)));
    
    // Restore palette
    r = run_string("restorepalette");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Check that restore was called on the device
    TEST_ASSERT_TRUE(mock_device_was_restore_palette_called());
    
    // Verify the core palette was restored (slot 50 should no longer be [255 0 0])
    r = run_string("palette 50");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    list = r.value.as.node;
    // Slot 50 default is from palette_24bit[50] = 0x4D7C0F → r=77, g=124, b=15
    TEST_ASSERT_EQUAL_STRING("77", mem_word_ptr(mem_car(list)));
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

void test_setpalette_overwrites_previous_value(void)
{
    // Set initial palette value
    Result r = run_string("setpalette 242 [197 134 192]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(mock_device_verify_palette(242, 197, 134, 192));

    // Read it back to confirm
    r = run_string("palette 242");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("197", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("134", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("192", mem_word_ptr(mem_car(list)));

    // Now set a DIFFERENT value on the same slot
    r = run_string("setpalette 242 [198 120 221]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(mock_device_verify_palette(242, 198, 120, 221));

    // Read it back — must show the NEW values, not the old ones
    r = run_string("palette 242");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("198", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("120", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("221", mem_word_ptr(mem_car(list)));
}

//==========================================================================
// Shape Tests
//==========================================================================

void test_shape_outputs_current_shape(void)
{
    // Default shape is 0
    Result r = run_string("shape");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    float shape;
    TEST_ASSERT_TRUE(value_to_number(r.value, &shape));
    TEST_ASSERT_EQUAL_FLOAT(0, shape);
}

void test_setsh_sets_shape(void)
{
    Result r = run_string("setsh 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    // Verify shape changed
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(1, state->shape.current_shape);
}

void test_setsh_and_shape_roundtrip(void)
{
    run_string("setsh 5");
    Result r = run_string("shape");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    float shape;
    TEST_ASSERT_TRUE(value_to_number(r.value, &shape));
    TEST_ASSERT_EQUAL_FLOAT(5, shape);
}

void test_setsh_shape_0(void)
{
    run_string("setsh 5");  // Change to something else first
    Result r = run_string("setsh 0");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(0, state->shape.current_shape);
}

void test_setsh_shape_15(void)
{
    Result r = run_string("setsh 15");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(15, state->shape.current_shape);
}

void test_setsh_rejects_negative(void)
{
    Result r = run_string("setsh -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setsh_rejects_above_the_last_slot(void)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "setsh %d", LOGO_SHAPE_MAX_SLOT + 1);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string(cmd).status);

    // and the last slot itself is legal
    snprintf(cmd, sizeof(cmd), "setsh %d", LOGO_SHAPE_MAX_SLOT);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string(cmd).status);
}

void test_setsh_requires_input(void)
{
    Result r = run_string("setsh");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// A shape is rows of hex pixel pairs, `ff` transparent and `fe` the pen
// colour. These helpers build a spec and read one back, so the tests can
// talk about a picture rather than about 128 characters of quoting.

// One 8-wide row: `fill` repeated, then the rest transparent
static void shape_row(char *out, const char *fill, int fill_pixels)
{
    out[0] = '\0';
    for (int i = 0; i < 8; i++)
    {
        strcat(out, i < fill_pixels ? fill : "ff");
    }
}

// An 8x8 spec whose row r has r pixels of `fill` at its left
static const char *shape_wedge_8x8(const char *fill)
{
    static char spec[8 * 20 + 8];
    char row[17];
    strcpy(spec, "[");
    for (int r = 0; r < 8; r++)
    {
        shape_row(row, fill, r);
        strcat(spec, " ");
        strcat(spec, row);
    }
    strcat(spec, "]");
    return spec;
}

// Assert the list is exactly these row words, in order
static void assert_shape_rows(Node list, const char **rows, int count)
{
    for (int i = 0; i < count; i++)
    {
        TEST_ASSERT_FALSE(mem_is_nil(list));
        Node item = mem_car(list);
        TEST_ASSERT_TRUE(mem_is_word(item));
        TEST_ASSERT_EQUAL_size_t(strlen(rows[i]), mem_word_len(item));
        TEST_ASSERT_EQUAL_STRING_LEN(rows[i], mem_word_ptr(item), strlen(rows[i]));
        list = mem_cdr(list);
    }
    TEST_ASSERT_TRUE(mem_is_nil(list));
}

void test_putsh_sets_shape_data(void)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "putsh 1 %s", shape_wedge_8x8("0c"));
    Result r = run_string(cmd);
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    uint8_t w = 0, h = 0;
    const uint8_t *pixels = mock_device_get_shape(1, &w, &h);
    TEST_ASSERT_NOT_NULL(pixels);
    TEST_ASSERT_EQUAL_UINT8(8, w);
    TEST_ASSERT_EQUAL_UINT8(8, h);

    // Row r is r pixels of colour 12, then transparent to the edge
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            TEST_ASSERT_EQUAL_UINT8(col < row ? 0x0c : LOGO_SHAPE_TRANSPARENT,
                                    pixels[row * 8 + col]);
        }
    }
}

void test_putsh_shape_15(void)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "putsh 15 %s", shape_wedge_8x8("fe"));
    Result r = run_string(cmd);
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    uint8_t w = 0, h = 0;
    const uint8_t *pixels = mock_device_get_shape(15, &w, &h);
    TEST_ASSERT_NOT_NULL(pixels);
    TEST_ASSERT_EQUAL_UINT8(LOGO_SHAPE_PEN, pixels[7 * 8]);  // last row starts with pen
}

// A shape need not be square: width is the row length, height the count
void test_putsh_accepts_a_rectangle(void)
{
    Result r = run_string(
        "putsh 2 [ffffffffffffffffffffffffffffffff"
        "         ffffffffffffffffffffffffffffffff"
        "         ffffffffffffffffffffffffffffffff"
        "         ffffffffffffffffffffffffffffffff"
        "         ffffffffffffffffffffffffffffffff"
        "         ffffffffffffffffffffffffffffffff"
        "         ffffffffffffffffffffffffffffffff"
        "         ffffffffffffffffffffffff090909ff]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    uint8_t w = 0, h = 0;
    const uint8_t *pixels = mock_device_get_shape(2, &w, &h);
    TEST_ASSERT_NOT_NULL(pixels);
    TEST_ASSERT_EQUAL_UINT8(16, w);
    TEST_ASSERT_EQUAL_UINT8(8, h);
    TEST_ASSERT_EQUAL_UINT8(9, pixels[7 * 16 + 12]);
}

void test_putsh_rejects_shape_0(void)
{
    // Shape 0 cannot be changed (it's the line-drawn turtle)
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "putsh 0 %s", shape_wedge_8x8("0c"));
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string(cmd).status);
}

void test_putsh_rejects_negative_shape(void)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "putsh -1 %s", shape_wedge_8x8("0c"));
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string(cmd).status);
}

void test_putsh_rejects_shape_above_the_last_slot(void)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "putsh %d %s", LOGO_SHAPE_MAX_SLOT + 1, shape_wedge_8x8("0c"));
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string(cmd).status);

    // The last slot is a real slot: it takes a shape and gives it back.
    snprintf(cmd, sizeof(cmd), "putsh %d %s", LOGO_SHAPE_MAX_SLOT, shape_wedge_8x8("0c"));
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string(cmd).status);

    uint8_t w = 0, h = 0;
    TEST_ASSERT_NOT_NULL(mock_device_get_shape(LOGO_SHAPE_MAX_SLOT, &w, &h));
    TEST_ASSERT_EQUAL_UINT8(8, w);
    TEST_ASSERT_EQUAL_UINT8(8, h);
}

void test_putsh_requires_list(void)
{
    Result r = run_string("putsh 1 123");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// Every row is the same width, or there is no rectangle to store
void test_putsh_rejects_ragged_rows(void)
{
    Result r = run_string(
        "putsh 1 [ffffffffffffffff ffffffffffffffff ffffffffffffffff"
        "         ffffffffffffffff ffffffffffffffff ffffffffffffffff"
        "         ffffffffffffffff ffffffffffffff]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// Two digits a pixel, so an odd count is half a pixel short
void test_putsh_rejects_odd_length_row(void)
{
    Result r = run_string(
        "putsh 1 [fffffffffffffffff ffffffffffffffff ffffffffffffffff"
        "         ffffffffffffffff ffffffffffffffff ffffffffffffffff"
        "         ffffffffffffffff ffffffffffffffff]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_putsh_rejects_non_hex(void)
{
    Result r = run_string(
        "putsh 1 [ffffffffffffffgg ffffffffffffffff ffffffffffffffff"
        "         ffffffffffffffff ffffffffffffffff ffffffffffffffff"
        "         ffffffffffffffff ffffffffffffffff]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_putsh_rejects_shape_narrower_than_eight(void)
{
    Result r = run_string(
        "putsh 1 [ffffffffffffff ffffffffffffff ffffffffffffff"
        "         ffffffffffffff ffffffffffffff ffffffffffffff"
        "         ffffffffffffff ffffffffffffff]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_putsh_rejects_shape_shorter_than_eight(void)
{
    Result r = run_string(
        "putsh 1 [ffffffffffffffff ffffffffffffffff ffffffffffffffff"
        "         ffffffffffffffff ffffffffffffffff ffffffffffffffff"
        "         ffffffffffffffff]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_putsh_rejects_shape_wider_than_thirty_two(void)
{
    char cmd[4096];
    char row[80];
    memset(row, 'f', 66);
    row[66] = '\0';  // 33 pixels
    int n = snprintf(cmd, sizeof(cmd), "putsh 1 [");
    for (int i = 0; i < 8; i++)
    {
        n += snprintf(cmd + n, sizeof(cmd) - n, " %s", row);
    }
    snprintf(cmd + n, sizeof(cmd) - n, "]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string(cmd).status);
}

void test_putsh_rejects_shape_taller_than_thirty_two(void)
{
    char cmd[4096];
    int n = snprintf(cmd, sizeof(cmd), "putsh 1 [");
    for (int i = 0; i < 33; i++)
    {
        n += snprintf(cmd + n, sizeof(cmd) - n, " ffffffffffffffff");
    }
    snprintf(cmd + n, sizeof(cmd) - n, "]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string(cmd).status);
}

void test_putsh_requires_inputs(void)
{
    Result r = run_string("putsh");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    
    r = run_string("putsh 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// The pool is finite, and a shape it cannot hold says so rather than
// half-storing
void test_putsh_reports_full_pool(void)
{
    char cmd[4096];
    char row[80];
    memset(row, 'f', 64);
    row[64] = '\0';  // 32 pixels

    // Eight 32x32 shapes are 8192 bytes, exactly the pool
    for (int slot = 1; slot <= 9; slot++)
    {
        int n = snprintf(cmd, sizeof(cmd), "putsh %d [", slot);
        for (int i = 0; i < 32; i++)
        {
            n += snprintf(cmd + n, sizeof(cmd) - n, " %s", row);
        }
        snprintf(cmd + n, sizeof(cmd) - n, "]");

        Result r = run_string(cmd);
        if (slot <= 8)
        {
            TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
        }
        else
        {
            TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
            TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, result_get_error_code(r));
        }
    }
}

void test_getsh_outputs_shape_rows(void)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "putsh 1 %s", shape_wedge_8x8("0c"));
    run_string(cmd);

    Result r = run_string("getsh 1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);

    const char *rows[] = {
        "ffffffffffffffff",
        "0cffffffffffffff",
        "0c0cffffffffffff",
        "0c0c0cffffffffff",
        "0c0c0c0cffffffff",
        "0c0c0c0c0cffffff",
        "0c0c0c0c0c0cffff",
        "0c0c0c0c0c0c0cff",
    };
    assert_shape_rows(r.value.as.node, rows, 8);
}

// What putsh took, getsh gives back -- the point of one format
void test_putsh_getsh_roundtrip(void)
{
    Result put = run_string(
        "putsh 3 [00010203040506070809fefeffff0f10"
        "         101112131415161718191a1b1c1d1e1f"
        "         202122232425262728292a2b2c2d2e2f"
        "         303132333435363738393a3b3c3d3e3f"
        "         404142434445464748494a4b4c4d4e4f"
        "         505152535455565758595a5b5c5d5e5f"
        "         606162636465666768696a6b6c6d6e6f"
        "         707172737475767778797a7b7c7d7e7f]");
    TEST_ASSERT_EQUAL(RESULT_NONE, put.status);

    Result r = run_string("getsh 3");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);

    const char *rows[] = {
        "00010203040506070809fefeffff0f10",
        "101112131415161718191a1b1c1d1e1f",
        "202122232425262728292a2b2c2d2e2f",
        "303132333435363738393a3b3c3d3e3f",
        "404142434445464748494a4b4c4d4e4f",
        "505152535455565758595a5b5c5d5e5f",
        "606162636465666768696a6b6c6d6e6f",
        "707172737475767778797a7b7c7d7e7f",
    };
    assert_shape_rows(r.value.as.node, rows, 8);
}

// The diamond the reference prints under getsh, so the example there
// cannot drift away from what Logo actually says
void test_getsh_matches_the_reference_example(void)
{
    Result put = run_string(
        "putsh 3 [fffffffefeffffff"
        "         fffffefefefeffff"
        "         fffefefefefefeff"
        "         fefefefefefefefe"
        "         fefefefefefefefe"
        "         fffefefefefefeff"
        "         fffffefefefeffff"
        "         fffffffefeffffff]");
    TEST_ASSERT_EQUAL(RESULT_NONE, put.status);

    reset_output();
    Result r = run_string("show getsh 3");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING(
        "[fffffffefeffffff fffffefefefeffff fffefefefefefeff fefefefefefefefe "
        "fefefefefefefefe fffefefefefefeff fffffefefefeffff fffffffefeffffff]\n",
        mock_device_get_output());
}

// Hand-typed uppercase reads the same, and comes back lowercase
void test_putsh_accepts_uppercase_hex(void)
{
    run_string(
        "putsh 1 [FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF"
        "         FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF"
        "         FFFFFFFFFFFFFFFF FFFFFFFFFFFFFFAB]");

    Result r = run_string("getsh 1");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);

    Node list = r.value.as.node;
    for (int i = 0; i < 7; i++) list = mem_cdr(list);
    Node last = mem_car(list);
    TEST_ASSERT_EQUAL_STRING_LEN("ffffffffffffffab", mem_word_ptr(last), 16);
}

// A slot nothing has been put in holds no shape, and says so
void test_getsh_empty_slot_outputs_empty_list(void)
{
    Result r = run_string("getsh 7");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
}

void test_getsh_rejects_shape_0(void)
{
    // Shape 0 is the line-drawn turtle, not a stored shape
    Result r = run_string("getsh 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_getsh_rejects_negative(void)
{
    Result r = run_string("getsh -1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_getsh_rejects_above_the_last_slot(void)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "getsh %d", LOGO_SHAPE_MAX_SLOT + 1);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string(cmd).status);
}

void test_getsh_requires_input(void)
{
    Result r = run_string("getsh");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// One slot, one shape: a capture reads back through getsh like anything
// else, and a putsh over it replaces it rather than hiding behind it
void test_snapsh_capture_reads_back_through_getsh(void)
{
    // A block of colour 9 on the canvas, centred on the home turtle
    mock_device_paint_canvas(160 - 4, 160 - 4, 8, 8, 9);
    Result snap = run_string("snapsh 4 8 8");
    TEST_ASSERT_EQUAL(RESULT_NONE, snap.status);

    Result r = run_string("getsh 4");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);

    const char *rows[] = {
        "0909090909090909", "0909090909090909", "0909090909090909",
        "0909090909090909", "0909090909090909", "0909090909090909",
        "0909090909090909", "0909090909090909",
    };
    assert_shape_rows(r.value.as.node, rows, 8);
}

void test_putsh_replaces_a_capture(void)
{
    mock_device_paint_canvas(160 - 8, 160 - 8, 16, 16, 9);
    run_string("snapsh 4 16 16");

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "putsh 4 %s", shape_wedge_8x8("0c"));
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string(cmd).status);

    uint8_t w = 0, h = 0;
    const uint8_t *pixels = mock_device_get_shape(4, &w, &h);
    TEST_ASSERT_NOT_NULL(pixels);
    TEST_ASSERT_EQUAL_UINT8(8, w);
    TEST_ASSERT_EQUAL_UINT8(8, h);
    TEST_ASSERT_EQUAL_UINT8(0x0c, pixels[7 * 8]);
}

void test_snapsh_replaces_a_putsh_shape(void)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "putsh 4 %s", shape_wedge_8x8("0c"));
    run_string(cmd);

    mock_device_paint_canvas(160 - 8, 160 - 8, 16, 16, 9);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("snapsh 4 16 16").status);

    uint8_t w = 0, h = 0;
    const uint8_t *pixels = mock_device_get_shape(4, &w, &h);
    TEST_ASSERT_NOT_NULL(pixels);
    TEST_ASSERT_EQUAL_UINT8(16, w);
    TEST_ASSERT_EQUAL_UINT8(16, h);
    TEST_ASSERT_EQUAL_UINT8(9, pixels[0]);
}

void test_shape_primitives_registered(void)
{
    // Verify primitives are registered by checking they don't produce "I don't know how to" error
    Result r = run_string("shape");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
    
    r = run_string("setsh 0");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Arc Tests
//==========================================================================

void test_arc_quarter_circle_segments(void)
{
    // 90 degrees at 4 degrees per segment -> ceil(90/4) = 23 segments
    Result r = run_string("arc 90 100");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL(23, mock_device_line_count());

    // Starts at the heading (north of centre), sweeps clockwise to east
    const MockLine *first = mock_device_get_line(0);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, first->x1);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 100.0f, first->y1);

    const MockLine *last = mock_device_get_line(22);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 100.0f, last->x2);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, last->y2);
}

void test_arc_full_circle_closes(void)
{
    Result r = run_string("arc 360 50");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL(90, mock_device_line_count());

    // Last segment ends where the first began
    const MockLine *first = mock_device_get_line(0);
    const MockLine *last = mock_device_get_line(89);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, first->x1, last->x2);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, first->y1, last->y2);
}

void test_arc_negative_angle_counterclockwise(void)
{
    // -90 from heading 0 sweeps counterclockwise from north to west
    Result r = run_string("arc -90 100");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL(23, mock_device_line_count());

    const MockLine *last = mock_device_get_line(22);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -100.0f, last->x2);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, last->y2);
}

void test_arc_respects_heading(void)
{
    // Heading east: arc starts east of centre and sweeps clockwise to south
    run_string("seth 90");
    Result r = run_string("arc 90 100");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockLine *first = mock_device_get_line(0);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 100.0f, first->x1);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, first->y1);

    const MockLine *last = mock_device_get_line(mock_device_line_count() - 1);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, last->x2);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, -100.0f, last->y2);
}

void test_arc_does_not_move_turtle(void)
{
    run_string("setpos [30 40] seth 45");
    Result r = run_string("arc 180 20");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(30, 40);
    ASSERT_HEADING(45);

    // Pen is still down afterwards
    r = run_string("pen");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("pendown", mem_word_ptr(r.value.as.node));
}

void test_arc_pen_up_draws_nothing(void)
{
    run_string("penup");
    Result r = run_string("arc 360 50");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL(0, mock_device_line_count());

    // Pen state restored to up
    r = run_string("pen");
    TEST_ASSERT_EQUAL_STRING("penup", mem_word_ptr(r.value.as.node));
}

void test_arc_angle_clamped_to_full_circle(void)
{
    Result r = run_string("arc 720 50");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL(90, mock_device_line_count());
}

void test_arc_zero_angle_draws_nothing(void)
{
    Result r = run_string("arc 0 50");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL(0, mock_device_line_count());
    ASSERT_POSITION(0, 0);
}

void test_arc_fence_out_of_bounds(void)
{
    run_string("fence");
    Result r = run_string("arc 360 1000");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_TURTLE_BOUNDS, result_get_error_code(r));

    // Turtle restored to the centre with the pen back down
    ASSERT_POSITION(0, 0);
    r = run_string("pen");
    TEST_ASSERT_EQUAL_STRING("pendown", mem_word_ptr(r.value.as.node));
}

void test_arc_requires_numbers(void)
{
    Result r = run_string("arc \"abc 10");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));

    r = run_string("arc 90 \"abc");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

//==========================================================================
// Multi-turtle addressing tests (tell / ask / each / who)
//==========================================================================

void test_tell_single_directs_commands(void)
{
    Result r = run_string("tell 3 forward 10");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 10.0f, mock_device_get_turtle(3)->y);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, mock_device_get_turtle(0)->y);
}

void test_tell_list_fans_out_ascending(void)
{
    Result r = run_string("tell [2 0] forward 25");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 25.0f, mock_device_get_turtle(0)->y);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, mock_device_get_turtle(1)->y);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 25.0f, mock_device_get_turtle(2)->y);
}

void test_tell_collapses_duplicates(void)
{
    Result r = run_string("tell [2 2 2] show who");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "2") != NULL);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "[") == NULL);
}

void test_tell_out_of_range_errors(void)
{
    Result r = run_string("tell 5");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    r = run_string("tell 8");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));

    r = run_string("tell [0 9]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("tell [-1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("tell []");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    // A failed tell leaves the previous set (turtle 5) in force
    r = run_string("print who");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "5") != NULL);
}

void test_tell_non_integer_errors(void)
{
    Result r = run_string("tell 1.5");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("tell \"abc");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_queries_answer_for_lowest_active(void)
{
    Result r = run_string("tell 3 forward 44 tell [1 3] print ycor");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // Turtle 1 (the lowest active) is still at home; if the query had
    // answered for turtle 3 the output would be 44
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "44") == NULL);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "0") != NULL);
}

void test_who_outputs_number_when_single(void)
{
    Result r = run_string("tell 5 print who");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "5") != NULL);
}

void test_who_outputs_sorted_list(void)
{
    Result r = run_string("tell [5 2] show who");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "[2 5]") != NULL);
}

void test_ask_runs_for_named_turtle_and_restores(void)
{
    Result r = run_string("ask 2 [forward 30] print who");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 30.0f, mock_device_get_turtle(2)->y);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, mock_device_get_turtle(0)->y);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "0") != NULL);
}

void test_ask_restores_set_on_error(void)
{
    Result r = run_string("tell 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    r = run_string("ask 2 [forward \"abc]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("print who");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "1") != NULL);
}

void test_ask_restores_set_on_throw(void)
{
    Result r = run_string("tell 1 catch \"tag [ask 2 [throw \"tag]] print who");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "1") != NULL);
}

void test_ask_as_operation_outputs_value(void)
{
    Result r = run_string("ask 2 [forward 30]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    reset_output();
    r = run_string("print ask 2 [ycor]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("30\n", mock_device_get_output());
}

void test_ask_as_operation_within_procedure(void)
{
    Result r = proc_define_from_text(
        "to turtle.x :n\n"
        "output ask :n [xcor]\n"
        "end");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);

    r = run_string("tell 2 setx 15 tell 0");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    reset_output();
    r = run_string("print turtle.x 2");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("15\n", mock_device_get_output());
}

void test_ask_still_works_as_plain_command(void)
{
    // A list whose last instruction is a command (not a value) must still
    // behave exactly as before: ask outputs nothing usable as a command.
    Result r = run_string("ask 2 [forward 12 forward 8]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 20.0f, mock_device_get_turtle(2)->y);
}

void test_each_narrows_set_with_who(void)
{
    Result r = run_string("tell [0 1 2 3] each [setheading who * 90]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, mock_device_get_turtle(0)->heading);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 90.0f, mock_device_get_turtle(1)->heading);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 180.0f, mock_device_get_turtle(2)->heading);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 270.0f, mock_device_get_turtle(3)->heading);
}

void test_each_restores_set(void)
{
    Result r = run_string("tell [1 2] each [forward 5] show who");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "[1 2]") != NULL);
}

void test_each_propagates_error_and_restores(void)
{
    Result r = run_string("tell [1 2]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    r = run_string("each [forward \"abc]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("show who");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "[1 2]") != NULL);
}

void test_pen_state_is_per_turtle(void)
{
    Result r = run_string("tell 1 penup tell 0");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    TEST_ASSERT_EQUAL(LOGO_PEN_UP, mock_device_get_turtle(1)->pen_state);
    TEST_ASSERT_EQUAL(LOGO_PEN_DOWN, mock_device_get_turtle(0)->pen_state);
}

void test_clearscreen_resets_to_turtle_zero(void)
{
    Result r = run_string("tell [1 2] showturtle forward 15 clearscreen print who");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(strstr(mock_device_get_output(), "0") != NULL);

    // Turtles 1-7 are re-hidden at home
    TEST_ASSERT_FALSE(mock_device_get_turtle(1)->visible);
    TEST_ASSERT_FALSE(mock_device_get_turtle(2)->visible);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, mock_device_get_turtle(1)->y);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, mock_device_get_turtle(2)->y);
}

void test_turtles_boot_hidden_except_zero(void)
{
    TEST_ASSERT_TRUE(mock_device_get_turtle(0)->visible);
    for (int i = 1; i < MOCK_MAX_TURTLES; i++)
    {
        TEST_ASSERT_FALSE(mock_device_get_turtle(i)->visible);
    }
}

//==========================================================================
// Costume tests (stamp / snapsh)
//==========================================================================

void test_stamp_fans_out_over_active_set(void)
{
    Result r = run_string("tell [1 3] stamp");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // One stamp per active turtle
    int stamps = 0;
    const MockDeviceState *state = mock_device_get_state();
    for (int i = 0; i < state->command_count; i++)
    {
        if (mock_device_get_command(i)->type == MOCK_CMD_STAMP)
        {
            stamps++;
        }
    }
    TEST_ASSERT_EQUAL(2, stamps);
}

//==========================================================================
// write tests (graphics-screen text at the turtle)
//==========================================================================

void test_write_draws_word_in_pen_colour(void)
{
    Result r = run_string("setpc 4 write \"hello");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(1, state->label.count);
    TEST_ASSERT_EQUAL_STRING("hello", state->label.last_text);
    TEST_ASSERT_EQUAL(4, state->label.last_colour);
}

void test_write_formats_list_without_brackets(void)
{
    // Like print: a list loses its outer brackets, numbers are formatted.
    Result r = run_string("write [score 42]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("score 42", mock_device_get_state()->label.last_text);
}

void test_write_draws_number(void)
{
    Result r = run_string("write 3.5");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("3.5", mock_device_get_state()->label.last_text);
}

void test_write_does_not_move_turtle(void)
{
    run_string("setpos [20 30]");
    Result r = run_string("write \"hi");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    ASSERT_POSITION(20, 30);
}

void test_write_draws_max_length_word(void)
{
    // A 255-char word (the atom limit) must draw fully, not get dropped
    // whole by the formatter's all-or-nothing word write.
    char word[256];
    memset(word, 'a', 255);
    word[255] = '\0';

    char cmd[300];
    snprintf(cmd, sizeof(cmd), "write \"%s", word);
    Result r = run_string(cmd);
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL(255, (int)strlen(mock_device_get_state()->label.last_text));
}

void test_write_fans_out_over_active_set(void)
{
    Result r = run_string("tell [1 3] write \"x");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    int writes = 0;
    const MockDeviceState *state = mock_device_get_state();
    for (int i = 0; i < state->command_count; i++)
    {
        if (mock_device_get_command(i)->type == MOCK_CMD_WRITE)
        {
            writes++;
        }
    }
    TEST_ASSERT_EQUAL(2, writes);
}

//==========================================================================
// P18 M1: the parenthesised forms of `write`.
//
// The table the milestone was specified from:
//
//   write text            glyph in the pen colour, cell untouched
//   (write text fg)       glyph in fg,             cell untouched
//   (write text fg bg)    glyph in fg,             cell filled with bg
//
// Transparent stays the default because `write`'s job is labelling a picture;
// an opaque default would punch a rectangle through every existing use of it.
// What the cell actually looks like is a pixel claim and is made in
// tests/test_screen_text.c against the real rasteriser; these check that the
// primitive hands the device the right numbers.
//==========================================================================

void test_write_is_transparent_by_default(void)
{
    Result r = run_string("setpc 4 write \"hello");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_MESSAGE(-1, mock_device_get_state()->label.last_background,
        "a bare `write` asked the device to fill the cell");
}

void test_write_with_a_colour_overrides_the_pen_and_stays_transparent(void)
{
    Result r = run_string("setpc 4 (write \"hello 7)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_STRING("hello", state->label.last_text);
    TEST_ASSERT_EQUAL(7, state->label.last_colour);
    TEST_ASSERT_EQUAL_MESSAGE(-1, state->label.last_background,
        "the two-input form filled the cell");
    // And it is an override, not a `setpc`: the pen is where it was.
    TEST_ASSERT_EQUAL_STRING("4", value_to_string(eval_string("pencolor").value));
}

void test_write_with_a_background_fills_the_cell(void)
{
    Result r = run_string("(write \"hello 0 254)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(0, state->label.last_colour);
    TEST_ASSERT_EQUAL(254, state->label.last_background);
}

// 255 is the palette slot holding the current background colour, which is
// what makes `(write :old 255 255)` the eraser for text in a picture -- no
// new spelling invented.
void test_write_accepts_the_background_colour_sentinel(void)
{
    Result r = run_string("(write \"hello 255 255)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(255, state->label.last_colour);
    TEST_ASSERT_EQUAL(255, state->label.last_background);
}

void test_write_rejects_a_colour_outside_the_palette(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("(write \"hello 256)").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("(write \"hello -1)").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("(write \"hello 4 300)").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("(write \"hello 1.5)").status);
}

void test_write_rejects_four_inputs(void)
{
    Result r = run_string("(write \"hello 1 2 3)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_TOO_MANY_INPUTS, result_get_error_code(r));
}

// The colours are per call, not per turtle, but the transparent default still
// has to take each turtle's OWN pen colour -- which is why the sentinel is
// resolved in the device and not in the primitive.
void test_a_bare_write_takes_each_turtles_own_pen_colour(void)
{
    run_string("tell 1 setpc 3");
    run_string("tell 5 setpc 9");
    Result r = run_string("tell [1 5] write \"x");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // The last of the two writes is turtle 5's, in turtle 5's colour.
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(5, state->label.last_turtle);
    TEST_ASSERT_EQUAL(9, state->label.last_colour);
}

//==========================================================================
// P18 M2: setpendash / pendash
//==========================================================================

void test_pendash_starts_solid(void)
{
    TEST_ASSERT_EQUAL_STRING("1", value_to_string(eval_string("pendash").value));
}

void test_setpendash_round_trips(void)
{
    Result r = run_string("setpendash 4");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("4", value_to_string(eval_string("pendash").value));
}

// Rounded and clamped exactly as `setpensize` is, so the pair behaves the
// same way at both ends.
void test_setpendash_rounds_and_clamps(void)
{
    run_string("setpendash 3.6");
    TEST_ASSERT_EQUAL_STRING("4", value_to_string(eval_string("pendash").value));

    run_string("setpendash 1000");
    TEST_ASSERT_EQUAL_STRING("255", value_to_string(eval_string("pendash").value));
}

void test_setpendash_rejects_less_than_one(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setpendash 0").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setpendash -2").status);
}

// The dash is pen state, so it follows the pen through `tell` the way the
// colour and the size do.
void test_setpendash_is_per_turtle(void)
{
    run_string("tell 1 setpendash 4");
    run_string("tell 2 setpendash 9");

    run_string("tell 1");
    TEST_ASSERT_EQUAL_STRING("4", value_to_string(eval_string("pendash").value));
    run_string("tell 2");
    TEST_ASSERT_EQUAL_STRING("9", value_to_string(eval_string("pendash").value));
}

// And it reaches the stroke: the mock records the dash each line was drawn
// with, so a change of pen shows up in the drawing rather than only in the
// operation that reports it.
void test_a_stroke_carries_the_dash_the_pen_had(void)
{
    run_string("setpendash 4 fd 10 setpendash 1 fd 10");

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(2, state->graphics.line_count);
    TEST_ASSERT_EQUAL(4, state->graphics.lines[0].pen_dash);
    TEST_ASSERT_EQUAL(1, state->graphics.lines[1].pen_dash);
}

void test_snapsh_captures_for_first_active(void)
{
    Result r = run_string("tell [2 5] snapsh 3 16 24");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL(1, state->costume.snap_count);
    TEST_ASSERT_EQUAL(3, state->costume.last_snap_slot);
    TEST_ASSERT_EQUAL(16, state->costume.last_snap_w);
    TEST_ASSERT_EQUAL(24, state->costume.last_snap_h);
    TEST_ASSERT_EQUAL(2, state->costume.last_snap_turtle);
}

void test_snapsh_validates_inputs(void)
{
    // Slot out of range
    Result r = run_string("snapsh 0 16 16");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "snapsh %d 16 16", LOGO_SHAPE_MAX_SLOT + 1);
    r = run_string(cmd);
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    // Dimensions out of range
    r = run_string("snapsh 1 7 16");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    r = run_string("snapsh 1 16 33");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    // No capture happened
    TEST_ASSERT_EQUAL(0, mock_device_get_state()->costume.snap_count);
}

void test_snapsh_reports_full_pool(void)
{
    mock_device_set_snap_result(false);

    Result r = run_string("snapsh 1 16 16");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, result_get_error_code(r));
}

void test_setrot_is_per_turtle(void)
{
    Result r = run_string("tell [1 2] setrot \"full tell 0");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    TEST_ASSERT_EQUAL(1, mock_device_get_turtle(1)->rot_style);  // LOGO_ROT_FULL
    TEST_ASSERT_EQUAL(1, mock_device_get_turtle(2)->rot_style);
    TEST_ASSERT_EQUAL(0, mock_device_get_turtle(0)->rot_style);  // LOGO_ROT_FIXED
}

void test_setrot_rejects_unknown_style(void)
{
    Result r = run_string("setrot \"sideways");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_setrot_case_insensitive(void)
{
    Result r = run_string("setrot \"FLIP");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL(2, mock_device_get_turtle(0)->rot_style);  // LOGO_ROT_FLIP
}

void test_setmag_is_per_turtle(void)
{
    Result r = run_string("tell 3 setmag 2 tell 0");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    TEST_ASSERT_EQUAL(2, mock_device_get_turtle(3)->mag);
    TEST_ASSERT_EQUAL(1, mock_device_get_turtle(0)->mag);
}

void test_setmag_rejects_other_values(void)
{
    Result r = run_string("setmag 3");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));

    r = run_string("setmag 0");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("setmag 1.5");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_addressing_primitives_registered(void)
{
    // Verify primitives are registered by checking they don't produce
    // "I don't know how to" errors
    Result r = run_string("tell 0");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("ask 0 []");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("each []");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);

    r = run_string("show who");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Sensing and collision
//==========================================================================

// Stage turtle n's rendered raster in screen-pixel space. The anchor
// (cx, cy) defaults to the mask centre.
static void stage_raster(uint8_t n, int x, int y, int w, int h,
                         bool indexed, bool visible, const uint8_t *mask)
{
    LogoTurtleRaster r;
    memset(&r, 0, sizeof(r));
    r.x = (int16_t)x;
    r.y = (int16_t)y;
    r.cx = (int16_t)(x + w / 2);
    r.cy = (int16_t)(y + h / 2);
    r.w = (uint8_t)w;
    r.h = (uint8_t)h;
    r.indexed = indexed;
    r.visible = visible;
    r.mask = mask;
    mock_device_set_raster(n, &r);
}

static bool output_has(const char *needle)
{
    return strstr(mock_device_get_output(), needle) != NULL;
}

void test_distance_is_euclidean(void)
{
    // Turtle 0 at home (0,0); place turtle 1 at (30,40): distance 50.
    run_string("tell 1 setpos [30 40] tell 0");
    Result r = run_string("print distance 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("50"));
}

void test_distance_answers_for_first_active(void)
{
    // From turtle 2 (at 0,0 after we move it) to turtle 0.
    run_string("tell 0 setpos [0 0] tell 2 setpos [0 0]");
    run_string("tell 0 setpos [60 0]");
    Result r = run_string("tell 2 print distance 0");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("60"));
}

void test_distance_rejects_bad_turtle(void)
{
    Result r = run_string("print distance 8");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_touching_true_when_masks_overlap(void)
{
    static uint8_t solid[8 * 8];
    memset(solid, 1, sizeof(solid));
    run_string("window");  // deterministic (no wrap fold)
    stage_raster(0, 100, 100, 8, 8, false, true, solid);
    stage_raster(1, 104, 104, 8, 8, false, true, solid);

    Result r = run_string("print touching? 0 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("true"));
}

void test_touching_false_when_apart(void)
{
    static uint8_t solid[8 * 8];
    memset(solid, 1, sizeof(solid));
    run_string("window");
    stage_raster(0, 100, 100, 8, 8, false, true, solid);
    stage_raster(1, 150, 100, 8, 8, false, true, solid);

    Result r = run_string("print touching? 0 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("false"));
}

void test_touching_is_pixel_precise(void)
{
    // Bounding boxes overlap, but the opaque pixels do not: turtle 0 is
    // solid only in its left column, turtle 1 only in its right column.
    static uint8_t left[8 * 8];
    static uint8_t right[8 * 8];
    memset(left, 0, sizeof(left));
    memset(right, 0, sizeof(right));
    for (int row = 0; row < 8; row++)
    {
        left[row * 8 + 0] = 1;
        right[row * 8 + 7] = 1;
    }
    run_string("window");
    stage_raster(0, 100, 100, 8, 8, false, true, left);
    stage_raster(1, 102, 100, 8, 8, false, true, right);

    Result r = run_string("print touching? 0 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("false"));
}

void test_touching_requires_both_visible(void)
{
    static uint8_t solid[8 * 8];
    memset(solid, 1, sizeof(solid));
    run_string("window");
    stage_raster(0, 100, 100, 8, 8, false, true, solid);
    stage_raster(1, 104, 104, 8, 8, false, false, solid);  // hidden

    Result r = run_string("print touching? 0 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("false"));
}

void test_touching_wrap_edge_contact(void)
{
    static uint8_t solid[8 * 8];
    memset(solid, 1, sizeof(solid));
    run_string("wrap");
    // Turtle 0 straddles the right edge; turtle 1 sits at the left edge.
    // In wrap mode the compositor draws 0 on both sides, so they touch.
    stage_raster(0, 316, 100, 8, 8, false, true, solid);  // 316..323
    stage_raster(1, 0, 100, 8, 8, false, true, solid);    // 0..7

    Result r = run_string("print touching? 0 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("true"));
}

void test_touching_indexed_transparency(void)
{
    // Two indexed rasters whose only overlapping cell is transparent in
    // one of them: no contact.
    static uint8_t a[8 * 8];
    static uint8_t b[8 * 8];
    memset(a, LOGO_RASTER_TRANSPARENT, sizeof(a));
    memset(b, LOGO_RASTER_TRANSPARENT, sizeof(b));
    a[0] = 12;                 // top-left opaque
    b[7 * 8 + 7] = 12;         // bottom-right opaque
    run_string("window");
    stage_raster(0, 100, 100, 8, 8, true, true, a);
    stage_raster(1, 101, 101, 8, 8, true, true, b);

    Result r = run_string("print touching? 0 1");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("false"));
}

void test_touching_rejects_bad_turtle(void)
{
    Result r = run_string("print touching? 0 9");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_over_true_when_canvas_matches(void)
{
    static uint8_t solid[8 * 8];
    memset(solid, 1, sizeof(solid));
    run_string("window");
    stage_raster(0, 100, 100, 8, 8, false, true, solid);
    mock_device_paint_canvas(104, 104, 2, 2, 12);  // under the mask

    Result r = run_string("print over? 12");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("true"));
}

void test_over_false_for_other_colour(void)
{
    static uint8_t solid[8 * 8];
    memset(solid, 1, sizeof(solid));
    run_string("window");
    stage_raster(0, 100, 100, 8, 8, false, true, solid);
    mock_device_paint_canvas(104, 104, 2, 2, 12);

    Result r = run_string("print over? 9");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("false"));
}

void test_over_ignores_transparent_mask_pixels(void)
{
    // The matching colour lies only under a transparent mask cell.
    static uint8_t mask[8 * 8];
    memset(mask, 0, sizeof(mask));
    mask[0] = 1;  // only top-left is opaque
    run_string("window");
    stage_raster(0, 100, 100, 8, 8, false, true, mask);
    // Paint colour 12 away from the opaque cell (at 105,105).
    mock_device_set_canvas_point(105, 105, 12);

    Result r = run_string("print over? 12");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("false"));
}

void test_over_answers_for_first_active(void)
{
    static uint8_t solid[8 * 8];
    memset(solid, 1, sizeof(solid));
    run_string("window");
    stage_raster(2, 100, 100, 8, 8, false, true, solid);
    mock_device_paint_canvas(104, 104, 2, 2, 7);

    Result r = run_string("tell [2 5] print over? 7");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("true"));
}

void test_over_rejects_bad_colour(void)
{
    Result r = run_string("print over? 300");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_colourunder_reads_turtle_position(void)
{
    static uint8_t solid[8 * 8];
    memset(solid, 1, sizeof(solid));
    run_string("window");
    // Anchor is the mask centre: (100 + 4, 100 + 4) = (104, 104).
    stage_raster(0, 100, 100, 8, 8, false, true, solid);
    mock_device_set_canvas_point(104, 104, 42);

    Result r = run_string("print colourunder");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("42"));
}

void test_colourunder_works_when_hidden(void)
{
    static uint8_t solid[8 * 8];
    memset(solid, 1, sizeof(solid));
    run_string("window");
    stage_raster(0, 100, 100, 8, 8, false, false, solid);  // hidden
    mock_device_set_canvas_point(104, 104, 9);

    Result r = run_string("print colourunder");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(output_has("9"));
}

void test_sensing_primitives_registered(void)
{
    Result r = run_string("show touchingp 0 1");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
    r = run_string("show overp 0");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
    r = run_string("show colorunder");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
    r = run_string("show distance 1");
    TEST_ASSERT_NOT_EQUAL(RESULT_ERROR, r.status);
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
    RUN_TEST(test_setpos_accepts_two_numbers);
    RUN_TEST(test_setpos_two_number_form_draws_one_straight_line);
    RUN_TEST(test_setpos_two_number_form_does_not_allocate_per_call);
    RUN_TEST(test_setpos_list_form_allocates_per_call);
    RUN_TEST(test_setpos_rejects_too_many_inputs);
    RUN_TEST(test_setpos_list_form_survives_the_variadic_form);
    
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
    RUN_TEST(test_heading_outputs_normalized_after_negative);
    RUN_TEST(test_heading_outputs_normalized_after_large_positive);
    RUN_TEST(test_setheading_negative_normalized);
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
    RUN_TEST(test_pensize_defaults_to_one);
    RUN_TEST(test_setpensize_sets_pen_size);
    RUN_TEST(test_pensize_outputs_current_size);
    RUN_TEST(test_setpensize_rounds_to_nearest_pixel);
    RUN_TEST(test_setpensize_clamps_to_maximum);
    RUN_TEST(test_setpensize_rejects_non_positive);
    RUN_TEST(test_setpensize_records_on_drawn_line);
    RUN_TEST(test_setpensize_records_on_a_dot);
    RUN_TEST(test_setbg_sets_background_color);
    RUN_TEST(test_setbg_accepts_zero);
    RUN_TEST(test_setbg_accepts_254);
    RUN_TEST(test_setbg_rejects_255);
    RUN_TEST(test_setbg_rejects_negative);
    RUN_TEST(test_setbg_rejects_256);
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
    RUN_TEST(test_clearscreen_does_not_draw_line);
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
    RUN_TEST(test_fence_prevents_setpos_past_boundary);
    RUN_TEST(test_fence_prevents_setx_past_boundary);
    RUN_TEST(test_fence_prevents_sety_past_boundary);
    RUN_TEST(test_fence_allows_setpos_within_bounds);
    RUN_TEST(test_window_allows_setpos_past_boundary);
    RUN_TEST(test_wrap_wraps_setpos_at_boundary);
    RUN_TEST(test_wrap_setpos_draws_correct_line);
    
    // Line drawing tests
    RUN_TEST(test_forward_with_pendown_draws_line);
    RUN_TEST(test_forward_with_penup_no_line);
    RUN_TEST(test_setpos_with_pendown_draws_line);
    RUN_TEST(test_back_with_pendown_draws_line);
    RUN_TEST(test_home_with_pendown_draws_line);
    RUN_TEST(test_setx_with_pendown_draws_line);
    RUN_TEST(test_sety_with_pendown_draws_line);
    
    // Palette tests
    RUN_TEST(test_setpalette_sets_rgb_values);
    RUN_TEST(test_palette_outputs_rgb_list);
    RUN_TEST(test_restorepalette_resets_palette);
    RUN_TEST(test_setpalette_clamps_values);
    RUN_TEST(test_setpalette_requires_list);
    RUN_TEST(test_setpalette_requires_three_elements);
    RUN_TEST(test_palette_validates_slot);
    RUN_TEST(test_setpalette_overwrites_previous_value);
    
    // Integration tests
    RUN_TEST(test_draw_square);
    RUN_TEST(test_draw_triangle);
    RUN_TEST(test_movement_preserves_heading);
    RUN_TEST(test_combined_movements);
    RUN_TEST(test_primitives_are_registered);
    
    // Shape tests
    RUN_TEST(test_shape_outputs_current_shape);
    RUN_TEST(test_setsh_sets_shape);
    RUN_TEST(test_setsh_and_shape_roundtrip);
    RUN_TEST(test_setsh_shape_0);
    RUN_TEST(test_setsh_shape_15);
    RUN_TEST(test_setsh_rejects_negative);
    RUN_TEST(test_setsh_rejects_above_the_last_slot);
    RUN_TEST(test_setsh_requires_input);
    RUN_TEST(test_putsh_sets_shape_data);
    RUN_TEST(test_putsh_shape_15);
    RUN_TEST(test_putsh_accepts_a_rectangle);
    RUN_TEST(test_putsh_rejects_shape_0);
    RUN_TEST(test_putsh_rejects_negative_shape);
    RUN_TEST(test_putsh_rejects_shape_above_the_last_slot);
    RUN_TEST(test_putsh_requires_list);
    RUN_TEST(test_putsh_rejects_ragged_rows);
    RUN_TEST(test_putsh_rejects_odd_length_row);
    RUN_TEST(test_putsh_rejects_non_hex);
    RUN_TEST(test_putsh_rejects_shape_narrower_than_eight);
    RUN_TEST(test_putsh_rejects_shape_shorter_than_eight);
    RUN_TEST(test_putsh_rejects_shape_wider_than_thirty_two);
    RUN_TEST(test_putsh_rejects_shape_taller_than_thirty_two);
    RUN_TEST(test_putsh_requires_inputs);
    RUN_TEST(test_putsh_reports_full_pool);
    RUN_TEST(test_getsh_outputs_shape_rows);
    RUN_TEST(test_getsh_empty_slot_outputs_empty_list);
    RUN_TEST(test_getsh_rejects_shape_0);
    RUN_TEST(test_getsh_rejects_negative);
    RUN_TEST(test_getsh_rejects_above_the_last_slot);
    RUN_TEST(test_getsh_requires_input);
    RUN_TEST(test_putsh_getsh_roundtrip);
    RUN_TEST(test_getsh_matches_the_reference_example);
    RUN_TEST(test_putsh_accepts_uppercase_hex);
    RUN_TEST(test_snapsh_capture_reads_back_through_getsh);
    RUN_TEST(test_putsh_replaces_a_capture);
    RUN_TEST(test_snapsh_replaces_a_putsh_shape);
    RUN_TEST(test_shape_primitives_registered);

    // Arc tests
    RUN_TEST(test_arc_quarter_circle_segments);
    RUN_TEST(test_arc_full_circle_closes);
    RUN_TEST(test_arc_negative_angle_counterclockwise);
    RUN_TEST(test_arc_respects_heading);
    RUN_TEST(test_arc_does_not_move_turtle);
    RUN_TEST(test_arc_pen_up_draws_nothing);
    RUN_TEST(test_arc_angle_clamped_to_full_circle);
    RUN_TEST(test_arc_zero_angle_draws_nothing);
    RUN_TEST(test_arc_fence_out_of_bounds);
    RUN_TEST(test_arc_requires_numbers);

    // Multi-turtle addressing tests
    RUN_TEST(test_tell_single_directs_commands);
    RUN_TEST(test_tell_list_fans_out_ascending);
    RUN_TEST(test_tell_collapses_duplicates);
    RUN_TEST(test_tell_out_of_range_errors);
    RUN_TEST(test_tell_non_integer_errors);
    RUN_TEST(test_queries_answer_for_lowest_active);
    RUN_TEST(test_who_outputs_number_when_single);
    RUN_TEST(test_who_outputs_sorted_list);
    RUN_TEST(test_ask_runs_for_named_turtle_and_restores);
    RUN_TEST(test_ask_restores_set_on_error);
    RUN_TEST(test_ask_restores_set_on_throw);
    RUN_TEST(test_ask_as_operation_outputs_value);
    RUN_TEST(test_ask_as_operation_within_procedure);
    RUN_TEST(test_ask_still_works_as_plain_command);
    RUN_TEST(test_each_narrows_set_with_who);
    RUN_TEST(test_each_restores_set);
    RUN_TEST(test_each_propagates_error_and_restores);
    RUN_TEST(test_pen_state_is_per_turtle);
    RUN_TEST(test_clearscreen_resets_to_turtle_zero);
    RUN_TEST(test_turtles_boot_hidden_except_zero);
    RUN_TEST(test_addressing_primitives_registered);

    // Costume tests
    RUN_TEST(test_stamp_fans_out_over_active_set);
    RUN_TEST(test_write_draws_word_in_pen_colour);
    RUN_TEST(test_write_formats_list_without_brackets);
    RUN_TEST(test_write_draws_number);
    RUN_TEST(test_write_does_not_move_turtle);
    RUN_TEST(test_write_draws_max_length_word);
    RUN_TEST(test_write_fans_out_over_active_set);
    // P18 M1: the parenthesised forms of `write`
    RUN_TEST(test_write_is_transparent_by_default);
    RUN_TEST(test_write_with_a_colour_overrides_the_pen_and_stays_transparent);
    RUN_TEST(test_write_with_a_background_fills_the_cell);
    RUN_TEST(test_write_accepts_the_background_colour_sentinel);
    RUN_TEST(test_write_rejects_a_colour_outside_the_palette);
    RUN_TEST(test_write_rejects_four_inputs);
    RUN_TEST(test_a_bare_write_takes_each_turtles_own_pen_colour);

    // P18 M2: setpendash / pendash
    RUN_TEST(test_pendash_starts_solid);
    RUN_TEST(test_setpendash_round_trips);
    RUN_TEST(test_setpendash_rounds_and_clamps);
    RUN_TEST(test_setpendash_rejects_less_than_one);
    RUN_TEST(test_setpendash_is_per_turtle);
    RUN_TEST(test_a_stroke_carries_the_dash_the_pen_had);

    RUN_TEST(test_snapsh_captures_for_first_active);
    RUN_TEST(test_snapsh_validates_inputs);
    RUN_TEST(test_snapsh_reports_full_pool);
    RUN_TEST(test_setrot_is_per_turtle);
    RUN_TEST(test_setrot_rejects_unknown_style);
    RUN_TEST(test_setrot_case_insensitive);
    RUN_TEST(test_setmag_is_per_turtle);
    RUN_TEST(test_setmag_rejects_other_values);

    // Sensing and collision tests
    RUN_TEST(test_distance_is_euclidean);
    RUN_TEST(test_distance_answers_for_first_active);
    RUN_TEST(test_distance_rejects_bad_turtle);
    RUN_TEST(test_touching_true_when_masks_overlap);
    RUN_TEST(test_touching_false_when_apart);
    RUN_TEST(test_touching_is_pixel_precise);
    RUN_TEST(test_touching_requires_both_visible);
    RUN_TEST(test_touching_wrap_edge_contact);
    RUN_TEST(test_touching_indexed_transparency);
    RUN_TEST(test_touching_rejects_bad_turtle);
    RUN_TEST(test_over_true_when_canvas_matches);
    RUN_TEST(test_over_false_for_other_colour);
    RUN_TEST(test_over_ignores_transparent_mask_pixels);
    RUN_TEST(test_over_answers_for_first_active);
    RUN_TEST(test_over_rejects_bad_colour);
    RUN_TEST(test_colourunder_reads_turtle_position);
    RUN_TEST(test_colourunder_works_when_hidden);
    RUN_TEST(test_sensing_primitives_registered);

    return UNITY_END();
}
