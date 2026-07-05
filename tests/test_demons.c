//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for `when` demons, freeze/thaw, and autonomous turtle motion and
//  animation (core/demons.c plus the setspeed/speed/setanim primitives).
//

#include "test_scaffold.h"
#include "mock_device.h"
#include "core/demons.h"
#include <stdio.h>
#include <string.h>

void setUp(void)
{
    test_scaffold_setUp_with_device_and_hardware();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// Arming, firing, and edge triggering
//==========================================================================

void test_when_fires_on_false_to_true_edge(void)
{
    run_string("make \"x 0  make \"fired 0");
    run_string("when [:x = 5] [make \"fired :fired + 1]");

    // Condition false: poll does not fire.
    demons_poll();
    mock_device_clear_output();
    run_string("show :fired");
    TEST_ASSERT_EQUAL_STRING("0\n", mock_device_get_output());

    // Condition becomes true: fires exactly once.
    run_string("make \"x 5");
    demons_poll();
    demons_poll();  // stays true — no re-fire (edge triggered)
    mock_device_clear_output();
    run_string("show :fired");
    TEST_ASSERT_EQUAL_STRING("1\n", mock_device_get_output());

    // Goes false then true again: fires once more.
    run_string("make \"x 0");
    demons_poll();
    run_string("make \"x 5");
    demons_poll();
    mock_device_clear_output();
    run_string("show :fired");
    TEST_ASSERT_EQUAL_STRING("2\n", mock_device_get_output());
}

void test_when_empty_action_disarms(void)
{
    run_string("make \"x 5  make \"fired 0");
    run_string("when [:x = 5] [make \"fired :fired + 1]");
    run_string("when [:x = 5] []");  // disarm the same condition

    demons_poll();
    mock_device_clear_output();
    run_string("show :fired");
    TEST_ASSERT_EQUAL_STRING("0\n", mock_device_get_output());
}

void test_when_rearm_replaces_action_and_resets_edge(void)
{
    run_string("make \"x 5  make \"a 0  make \"b 0");
    run_string("when [:x = 5] [make \"a :a + 1]");
    demons_poll();  // fires action a

    // Re-arm the same condition with a new action; edge resets so it fires
    // again even though the condition was already true.
    run_string("when [:x = 5] [make \"b :b + 1]");
    demons_poll();

    mock_device_clear_output();
    run_string("show (list :a :b)");
    TEST_ASSERT_EQUAL_STRING("[1 1]\n", mock_device_get_output());
}

void test_when_table_full_errors(void)
{
    char buf[64];
    for (int i = 0; i < 8; i++)
    {
        snprintf(buf, sizeof(buf), "when [:x = %d] [make \"n 1]", i);
        Result r = run_string(buf);
        TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    }
    Result r = run_string("when [:x = 99] [make \"n 1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, r.error_code);
}

void test_when_print_lists_armed_demons(void)
{
    run_string("make \"x 0  make \"y 0");  // so the poll can evaluate them
    run_string("when [:x = 1] [fd 10]");
    run_string("when [:y = 2] [rt 90]");
    mock_device_clear_output();
    run_string("(when)");
    TEST_ASSERT_NOT_NULL(strstr(mock_device_get_output(), "when [:x = 1] [fd 10]"));
    TEST_ASSERT_NOT_NULL(strstr(mock_device_get_output(), "when [:y = 2] [rt 90]"));
}

//==========================================================================
// Action errors and re-entrancy
//==========================================================================

void test_when_action_error_propagates(void)
{
    run_string("make \"x 5");
    run_string("when [:x = 5] [fd \"notanumber]");
    Result r = demons_poll();
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_when_bad_condition_errors(void)
{
    run_string("when [fd 10] []");  // empty action disarms, so arm a real one
    run_string("when [sum 1 2] [make \"n 1]");  // condition outputs a number
    Result r = demons_poll();
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, r.error_code);
}

void test_action_does_not_reenter_poll(void)
{
    // The action arms nothing new but bumps a counter; a nested poll during
    // the action is suppressed, so the counter rises by exactly one.
    run_string("make \"x 5  make \"n 0");
    run_string("when [:x = 5] [make \"n :n + 1]");
    demons_poll();
    mock_device_clear_output();
    run_string("show :n");
    TEST_ASSERT_EQUAL_STRING("1\n", mock_device_get_output());
}

//==========================================================================
// freeze / thaw
//==========================================================================

void test_freeze_suspends_demons(void)
{
    run_string("make \"x 5  make \"fired 0");
    run_string("when [:x = 5] [make \"fired :fired + 1]");
    run_string("freeze");
    TEST_ASSERT_TRUE(demons_frozen());
    demons_poll();
    mock_device_clear_output();
    run_string("show :fired");
    TEST_ASSERT_EQUAL_STRING("0\n", mock_device_get_output());

    run_string("thaw");
    TEST_ASSERT_FALSE(demons_frozen());
    demons_poll();
    mock_device_clear_output();
    run_string("show :fired");
    TEST_ASSERT_EQUAL_STRING("1\n", mock_device_get_output());
}

//==========================================================================
// Lifetime: cs and reset clear demons
//==========================================================================

void test_clearscreen_clears_demons(void)
{
    run_string("make \"x 5  make \"fired 0");
    run_string("when [:x = 5] [make \"fired :fired + 1]");
    run_string("cs");
    demons_poll();
    mock_device_clear_output();
    run_string("show :fired");
    TEST_ASSERT_EQUAL_STRING("0\n", mock_device_get_output());
}

//==========================================================================
// Poll budget
//==========================================================================

void test_maybe_poll_respects_budget(void)
{
    run_string("make \"x 0  make \"fired 0");
    run_string("when [:x = 5] [make \"fired :fired + 1]");

    set_mock_ticks(1000);
    demons_maybe_poll();   // observes the condition false (no fire)
    run_string("make \"x 5");

    set_mock_ticks(1010);  // < 20 ms later: gated, edge not seen yet
    demons_maybe_poll();
    mock_device_clear_output();
    run_string("show :fired");
    TEST_ASSERT_EQUAL_STRING("0\n", mock_device_get_output());

    set_mock_ticks(1030);  // >= 20 ms later: polls again, sees the edge
    demons_maybe_poll();
    mock_device_clear_output();
    run_string("show :fired");
    TEST_ASSERT_EQUAL_STRING("1\n", mock_device_get_output());
}

//==========================================================================
// Autonomous motion (setspeed / speed)
//==========================================================================

void test_setspeed_speed_roundtrip(void)
{
    run_string("setspeed 30");
    mock_device_clear_output();
    run_string("show speed");
    TEST_ASSERT_EQUAL_STRING("30\n", mock_device_get_output());
    TEST_ASSERT_EQUAL_FLOAT(30.0f, mock_device_get_turtle(0)->speed);
}

void test_setspeed_glides_turtle_on_tick(void)
{
    run_string("setspeed 100  seth 90");  // 100 steps/s heading east
    float x0 = mock_device_get_turtle(0)->x;

    set_mock_ticks(0);
    demons_poll();          // establishes the motion baseline (dt 0)
    set_mock_ticks(1000);
    demons_poll();          // 1 second elapsed -> +100 steps east

    float x1 = mock_device_get_turtle(0)->x;
    TEST_ASSERT_FLOAT_WITHIN(0.5f, x0 + 100.0f, x1);
}

void test_freeze_halts_motion(void)
{
    run_string("setspeed 100  seth 90");
    set_mock_ticks(0);
    demons_poll();
    run_string("freeze");
    float x0 = mock_device_get_turtle(0)->x;
    set_mock_ticks(5000);
    demons_poll();          // frozen: no motion
    TEST_ASSERT_EQUAL_FLOAT(x0, mock_device_get_turtle(0)->x);
}

void test_clearscreen_stops_motion(void)
{
    run_string("setspeed 100  seth 90");
    run_string("cs");
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_device_get_turtle(0)->speed);

    // A tick after cs must not move the turtle.
    set_mock_ticks(0);
    demons_poll();
    float x0 = mock_device_get_turtle(0)->x;
    set_mock_ticks(1000);
    demons_poll();
    TEST_ASSERT_EQUAL_FLOAT(x0, mock_device_get_turtle(0)->x);
}

void test_setspeed_rejects_negative(void)
{
    Result r = run_string("setspeed -5");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Animation (setanim)
//==========================================================================

void test_setanim_cycles_shape_on_tick(void)
{
    run_string("setsh 0");
    run_string("setanim 1 3 100");  // frames 1..3, 100 ms each

    set_mock_ticks(0);
    demons_poll();                  // baseline
    TEST_ASSERT_EQUAL_UINT8(0, mock_device_get_turtle(0)->shape);

    set_mock_ticks(100);
    demons_poll();
    TEST_ASSERT_EQUAL_UINT8(1, mock_device_get_turtle(0)->shape);

    set_mock_ticks(200);
    demons_poll();
    TEST_ASSERT_EQUAL_UINT8(2, mock_device_get_turtle(0)->shape);

    set_mock_ticks(300);
    demons_poll();
    TEST_ASSERT_EQUAL_UINT8(3, mock_device_get_turtle(0)->shape);

    set_mock_ticks(400);
    demons_poll();                  // wraps back to first
    TEST_ASSERT_EQUAL_UINT8(1, mock_device_get_turtle(0)->shape);
}

void test_setanim_zero_interval_stops(void)
{
    run_string("setsh 2");
    run_string("setanim 1 3 0");    // interval 0 -> not animating
    set_mock_ticks(0);
    demons_poll();
    set_mock_ticks(1000);
    demons_poll();
    TEST_ASSERT_EQUAL_UINT8(2, mock_device_get_turtle(0)->shape);
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_when_fires_on_false_to_true_edge);
    RUN_TEST(test_when_empty_action_disarms);
    RUN_TEST(test_when_rearm_replaces_action_and_resets_edge);
    RUN_TEST(test_when_table_full_errors);
    RUN_TEST(test_when_print_lists_armed_demons);
    RUN_TEST(test_when_action_error_propagates);
    RUN_TEST(test_when_bad_condition_errors);
    RUN_TEST(test_action_does_not_reenter_poll);
    RUN_TEST(test_freeze_suspends_demons);
    RUN_TEST(test_clearscreen_clears_demons);
    RUN_TEST(test_clearscreen_stops_motion);
    RUN_TEST(test_maybe_poll_respects_budget);
    RUN_TEST(test_setspeed_speed_roundtrip);
    RUN_TEST(test_setspeed_glides_turtle_on_tick);
    RUN_TEST(test_freeze_halts_motion);
    RUN_TEST(test_setspeed_rejects_negative);
    RUN_TEST(test_setanim_cycles_shape_on_tick);
    RUN_TEST(test_setanim_zero_interval_stops);

    return UNITY_END();
}
