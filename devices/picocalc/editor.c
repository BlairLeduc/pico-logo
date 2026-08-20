//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
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
#include "editor_lines.h"
#include "editor_pattern.h"
#include "editor_search.h"
#include "editor_undo.h"
#include "editor_vi.h"
#include "keyboard.h"
#include "lcd.h"
#include "screen.h"
#include "devices/font.h"
#include "core/syntax_highlight.h"

#include <stdio.h>
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
#define EDITOR_SEARCH_MAX     32     // Longest search or replacement text
#define EDITOR_PROMPT_COLS    8      // Width of the footer's "Search: "/"Replace:" prompt
#define EDITOR_HIGHLIGHT_MAX  512    // Longest line the syntax highlighter is run on

// The editor's scrolling region: the header row is fixed at the top, the footer
// row at the bottom, and the thirty content rows between them scroll. The fixed
// areas are measured against the controller's frame memory, so the bottom one
// covers the rows below the display as well as the footer (see lcd.h)
#define EDITOR_SCROLL_TOP     (GLYPH_HEIGHT)
#define EDITOR_SCROLL_BOTTOM  (FRAME_HEIGHT - HEIGHT + GLYPH_HEIGHT)

// Tab width for indentation (2 spaces per tab stop)
#define TAB_WIDTH             2

// The vi layer computes its paging motions in lines and has no idea where the
// view is, so the two have to agree on how big a page is
_Static_assert(EDITOR_VI_PAGE_LINES == EDITOR_VISIBLE_ROWS,
               "vi paging must match the editor's visible rows");

// Syntax highlighting palette slots — use the text palette (slots 0-15)
#define PALETTE_SYNTAX_DEFAULT      3   // White
#define PALETTE_SYNTAX_COMMENT     10   // Comments (green)
#define PALETTE_SYNTAX_KEYWORD     12   // Keywords (purple)
#define PALETTE_SYNTAX_FUNCTION     8   // Procedures (yellow-green)
#define PALETTE_SYNTAX_VARIABLE    11   // Variables (cyan)
#define PALETTE_SYNTAX_STRING       6   // Strings (orange)
#define PALETTE_SYNTAX_NUMBER       9   // Numbers (green)
#define PALETTE_SYNTAX_COMMAND      7   // Commands (gold)
#define PALETTE_SYNTAX_BRACKET_1   13   // Bracket depth 1 (pink)
#define PALETTE_SYNTAX_BRACKET_2   14   // Bracket depth 2 (gold)
#define PALETTE_SYNTAX_BRACKET_3   15   // Bracket depth 3 (blue)
#define PALETTE_SYNTAX_BG           4   // Editor background

// Map SyntaxCategory enum values to palette slots
static const uint8_t category_to_palette[] = {
    [SYNTAX_DEFAULT]   = PALETTE_SYNTAX_DEFAULT,
    [SYNTAX_COMMENT]   = PALETTE_SYNTAX_COMMENT,
    [SYNTAX_KEYWORD]   = PALETTE_SYNTAX_KEYWORD,
    [SYNTAX_FUNCTION]  = PALETTE_SYNTAX_FUNCTION,
    [SYNTAX_VARIABLE]  = PALETTE_SYNTAX_VARIABLE,
    [SYNTAX_STRING]    = PALETTE_SYNTAX_STRING,
    [SYNTAX_NUMBER]    = PALETTE_SYNTAX_NUMBER,
    [SYNTAX_COMMAND]   = PALETTE_SYNTAX_COMMAND,
    [SYNTAX_BRACKET_1] = PALETTE_SYNTAX_BRACKET_1,
    [SYNTAX_BRACKET_2] = PALETTE_SYNTAX_BRACKET_2,
    [SYNTAX_BRACKET_3] = PALETTE_SYNTAX_BRACKET_3,
};

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

    EditorLineIndex lines;  // Memo for line <-> offset lookups

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
    
    // Incremental search state
    bool searching;                          // True while incremental search is active
    char search_text[EDITOR_SEARCH_MAX + 1]; // Text typed so far
    size_t search_len;                       // Length of search_text
    size_t search_origin;                    // Where the next search starts (last match, or
                                             // the cursor position when the search began)

    // Replace state (entered from a search with Ctrl+R)
    bool replacing;                           // True while the replacement text is being typed
    char replace_text[EDITOR_SEARCH_MAX + 1]; // Text every match is replaced with
    size_t replace_len;                       // Length of replace_text
    size_t replace_cursor;                    // Insert point within replace_text

    // Vi mode state (docs/vi-mode-design.md). Only the key layer differs: every
    // buffer, view and redraw field above is shared with the default mode.
    bool vi_mode;              // True when the vi key layer is in charge
    ViState vi;                // Its state machine
    EditorUndo undo;           // The change journal `u` and Ctrl+R walk
    const char *vi_msg;        // Footer text for one keystroke, NULL for the mode
    LogoEditorSave save;       // Write-back for `:w`, NULL when there is nowhere
    void *save_ctx;            // to write to yet (editing the workspace)

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
static void editor_draw_line(int screen_row, int line_index, int bracket_depth);
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
static void editor_move_cursor_word_left(void);
static void editor_move_cursor_word_right(void);
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
static int editor_ensure_cursor_visible(void);
static void editor_update_h_scroll(void);
static void editor_mark_line_dirty(int line_index);
static void editor_mark_from_line_dirty(int line_index);
static void editor_mark_all_dirty(void);
static void editor_update_dirty(void);
static void editor_decrease_indent(void);
static void editor_increase_indent(void);

//
// Tell the undo journal about a change before it is made: `del_len` bytes at
// pos (still in the buffer, which is why this comes first) give way to the
// `ins_len` bytes at `inserted`. Either side may be empty.
//
// Every mutation in this file calls it, for the same reason every one of them
// calls editor_lines_edit: a change the journal did not see leaves every record
// after it describing a buffer that never existed. It costs nothing outside vi
// mode, where there is no journal to record into.
//
static void editor_note_change(size_t pos, size_t del_len,
                               const char *inserted, size_t ins_len)
{
    editor_undo_record(&editor.undo, pos, &editor.buffer[pos], del_len,
                       inserted, ins_len);
}

//
// Draw a string in reverse video at the specified row, centred or left justified
// Using lcd_putc with bit 7 set for reverse video
//
static void editor_draw_reverse_row(int row, const char *text, bool centred)
{
    // Clear the row first (fill with spaces in reverse - bit 7 set)
    for (int col = 0; col < EDITOR_MAX_COLS; col++) {
        lcd_putc(col, row, ' ' | 0x80);
    }

    int text_len = strlen(text);
    int start_col = centred ? (EDITOR_MAX_COLS - text_len) / 2 : 0;
    if (start_col < 0) start_col = 0;

    // Draw the text in reverse video (bit 7 set)
    for (int i = 0; i < text_len && (start_col + i) < EDITOR_MAX_COLS; i++) {
        lcd_putc(start_col + i, row, text[i] | 0x80);
    }
}

static void editor_draw_header(void)
{
    editor_draw_reverse_row(EDITOR_HEADER_ROW, "PICO LOGO EDITOR", true);
}

//
// The footer shows the search or replacement text while incremental search is
// active, otherwise the exit prompt. Both prompts are EDITOR_PROMPT_COLS wide,
// so the text starts in the same column either way.
//
static void editor_draw_footer(void)
{
    if (editor.vi_mode) {
        // The mode indicator, or whatever the last keystroke had to say about
        // itself. Vi has no "ESC - ACCEPT" to offer: Esc is vi's, and leaving
        // is :w / ZZ or :q! / ZQ
        const char *text = editor.vi_msg ? editor.vi_msg
                                         : editor_vi_status(&editor.vi);
        editor_draw_reverse_row(EDITOR_FOOTER_ROW, text, false);

        // Normal mode also gets a ruler: the cursor's line number, right
        // justified. The other modes leave the room to the command line, the
        // insert indicator and the selection
        if (editor.vi.mode == VI_NORMAL) {
            char ruler[12];
            snprintf(ruler, sizeof(ruler), "%d",
                     editor_get_line_at_pos(editor.cursor_pos) + 1);
            int len = (int)strlen(ruler);
            int col = EDITOR_MAX_COLS - len;
            if (col > (int)strlen(text)) {  // Never write over the indicator
                for (int i = 0; i < len; i++) {
                    lcd_putc(col + i, EDITOR_FOOTER_ROW, ruler[i] | 0x80);
                }
            }
        }
    } else if (editor.searching) {
        char footer[EDITOR_PROMPT_COLS + EDITOR_SEARCH_MAX + 1];
        strcpy(footer, editor.replacing ? "Replace:" : "Search: ");
        strcat(footer, editor.replacing ? editor.replace_text : editor.search_text);
        editor_draw_reverse_row(EDITOR_FOOTER_ROW, footer, false);
    } else {
        editor_draw_reverse_row(EDITOR_FOOTER_ROW, "ESC - ACCEPT    BRK - CANCEL", true);
    }
}

//
// Get the buffer position of the start of a line (0-indexed)
// Returns buffer position, or content_length if line doesn't exist
//
static int editor_get_line_start(int line_index)
{
    return (int)editor_lines_start(&editor.lines, editor.buffer,
                                   editor.content_length, line_index);
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
    return editor_lines_at_pos(&editor.lines, editor.buffer,
                               editor.content_length, pos);
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
// Returns the number of lines the view moved: positive down, negative up,
// zero if it did not move
//
static int editor_ensure_cursor_visible(void)
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
    
    return editor.view_start_line - old_view_start;
}

//
// Compute the bracket nesting depth at the start of a given line.
// Scans backward to the nearest TO line (which resets depth to 0),
// then scans forward line-by-line using syntax_highlight_line in
// depth-only mode (NULL categories).
//
static int editor_compute_depth_at_line(int target_line)
{
    // Find the nearest preceding TO line (or line 0)
    int scan_from = 0;
    for (int ln = target_line - 1; ln >= 0; ln--) {
        int ls = editor_get_line_start(ln);
        if (ls >= (int)editor.content_length) continue;
        
        // Check if this line starts with optional whitespace then "to"
        int p = ls;
        while (p < (int)editor.content_length && (editor.buffer[p] == ' ' || editor.buffer[p] == '\t'))
            p++;
        int le = editor_get_line_end(ln);
        int remaining = le - p;
        if (remaining >= 2 &&
            (editor.buffer[p] == 't' || editor.buffer[p] == 'T') &&
            (editor.buffer[p + 1] == 'o' || editor.buffer[p + 1] == 'O') &&
            (remaining == 2 || editor.buffer[p + 2] == ' ' || editor.buffer[p + 2] == '\t')) {
            scan_from = ln;
            break;
        }
    }
    
    // Scan forward from scan_from, accumulating depth
    int depth = 0;
    for (int ln = scan_from; ln < target_line; ln++) {
        int ls = editor_get_line_start(ln);
        int le = editor_get_line_end(ln);
        int len = le - ls;
        if (ls < (int)editor.content_length && len > 0) {
            depth = syntax_highlight_line(editor.buffer + ls, len, NULL, depth);
        }
    }
    return depth;
}

//
// Draw a single line of content at the specified screen row
// Handles horizontal scrolling with left/right arrow indicators
// Always draws full 40 characters to avoid flicker (no erase needed)
// bracket_depth: nesting depth at the start of this line (for coloring)
//
static void editor_draw_line(int screen_row, int line_index, int bracket_depth)
{
    int actual_row = EDITOR_FIRST_ROW + screen_row;
    
    int line_start = editor_get_line_start(line_index);
    int line_end = editor_get_line_end(line_index);
    int line_len = line_end - line_start;
    
    // Handle empty/non-existent lines - just draw spaces
    if (line_start >= (int)editor.content_length) {
        lcd_set_foreground(PALETTE_SYNTAX_DEFAULT);
        for (int col = 0; col < EDITOR_MAX_COLS; col++) {
            lcd_putc(col, actual_row, ' ');
        }
        return;
    }
    
    // Syntax-highlight this line
    // Use a stack buffer (lines rarely exceed a few hundred chars)
    uint8_t categories_buf[EDITOR_HIGHLIGHT_MAX];
    uint8_t *categories = categories_buf;
    if (line_len > (int)sizeof(categories_buf)) {
        // Extremely long line — skip highlighting, use default
        categories = NULL;
    } else if (line_len > 0) {
        syntax_highlight_line(editor.buffer + line_start, line_len,
                              categories, bracket_depth);
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
        lcd_set_foreground(PALETTE_SYNTAX_DEFAULT);
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
        uint8_t palette_slot = PALETTE_SYNTAX_DEFAULT;
        
        if (buf_col < line_len) {
            size_t buf_pos = line_start + buf_col;
            c = editor.buffer[buf_pos];
            
            // Look up syntax color for this character
            if (categories && buf_col < line_len) {
                palette_slot = category_to_palette[categories[buf_col]];
            }
            
            // Check if this character is in selection
            if (editor.selecting) {
                size_t sel_start = editor.select_anchor < editor.cursor_pos ? 
                                   editor.select_anchor : editor.cursor_pos;
                size_t sel_end = editor.select_anchor > editor.cursor_pos ?
                                 editor.select_anchor : editor.cursor_pos;
                in_selection = (buf_pos >= sel_start && buf_pos < sel_end);
            }
        }
        
        // Use bit 7 for reverse video when selected (default color for uniform selection)
        if (in_selection) {
            lcd_set_foreground(PALETTE_SYNTAX_DEFAULT);
            lcd_putc(screen_col++, actual_row, c | 0x80);
        } else {
            lcd_set_foreground(palette_slot);
            lcd_putc(screen_col++, actual_row, c);
        }
    }
    
    // Draw right arrow or final space
    if (show_right_arrow) {
        lcd_set_foreground(PALETTE_SYNTAX_DEFAULT);
        lcd_putc(screen_col++, actual_row, EDITOR_RIGHT_ARROW);
    }
    
    // Fill any remaining columns with spaces (shouldn't happen but be safe)
    lcd_set_foreground(PALETTE_SYNTAX_DEFAULT);
    while (screen_col < EDITOR_MAX_COLS) {
        lcd_putc(screen_col++, actual_row, ' ');
    }
}

//
// Draw all visible content
// Tracks bracket nesting depth across lines for syntax coloring
//
static void editor_draw_content(void)
{
    int depth = editor_compute_depth_at_line(editor.view_start_line);
    for (int row = 0; row < EDITOR_VISIBLE_ROWS; row++) {
        int line_index = editor.view_start_line + row;
        int line_start = editor_get_line_start(line_index);
        int line_end = editor_get_line_end(line_index);
        int line_len = line_end - line_start;
        
        editor_draw_line(row, line_index, depth);
        
        // Advance depth for the next line
        if (line_start < (int)editor.content_length && line_len > 0) {
            depth = syntax_highlight_line(editor.buffer + line_start,
                                          line_len, NULL, depth);
        }
    }
}

//
// Move the view one line using the LCD's hardware scroll, then draw the line
// that came into view. The panel shifts its own start line, so this costs one
// row draw instead of the thirty a full redraw of the content area costs.
//
// The header and footer are outside the scrolling area, so neither is touched.
//
static void editor_scroll_one_line(int delta)
{
    int line_index;

    if (delta > 0) {
        lcd_scroll_up(PALETTE_SYNTAX_BG);
        line_index = editor.view_start_line + EDITOR_VISIBLE_ROWS - 1;
    } else {
        lcd_scroll_down(PALETTE_SYNTAX_BG);
        line_index = editor.view_start_line;
    }

    editor_draw_line(line_index - editor.view_start_line, line_index,
                     editor_compute_depth_at_line(line_index));
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
        // Redraw from dirty_from to bottom, tracking bracket depth
        int start_row = editor.dirty_from - editor.view_start_line;
        if (start_row < 0) start_row = 0;
        int first_line = editor.view_start_line + start_row;
        int depth = editor_compute_depth_at_line(first_line);
        for (int row = start_row; row < EDITOR_VISIBLE_ROWS; row++) {
            int line_index = editor.view_start_line + row;
            int ls = editor_get_line_start(line_index);
            int le = editor_get_line_end(line_index);
            int len = le - ls;
            editor_draw_line(row, line_index, depth);
            if (ls < (int)editor.content_length && len > 0)
                depth = syntax_highlight_line(editor.buffer + ls, len, NULL, depth);
        }
    } else if (editor.dirty_flags & DIRTY_LINE) {
        // Redraw single line
        int screen_row = editor.dirty_line - editor.view_start_line;
        if (screen_row >= 0 && screen_row < EDITOR_VISIBLE_ROWS) {
            int depth = editor_compute_depth_at_line(editor.dirty_line);
            editor_draw_line(screen_row, editor.dirty_line, depth);
        }
    }
    // DIRTY_CURSOR doesn't need any content redraw, just cursor positioning
    
    // Clear dirty flags
    editor.dirty_flags = DIRTY_NONE;
}

//
// The palette slot the character at a buffer position is drawn in
//
static uint8_t editor_palette_at(size_t pos)
{
    int line_index = editor_get_line_at_pos(pos);
    int line_start = editor_get_line_start(line_index);
    int line_len = editor_get_line_end(line_index) - line_start;
    int col = (int)pos - line_start;

    // Past the end of the line, or a line too long to highlight
    if (col >= line_len || line_len > EDITOR_HIGHLIGHT_MAX) {
        return PALETTE_SYNTAX_DEFAULT;
    }

    uint8_t categories[EDITOR_HIGHLIGHT_MAX];
    syntax_highlight_line(editor.buffer + line_start, line_len, categories,
                          editor_compute_depth_at_line(line_index));
    return category_to_palette[categories[col]];
}

//
// Position the hardware cursor at the current cursor position
//
static void editor_position_cursor(void)
{
    if (editor.replacing) {
        // The replacement is typed in the footer, so the cursor goes there. A
        // full field puts the last insert point one column past the row, where
        // the underline sits under the last character instead.
        int screen_col = EDITOR_PROMPT_COLS + (int)editor.replace_cursor;
        if (screen_col >= EDITOR_MAX_COLS) screen_col = EDITOR_MAX_COLS - 1;
        screen_txt_set_cursor(screen_col, EDITOR_FOOTER_ROW);

        // The footer is reverse video, so the underline takes the editor's
        // background colour and is erased with the white the footer is drawn on
        lcd_set_cursor_char(TXT_PACK(PALETTE_SYNTAX_BG, TXT_WHITE, ' '));
        return;
    }

    if (editor.vi_mode && editor.vi.mode == VI_CMDLINE) {
        // The command line is typed in the footer, so the cursor goes there.
        // editor_draw_footer draws the command from column 0, leading ':', '/'
        // or '?' included, and the line is append-only: the insert point is one
        // column past what has been typed.
        int screen_col = (int)editor.vi.cmdline_len;
        if (screen_col >= EDITOR_MAX_COLS) screen_col = EDITOR_MAX_COLS - 1;
        screen_txt_set_cursor(screen_col, EDITOR_FOOTER_ROW);

        // Reverse video footer, as the replacement prompt above
        lcd_set_cursor_char(TXT_PACK(PALETTE_SYNTAX_BG, TXT_WHITE, ' '));
        return;
    }

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

    // Override the cursor character synced from txt_buffer: the editor draws
    // directly to the LCD, so txt_buffer holds the stale text screen
    // underneath, not the editor content the block cursor must render.
    // The colour matters as much as the character: the blink's hidden phase
    // repaints the cell from this entry, so a fixed foreground would flash
    // whatever the block cursor sits on in white. Only the block cursor
    // repaints the character — the underline is drawn in this colour instead
    // of over the character, and stays white.
    uint8_t cursor_char = ' ';
    uint8_t cursor_palette = PALETTE_SYNTAX_DEFAULT;
    if (editor.cursor_pos < editor.content_length) {
        cursor_char = (uint8_t)editor.buffer[editor.cursor_pos];
        if (cursor_char == '\n') {
            cursor_char = ' ';  // Show space for newline
        } else if (lcd_get_cursor_style() == LCD_CURSOR_BLOCK) {
            cursor_palette = editor_palette_at(editor.cursor_pos);
        }
    }
    lcd_set_cursor_char(TXT_PACK(cursor_palette, PALETTE_SYNTAX_BG, cursor_char));
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
    editor_note_change(editor.cursor_pos, 0, &c, 1);
    editor_lines_edit(&editor.lines, editor.cursor_pos);
    memmove(&editor.buffer[editor.cursor_pos + 1], &editor.buffer[editor.cursor_pos],
            editor.content_length - editor.cursor_pos);

    // Insert character
    editor.buffer[editor.cursor_pos] = c;
    editor.cursor_pos++;
    editor.content_length++;
    editor.buffer[editor.content_length] = '\0';
}

//
// Insert spaces until the next tab stop (tab stops every TAB_WIDTH columns)
//
static void editor_insert_tab(void)
{
    int current_col = editor_get_col_at_pos(editor.cursor_pos);
    int spaces_to_insert = TAB_WIDTH - (current_col % TAB_WIDTH);
    
    for (int i = 0; i < spaces_to_insert; i++) {
        editor_insert_char(' ');
    }
}

//
// Insert a newline at cursor position with auto-indentation
// Also adds extra indentation for unmatched open brackets '[' on current line
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
    
    // Count unmatched open brackets '[' on current line (from line start to cursor)
    int unmatched_brackets = 0;
    for (int i = line_start; i < (int)editor.cursor_pos; i++) {
        if (editor.buffer[i] == '[') {
            unmatched_brackets++;
        } else if (editor.buffer[i] == ']') {
            if (unmatched_brackets > 0) {
                unmatched_brackets--;
            }
        }
    }
    
    // Insert the newline
    editor_insert_char('\n');
    
    // Strip leading whitespace from content that is now on the new line
    // (the content that was after the cursor before the newline was inserted)
    while (editor.cursor_pos < editor.content_length &&
           editor.buffer[editor.cursor_pos] == ' ') {
        // Delete the space at cursor position
        editor_note_change(editor.cursor_pos, 1, NULL, 0);
        editor_lines_edit(&editor.lines, editor.cursor_pos);
        memmove(&editor.buffer[editor.cursor_pos], &editor.buffer[editor.cursor_pos + 1],
                editor.content_length - editor.cursor_pos - 1);
        editor.content_length--;
        editor.buffer[editor.content_length] = '\0';
    }
    
    // Insert the same number of leading spaces plus extra indentation for brackets
    // Each unmatched bracket adds one tab (TAB_WIDTH spaces)
    int total_indent = indent_spaces + (unmatched_brackets * TAB_WIDTH);
    for (int i = 0; i < total_indent; i++) {
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
    editor_note_change(editor.cursor_pos, 1, NULL, 0);
    editor_lines_edit(&editor.lines, editor.cursor_pos);
    memmove(&editor.buffer[editor.cursor_pos], &editor.buffer[editor.cursor_pos + 1],
            editor.content_length - editor.cursor_pos - 1);

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
        // Delete back to previous tab stop (tab stops every TAB_WIDTH columns)
        int current_col = (int)editor.cursor_pos - line_start;
        int prev_tab_stop = ((current_col - 1) / TAB_WIDTH) * TAB_WIDTH;
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
    editor_note_change(sel_start, sel_len, NULL, 0);
    editor_lines_edit(&editor.lines, sel_start);
    memmove(&editor.buffer[sel_start], &editor.buffer[sel_start + sel_len],
            editor.content_length - sel_start - sel_len);

    editor.content_length -= sel_len;
    editor.buffer[editor.content_length] = '\0';
    editor.cursor_pos = sel_start;
    editor.selecting = false;
    lcd_set_cursor_style(LCD_CURSOR_UNDERLINE);
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
    editor_note_change(editor.cursor_pos, 0, editor.copy_buffer, editor.copy_length);
    editor_lines_edit(&editor.lines, editor.cursor_pos);
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

// Ctrl + Left/Right: move by a word. The scan itself lives in editor_lines.c,
// where it can be tested on the host.
static void editor_move_cursor_word_left(void)
{
    editor.cursor_pos = editor_word_left(editor.buffer, editor.cursor_pos);
}

static void editor_move_cursor_word_right(void)
{
    editor.cursor_pos = editor_word_right(editor.buffer, editor.content_length,
                                          editor.cursor_pos);
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
    int line_start = editor_get_line_start(current_line);
    int line_end = editor_get_line_end(current_line);
    
    // Find first non-whitespace character on this line
    int first_non_ws = line_start;
    while (first_non_ws < line_end && 
           (editor.buffer[first_non_ws] == ' ' || editor.buffer[first_non_ws] == '\t')) {
        first_non_ws++;
    }
    
    // Toggle between first non-whitespace and line start:
    // - If cursor is at first non-whitespace (or line is all whitespace), go to line start
    // - Otherwise, go to first non-whitespace
    if (editor.cursor_pos == first_non_ws || first_non_ws == line_end) {
        editor.cursor_pos = line_start;
    } else {
        editor.cursor_pos = first_non_ws;
    }
    
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
    int current_line = editor_get_line_at_pos(editor.cursor_pos);
    int current_col = editor_get_col_at_pos(editor.cursor_pos);
    
    // Move cursor and viewport up by one page, preserving screen position
    int new_line = current_line - EDITOR_VISIBLE_ROWS;
    if (new_line < 0) {
        new_line = 0;
    }
    
    int new_view_start = editor.view_start_line - EDITOR_VISIBLE_ROWS;
    if (new_view_start < 0) {
        new_view_start = 0;
    }
    editor.view_start_line = new_view_start;
    
    // Move cursor to the same column on the new line
    int line_start = editor_get_line_start(new_line);
    int line_end = editor_get_line_end(new_line);
    int line_len = line_end - line_start;
    
    if (current_col > line_len) {
        editor.cursor_pos = line_end;
    } else {
        editor.cursor_pos = line_start + current_col;
    }
    
    editor.h_scroll_offset = 0;
}

static void editor_page_down(void)
{
    int current_line = editor_get_line_at_pos(editor.cursor_pos);
    int current_col = editor_get_col_at_pos(editor.cursor_pos);
    int total_lines = editor_count_lines();
    
    // Move cursor and viewport down by one page, preserving screen position
    int new_line = current_line + EDITOR_VISIBLE_ROWS;
    if (new_line >= total_lines) {
        new_line = total_lines - 1;
    }
    
    int new_view_start = editor.view_start_line + EDITOR_VISIBLE_ROWS;
    int max_view_start = total_lines - EDITOR_VISIBLE_ROWS;
    if (max_view_start < 0) {
        max_view_start = 0;
    }
    if (new_view_start > max_view_start) {
        new_view_start = max_view_start;
    }
    editor.view_start_line = new_view_start;
    
    // Move cursor to the same column on the new line
    int line_start = editor_get_line_start(new_line);
    int line_end = editor_get_line_end(new_line);
    int line_len = line_end - line_start;
    
    if (current_col > line_len) {
        editor.cursor_pos = line_end;
    } else {
        editor.cursor_pos = line_start + current_col;
    }
    
    editor.h_scroll_offset = 0;
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
// Decrease indent of selected block by one tab stop (Ctrl+,)
// Removes up to TAB_WIDTH spaces from the beginning of each line in selection
//
static void editor_decrease_indent(void)
{
    if (!editor.selecting) return;
    
    // Get selection bounds
    size_t sel_start = editor.select_anchor < editor.cursor_pos ? 
                       editor.select_anchor : editor.cursor_pos;
    size_t sel_end = editor.select_anchor > editor.cursor_pos ?
                     editor.select_anchor : editor.cursor_pos;
    
    // Find the first and last lines in the selection
    int first_line = editor_get_line_at_pos(sel_start);
    int last_line = editor_get_line_at_pos(sel_end > 0 ? sel_end - 1 : 0);
    
    // If selection ends exactly at start of a line (not including any content),
    // don't include that line
    if (sel_end > 0 && sel_end <= editor.content_length) {
        int line_start = editor_get_line_start(last_line);
        if ((size_t)line_start == sel_end) {
            last_line--;
        }
    }
    
    // Process each line from last to first (to preserve line numbers)
    for (int line = last_line; line >= first_line; line--) {
        int line_start = editor_get_line_start(line);
        int line_end = editor_get_line_end(line);
        int line_len = line_end - line_start;
        
        // Count leading spaces on this line
        int leading_spaces = 0;
        for (int i = 0; i < line_len && editor.buffer[line_start + i] == ' '; i++) {
            leading_spaces++;
        }
        
        // Calculate how many spaces to remove (up to TAB_WIDTH)
        int spaces_to_remove = leading_spaces < TAB_WIDTH ? leading_spaces : TAB_WIDTH;
        
        if (spaces_to_remove > 0) {
            // Shift content left to remove spaces
            editor_note_change((size_t)line_start, (size_t)spaces_to_remove, NULL, 0);
            editor_lines_edit(&editor.lines, (size_t)line_start);
            memmove(&editor.buffer[line_start],
                    &editor.buffer[line_start + spaces_to_remove],
                    editor.content_length - line_start - spaces_to_remove + 1);  // +1 for null
            
            editor.content_length -= spaces_to_remove;
            
            // Adjust cursor and anchor positions if they're after the removed spaces
            if (editor.cursor_pos >= (size_t)(line_start + spaces_to_remove)) {
                editor.cursor_pos -= spaces_to_remove;
            } else if (editor.cursor_pos > (size_t)line_start) {
                editor.cursor_pos = line_start;
            }
            
            if (editor.select_anchor >= (size_t)(line_start + spaces_to_remove)) {
                editor.select_anchor -= spaces_to_remove;
            } else if (editor.select_anchor > (size_t)line_start) {
                editor.select_anchor = line_start;
            }
        }
    }
}

//
// Increase indent of selected block by one tab stop (Ctrl+.)
// Adds TAB_WIDTH spaces to the beginning of each line in selection
//
static void editor_increase_indent(void)
{
    if (!editor.selecting) return;
    
    // Get selection bounds
    size_t sel_start = editor.select_anchor < editor.cursor_pos ? 
                       editor.select_anchor : editor.cursor_pos;
    size_t sel_end = editor.select_anchor > editor.cursor_pos ?
                     editor.select_anchor : editor.cursor_pos;
    
    // Find the first and last lines in the selection
    int first_line = editor_get_line_at_pos(sel_start);
    int last_line = editor_get_line_at_pos(sel_end > 0 ? sel_end - 1 : 0);
    
    // If selection ends exactly at start of a line (not including any content),
    // don't include that line
    if (sel_end > 0 && sel_end <= editor.content_length) {
        int line_start = editor_get_line_start(last_line);
        if ((size_t)line_start == sel_end) {
            last_line--;
        }
    }
    
    // Calculate total spaces needed
    int lines_to_indent = last_line - first_line + 1;
    size_t total_spaces = lines_to_indent * TAB_WIDTH;
    
    // Check buffer space
    if (editor.content_length + total_spaces >= editor.buffer_size) {
        return;  // Not enough space
    }
    
    char tab_spaces[TAB_WIDTH];
    memset(tab_spaces, ' ', sizeof(tab_spaces));

    // Process each line from last to first (to preserve line numbers)
    for (int line = last_line; line >= first_line; line--) {
        int line_start = editor_get_line_start(line);
        
        // Shift content right to make room for spaces
        editor_note_change((size_t)line_start, 0, tab_spaces, TAB_WIDTH);
        editor_lines_edit(&editor.lines, (size_t)line_start);
        memmove(&editor.buffer[line_start + TAB_WIDTH],
                &editor.buffer[line_start],
                editor.content_length - line_start + 1);  // +1 for null
        
        // Insert spaces
        for (int i = 0; i < TAB_WIDTH; i++) {
            editor.buffer[line_start + i] = ' ';
        }
        
        editor.content_length += TAB_WIDTH;
        
        // Adjust cursor and anchor positions if they're at or after the line start
        if (editor.cursor_pos >= (size_t)line_start) {
            editor.cursor_pos += TAB_WIDTH;
        }
        
        if (editor.select_anchor >= (size_t)line_start) {
            editor.select_anchor += TAB_WIDTH;
        }
    }
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
    editor_note_change((size_t)line_start, line_len, NULL, 0);
    editor_lines_edit(&editor.lines, (size_t)line_start);
    memmove(&editor.buffer[line_start],
            &editor.buffer[line_start + line_len],
            editor.content_length - line_start - line_len + 1);  // +1 for null terminator
    
    editor.content_length -= line_len;
    editor.buffer[editor.content_length] = '\0';
    editor.cursor_pos = line_start;
}

//
// Run the current search text from `from` and show the result.
// A match is selected (as block editing does); with no match anywhere the
// selection is dropped and the cursor returns to where the search left off.
//
static void editor_search_apply(size_t from, bool forward)
{
    size_t match;

    if (editor.search_len > 0 &&
        editor_search_find(editor.buffer, editor.content_length,
                           editor.search_text, editor.search_len,
                           from, forward, &match)) {
        editor.search_origin = match;
        editor.select_anchor = match;
        editor.cursor_pos = match + editor.search_len;
        editor.selecting = true;
        lcd_set_cursor_style(LCD_CURSOR_BLOCK);
    } else {
        editor.cursor_pos = editor.search_origin;
        editor.selecting = false;
        lcd_set_cursor_style(LCD_CURSOR_UNDERLINE);
    }

    editor_mark_all_dirty();  // The match can be anywhere in the buffer
}

//
// Handle a key while incremental search is active
// Returns false to let the main loop handle the key as well (BRK cancels)
//
static bool editor_handle_search_key(char key)
{
    switch (key) {
        case KEY_ESC:
            // Leave the search; any selected text remains selected
            editor.searching = false;
            editor_draw_footer();
            break;

        case KEY_BREAK:
            // Fall through to the editor's cancel handling
            editor.searching = false;
            return false;

        case KEY_DOWN:
            editor_search_apply(editor.search_origin + 1, true);
            break;

        case KEY_UP:
            editor_search_apply(editor.search_origin, false);
            break;

        case 0x12:  // Ctrl+R - type the text every match is replaced with
            if (editor.search_len > 0) {
                editor.replacing = true;
                editor.replace_text[0] = '\0';
                editor.replace_len = 0;
                editor.replace_cursor = 0;
                // The cursor moves to the footer, where a block would be
                // indistinguishable from the reverse video around it
                lcd_set_cursor_style(LCD_CURSOR_UNDERLINE);
                editor_draw_footer();
            }
            break;

        case KEY_BACKSPACE:
            // Remove the last letter to widen the search back out
            if (editor.search_len > 0) {
                editor.search_text[--editor.search_len] = '\0';
                editor_search_apply(editor.search_origin, true);
                editor_draw_footer();
            }
            break;

        default:
            // Printable characters extend the search text
            if (key >= 0x20 && key <= 0x7E && editor.search_len < EDITOR_SEARCH_MAX) {
                editor.search_text[editor.search_len++] = key;
                editor.search_text[editor.search_len] = '\0';
                editor_search_apply(editor.search_origin, true);
                editor_draw_footer();
            }
            break;
    }

    return true;
}

//
// Replace every match of the search text with the replacement and leave the
// search. The matches are all over the buffer, so the selection is dropped and
// the cursor keeps its place only as far as the rewritten text allows.
//
static void editor_replace_all(void)
{
    editor_search_replace_all(editor.buffer, &editor.content_length, editor.buffer_size,
                              editor.search_text, editor.search_len,
                              editor.replace_text, editor.replace_len);
    editor_lines_reset(&editor.lines);  // Matches anywhere; the whole buffer may have moved
    // The one rewrite the journal is not told about, since it is the search's
    // own bulk replace. Vi reaches this text through `:%s`, which is recorded;
    // forgetting the journal is what keeps the two from ever meeting.
    editor_undo_reset(&editor.undo);

    if (editor.cursor_pos > editor.content_length) {
        editor.cursor_pos = editor.content_length;
    }

    editor.replacing = false;
    editor.searching = false;
    editor.selecting = false;
    lcd_set_cursor_style(LCD_CURSOR_UNDERLINE);
    editor_draw_footer();
    editor_mark_all_dirty();
}

//
// Handle a key while the replacement text is being typed
// Returns false to let the main loop handle the key as well (BRK cancels)
//
static bool editor_handle_replace_key(char key)
{
    switch (key) {
        case KEY_ESC:
            // Abandon the replacement and go back to the search
            editor.replacing = false;
            if (editor.selecting) {
                lcd_set_cursor_style(LCD_CURSOR_BLOCK);
            }
            editor_draw_footer();
            break;

        case KEY_BREAK:
            // Fall through to the editor's cancel handling
            editor.replacing = false;
            editor.searching = false;
            return false;

        case KEY_ENTER:
        case KEY_RETURN:
            editor_replace_all();
            break;

        case KEY_LEFT:
            if (editor.replace_cursor > 0) {
                editor.replace_cursor--;
            }
            break;

        case KEY_RIGHT:
            if (editor.replace_cursor < editor.replace_len) {
                editor.replace_cursor++;
            }
            break;

        case KEY_BACKSPACE:
            // Delete the character to the left of the cursor
            if (editor.replace_cursor > 0) {
                editor.replace_cursor--;
                memmove(&editor.replace_text[editor.replace_cursor],
                        &editor.replace_text[editor.replace_cursor + 1],
                        editor.replace_len - editor.replace_cursor);
                editor.replace_len--;
                editor_draw_footer();
            }
            break;

        case KEY_DEL:
            // Delete the character at the cursor
            if (editor.replace_cursor < editor.replace_len) {
                memmove(&editor.replace_text[editor.replace_cursor],
                        &editor.replace_text[editor.replace_cursor + 1],
                        editor.replace_len - editor.replace_cursor);
                editor.replace_len--;
                editor_draw_footer();
            }
            break;

        default:
            // Printable characters are inserted at the cursor; everything else,
            // TAB included, is ignored
            if (key >= 0x20 && key <= 0x7E && editor.replace_len < EDITOR_SEARCH_MAX) {
                memmove(&editor.replace_text[editor.replace_cursor + 1],
                        &editor.replace_text[editor.replace_cursor],
                        editor.replace_len - editor.replace_cursor + 1);
                editor.replace_text[editor.replace_cursor++] = key;
                editor.replace_len++;
                editor_draw_footer();
            }
            break;
    }

    return true;
}

//
// Main editor function
//
//
// Put the screen back the way the editor found it. Both exits do the same
// thing, and vi mode's :w / ZZ and :q! / ZQ do it from a third place.
//
static void editor_restore_screen(uint8_t saved_screen_mode,
                                  uint8_t saved_cursor_col, uint8_t saved_cursor_row)
{
    lcd_erase_cursor();
    screen_txt_enable_cursor(false);
    lcd_set_cursor_style(LCD_CURSOR_UNDERLINE);  // May still be block if exiting mid-selection
    input_active = false;  // Re-enable keyboard mode switching
    // Restore foreground/background palette slots
    lcd_set_foreground(PALETTE_FG);
    lcd_set_background(PALETTE_BG);
    lcd_define_scrolling(0, 0);          // Drop the editor's fixed header row
    screen_set_mode(saved_screen_mode);  // Restore screen mode
    screen_txt_mark_all_dirty();         // Editor drew directly to LCD; repaint text buffer
    screen_txt_update();                 // Flush immediately so the user sees the REPL
    screen_txt_set_cursor(saved_cursor_col, saved_cursor_row);
}

//
//  Vi mode (docs/vi-mode-design.md)
//
//  editor_vi.c decides what a keystroke means and says so as a byte range plus
//  an action; everything below turns that into an edit. Nothing here parses a
//  key, and nothing in editor_vi.c touches the screen.
//

// Whether the mode is on is set from Logo by `setvimode` before the editor
// opens, so all five entry points -- edit, edall, edn, edns, editfile -- get it
static bool editor_vi_requested = false;

// Where the undo journal lives. The interpreter owns it, because which tier a
// board gets is its decision to make: a slice of the aux/PSRAM region where
// there is one, a small heap block otherwise, and nothing at all if neither
// could be had -- in which case `u` says so (docs/vi-mode-design.md §8).
static char *editor_undo_store = NULL;
static size_t editor_undo_capacity = 0;

// What editor_vi_apply asks the main loop to do next
#define EDITOR_VI_CONTINUE 0
#define EDITOR_VI_ACCEPT   1
#define EDITOR_VI_CANCEL   2

//
// Insert text at pos. Returns false when it would not fit, having changed
// nothing. `text` must not point into the buffer: it moves.
//
static bool editor_vi_insert_text(size_t pos, const char *text, size_t len)
{
    if (len == 0) {
        return true;
    }
    if (editor.content_length + len >= editor.buffer_size) {
        return false;
    }
    editor_note_change(pos, 0, text, len);
    editor_lines_edit(&editor.lines, pos);
    memmove(&editor.buffer[pos + len], &editor.buffer[pos], editor.content_length - pos);
    memcpy(&editor.buffer[pos], text, len);
    editor.content_length += len;
    editor.buffer[editor.content_length] = '\0';
    return true;
}

static void editor_vi_delete_range(size_t start, size_t end)
{
    if (end > editor.content_length) end = editor.content_length;
    if (start >= end) return;

    editor_note_change(start, end - start, NULL, 0);
    editor_lines_edit(&editor.lines, start);
    memmove(&editor.buffer[start], &editor.buffer[end], editor.content_length - end);
    editor.content_length -= (end - start);
    editor.buffer[editor.content_length] = '\0';
}

//
// Take a range into the copy buffer, which is the unnamed register vi mode has
// instead of the sixteen it does not. Linewise text is marked by a trailing
// newline, the same way the editor's own Ctrl+C on a whole line marks it.
//
static void editor_vi_yank_range(size_t start, size_t end, bool linewise)
{
    if (end > editor.content_length) end = editor.content_length;
    if (start > end) start = end;

    size_t len = end - start;
    if (len > LOGO_COPY_BUFFER_SIZE - 2) {
        len = LOGO_COPY_BUFFER_SIZE - 2;
    }
    memcpy(editor.copy_buffer, &editor.buffer[start], len);

    if (linewise) {
        // `dd` on the last line has to take the newline *before* it, since
        // there is none after; the copy buffer wants it the other way round
        if (len > 0 && editor.copy_buffer[0] == '\n' && editor.copy_buffer[len - 1] != '\n') {
            memmove(editor.copy_buffer, editor.copy_buffer + 1, --len);
        }
        if (len == 0 || editor.copy_buffer[len - 1] != '\n') {
            editor.copy_buffer[len++] = '\n';
        }
    }

    editor.copy_buffer[len] = '\0';
    editor.copy_length = len;
}

static void editor_vi_paste(int count, bool after)
{
    if (editor.copy_length == 0) return;

    bool linewise = editor.copy_buffer[editor.copy_length - 1] == '\n';
    int line = editor_get_line_at_pos(editor.cursor_pos);
    size_t at;

    if (linewise) {
        if (after) {
            at = (size_t)editor_get_line_end(line);
            if (at < editor.content_length) {
                at++;  // Past the newline, onto the next line
            } else {
                // The last line has no newline of its own to paste after
                if (!editor_vi_insert_text(at, "\n", 1)) return;
                at = editor.content_length;
            }
        } else {
            at = (size_t)editor_get_line_start(line);
        }
    } else {
        at = editor.cursor_pos;
        if (after && at < (size_t)editor_get_line_end(line)) {
            at++;
        }
    }

    size_t inserted = 0;
    for (int i = 0; i < count; i++) {
        if (!editor_vi_insert_text(at + inserted, editor.copy_buffer, editor.copy_length)) {
            break;
        }
        inserted += editor.copy_length;
    }
    if (inserted == 0) return;

    if (linewise) {
        // Vi leaves the cursor on the first non-blank of the first line pasted
        editor.cursor_pos = (size_t)editor_get_line_start(editor_get_line_at_pos(at));
        int end = editor_get_line_end(editor_get_line_at_pos(at));
        while (editor.cursor_pos < (size_t)end &&
               (editor.buffer[editor.cursor_pos] == ' ' ||
                editor.buffer[editor.cursor_pos] == '\t')) {
            editor.cursor_pos++;
        }
    } else {
        editor.cursor_pos = at + inserted - 1;
    }
}

//
// Join `count` lines onto the cursor's, putting a single space where each line
// break was and swallowing the indentation that followed it
//
static void editor_vi_join(int count)
{
    for (int i = 1; i < count; i++) {
        int line = editor_get_line_at_pos(editor.cursor_pos);
        size_t line_start = (size_t)editor_get_line_start(line);
        size_t end = (size_t)editor_get_line_end(line);
        if (end >= editor.content_length) {
            break;  // Nothing left to join
        }

        size_t next = end + 1;
        while (next < editor.content_length &&
               (editor.buffer[next] == ' ' || editor.buffer[next] == '\t')) {
            next++;
        }
        editor_vi_delete_range(end, next);
        editor.cursor_pos = end;

        // No space onto an empty line, and none where the line already ends in one
        if (end > line_start && editor.buffer[end - 1] != ' ' &&
            end < editor.content_length && editor.buffer[end] != '\n') {
            editor_vi_insert_text(end, " ", 1);
        }
    }
}

static void editor_vi_toggle_case(size_t start, size_t end)
{
    if (end > editor.content_length) end = editor.content_length;
    for (size_t i = start; i < end; i++) {
        char c = editor.buffer[i];
        char flipped = c;
        if (c >= 'a' && c <= 'z')      flipped = (char)(c - 'a' + 'A');
        else if (c >= 'A' && c <= 'Z') flipped = (char)(c - 'A' + 'a');
        if (flipped == c) continue;
        editor_note_change(i, 1, &flipped, 1);
        editor.buffer[i] = flipped;
    }
}

//
// Open a line above or below the cursor's, carrying its indentation, and leave
// the cursor on it ready to be typed into
//
static void editor_vi_open_line(bool below)
{
    int line = editor_get_line_at_pos(editor.cursor_pos);
    size_t line_start = (size_t)editor_get_line_start(line);

    char text[EDITOR_MAX_COLS + 1];
    size_t indent = 0;
    while (line_start + indent < editor.content_length && indent < sizeof(text) - 1 &&
           (editor.buffer[line_start + indent] == ' ' ||
            editor.buffer[line_start + indent] == '\t')) {
        indent++;
    }

    if (below) {
        size_t at = (size_t)editor_get_line_end(line);
        text[0] = '\n';
        memcpy(text + 1, &editor.buffer[line_start], indent);
        if (!editor_vi_insert_text(at, text, indent + 1)) return;
        editor.cursor_pos = at + 1 + indent;
    } else {
        memcpy(text, &editor.buffer[line_start], indent);
        text[indent] = '\n';
        if (!editor_vi_insert_text(line_start, text, indent + 1)) return;
        editor.cursor_pos = line_start + indent;
    }
}

//
// Shift the lines the range covers by `stops` tab stops, driving the same
// indent pair Ctrl+, and Ctrl+. use by handing them a selection
//
static void editor_vi_indent(size_t start, size_t end, int stops)
{
    editor.selecting = true;
    editor.select_anchor = start;
    editor.cursor_pos = end > start ? end - 1 : start;

    for (int i = 0; i < (stops < 0 ? -stops : stops); i++) {
        if (stops > 0) editor_increase_indent();
        else           editor_decrease_indent();
    }

    editor.selecting = false;
    int line = editor_get_line_at_pos(editor.select_anchor);
    editor.cursor_pos = (size_t)editor_get_line_start(line);
    int line_end = editor_get_line_end(line);
    while (editor.cursor_pos < (size_t)line_end &&
           (editor.buffer[editor.cursor_pos] == ' ' ||
            editor.buffer[editor.cursor_pos] == '\t')) {
        editor.cursor_pos++;
    }
}

//
// Run the vi pattern through the editor's own search, so `/` and `n` land
// exactly where Ctrl+F does -- wrapping, case-insensitive -- but leave no
// selection behind: normal mode's block cursor is the only marker vi wants
//
static void editor_vi_search(char direction, size_t origin)
{
    // The vi pattern goes straight to the pattern walker -- no copy into
    // editor.search_text, which was only ever there to feed editor_search_find
    // and would truncate a `\<name\>` pattern to a dangling backslash (§16.5).
    // The origin is the cursor for `/`, `?`, `n` and `N`, and the start of the
    // word for `*` and `#` -- which is what stops `*` from the middle of a word
    // matching the word it started in
    bool forward = (direction == '/');
    size_t from = forward ? origin + 1 : origin;
    if (from > editor.content_length) from = editor.content_length;

    size_t match;
    bool too_complex = false;
    if (editor_pattern_find(editor.vi.pattern, editor.vi.pattern_len,
                            editor.buffer, editor.content_length,
                            from, forward, &match, &too_complex)) {
        editor.cursor_pos = match;
    } else if (too_complex) {
        // The search gave up rather than wedging the board (B36) -- said
        // separately, because it is not the same news as an honest miss
        editor.vi_msg = "E486: pattern too complex";
    } else {
        editor.vi_msg = "E486: pattern not found";
    }
    editor_mark_all_dirty();  // The match can be anywhere in the buffer
}

//
// Carry out one action. Returns EDITOR_VI_ACCEPT / _CANCEL when the action is
// one that leaves the editor.
//
static int editor_vi_apply(const ViAction *act, int cursor_line_before)
{
    switch (act->kind) {
        case VI_ACT_NONE:
            break;

        case VI_ACT_REDRAW:
            editor.dirty_flags = DIRTY_CURSOR;
            break;

        case VI_ACT_BEEP:
        case VI_ACT_MESSAGE:
            editor.vi_msg = act->msg;
            editor.dirty_flags = DIRTY_CURSOR;
            break;

        case VI_ACT_MOVE:
            editor.cursor_pos = act->start;
            editor.dirty_flags = DIRTY_CURSOR;
            break;

        case VI_ACT_YANK:
            editor_vi_yank_range(act->start, act->end, act->linewise);
            editor.cursor_pos = act->start;
            editor.dirty_flags = DIRTY_CURSOR;
            break;

        case VI_ACT_DELETE:
        case VI_ACT_CHANGE:
            editor_vi_yank_range(act->start, act->end, act->linewise);
            editor_vi_delete_range(act->start, act->end);
            editor.cursor_pos = act->start;
            editor.vi.modified = true;
            editor_mark_from_line_dirty(editor_get_line_at_pos(act->start));
            break;

        case VI_ACT_PASTE_AFTER:
        case VI_ACT_PASTE_BEFORE:
            editor_vi_paste(act->count > 0 ? act->count : 1,
                            act->kind == VI_ACT_PASTE_AFTER);
            editor.vi.modified = true;
            editor_mark_from_line_dirty(cursor_line_before);
            break;

        case VI_ACT_PASTE_OVER:
            editor_vi_delete_range(act->start, act->end);
            editor.cursor_pos = act->start;
            editor_vi_paste(1, false);
            editor.vi.modified = true;
            editor_mark_from_line_dirty(editor_get_line_at_pos(act->start));
            break;

        case VI_ACT_INDENT:
            editor_vi_indent(act->start, act->end, act->count);
            editor.vi.modified = true;
            editor_mark_all_dirty();
            break;

        case VI_ACT_REPLACE_CHAR:
            if (act->ch == '\n') {
                // A split, not an overwrite: the range goes and one line break
                // takes its place, so the buffer changes length and every line
                // from this one down moves
                editor_vi_delete_range(act->start, act->end);
                editor_vi_insert_text(act->start, "\n", 1);
                editor.cursor_pos = act->start + 1;
                editor.vi.modified = true;
                editor_mark_from_line_dirty(cursor_line_before);
                break;
            }
            for (size_t i = act->start; i < act->end && i < editor.content_length; i++) {
                if (editor.buffer[i] == act->ch) continue;
                editor_note_change(i, 1, &act->ch, 1);
                editor.buffer[i] = act->ch;
            }
            editor.cursor_pos = act->end > act->start ? act->end - 1 : act->start;
            editor.vi.modified = true;
            editor_mark_line_dirty(cursor_line_before);
            break;

        case VI_ACT_OPEN_BELOW:
        case VI_ACT_OPEN_ABOVE:
            editor_vi_open_line(act->kind == VI_ACT_OPEN_BELOW);
            editor.vi.modified = true;
            editor_mark_from_line_dirty(cursor_line_before);
            break;

        case VI_ACT_JOIN:
            editor.cursor_pos = act->start;
            editor_vi_join(act->count);
            editor.vi.modified = true;
            editor_mark_from_line_dirty(cursor_line_before);
            break;

        case VI_ACT_TOGGLE_CASE:
            editor_vi_toggle_case(act->start, act->end);
            editor.cursor_pos = act->end < editor.content_length ? act->end : act->start;
            editor.vi.modified = true;
            editor_mark_from_line_dirty(editor_get_line_at_pos(act->start));
            break;

        case VI_ACT_INCREMENT: {
            size_t landed = editor.cursor_pos;
            ViIncrement done = editor_vi_increment(editor.buffer, &editor.content_length,
                                                   editor.buffer_size, act->start,
                                                   act->count, &editor.undo, &landed);
            if (done != VI_INC_OK) {
                editor.vi_msg = (done == VI_INC_NO_ROOM) ? "Not enough room"
                                                         : "No number under the cursor";
                editor.dirty_flags = DIRTY_CURSOR;
                break;
            }
            editor_lines_reset(&editor.lines);
            editor.cursor_pos = landed;
            editor.vi.modified = true;
            editor_mark_from_line_dirty(cursor_line_before);
            break;
        }

        case VI_ACT_SEARCH:
            editor_vi_search(act->ch, act->start);
            break;

        case VI_ACT_SCROLL: {
            // The one action that moves the view and not the cursor, so it does
            // its own row arithmetic here rather than leaving it to
            // editor_ensure_cursor_visible, which only ever scrolls far enough
            // to bring the cursor back on screen
            int cursor_line = editor_get_line_at_pos(editor.cursor_pos);
            int max_start = editor_count_lines() - EDITOR_VISIBLE_ROWS;
            int start = cursor_line;
            if (act->ch == 'z') {
                start = cursor_line - (EDITOR_VISIBLE_ROWS - 1) / 2;
            } else if (act->ch == 'b') {
                start = cursor_line - EDITOR_VISIBLE_ROWS + 1;
            }
            // The view never starts past the last screenful, as the page keys
            // have it -- so `zt` near the end of the buffer moves less than it
            // was asked to rather than showing a screen of nothing
            if (start > max_start) start = max_start;
            if (start < 0) start = 0;
            editor.view_start_line = start;
            editor_mark_all_dirty();
            break;
        }

        case VI_ACT_SUBSTITUTE: {
            size_t landed = editor.cursor_pos;
            size_t count = editor_vi_substitute(editor.buffer, &editor.content_length,
                                                editor.buffer_size, act->start, act->end,
                                                editor.vi.pattern, editor.vi.pattern_len,
                                                editor.vi.replacement,
                                                editor.vi.replacement_len,
                                                editor.vi.sub_global, &editor.undo, &landed);
            if (count == SIZE_MAX) {
                // The pattern outran the matcher's step budget and the
                // substitute was abandoned before a byte moved (B36)
                editor.vi_msg = "E486: pattern too complex";
                editor.dirty_flags = DIRTY_CURSOR;
            } else if (count == 0) {
                // No match, or a result that would not fit -- editor_vi_substitute
                // refuses the second rather than rewriting half the buffer
                editor.vi_msg = "No substitution made";
                editor.dirty_flags = DIRTY_CURSOR;
            } else {
                editor_lines_reset(&editor.lines);
                editor.cursor_pos = landed;
                editor.vi.modified = true;
                editor_mark_all_dirty();
            }
            break;
        }

        case VI_ACT_MOVE_LINES: {
            size_t landed = editor.cursor_pos;
            if (!editor_vi_move_lines(editor.buffer, &editor.content_length,
                                      editor.buffer_size, act->start, act->end,
                                      act->dest, act->ch == 't', &editor.undo,
                                      &landed)) {
                editor.vi_msg = "Not enough room";
                editor.dirty_flags = DIRTY_CURSOR;
                break;
            }
            editor_lines_reset(&editor.lines);
            editor.cursor_pos = landed;
            editor.vi.modified = true;
            editor_mark_all_dirty();
            break;
        }

        case VI_ACT_GLOBAL: {
            size_t landed = editor.cursor_pos;
            size_t count = editor_vi_global(editor.buffer, &editor.content_length,
                                            editor.buffer_size, act->start, act->end,
                                            editor.vi.pattern, editor.vi.pattern_len,
                                            act->invert, act->ch,
                                            act->sub_pattern, act->sub_pattern_len,
                                            editor.vi.replacement,
                                            editor.vi.replacement_len,
                                            editor.vi.sub_global, &editor.undo, &landed);
            if (count == SIZE_MAX) {
                editor.vi_msg = "E486: pattern too complex";
                editor.dirty_flags = DIRTY_CURSOR;
                break;
            }
            if (count == 0) {
                editor.vi_msg = "No lines matched";
                editor.dirty_flags = DIRTY_CURSOR;
                break;
            }
            // One line of typing can take hundreds of lines away, and on a
            // board with no PSRAM a pass that large is more than the undo
            // journal holds -- so it says what it did rather than returning
            // silently (vi-mode-design.md §23.4)
            snprintf(editor.vi.msg, sizeof(editor.vi.msg),
                     act->ch == 'd' ? "%u fewer lines" : "%u lines changed",
                     (unsigned)count);
            editor.vi_msg = editor.vi.msg;
            editor_lines_reset(&editor.lines);
            editor.cursor_pos = landed;
            editor.vi.modified = true;
            editor_mark_all_dirty();
            break;
        }

        case VI_ACT_UNDO:
        case VI_ACT_REDO: {
            bool forward = (act->kind == VI_ACT_REDO);
            int steps = act->count > 0 ? act->count : 1;
            size_t at = editor.cursor_pos;
            bool any = false;

            for (int i = 0; i < steps; i++) {
                size_t pos;
                bool moved = forward
                    ? editor_undo_redo(&editor.undo, editor.buffer, &editor.content_length,
                                       editor.buffer_size, &pos)
                    : editor_undo_undo(&editor.undo, editor.buffer, &editor.content_length,
                                       editor.buffer_size, &pos);
                if (!moved) {
                    break;
                }
                any = true;
                at = pos;
            }

            if (!any) {
                editor.vi_msg = editor.undo.store == NULL
                    ? "Undo is not available"
                    : (forward ? "Already at newest change" : "Already at oldest change");
                editor.dirty_flags = DIRTY_CURSOR;
                break;
            }

            // A step can have moved text anywhere in the buffer, and several of
            // them certainly have
            editor_lines_reset(&editor.lines);
            editor.cursor_pos = at;
            editor.vi.modified = true;
            editor_mark_all_dirty();
            break;
        }

        case VI_ACT_WRITE:
            // With nowhere to write to, `:w` means what it always meant: hand
            // the buffer back to the caller, which is how the workspace is saved
            if (editor.save == NULL) {
                return EDITOR_VI_ACCEPT;
            }
            if (editor.save(editor.buffer, editor.save_ctx)) {
                editor.vi.modified = false;
                editor.vi_msg = "written";
            } else {
                editor.vi_msg = "E212: can't open file for writing";
            }
            editor.dirty_flags = DIRTY_CURSOR;
            break;

        case VI_ACT_ACCEPT:
            return EDITOR_VI_ACCEPT;

        case VI_ACT_CANCEL:
            return EDITOR_VI_CANCEL;
    }

    // `.` repeating a change that ended in insert mode carries the text that
    // was typed then. The action above has made room for it -- deleted the
    // word, opened the line -- and typing it here is what makes the repeat the
    // whole change rather than half of it (vi-mode-design.md §20). Then step
    // back off the last character, as Esc does.
    if (act->insert != NULL) {
        if (editor_vi_insert_text(editor.cursor_pos, act->insert, act->insert_len)) {
            editor.cursor_pos += act->insert_len;
            size_t line_start =
                (size_t)editor_get_line_start(editor_get_line_at_pos(editor.cursor_pos));
            if (editor.cursor_pos > line_start) {
                editor.cursor_pos--;
            }
            // Only bytes that actually went in are a change. A repeat of `i`
            // closed without typing anything is a cursor move, and the keys it
            // repeats did not set this either; a full buffer took nothing.
            // Whatever the change was, the action above has already said so.
            if (act->insert_len > 0) {
                editor.vi.modified = true;
            }
        }
        editor_mark_from_line_dirty(cursor_line_before);
    }

    if (editor.cursor_pos > editor.content_length) {
        editor.cursor_pos = editor.content_length;
    }

    // Visual mode is the editor's own selection anchor, verbatim: the block
    // cursor covers the character the anchor stops short of
    bool visual = (editor.vi.mode == VI_VISUAL || editor.vi.mode == VI_VISUAL_LINE);
    if (visual || editor.selecting) {
        editor.select_anchor = editor.vi.anchor;
        if (editor.selecting != visual) {
            editor_mark_all_dirty();
        } else if (visual) {
            int line = editor_get_line_at_pos(editor.cursor_pos);
            editor_mark_from_line_dirty(line < cursor_line_before ? line : cursor_line_before);
        }
        editor.selecting = visual;
    }

    lcd_set_cursor_style((editor.vi.mode == VI_INSERT || editor.vi.mode == VI_CMDLINE)
                         ? LCD_CURSOR_UNDERLINE : LCD_CURSOR_BLOCK);
    return EDITOR_VI_CONTINUE;
}

// Insert mode hands every key but Esc back to the editor's own handling, so
// this is where a change made by typing gets noticed -- `:q` has to know
static bool editor_vi_key_modifies(int key)
{
    return (key >= 0x20 && key <= 0x7E) || key == KEY_BACKSPACE || key == KEY_DEL ||
           key == KEY_ENTER || key == KEY_RETURN || key == KEY_TAB;
}

void picocalc_editor_set_vi_mode(bool on)
{
    editor_vi_requested = on;
}

void picocalc_editor_set_undo_store(void *store, size_t size)
{
    editor_undo_store = (char *)store;
    editor_undo_capacity = size;
}

LogoEditorResult picocalc_editor_edit(char *buffer, size_t buffer_size,
                                      LogoEditorSave save, void *save_ctx)
{
    // Save cursor position and screen mode to restore on exit
    uint8_t saved_cursor_col, saved_cursor_row;
    screen_txt_get_cursor(&saved_cursor_col, &saved_cursor_row);
    uint8_t saved_screen_mode = screen_get_mode();
    
    // Clear any pending screensaver dismissed flag - we're about to redraw anyway
    // This prevents a spurious redraw on the first keypress if the screensaver
    // was dismissed while typing the command that launched the editor
    screensaver_dismissed = false;
    
    // Initialize editor state
    editor.buffer = buffer;
    editor.buffer_size = buffer_size;
    editor.content_length = strlen(buffer);
    editor_lines_reset(&editor.lines);
    editor.cursor_pos = 0;  // Start at beginning of content
    editor.view_start_line = 0;
    editor.h_scroll_offset = 0;
    editor.selecting = false;
    editor.select_anchor = 0;
    editor.copy_buffer[0] = '\0';
    editor.copy_length = 0;
    editor.searching = false;
    editor.search_text[0] = '\0';
    editor.search_len = 0;
    editor.search_origin = 0;
    editor.replacing = false;
    editor.replace_text[0] = '\0';
    editor.replace_len = 0;
    editor.replace_cursor = 0;
    editor.in_graphics_preview = false;
    editor.dirty_flags = DIRTY_NONE;
    editor.vi_mode = editor_vi_requested;
    editor.vi_msg = NULL;
    editor.save = save;
    editor.save_ctx = save_ctx;
    editor_vi_reset(&editor.vi);
    // No journal outside vi mode: nothing there can reach it, and recording
    // into it would only cost the default editor time
    editor_undo_init(&editor.undo, editor.vi_mode ? editor_undo_store : NULL,
                     editor_undo_capacity);

    // Normal mode is a block cursor, which is also what the editor already uses
    // to mean "a selection is active" -- so visual mode needs no new drawing
    lcd_set_cursor_style(editor.vi_mode ? LCD_CURSOR_BLOCK : LCD_CURSOR_UNDERLINE);
    
    // Use syntax palette colours for the editor
    // (defaults set at LCD init; user can override with setpalette or theme files)
    lcd_set_foreground(PALETTE_SYNTAX_DEFAULT);
    lcd_set_background(PALETTE_SYNTAX_BG);
    
    // Ensure cursor is on second line if we have "to name\n" template
    // (currently we start at beginning; template-specific positioning can be added here)
    
    // Switch to full-screen text mode for the editor
    // Use no_update to avoid redrawing txt_buffer - we'll draw editor content instead
    screen_set_mode_no_update(SCREEN_MODE_TXT);
    
    // Tell keyboard_poll to skip mode switching - editor handles it
    input_active = true;
    
    // Clear screen and draw initial content
    lcd_clear_screen(PALETTE_SYNTAX_BG);

    // Fix the header and footer rows and scroll the content between them, so
    // moving the view by a line is a start-line change instead of a full repaint
    lcd_define_scrolling(EDITOR_SCROLL_TOP, EDITOR_SCROLL_BOTTOM);

    editor_draw_header();
    editor_draw_footer();
    editor_draw_content();
    editor_ensure_cursor_visible();
    
    // Position cursor BEFORE enabling it - this ensures screen_txt_enable_cursor
    // sees a valid cursor location (important when coming from splitscreen)
    editor_position_cursor();
    
    // Now enable and draw cursor - cursor position is already set.
    // Use the lcd_ cursor calls, not screen_txt_draw/erase_cursor: those
    // re-sync the cursor character from txt_buffer, which holds the stale
    // text screen underneath the editor.
    screen_txt_enable_cursor(true);
    lcd_draw_cursor();  // Draw cursor immediately after enabling

    // Main editor loop
    while (true) {
        // Draw cursor before waiting for key (in case it was erased)
        lcd_draw_cursor();
        char key = keyboard_get_key();
        // Erase cursor before modifying screen
        lcd_erase_cursor();
        
        // Check if screen saver was just dismissed - need full redraw
        if (screensaver_dismissed) {
            screensaver_dismissed = false;  // Clear the flag
            lcd_clear_screen(PALETTE_SYNTAX_BG);
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

        // Incremental search and its replacement prompt consume every key they
        // handle; clearing the key leaves the normal handling below with nothing to do
        if (editor.replacing) {
            if (editor_handle_replace_key(key)) {
                key = 0;
            }
        } else if (editor.searching && editor_handle_search_key(key)) {
            key = 0;
        }

        // The vi key layer. It either consumes the key -- and says what it
        // meant as a byte range the switch below never sees -- or hands it
        // back, which is how insert mode gets the arrows, backspace and every
        // printable character from the default handling unchanged, and how Brk
        // keeps its cancel from every mode.
        if (key != 0 && editor.vi_mode) {
            // One keystroke is one undo step, except while insert mode is
            // running: everything typed between `i` and `Esc` belongs to the
            // command that opened it, which is what vi undoes in one go
            if (editor.vi.mode != VI_INSERT) {
                editor_undo_begin(&editor.undo);
            }

            const char *msg_before = editor.vi_msg;
            ViMode mode_before = editor.vi.mode;
            ViAction act;

            editor.vi_msg = NULL;
            if (editor_vi_key(&editor.vi, editor.buffer, editor.content_length,
                              editor.cursor_pos, (unsigned char)key, &act)) {
                int exit_how = editor_vi_apply(&act, cursor_line_before);
                if (exit_how != EDITOR_VI_CONTINUE) {
                    editor_restore_screen(saved_screen_mode, saved_cursor_col, saved_cursor_row);
                    return exit_how == EDITOR_VI_ACCEPT ? LOGO_EDITOR_ACCEPT : LOGO_EDITOR_CANCEL;
                }
                // Insert mode has just begun. Where the cursor landed is the
                // editor's decision -- past `o`'s auto-indent, at what `cw`
                // deleted -- so the state machine is told, and the `Esc` can
                // then see what was typed (vi-mode-design.md §20)
                if (mode_before != VI_INSERT && editor.vi.mode == VI_INSERT) {
                    editor_vi_insert_began(&editor.vi, editor.cursor_pos,
                                           editor.content_length);
                }
                key = 0;
            } else if (editor.vi.mode == VI_INSERT &&
                       editor_vi_key_modifies((unsigned char)key)) {
                editor.vi.modified = true;
            }

            // The footer carries the mode indicator, the command line and any
            // complaint, so it is repainted whenever one of them moves. A
            // composed message is the one case the pointer cannot answer for:
            // it always points at the same buffer in the vi state, so the text
            // can change while the pointer does not
            if (editor.vi_msg != msg_before || act.kind == VI_ACT_MESSAGE ||
                editor.vi.mode != mode_before || editor.vi.mode == VI_CMDLINE) {
                editor_draw_footer();
            }
        }

        // Handle special keys
        switch (key) {
            case KEY_ESC:
                // Accept changes. In vi mode Esc never reaches here: it belongs
                // to the mode, and :w / ZZ accept instead
                editor_restore_screen(saved_screen_mode, saved_cursor_col, saved_cursor_row);
                return LOGO_EDITOR_ACCEPT;

            case KEY_BREAK:
                // Cancel changes - restore original buffer. Brk cancels from
                // every vi mode too, which is what makes the mode safe to be
                // wrong about
                editor_restore_screen(saved_screen_mode, saved_cursor_col, saved_cursor_row);
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
                
            case KEY_WORD_LEFT:
                editor_move_cursor_word_left();
                if (editor.selecting) {
                    // A word move can cross a line - mark the span, as up/down do
                    editor_mark_from_line_dirty(editor_get_line_at_pos(editor.cursor_pos));
                } else {
                    editor.dirty_flags = DIRTY_CURSOR;
                }
                break;

            case KEY_WORD_RIGHT:
                editor_move_cursor_word_right();
                if (editor.selecting) {
                    editor_mark_from_line_dirty(cursor_line_before);
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
                // Page moves change viewport directly - always need full redraw
                editor_mark_all_dirty();
                break;
                
            case KEY_PAGE_DOWN:
                editor_page_down();
                // Page moves change viewport directly - always need full redraw
                editor_mark_all_dirty();
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
                    lcd_set_cursor_style(LCD_CURSOR_UNDERLINE);
                } else {
                    editor.selecting = true;
                    editor.select_anchor = editor.cursor_pos;
                    lcd_set_cursor_style(LCD_CURSOR_BLOCK);
                }
                // Selection visual needs redraw - mark the lines involved
                editor_mark_all_dirty();  // Selection can span multiple lines
                break;
                
            case 0x06:  // Ctrl+F - start incremental search
                editor.searching = true;
                editor.search_text[0] = '\0';
                editor.search_len = 0;
                editor.search_origin = editor.cursor_pos;
                editor_draw_footer();
                break;

            case 0x03:  // Ctrl+C - copy
            case 0x19:  // Ctrl+Y - yank (also copy, Y is for yank)
                if (editor.selecting) {
                    editor_copy_selection();
                    editor.selecting = false;
                    lcd_set_cursor_style(LCD_CURSOR_UNDERLINE);
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
            
            case KEY_CTRL_COMMA:  // Ctrl+, - decrease indent
                if (editor.selecting) {
                    editor_decrease_indent();
                    editor_mark_all_dirty();  // Multiple lines may change
                }
                break;
            
            case KEY_CTRL_PERIOD:  // Ctrl+. - increase indent
                if (editor.selecting) {
                    editor_increase_indent();
                    editor_mark_all_dirty();  // Multiple lines may change
                }
                break;
                
            case KEY_F1:
                // Restore editor from graphics preview
                if (editor.in_graphics_preview) {
                    // Switch to text mode WITHOUT redrawing txt_buffer
                    // Then redraw the editor content directly to LCD
                    screen_set_mode_no_update(SCREEN_MODE_TXT);
                    lcd_clear_screen(PALETTE_SYNTAX_BG);
                    lcd_define_scrolling(EDITOR_SCROLL_TOP, EDITOR_SCROLL_BOTTOM);  // Mode switch reset it
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
                    screen_gfx_mark_all_dirty();  // Force full blit for preview
                    screen_gfx_present();  // System repaint: bypass refresh policy
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
            int scroll_delta = editor_ensure_cursor_visible();

            if (scroll_delta == 1 || scroll_delta == -1) {
                // One line: shift the panel and draw only the line that appeared.
                // Any dirty flags the operation set are still honoured below.
                editor_scroll_one_line(scroll_delta);
                if (h_scroll_before > 0) {
                    // The line we left still shows its scroll arrows
                    editor_mark_line_dirty(cursor_line_before);
                }
            } else if (scroll_delta != 0) {
                // Jumped further than a line (page keys, search) - full redraw
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
        
        // The ruler follows the cursor, so a keystroke that changed the line
        // has to repaint the footer even when the mode indicator did not move
        if (editor.vi_mode && editor.vi.mode == VI_NORMAL &&
            editor_get_line_at_pos(editor.cursor_pos) != cursor_line_before) {
            editor_draw_footer();
        }

        if (needs_cursor_update) {
            editor_position_cursor();
        }
    }
}

// Editor operations structure
static const LogoConsoleEditor picocalc_editor_ops = {
    .edit = picocalc_editor_edit,
    .set_vi_mode = picocalc_editor_set_vi_mode,
    .set_undo_store = picocalc_editor_set_undo_store
};

const LogoConsoleEditor *picocalc_editor_get_ops(void)
{
    return &picocalc_editor_ops;
}
