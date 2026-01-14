//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements time management primitives: date, time, setdate, settime
//

#include "primitives.h"
#include "procedures.h"
#include "memory.h"
#include "format.h"
#include "error.h"
#include "eval.h"
#include "devices/io.h"

#include <string.h>
#include <stdio.h>

//
// Helper: Get days in a month, accounting for leap years
//
static bool valid_date(int year, int month, int day)
{
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month < 1 || month > 12) return false;
    int last_day = days[month];

    // February leap year check
    if (month == 2)
    {
        bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (is_leap) last_day = 29;
    }
    
    if (day < 1 || day > last_day) return false;
    
    return true;
}


//
// date - outputs the current date as [year month day]
//
static Result prim_date(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(0);
    (void)args;

    LogoIO *io = primitives_get_io();
    if (!io || !io->hardware || !io->hardware->ops ||
        !io->hardware->ops->get_date)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, "date", NULL);
    }

    int year, month, day;
    if (!io->hardware->ops->get_date(&year, &month, &day))
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, "date", NULL);
    }

    // Build list [year month day]
    char year_buf[16], month_buf[8], day_buf[8];
    snprintf(year_buf, sizeof(year_buf), "%d", year);
    snprintf(month_buf, sizeof(month_buf), "%d", month);
    snprintf(day_buf, sizeof(day_buf), "%d", day);

    Node day_atom = mem_atom_cstr(day_buf);
    Node month_atom = mem_atom_cstr(month_buf);
    Node year_atom = mem_atom_cstr(year_buf);

    Node list = mem_cons(year_atom, mem_cons(month_atom, mem_cons(day_atom, NODE_NIL)));

    return result_ok(value_list(list));
}

//
// time - outputs the current time as [hour minute second]
//
static Result prim_time(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(0);
    (void)args;

    LogoIO *io = primitives_get_io();
    if (!io || !io->hardware || !io->hardware->ops ||
        !io->hardware->ops->get_time)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, "time", NULL);
    }

    int hour, minute, second;
    if (!io->hardware->ops->get_time(&hour, &minute, &second))
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, "time", NULL);
    }

    // Build list [hour minute second]
    char hour_buf[8], min_buf[8], sec_buf[8];
    snprintf(hour_buf, sizeof(hour_buf), "%d", hour);
    snprintf(min_buf, sizeof(min_buf), "%d", minute);
    snprintf(sec_buf, sizeof(sec_buf), "%d", second);

    Node sec_atom = mem_atom_cstr(sec_buf);
    Node min_atom = mem_atom_cstr(min_buf);
    Node hour_atom = mem_atom_cstr(hour_buf);

    Node list = mem_cons(hour_atom, mem_cons(min_atom, mem_cons(sec_atom, NODE_NIL)));

    return result_ok(value_list(list));
}

//
// setdate [year month day] - sets the current date
//
static Result prim_setdate(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_LIST(args[0]);

    LogoIO *io = primitives_get_io();
    if (!io || !io->hardware || !io->hardware->ops ||
        !io->hardware->ops->set_date)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, "setdate", NULL);
    }

    // Parse [year month day] from the input list
    Node list = args[0].as.node;

    // Need exactly 3 elements
    if (mem_is_nil(list))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setdate", "[]");
    }
    Node first = mem_car(list);
    list = mem_cdr(list);

    if (mem_is_nil(list))
    {
        return result_error_arg(ERR_TOO_FEW_ITEMS, "setdate", NULL);
    }
    Node second = mem_car(list);
    list = mem_cdr(list);

    if (mem_is_nil(list))
    {
        return result_error_arg(ERR_TOO_FEW_ITEMS, "setdate", NULL);
    }
    Node third = mem_car(list);
    list = mem_cdr(list);

    if (!mem_is_nil(list))
    {
        return result_error_arg(ERR_TOO_MANY_INPUTS, "setdate", NULL);
    }

    // All must be numbers (stored as word atoms)
    Value year_val = value_word(first);
    Value month_val = value_word(second);
    Value day_val = value_word(third);

    float year_f, month_f, day_f;
    if (!value_to_number(year_val, &year_f) ||
        !value_to_number(month_val, &month_f) ||
        !value_to_number(day_val, &day_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setdate", value_to_string(args[0]));
    }

    int year = (int)year_f;
    int month = (int)month_f;
    int day = (int)day_f;

    // Validate ranges
    if (!valid_date(year, month, day))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "setdate", value_to_string(args[0]));
    }

    if (!io->hardware->ops->set_date(year, month, day))
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, "setdate", NULL);
    }

    return result_none();
}

//
// settime [hour minute second] - sets the current time
//
static Result prim_settime(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_LIST(args[0]);

    LogoIO *io = primitives_get_io();
    if (!io || !io->hardware || !io->hardware->ops ||
        !io->hardware->ops->set_time)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, "settime", NULL);
    }

    // Parse [hour minute second] from the input list
    Node list = args[0].as.node;

    // Need exactly 3 elements
    if (mem_is_nil(list))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "settime", "[]");
    }
    Node first = mem_car(list);
    list = mem_cdr(list);

    if (mem_is_nil(list))
    {
        return result_error_arg(ERR_TOO_FEW_ITEMS, "settime", NULL);
    }
    Node second_node = mem_car(list);
    list = mem_cdr(list);

    if (mem_is_nil(list))
    {
        return result_error_arg(ERR_TOO_FEW_ITEMS, "settime", NULL);
    }
    Node third = mem_car(list);
    list = mem_cdr(list);

    if (!mem_is_nil(list))
    {
        return result_error_arg(ERR_TOO_MANY_INPUTS, "settime", NULL);
    }

    // All must be numbers (stored as word atoms)
    Value hour_val = value_word(first);
    Value minute_val = value_word(second_node);
    Value second_val = value_word(third);

    float hour_f, minute_f, second_f;
    if (!value_to_number(hour_val, &hour_f) ||
        !value_to_number(minute_val, &minute_f) ||
        !value_to_number(second_val, &second_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "settime", value_to_string(args[0]));
    }

    int hour = (int)hour_f;
    int minute = (int)minute_f;
    int second = (int)second_f;

    // Validate ranges
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 59)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "settime", value_to_string(args[0]));
    }

    if (!io->hardware->ops->set_time(hour, minute, second))
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, "settime", NULL);
    }

    return result_none();
}


//
// Register time management primitives
//
void primitives_time_init(void)
{
    primitive_register("date", 0, prim_date);
    primitive_register("time", 0, prim_time);
    primitive_register("setdate", 1, prim_setdate);
    primitive_register("settime", 1, prim_settime);
}
