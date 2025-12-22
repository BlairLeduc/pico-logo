//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Hardware primitives: batterylevel
//

#include "primitives.h"
#include "procedures.h"
#include "memory.h"
#include "error.h"
#include "eval.h"
#include "devices/io.h"

#include <stdio.h>

// batteryLevel
static Result prim_battery_level(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    int level = -1;
    bool charging = false;

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->get_battery_level)
    {
        io->hardware->ops->get_battery_level(&level, &charging);
    }

    Node list = NODE_NIL;

    if (charging)
    {
        list = mem_cons(mem_atom("true", 4), list);
    }
    else
    {
        list = mem_cons(mem_atom("false", 5), list);
    }
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", (double)level);
    list = mem_cons(mem_atom_cstr(buf), list);

    return result_ok(value_list(list));
}

static Result prim_poweroff(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    (void)args;

    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->power_off)
    {
        bool success = io->hardware->ops->power_off();
        if (success)
        {
            // Power off has a forced delay before powering off, so close I/O now
            logo_io_close_all(io); // Close all I/O before powering off

            // Wait indefinitely, the device will power off and we won't return
            while (1)
            {
                io->hardware->ops->sleep(1000);
            }
        }
    }
    return result_error_arg(ERR_DONT_KNOW_HOW, ".poweroff", "");
}

// toot duration frequency
// (toot duration leftfrequency rightfrequency)
// Plays a tone for the specified duration.
// Duration is in 1/1000ths of a second (milliseconds).
// Frequency is in Hz (131 to 1976).
static Result prim_toot(Evaluator *eval, int argc, Value *args)
{
    (void)eval;

    // Validate argument count (2 or 3)
    if (argc < 2 || argc > 3)
    {
        if (argc < 2)
        {
            return result_error_arg(ERR_NOT_ENOUGH_INPUTS, "toot", NULL);
        }
        return result_error_arg(ERR_TOO_MANY_INPUTS, "toot", NULL);
    }

    // Get duration (first argument)
    if (args[0].type != VALUE_NUMBER)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "toot", 
            args[0].type == VALUE_WORD ? mem_word_ptr(args[0].as.node) : "[]");
    }
    int duration_ms = (int)args[0].as.number;
    if (duration_ms < 0)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", duration_ms);
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "toot", buf);
    }

    // Get frequency/frequencies
    uint32_t left_freq, right_freq;
    
    if (argc == 2)
    {
        // Single frequency for both channels
        if (args[1].type != VALUE_NUMBER)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, "toot", 
                args[1].type == VALUE_WORD ? mem_word_ptr(args[1].as.node) : "[]");
        }
        left_freq = right_freq = (uint32_t)args[1].as.number;
    }
    else
    {
        // Separate frequencies for left and right channels
        if (args[1].type != VALUE_NUMBER)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, "toot", 
                args[1].type == VALUE_WORD ? mem_word_ptr(args[1].as.node) : "[]");
        }
        if (args[2].type != VALUE_NUMBER)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, "toot", 
                args[2].type == VALUE_WORD ? mem_word_ptr(args[2].as.node) : "[]");
        }
        left_freq = (uint32_t)args[1].as.number;
        right_freq = (uint32_t)args[2].as.number;
    }

    // Play the tone if hardware supports it
    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops && io->hardware->ops->toot)
    {
        io->hardware->ops->toot((uint32_t)duration_ms, left_freq, right_freq);
    }
    // If no audio hardware, silently succeed (command has no output)

    return result_none();
}

void primitives_hardware_init(void)
{
    primitive_register("battery", 0, prim_battery_level);
    primitive_register(".poweroff", 0, prim_poweroff);
    primitive_register("toot", 2, prim_toot);
}