//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Tests for text screen primitives.
//

#include "test_scaffold.h"
#include "mock_device.h"

void setUp(void)
{
    test_scaffold_setUp_with_device();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

// ============================================================================
// cleartext / ct tests
// ============================================================================

void test_cleartext_clears_screen(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    // First set cursor somewhere
    run_string("setcursor [10 5]");
    
    // Now clear
    run_string("cleartext");
    
    TEST_ASSERT_TRUE(state->text.cleared);
    TEST_ASSERT_EQUAL(0, state->text.cursor_col);
    TEST_ASSERT_EQUAL(0, state->text.cursor_row);
}

void test_ct_is_alias_for_cleartext(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    run_string("setcursor [10 5]");
    run_string("ct");
    
    TEST_ASSERT_TRUE(state->text.cleared);
    TEST_ASSERT_EQUAL(0, state->text.cursor_col);
    TEST_ASSERT_EQUAL(0, state->text.cursor_row);
}

// ============================================================================
// cursor tests
// ============================================================================

void test_cursor_returns_position_list(void)
{
    Result r = eval_string("cursor");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    // Should be [0 0] initially
    Node list = r.value.as.node;
    TEST_ASSERT_FALSE(mem_is_nil(list));
    
    Node col = mem_car(list);
    TEST_ASSERT_TRUE(mem_is_word(col));
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(col));
    
    Node rest = mem_cdr(list);
    TEST_ASSERT_FALSE(mem_is_nil(rest));
    
    Node row = mem_car(rest);
    TEST_ASSERT_TRUE(mem_is_word(row));
    TEST_ASSERT_EQUAL_STRING("0", mem_word_ptr(row));
}

void test_cursor_reflects_setcursor(void)
{
    run_string("setcursor [15 8]");
    
    Result r = eval_string("cursor");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);
    
    Node list = r.value.as.node;
    Node col = mem_car(list);
    Node row = mem_car(mem_cdr(list));
    
    TEST_ASSERT_EQUAL_STRING("15", mem_word_ptr(col));
    TEST_ASSERT_EQUAL_STRING("8", mem_word_ptr(row));
}

// ============================================================================
// setcursor tests
// ============================================================================

void test_setcursor_sets_position(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    run_string("setcursor [20 12]");
    
    TEST_ASSERT_EQUAL(20, state->text.cursor_col);
    TEST_ASSERT_EQUAL(12, state->text.cursor_row);
}

void test_setcursor_with_zero(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    run_string("setcursor [0 0]");
    
    TEST_ASSERT_EQUAL(0, state->text.cursor_col);
    TEST_ASSERT_EQUAL(0, state->text.cursor_row);
}

void test_setcursor_at_edge(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    // Set cursor to edge of 40-column screen
    run_string("setcursor [39 31]");
    
    TEST_ASSERT_EQUAL(39, state->text.cursor_col);
    TEST_ASSERT_EQUAL(31, state->text.cursor_row);
}

void test_setcursor_requires_list(void)
{
    Result r = run_string("setcursor 10");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setcursor_requires_two_items(void)
{
    Result r = run_string("setcursor [10]");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_setcursor_rejects_negative(void)
{
    Result r = run_string("setcursor [-1 0]");
    
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

// ============================================================================
// fullscreen / fs tests
// ============================================================================

void test_fullscreen_sets_mode(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    run_string("fullscreen");
    
    TEST_ASSERT_EQUAL(MOCK_SCREEN_FULLSCREEN, state->screen_mode);
}

void test_fs_is_alias(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    run_string("fs");
    
    TEST_ASSERT_EQUAL(MOCK_SCREEN_FULLSCREEN, state->screen_mode);
}

// ============================================================================
// splitscreen / ss tests
// ============================================================================

void test_splitscreen_sets_mode(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    run_string("splitscreen");
    
    TEST_ASSERT_EQUAL(MOCK_SCREEN_SPLIT, state->screen_mode);
}

void test_ss_is_alias(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    run_string("ss");
    
    TEST_ASSERT_EQUAL(MOCK_SCREEN_SPLIT, state->screen_mode);
}

// ============================================================================
// textscreen / ts tests
// ============================================================================

void test_textscreen_sets_mode(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    // First switch to another mode
    run_string("fullscreen");
    
    // Now switch to text
    run_string("textscreen");
    
    TEST_ASSERT_EQUAL(MOCK_SCREEN_TEXT, state->screen_mode);
}

void test_ts_is_alias(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    run_string("fullscreen");
    run_string("ts");
    
    TEST_ASSERT_EQUAL(MOCK_SCREEN_TEXT, state->screen_mode);
}

// ============================================================================
// Screen mode cycling tests
// ============================================================================

void test_screen_mode_cycle(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    // Start in text mode (default)
    TEST_ASSERT_EQUAL(MOCK_SCREEN_TEXT, state->screen_mode);
    
    // Switch to fullscreen
    run_string("fullscreen");
    TEST_ASSERT_EQUAL(MOCK_SCREEN_FULLSCREEN, state->screen_mode);
    
    // Switch to splitscreen
    run_string("splitscreen");
    TEST_ASSERT_EQUAL(MOCK_SCREEN_SPLIT, state->screen_mode);
    
    // Switch to textscreen
    run_string("textscreen");
    TEST_ASSERT_EQUAL(MOCK_SCREEN_TEXT, state->screen_mode);
}

// ============================================================================
// Command recording tests
// ============================================================================

void test_cleartext_records_command(void)
{
    mock_device_clear_commands();
    
    run_string("cleartext");
    
    const MockCommand *cmd = mock_device_last_command();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(MOCK_CMD_CLEAR_TEXT, cmd->type);
}

void test_setcursor_records_command(void)
{
    mock_device_clear_commands();
    
    run_string("setcursor [5 10]");
    
    const MockCommand *cmd = mock_device_last_command();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(MOCK_CMD_SET_CURSOR, cmd->type);
    TEST_ASSERT_EQUAL(5, cmd->params.cursor.col);
    TEST_ASSERT_EQUAL(10, cmd->params.cursor.row);
}

void test_fullscreen_records_command(void)
{
    mock_device_clear_commands();
    
    run_string("fullscreen");
    
    const MockCommand *cmd = mock_device_last_command();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(MOCK_CMD_FULLSCREEN, cmd->type);
}

void test_splitscreen_records_command(void)
{
    mock_device_clear_commands();
    
    run_string("splitscreen");
    
    const MockCommand *cmd = mock_device_last_command();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(MOCK_CMD_SPLITSCREEN, cmd->type);
}

void test_textscreen_records_command(void)
{
    mock_device_clear_commands();
    
    run_string("textscreen");
    
    const MockCommand *cmd = mock_device_last_command();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(MOCK_CMD_TEXTSCREEN, cmd->type);
}

// ============================================================================
// Integration tests
// ============================================================================

void test_cursor_with_first(void)
{
    run_string("setcursor [25 10]");
    
    Result r = eval_string("first cursor");
    
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    // first of [25 10] should be the word "25"
    float num;
    TEST_ASSERT_TRUE(value_to_number(r.value, &num) || mem_is_word(r.value.as.node));
}

void test_setcursor_with_list_operation(void)
{
    const MockDeviceState *state = mock_device_get_state();
    
    // Use list to build the position
    run_string("setcursor list 30 15");
    
    TEST_ASSERT_EQUAL(30, state->text.cursor_col);
    TEST_ASSERT_EQUAL(15, state->text.cursor_row);
}

// ============================================================================
// Main
// ============================================================================

int main(void)
{
    UNITY_BEGIN();
    
    // cleartext tests
    RUN_TEST(test_cleartext_clears_screen);
    RUN_TEST(test_ct_is_alias_for_cleartext);
    
    // cursor tests
    RUN_TEST(test_cursor_returns_position_list);
    RUN_TEST(test_cursor_reflects_setcursor);
    
    // setcursor tests
    RUN_TEST(test_setcursor_sets_position);
    RUN_TEST(test_setcursor_with_zero);
    RUN_TEST(test_setcursor_at_edge);
    RUN_TEST(test_setcursor_requires_list);
    RUN_TEST(test_setcursor_requires_two_items);
    RUN_TEST(test_setcursor_rejects_negative);
    
    // fullscreen tests
    RUN_TEST(test_fullscreen_sets_mode);
    RUN_TEST(test_fs_is_alias);
    
    // splitscreen tests
    RUN_TEST(test_splitscreen_sets_mode);
    RUN_TEST(test_ss_is_alias);
    
    // textscreen tests
    RUN_TEST(test_textscreen_sets_mode);
    RUN_TEST(test_ts_is_alias);
    
    // Screen mode cycling
    RUN_TEST(test_screen_mode_cycle);
    
    // Command recording
    RUN_TEST(test_cleartext_records_command);
    RUN_TEST(test_setcursor_records_command);
    RUN_TEST(test_fullscreen_records_command);
    RUN_TEST(test_splitscreen_records_command);
    RUN_TEST(test_textscreen_records_command);
    
    // Integration tests
    RUN_TEST(test_cursor_with_first);
    RUN_TEST(test_setcursor_with_list_operation);
    
    return UNITY_END();
}
