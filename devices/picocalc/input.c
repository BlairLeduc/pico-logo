//
//  Pico  Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  PicoCalc input handling
//

#include <stdio.h>
#include <string.h>

#include "core/syntax_highlight.h"
#include "keyboard.h"

#include "screen.h"
#include "history.h"
#include "input.h"
#include "devices/stream.h"

static bool using_serial = false; // Flag to indicate if using serial input

enum
{
    INPUT_SYNTAX_DEFAULT = 3,
    INPUT_SYNTAX_COMMENT = 10,
    INPUT_SYNTAX_KEYWORD = 12,
    INPUT_SYNTAX_FUNCTION = 8,
    INPUT_SYNTAX_VARIABLE = 11,
    INPUT_SYNTAX_STRING = 6,
    INPUT_SYNTAX_NUMBER = 9,
    INPUT_SYNTAX_COMMAND = 7,
    INPUT_SYNTAX_BRACKET_1 = 13,
    INPUT_SYNTAX_BRACKET_2 = 14,
    INPUT_SYNTAX_BRACKET_3 = 15,
};

static const uint8_t category_to_input_colour[SYNTAX_CATEGORY_COUNT] = {
    [SYNTAX_DEFAULT] = INPUT_SYNTAX_DEFAULT,
    [SYNTAX_COMMENT] = INPUT_SYNTAX_COMMENT,
    [SYNTAX_KEYWORD] = INPUT_SYNTAX_KEYWORD,
    [SYNTAX_FUNCTION] = INPUT_SYNTAX_FUNCTION,
    [SYNTAX_VARIABLE] = INPUT_SYNTAX_VARIABLE,
    [SYNTAX_STRING] = INPUT_SYNTAX_STRING,
    [SYNTAX_NUMBER] = INPUT_SYNTAX_NUMBER,
    [SYNTAX_COMMAND] = INPUT_SYNTAX_COMMAND,
    [SYNTAX_BRACKET_1] = INPUT_SYNTAX_BRACKET_1,
    [SYNTAX_BRACKET_2] = INPUT_SYNTAX_BRACKET_2,
    [SYNTAX_BRACKET_3] = INPUT_SYNTAX_BRACKET_3,
};

static void picocalc_beep(void)
{
    //audio_play_sound_blocking(HIGH_BEEP, HIGH_BEEP, NOTE_EIGHTH);
}

// Helper to calculate cursor position from character index
// Takes the starting column/row and character index, returns the target column/row
static void calc_cursor_pos(uint8_t start_col, uint8_t start_row, uint8_t index,
                            uint8_t *out_col, uint8_t *out_row)
{
    uint16_t total_offset = start_col + index;
    *out_col = total_offset % SCREEN_COLUMNS;
    *out_row = start_row + (total_offset / SCREEN_COLUMNS);
}

// Helper to calculate start row from end position and text length
// After displaying text that may have scrolled, recalculate where the line starts
static void calc_start_row(uint8_t start_col, uint8_t end_col, uint8_t end_row,
                           uint8_t length, uint8_t *out_start_row)
{
    (void)end_col; // Unused but kept for API consistency
    
    // Total characters from start to end position
    uint16_t total_offset = start_col + length;
    // Number of rows the text spans
    uint8_t rows_used = total_offset / SCREEN_COLUMNS;
    
    // Bounds check to prevent underflow
    if (rows_used > end_row)
    {
        *out_start_row = 0;
    }
    else
    {
        *out_start_row = end_row - rows_used;
    }
}

static void redraw_input_line(char *buf, uint8_t length, uint8_t previous_length,
                              uint8_t index, uint8_t start_col, uint8_t *start_row,
                              uint8_t *end_col, uint8_t *end_row, int initial_depth)
{
    uint8_t categories[length > 0 ? length : 1];
    uint8_t cursor_col = start_col;
    uint8_t cursor_row = *start_row;

    if (length > 0)
    {
        syntax_highlight_line(buf, length, categories, initial_depth);
    }

    screen_txt_set_cursor(start_col, *start_row);

    for (uint8_t i = 0; i < length; i++)
    {
        screen_txt_set_foreground(category_to_input_colour[categories[i]]);
        if (screen_txt_putc((uint8_t)buf[i]) && *start_row > 0)
        {
            (*start_row)--;
        }
    }

    screen_txt_set_foreground(INPUT_SYNTAX_DEFAULT);
    for (uint8_t i = length; i < previous_length; i++)
    {
        if (screen_txt_putc(' ') && *start_row > 0)
        {
            (*start_row)--;
        }
    }

    screen_txt_get_cursor(end_col, end_row);
    calc_cursor_pos(start_col, *start_row, index, &cursor_col, &cursor_row);
    screen_txt_set_cursor(cursor_col, cursor_row);
}

int picocalc_read_line(char *buf, int size, int initial_depth)
{
    char key;
    uint8_t start_row = 0, start_col = 0;
    uint8_t end_row = 0, end_col = 0;
    uint8_t index = 0;
    uint8_t length = 0;
    uint history_index = history_get_start_index();
    
    // For prefix-based history search
    char search_prefix[HISTORY_LINE_LENGTH] = {0};
    size_t search_prefix_len = 0;
    bool in_history_search = false;

    screen_txt_get_cursor(&start_col, &start_row);
    end_row = start_row;
    end_col = start_col;
    screen_txt_enable_cursor(true);
    buf[0] = 0; // Null-terminate the string
    
    // Tell keyboard_poll that we're handling input - it should buffer F1/F2/F3
    input_active = true;

    while (true)
    {
        screen_txt_draw_cursor();
        key = getchar();
        screen_txt_erase_cursor();

        // Check for user interrupt (BRK key)
        if (key == KEY_BREAK)
        {
            screen_txt_enable_cursor(false);
            input_active = false;  // Re-enable keyboard mode switching during execution
            return LOGO_STREAM_INTERRUPTED;  // Signal user interrupt
        }

        switch (key)
        {
        case KEY_BACKSPACE:
            if (index > 0)
            {
                // Reset history search when editing
                in_history_search = false;
                history_index = history_get_start_index();

                uint8_t previous_length = length;
                index--;
                length--;
                memmove(buf + index, buf + index + 1, length - index + 1);
                redraw_input_line(buf, length, previous_length, index, start_col,
                                  &start_row, &end_col, &end_row, initial_depth);
            }
            break;
        case KEY_F1:
        case KEY_F2:
        case KEY_F3:
            screen_handle_mode_key(key);
            break;
        // case KEY_F5:
        //     screen_gfx_save("/Logo/screenshot.bmp");
        //     break;
        case KEY_DEL: // DEL key
            if (index < length)
            {
                // Reset history search when editing
                in_history_search = false;
                history_index = history_get_start_index();

                uint8_t previous_length = length;
                memmove(buf + index, buf + index + 1, length - index);
                length--;
                redraw_input_line(buf, length, previous_length, index, start_col,
                                  &start_row, &end_col, &end_row, initial_depth);
            }
            break;
        case KEY_ESC: // delete to beginning of line
            if (index > 0)
            {
                // Reset history search when editing
                in_history_search = false;
                history_index = history_get_start_index();

                uint8_t previous_length = length;
                index = 0;
                length = 0;
                buf[0] = 0; // Reset buffer
                redraw_input_line(buf, length, previous_length, index, start_col,
                                  &start_row, &end_col, &end_row, initial_depth);
            }
            break;
        case KEY_HOME:
            if (index > 0)
            {
                index = 0;
                screen_txt_set_cursor(start_col, start_row);
            }
            break;
        case KEY_END:
            if (index < length)
            {
                index = length;
                screen_txt_set_cursor(end_col, end_row);
            }
            break;
        case KEY_UP:
            if (!history_is_empty()) // history is not empty
            {
                // If starting a new search, save the current input as the prefix
                if (!in_history_search)
                {
                    strncpy(search_prefix, buf, sizeof(search_prefix) - 1);
                    search_prefix[sizeof(search_prefix) - 1] = '\0';
                    search_prefix_len = length;
                    in_history_search = true;
                }
                
                uint new_index = history_prev_matching(history_index, search_prefix, search_prefix_len);
                if (new_index != history_index)
                {
                    uint8_t previous_length = length;
                    history_index = new_index;
                    history_get(buf, size - 1, history_index);

                    index = strlen(buf);
                    length = index;
                    redraw_input_line(buf, length, previous_length, index, start_col,
                                      &start_row, &end_col, &end_row, initial_depth);
                }
            }
            break;
        case KEY_DOWN:
            if (!history_is_empty() && !history_is_end_index(history_index)) // history is not empty and not at the end
            {
                // If starting a new search, save the current input as the prefix
                if (!in_history_search)
                {
                    strncpy(search_prefix, buf, sizeof(search_prefix) - 1);
                    search_prefix[sizeof(search_prefix) - 1] = '\0';
                    search_prefix_len = length;
                    in_history_search = true;
                }
                // Move to the next matching entry
                uint new_index = history_next_matching(history_index, search_prefix, search_prefix_len);
                history_index = new_index;
                
                if (history_is_end_index(history_index))
                {
                    // At the newest entry, restore the original prefix
                    strncpy(buf, search_prefix, size - 1);
                    buf[size - 1] = '\0';
                    in_history_search = false;
                }
                else
                {
                    history_get(buf, size - 1, history_index);
                }
                {
                    uint8_t previous_length = length;
                    index = strlen(buf);
                    length = index;
                    redraw_input_line(buf, length, previous_length, index, start_col,
                                      &start_row, &end_col, &end_row, initial_depth);
                }
            }
            break;
        case KEY_LEFT:
            if (index > 0)
            {
                uint8_t col, row;
                index--;
                calc_cursor_pos(start_col, start_row, index, &col, &row);
                screen_txt_set_cursor(col, row);
            }
            break;
        case KEY_RIGHT:
            if (index < length)
            {
                uint8_t col, row;
                index++;
                calc_cursor_pos(start_col, start_row, index, &col, &row);
                screen_txt_set_cursor(col, row);
            }
            break;
        default:
            if (key == KEY_ENTER || key == KEY_RETURN)
            {
                screen_txt_erase_cursor();
                screen_txt_enable_cursor(false);
                // Move cursor to end of input before newline (handles wrapped lines)
                screen_txt_set_cursor(end_col, end_row);
                printf("\n"); // Print newline

                history_add((const char *)buf); // Add to history
                input_active = false;  // Re-enable keyboard mode switching during execution
                return length;
            }
            if (key >= 0x20 && key < 0x7F)
            {
                if (length < size - 1)
                {
                    // Reset history search when typing
                    in_history_search = false;
                    history_index = history_get_start_index();

                    uint8_t previous_length = length;
                    memmove(buf + index + 1, buf + index, length - index + 1);
                    buf[index++] = key;
                    length++;
                    redraw_input_line(buf, length, previous_length, index, start_col,
                                      &start_row, &end_col, &end_row, initial_depth);
                }
                else
                {
                    picocalc_beep(); // Beep if buffer is full or width exceeded
                }
            }
            break;
        }
    }

    return length;
}
