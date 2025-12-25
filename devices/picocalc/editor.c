//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  PicoCalc full-screen text editor implementation
//
//  Based on the Pico Logo reference manual editor specification:
//  - Header: "PICO LOGO EDITOR" (centered, reverse video)
//  - Footer: "ESC - ACCEPT    BRK - CANCEL" (centered, reverse video)
//  - 30 lines of editable text
//  - Cursor movement, insert/delete, block operations
//

#include "editor.h"
#include "keyboard.h"
#include "lcd.h"
#include "screen.h"
#include "devices/font.h"

#include <string.h>
#include <stdbool.h>

// Editor screen layout constants
#define EDITOR_HEADER_ROW     0      // Header at top of screen
#define EDITOR_FIRST_ROW      1      // First editable row
#define EDITOR_LAST_ROW       30     // Last editable row (30 lines)
#define EDITOR_FOOTER_ROW     31     // Footer at bottom
#define EDITOR_VISIBLE_ROWS   30     // Number of visible editable rows
#define EDITOR_MAX_COLS       40     // Maximum columns per line
#define EDITOR_SCROLL_MARGIN  5      // Columns from edge before horizontal scroll triggers
#define EDITOR_LEFT_ARROW     30     // Left arrow glyph (content scrolled left)
#define EDITOR_RIGHT_ARROW    31     // Right arrow glyph (content continues right)

// Copy buffer size (default 1024 for RP2040, 8192 for RP2350)
#ifndef LOGO_COPY_BUFFER_SIZE
#define LOGO_COPY_BUFFER_SIZE 1024
#endif

// Editor state
typedef struct {
    char *buffer;           // Pointer to the edit buffer
    size_t buffer_size;     // Maximum buffer size
    size_t content_length;  // Current content length
    
    // Cursor position (in buffer coordinates)
    size_t cursor_pos;      // Cursor position in buffer (0-based)
    
    // View position (for scrolling)
    int view_start_line;    // First visible line index (vertical scroll)
    int h_scroll_offset;    // Horizontal scroll offset for current line
    
    // Selection state
    bool selecting;         // Whether selection is active
    size_t select_anchor;   // Start of selection (buffer position)
    
    // Copy buffer
    char copy_buffer[LOGO_COPY_BUFFER_SIZE];
    size_t copy_length;
    
    // Graphics preview state
    bool in_graphics_preview;  // True when viewing graphics screen (F3)
} EditorState;

static EditorState editor;

// Forward declarations
static void editor_draw_header(void);
static void editor_draw_footer(void);
static void editor_draw_content(void);
static void editor_draw_line(int screen_row, int line_index);
static void editor_position_cursor(void);
static void editor_insert_char(char c);
static void editor_insert_tab(void);
static void editor_delete_char(void);
static void editor_backspace(void);
static void editor_delete_selection(void);
static void editor_copy_selection(void);
static void editor_paste(void);
static void editor_move_cursor_left(void);
static void editor_move_cursor_right(void);
static void editor_move_cursor_up(void);
static void editor_move_cursor_down(void);
static void editor_move_cursor_home(void);
static void editor_move_cursor_end(void);
static void editor_page_up(void);
static void editor_page_down(void);
static void editor_new_line(void);
static int editor_get_line_start(int line_index);
static int editor_get_line_end(int line_index);
static int editor_get_line_at_pos(size_t pos);
static int editor_get_col_at_pos(size_t pos);
static int editor_count_lines(void);
static void editor_ensure_cursor_visible(void);
static void editor_update_h_scroll(void);

//
// Draw a string in reverse video at the specified row
// Using lcd_putc with bit 7 set for reverse video
//
static void editor_draw_reverse_row(int row, const char *text)
{
    // Clear the row first (fill with spaces in reverse - bit 7 set)
    for (int col = 0; col < EDITOR_MAX_COLS; col++) {
        lcd_putc(col, row, ' ' | 0x80);
    }
    
    // Center the text
    int text_len = strlen(text);
    int start_col = (EDITOR_MAX_COLS - text_len) / 2;
    if (start_col < 0) start_col = 0;
    
    // Draw the text in reverse video (bit 7 set)
    for (int i = 0; i < text_len && (start_col + i) < EDITOR_MAX_COLS; i++) {
        lcd_putc(start_col + i, row, text[i] | 0x80);
    }
}

static void editor_draw_header(void)
{
    editor_draw_reverse_row(EDITOR_HEADER_ROW, "PICO LOGO EDITOR");
}

static void editor_draw_footer(void)
{
    editor_draw_reverse_row(EDITOR_FOOTER_ROW, "ESC - ACCEPT    BRK - CANCEL");
}

//
// Get the buffer position of the start of a line (0-indexed)
// Returns buffer position, or content_length if line doesn't exist
//
static int editor_get_line_start(int line_index)
{
    if (line_index <= 0) return 0;
    
    int line = 0;
    for (size_t i = 0; i < editor.content_length; i++) {
        if (editor.buffer[i] == '\n') {
            line++;
            if (line == line_index) {
                return (int)(i + 1);
            }
        }
    }
    
    // Line doesn't exist, return end of buffer
    return (int)editor.content_length;
}

//
// Get the buffer position of the end of a line (before newline or at end)
//
static int editor_get_line_end(int line_index)
{
    int start = editor_get_line_start(line_index);
    
    for (size_t i = start; i < editor.content_length; i++) {
        if (editor.buffer[i] == '\n') {
            return (int)i;
        }
    }
    
    return (int)editor.content_length;
}

//
// Get the line number at a buffer position
//
static int editor_get_line_at_pos(size_t pos)
{
    int line = 0;
    for (size_t i = 0; i < pos && i < editor.content_length; i++) {
        if (editor.buffer[i] == '\n') {
            line++;
        }
    }
    return line;
}

//
// Get the column at a buffer position
//
static int editor_get_col_at_pos(size_t pos)
{
    int line_start = editor_get_line_start(editor_get_line_at_pos(pos));
    return (int)(pos - line_start);
}

//
// Count total lines in buffer
//
static int editor_count_lines(void)
{
    int lines = 1;  // At least one line
    for (size_t i = 0; i < editor.content_length; i++) {
        if (editor.buffer[i] == '\n') {
            lines++;
        }
    }
    return lines;
}

//
// Update horizontal scroll offset based on cursor position
// Ensures cursor is visible within the line's horizontal view
//
static void editor_update_h_scroll(void)
{
    int cursor_col = editor_get_col_at_pos(editor.cursor_pos);
    
    // Calculate visible columns (account for scroll indicators)
    int visible_start = 0;
    int visible_cols = EDITOR_MAX_COLS;
    
    if (editor.h_scroll_offset > 0) {
        // Left arrow takes column 0
        visible_start = 1;
        visible_cols--;
    }
    
    // Check if we need to show right arrow (content extends past visible area)
    int cursor_line = editor_get_line_at_pos(editor.cursor_pos);
    int line_len = editor_get_line_end(cursor_line) - editor_get_line_start(cursor_line);
    
    if (line_len > editor.h_scroll_offset + EDITOR_MAX_COLS - (editor.h_scroll_offset > 0 ? 1 : 0)) {
        // Right arrow takes last column
        visible_cols--;
    }
    
    // Calculate screen column for cursor
    int screen_col = cursor_col - editor.h_scroll_offset + visible_start;
    
    // Scroll right if cursor is past visible area
    if (screen_col >= visible_start + visible_cols) {
        editor.h_scroll_offset = cursor_col - visible_cols + 1;
        if (editor.h_scroll_offset < 0) editor.h_scroll_offset = 0;
    }
    
    // Scroll left if cursor is before visible area
    if (screen_col < visible_start || cursor_col < editor.h_scroll_offset) {
        editor.h_scroll_offset = cursor_col > EDITOR_SCROLL_MARGIN ? cursor_col - EDITOR_SCROLL_MARGIN : 0;
    }
}

//
// Ensure cursor is visible in the view (vertical scrolling)
//
static void editor_ensure_cursor_visible(void)
{
    int cursor_line = editor_get_line_at_pos(editor.cursor_pos);
    
    // Scroll up if cursor is above view
    if (cursor_line < editor.view_start_line) {
        editor.view_start_line = cursor_line;
    }
    
    // Scroll down if cursor is below view
    if (cursor_line >= editor.view_start_line + EDITOR_VISIBLE_ROWS) {
        editor.view_start_line = cursor_line - EDITOR_VISIBLE_ROWS + 1;
    }
    
    // Also update horizontal scroll for the current line
    editor_update_h_scroll();
}

//
// Draw a single line of content at the specified screen row
// Handles horizontal scrolling with left/right arrow indicators
//
static void editor_draw_line(int screen_row, int line_index)
{
    int actual_row = EDITOR_FIRST_ROW + screen_row;
    
    // Clear the row
    lcd_erase_line(actual_row, 0, EDITOR_MAX_COLS - 1);
    
    int line_start = editor_get_line_start(line_index);
    int line_end = editor_get_line_end(line_index);
    int line_len = line_end - line_start;
    
    if (line_start >= (int)editor.content_length) {
        return;  // Line doesn't exist
    }
    
    // Check if this is the line with the cursor (uses h_scroll)
    int cursor_line = editor_get_line_at_pos(editor.cursor_pos);
    int h_offset = (line_index == cursor_line) ? editor.h_scroll_offset : 0;
    
    // Determine if we need scroll indicators
    bool show_left_arrow = (h_offset > 0);
    bool show_right_arrow = (line_len > h_offset + EDITOR_MAX_COLS - (show_left_arrow ? 1 : 0));
    
    // Recalculate right arrow after knowing left arrow status
    int visible_cols = EDITOR_MAX_COLS;
    int start_col = 0;
    
    if (show_left_arrow) {
        visible_cols--;
        start_col = 1;
        lcd_putc(0, actual_row, EDITOR_LEFT_ARROW);
    }
    
    if (show_right_arrow) {
        visible_cols--;
    }
    
    // Draw visible characters
    for (int col = 0; col < visible_cols; col++) {
        int buf_col = h_offset + col;
        if (buf_col >= line_len) break;
        
        size_t buf_pos = line_start + buf_col;
        char c = editor.buffer[buf_pos];
        
        // Check if this character is in selection
        bool in_selection = false;
        if (editor.selecting) {
            size_t sel_start = editor.select_anchor < editor.cursor_pos ? 
                               editor.select_anchor : editor.cursor_pos;
            size_t sel_end = editor.select_anchor > editor.cursor_pos ?
                             editor.select_anchor : editor.cursor_pos;
            in_selection = (buf_pos >= sel_start && buf_pos < sel_end);
        }
        
        // Use bit 7 for reverse video when selected
        if (in_selection) {
            lcd_putc(start_col + col, actual_row, c | 0x80);
        } else {
            lcd_putc(start_col + col, actual_row, c);
        }
    }
    
    // Draw right arrow if needed
    if (show_right_arrow) {
        lcd_putc(EDITOR_MAX_COLS - 1, actual_row, EDITOR_RIGHT_ARROW);
    }
}

//
// Draw all visible content
//
static void editor_draw_content(void)
{
    for (int row = 0; row < EDITOR_VISIBLE_ROWS; row++) {
        editor_draw_line(row, editor.view_start_line + row);
    }
}

//
// Position the hardware cursor at the current cursor position
//
static void editor_position_cursor(void)
{
    int cursor_line = editor_get_line_at_pos(editor.cursor_pos);
    int cursor_col = editor_get_col_at_pos(editor.cursor_pos);
    
    // Calculate screen row
    int screen_row = cursor_line - editor.view_start_line + EDITOR_FIRST_ROW;
    
    // Calculate screen column accounting for horizontal scroll
    int screen_col = cursor_col - editor.h_scroll_offset;
    
    // Adjust for left arrow indicator if scrolled
    if (editor.h_scroll_offset > 0) {
        screen_col++;  // Left arrow takes column 0
    }
    
    // Clamp to visible area
    if (screen_col < 0) screen_col = 0;
    if (screen_col >= EDITOR_MAX_COLS) screen_col = EDITOR_MAX_COLS - 1;
    if (screen_row < EDITOR_FIRST_ROW) screen_row = EDITOR_FIRST_ROW;
    if (screen_row > EDITOR_LAST_ROW) screen_row = EDITOR_LAST_ROW;
    
    screen_txt_set_cursor(screen_col, screen_row);
}

//
// Insert a character at cursor position
//
static void editor_insert_char(char c)
{
    // Check buffer space
    if (editor.content_length >= editor.buffer_size - 1) {
        return;  // Buffer full
    }
    
    // Delete selection first if active
    if (editor.selecting) {
        editor_delete_selection();
    }
    
    // Shift content to make room
    for (size_t i = editor.content_length; i > editor.cursor_pos; i--) {
        editor.buffer[i] = editor.buffer[i - 1];
    }
    
    // Insert character
    editor.buffer[editor.cursor_pos] = c;
    editor.cursor_pos++;
    editor.content_length++;
    editor.buffer[editor.content_length] = '\0';
}

//
// Insert spaces until the next tab stop (tab stops every 2 columns)
//
static void editor_insert_tab(void)
{
    int current_col = editor_get_col_at_pos(editor.cursor_pos);
    int spaces_to_insert = 2 - (current_col % 2);
    
    for (int i = 0; i < spaces_to_insert; i++) {
        editor_insert_char(' ');
    }
}

//
// Insert a newline at cursor position
//
static void editor_new_line(void)
{
    editor_insert_char('\n');
}

//
// Delete character at cursor position (DEL key)
//
static void editor_delete_char(void)
{
    if (editor.selecting) {
        editor_delete_selection();
        return;
    }
    
    if (editor.cursor_pos >= editor.content_length) {
        return;  // Nothing to delete
    }
    
    // Shift content
    for (size_t i = editor.cursor_pos; i < editor.content_length - 1; i++) {
        editor.buffer[i] = editor.buffer[i + 1];
    }
    
    editor.content_length--;
    editor.buffer[editor.content_length] = '\0';
}

//
// Delete character before cursor (BACKSPACE key)
//
static void editor_backspace(void)
{
    if (editor.selecting) {
        editor_delete_selection();
        return;
    }
    
    if (editor.cursor_pos == 0) {
        return;  // Nothing to delete
    }
    
    editor.cursor_pos--;
    editor_delete_char();
}

//
// Delete selected text
//
static void editor_delete_selection(void)
{
    if (!editor.selecting) return;
    
    size_t sel_start = editor.select_anchor < editor.cursor_pos ? 
                       editor.select_anchor : editor.cursor_pos;
    size_t sel_end = editor.select_anchor > editor.cursor_pos ?
                     editor.select_anchor : editor.cursor_pos;
    size_t sel_len = sel_end - sel_start;
    
    // Shift content
    for (size_t i = sel_start; i < editor.content_length - sel_len; i++) {
        editor.buffer[i] = editor.buffer[i + sel_len];
    }
    
    editor.content_length -= sel_len;
    editor.buffer[editor.content_length] = '\0';
    editor.cursor_pos = sel_start;
    editor.selecting = false;
}

//
// Copy selected text to copy buffer
//
static void editor_copy_selection(void)
{
    if (!editor.selecting) return;
    
    size_t sel_start = editor.select_anchor < editor.cursor_pos ? 
                       editor.select_anchor : editor.cursor_pos;
    size_t sel_end = editor.select_anchor > editor.cursor_pos ?
                     editor.select_anchor : editor.cursor_pos;
    size_t sel_len = sel_end - sel_start;
    
    if (sel_len > LOGO_COPY_BUFFER_SIZE - 1) {
        sel_len = LOGO_COPY_BUFFER_SIZE - 1;
    }
    
    memcpy(editor.copy_buffer, &editor.buffer[sel_start], sel_len);
    editor.copy_buffer[sel_len] = '\0';
    editor.copy_length = sel_len;
}

//
// Paste from copy buffer
//
static void editor_paste(void)
{
    if (editor.copy_length == 0) return;
    
    // Delete selection first if active
    if (editor.selecting) {
        editor_delete_selection();
    }
    
    // Check buffer space
    if (editor.content_length + editor.copy_length >= editor.buffer_size) {
        return;  // Not enough space
    }
    
    // Shift content to make room
    for (size_t i = editor.content_length + editor.copy_length; 
         i > editor.cursor_pos + editor.copy_length; i--) {
        editor.buffer[i - 1] = editor.buffer[i - editor.copy_length - 1];
    }
    
    // Insert copy buffer content
    memcpy(&editor.buffer[editor.cursor_pos], editor.copy_buffer, editor.copy_length);
    editor.cursor_pos += editor.copy_length;
    editor.content_length += editor.copy_length;
    editor.buffer[editor.content_length] = '\0';
}

//
// Cursor movement functions
//
static void editor_move_cursor_left(void)
{
    if (editor.cursor_pos > 0) {
        editor.cursor_pos--;
    }
}

static void editor_move_cursor_right(void)
{
    if (editor.cursor_pos < editor.content_length) {
        editor.cursor_pos++;
    }
}

static void editor_move_cursor_up(void)
{
    int current_line = editor_get_line_at_pos(editor.cursor_pos);
    int current_col = editor_get_col_at_pos(editor.cursor_pos);
    
    if (current_line > 0) {
        int prev_line_start = editor_get_line_start(current_line - 1);
        int prev_line_end = editor_get_line_end(current_line - 1);
        int prev_line_len = prev_line_end - prev_line_start;
        
        if (current_col > prev_line_len) {
            editor.cursor_pos = prev_line_end;
        } else {
            editor.cursor_pos = prev_line_start + current_col;
        }
        
        // Reset horizontal scroll when changing lines
        editor.h_scroll_offset = 0;
    }
}

static void editor_move_cursor_down(void)
{
    int current_line = editor_get_line_at_pos(editor.cursor_pos);
    int current_col = editor_get_col_at_pos(editor.cursor_pos);
    int total_lines = editor_count_lines();
    
    if (current_line < total_lines - 1) {
        int next_line_start = editor_get_line_start(current_line + 1);
        int next_line_end = editor_get_line_end(current_line + 1);
        int next_line_len = next_line_end - next_line_start;
        
        if (current_col > next_line_len) {
            editor.cursor_pos = next_line_end;
        } else {
            editor.cursor_pos = next_line_start + current_col;
        }
        
        // Reset horizontal scroll when changing lines
        editor.h_scroll_offset = 0;
    }
}

static void editor_move_cursor_home(void)
{
    int current_line = editor_get_line_at_pos(editor.cursor_pos);
    editor.cursor_pos = editor_get_line_start(current_line);
    editor.h_scroll_offset = 0;  // Reset horizontal scroll
}

static void editor_move_cursor_end(void)
{
    int current_line = editor_get_line_at_pos(editor.cursor_pos);
    editor.cursor_pos = editor_get_line_end(current_line);
    // h_scroll will be updated by editor_update_h_scroll()
}

static void editor_page_up(void)
{
    for (int i = 0; i < EDITOR_VISIBLE_ROWS; i++) {
        editor_move_cursor_up();
    }
}

static void editor_page_down(void)
{
    for (int i = 0; i < EDITOR_VISIBLE_ROWS; i++) {
        editor_move_cursor_down();
    }
}

//
// Copy current line to copy buffer (Ctrl+C without selection)
//
static void editor_copy_line(void)
{
    int current_line = editor_get_line_at_pos(editor.cursor_pos);
    int line_start = editor_get_line_start(current_line);
    int line_end = editor_get_line_end(current_line);
    
    // Include the newline if present
    if (line_end < (int)editor.content_length && editor.buffer[line_end] == '\n') {
        line_end++;
    }
    
    size_t line_len = line_end - line_start;
    if (line_len > LOGO_COPY_BUFFER_SIZE - 1) {
        line_len = LOGO_COPY_BUFFER_SIZE - 1;
    }
    
    memcpy(editor.copy_buffer, &editor.buffer[line_start], line_len);
    editor.copy_buffer[line_len] = '\0';
    editor.copy_length = line_len;
}

//
// Cut current line (Ctrl+X without selection)
//
static void editor_cut_line(void)
{
    int current_line = editor_get_line_at_pos(editor.cursor_pos);
    int line_start = editor_get_line_start(current_line);
    int line_end = editor_get_line_end(current_line);
    
    // Include the newline if present
    if (line_end < (int)editor.content_length && editor.buffer[line_end] == '\n') {
        line_end++;
    }
    
    size_t line_len = line_end - line_start;
    if (line_len > LOGO_COPY_BUFFER_SIZE - 1) {
        line_len = LOGO_COPY_BUFFER_SIZE - 1;
    }
    
    // Copy to buffer
    memcpy(editor.copy_buffer, &editor.buffer[line_start], line_len);
    editor.copy_buffer[line_len] = '\0';
    editor.copy_length = line_len;
    
    // Delete the line
    for (size_t i = line_start; i < editor.content_length - line_len; i++) {
        editor.buffer[i] = editor.buffer[i + line_len];
    }
    
    editor.content_length -= line_len;
    editor.buffer[editor.content_length] = '\0';
    editor.cursor_pos = line_start;
}

//
// Main editor function
//
LogoEditorResult picocalc_editor_edit(char *buffer, size_t buffer_size)
{
    // Save cursor position and screen mode to restore on exit
    uint8_t saved_cursor_col, saved_cursor_row;
    screen_txt_get_cursor(&saved_cursor_col, &saved_cursor_row);
    uint8_t saved_screen_mode = screen_get_mode();
    
    // Initialize editor state
    editor.buffer = buffer;
    editor.buffer_size = buffer_size;
    editor.content_length = strlen(buffer);
    editor.cursor_pos = editor.content_length;  // Start at end of content
    editor.view_start_line = 0;
    editor.h_scroll_offset = 0;
    editor.selecting = false;
    editor.select_anchor = 0;
    editor.copy_buffer[0] = '\0';
    editor.copy_length = 0;
    editor.in_graphics_preview = false;
    
    // Ensure cursor is on second line if we have "to name\n" template
    // (cursor_pos is already at end which is correct for template)
    
    // Switch to full-screen text mode for the editor
    // Use no_update to avoid redrawing txt_buffer - we'll draw editor content instead
    screen_set_mode_no_update(SCREEN_MODE_TXT);
    
    // Tell keyboard_poll to skip mode switching - editor handles it
    input_active = true;
    
    // Clear screen and draw initial content
    lcd_clear_screen();
    editor_draw_header();
    editor_draw_footer();
    editor_draw_content();
    editor_ensure_cursor_visible();
    
    // Position cursor BEFORE enabling it - this ensures screen_txt_enable_cursor
    // sees a valid cursor location (important when coming from splitscreen)
    editor_position_cursor();
    
    // Now enable and draw cursor - cursor position is already set
    screen_txt_enable_cursor(true);
    screen_txt_draw_cursor();  // Draw cursor immediately after enabling
    
    // Main editor loop
    while (true) {
        // Draw cursor before waiting for key (in case it was erased)
        screen_txt_draw_cursor();
        char key = keyboard_get_key();
        // Erase cursor before modifying screen
        screen_txt_erase_cursor();
        
        bool needs_redraw = false;
        bool needs_cursor_update = true;
        
        // Handle special keys
        switch (key) {
            case KEY_ESC:
                // Accept changes
                screen_txt_erase_cursor();
                screen_txt_enable_cursor(false);
                input_active = false;  // Re-enable keyboard mode switching
                screen_set_mode(saved_screen_mode);  // Restore screen mode
                screen_txt_set_cursor(saved_cursor_col, saved_cursor_row);
                return LOGO_EDITOR_ACCEPT;
                
            case KEY_BREAK:
                // Cancel changes - restore original buffer
                screen_txt_erase_cursor();
                screen_txt_enable_cursor(false);
                input_active = false;  // Re-enable keyboard mode switching
                screen_set_mode(saved_screen_mode);  // Restore screen mode
                screen_txt_set_cursor(saved_cursor_col, saved_cursor_row);
                return LOGO_EDITOR_CANCEL;
            
            case KEY_LEFT:
                editor_move_cursor_left();
                needs_redraw = true;  // May need to update h_scroll
                break;
                
            case KEY_RIGHT:
                editor_move_cursor_right();
                needs_redraw = true;  // May need to update h_scroll
                break;
                
            case KEY_UP:
                editor_move_cursor_up();
                needs_redraw = true;  // Line changed, reset h_scroll
                break;
                
            case KEY_DOWN:
                editor_move_cursor_down();
                needs_redraw = true;  // Line changed, reset h_scroll
                break;
                
            case KEY_HOME:
                editor_move_cursor_home();
                needs_redraw = true;  // h_scroll reset
                break;
                
            case KEY_END:
                editor_move_cursor_end();
                needs_redraw = true;  // May need to update h_scroll
                break;
                
            case KEY_PAGE_UP:
                editor_page_up();
                needs_redraw = true;
                break;
                
            case KEY_PAGE_DOWN:
                editor_page_down();
                needs_redraw = true;
                break;
                
            case KEY_BACKSPACE:
                editor_backspace();
                needs_redraw = true;
                break;
                
            case KEY_DEL:
                editor_delete_char();
                needs_redraw = true;
                break;
                
            case KEY_ENTER:
            case KEY_RETURN:
                editor_new_line();
                needs_redraw = true;
                break;
                
            case KEY_TAB:
                if (!editor.selecting) {
                    editor_insert_tab();
                    needs_redraw = true;
                }
                break;
                
            case 0x02:  // Ctrl+B - toggle block selection
                if (editor.selecting) {
                    editor.selecting = false;
                } else {
                    editor.selecting = true;
                    editor.select_anchor = editor.cursor_pos;
                }
                needs_redraw = true;
                break;
                
            case 0x03:  // Ctrl+C - copy
                if (editor.selecting) {
                    editor_copy_selection();
                    editor.selecting = false;
                } else {
                    editor_copy_line();
                }
                needs_redraw = true;
                break;
                
            case 0x16:  // Ctrl+V - paste
                editor_paste();
                needs_redraw = true;
                break;
                
            case 0x18:  // Ctrl+X - cut
                if (editor.selecting) {
                    editor_copy_selection();
                    editor_delete_selection();
                } else {
                    editor_cut_line();
                }
                needs_redraw = true;
                break;
                
            case KEY_F1:
                // Restore editor from graphics preview
                if (editor.in_graphics_preview) {
                    // Switch to text mode WITHOUT redrawing txt_buffer
                    // Then redraw the editor content directly to LCD
                    screen_set_mode_no_update(SCREEN_MODE_TXT);
                    lcd_clear_screen();
                    editor_draw_header();
                    editor_draw_footer();
                    editor_draw_content();
                    screen_txt_enable_cursor(true);
                    editor.in_graphics_preview = false;
                }
                // If not in preview, F1 does nothing (already showing editor)
                break;
            
            case KEY_F2:
                // Split screen doesn't make sense in editor - ignore
                break;
                
            case KEY_F3:
                // Preview graphics screen temporarily
                if (!editor.in_graphics_preview) {
                    screen_txt_enable_cursor(false);
                    // Use no_update to avoid unnecessary txt_buffer redraw
                    screen_set_mode_no_update(SCREEN_MODE_GFX);
                    screen_gfx_update();  // Just update graphics
                    editor.in_graphics_preview = true;
                }
                needs_cursor_update = false;
                break;
                
            default:
                // Printable characters (space through ~)
                if (key >= 0x20 && key <= 0x7E) {
                    if (!editor.selecting) {
                        editor_insert_char(key);
                        needs_redraw = true;
                    }
                }
                break;
        }
        
        // Update display
        if (needs_redraw) {
            editor_ensure_cursor_visible();
            editor_draw_content();
        }
        
        if (needs_cursor_update) {
            editor_position_cursor();
        }
    }
}

// Editor operations structure
static const LogoConsoleEditor picocalc_editor_ops = {
    .edit = picocalc_editor_edit
};

const LogoConsoleEditor *picocalc_editor_get_ops(void)
{
    return &picocalc_editor_ops;
}
