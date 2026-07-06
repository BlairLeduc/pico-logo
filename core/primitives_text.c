//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Text screen primitives: cleartext, cursor, fullscreen, setcursor,
//                          setwidth, splitscreen, textscreen, width
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "frame_sync.h"
#include "devices/io.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>  // for strcasecmp

// Default frame rate for setrefresh "sync when no rate is given.
#define SYNC_DEFAULT_HZ 30.0f

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
    UNUSED(eval); UNUSED(argc); UNUSED(args);

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
    UNUSED(eval); UNUSED(argc); UNUSED(args);

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
    UNUSED(eval); UNUSED(argc); UNUSED(args);

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
    UNUSED(eval);
    REQUIRE_ARGC(1);

    // Extract column and row from [col row] list
    float col_num, row_num;
    Result error;
    if (!value_extract_xy(args[0], &col_num, &row_num, &error))
    {
        return error;
    }

    int column = (int)col_num;
    int row = (int)row_num;

    // Validate range. Reference defines the screen as 40 columns × 32 rows
    // (columns 0..39, rows 0..31). Reject out-of-range coordinates rather
    // than silently truncating to uint8_t at the device boundary.
    if (column < 0 || column >= 40 || row < 0 || row >= 32)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
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
    UNUSED(eval); UNUSED(argc); UNUSED(args);

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
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleScreen *screen = get_screen_ops();
    if (screen && screen->textscreen)
    {
        screen->textscreen();
    }
    
    return result_none();
}

//==========================================================================
// setrefresh "auto | "manual | "sync [rate] - Select the display refresh policy
//==========================================================================

static Result prim_setrefresh(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    const char *mode = value_to_string(args[0]);
    bool auto_mode;
    bool sync_on = false;
    uint32_t sync_period = 0;

    if (strcasecmp(mode, "auto") == 0)
    {
        auto_mode = true;
    }
    else if (strcasecmp(mode, "manual") == 0)
    {
        auto_mode = false;
    }
    else if (strcasecmp(mode, "sync") == 0)
    {
        // sync accumulates drawing like manual, but the `sync` primitive also
        // paces the loop to a fixed rate (steps per second); (setrefresh
        // "sync 25) overrides the default.
        auto_mode = false;
        sync_on = true;
        float hz = SYNC_DEFAULT_HZ;
        if (argc >= 2)
        {
            if (!value_to_number(args[1], &hz) || hz <= 0.0f)
            {
                return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL,
                                        value_to_string(args[1]));
            }
        }
        sync_period = (uint32_t)(1000.0f / hz + 0.5f);
        if (sync_period < 1)
        {
            sync_period = 1;
        }
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, mode);
    }

    const LogoConsoleScreen *screen = get_screen_ops();
    if (screen && screen->set_refresh_auto)
    {
        screen->set_refresh_auto(auto_mode);
    }
    frame_sync_set(sync_on, sync_period);

    return result_none();
}

//==========================================================================
// refresh - Present pending drawing to the display now
//==========================================================================

static Result prim_refresh(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleScreen *screen = get_screen_ops();
    if (screen && screen->refresh_now)
    {
        screen->refresh_now();
    }

    return result_none();
}

//==========================================================================
// sync - Present the frame, then block until the next frame boundary
//==========================================================================

static Result prim_sync(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    // Present this frame, exactly like refresh.
    const LogoConsoleScreen *screen = get_screen_ops();
    if (screen && screen->refresh_now)
    {
        screen->refresh_now();
    }

    // Pace to the frame boundary only in sync mode with a clock to pace against;
    // otherwise sync degrades to a plain present.
    if (!frame_sync_active())
    {
        return result_none();
    }
    LogoIO *io = primitives_get_io();
    if (!io || !logo_io_has_ticks_ms(io))
    {
        return result_none();
    }

    // Sleep the computed remainder in interruptible chunks, like wait, so a
    // game loop can still be stopped with the interrupt key.
    int ms = (int)frame_sync_wait_ms(logo_io_ticks_ms(io));
    while (ms > 0)
    {
        if (logo_io_check_user_interrupt(io))
        {
            return result_error(ERR_STOPPED);
        }
        int chunk = ms < 100 ? ms : 100;
        logo_io_sleep(io, chunk);
        ms -= chunk;
    }

    return result_none();
}

//==========================================================================
// refreshmode - Output the current refresh policy (auto, manual or sync)
//==========================================================================

static Result prim_refreshmode(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const char *mode;
    if (frame_sync_active())
    {
        mode = "sync";
    }
    else
    {
        const LogoConsoleScreen *screen = get_screen_ops();
        bool auto_mode = true; // Devices without a screen are always "auto"
        if (screen && screen->get_refresh_auto)
        {
            auto_mode = screen->get_refresh_auto();
        }
        mode = auto_mode ? "auto" : "manual";
    }

    Node atom = mem_atom(mode, strlen(mode));
    return result_ok(value_word(atom));
}

//==========================================================================
// settextcolor (settc) [fg bg] - Set text foreground and background colors
//==========================================================================

static Result prim_settextcolor(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    // Extract fg and bg from [fg bg] list
    float fg_num, bg_num;
    Result error;
    if (!value_extract_xy(args[0], &fg_num, &bg_num, &error))
    {
        return error;
    }

    int fg = (int)fg_num;
    int bg = (int)bg_num;

    // Validate range: 0-15 (text palette slots)
    if (fg < 0 || fg > 15 || bg < 0 || bg > 15)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const LogoConsoleText *text = get_text_ops();
    if (text)
    {
        if (text->set_foreground) text->set_foreground((uint8_t)fg);
        if (text->set_background) text->set_background((uint8_t)bg);
    }

    return result_none();
}

//==========================================================================
// textcolor (tc) - Report current text foreground and background colors
//==========================================================================

static Result prim_textcolor(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleText *text = get_text_ops();

    uint8_t fg = 0, bg = 0;
    if (text)
    {
        if (text->get_foreground) fg = text->get_foreground();
        if (text->get_background) bg = text->get_background();
    }

    // Build list [fg bg]
    char fg_buf[8], bg_buf[8];
    snprintf(fg_buf, sizeof(fg_buf), "%d", fg);
    snprintf(bg_buf, sizeof(bg_buf), "%d", bg);

    Node fg_atom = mem_atom(fg_buf, strlen(fg_buf));
    Node bg_atom = mem_atom(bg_buf, strlen(bg_buf));

    Node list = mem_cons(fg_atom, mem_cons(bg_atom, NODE_NIL));

    return result_ok(value_list(list));
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
    
    primitive_register("settextcolor", 1, prim_settextcolor);
    primitive_register("settc", 1, prim_settextcolor);
    
    // Screen mode commands
    primitive_register("fullscreen", 0, prim_fullscreen);
    primitive_register("fs", 0, prim_fullscreen);
    
    primitive_register("splitscreen", 0, prim_splitscreen);
    primitive_register("ss", 0, prim_splitscreen);
    
    primitive_register("textscreen", 0, prim_textscreen);
    primitive_register("ts", 0, prim_textscreen);

    // Refresh policy commands
    primitive_register("setrefresh", 1, prim_setrefresh);
    primitive_register("refresh", 0, prim_refresh);
    primitive_register("sync", 0, prim_sync);

    // Operations (queries)
    primitive_register("cursor", 0, prim_cursor);

    primitive_register("refreshmode", 0, prim_refreshmode);

    primitive_register("textcolor", 0, prim_textcolor);
    primitive_register("tc", 0, prim_textcolor);
}
