//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "variables.h"
#include "memory.h"
#include "procedures.h"  // For proc_get_frame_stack()
#include "frame.h"
#include <string.h>
#include <strings.h>

// Configuration for memory constraints
#define MAX_GLOBAL_VARIABLES 128

typedef struct
{
    const char *name; // Points to interned atom string
    Value value;
    bool active;
    bool has_value;   // false if declared but not yet assigned
    bool buried;      // if true, hidden from pons/erns/etc
} Variable;

// Global variables storage
static Variable global_variables[MAX_GLOBAL_VARIABLES];
static int global_count = 0;

// Top-level (global) test state
static bool global_test_valid = false;
static bool global_test_value = false;

void variables_init(void)
{
    global_count = 0;
    for (int i = 0; i < MAX_GLOBAL_VARIABLES; i++)
    {
        global_variables[i].active = false;
        global_variables[i].has_value = false;
        global_variables[i].buried = false;
    }
    global_test_valid = false;
    global_test_value = false;
}

// Find variable in global storage, returns index or -1
static int find_global(const char *name)
{
    for (int i = 0; i < global_count; i++)
    {
        if (global_variables[i].active && strcasecmp(global_variables[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

bool var_declare_local(const char *name)
{
    // Check if we're inside a procedure (frame stack not empty)
    FrameStack *frames = proc_get_frame_stack();
    if (frames && !frame_stack_is_empty(frames))
    {
        // Declare local in current frame
        return frame_declare_local(frames, name);
    }

    // At top level, local behaves like global but unbound
    int idx = find_global(name);
    if (idx >= 0)
    {
        // Already exists
        return true;
    }
    
    // Create new unbound global
    for (int i = 0; i < MAX_GLOBAL_VARIABLES; i++)
    {
        if (!global_variables[i].active)
        {
            global_variables[i].name = name;
            global_variables[i].active = true;
            global_variables[i].has_value = false;
            if (i >= global_count)
                global_count = i + 1;
            return true;
        }
    }
    
    // Global table full
    return false;
}

bool var_set_local(const char *name, Value value)
{
    // Use frame system if inside a procedure
    FrameStack *frames = proc_get_frame_stack();
    if (frames && !frame_stack_is_empty(frames))
    {
        // Add or update local in current frame
        return frame_add_local(frames, name, value);
    }

    // At top level, set as global
    return var_set(name, value);
}

bool var_set(const char *name, Value value)
{
    // First, search frame stack for existing local binding (if in a procedure)
    FrameStack *frames = proc_get_frame_stack();
    if (frames && !frame_stack_is_empty(frames))
    {
        FrameHeader *found_frame = NULL;
        Binding *binding = frame_find_binding_in_chain(frames, name, &found_frame);
        if (binding)
        {
            // Found in frame chain - update it
            binding->value = value;
            return true;
        }
    }

    // Not in any local frame, check/create global
    int idx = find_global(name);
    if (idx >= 0)
    {
        global_variables[idx].value = value;
        global_variables[idx].has_value = true;
        return true;
    }

    // Create new global
    for (int i = 0; i < MAX_GLOBAL_VARIABLES; i++)
    {
        if (!global_variables[i].active)
        {
            global_variables[i].name = name;
            global_variables[i].value = value;
            global_variables[i].active = true;
            global_variables[i].has_value = true;
            if (i >= global_count)
                global_count = i + 1;
            return true;
        }
    }
    // Out of space
    return false;
}

bool var_get(const char *name, Value *out)
{
    // First, search frame stack for local bindings (if in a procedure)
    FrameStack *frames = proc_get_frame_stack();
    if (frames && !frame_stack_is_empty(frames))
    {
        Binding *binding = frame_find_binding_in_chain(frames, name, NULL);
        if (binding)
        {
            *out = binding->value;
            return true;
        }
    }

    // Check globals
    int idx = find_global(name);
    if (idx >= 0)
    {
        if (!global_variables[idx].has_value)
        {
            return false;  // Declared but no value
        }
        *out = global_variables[idx].value;
        return true;
    }
    return false;
}

bool var_exists(const char *name)
{
    // Search frame stack first (if in a procedure)
    FrameStack *frames = proc_get_frame_stack();
    if (frames && !frame_stack_is_empty(frames))
    {
        Binding *binding = frame_find_binding_in_chain(frames, name, NULL);
        if (binding)
        {
            return true;
        }
    }

    // Check globals
    int idx = find_global(name);
    return (idx >= 0 && global_variables[idx].has_value);
}

void var_erase(const char *name)
{
    // Note: Erasing locals is not supported - locals are scoped to procedure frames
    // and automatically cleaned up when the frame is popped.
    // This function erases from globals only.
    
    int idx = find_global(name);
    if (idx >= 0)
    {
        global_variables[idx].active = false;
        global_variables[idx].has_value = false;
    }
}

void var_erase_all(void)
{
    variables_init();
}

// Erase all global variables (respects buried flag if check_buried is true)
void var_erase_all_globals(bool check_buried)
{
    for (int i = 0; i < global_count; i++)
    {
        if (global_variables[i].active)
        {
            if (!check_buried || !global_variables[i].buried)
            {
                global_variables[i].active = false;
                global_variables[i].has_value = false;
            }
        }
    }
}

// Bury/unbury support
void var_bury(const char *name)
{
    int idx = find_global(name);
    if (idx >= 0)
    {
        global_variables[idx].buried = true;
    }
}

void var_unbury(const char *name)
{
    int idx = find_global(name);
    if (idx >= 0)
    {
        global_variables[idx].buried = false;
    }
}

void var_bury_all(void)
{
    for (int i = 0; i < global_count; i++)
    {
        if (global_variables[i].active && global_variables[i].has_value)
        {
            global_variables[i].buried = true;
        }
    }
}

void var_unbury_all(void)
{
    for (int i = 0; i < global_count; i++)
    {
        if (global_variables[i].active)
        {
            global_variables[i].buried = false;
        }
    }
}

// Iteration support for workspace display
int var_global_count(bool include_buried)
{
    int count = 0;
    for (int i = 0; i < global_count; i++)
    {
        if (global_variables[i].active && global_variables[i].has_value)
        {
            if (include_buried || !global_variables[i].buried)
            {
                count++;
            }
        }
    }
    return count;
}

// Get global variable by index (for iteration)
// Returns false if index is out of range or variable is filtered out
bool var_get_global_by_index(int index, bool include_buried, 
                              const char **name_out, Value *value_out)
{
    int current = 0;
    for (int i = 0; i < global_count; i++)
    {
        if (global_variables[i].active && global_variables[i].has_value)
        {
            if (include_buried || !global_variables[i].buried)
            {
                if (current == index)
                {
                    if (name_out) *name_out = global_variables[i].name;
                    if (value_out) *value_out = global_variables[i].value;
                    return true;
                }
                current++;
            }
        }
    }
    return false;
}

// Count local variables visible in the current scope chain
int var_local_count(void)
{
    // Use frame system
    FrameStack *frames = proc_get_frame_stack();
    if (frames && frame_stack_depth(frames) > 0)
    {
        int count = 0;
        // Iterate through all frames and count bindings with values
        for (int d = frame_stack_depth(frames) - 1; d >= 0; d--)
        {
            FrameHeader *frame = frame_at_depth(frames, d);
            if (frame)
            {
                Binding *bindings = frame_get_bindings(frame);
                int binding_count = frame_binding_count(frame);
                for (int i = 0; i < binding_count; i++)
                {
                    // Only count bindings that have actual values
                    if (bindings[i].value.type != VALUE_NONE)
                    {
                        count++;
                    }
                }
            }
        }
        return count;
    }
    
    // Not inside a procedure - no locals
    return 0;
}

// Get local variable by index (for iteration)
// Iterates from newest scope to oldest (most recent first)
bool var_get_local_by_index(int index, const char **name_out, Value *value_out)
{
    // Use frame system
    FrameStack *frames = proc_get_frame_stack();
    if (frames && frame_stack_depth(frames) > 0)
    {
        int current = 0;
        // Iterate from newest frame to oldest
        for (int d = frame_stack_depth(frames) - 1; d >= 0; d--)
        {
            FrameHeader *frame = frame_at_depth(frames, d);
            if (!frame) continue;
            
            Binding *bindings = frame_get_bindings(frame);
            int count = frame_binding_count(frame);
            
            for (int i = 0; i < count; i++)
            {
                // In frame system, VALUE_NONE means declared but not set
                if (bindings[i].value.type != VALUE_NONE)
                {
                    if (current == index)
                    {
                        if (name_out) *name_out = bindings[i].name;
                        if (value_out) *value_out = bindings[i].value;
                        return true;
                    }
                    current++;
                }
            }
        }
    }
    
    // Not inside a procedure or index out of range
    return false;
}

// Check if a variable name is shadowed by a local variable in the scope chain
bool var_is_shadowed_by_local(const char *name)
{
    // Use frame system
    FrameStack *frames = proc_get_frame_stack();
    if (frames && frame_stack_depth(frames) > 0)
    {
        FrameHeader *found_frame = NULL;
        Binding *binding = frame_find_binding_in_chain(frames, name, &found_frame);
        // In frame system, binding exists means variable is local
        // VALUE_NONE means declared but not yet assigned, still counts as shadowing
        return binding != NULL;
    }
    
    // Not inside a procedure - no local shadowing possible
    return false;
}

// Mark all variable values as GC roots
void var_gc_mark_all(void)
{
    // Mark global variables
    for (int i = 0; i < global_count; i++)
    {
        if (global_variables[i].active && global_variables[i].has_value)
        {
            Value v = global_variables[i].value;
            if (v.type == VALUE_WORD || v.type == VALUE_LIST)
            {
                mem_gc_mark(v.as.node);
            }
        }
    }
    
    // Note: Local variables in frames are marked via frame_gc_mark_all()
    // which is called from memory.c's garbage collector
}

//==========================================================================
// Test state management (local to procedure scope)
//==========================================================================

void var_set_test(bool value)
{
    // Try to use frame system if frames are active
    FrameStack *frames = proc_get_frame_stack();
    if (frames && !frame_stack_is_empty(frames))
    {
        frame_set_test(frames, value);
        return;
    }

    // At top level, set global test state
    global_test_valid = true;
    global_test_value = value;
}

bool var_get_test(bool *out)
{
    // Try to use frame system if frames are active
    FrameStack *frames = proc_get_frame_stack();
    if (frames && !frame_stack_is_empty(frames))
    {
        // Check frame chain first
        if (frame_get_test(frames, out))
        {
            return true;
        }
        // Fall through to check global test state
    }
    
    // Check global test state
    if (global_test_valid)
    {
        if (out) *out = global_test_value;
        return true;
    }
    
    return false;  // No test has been run in scope chain
}

bool var_test_is_valid(void)
{
    return var_get_test(NULL);
}

void var_reset_test_state(void)
{
    global_test_valid = false;
    global_test_value = false;
    // Note: scope-level test state is automatically reset when scopes are pushed
}
