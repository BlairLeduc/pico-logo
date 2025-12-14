//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Text screen primitives: cleartext, cursor, fullscreen, setcursor,
//                          setwidth, splitscreen, textscreen, width
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "devices/io.h"
#include <stdio.h>
#include <string.h>

//==========================================================================
// Helper functions
//==========================================================================

// Get the console's text operations, or NULL if not available
static const LogoConsoleText *get_text_ops(void)
{
    LogoIO *io = primitives_get_io();
    if (!io || !io->console)
        return NULL;
    return io->console->text;
}

// Get the console's screen operations, or NULL if not available
static const LogoConsoleScreen *get_screen_ops(void)
{
    LogoIO *io = primitives_get_io();
    if (!io || !io->console)
        return NULL;
    return io->console->screen;
}

//==========================================================================
// cleartext (ct) - Clear the text screen
//==========================================================================

static Result prim_cleartext(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    const LogoConsoleText *text = get_text_ops();
    if (text && text->clear)
    {
        text->clear();
    }
    
    return result_none();
}

//==========================================================================
// cursor - Output cursor position as [column row]
//==========================================================================

static Result prim_cursor(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    const LogoConsoleText *text = get_text_ops();
    
    uint8_t column = 0, row = 0;
    if (text && text->get_cursor)
    {
        text->get_cursor(&column, &row);
    }

    // Build list [column row]
    char col_buf[8], row_buf[8];
    snprintf(col_buf, sizeof(col_buf), "%d", column);
    snprintf(row_buf, sizeof(row_buf), "%d", row);
    
    Node col_atom = mem_atom(col_buf, strlen(col_buf));
    Node row_atom = mem_atom(row_buf, strlen(row_buf));
    
    Node list = mem_cons(col_atom, mem_cons(row_atom, NODE_NIL));
    
    return result_ok(value_list(list));
}

//==========================================================================
// fullscreen (fs) - Full-screen graphics mode
//==========================================================================

static Result prim_fullscreen(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    const LogoConsoleScreen *screen = get_screen_ops();
    if (screen && screen->fullscreen)
    {
        screen->fullscreen();
    }
    
    return result_none();
}

//==========================================================================
// setcursor [column row] - Set cursor position
//==========================================================================

static Result prim_setcursor(Evaluator *eval, int argc, Value *args)
{
    (void)eval;

    if (argc < 1)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "setcursor", NULL);
    }

    Value pos = args[0];
    if (pos.type != VALUE_LIST)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setcursor", value_to_string(pos));
    }

    // Extract column and row from list
    Node list = pos.as.node;
    if (mem_is_nil(list))
    {
        return result_error_arg(ERR_TOO_FEW_ITEMS_LIST, "setcursor", NULL);
    }

    Node col_node = mem_car(list);
    list = mem_cdr(list);
    
    if (mem_is_nil(list))
    {
        return result_error_arg(ERR_TOO_FEW_ITEMS_LIST, "setcursor", NULL);
    }
    
    Node row_node = mem_car(list);

    // Convert to numbers
    Value col_val = mem_is_word(col_node) ? value_word(col_node) : value_list(col_node);
    Value row_val = mem_is_word(row_node) ? value_word(row_node) : value_list(row_node);
    
    float col_num, row_num;
    if (!value_to_number(col_val, &col_num) || !value_to_number(row_val, &row_num))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setcursor", value_to_string(pos));
    }

    int column = (int)col_num;
    int row = (int)row_num;

    // Validate range
    if (column < 0 || row < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setcursor", value_to_string(pos));
    }

    const LogoConsoleText *text = get_text_ops();
    if (text && text->set_cursor)
    {
        text->set_cursor((uint8_t)column, (uint8_t)row);
    }
    
    return result_none();
}


//==========================================================================
// splitscreen (ss) - Split screen mode
//==========================================================================

static Result prim_splitscreen(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    const LogoConsoleScreen *screen = get_screen_ops();
    if (screen && screen->splitscreen)
    {
        screen->splitscreen();
    }
    
    return result_none();
}

//==========================================================================
// textscreen (ts) - Full text mode
//==========================================================================

static Result prim_textscreen(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    const LogoConsoleScreen *screen = get_screen_ops();
    if (screen && screen->textscreen)
    {
        screen->textscreen();
    }
    
    return result_none();
}

//==========================================================================
// Registration
//==========================================================================

void primitives_text_init(void)
{
    // Text screen commands
    primitive_register("cleartext", 0, prim_cleartext);
    primitive_register("ct", 0, prim_cleartext);
    
    primitive_register("setcursor", 1, prim_setcursor);
    
    // Screen mode commands
    primitive_register("fullscreen", 0, prim_fullscreen);
    primitive_register("fs", 0, prim_fullscreen);
    
    primitive_register("splitscreen", 0, prim_splitscreen);
    primitive_register("ss", 0, prim_splitscreen);
    
    primitive_register("textscreen", 0, prim_textscreen);
    primitive_register("ts", 0, prim_textscreen);
    
    // Operations (queries)
    primitive_register("cursor", 0, prim_cursor);
}
