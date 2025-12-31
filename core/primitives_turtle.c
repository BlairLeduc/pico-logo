//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Turtle graphics primitives
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "devices/io.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

//==========================================================================
// Helper functions
//==========================================================================

// Get the console's turtle operations, or NULL if not available
static const LogoConsoleTurtle *get_turtle_ops(void)
{
    LogoIO *io = primitives_get_io();
    if (!io || !io->console)
        return NULL;
    return io->console->turtle;
}

// Helper to extract [x y] list into coordinates
static bool extract_position(Value pos, float *x, float *y, const char *proc_name, Result *error)
{
    if (pos.type != VALUE_LIST)
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, proc_name, value_to_string(pos));
        return false;
    }

    Node list = pos.as.node;
    if (mem_is_nil(list))
    {
        *error = result_error_arg(ERR_TOO_FEW_ITEMS_LIST, proc_name, NULL);
        return false;
    }

    Node x_node = mem_car(list);
    list = mem_cdr(list);
    
    if (mem_is_nil(list))
    {
        *error = result_error_arg(ERR_TOO_FEW_ITEMS_LIST, proc_name, NULL);
        return false;
    }
    
    Node y_node = mem_car(list);

    // Convert to numbers
    Value x_val = mem_is_word(x_node) ? value_word(x_node) : value_list(x_node);
    Value y_val = mem_is_word(y_node) ? value_word(y_node) : value_list(y_node);
    
    float x_num, y_num;
    if (!value_to_number(x_val, &x_num) || !value_to_number(y_val, &y_num))
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, proc_name, value_to_string(pos));
        return false;
    }

    *x = x_num;
    *y = y_num;
    return true;
}

// Helper to build [x y] list from coordinates
static Value make_position_list(float x, float y)
{
    char x_buf[32], y_buf[32];
    format_number(x_buf, sizeof(x_buf), x);
    format_number(y_buf, sizeof(y_buf), y);
    
    Node x_atom = mem_atom(x_buf, strlen(x_buf));
    Node y_atom = mem_atom(y_buf, strlen(y_buf));
    
    return value_list(mem_cons(x_atom, mem_cons(y_atom, NODE_NIL)));
}

//==========================================================================
// Movement primitives
//==========================================================================

// back (bk) - Move turtle backward
static Result prim_back(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("back", 1);
    REQUIRE_NUMBER("back", args[0], distance);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->move)
    {
        if (!turtle->move(-distance))  // Negative for backward
        {
            return result_error(ERR_TURTLE_BOUNDS);
        }
    }
    
    return result_none();
}

// forward (fd) - Move turtle forward
static Result prim_forward(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("forward", 1);
    REQUIRE_NUMBER("forward", args[0], distance);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->move)
    {
        if (!turtle->move(distance))
        {
            return result_error(ERR_TURTLE_BOUNDS);
        }
    }
    
    return result_none();
}

// home - Move turtle to center, heading north
static Result prim_home(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->home)
    {
        turtle->home();
    }
    
    return result_none();
}

// setpos [x y] - Move turtle to position
static Result prim_setpos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("setpos", 1);

    float x, y;
    Result error;
    if (!extract_position(args[0], &x, &y, "setpos", &error))
    {
        return error;
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_position)
    {
        turtle->set_position(x, y);
    }
    
    return result_none();
}

// setx - Set turtle x-coordinate
static Result prim_setx(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("setx", 1);
    REQUIRE_NUMBER("setx", args[0], x);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_position && turtle->get_position)
    {
        float curr_x, curr_y;
        turtle->get_position(&curr_x, &curr_y);
        turtle->set_position(x, curr_y);
    }
    
    return result_none();
}

// sety - Set turtle y-coordinate
static Result prim_sety(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("sety", 1);
    REQUIRE_NUMBER("sety", args[0], y);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_position && turtle->get_position)
    {
        float curr_x, curr_y;
        turtle->get_position(&curr_x, &curr_y);
        turtle->set_position(curr_x, y);
    }
    
    return result_none();
}

//==========================================================================
// Rotation primitives
//==========================================================================

// left (lt) - Turn turtle left
static Result prim_left(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("left", 1);
    REQUIRE_NUMBER("left", args[0], degrees);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->get_heading && turtle->set_heading)
    {
        float heading = turtle->get_heading();
        heading -= degrees;  // Left is counterclockwise (negative)
        turtle->set_heading(heading);
    }
    
    return result_none();
}

// right (rt) - Turn turtle right
static Result prim_right(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("right", 1);
    REQUIRE_NUMBER("right", args[0], degrees);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->get_heading && turtle->set_heading)
    {
        float heading = turtle->get_heading();
        heading += degrees;  // Right is clockwise (positive)
        turtle->set_heading(heading);
    }
    
    return result_none();
}

// setheading (seth) - Set absolute heading
static Result prim_setheading(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("setheading", 1);
    REQUIRE_NUMBER("setheading", args[0], degrees);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_heading)
    {
        turtle->set_heading(degrees);
    }
    
    return result_none();
}

//==========================================================================
// Query primitives
//==========================================================================

// heading - Output current heading
static Result prim_heading(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    float heading = 0.0f;
    if (turtle && turtle->get_heading)
    {
        heading = turtle->get_heading();
    }

    return result_ok(value_number(heading));
}

// pos - Output current position as [x y]
static Result prim_pos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    float x = 0.0f, y = 0.0f;
    if (turtle && turtle->get_position)
    {
        turtle->get_position(&x, &y);
    }

    return result_ok(make_position_list(x, y));
}

// xcor - Output x-coordinate
static Result prim_xcor(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    float x = 0.0f, y = 0.0f;
    if (turtle && turtle->get_position)
    {
        turtle->get_position(&x, &y);
    }

    return result_ok(value_number(x));
}

// ycor - Output y-coordinate
static Result prim_ycor(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    float x = 0.0f, y = 0.0f;
    if (turtle && turtle->get_position)
    {
        turtle->get_position(&x, &y);
    }

    return result_ok(value_number(y));
}

// towards [x y] - Output heading towards position
static Result prim_towards(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("towards", 1);

    float target_x, target_y;
    Result error;
    if (!extract_position(args[0], &target_x, &target_y, "towards", &error))
    {
        return error;
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    float x = 0.0f, y = 0.0f;
    if (turtle && turtle->get_position)
    {
        turtle->get_position(&x, &y);
    }

    // Calculate angle from current position to target
    // Logo heading: 0 = north, 90 = east (clockwise from north)
    float dx = target_x - x;
    float dy = target_y - y;
    
    // atan2 gives angle from positive x-axis (counterclockwise)
    // Convert to Logo heading (clockwise from north)
    float angle = atan2f(dx, dy) * (180.0f / 3.14159265358979f);
    
    // Normalize to [0, 360)
    while (angle < 0.0f) angle += 360.0f;
    while (angle >= 360.0f) angle -= 360.0f;

    return result_ok(value_number(angle));
}

//==========================================================================
// Pen control primitives
//==========================================================================

// pendown (pd) - Put pen down
static Result prim_pendown(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_pen_state)
    {
        turtle->set_pen_state(LOGO_PEN_DOWN);
    }
    
    return result_none();
}

// penerase (pe) - Put eraser down
static Result prim_penerase(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    // Note: penerase sets a special mode - the mock device tracks pen_mode
    // The actual implementation would need to set pen down in erase mode
    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_pen_state)
    {
        turtle->set_pen_state(LOGO_PEN_ERASE);  // Pen is down, but in erase mode
        // The device should track erase mode separately
    }
    
    return result_none();
}

// penreverse (px) - Put reversing pen down
static Result prim_penreverse(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_pen_state)
    {
        turtle->set_pen_state(LOGO_PEN_REVERSE);  // Pen is down, but in reverse mode
        // The device should track reverse mode separately
    }
    
    return result_none();
}

// penup (pu) - Lift pen up
static Result prim_penup(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_pen_state)
    {
        turtle->set_pen_state(LOGO_PEN_UP);
    }
    
    return result_none();
}

// pen - Output current pen state
static Result prim_pen(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    // Default to pendown
    const char *state = "pendown";
    if (turtle && turtle->get_pen_state)
    {
        LogoPen pen_state = turtle->get_pen_state();
        switch (pen_state)
        {
            case LOGO_PEN_UP:
                state = "penup";
                break;
            case LOGO_PEN_DOWN:
                state = "pendown";
                break;
            case LOGO_PEN_ERASE:
                state = "penerase";
                break;
            case LOGO_PEN_REVERSE:
                state = "penreverse";
                break;
        }
    }

    return result_ok(value_word(mem_atom(state, strlen(state))));
}

// setpc (setpencolor) - Set pen color
static Result prim_setpc(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("setpc", 1);
    REQUIRE_NUMBER("setpc", args[0], colour);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_pen_colour)
    {
        turtle->set_pen_colour((uint16_t)colour);
    }
    
    return result_none();
}

// pencolor (pc) - Output pen color
static Result prim_pencolor(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    uint16_t colour = 0;
    if (turtle && turtle->get_pen_colour)
    {
        colour = turtle->get_pen_colour();
    }

    return result_ok(value_number((float)colour));
}

// setbg - Set background color
static Result prim_setbg(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("setbg", 1);
    REQUIRE_NUMBER("setbg", args[0], colour);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_bg_colour)
    {
        turtle->set_bg_colour((uint16_t)colour);
    }
    
    return result_none();
}

// background (bg) - Output background color
static Result prim_background(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    uint16_t colour = 0;
    if (turtle && turtle->get_bg_colour)
    {
        colour = turtle->get_bg_colour();
    }

    return result_ok(value_number((float)colour));
}

//==========================================================================
// Visibility primitives
//==========================================================================

// hideturtle (ht) - Hide the turtle
static Result prim_hideturtle(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_visible)
    {
        turtle->set_visible(false);
    }
    
    return result_none();
}

// showturtle (st) - Show the turtle
static Result prim_showturtle(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_visible)
    {
        turtle->set_visible(true);
    }
    
    return result_none();
}

// shown? (shownp) - Output whether turtle is visible
static Result prim_shownp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    bool visible = true;
    if (turtle && turtle->get_visible)
    {
        visible = turtle->get_visible();
    }

    return result_ok(value_word(mem_atom(visible ? "true" : "false", visible ? 4 : 5)));
}

//==========================================================================
// Screen primitives
//==========================================================================

// clearscreen (cs) - Clear screen and reset turtle
static Result prim_clearscreen(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle)
    {
        if (turtle->clear)
        {
            turtle->clear();
        }
        if (turtle->home)
        {
            // Save pen state and set to pen up so home() doesn't draw a line
            // on the freshly cleared screen
            LogoPen saved_pen = LOGO_PEN_DOWN;
            if (turtle->get_pen_state)
            {
                saved_pen = turtle->get_pen_state();
            }
            if (turtle->set_pen_state)
            {
                turtle->set_pen_state(LOGO_PEN_UP);
            }
            
            turtle->home();
            
            // Restore pen state
            if (turtle->set_pen_state)
            {
                turtle->set_pen_state(saved_pen);
            }
        }
    }
    
    return result_none();
}

// clean - Clear graphics without moving turtle
static Result prim_clean(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->clear)
    {
        turtle->clear();
    }
    
    return result_none();
}

//==========================================================================
// Drawing primitives
//==========================================================================

// dot [x y] - Draw a dot at position
static Result prim_dot(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("dot", 1);

    float x, y;
    Result error;
    if (!extract_position(args[0], &x, &y, "dot", &error))
    {
        return error;
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->dot)
    {
        turtle->dot(x, y);
    }
    
    return result_none();
}

// dot? (dotp) - Check if dot at position
static Result prim_dotp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("dot?", 1);

    float x, y;
    Result error;
    if (!extract_position(args[0], &x, &y, "dot?", &error))
    {
        return error;
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    bool has_dot = false;
    if (turtle && turtle->dot_at)
    {
        has_dot = turtle->dot_at(x, y);
    }

    return result_ok(value_word(mem_atom(has_dot ? "true" : "false", has_dot ? 4 : 5)));
}

// fill - Fill enclosed area
static Result prim_fill(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->fill)
    {
        turtle->fill();
    }
    
    return result_none();
}

//==========================================================================
// Boundary mode primitives
//==========================================================================

// fence - Turtle stops at boundary
static Result prim_fence(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_fence)
    {
        turtle->set_fence();
    }
    
    return result_none();
}

// window - Turtle can go off-screen
static Result prim_window(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_window)
    {
        turtle->set_window();
    }
    
    return result_none();
}

// wrap - Turtle wraps around edges
static Result prim_wrap(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_wrap)
    {
        turtle->set_wrap();
    }
    
    return result_none();
}

//==========================================================================
// Palette primitives
//==========================================================================

// Helper to extract [r g b] list into RGB components
static bool extract_rgb(Value rgb, uint8_t *r, uint8_t *g, uint8_t *b, const char *proc_name, Result *error)
{
    if (rgb.type != VALUE_LIST)
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, proc_name, value_to_string(rgb));
        return false;
    }

    Node list = rgb.as.node;
    if (mem_is_nil(list))
    {
        *error = result_error_arg(ERR_TOO_FEW_ITEMS_LIST, proc_name, NULL);
        return false;
    }

    Node r_node = mem_car(list);
    list = mem_cdr(list);
    
    if (mem_is_nil(list))
    {
        *error = result_error_arg(ERR_TOO_FEW_ITEMS_LIST, proc_name, NULL);
        return false;
    }
    
    Node g_node = mem_car(list);
    list = mem_cdr(list);
    
    if (mem_is_nil(list))
    {
        *error = result_error_arg(ERR_TOO_FEW_ITEMS_LIST, proc_name, NULL);
        return false;
    }
    
    Node b_node = mem_car(list);

    // Convert to numbers
    Value r_val = mem_is_word(r_node) ? value_word(r_node) : value_list(r_node);
    Value g_val = mem_is_word(g_node) ? value_word(g_node) : value_list(g_node);
    Value b_val = mem_is_word(b_node) ? value_word(b_node) : value_list(b_node);
    
    float r_num, g_num, b_num;
    if (!value_to_number(r_val, &r_num) || 
        !value_to_number(g_val, &g_num) || 
        !value_to_number(b_val, &b_num))
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, proc_name, value_to_string(rgb));
        return false;
    }

    // Clamp to 0-255 range
    *r = (uint8_t)(r_num < 0 ? 0 : (r_num > 255 ? 255 : r_num));
    *g = (uint8_t)(g_num < 0 ? 0 : (g_num > 255 ? 255 : g_num));
    *b = (uint8_t)(b_num < 0 ? 0 : (b_num > 255 ? 255 : b_num));
    
    return true;
}

// Helper to build [r g b] list from RGB components
static Value make_rgb_list(uint8_t r, uint8_t g, uint8_t b)
{
    char r_buf[8], g_buf[8], b_buf[8];
    snprintf(r_buf, sizeof(r_buf), "%d", r);
    snprintf(g_buf, sizeof(g_buf), "%d", g);
    snprintf(b_buf, sizeof(b_buf), "%d", b);
    
    Node r_atom = mem_atom(r_buf, strlen(r_buf));
    Node g_atom = mem_atom(g_buf, strlen(g_buf));
    Node b_atom = mem_atom(b_buf, strlen(b_buf));
    
    return value_list(mem_cons(r_atom, mem_cons(g_atom, mem_cons(b_atom, NODE_NIL))));
}

// setpalette slot [r g b] - Set palette slot to RGB color
static Result prim_setpalette(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("setpalette", 2);
    REQUIRE_NUMBER("setpalette", args[0], slot_num);

    if (slot_num < 0 || slot_num > 255)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setpalette", value_to_string(args[0]));
    }

    uint8_t slot = (uint8_t)slot_num;
    uint8_t r, g, b;
    Result error;
    
    if (!extract_rgb(args[1], &r, &g, &b, "setpalette", &error))
    {
        return error;
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_palette)
    {
        turtle->set_palette(slot, r, g, b);
    }
    
    return result_none();
}

// palette slot - Output RGB list for palette slot
static Result prim_palette(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("palette", 1);
    REQUIRE_NUMBER("palette", args[0], slot_num);

    if (slot_num < 0 || slot_num > 255)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "palette", value_to_string(args[0]));
    }

    uint8_t slot = (uint8_t)slot_num;
    uint8_t r = 0, g = 0, b = 0;

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->get_palette)
    {
        turtle->get_palette(slot, &r, &g, &b);
    }
    
    return result_ok(make_rgb_list(r, g, b));
}

// restorepalette - Restore default palette (slots 0-127)
static Result prim_restorepalette(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->restore_palette)
    {
        turtle->restore_palette();
    }
    
    return result_none();
}

//==========================================================================
// Shape primitives
//==========================================================================

// getsh shapenumber - Output list of 16 numbers representing shape (1-15)
static Result prim_getsh(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("getsh", 1);
    REQUIRE_NUMBER("getsh", args[0], shape_num);

    // Shape must be 1-15 (shape 0 is the line-drawn turtle)
    if (shape_num < 1 || shape_num > 15)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "getsh", value_to_string(args[0]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (!turtle || !turtle->get_shape_data)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "getsh", value_to_string(args[0]));
    }

    uint8_t shape_data[16];
    if (!turtle->get_shape_data((uint8_t)shape_num, shape_data))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "getsh", value_to_string(args[0]));
    }

    // Build a list of 16 numbers
    Node list = NODE_NIL;
    for (int i = 15; i >= 0; i--)
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", shape_data[i]);
        Node atom = mem_atom(buf, strlen(buf));
        list = mem_cons(atom, list);
    }

    return result_ok(value_list(list));
}

// putsh shapenumber shapespec - Set shape data for shapes 1-15
static Result prim_putsh(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("putsh", 2);
    REQUIRE_NUMBER("putsh", args[0], shape_num);

    // Shape must be 1-15 (shape 0 cannot be changed)
    if (shape_num < 1 || shape_num > 15)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "putsh", value_to_string(args[0]));
    }

    if (args[1].type != VALUE_LIST)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "putsh", value_to_string(args[1]));
    }

    // Extract 16 numbers from the list
    uint8_t shape_data[16];
    Node list = args[1].as.node;
    int count = 0;

    while (!mem_is_nil(list) && count < 16)
    {
        Node item = mem_car(list);
        Value item_val = mem_is_word(item) ? value_word(item) : value_list(item);
        
        float num;
        if (!value_to_number(item_val, &num))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, "putsh", value_to_string(args[1]));
        }

        if (num < 0 || num > 255)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, "putsh", value_to_string(args[1]));
        }

        shape_data[count] = (uint8_t)num;
        count++;
        list = mem_cdr(list);
    }

    if (count != 16 || !mem_is_nil(list))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "putsh", value_to_string(args[1]));
    }
    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (!turtle || !turtle->put_shape_data)
    {
        return result_none();
    }

    turtle->put_shape_data((uint8_t)shape_num, shape_data);
    return result_none();
}

// setsh shapenumber - Set the current turtle shape (0-15)
static Result prim_setsh(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC("setsh", 1);
    REQUIRE_NUMBER("setsh", args[0], shape_num);

    if (shape_num < 0 || shape_num > 15)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setsh", value_to_string(args[0]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_shape)
    {
        turtle->set_shape((uint8_t)shape_num);
    }

    return result_none();
}

// shape - Output the current turtle shape number
static Result prim_shape(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (!turtle || !turtle->get_shape)
    {
        return result_ok(value_number(0));
    }

    uint8_t shape = turtle->get_shape();
    return result_ok(value_number(shape));
}

//==========================================================================
// Registration
//==========================================================================

void primitives_turtle_init(void)
{
    // Movement primitives
    primitive_register("back", 1, prim_back);
    primitive_register("bk", 1, prim_back);
    primitive_register("forward", 1, prim_forward);
    primitive_register("fd", 1, prim_forward);
    primitive_register("home", 0, prim_home);
    primitive_register("setpos", 1, prim_setpos);
    primitive_register("setx", 1, prim_setx);
    primitive_register("sety", 1, prim_sety);
    
    // Rotation primitives
    primitive_register("left", 1, prim_left);
    primitive_register("lt", 1, prim_left);
    primitive_register("right", 1, prim_right);
    primitive_register("rt", 1, prim_right);
    primitive_register("setheading", 1, prim_setheading);
    primitive_register("seth", 1, prim_setheading);
    
    // Query primitives
    primitive_register("heading", 0, prim_heading);
    primitive_register("pos", 0, prim_pos);
    primitive_register("xcor", 0, prim_xcor);
    primitive_register("ycor", 0, prim_ycor);
    primitive_register("towards", 1, prim_towards);
    
    // Pen control primitives
    primitive_register("pendown", 0, prim_pendown);
    primitive_register("pd", 0, prim_pendown);
    primitive_register("penerase", 0, prim_penerase);
    primitive_register("pe", 0, prim_penerase);
    primitive_register("penreverse", 0, prim_penreverse);
    primitive_register("px", 0, prim_penreverse);
    primitive_register("penup", 0, prim_penup);
    primitive_register("pu", 0, prim_penup);
    primitive_register("pen", 0, prim_pen);
    primitive_register("setpc", 1, prim_setpc);
    primitive_register("setpencolor", 1, prim_setpc);
    primitive_register("pencolor", 0, prim_pencolor);
    primitive_register("pc", 0, prim_pencolor);
    primitive_register("setbg", 1, prim_setbg);
    primitive_register("background", 0, prim_background);
    primitive_register("bg", 0, prim_background);
    
    // Visibility primitives
    primitive_register("hideturtle", 0, prim_hideturtle);
    primitive_register("ht", 0, prim_hideturtle);
    primitive_register("showturtle", 0, prim_showturtle);
    primitive_register("st", 0, prim_showturtle);
    primitive_register("shown?", 0, prim_shownp);
    primitive_register("shownp", 0, prim_shownp);
    
    // Screen primitives
    primitive_register("clearscreen", 0, prim_clearscreen);
    primitive_register("cs", 0, prim_clearscreen);
    primitive_register("clean", 0, prim_clean);
    
    // Drawing primitives
    primitive_register("dot", 1, prim_dot);
    primitive_register("dot?", 1, prim_dotp);
    primitive_register("dotp", 1, prim_dotp);
    primitive_register("fill", 0, prim_fill);
    
    // Boundary mode primitives
    primitive_register("fence", 0, prim_fence);
    primitive_register("window", 0, prim_window);
    primitive_register("wrap", 0, prim_wrap);
    
    // Palette primitives
    primitive_register("setpalette", 2, prim_setpalette);
    primitive_register("palette", 1, prim_palette);
    primitive_register("restorepalette", 0, prim_restorepalette);

    // Shape primitives
    primitive_register("getsh", 1, prim_getsh);
    primitive_register("putsh", 2, prim_putsh);
    primitive_register("setsh", 1, prim_setsh);
    primitive_register("shape", 0, prim_shape);
}
