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

// Dirty tracking flags
#define DIRTY_NONE      0x00    // No redraw needed
#define DIRTY_LINE      0x01    // Single line needs redraw
#define DIRTY_FROM_LINE 0x02    // Redraw from line to bottom
#define DIRTY_ALL       0x04    // Full screen redraw
#define DIRTY_CURSOR    0x08    // Just cursor position changed

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
    
    // Dirty tracking for optimized redraws
    uint8_t dirty_flags;    // What needs to be redrawn
    int dirty_line;         // Line that needs redraw (for DIRTY_LINE)
    int dirty_from;         // First line to redraw (for DIRTY_FROM_LINE)
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
static bool editor_ensure_cursor_visible(void);
static void editor_update_h_scroll(void);
static void editor_mark_line_dirty(int line_index);
static void editor_mark_from_line_dirty(int line_index);
static void editor_mark_all_dirty(void);
static void editor_update_dirty(void);

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
    int cursor_line = editor_get_line_at_pos(editor.cursor_pos);
    int line_len = editor_get_line_end(cursor_line) - editor_get_line_start(cursor_line);
    
    // Calculate how many columns are available for content given current h_scroll
    // When h_scroll > 0, left arrow takes column 0
    // When content extends past view, right arrow takes last column
    
    // First, check if we need to scroll right (cursor past visible area)
    // Account for left arrow if we'll have h_scroll > 0 after adjustment
    // and right arrow if content extends past the view
    
    // Calculate visible content columns assuming we might scroll
    // If h_scroll will be > 0, we lose 1 column for left arrow
    // If line extends past view, we lose 1 column for right arrow
    
    // Simple approach: calculate the rightmost cursor position we can display
    // With both arrows shown, we have 38 content columns (positions 1-38)
    // With only right arrow, we have 39 content columns (positions 0-38)
    // With only left arrow, we have 39 content columns (positions 1-39)
    // With no arrows, we have 40 content columns (positions 0-39)
    
    // Check if cursor is past visible area (need to scroll right)
    bool needs_left_arrow = (editor.h_scroll_offset > 0);
    int visible_start = needs_left_arrow ? 1 : 0;
    bool needs_right_arrow = (line_len > editor.h_scroll_offset + EDITOR_MAX_COLS - visible_start);
    int visible_cols = EDITOR_MAX_COLS - visible_start - (needs_right_arrow ? 1 : 0);
    
    int screen_col = cursor_col - editor.h_scroll_offset + visible_start;
    
    if (screen_col >= visible_start + visible_cols) {
        // Need to scroll right
        // After scrolling, we'll definitely have left arrow (h_scroll > 0)
        // Check if we'll also need right arrow
        // With left arrow, content starts at column 1
        // Target: put cursor near right edge but visible
        
        // Calculate new offset: cursor should be at rightmost visible position
        // Visible positions: 1 to (39 or 38 depending on right arrow)
        // We want cursor_col - new_offset + 1 = rightmost_visible
        // So new_offset = cursor_col - rightmost_visible + 1
        
        // After scroll, will we need right arrow?
        // We need right arrow if: line_len > new_offset + 39 (since left arrow takes col 0)
        // Let's assume we might need it (38 visible) and recalculate if not
        
        int target_visible = EDITOR_MAX_COLS - 2;  // Assume both arrows (38 cols)
        int new_offset = cursor_col - target_visible + 1;
        if (new_offset < 0) new_offset = 0;
        
        // Now check if we actually need right arrow with this offset
        bool will_need_right = (new_offset > 0) && (line_len > new_offset + EDITOR_MAX_COLS - 1);
        if (!will_need_right && new_offset > 0) {
            // Only left arrow needed, we have 39 visible columns
            target_visible = EDITOR_MAX_COLS - 1;
            new_offset = cursor_col - target_visible + 1;
            if (new_offset < 0) new_offset = 0;
        }
        
        editor.h_scroll_offset = new_offset;
    }
    
    // Check if cursor is before visible area (need to scroll left)
    // Recalculate with potentially new h_scroll
    needs_left_arrow = (editor.h_scroll_offset > 0);
    visible_start = needs_left_arrow ? 1 : 0;
    screen_col = cursor_col - editor.h_scroll_offset + visible_start;
    
    if (screen_col < visible_start || cursor_col < editor.h_scroll_offset) {
        // Scroll left - position cursor with some margin from left edge
        editor.h_scroll_offset = cursor_col > EDITOR_SCROLL_MARGIN ? cursor_col - EDITOR_SCROLL_MARGIN : 0;
    }
}

//
// Ensure cursor is visible in the view (vertical scrolling)
// Returns true if vertical scrolling occurred
//
static bool editor_ensure_cursor_visible(void)
{
    int cursor_line = editor_get_line_at_pos(editor.cursor_pos);
    int old_view_start = editor.view_start_line;
    
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
    
    // Return true if vertical scroll changed (full redraw needed)
    return editor.view_start_line != old_view_start;
}

//
// Draw a single line of content at the specified screen row
// Handles horizontal scrolling with left/right arrow indicators
// Always draws full 40 characters to avoid flicker (no erase needed)
//
static void editor_draw_line(int screen_row, int line_index)
{
    int actual_row = EDITOR_FIRST_ROW + screen_row;
    
    int line_start = editor_get_line_start(line_index);
    int line_end = editor_get_line_end(line_index);
    int line_len = line_end - line_start;
    
    // Handle empty/non-existent lines - just draw spaces
    if (line_start >= (int)editor.content_length) {
        for (int col = 0; col < EDITOR_MAX_COLS; col++) {
            lcd_putc(col, actual_row, ' ');
        }
        return;
    }
    
    // Check if this is the line with the cursor (uses h_scroll)
    int cursor_line = editor_get_line_at_pos(editor.cursor_pos);
    int h_offset = (line_index == cursor_line) ? editor.h_scroll_offset : 0;
    
    // Determine if we need scroll indicators
    bool show_left_arrow = (h_offset > 0);
    bool show_right_arrow = (line_len > h_offset + EDITOR_MAX_COLS - (show_left_arrow ? 1 : 0));
    
    // Recalculate right arrow after knowing left arrow status
    int visible_cols = EDITOR_MAX_COLS;
    int screen_col = 0;
    
    // Draw left arrow if needed
    if (show_left_arrow) {
        lcd_putc(screen_col++, actual_row, EDITOR_LEFT_ARROW);
        visible_cols--;
    }
    
    // Reserve space for right arrow if needed
    if (show_right_arrow) {
        visible_cols--;
    }
    
    // Draw visible characters (or spaces if past end of line content)
    for (int col = 0; col < visible_cols; col++) {
        int buf_col = h_offset + col;
        char c = ' ';  // Default to space
        bool in_selection = false;
        
        if (buf_col < line_len) {
            size_t buf_pos = line_start + buf_col;
            c = editor.buffer[buf_pos];
            
            // Check if this character is in selection
            if (editor.selecting) {
                size_t sel_start = editor.select_anchor < editor.cursor_pos ? 
                                   editor.select_anchor : editor.cursor_pos;
                size_t sel_end = editor.select_anchor > editor.cursor_pos ?
                                 editor.select_anchor : editor.cursor_pos;
                in_selection = (buf_pos >= sel_start && buf_pos < sel_end);
            }
        }
        
        // Use bit 7 for reverse video when selected
        if (in_selection) {
            lcd_putc(screen_col++, actual_row, c | 0x80);
        } else {
            lcd_putc(screen_col++, actual_row, c);
        }
    }
    
    // Draw right arrow or final space
    if (show_right_arrow) {
        lcd_putc(screen_col++, actual_row, EDITOR_RIGHT_ARROW);
    }
    
    // Fill any remaining columns with spaces (shouldn't happen but be safe)
    while (screen_col < EDITOR_MAX_COLS) {
        lcd_putc(screen_col++, actual_row, ' ');
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
// Mark a single line as needing redraw
//
static void editor_mark_line_dirty(int line_index)
{
    int screen_row = line_index - editor.view_start_line;
    if (screen_row < 0 || screen_row >= EDITOR_VISIBLE_ROWS) {
        return;  // Line not visible, no need to mark dirty
    }
    
    if (editor.dirty_flags & DIRTY_ALL) {
        return;  // Already doing full redraw
    }
    
    if (editor.dirty_flags & DIRTY_FROM_LINE) {
        // Already redrawing from a line - extend if needed
        if (line_index < editor.dirty_from) {
            editor.dirty_from = line_index;
        }
        return;
    }
    
    if (editor.dirty_flags & DIRTY_LINE) {
        // Already have one dirty line - if it's different, upgrade to FROM_LINE
        if (editor.dirty_line != line_index) {
            int earlier = editor.dirty_line < line_index ? editor.dirty_line : line_index;
            editor.dirty_flags = DIRTY_FROM_LINE;
            editor.dirty_from = earlier;
        }
        return;
    }
    
    editor.dirty_flags |= DIRTY_LINE;
    editor.dirty_line = line_index;
}

//
// Mark from a line to the bottom as needing redraw
// Used for operations that may affect multiple lines (insert/delete newline)
//
static void editor_mark_from_line_dirty(int line_index)
{
    if (editor.dirty_flags & DIRTY_ALL) {
        return;  // Already doing full redraw
    }
    
    if (editor.dirty_flags & DIRTY_FROM_LINE) {
        // Extend existing range if needed
        if (line_index < editor.dirty_from) {
            editor.dirty_from = line_index;
        }
        return;
    }
    
    editor.dirty_flags = DIRTY_FROM_LINE;
    editor.dirty_from = line_index;
}

//
// Mark entire screen as needing redraw
//
static void editor_mark_all_dirty(void)
{
    editor.dirty_flags = DIRTY_ALL;
}

//
// Update screen based on dirty flags
//
static void editor_update_dirty(void)
{
    if (editor.dirty_flags & DIRTY_ALL) {
        // Full redraw
        editor_draw_content();
    } else if (editor.dirty_flags & DIRTY_FROM_LINE) {
        // Redraw from dirty_from to bottom
        int start_row = editor.dirty_from - editor.view_start_line;
        if (start_row < 0) start_row = 0;
        for (int row = start_row; row < EDITOR_VISIBLE_ROWS; row++) {
            editor_draw_line(row, editor.view_start_line + row);
        }
    } else if (editor.dirty_flags & DIRTY_LINE) {
        // Redraw single line
        int screen_row = editor.dirty_line - editor.view_start_line;
        if (screen_row >= 0 && screen_row < EDITOR_VISIBLE_ROWS) {
            editor_draw_line(screen_row, editor.dirty_line);
        }
    }
    // DIRTY_CURSOR doesn't need any content redraw, just cursor positioning
    
    // Clear dirty flags
    editor.dirty_flags = DIRTY_NONE;
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
// Insert a newline at cursor position with auto-indentation
//
static void editor_new_line(void)
{
    // Find the start of the current line
    int current_line = editor_get_line_at_pos(editor.cursor_pos);
    int line_start = editor_get_line_start(current_line);
    
    // Count leading spaces on the current line
    int indent_spaces = 0;
    for (int i = line_start; i < (int)editor.content_length && editor.buffer[i] == ' '; i++) {
        indent_spaces++;
    }
    
    // Insert the newline
    editor_insert_char('\n');
    
    // Insert the same number of leading spaces
    for (int i = 0; i < indent_spaces; i++) {
        editor_insert_char(' ');
    }
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
// If only whitespace before cursor on current line, delete to previous tab stop
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
    
    // Find the start of the current line
    int current_line = editor_get_line_at_pos(editor.cursor_pos);
    int line_start = editor_get_line_start(current_line);
    
    // Check if there's only whitespace between line start and cursor
    bool only_whitespace = true;
    for (int i = line_start; i < (int)editor.cursor_pos; i++) {
        if (editor.buffer[i] != ' ') {
            only_whitespace = false;
            break;
        }
    }
    
    if (only_whitespace && editor.cursor_pos > (size_t)line_start) {
        // Delete back to previous tab stop (tab stops every 2 columns)
        int current_col = (int)editor.cursor_pos - line_start;
        int prev_tab_stop = ((current_col - 1) / 2) * 2;
        int chars_to_delete = current_col - prev_tab_stop;
        
        // Delete at least 1 character
        if (chars_to_delete < 1) chars_to_delete = 1;
        
        for (int i = 0; i < chars_to_delete; i++) {
            editor.cursor_pos--;
            editor_delete_char();
        }
    } else {
        // Normal backspace - delete one character
        editor.cursor_pos--;
        editor_delete_char();
    }
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
    memmove(&editor.buffer[editor.cursor_pos + editor.copy_length],
            &editor.buffer[editor.cursor_pos],
            editor.content_length - editor.cursor_pos);
    
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
    memmove(&editor.buffer[line_start],
            &editor.buffer[line_start + line_len],
            editor.content_length - line_start - line_len + 1);  // +1 for null terminator
    
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
    editor.cursor_pos = 0;  // Start at beginning of content
    editor.view_start_line = 0;
    editor.h_scroll_offset = 0;
    editor.selecting = false;
    editor.select_anchor = 0;
    editor.copy_buffer[0] = '\0';
    editor.copy_length = 0;
    editor.in_graphics_preview = false;
    editor.dirty_flags = DIRTY_NONE;
    
    // Ensure cursor is on second line if we have "to name\n" template
    // (currently we start at beginning; template-specific positioning can be added here)
    
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
        
        // Check if screen saver was just dismissed - need full redraw
        if (screensaver_dismissed) {
            screensaver_dismissed = false;  // Clear the flag
            lcd_clear_screen();
            editor_draw_header();
            editor_draw_footer();
            editor_draw_content();
            editor_position_cursor();
            continue;  // Skip normal key processing for this keypress
        }
        
        // Track cursor line before operation for dirty tracking
        int cursor_line_before = editor_get_line_at_pos(editor.cursor_pos);
        int h_scroll_before = editor.h_scroll_offset;
        bool needs_cursor_update = true;
        
        // Reset dirty flags at start of each key press
        editor.dirty_flags = DIRTY_NONE;
        
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
                if (editor.selecting) {
                    // Selection changed - redraw affected lines
                    editor_mark_line_dirty(cursor_line_before);
                    editor_mark_line_dirty(editor_get_line_at_pos(editor.cursor_pos));
                } else {
                    editor.dirty_flags = DIRTY_CURSOR;
                }
                break;
                
            case KEY_RIGHT:
                editor_move_cursor_right();
                if (editor.selecting) {
                    editor_mark_line_dirty(cursor_line_before);
                    editor_mark_line_dirty(editor_get_line_at_pos(editor.cursor_pos));
                } else {
                    editor.dirty_flags = DIRTY_CURSOR;
                }
                break;
                
            case KEY_UP:
                editor_move_cursor_up();
                if (editor.selecting) {
                    // Selection spans lines - mark range
                    editor_mark_from_line_dirty(editor_get_line_at_pos(editor.cursor_pos));
                } else {
                    editor.dirty_flags = DIRTY_CURSOR;
                }
                break;
                
            case KEY_DOWN:
                editor_move_cursor_down();
                if (editor.selecting) {
                    editor_mark_from_line_dirty(cursor_line_before);
                } else {
                    editor.dirty_flags = DIRTY_CURSOR;
                }
                break;
                
            case KEY_HOME:
                editor_move_cursor_home();
                if (editor.selecting) {
                    editor_mark_line_dirty(cursor_line_before);
                } else {
                    editor.dirty_flags = DIRTY_CURSOR;
                }
                break;
                
            case KEY_END:
                editor_move_cursor_end();
                if (editor.selecting) {
                    editor_mark_line_dirty(cursor_line_before);
                } else {
                    editor.dirty_flags = DIRTY_CURSOR;
                }
                break;
                
            case KEY_PAGE_UP:
                editor_page_up();
                if (editor.selecting) {
                    // Selection spans many lines
                    editor_mark_all_dirty();
                } else {
                    // Page moves likely cause vertical scroll - check after ensure_visible
                    editor.dirty_flags = DIRTY_CURSOR;
                }
                break;
                
            case KEY_PAGE_DOWN:
                editor_page_down();
                if (editor.selecting) {
                    editor_mark_all_dirty();
                } else {
                    editor.dirty_flags = DIRTY_CURSOR;
                }
                break;
                
            case KEY_BACKSPACE:
                {
                    // Check if we're deleting a newline (cursor at start of line, will merge with prev)
                    int col_before = editor_get_col_at_pos(editor.cursor_pos);
                    editor_backspace();
                    if (col_before == 0 && cursor_line_before > 0) {
                        // Deleted a newline - need to redraw from previous line down
                        editor_mark_from_line_dirty(cursor_line_before - 1);
                    } else {
                        // Just deleted a char on the same line
                        editor_mark_line_dirty(editor_get_line_at_pos(editor.cursor_pos));
                    }
                }
                break;
                
            case KEY_DEL:
                {
                    // Check if we're deleting a newline (at end of line)
                    int line_end = editor_get_line_end(cursor_line_before);
                    bool deleting_newline = (editor.cursor_pos == (size_t)line_end && 
                                             editor.cursor_pos < editor.content_length);
                    editor_delete_char();
                    if (deleting_newline) {
                        // Deleted a newline - need to redraw from current line down
                        editor_mark_from_line_dirty(cursor_line_before);
                    } else {
                        // Just deleted a char on the same line
                        editor_mark_line_dirty(cursor_line_before);
                    }
                }
                break;
                
            case KEY_ENTER:
            case KEY_RETURN:
                editor_new_line();
                // Newline insertion affects current line and everything below
                editor_mark_from_line_dirty(cursor_line_before);
                break;
                
            case KEY_TAB:
                if (!editor.selecting) {
                    editor_insert_tab();
                    editor_mark_line_dirty(editor_get_line_at_pos(editor.cursor_pos));
                }
                break;
                
            case 0x02:  // Ctrl+B - toggle block selection
                if (editor.selecting) {
                    editor.selecting = false;
                } else {
                    editor.selecting = true;
                    editor.select_anchor = editor.cursor_pos;
                }
                // Selection visual needs redraw - mark the lines involved
                editor_mark_all_dirty();  // Selection can span multiple lines
                break;
                
            case 0x03:  // Ctrl+C - copy
            case 0x19:  // Ctrl+Y - yank (also copy, Y is for yank)
                if (editor.selecting) {
                    editor_copy_selection();
                    editor.selecting = false;
                    editor_mark_all_dirty();  // Clear selection highlighting
                } else {
                    editor_copy_line();
                    // No visual change for copy line
                }
                break;
                
            case 0x16:  // Ctrl+V - paste
            case 0x10:  // Ctrl+P - paste (also Paste, P is for paste)
                {
                    // Check if paste includes newlines
                    bool has_newline = false;
                    for (size_t i = 0; i < editor.copy_length; i++) {
                        if (editor.copy_buffer[i] == '\n') {
                            has_newline = true;
                            break;
                        }
                    }
                    editor_paste();
                    if (has_newline) {
                        editor_mark_from_line_dirty(cursor_line_before);
                    } else {
                        editor_mark_line_dirty(editor_get_line_at_pos(editor.cursor_pos));
                    }
                }
                break;
                
            case 0x18:  // Ctrl+X - cut
            case 0x14:  // Ctrl+T - take (also cut, T is for take)
                if (editor.selecting) {
                    editor_copy_selection();
                    editor_delete_selection();
                    // Cut selection may span lines
                    editor_mark_from_line_dirty(editor_get_line_at_pos(editor.cursor_pos));
                } else {
                    editor_cut_line();
                    // Cut line affects everything below
                    editor_mark_from_line_dirty(editor_get_line_at_pos(editor.cursor_pos));
                }
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
                        // Auto-close brackets and parentheses
                        if (key == '[') {
                            editor_insert_char(']');
                            editor.cursor_pos--;  // Move cursor between the pair
                        } else if (key == '(') {
                            editor_insert_char(')');
                            editor.cursor_pos--;  // Move cursor between the pair
                        }
                        editor_mark_line_dirty(editor_get_line_at_pos(editor.cursor_pos));
                    }
                }
                break;
        }
        
        // Update display based on what changed
        if (editor.dirty_flags != DIRTY_NONE) {
            // Check if vertical scroll occurred
            bool scrolled = editor_ensure_cursor_visible();
            
            if (scrolled) {
                // Vertical scroll requires full redraw
                editor_mark_all_dirty();
            } else if (editor.dirty_flags == DIRTY_CURSOR) {
                // Just cursor movement - only redraw if h_scroll changed
                int cursor_line_after = editor_get_line_at_pos(editor.cursor_pos);
                if (editor.h_scroll_offset != h_scroll_before) {
                    // H_scroll changed - redraw the new line
                    editor_mark_line_dirty(cursor_line_after);
                    // If we moved lines and old line had h_scroll, redraw it too
                    if (cursor_line_after != cursor_line_before && h_scroll_before > 0) {
                        editor_mark_line_dirty(cursor_line_before);
                    }
                } else if (cursor_line_after != cursor_line_before && h_scroll_before > 0) {
                    // Moved lines but h_scroll didn't change on new line
                    // Old line may need redraw if it had h_scroll
                    editor_mark_line_dirty(cursor_line_before);
                }
                // If h_scroll was 0 before and after, no line redraw needed
            }
            // For other dirty flags (DIRTY_LINE, DIRTY_FROM_LINE, DIRTY_ALL),
            // the flags were already set by the operation
            
            // Actually do the redraw
            editor_update_dirty();
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
