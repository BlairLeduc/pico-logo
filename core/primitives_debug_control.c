//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Debug control primitives: pause, co, go, label, wait
//

#include "primitives.h"
#include "eval.h"
#include "procedures.h"
#include "memory.h"
#include "repl.h"
#include "value.h"
#include "devices/io.h"
#include "frame.h"

//==========================================================================
// Timing
//==========================================================================

static Result prim_wait(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], tenths_f);
    
    int tenths = (int)tenths_f;
    if (tenths < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }
    
    // Wait for tenths of a second (each tenth is 100 milliseconds)
    // Check for user interrupt every 100ms to make the wait interruptible
    for (int i = 0; i < tenths; i++)
    {
        if (logo_io_check_user_interrupt(io))
        {
            return result_error(ERR_STOPPED);
        }
        logo_io_sleep(io, 100);
    }
    
    return result_none();
}

//==========================================================================
// Pause/Continue
//==========================================================================

// Flag to signal continue from pause
static bool pause_continue_requested = false;

// Check if continue has been requested and reset the flag
bool pause_check_continue(void)
{
    bool result = pause_continue_requested;
    pause_continue_requested = false;
    return result;
}

// Request continue from pause
void pause_request_continue(void)
{
    pause_continue_requested = true;
}

// Reset pause state (for testing)
void pause_reset_state(void)
{
    pause_continue_requested = false;
}

static Result prim_pause(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    // Get the current procedure name
    const char *proc_name = proc_get_current();
    if (proc_name == NULL && eval && eval->frames && !frame_stack_is_empty(eval->frames))
    {
        FrameHeader *frame = frame_current(eval->frames);
        if (frame && frame->proc)
        {
            proc_name = frame->proc->name;
        }
    }
    if (proc_name == NULL)
    {
        // pause at top level is an error
        return result_error(ERR_AT_TOPLEVEL);
    }
    
    // Run the pause REPL - this blocks until co is called or throw "toplevel
    LogoIO *io = primitives_get_io();
    if (!io || !io->console)
    {
        return result_none();
    }
    
    logo_io_write_line(io, "Pausing...");
    
    ReplState state;
    repl_init(&state, io, REPL_FLAGS_PAUSE, proc_name);
    return repl_run(&state);
}

static Result prim_co(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc); UNUSED(args);
    
    // Signal to exit the pause REPL
    pause_request_continue();
    return result_none();
}

//==========================================================================
// Control Transfer (Go/Label)
//==========================================================================

static Result prim_go(Evaluator *eval, int argc, Value *args)
{
    UNUSED(argc);
    REQUIRE_WORD(args[0]);
    
    // go can only be used inside a procedure
    if (eval->proc_depth == 0)
    {
        return result_error(ERR_ONLY_IN_PROCEDURE);
    }
    
    // Return RESULT_GOTO with the label name
    // The label will be found and jumped to by eval_run_list_with_tco
    const char *label = mem_word_ptr(args[0].as.node);
    return result_goto(label);
}

static Result prim_label(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_WORD(args[0]);
    
    // label does nothing itself
    return result_none();
}

void primitives_debug_control_init(void)
{
    // Timing
    primitive_register("wait", 1, prim_wait);
    
    // Pause/continue
    primitive_register("pause", 0, prim_pause);
    primitive_register("co", 0, prim_co);
    
    // Control transfer
    primitive_register("go", 1, prim_go);
    primitive_register("label", 1, prim_label);
}
