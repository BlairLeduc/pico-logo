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
// addtime primitive tests
// ============================================================================

void test_addtime_simple_addition(void)
{
    Result r = eval_string("addtime [1 2 3] [4 5 6]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    // Result should be [5 7 9]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("5", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("7", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("9", mem_word_ptr(mem_car(list)));
}

void test_addtime_seconds_overflow(void)
{
    // 30 + 45 = 75 seconds = 1 minute 15 seconds
    Result r = eval_string("addtime [0 0 30] [0 0 45]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("15", mem_word_ptr(mem_car(list)));
}

void test_addtime_minutes_overflow(void)
{
    // 45 + 30 = 75 minutes = 1 hour 15 minutes
    Result r = eval_string("addtime [0 45 0] [0 30 0]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("15", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
}

void test_addtime_cascading_overflow(void)
{
    // 23:59:59 + 0:0:2 = 24:00:01
    Result r = eval_string("addtime [23 59 59] [0 0 2]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("24", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
}

void test_addtime_negative_offset(void)
{
    // 10:30:45 + 0:0:-45 = 10:30:0
    Result r = eval_string("addtime [10 30 45] [0 0 -45]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("10", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("30", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
}

void test_addtime_negative_with_borrow(void)
{
    // 10:30:00 + 0:0:-1 = 10:29:59
    Result r = eval_string("addtime [10 30 0] [0 0 -1]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("10", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("29", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("59", mem_word_ptr(mem_car(list)));
}

void test_addtime_negative_hours(void)
{
    // 10:30:45 + -2:0:0 = 8:30:45
    Result r = eval_string("addtime [10 30 45] [-2 0 0]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("8", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("30", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("45", mem_word_ptr(mem_car(list)));
}

void test_addtime_zero_offset(void)
{
    Result r = eval_string("addtime [10 30 45] [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("10", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("30", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("45", mem_word_ptr(mem_car(list)));
}

void test_addtime_rejects_non_list_first(void)
{
    Result r = eval_string("addtime 123 [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_addtime_rejects_non_list_second(void)
{
    Result r = eval_string("addtime [0 0 0] 123");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_addtime_rejects_empty_list(void)
{
    Result r = eval_string("addtime [] [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_addtime_rejects_negative_base_time(void)
{
    Result r = eval_string("addtime [-1 0 0] [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// adddate primitive tests
// ============================================================================

void test_adddate_simple_addition(void)
{
    Result r = eval_string("adddate [2025 1 1] [0 0 5]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    // Result should be [2025 1 6]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2025", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("6", mem_word_ptr(mem_car(list)));
}

void test_adddate_day_overflow(void)
{
    // Jan 30 + 5 days = Feb 4
    Result r = eval_string("adddate [2025 1 30] [0 0 5]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2025", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("4", mem_word_ptr(mem_car(list)));
}

void test_adddate_month_overflow(void)
{
    // Add 3 months from November = February next year
    Result r = eval_string("adddate [2025 11 15] [0 3 0]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2026", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("15", mem_word_ptr(mem_car(list)));
}

void test_adddate_year_rollover(void)
{
    // Dec 31 + 1 day = Jan 1 next year
    Result r = eval_string("adddate [2025 12 31] [0 0 1]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2026", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
}

void test_adddate_leap_year(void)
{
    // Feb 28, 2024 (leap year) + 1 day = Feb 29
    Result r = eval_string("adddate [2024 2 28] [0 0 1]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2024", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("29", mem_word_ptr(mem_car(list)));
}

void test_adddate_non_leap_year(void)
{
    // Feb 28, 2025 (not leap year) + 1 day = March 1
    Result r = eval_string("adddate [2025 2 28] [0 0 1]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2025", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("3", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
}

void test_adddate_negative_days(void)
{
    // Jan 5 - 5 days = Dec 31 previous year
    Result r = eval_string("adddate [2025 1 5] [0 0 -5]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2024", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("12", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("31", mem_word_ptr(mem_car(list)));
}

void test_adddate_negative_months(void)
{
    // March 15 - 2 months = January 15
    Result r = eval_string("adddate [2025 3 15] [0 -2 0]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2025", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("15", mem_word_ptr(mem_car(list)));
}

void test_adddate_zero_offset(void)
{
    Result r = eval_string("adddate [2025 6 15] [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2025", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("6", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("15", mem_word_ptr(mem_car(list)));
}

void test_adddate_rejects_non_list(void)
{
    Result r = eval_string("adddate 2025 [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_adddate_rejects_empty_list(void)
{
    Result r = eval_string("adddate [] [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_adddate_rejects_invalid_base_month(void)
{
    Result r = eval_string("adddate [2025 0 1] [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_adddate_rejects_invalid_base_day(void)
{
    Result r = eval_string("adddate [2025 1 0] [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// difftime primitive tests
// ============================================================================

void test_difftime_simple(void)
{
    Result r = eval_string("difftime [10 30 45] [8 20 15]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    // Result should be [2 10 30]
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("10", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("30", mem_word_ptr(mem_car(list)));
}

void test_difftime_same_time(void)
{
    Result r = eval_string("difftime [10 30 45] [10 30 45]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
}

void test_difftime_negative_result(void)
{
    // 8:20:15 - 10:30:45 = negative
    Result r = eval_string("difftime [8 20 15] [10 30 45]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // Result should be [-2 10 30] (negative hours)
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("-2", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("10", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("30", mem_word_ptr(mem_car(list)));
}

void test_difftime_seconds_only(void)
{
    Result r = eval_string("difftime [0 0 45] [0 0 15]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("30", mem_word_ptr(mem_car(list)));
}

void test_difftime_borrow_required(void)
{
    // 10:30:15 - 8:45:30 requires borrowing
    Result r = eval_string("difftime [10 30 15] [8 45 30]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    // 10:30:15 - 8:45:30 = 1:44:45
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("1", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("44", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("45", mem_word_ptr(mem_car(list)));
}

void test_difftime_zero_times(void)
{
    Result r = eval_string("difftime [0 0 0] [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    
    Node list = r.value.as.node;
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
    list = mem_cdr(list);
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(mem_car(list)));
}

void test_difftime_rejects_non_list(void)
{
    Result r = eval_string("difftime 123 [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_difftime_rejects_empty_list(void)
{
    Result r = eval_string("difftime [] [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_difftime_rejects_negative_first_time(void)
{
    Result r = eval_string("difftime [-1 0 0] [0 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_difftime_rejects_negative_second_time(void)
{
    Result r = eval_string("difftime [0 0 0] [-1 0 0]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
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

    // addtime tests
    RUN_TEST(test_addtime_simple_addition);
    RUN_TEST(test_addtime_seconds_overflow);
    RUN_TEST(test_addtime_minutes_overflow);
    RUN_TEST(test_addtime_cascading_overflow);
    RUN_TEST(test_addtime_negative_offset);
    RUN_TEST(test_addtime_negative_with_borrow);
    RUN_TEST(test_addtime_negative_hours);
    RUN_TEST(test_addtime_zero_offset);
    RUN_TEST(test_addtime_rejects_non_list_first);
    RUN_TEST(test_addtime_rejects_non_list_second);
    RUN_TEST(test_addtime_rejects_empty_list);
    RUN_TEST(test_addtime_rejects_negative_base_time);

    // adddate tests
    RUN_TEST(test_adddate_simple_addition);
    RUN_TEST(test_adddate_day_overflow);
    RUN_TEST(test_adddate_month_overflow);
    RUN_TEST(test_adddate_year_rollover);
    RUN_TEST(test_adddate_leap_year);
    RUN_TEST(test_adddate_non_leap_year);
    RUN_TEST(test_adddate_negative_days);
    RUN_TEST(test_adddate_negative_months);
    RUN_TEST(test_adddate_zero_offset);
    RUN_TEST(test_adddate_rejects_non_list);
    RUN_TEST(test_adddate_rejects_empty_list);
    RUN_TEST(test_adddate_rejects_invalid_base_month);
    RUN_TEST(test_adddate_rejects_invalid_base_day);

    // difftime tests
    RUN_TEST(test_difftime_simple);
    RUN_TEST(test_difftime_same_time);
    RUN_TEST(test_difftime_negative_result);
    RUN_TEST(test_difftime_seconds_only);
    RUN_TEST(test_difftime_borrow_required);
    RUN_TEST(test_difftime_zero_times);
    RUN_TEST(test_difftime_rejects_non_list);
    RUN_TEST(test_difftime_rejects_empty_list);
    RUN_TEST(test_difftime_rejects_negative_first_time);
    RUN_TEST(test_difftime_rejects_negative_second_time);

    return UNITY_END();
}
