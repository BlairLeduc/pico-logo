//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  `when` demons: edge-triggered event handlers plus the poll that drives
//  them and autonomous turtle motion/animation. See
//  docs/multi-sprite-design.md §7.
//

#pragma once

#include "value.h"
#include "error.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Arm (or replace) the demon whose condition is structurally equal to
    // `cond`. An empty `action` list disarms that demon instead. Both nodes
    // must be list nodes. Returns RESULT_NONE on success, or an error result
    // (ERR_OUT_OF_SPACE) when the table is full and no matching demon exists.
    Result demons_set(Node cond, Node action);

    // Print the armed demon table to the console (backs `(when)`).
    void demons_print(void);

    // Suspend / resume all autonomous activity — demons and moving or
    // animating turtles (freeze/thaw). Frozen turtles hold their position.
    void demons_freeze(void);
    void demons_thaw(void);
    bool demons_frozen(void);

    // Clear every demon, reset the freeze/edge/timing state, and stop the
    // motion clock. Applied by `cs` and when execution unwinds to the
    // toplevel REPL (error or `throw "toplevel`): nothing acts on its own
    // after a reset.
    void demons_reset(void);

    // Evaluate every armed demon's condition and fire the ones that just
    // became true (false->true edge), and advance autonomous motion and
    // animation. Runs the actions in a fresh nested evaluator; a demon
    // action that errors or throws is returned so the caller can unwind.
    // No-ops while frozen or while a demon action is already running.
    Result demons_poll(void);

    // Budget-gated poll for the instruction poll point and the device idle
    // loop: calls demons_poll() at most once per DEMON_POLL_MS.
    Result demons_maybe_poll(void);

    // Mark the demon table's condition and action nodes as GC roots. Called
    // by `recycle` so armed demons survive a manual garbage collection.
    void demons_gc_mark_all(void);

#ifdef __cplusplus
}
#endif
