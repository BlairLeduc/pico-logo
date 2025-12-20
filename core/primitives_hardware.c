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

            // Wait indefinitely
            while (1)
            {
                io->hardware->ops->sleep(1000);
            }
        }
    }
    return result_error_arg(ERR_DONT_KNOW_HOW, ".poweroff", "");
}

void primitives_hardware_init(void)
{
    primitive_register("battery", 0, prim_battery_level);
    primitive_register(".poweroff", 0, prim_poweroff);
}