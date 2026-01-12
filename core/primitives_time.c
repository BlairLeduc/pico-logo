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
    if (month < 1 || month > 12 || day < 1 || day > 31)
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
// Helper: Parse a 3-element list into three integers
// Returns true on success, false if the list doesn't have exactly 3 numeric elements
//
static bool parse_three_element_list(Node list, int *a, int *b, int *c)
{
    if (mem_is_nil(list)) return false;
    
    Node first = mem_car(list);
    list = mem_cdr(list);
    if (mem_is_nil(list)) return false;
    
    Node second = mem_car(list);
    list = mem_cdr(list);
    if (mem_is_nil(list)) return false;
    
    Node third = mem_car(list);
    list = mem_cdr(list);
    if (!mem_is_nil(list)) return false;  // Too many elements
    
    Value v1 = value_word(first);
    Value v2 = value_word(second);
    Value v3 = value_word(third);
    
    float f1, f2, f3;
    if (!value_to_number(v1, &f1) ||
        !value_to_number(v2, &f2) ||
        !value_to_number(v3, &f3))
    {
        return false;
    }
    
    *a = (int)f1;
    *b = (int)f2;
    *c = (int)f3;
    return true;
}

//
// Helper: Build a time/date list from three integers
//
static Node build_three_element_list(int a, int b, int c)
{
    char buf1[16], buf2[16], buf3[16];
    snprintf(buf1, sizeof(buf1), "%d", a);
    snprintf(buf2, sizeof(buf2), "%d", b);
    snprintf(buf3, sizeof(buf3), "%d", c);
    
    Node atom3 = mem_atom_cstr(buf3);
    Node atom2 = mem_atom_cstr(buf2);
    Node atom1 = mem_atom_cstr(buf1);
    
    return mem_cons(atom1, mem_cons(atom2, mem_cons(atom3, NODE_NIL)));
}

//
// Helper: Get days in a month, accounting for leap years
//
static int days_in_month(int year, int month)
{
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month < 1 || month > 12) return 30;  // Fallback
    
    int d = days[month];
    
    // February leap year check
    if (month == 2)
    {
        bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (is_leap) d = 29;
    }
    
    return d;
}

//
// addtime [h1 m1 s1] [h2 m2 s2] - adds two times together
// The first time must be valid (positive). The second can be positive or negative offset.
//
static Result prim_addtime(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);
    
    int h1, m1, s1;
    if (!parse_three_element_list(args[0].as.node, &h1, &m1, &s1))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    // First time must be valid positive integers
    if (h1 < 0 || m1 < 0 || s1 < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    int h2, m2, s2;
    if (!parse_three_element_list(args[1].as.node, &h2, &m2, &s2))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    
    // Add the times
    int total_seconds = s1 + s2;
    int total_minutes = m1 + m2;
    int total_hours = h1 + h2;
    
    // Handle second overflow/underflow
    while (total_seconds >= 60)
    {
        total_seconds -= 60;
        total_minutes++;
    }
    while (total_seconds < 0)
    {
        total_seconds += 60;
        total_minutes--;
    }
    
    // Handle minute overflow/underflow
    while (total_minutes >= 60)
    {
        total_minutes -= 60;
        total_hours++;
    }
    while (total_minutes < 0)
    {
        total_minutes += 60;
        total_hours--;
    }
    
    Node result = build_three_element_list(total_hours, total_minutes, total_seconds);
    return result_ok(value_list(result));
}

//
// adddate [y1 m1 d1] [y2 m2 d2] - adds two dates together
// The first date must be valid (positive). The second can be positive or negative offset.
//
static Result prim_adddate(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);
    
    int y1, m1, d1;
    if (!parse_three_element_list(args[0].as.node, &y1, &m1, &d1))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    // First date must be valid positive integers
    if (y1 < 0 || m1 < 1 || d1 < 1)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    int y2, m2, d2;
    if (!parse_three_element_list(args[1].as.node, &y2, &m2, &d2))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    
    // Add the dates
    int total_days = d1 + d2;
    int total_months = m1 + m2;
    int total_years = y1 + y2;
    
    // Handle month overflow/underflow first (affects days calculation)
    while (total_months > 12)
    {
        total_months -= 12;
        total_years++;
    }
    while (total_months < 1)
    {
        total_months += 12;
        total_years--;
    }
    
    // Handle day overflow
    while (total_days > days_in_month(total_years, total_months))
    {
        total_days -= days_in_month(total_years, total_months);
        total_months++;
        if (total_months > 12)
        {
            total_months = 1;
            total_years++;
        }
    }
    
    // Handle day underflow
    while (total_days < 1)
    {
        total_months--;
        if (total_months < 1)
        {
            total_months = 12;
            total_years--;
        }
        total_days += days_in_month(total_years, total_months);
    }
    
    Node result = build_three_element_list(total_years, total_months, total_days);
    return result_ok(value_list(result));
}

//
// difftime [h1 m1 s1] [h2 m2 s2] - calculates the difference between two times
// Both times must be valid positive integers.
// Outputs time1 - time2 (can be negative hours if time2 > time1)
//
static Result prim_difftime(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);
    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);
    
    int h1, m1, s1;
    if (!parse_three_element_list(args[0].as.node, &h1, &m1, &s1))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    // First time must be valid positive integers
    if (h1 < 0 || m1 < 0 || s1 < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    int h2, m2, s2;
    if (!parse_three_element_list(args[1].as.node, &h2, &m2, &s2))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    
    // Second time must also be valid positive integers
    if (h2 < 0 || m2 < 0 || s2 < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    
    // Convert to total seconds for easy calculation
    int total1 = h1 * 3600 + m1 * 60 + s1;
    int total2 = h2 * 3600 + m2 * 60 + s2;
    int diff = total1 - total2;
    
    // Determine sign
    bool negative = diff < 0;
    if (negative) diff = -diff;
    
    int diff_hours = diff / 3600;
    diff %= 3600;
    int diff_minutes = diff / 60;
    int diff_seconds = diff % 60;
    
    if (negative) diff_hours = -diff_hours;
    
    Node result = build_three_element_list(diff_hours, diff_minutes, diff_seconds);
    return result_ok(value_list(result));
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
    primitive_register("addtime", 2, prim_addtime);
    primitive_register("adddate", 2, prim_adddate);
    primitive_register("difftime", 2, prim_difftime);
}
