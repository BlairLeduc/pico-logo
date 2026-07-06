//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Turtle graphics primitives
//

#include "primitives.h"
#include "format.h"
#include "error.h"
#include "eval.h"
#include "demons.h"
#include "frame_sync.h"
#include "limits.h"
#include "devices/io.h"
#include "devices/palette.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

// Normalize a heading in degrees to the half-open range [0, 360).
// Used so that HEADING and SETHEADING expose canonical angles regardless
// of how many full turns the underlying device accumulates.
static float normalize_heading(float h)
{
    h = fmodf(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    return h;
}

//==========================================================================
// Core palette state
//
// The interpreter maintains its own authoritative 24-bit RGB palette so that
// PALETTE always returns the exact values set by SETPALETTE, regardless of
// any precision loss in the device layer (e.g. RGB565 on PicoCalc).
//==========================================================================

static struct { uint8_t r, g, b; } core_palette[256];

static void core_palette_init(void)
{
    for (int i = 0; i < 256; i++)
    {
        uint32_t rgb = palette_24bit[i];
        core_palette[i].r = (rgb >> 16) & 0xFF;
        core_palette[i].g = (rgb >> 8)  & 0xFF;
        core_palette[i].b =  rgb        & 0xFF;
    }
}

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

//==========================================================================
// Active turtle set
//
// `tell` selects which turtles subsequent turtle commands address. The
// set is kept sorted ascending with no duplicates. Commands fan out over
// the set in ascending order; queries answer for the lowest member.
// Every fan-out loop leaves the device selected on the lowest active
// turtle, so query primitives read the right turtle without changes.
// Devices without a `select` op (host) always address their single
// turtle regardless of the set.
//==========================================================================

static uint8_t active_set[MAX_TURTLES] = {0};
static int active_count = 1;

// Route subsequent stateful turtle ops to turtle n
static void select_turtle(const LogoConsoleTurtle *turtle, uint8_t n)
{
    if (turtle->select)
    {
        turtle->select(n);
    }
}

// Restore the invariant: device selected on the lowest active turtle
static void select_first_active(const LogoConsoleTurtle *turtle)
{
    select_turtle(turtle, active_set[0]);
}

// Reset the active set to turtle 0 (boot state; also applied by cs)
static void reset_active_set(void)
{
    active_set[0] = 0;
    active_count = 1;
}

// Parse a turtle number or list of numbers into a sorted, deduplicated
// set. Returns false with *error set on any non-integer, out-of-range,
// or empty input.
static bool parse_turtle_set(Value v, uint8_t *set, int *count, Result *error)
{
    *count = 0;

    Node list = NODE_NIL;
    bool single = (v.type != VALUE_LIST);
    if (!single)
    {
        list = v.as.node;
    }

    while (true)
    {
        Value item;
        if (single)
        {
            item = v;
        }
        else
        {
            if (mem_is_nil(list))
            {
                break;
            }
            Node node = mem_car(list);
            item = mem_is_word(node) ? value_word(node) : value_list(node);
            list = mem_cdr(list);
        }

        float num;
        if (!value_to_number(item, &num) ||
            num != (float)(int)num || num < 0 || num >= MAX_TURTLES)
        {
            *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
            return false;
        }

        // Insert in ascending order, collapsing duplicates
        int n = (int)num;
        int pos = 0;
        while (pos < *count && set[pos] < n)
        {
            pos++;
        }
        if (pos == *count || set[pos] != n)
        {
            for (int j = *count; j > pos; j--)
            {
                set[j] = set[j - 1];
            }
            set[pos] = (uint8_t)n;
            (*count)++;
        }

        if (single)
        {
            break;
        }
    }

    if (*count == 0)
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
        return false;
    }

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
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], distance);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->move)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            if (!turtle->move(-distance))  // Negative for backward
            {
                select_first_active(turtle);
                return result_error(ERR_TURTLE_BOUNDS);
            }
        }
        select_first_active(turtle);
    }

    return result_none();
}

// forward (fd) - Move turtle forward
static Result prim_forward(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], distance);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->move)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            if (!turtle->move(distance))
            {
                select_first_active(turtle);
                return result_error(ERR_TURTLE_BOUNDS);
            }
        }
        select_first_active(turtle);
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
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->home();
        }
        select_first_active(turtle);
    }

    return result_none();
}

// setpos [x y] - Move turtle to position
static Result prim_setpos(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    float x, y;
    Result error;
    if (!value_extract_xy(args[0], &x, &y, &error))
    {
        return error;
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_position)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            if (!turtle->set_position(x, y))
            {
                select_first_active(turtle);
                return result_error(ERR_TURTLE_BOUNDS);
            }
        }
        select_first_active(turtle);
    }

    return result_none();
}

// setx - Set turtle x-coordinate
static Result prim_setx(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], x);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_position && turtle->get_position)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            float curr_x, curr_y;
            turtle->get_position(&curr_x, &curr_y);
            if (!turtle->set_position(x, curr_y))
            {
                select_first_active(turtle);
                return result_error(ERR_TURTLE_BOUNDS);
            }
        }
        select_first_active(turtle);
    }

    return result_none();
}

// sety - Set turtle y-coordinate
static Result prim_sety(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], y);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_position && turtle->get_position)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            float curr_x, curr_y;
            turtle->get_position(&curr_x, &curr_y);
            if (!turtle->set_position(curr_x, y))
            {
                select_first_active(turtle);
                return result_error(ERR_TURTLE_BOUNDS);
            }
        }
        select_first_active(turtle);
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
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], degrees);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->get_heading && turtle->set_heading)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            float heading = turtle->get_heading();
            heading -= degrees;  // Left is counterclockwise (negative)
            turtle->set_heading(heading);
        }
        select_first_active(turtle);
    }

    return result_none();
}

// right (rt) - Turn turtle right
static Result prim_right(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], degrees);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->get_heading && turtle->set_heading)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            float heading = turtle->get_heading();
            heading += degrees;  // Right is clockwise (positive)
            turtle->set_heading(heading);
        }
        select_first_active(turtle);
    }

    return result_none();
}

// setheading (seth) - Set absolute heading
static Result prim_setheading(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], degrees);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_heading)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_heading(normalize_heading(degrees));
        }
        select_first_active(turtle);
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

    return result_ok(value_number(normalize_heading(heading)));
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
    REQUIRE_ARGC(1);

    float target_x, target_y;
    Result error;
    if (!value_extract_xy(args[0], &target_x, &target_y, &error))
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

    // Normalize to [0, 360). atan2f always returns a value in [-pi, pi]
    // so a single conditional add is sufficient (no loops needed).
    return result_ok(value_number(normalize_heading(angle)));
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
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_pen_state(LOGO_PEN_DOWN);
        }
        select_first_active(turtle);
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
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_pen_state(LOGO_PEN_ERASE);  // Pen is down, but in erase mode
        }
        select_first_active(turtle);
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
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_pen_state(LOGO_PEN_REVERSE);  // Pen is down, but in reverse mode
        }
        select_first_active(turtle);
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
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_pen_state(LOGO_PEN_UP);
        }
        select_first_active(turtle);
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
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], colour);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_pen_colour)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_pen_colour((uint16_t)colour);
        }
        select_first_active(turtle);
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
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], colour);

    if (colour < 0 || colour > 254)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

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
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_visible(false);
        }
        select_first_active(turtle);
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
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_visible(true);
        }
        select_first_active(turtle);
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

    return result_ok(value_bool(visible));
}

//==========================================================================
// Screen primitives
//==========================================================================

// clearscreen (cs) - Clear screen and reset turtle
static Result prim_clearscreen(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    // cs restores the single-turtle world: only turtle 0 is addressed
    // afterwards (the device's clear op re-hides turtles 1-7 at home).
    reset_active_set();

    // A full reset also disarms every `when` demon and stops autonomous
    // turtle motion/animation: nothing acts on its own after a reset.
    demons_reset();

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle)
    {
        select_first_active(turtle);
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

        // Re-hide turtles 1-7 at home so cs restores the boot state;
        // multi-turtle stays strictly opt-in afterwards.
        if (turtle->select)
        {
            for (int i = 1; i < MAX_TURTLES; i++)
            {
                turtle->select((uint8_t)i);
                if (turtle->set_pen_state)
                {
                    turtle->set_pen_state(LOGO_PEN_UP);
                }
                if (turtle->home)
                {
                    turtle->home();
                }
                if (turtle->set_pen_state)
                {
                    turtle->set_pen_state(LOGO_PEN_DOWN);
                }
                if (turtle->set_visible)
                {
                    turtle->set_visible(false);
                }
            }
            select_first_active(turtle);
        }
    }

    // A full reset also restores the automatic refresh policy, so a program
    // that left the screen in manual or sync mode cannot leave it stale (or
    // the prompt paced) after cs.
    LogoIO *io = primitives_get_io();
    if (io && io->console && io->console->screen &&
        io->console->screen->set_refresh_auto)
    {
        io->console->screen->set_refresh_auto(true);
    }
    frame_sync_reset();

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
    REQUIRE_ARGC(1);

    float x, y;
    Result error;
    if (!value_extract_xy(args[0], &x, &y, &error))
    {
        return error;
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->dot)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->dot(x, y);
        }
        select_first_active(turtle);
    }

    return result_none();
}

// dot? (dotp) - Check if dot at position
static Result prim_dotp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    float x, y;
    Result error;
    if (!value_extract_xy(args[0], &x, &y, &error))
    {
        return error;
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    
    bool has_dot = false;
    if (turtle && turtle->dot_at)
    {
        has_dot = turtle->dot_at(x, y);
    }

    return result_ok(value_bool(has_dot));
}

// fill - Fill enclosed area
static Result prim_fill(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->fill)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->fill();
        }
        select_first_active(turtle);
    }

    return result_none();
}

// arc - Draw an arc centred on the turtle; the turtle does not move
static Result prim_arc(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_NUMBER(args[0], angle);
    REQUIRE_NUMBER(args[1], radius);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (!turtle || !turtle->get_position || !turtle->set_position ||
        !turtle->get_heading || !turtle->get_pen_state || !turtle->set_pen_state)
    {
        return result_none();
    }

    // Clamp the sweep to one full circle; the sign gives the direction
    // (positive = clockwise from the turtle's heading).
    float sweep = angle;
    if (sweep > 360.0f) sweep = 360.0f;
    if (sweep < -360.0f) sweep = -360.0f;
    if (sweep == 0.0f)
    {
        return result_none();
    }

    // One segment per ~4 degrees keeps the chord error under a pixel
    // for any radius that fits on screen.
    int segments = (int)ceilf(fabsf(sweep) / 4.0f);
    const float deg_to_rad = 3.14159265358979323846f / 180.0f;

    for (int t = 0; t < active_count; t++)
    {
        select_turtle(turtle, active_set[t]);

        float cx, cy;
        turtle->get_position(&cx, &cy);
        float heading = turtle->get_heading();
        LogoPen pen = turtle->get_pen_state();

        // Jump (pen up) to the arc's start point, then sweep with the pen
        // restored so colour/erase/reverse modes and wrap/fence behave exactly
        // like setpos. In fence mode a segment past the boundary fails; the
        // turtle is restored before the error is reported.
        float rad = heading * deg_to_rad;
        turtle->set_pen_state(LOGO_PEN_UP);
        bool ok = turtle->set_position(cx + radius * sinf(rad), cy + radius * cosf(rad));
        if (ok)
        {
            turtle->set_pen_state(pen);
            for (int i = 1; ok && i <= segments; i++)
            {
                rad = (heading + sweep * (float)i / (float)segments) * deg_to_rad;
                ok = turtle->set_position(cx + radius * sinf(rad), cy + radius * cosf(rad));
            }
        }

        // Return to the centre (always in bounds) and restore the pen.
        turtle->set_pen_state(LOGO_PEN_UP);
        turtle->set_position(cx, cy);
        turtle->set_pen_state(pen);

        if (!ok)
        {
            select_first_active(turtle);
            return result_error(ERR_TURTLE_BOUNDS);
        }
    }
    select_first_active(turtle);

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
    REQUIRE_ARGC(2);
    REQUIRE_NUMBER(args[0], slot_num);

    if (slot_num < 0 || slot_num > 255)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    uint8_t slot = (uint8_t)slot_num;
    uint8_t r, g, b;
    Result error;
    
    if (!value_extract_rgb(args[1], &r, &g, &b, &error))
    {
        return error;
    }

    // Update the core 24-bit palette (authoritative source for PALETTE)
    core_palette[slot].r = r;
    core_palette[slot].g = g;
    core_palette[slot].b = b;

    // Forward to device for display update
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
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], slot_num);

    if (slot_num < 0 || slot_num > 255)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    uint8_t slot = (uint8_t)slot_num;

    // Read from core palette (exact 24-bit values, no device round-trip)
    return result_ok(make_rgb_list(
        core_palette[slot].r, core_palette[slot].g, core_palette[slot].b));
}

// restorepalette - Restore default palette (slots 0-254)
static Result prim_restorepalette(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    // Restore core palette slots 0-254 from defaults (slot 255 is user BG)
    for (int i = 0; i < 255; i++)
    {
        uint32_t rgb = palette_24bit[i];
        core_palette[i].r = (rgb >> 16) & 0xFF;
        core_palette[i].g = (rgb >> 8)  & 0xFF;
        core_palette[i].b =  rgb        & 0xFF;
    }

    // Forward to device
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
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], shape_num);

    // Shape must be 1-15 (shape 0 is the line-drawn turtle)
    if (shape_num < 1 || shape_num > 15)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (!turtle || !turtle->get_shape_data)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    uint8_t shape_data[16];
    if (!turtle->get_shape_data((uint8_t)shape_num, shape_data))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
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
    REQUIRE_ARGC(2);
    REQUIRE_NUMBER(args[0], shape_num);

    // Shape must be 1-15 (shape 0 cannot be changed)
    if (shape_num < 1 || shape_num > 15)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    if (args[1].type != VALUE_LIST)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
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
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
        }

        if (num < 0 || num > 255)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
        }

        shape_data[count] = (uint8_t)num;
        count++;
        list = mem_cdr(list);
    }

    if (count != 16 || !mem_is_nil(list))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
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
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], shape_num);

    if (shape_num < 0 || shape_num > 15)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_shape)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_shape((uint8_t)shape_num);
        }
        select_first_active(turtle);
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

// setrot "full | "flip | "fixed - How the costume follows the heading
static Result prim_setrot(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    const char *style_name = value_to_string(args[0]);
    LogoRotationStyle style;
    if (strcasecmp(style_name, "fixed") == 0)
    {
        style = LOGO_ROT_FIXED;
    }
    else if (strcasecmp(style_name, "full") == 0)
    {
        style = LOGO_ROT_FULL;
    }
    else if (strcasecmp(style_name, "flip") == 0)
    {
        style = LOGO_ROT_FLIP;
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, style_name);
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_rotation_style)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_rotation_style(style);
        }
        select_first_active(turtle);
    }

    return result_none();
}

// setmag 1 | 2 - Magnification of the rendered costume
static Result prim_setmag(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], mag);

    if (mag != 1.0f && mag != 2.0f)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_scale)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_scale((uint8_t)mag);
        }
        select_first_active(turtle);
    }

    return result_none();
}

// stamp - Composite the turtle's costume into the canvas at its position
static Result prim_stamp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->stamp)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->stamp();
        }
        select_first_active(turtle);
    }

    return result_none();
}

// snapsh shapenumber width height - Capture the canvas region centred on
// the (first active) turtle into a colour costume
static Result prim_snapsh(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(3);
    REQUIRE_NUMBER(args[0], shape_num);
    REQUIRE_NUMBER(args[1], width);
    REQUIRE_NUMBER(args[2], height);

    if (shape_num < 1 || shape_num > 15 || shape_num != (float)(int)shape_num)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    if (width < 8 || width > 32 || width != (float)(int)width)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    if (height < 8 || height > 32 || height != (float)(int)height)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[2]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->snap_costume)
    {
        select_first_active(turtle);
        if (!turtle->snap_costume((uint8_t)shape_num, (uint8_t)width, (uint8_t)height))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
    }

    return result_none();
}

//==========================================================================
// Autonomous motion and animation
//
// The device owns the moving state and advances it on turtle_tick (driven
// from the demon poll). These primitives just set it and read it back,
// fanning out over the active set like the other turtle setters.
//==========================================================================

// setspeed n - Set the autonomous speed (turtle steps per second; 0 stops)
static Result prim_setspeed(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], speed);

    if (speed < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_speed)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_speed(speed);
        }
        select_first_active(turtle);
    }

    return result_none();
}

// speed - Output the (first active) turtle's autonomous speed
static Result prim_speed(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();

    float speed = 0.0f;
    if (turtle && turtle->get_speed)
    {
        select_first_active(turtle);
        speed = turtle->get_speed();
    }

    return result_ok(value_number(speed));
}

// setanim first last interval - Cycle the costume from first to last,
// advancing one frame every interval milliseconds (interval 0 stops)
static Result prim_setanim(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(3);
    REQUIRE_NUMBER(args[0], first);
    REQUIRE_NUMBER(args[1], last);
    REQUIRE_NUMBER(args[2], interval);

    if (first < 0 || first > 15 || first != (float)(int)first)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    if (last < 0 || last > 15 || last != (float)(int)last)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    if (interval < 0 || interval > 65535 || interval != (float)(int)interval)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[2]));
    }
    if (first > last)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle && turtle->set_anim)
    {
        for (int i = 0; i < active_count; i++)
        {
            select_turtle(turtle, active_set[i]);
            turtle->set_anim((uint8_t)first, (uint8_t)last, (uint16_t)interval);
        }
        select_first_active(turtle);
    }

    return result_none();
}

//==========================================================================
// Multi-turtle addressing
//==========================================================================

// tell n | [n1 n2 ...] - Set the active turtle set
static Result prim_tell(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    uint8_t set[MAX_TURTLES];
    int count;
    Result error;
    if (!parse_turtle_set(args[0], set, &count, &error))
    {
        return error;
    }

    memcpy(active_set, set, (size_t)count);
    active_count = count;

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle)
    {
        select_first_active(turtle);
    }

    return result_none();
}

// who - Output the active turtle set: a number when one turtle is
// active, a list otherwise (so inside each, who is the current turtle)
static Result prim_who(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    if (active_count == 1)
    {
        return result_ok(value_number((float)active_set[0]));
    }

    Node list = NODE_NIL;
    for (int i = active_count - 1; i >= 0; i--)
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", active_set[i]);
        list = mem_cons(mem_atom(buf, strlen(buf)), list);
    }
    return result_ok(value_list(list));
}

// ask n | [list] [cmds] - Run cmds with the active set temporarily
// replaced; the previous set is restored on every outcome, errors and
// throws included (the pause save/restore discipline).
static Result prim_ask(Evaluator *eval, int argc, Value *args)
{
    REQUIRE_ARGC(2);
    REQUIRE_LIST(args[1]);

    uint8_t set[MAX_TURTLES];
    int count;
    Result error;
    if (!parse_turtle_set(args[0], set, &count, &error))
    {
        return error;
    }

    uint8_t saved[MAX_TURTLES];
    int saved_count = active_count;
    memcpy(saved, active_set, sizeof(saved));

    memcpy(active_set, set, (size_t)count);
    active_count = count;

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (turtle)
    {
        select_first_active(turtle);
    }

    Result r = eval_run_list_expr(eval, args[1].as.node);

    memcpy(active_set, saved, sizeof(saved));
    active_count = saved_count;
    if (turtle)
    {
        select_first_active(turtle);
    }

    return r;
}

// each [cmds] - Run cmds once per active turtle in ascending order with
// the set narrowed to that turtle; restores the set afterwards.
static Result prim_each(Evaluator *eval, int argc, Value *args)
{
    REQUIRE_ARGC(1);
    REQUIRE_LIST(args[0]);

    uint8_t saved[MAX_TURTLES];
    int saved_count = active_count;
    memcpy(saved, active_set, sizeof(saved));

    const LogoConsoleTurtle *turtle = get_turtle_ops();

    Result r = result_none();
    for (int i = 0; i < saved_count; i++)
    {
        active_set[0] = saved[i];
        active_count = 1;
        if (turtle)
        {
            select_first_active(turtle);
        }

        r = eval_run_list(eval, args[0].as.node);
        if (r.status != RESULT_NONE)
        {
            break;  // Error, throw, stop, output: propagate to the caller
        }
    }

    memcpy(active_set, saved, sizeof(saved));
    active_count = saved_count;
    if (turtle)
    {
        select_first_active(turtle);
    }

    return r;
}

//==========================================================================
// Sensing and collision
//
// touching?/over?/colourunder work in the device's screen-pixel space:
// the device exposes each turtle's rendered raster (get_raster) and the
// canvas beneath it (canvas_point), and core does the geometry here so
// the logic is device-independent and testable on the mock. distance is
// pure geometry over the Logo-coordinate positions.
//==========================================================================

// Validate a single turtle-number argument (integer 0..MAX_TURTLES-1).
static bool arg_turtle(Value v, uint8_t *out, Result *error)
{
    float num;
    // Range-check before any integer conversion: a huge or non-finite
    // input must be rejected without reaching an out-of-range (int) cast.
    // floorf handles the integrality test (and NaN, which fails it).
    if (!value_to_number(v, &num) || num < 0 || num >= MAX_TURTLES ||
        num != floorf(num))
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
        return false;
    }
    *out = (uint8_t)num;
    return true;
}

// Is the raster's mask pixel at flat index i opaque?
static inline bool raster_solid(const LogoTurtleRaster *r, int i)
{
    uint8_t v = r->mask[i];
    return r->indexed ? (v != LOGO_RASTER_TRANSPARENT) : (v != 0);
}

// True when any opaque pixel of raster a coincides with one of raster b.
// In wrap mode (sw/sh nonzero) b is also tested shifted by +/- the screen
// size, so a contact that straddles an edge counts (the compositor draws
// such a sprite on both sides).
static bool rasters_overlap(const LogoTurtleRaster *a, const LogoTurtleRaster *b,
                            int sw, int sh)
{
    for (int kx = (sw ? -1 : 0); kx <= (sw ? 1 : 0); kx++)
    {
        for (int ky = (sh ? -1 : 0); ky <= (sh ? 1 : 0); ky++)
        {
            int bx = b->x + kx * sw;
            int by = b->y + ky * sh;

            int x0 = a->x > bx ? a->x : bx;
            int y0 = a->y > by ? a->y : by;
            int x1 = (a->x + a->w) < (bx + b->w) ? (a->x + a->w) : (bx + b->w);
            int y1 = (a->y + a->h) < (by + b->h) ? (a->y + a->h) : (by + b->h);

            for (int y = y0; y < y1; y++)
            {
                for (int x = x0; x < x1; x++)
                {
                    int ai = (y - a->y) * a->w + (x - a->x);
                    int bi = (y - by) * b->w + (x - bx);
                    if (raster_solid(a, ai) && raster_solid(b, bi))
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// touching? t1 t2 - true when the two turtles' rendered pixels overlap.
// Both must be visible; the masks are AND-ed at their relative offset.
static Result prim_touchingp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);

    uint8_t a, b;
    Result error;
    if (!arg_turtle(args[0], &a, &error)) return error;
    if (!arg_turtle(args[1], &b, &error)) return error;

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (!turtle || !turtle->get_raster)
    {
        return result_ok(value_bool(false));
    }

    LogoTurtleRaster ra, rb;
    select_turtle(turtle, a);
    bool ga = turtle->get_raster(&ra);
    select_turtle(turtle, b);
    bool gb = turtle->get_raster(&rb);
    select_first_active(turtle);

    bool hit = false;
    if (ga && gb && ra.visible && rb.visible)
    {
        int sw = 0, sh = 0;
        bool wrap = false;
        if (turtle->sense_metrics)
        {
            turtle->sense_metrics(&sw, &sh, &wrap);
        }
        hit = rasters_overlap(&ra, &rb, wrap ? sw : 0, wrap ? sh : 0);
    }

    return result_ok(value_bool(hit));
}

// over? colour - true when any canvas pixel under the first active
// turtle's mask has palette index colour (senses drawings, not sprites).
static Result prim_overp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_NUMBER(args[0], colour);

    if (colour < 0 || colour > 255 || colour != (float)(int)colour)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (!turtle || !turtle->get_raster || !turtle->canvas_point)
    {
        return result_ok(value_bool(false));
    }

    select_first_active(turtle);

    LogoTurtleRaster r;
    bool found = false;
    if (turtle->get_raster(&r))
    {
        uint8_t want = (uint8_t)colour;
        for (int row = 0; row < r.h && !found; row++)
        {
            for (int col = 0; col < r.w; col++)
            {
                if (raster_solid(&r, row * r.w + col) &&
                    turtle->canvas_point(r.x + col, r.y + row) == want)
                {
                    found = true;
                    break;
                }
            }
        }
    }

    return result_ok(value_bool(found));
}

// colourunder - output the canvas palette index at the first active
// turtle's position (the drawing beneath it, sprites excluded).
static Result prim_colourunder(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (!turtle || !turtle->get_raster || !turtle->canvas_point)
    {
        return result_ok(value_number(0));
    }

    select_first_active(turtle);

    LogoTurtleRaster r;
    uint8_t idx = 0;
    if (turtle->get_raster(&r))
    {
        idx = turtle->canvas_point(r.cx, r.cy);
    }

    return result_ok(value_number((float)idx));
}

// distance t - Euclidean distance from the first active turtle to turtle t
static Result prim_distance(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    uint8_t target;
    Result error;
    if (!arg_turtle(args[0], &target, &error)) return error;

    const LogoConsoleTurtle *turtle = get_turtle_ops();
    if (!turtle || !turtle->get_position)
    {
        return result_ok(value_number(0));
    }

    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    select_first_active(turtle);
    turtle->get_position(&x0, &y0);
    select_turtle(turtle, target);
    turtle->get_position(&x1, &y1);
    select_first_active(turtle);

    float dx = x1 - x0;
    float dy = y1 - y0;
    return result_ok(value_number(sqrtf(dx * dx + dy * dy)));
}

//==========================================================================
// Registration
//==========================================================================

void primitives_turtle_init(void)
{
    // Initialize the core 24-bit palette from default values
    core_palette_init();

    // Boot addresses turtle 0 only
    reset_active_set();

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
    primitive_register("arc", 2, prim_arc);
    
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
    primitive_register("stamp", 0, prim_stamp);
    primitive_register("snapsh", 3, prim_snapsh);
    primitive_register("setrot", 1, prim_setrot);
    primitive_register("setmag", 1, prim_setmag);

    // Autonomous motion and animation
    primitive_register("setspeed", 1, prim_setspeed);
    primitive_register("speed", 0, prim_speed);
    primitive_register("setanim", 3, prim_setanim);

    // Multi-turtle addressing
    primitive_register("tell", 1, prim_tell);
    primitive_register("ask", 2, prim_ask);
    primitive_register("each", 1, prim_each);
    primitive_register("who", 0, prim_who);

    // Sensing and collision
    primitive_register("touching?", 2, prim_touchingp);
    primitive_register("touchingp", 2, prim_touchingp);
    primitive_register("over?", 1, prim_overp);
    primitive_register("overp", 1, prim_overp);
    primitive_register("colourunder", 0, prim_colourunder);
    primitive_register("colorunder", 0, prim_colourunder);
    primitive_register("distance", 1, prim_distance);
}
