//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Unit tests for time management primitives: date, time, setdate, settime
//

#include "test_scaffold.h"
#include "mock_device.h"

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

// ============================================================================
// date primitive tests
// ============================================================================

void test_date_outputs_list_with_three_elements(void)
{
    mock_device_set_time(2025, 6, 15, 10, 30, 45);

    Result r = eval_string("date");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    // Verify 3 elements: year, month, day
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));  // year
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));  // month
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));  // day
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(list)));  // no more
}

void test_date_outputs_correct_year(void)
{
    mock_device_set_time(2025, 6, 15, 10, 30, 45);

    Result r = eval_string("first date");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("2025", mem_word_ptr(r.value.as.node));
}

void test_date_outputs_correct_month(void)
{
    mock_device_set_time(2025, 6, 15, 10, 30, 45);

    Result r = eval_string("first butfirst date");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("6", mem_word_ptr(r.value.as.node));
}

void test_date_outputs_correct_day(void)
{
    mock_device_set_time(2025, 6, 15, 10, 30, 45);

    Result r = eval_string("last date");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("15", mem_word_ptr(r.value.as.node));
}

void test_date_outputs_different_values(void)
{
    mock_device_set_time(2024, 12, 31, 23, 59, 59);

    Result r = eval_string("first date");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("2024", mem_word_ptr(r.value.as.node));
}

void test_date_error_when_not_available(void)
{
    mock_device_set_time_enabled(false, true, true, true);

    Result r = eval_string("date");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// time primitive tests
// ============================================================================

void test_time_outputs_list_with_three_elements(void)
{
    mock_device_set_time(2025, 6, 15, 10, 30, 45);

    Result r = eval_string("time");

    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    // Verify 3 elements: hour, minute, second
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));  // hour
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));  // minute
    list = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(list));  // second
    TEST_ASSERT_TRUE(mem_is_nil(mem_cdr(list)));  // no more
}

void test_time_outputs_correct_hour(void)
{
    mock_device_set_time(2025, 6, 15, 10, 30, 45);

    Result r = eval_string("first time");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("10", mem_word_ptr(r.value.as.node));
}

void test_time_outputs_correct_minute(void)
{
    mock_device_set_time(2025, 6, 15, 10, 30, 45);

    Result r = eval_string("first butfirst time");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("30", mem_word_ptr(r.value.as.node));
}

void test_time_outputs_correct_second(void)
{
    mock_device_set_time(2025, 6, 15, 10, 30, 45);

    Result r = eval_string("last time");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("45", mem_word_ptr(r.value.as.node));
}

void test_time_outputs_midnight(void)
{
    mock_device_set_time(2025, 1, 1, 0, 0, 0);

    Result r = eval_string("first time");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(r.value.as.node));
}

void test_time_outputs_end_of_day(void)
{
    mock_device_set_time(2025, 1, 1, 23, 59, 59);

    Result r = eval_string("first time");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("23", mem_word_ptr(r.value.as.node));
}

void test_time_error_when_not_available(void)
{
    mock_device_set_time_enabled(true, false, true, true);

    Result r = eval_string("time");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// setdate primitive tests
// ============================================================================

void test_setdate_sets_date(void)
{
    mock_device_set_time(2025, 1, 1, 12, 0, 0);

    Result r = run_string("setdate [2030 7 20]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // Verify date was changed
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT(2030, state->time.year);
    TEST_ASSERT_EQUAL_INT(7, state->time.month);
    TEST_ASSERT_EQUAL_INT(20, state->time.day);
}

void test_setdate_preserves_time(void)
{
    mock_device_set_time(2025, 1, 1, 15, 45, 30);

    Result r = run_string("setdate [2030 7 20]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // Verify time was preserved
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT(15, state->time.hour);
    TEST_ASSERT_EQUAL_INT(45, state->time.minute);
    TEST_ASSERT_EQUAL_INT(30, state->time.second);
}

void test_setdate_rejects_invalid_month_high(void)
{
    mock_device_set_time(2025, 1, 1, 12, 0, 0);

    Result r = run_string("setdate [2025 13 1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setdate_rejects_invalid_month_low(void)
{
    mock_device_set_time(2025, 1, 1, 12, 0, 0);

    Result r = run_string("setdate [2025 0 1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setdate_rejects_invalid_day_high(void)
{
    mock_device_set_time(2025, 1, 1, 12, 0, 0);

    Result r = run_string("setdate [2025 2 32]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setdate_rejects_invalid_day_low(void)
{
    mock_device_set_time(2025, 1, 1, 12, 0, 0);

    Result r = run_string("setdate [2025 2 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setdate_rejects_non_list(void)
{
    Result r = run_string("setdate 2025");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setdate_rejects_empty_list(void)
{
    Result r = run_string("setdate []");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setdate_rejects_too_few_elements(void)
{
    Result r = run_string("setdate [2025 1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setdate_rejects_too_many_elements(void)
{
    Result r = run_string("setdate [2025 1 1 1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setdate_rejects_non_numbers(void)
{
    Result r = run_string("setdate [abc 1 1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setdate_error_when_not_available(void)
{
    mock_device_set_time_enabled(true, true, false, true);

    Result r = run_string("setdate [2025 1 1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// settime primitive tests
// ============================================================================

void test_settime_sets_time(void)
{
    mock_device_set_time(2025, 6, 15, 12, 0, 0);

    Result r = run_string("settime [18 30 45]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // Verify time was changed
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT(18, state->time.hour);
    TEST_ASSERT_EQUAL_INT(30, state->time.minute);
    TEST_ASSERT_EQUAL_INT(45, state->time.second);
}

void test_settime_preserves_date(void)
{
    mock_device_set_time(2025, 6, 15, 12, 0, 0);

    Result r = run_string("settime [18 30 45]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // Verify date was preserved
    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT(2025, state->time.year);
    TEST_ASSERT_EQUAL_INT(6, state->time.month);
    TEST_ASSERT_EQUAL_INT(15, state->time.day);
}

void test_settime_accepts_midnight(void)
{
    mock_device_set_time(2025, 6, 15, 12, 0, 0);

    Result r = run_string("settime [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT(0, state->time.hour);
    TEST_ASSERT_EQUAL_INT(0, state->time.minute);
    TEST_ASSERT_EQUAL_INT(0, state->time.second);
}

void test_settime_accepts_end_of_day(void)
{
    mock_device_set_time(2025, 6, 15, 12, 0, 0);

    Result r = run_string("settime [23 59 59]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *state = mock_device_get_state();
    TEST_ASSERT_EQUAL_INT(23, state->time.hour);
    TEST_ASSERT_EQUAL_INT(59, state->time.minute);
    TEST_ASSERT_EQUAL_INT(59, state->time.second);
}

void test_settime_rejects_invalid_hour_high(void)
{
    Result r = run_string("settime [24 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_rejects_invalid_hour_negative(void)
{
    Result r = run_string("settime [-1 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_rejects_invalid_minute_high(void)
{
    Result r = run_string("settime [12 60 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_rejects_invalid_minute_negative(void)
{
    Result r = run_string("settime [12 -1 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_rejects_invalid_second_high(void)
{
    Result r = run_string("settime [12 30 60]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_rejects_invalid_second_negative(void)
{
    Result r = run_string("settime [12 30 -1]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_rejects_non_list(void)
{
    Result r = run_string("settime 12");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_rejects_empty_list(void)
{
    Result r = run_string("settime []");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_rejects_too_few_elements(void)
{
    Result r = run_string("settime [12 30]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_rejects_too_many_elements(void)
{
    Result r = run_string("settime [12 30 45 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_rejects_non_numbers(void)
{
    Result r = run_string("settime [abc 30 45]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_settime_error_when_not_available(void)
{
    mock_device_set_time_enabled(true, true, true, false);

    Result r = run_string("settime [12 30 45]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// Integration tests
// ============================================================================

void test_date_and_setdate_roundtrip(void)
{
    mock_device_set_time(2025, 1, 1, 12, 0, 0);

    // Set a new date
    Result r = run_string("setdate [2030 7 20]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // Read it back
    r = eval_string("first date");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("2030", mem_word_ptr(r.value.as.node));

    r = eval_string("first butfirst date");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("7", mem_word_ptr(r.value.as.node));

    r = eval_string("last date");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("20", mem_word_ptr(r.value.as.node));
}

void test_time_and_settime_roundtrip(void)
{
    mock_device_set_time(2025, 1, 1, 12, 0, 0);

    // Set a new time
    Result r = run_string("settime [18 30 45]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    // Read it back
    r = eval_string("first time");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("18", mem_word_ptr(r.value.as.node));

    r = eval_string("first butfirst time");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("30", mem_word_ptr(r.value.as.node));

    r = eval_string("last time");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL_STRING("45", mem_word_ptr(r.value.as.node));
}

// ============================================================================
// Main
// ============================================================================

int main(void)
{
    UNITY_BEGIN();

    // date tests
    RUN_TEST(test_date_outputs_list_with_three_elements);
    RUN_TEST(test_date_outputs_correct_year);
    RUN_TEST(test_date_outputs_correct_month);
    RUN_TEST(test_date_outputs_correct_day);
    RUN_TEST(test_date_outputs_different_values);
    RUN_TEST(test_date_error_when_not_available);

    // time tests
    RUN_TEST(test_time_outputs_list_with_three_elements);
    RUN_TEST(test_time_outputs_correct_hour);
    RUN_TEST(test_time_outputs_correct_minute);
    RUN_TEST(test_time_outputs_correct_second);
    RUN_TEST(test_time_outputs_midnight);
    RUN_TEST(test_time_outputs_end_of_day);
    RUN_TEST(test_time_error_when_not_available);

    // setdate tests
    RUN_TEST(test_setdate_sets_date);
    RUN_TEST(test_setdate_preserves_time);
    RUN_TEST(test_setdate_rejects_invalid_month_high);
    RUN_TEST(test_setdate_rejects_invalid_month_low);
    RUN_TEST(test_setdate_rejects_invalid_day_high);
    RUN_TEST(test_setdate_rejects_invalid_day_low);
    RUN_TEST(test_setdate_rejects_non_list);
    RUN_TEST(test_setdate_rejects_empty_list);
    RUN_TEST(test_setdate_rejects_too_few_elements);
    RUN_TEST(test_setdate_rejects_too_many_elements);
    RUN_TEST(test_setdate_rejects_non_numbers);
    RUN_TEST(test_setdate_error_when_not_available);

    // settime tests
    RUN_TEST(test_settime_sets_time);
    RUN_TEST(test_settime_preserves_date);
    RUN_TEST(test_settime_accepts_midnight);
    RUN_TEST(test_settime_accepts_end_of_day);
    RUN_TEST(test_settime_rejects_invalid_hour_high);
    RUN_TEST(test_settime_rejects_invalid_hour_negative);
    RUN_TEST(test_settime_rejects_invalid_minute_high);
    RUN_TEST(test_settime_rejects_invalid_minute_negative);
    RUN_TEST(test_settime_rejects_invalid_second_high);
    RUN_TEST(test_settime_rejects_invalid_second_negative);
    RUN_TEST(test_settime_rejects_non_list);
    RUN_TEST(test_settime_rejects_empty_list);
    RUN_TEST(test_settime_rejects_too_few_elements);
    RUN_TEST(test_settime_rejects_too_many_elements);
    RUN_TEST(test_settime_rejects_non_numbers);
    RUN_TEST(test_settime_error_when_not_available);

    // Integration tests
    RUN_TEST(test_date_and_setdate_roundtrip);
    RUN_TEST(test_time_and_settime_roundtrip);

    return UNITY_END();
}
