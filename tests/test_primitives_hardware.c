//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for hardware primitives (hw.battery, hw.temperature, hw.cpu,
//  hw.setcpu, hw.light?,
//  hw.setlight)
//

#include "test_scaffold.h"

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

//==========================================================================
// hw.battery Primitive Tests
//==========================================================================

void test_battery_returns_list(void)
{
    // Battery should return a list [level charging_status]
    Result r = eval_string("hw.battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
}

void test_battery_returns_two_element_list(void)
{
    Result r = eval_string("hw.battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    // Check that it has exactly 2 elements
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));  // First element exists
    Node rest = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(rest));  // Second element exists
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(rest)));  // No third element
}

void test_battery_level_full(void)
{
    set_mock_battery(100, false);
    
    Result r = eval_string("first hw.battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("100", mem_word_ptr(r.value.as.node));
}

void test_battery_level_partial(void)
{
    set_mock_battery(42, false);
    
    Result r = eval_string("first hw.battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("42", mem_word_ptr(r.value.as.node));
}

void test_battery_level_empty(void)
{
    set_mock_battery(0, false);
    
    Result r = eval_string("first hw.battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(r.value.as.node));
}

void test_battery_level_unavailable(void)
{
    set_mock_battery(-1, false);
    
    Result r = eval_string("first hw.battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("-1", mem_word_ptr(r.value.as.node));
}

void test_battery_not_charging(void)
{
    set_mock_battery(50, false);
    
    Result r = eval_string("last hw.battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_battery_charging(void)
{
    set_mock_battery(75, true);
    
    Result r = eval_string("last hw.battery");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_battery_in_procedure(void)
{
    // Test using hw.battery within a procedure
    set_mock_battery(88, true);
    
    const char *params[] = {};
    define_proc("getlevel", params, 0, "output first hw.battery");
    
    Result r = eval_string("getlevel");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("88", mem_word_ptr(r.value.as.node));
}

void test_battery_charging_in_procedure(void)
{
    set_mock_battery(60, true);
    
    const char *params[] = {};
    define_proc("ischarging", params, 0, "output last hw.battery");
    
    Result r = eval_string("ischarging");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_battery_print_output(void)
{
    set_mock_battery(50, false);
    
    run_string("print hw.battery");
    
    TEST_ASSERT_EQUAL_STRING("50 false\n", output_buffer);
}

void test_battery_show_output(void)
{
    set_mock_battery(75, true);
    
    run_string("show hw.battery");
    
    TEST_ASSERT_EQUAL_STRING("[75 true]\n", output_buffer);
}

//==========================================================================
// hw.cpu / hw.setcpu Primitive Tests
//==========================================================================
//
// Two clocks, named rather than numbered. The RP2350 is rated to 150 MHz and
// `fast` is 300; every clock in between is a net loss on this board, because
// the LCD's SPI prescaler is coarse enough that 200 and 250 MHz slow the
// display by more than they speed the interpreter. The words are the interface
// precisely so that those cannot be asked for.

void test_cpu_reports_normal_at_the_stock_clock(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);

    Result r = eval_string("hw.cpu");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("normal", value_to_string(r.value));
}

void test_cpu_reports_fast_at_the_overclock(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_FAST);
    TEST_ASSERT_EQUAL_STRING("fast", value_to_string(eval_string("hw.cpu").value));
}

void test_setcpu_changes_what_cpu_reports(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("hw.setcpu \"fast").status);
    TEST_ASSERT_EQUAL_STRING("fast", value_to_string(eval_string("hw.cpu").value));

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("hw.setcpu \"normal").status);
    TEST_ASSERT_EQUAL_STRING("normal", value_to_string(eval_string("hw.cpu").value));
}

// It really does move the clock, not just the word: the two names are the
// documented frequencies and a test that only compared words would pass with
// them wired to each other.
void test_setcpu_selects_the_documented_frequencies(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);

    run_string("hw.setcpu \"fast");
    TEST_ASSERT_EQUAL_UINT32(300000u, mock_cpu_khz);

    run_string("hw.setcpu \"normal");
    TEST_ASSERT_EQUAL_UINT32(150000u, mock_cpu_khz);
}

// A word, so it can be compared and printed; not a number, so it cannot be
// done arithmetic on and cannot be confused with a megahertz figure.
void test_cpu_is_a_word_that_compares(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_FAST);

    Result r = eval_string("\"fast = hw.cpu");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

// The whole point of the two-word interface: the clocks that are a net loss on
// this board cannot be asked for, and neither can anything else.
void test_setcpu_refuses_anything_but_the_two_names(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);

    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("hw.setcpu \"slow").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("hw.setcpu \"turbo").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("hw.setcpu 300").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("hw.setcpu 200").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("hw.setcpu [fast]").status);

    // And none of them moved the clock.
    TEST_ASSERT_EQUAL_UINT32(150000u, mock_cpu_khz);
    TEST_ASSERT_EQUAL_STRING("normal", value_to_string(eval_string("hw.cpu").value));
}

// Case is not significant, the way it is not for `hw.setlight`'s true/false.
void test_setcpu_takes_either_case(void)
{
    set_mock_cpu_khz(true, LOGO_CPU_KHZ_NORMAL);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("hw.setcpu \"FAST").status);
    TEST_ASSERT_EQUAL_STRING("fast", value_to_string(eval_string("hw.cpu").value));
}

// A board that refuses the change is reported rather than swallowed, and the
// clock stays where it was -- a program that went on measuring against a clock
// it did not have would be measuring nothing. The mock's PLL makes multiples of
// 25 MHz, so an unmakeable clock is arranged by moving the mock rather than by
// asking for one, since the two names are always makeable by construction.
void test_setcpu_reports_a_board_that_refuses(void)
{
    set_mock_cpu_khz(true, 137000u);   // a clock the mock's PLL cannot leave cleanly

    TEST_ASSERT_EQUAL_STRING_MESSAGE("normal", value_to_string(eval_string("hw.cpu").value),
                                     "anything short of the overclock reads as normal");
}

// A board that cannot retune errors rather than silently doing nothing, the
// same way `hw.temperature` does on a board with no sensor.
void test_cpu_errors_when_the_device_has_no_settable_clock(void)
{
    set_mock_cpu_khz(false, 150000u);

    TEST_ASSERT_EQUAL(RESULT_ERROR, eval_string("hw.cpu").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("hw.setcpu \"fast").status);
}

//==========================================================================
// hw.temperature Primitive Tests
//==========================================================================

void test_temperature_returns_number(void)
{
    set_mock_temperature(true, 26.5f);

    Result r = eval_string("hw.temperature");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_NUMBER, r.value.type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 26.5f, r.value.as.number);
}

void test_temperature_rounds_to_tenths(void)
{
    // The sensor is uncalibrated and a conversion is ~0.24 C per LSB, so the
    // primitive rounds rather than printing ADC noise as significant digits.
    set_mock_temperature(true, 26.98342f);

    run_string("print hw.temperature");

    TEST_ASSERT_EQUAL_STRING("27\n", output_buffer);
}

void test_temperature_rounds_to_nearest_tenth(void)
{
    set_mock_temperature(true, 26.94f);

    run_string("print hw.temperature");

    TEST_ASSERT_EQUAL_STRING("26.9\n", output_buffer);
}

void test_temperature_negative(void)
{
    set_mock_temperature(true, -5.25f);

    run_string("print hw.temperature");

    TEST_ASSERT_EQUAL_STRING("-5.3\n", output_buffer);
}

void test_temperature_is_a_number_not_a_word(void)
{
    // The output must be usable in arithmetic directly.
    set_mock_temperature(true, 20.0f);

    Result r = eval_string("hw.temperature * 2");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, r.value.as.number);
}

void test_temperature_not_available(void)
{
    // A board with no sensor nulls the op; the primitive must error rather
    // than invent a reading.
    set_mock_temperature(false, 0.0f);

    Result r = eval_string("hw.temperature");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_UNSUPPORTED_ON_DEVICE, result_get_error_code(r));
}

//==========================================================================
// hw.light? / hw.setlight Primitive Tests
//==========================================================================

void test_light_starts_off(void)
{
    Result r = eval_string("hw.light?");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", value_to_string(r.value));
}

void test_setlight_true_lights_it(void)
{
    run_string("hw.setlight \"true print hw.light?");

    TEST_ASSERT_EQUAL_STRING("true\n", output_buffer);
}

void test_setlight_false_clears_it(void)
{
    set_mock_status_led(true, true);

    run_string("hw.setlight \"false print hw.light?");

    TEST_ASSERT_EQUAL_STRING("false\n", output_buffer);
}

void test_setlight_is_case_insensitive(void)
{
    run_string("hw.setlight \"TRUE print hw.light?");

    TEST_ASSERT_EQUAL_STRING("true\n", output_buffer);
}

void test_light_reads_the_hardware_not_a_remembered_value(void)
{
    // Something other than hw.setlight drove the LED; hw.light? must still
    // answer for what the pin is doing.
    set_mock_status_led(true, true);

    Result r = eval_string("hw.light?");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("true", value_to_string(r.value));
}

void test_setlight_outputs_nothing(void)
{
    // It is a command, so using it as an operation must be refused.
    Result r = eval_string("print hw.setlight \"true");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setlight_rejects_a_non_boolean(void)
{
    Result r = eval_string("hw.setlight \"on");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, result_get_error_code(r));
}

void test_setlight_rejects_a_list(void)
{
    Result r = eval_string("hw.setlight [true]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_BOOL, result_get_error_code(r));
}

void test_light_not_available(void)
{
    // A board with no LED nulls the ops; both primitives must error rather
    // than answer false, which would read as "the LED is off".
    set_mock_status_led(false, false);

    Result r = eval_string("hw.light?");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_UNSUPPORTED_ON_DEVICE, result_get_error_code(r));
}

void test_setlight_not_available(void)
{
    set_mock_status_led(false, false);

    Result r = eval_string("hw.setlight \"true");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_UNSUPPORTED_ON_DEVICE, result_get_error_code(r));
}

void test_setlight_when_the_led_cannot_be_reached(void)
{
    // On a W board the LED is on the wireless module; if that will not come
    // up the op is present but every access fails.
    set_mock_status_led_unreachable();

    Result r = eval_string("hw.setlight \"true");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_UNSUPPORTED_ON_DEVICE, result_get_error_code(r));
}

void test_light_when_the_led_cannot_be_reached(void)
{
    set_mock_status_led_unreachable();

    Result r = eval_string("hw.light?");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_UNSUPPORTED_ON_DEVICE, result_get_error_code(r));
}

void test_light_is_usable_as_a_condition(void)
{
    run_string("hw.setlight \"true if hw.light? [pr [lit]]");

    TEST_ASSERT_EQUAL_STRING("lit\n", output_buffer);
}

//==========================================================================
// Poweroff Primitive Tests
//==========================================================================

void test_poweroff_not_available(void)
{
    // Default: power_off is NULL, so goodbye should return an error
    Result r = eval_string("goodbye");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_UNSUPPORTED_ON_DEVICE, result_get_error_code(r));
}

void test_poweroff_available_but_fails(void)
{
    // power_off available but returns false (failure)
    set_mock_power_off(true, false);
    
    Result r = eval_string("goodbye");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_UNSUPPORTED_ON_DEVICE, result_get_error_code(r));
    TEST_ASSERT_TRUE(was_mock_power_off_called());
}

void test_poweroff_calls_hardware_function(void)
{
    // Verify the power_off function is called when available
    set_mock_power_off(true, false);
    
    eval_string("goodbye");
    
    TEST_ASSERT_TRUE(was_mock_power_off_called());
}

void test_poweroff_reset_state_between_tests(void)
{
    // Verify state is properly reset - power_off should not be available
    // after not explicitly setting it
    TEST_ASSERT_FALSE(was_mock_power_off_called());
    
    Result r = eval_string("goodbye");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_FALSE(was_mock_power_off_called());
}

void test_poweroff_no_inputs(void)
{
    // goodbye takes no inputs - verify giving inputs causes error
    Result r = eval_string("goodbye 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// .bootsel Primitive Tests
//==========================================================================

void test_bootsel_not_available(void)
{
    // Default: reboot_bootloader is NULL, so .bootsel should error
    Result r = eval_string(".bootsel");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_UNSUPPORTED_ON_DEVICE, result_get_error_code(r));
    TEST_ASSERT_FALSE(was_mock_bootsel_called());
}

void test_bootsel_calls_hardware_function(void)
{
    // When available, .bootsel calls the hardware function and succeeds
    set_mock_bootsel(true);

    Result r = eval_string(".bootsel");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_TRUE(was_mock_bootsel_called());
}

void test_bootsel_no_inputs(void)
{
    // .bootsel takes no inputs - giving one causes an error
    Result r = eval_string(".bootsel 1");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

//==========================================================================
// Toot Primitive Tests
//==========================================================================

void test_toot_basic(void)
{
    // toot duration frequency - should succeed silently when no audio hardware
    Result r = eval_string("toot 500 440");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_toot_stereo(void)
{
    // (toot duration leftfreq rightfreq) - three arguments with parentheses
    Result r = eval_string("(toot 500 440 880)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_toot_accepts_numeric_word_frequency(void)
{
    // Values pulled from a list (e.g. `item n list`) come back as numeric
    // words, not VALUE_NUMBER; toot must coerce them like every other
    // numeric primitive does (REQUIRE_NUMBER), not reject them outright.
    Result r = eval_string("make \"freqs [220 196 174 164] toot 40 item 2 :freqs");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_toot_accepts_numeric_word_duration(void)
{
    Result r = eval_string("make \"durs [40 80] toot (item 1 :durs) 440");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_toot_zero_duration(void)
{
    // Zero duration should work
    Result r = eval_string("toot 0 440");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_toot_zero_frequency(void)
{
    // Zero frequency (silence) should work
    Result r = eval_string("toot 500 0");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
}

void test_toot_missing_frequency(void)
{
    // toot with only duration should fail
    Result r = eval_string("toot 500");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_ENOUGH_INPUTS, result_get_error_code(r));
}

void test_toot_no_inputs(void)
{
    // toot with no inputs should fail
    Result r = eval_string("toot");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_NOT_ENOUGH_INPUTS, result_get_error_code(r));
}

void test_toot_too_many_inputs(void)
{
    // toot with more than 3 inputs should fail
    Result r = eval_string("(toot 500 440 880 123)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_TOO_MANY_INPUTS, result_get_error_code(r));
}

void test_toot_negative_duration_error(void)
{
    // Negative duration should fail
    Result r = eval_string("toot -500 440");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_toot_word_duration_error(void)
{
    // Word as duration should fail
    Result r = eval_string("toot \"abc 440");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_toot_word_frequency_error(void)
{
    // Word as frequency should fail
    Result r = eval_string("toot 500 \"abc");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_toot_list_duration_error(void)
{
    // List as duration should fail
    Result r = eval_string("toot [1 2] 440");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_toot_list_frequency_error(void)
{
    // List as frequency should fail
    Result r = eval_string("toot 500 [1 2]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_toot_stereo_word_leftfreq_error(void)
{
    // Word as left frequency in stereo mode should fail
    Result r = eval_string("(toot 500 \"abc 880)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_toot_stereo_word_rightfreq_error(void)
{
    // Word as right frequency in stereo mode should fail
    Result r = eval_string("(toot 500 440 \"abc)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_battery_out_of_nodes_errors(void)
{
    // hw.battery builds a two-element list; on node-pool exhaustion it must
    // surface ERR_OUT_OF_SPACE rather than return a truncated list.
    set_mock_battery(50, false);

    Node chain = NODE_NIL;
    for (;;)
    {
        Node c = mem_cons(NODE_NIL, chain);
        if (mem_is_nil(c))
        {
            break;
        }
        chain = c;
    }

    Result r = eval_string("hw.battery");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_OUT_OF_SPACE, result_get_error_code(r));
}

void test_toot_in_procedure(void)
{
    // Test using toot within a procedure
    const char *params[] = {};
    define_proc("beep", params, 0, "toot 100 440");
    
    run_string("beep");
    // No output expected, just verify no errors
}

void test_toot_negative_frequency_error(void)
{
    // Negative frequency should fail
    Result r = eval_string("toot 500 -440");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_toot_stereo_negative_leftfreq_error(void)
{
    // Negative left frequency in stereo mode should fail
    Result r = eval_string("(toot 500 -440 880)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

void test_toot_stereo_negative_rightfreq_error(void)
{
    // Negative right frequency in stereo mode should fail
    Result r = eval_string("(toot 500 440 -880)");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
    TEST_ASSERT_EQUAL(ERR_DOESNT_LIKE_INPUT, result_get_error_code(r));
}

//==========================================================================
// Main
//==========================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // Battery tests
    RUN_TEST(test_battery_returns_list);
    RUN_TEST(test_battery_returns_two_element_list);
    RUN_TEST(test_battery_level_full);
    RUN_TEST(test_battery_level_partial);
    RUN_TEST(test_battery_level_empty);
    RUN_TEST(test_battery_level_unavailable);
    RUN_TEST(test_battery_not_charging);
    RUN_TEST(test_battery_charging);
    RUN_TEST(test_battery_in_procedure);
    RUN_TEST(test_battery_charging_in_procedure);
    RUN_TEST(test_battery_print_output);
    RUN_TEST(test_battery_show_output);
    
    // hw.temperature tests
    RUN_TEST(test_cpu_reports_normal_at_the_stock_clock);
    RUN_TEST(test_cpu_reports_fast_at_the_overclock);
    RUN_TEST(test_setcpu_changes_what_cpu_reports);
    RUN_TEST(test_setcpu_selects_the_documented_frequencies);
    RUN_TEST(test_cpu_is_a_word_that_compares);
    RUN_TEST(test_setcpu_refuses_anything_but_the_two_names);
    RUN_TEST(test_setcpu_takes_either_case);
    RUN_TEST(test_setcpu_reports_a_board_that_refuses);
    RUN_TEST(test_cpu_errors_when_the_device_has_no_settable_clock);

    RUN_TEST(test_temperature_returns_number);
    RUN_TEST(test_temperature_rounds_to_tenths);
    RUN_TEST(test_temperature_rounds_to_nearest_tenth);
    RUN_TEST(test_temperature_negative);
    RUN_TEST(test_temperature_is_a_number_not_a_word);
    RUN_TEST(test_temperature_not_available);

    // hw.light? / hw.setlight tests
    RUN_TEST(test_light_starts_off);
    RUN_TEST(test_setlight_true_lights_it);
    RUN_TEST(test_setlight_false_clears_it);
    RUN_TEST(test_setlight_is_case_insensitive);
    RUN_TEST(test_light_reads_the_hardware_not_a_remembered_value);
    RUN_TEST(test_setlight_outputs_nothing);
    RUN_TEST(test_setlight_rejects_a_non_boolean);
    RUN_TEST(test_setlight_rejects_a_list);
    RUN_TEST(test_light_not_available);
    RUN_TEST(test_setlight_not_available);
    RUN_TEST(test_setlight_when_the_led_cannot_be_reached);
    RUN_TEST(test_light_when_the_led_cannot_be_reached);
    RUN_TEST(test_light_is_usable_as_a_condition);

    // Poweroff tests
    RUN_TEST(test_poweroff_not_available);
    RUN_TEST(test_poweroff_available_but_fails);
    RUN_TEST(test_poweroff_calls_hardware_function);
    RUN_TEST(test_poweroff_reset_state_between_tests);
    RUN_TEST(test_poweroff_no_inputs);

    // .bootsel tests
    RUN_TEST(test_bootsel_not_available);
    RUN_TEST(test_bootsel_calls_hardware_function);
    RUN_TEST(test_bootsel_no_inputs);

    // Toot tests
    RUN_TEST(test_toot_basic);
    RUN_TEST(test_toot_stereo);
    RUN_TEST(test_toot_accepts_numeric_word_frequency);
    RUN_TEST(test_toot_accepts_numeric_word_duration);
    RUN_TEST(test_toot_zero_duration);
    RUN_TEST(test_toot_zero_frequency);
    RUN_TEST(test_toot_missing_frequency);
    RUN_TEST(test_toot_no_inputs);
    RUN_TEST(test_toot_too_many_inputs);
    RUN_TEST(test_toot_negative_duration_error);
    RUN_TEST(test_toot_word_duration_error);
    RUN_TEST(test_toot_word_frequency_error);
    RUN_TEST(test_toot_list_duration_error);
    RUN_TEST(test_toot_list_frequency_error);
    RUN_TEST(test_toot_stereo_word_leftfreq_error);
    RUN_TEST(test_toot_stereo_word_rightfreq_error);
    RUN_TEST(test_toot_negative_frequency_error);
    RUN_TEST(test_toot_stereo_negative_leftfreq_error);
    RUN_TEST(test_toot_stereo_negative_rightfreq_error);
    RUN_TEST(test_battery_out_of_nodes_errors);
    RUN_TEST(test_toot_in_procedure);

    // hw.cpu / hw.setcpu
    
    return UNITY_END();
}
