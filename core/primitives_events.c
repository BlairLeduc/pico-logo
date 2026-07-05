//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Event primitives: `when` demons and `freeze`/`thaw`. The demon table and
//  the poll that drives it live in core/demons.c.
//

#include "primitives.h"
#include "demons.h"
#include "eval.h"
#include "error.h"

// when [cond] [action]   arm/replace a demon (empty action disarms)
// (when)                 print the armed demon table
static Result prim_when(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    if (argc == 0)
    {
        demons_print();
        return result_none();
    }

    if (argc < 2)
    {
        return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL);
    }

    REQUIRE_LIST(args[0]);
    REQUIRE_LIST(args[1]);

    return demons_set(args[0].as.node, args[1].as.node);
}

// freeze - suspend all autonomous activity (demons and moving turtles)
static Result prim_freeze(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    demons_freeze();
    return result_none();
}

// thaw - resume autonomous activity suspended by freeze
static Result prim_thaw(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    demons_thaw();
    return result_none();
}

void primitives_events_init(void)
{
    // Fresh interpreter: no demons armed, nothing frozen or ticking.
    demons_reset();

    primitive_register("when", 2, prim_when);
    primitive_register("freeze", 0, prim_freeze);
    primitive_register("thaw", 0, prim_thaw);
}
