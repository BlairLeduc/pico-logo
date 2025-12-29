//
//  Pico  Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  PicoCalc input handling
//

#include <stdio.h>
#include <string.h>

#include "keyboard.h"

#include "screen.h"
#include "history.h"
#include "input.h"
#include "devices/stream.h"

static bool using_serial = false; // Flag to indicate if using serial input

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

int picocalc_read_line(char *buf, int size)
{
    char key;
    uint8_t start_row = 0, start_col = 0;
    uint8_t end_row = 0, end_col = 0;
    uint8_t index = 0;
    uint8_t length = 0;
    uint history_index = history_get_start_index();

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
                index--;
                length--;

                if (index == length)
                {
                    buf[index] = 0; // Null-terminate the string
                    putchar('\b');  // Move cursor back
                    screen_txt_get_cursor(&end_col, &end_row);
                }
                else
                {
                    uint8_t col, row;
                    putchar('\b'); // Move cursor back
                    screen_txt_get_cursor(&col, &row);
                    memmove(buf + index, buf + index + 1, length - index + 1);
                    screen_txt_puts(buf + index); // Redisplay the rest of the line
                    screen_txt_get_cursor(&end_col, &end_row);
                    screen_txt_putc(' '); // Clear the rest of the line
                    screen_txt_set_cursor(col, row);
                }
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
                uint8_t col, row;
                screen_txt_get_cursor(&col, &row);
                memmove(buf + index, buf + index + 1, length - index + 1);
                length--;
                screen_txt_puts(buf + index); // Redisplay the rest of the line
                screen_txt_get_cursor(&end_col, &end_row);
                screen_txt_putc(' '); // Clear the rest of the line
                screen_txt_set_cursor(col, row);
            }
            break;
        case KEY_ESC: // delete to beginning of line
            if (index > 0)
            {
                screen_txt_set_cursor(start_col, start_row);
                for (int i = index; i < length - 1; i++)
                {
                    screen_txt_putc(' '); // Clear the rest of the line
                }
                index = 0;
                length = 0;
                buf[0] = 0; // Reset buffer
                screen_txt_set_cursor(start_col, start_row);
                end_col = start_col;
                end_row = start_row;
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
                history_index = history_prev_index(history_index);
                history_get(buf, size - 1, history_index);

                index = strlen(buf);
                screen_txt_set_cursor(start_col, start_row);
                screen_txt_puts(buf);
                screen_txt_get_cursor(&end_col, &end_row);
                for (int i = index; i < length; i++)
                {
                    screen_txt_putc(' '); // Clear the rest of the line
                }
                length = index;
                // Recalculate start_row based on where we ended up after potential scrolling
                calc_start_row(start_col, end_col, end_row, length, &start_row);
                screen_txt_set_cursor(end_col, end_row);
            }
            break;
        case KEY_DOWN:
            if (!history_is_empty() && !history_is_end_index(history_index)) // history is not empty and not at the end
            {
                // Move to the next entry, wrapping if needed
                history_index = history_next_index(history_index);
                if (history_is_end_index(history_index))
                {
                    // At the newest entry (blank line)
                    buf[0] = 0;
                }
                else
                {
                    history_get(buf, size - 1, history_index);
                }
                index = strlen(buf);
                screen_txt_set_cursor(start_col, start_row);
                screen_txt_puts(buf);
                screen_txt_get_cursor(&end_col, &end_row);
                for (int i = index; i < length; i++)
                {
                    screen_txt_putc(' '); // Clear the rest of the line
                }
                length = index;
                // Recalculate start_row based on where we ended up after potential scrolling
                calc_start_row(start_col, end_col, end_row, length, &start_row);
                screen_txt_set_cursor(end_col, end_row);
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
                    if (index == length)
                    {
                        buf[index++] = key;
                        buf[index] = 0; // Null-terminate the string
                        length++;
                        if (screen_txt_putc(key))
                        {
                            start_row--; // Adjust start row if text scrolled
                        }
                        screen_txt_get_cursor(&end_col, &end_row);
                    }
                    else
                    {
                        uint8_t col, row;
                        memmove(buf + index + 1, buf + index, length - index + 1);
                        buf[index++] = key;
                        length++;
                        screen_txt_get_cursor(&col, &row);
                        if (screen_txt_puts(buf + index - 1))
                        {
                            start_row--; // Adjust start row if text scrolled
                        }
                        screen_txt_get_cursor(&end_col, &end_row);
                        // Calculate new cursor position accounting for wrap
                        calc_cursor_pos(start_col, start_row, index, &col, &row);
                        screen_txt_set_cursor(col, row);
                    }
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
