//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  `when` demons and the poll that drives them and autonomous turtle
//  motion/animation. See docs/multi-sprite-design.md §7.
//
//  A demon pairs a condition expression with an action list. On every poll
//  (budget-gated at the instruction poll point and the device idle loop)
//  each armed condition is evaluated; a demon fires its action on the
//  condition's false->true edge, so contact fires once, not every poll.
//  Actions run in a fresh nested evaluator (the pause precedent) so the
//  parent trampoline's TCO state is untouched, and polling is suppressed
//  while an action runs, so demons never nest or storm.
//

#include "demons.h"
#include "eval.h"
#include "lexer.h"
#include "limits.h"
#include "memory.h"
#include "procedures.h"
#include "primitives.h"
#include "devices/io.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

typedef struct Demon
{
    bool armed;      // Slot in use
    bool was_true;   // Condition's value at the previous poll (edge state)
    Node cond;       // Condition expression (a list)
    Node action;     // Action instruction list
} Demon;

static Demon demons[MAX_DEMONS];
static bool g_frozen = false;
static bool g_polling = false;       // Re-entrancy guard: an action is running
static bool g_have_ticked = false;   // Motion clock has a baseline
static uint32_t g_last_tick_ms = 0;  // Motion clock baseline
static uint32_t g_last_poll_ms = 0;  // Budget baseline
static bool g_have_polled = false;   // Budget has fired at least once

// True when the value is the boolean word "true"; sets *ok false when the
// value is not a boolean word at all (so a malformed condition can error).
static bool value_is_true(Value v, bool *ok)
{
    *ok = true;
    if (v.type == VALUE_WORD)
    {
        const char *s = mem_word_ptr(v.as.node);
        if (strcasecmp(s, "true") == 0) return true;
        if (strcasecmp(s, "false") == 0) return false;
    }
    *ok = false;
    return false;
}

Result demons_set(Node cond, Node action)
{
    bool disarm = mem_is_nil(action);
    Value cond_val = value_list(cond);

    // Replace or disarm an already-armed demon with an equal condition.
    for (int i = 0; i < MAX_DEMONS; i++)
    {
        if (demons[i].armed && values_equal(value_list(demons[i].cond), cond_val))
        {
            if (disarm)
            {
                demons[i].armed = false;
            }
            else
            {
                demons[i].action = action;
                demons[i].was_true = false;  // Re-arm fires fresh
            }
            return result_none();
        }
    }

    if (disarm)
    {
        return result_none();  // Disarming an absent demon is a no-op
    }

    // Arm a new demon in the first free slot.
    for (int i = 0; i < MAX_DEMONS; i++)
    {
        if (!demons[i].armed)
        {
            demons[i].armed = true;
            demons[i].was_true = false;
            demons[i].cond = cond;
            demons[i].action = action;
            return result_none();
        }
    }

    return result_error(ERR_OUT_OF_SPACE);
}

void demons_print(void)
{
    LogoIO *io = primitives_get_io();
    if (!io) return;

    for (int i = 0; i < MAX_DEMONS; i++)
    {
        if (!demons[i].armed) continue;

        // value_to_string already brackets a list and reuses one static
        // buffer, so snapshot the condition before formatting the action.
        char cond_buf[64];
        strncpy(cond_buf, value_to_string(value_list(demons[i].cond)), sizeof(cond_buf) - 1);
        cond_buf[sizeof(cond_buf) - 1] = '\0';

        char line[160];
        snprintf(line, sizeof(line), "when %s %s",
                 cond_buf, value_to_string(value_list(demons[i].action)));
        logo_io_console_write_line(io, line);
    }
}

void demons_freeze(void) { g_frozen = true; }

void demons_thaw(void)
{
    g_frozen = false;
    // Resume the motion clock without crediting the frozen interval.
    g_have_ticked = false;
}

bool demons_frozen(void) { return g_frozen; }

bool demons_running(void) { return g_polling; }

void demons_reset(void)
{
    for (int i = 0; i < MAX_DEMONS; i++)
    {
        demons[i].armed = false;
    }
    g_frozen = false;
    g_polling = false;
    g_have_ticked = false;
    g_have_polled = false;

    // Stop autonomous motion and animation on every turtle: nothing acts on
    // its own after a reset. (No-op at boot, when there is no device yet.)
    LogoIO *io = primitives_get_io();
    const LogoConsoleTurtle *turtle =
        (io && io->console) ? io->console->turtle : NULL;
    if (turtle && turtle->select)
    {
        for (uint8_t n = 0; n < MAX_TURTLES; n++)
        {
            turtle->select(n);
            if (turtle->set_speed) turtle->set_speed(0.0f);
            if (turtle->set_anim) turtle->set_anim(0, 0, 0);
        }
        turtle->select(0);
    }
    else if (turtle)
    {
        if (turtle->set_speed) turtle->set_speed(0.0f);
        if (turtle->set_anim) turtle->set_anim(0, 0, 0);
    }
}

Result demons_poll(void)
{
    if (g_frozen || g_polling)
    {
        return result_none();
    }

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_none();
    }

    g_polling = true;
    uint32_t now = logo_io_ticks_ms(io);

    // Advance autonomous motion and animation. The device owns positions;
    // dt is the wall-clock elapsed since the previous tick.
    const LogoConsoleTurtle *turtle =
        (io->console) ? io->console->turtle : NULL;
    if (turtle && turtle->turtle_tick)
    {
        uint32_t dt = g_have_ticked ? (now - g_last_tick_ms) : 0;
        turtle->turtle_tick(dt);
    }
    g_last_tick_ms = now;
    g_have_ticked = true;

    // Fire demons on the false->true edge. Actions run on a fresh evaluator
    // that shares the live frame stack and op stack.
    Result r = result_none();
    Lexer lexer;
    Evaluator eval;
    lexer_init(&lexer, "");
    eval_init(&eval, &lexer);
    eval_set_frames(&eval, proc_get_frame_stack());

    for (int i = 0; i < MAX_DEMONS; i++)
    {
        if (!demons[i].armed) continue;

        Result cr = eval_run_list_expr(&eval, demons[i].cond);
        if (cr.status == RESULT_ERROR || cr.status == RESULT_THROW)
        {
            r = cr;  // Propagate: unwinds and (at toplevel) resets demons
            break;
        }

        bool ok = false;
        bool now_true = (cr.status == RESULT_OK) && value_is_true(cr.value, &ok);
        if (cr.status == RESULT_OK && !ok)
        {
            // Condition is not a truth value
            r = result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(cr.value));
            break;
        }

        bool prev = demons[i].was_true;
        demons[i].was_true = now_true;
        if (now_true && !prev)
        {
            Result ar = eval_run_list(&eval, demons[i].action);
            if (ar.status == RESULT_ERROR || ar.status == RESULT_THROW)
            {
                r = ar;
                break;
            }
        }
    }

    g_polling = false;
    return r;
}

Result demons_maybe_poll(void)
{
    if (g_frozen || g_polling)
    {
        return result_none();
    }

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_none();
    }

    // Without a clock there's no way to budget, so poll every time rather
    // than gating on a timestamp that never advances (which would arm the
    // gate once and then suppress every poll thereafter).
    if (!logo_io_has_ticks_ms(io))
    {
        return demons_poll();
    }

    uint32_t now = logo_io_ticks_ms(io);
    if (g_have_polled && (uint32_t)(now - g_last_poll_ms) < DEMON_POLL_MS)
    {
        return result_none();
    }
    g_last_poll_ms = now;
    g_have_polled = true;
    return demons_poll();
}

void demons_gc_mark_all(void)
{
    for (int i = 0; i < MAX_DEMONS; i++)
    {
        if (demons[i].armed)
        {
            mem_gc_mark(demons[i].cond);
            mem_gc_mark(demons[i].action);
        }
    }
}
