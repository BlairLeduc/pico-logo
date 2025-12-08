//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "variables.h"
#include <string.h>
#include <strings.h>

// Configuration for memory constraints
#define MAX_GLOBAL_VARIABLES 128
#define MAX_LOCAL_VARIABLES 64
#define MAX_SCOPE_DEPTH 32

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

// Local scope frame
typedef struct
{
    Variable variables[MAX_LOCAL_VARIABLES];
    int count;
} ScopeFrame;

// Scope stack
static ScopeFrame scope_stack[MAX_SCOPE_DEPTH];
static int scope_depth = 0;  // 0 means at top level (global scope only)

void variables_init(void)
{
    global_count = 0;
    for (int i = 0; i < MAX_GLOBAL_VARIABLES; i++)
    {
        global_variables[i].active = false;
        global_variables[i].has_value = false;
        global_variables[i].buried = false;
    }
    scope_depth = 0;
}

void var_push_scope(void)
{
    if (scope_depth >= MAX_SCOPE_DEPTH)
    {
        // Out of scope space - could set an error flag
        return;
    }
    ScopeFrame *frame = &scope_stack[scope_depth];
    frame->count = 0;
    for (int i = 0; i < MAX_LOCAL_VARIABLES; i++)
    {
        frame->variables[i].active = false;
        frame->variables[i].has_value = false;
    }
    scope_depth++;
}

void var_pop_scope(void)
{
    if (scope_depth > 0)
    {
        scope_depth--;
    }
}

int var_scope_depth(void)
{
    return scope_depth;
}

// Find variable in a specific scope frame, returns index or -1
static int find_in_frame(ScopeFrame *frame, const char *name)
{
    for (int i = 0; i < frame->count; i++)
    {
        if (frame->variables[i].active && strcasecmp(frame->variables[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
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

void var_declare_local(const char *name)
{
    if (scope_depth == 0)
    {
        // At top level, local behaves like global but unbound
        int idx = find_global(name);
        if (idx < 0)
        {
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
                    return;
                }
            }
        }
        return;
    }

    ScopeFrame *frame = &scope_stack[scope_depth - 1];
    
    // Check if already declared in this scope
    int idx = find_in_frame(frame, name);
    if (idx >= 0)
    {
        return;  // Already declared
    }

    // Find a free slot
    for (int i = 0; i < MAX_LOCAL_VARIABLES; i++)
    {
        if (!frame->variables[i].active)
        {
            frame->variables[i].name = name;
            frame->variables[i].active = true;
            frame->variables[i].has_value = false;
            if (i >= frame->count)
                frame->count = i + 1;
            return;
        }
    }
    // Out of local variable space - silently fail
}

void var_set_local(const char *name, Value value)
{
    if (scope_depth == 0)
    {
        // At top level, set as global
        var_set(name, value);
        return;
    }

    ScopeFrame *frame = &scope_stack[scope_depth - 1];
    
    // Check if already in current scope
    int idx = find_in_frame(frame, name);
    if (idx >= 0)
    {
        frame->variables[idx].value = value;
        frame->variables[idx].has_value = true;
        return;
    }

    // Create new local variable
    for (int i = 0; i < MAX_LOCAL_VARIABLES; i++)
    {
        if (!frame->variables[i].active)
        {
            frame->variables[i].name = name;
            frame->variables[i].value = value;
            frame->variables[i].active = true;
            frame->variables[i].has_value = true;
            if (i >= frame->count)
                frame->count = i + 1;
            return;
        }
    }
    // Out of space - silently fail
}

void var_set(const char *name, Value value)
{
    // First, search scope chain from innermost to outermost
    for (int d = scope_depth - 1; d >= 0; d--)
    {
        ScopeFrame *frame = &scope_stack[d];
        int idx = find_in_frame(frame, name);
        if (idx >= 0)
        {
            // Found in scope chain - update it
            frame->variables[idx].value = value;
            frame->variables[idx].has_value = true;
            return;
        }
    }

    // Not in any local scope, check/create global
    int idx = find_global(name);
    if (idx >= 0)
    {
        global_variables[idx].value = value;
        global_variables[idx].has_value = true;
        return;
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
            return;
        }
    }
    // Out of space - silently fail
}

bool var_get(const char *name, Value *out)
{
    // Search scope chain from innermost to outermost
    for (int d = scope_depth - 1; d >= 0; d--)
    {
        ScopeFrame *frame = &scope_stack[d];
        int idx = find_in_frame(frame, name);
        if (idx >= 0)
        {
            if (!frame->variables[idx].has_value)
            {
                return false;  // Declared but no value
            }
            *out = frame->variables[idx].value;
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
    // Search scope chain
    for (int d = scope_depth - 1; d >= 0; d--)
    {
        ScopeFrame *frame = &scope_stack[d];
        int idx = find_in_frame(frame, name);
        if (idx >= 0 && frame->variables[idx].has_value)
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
    // Search scope chain first
    for (int d = scope_depth - 1; d >= 0; d--)
    {
        ScopeFrame *frame = &scope_stack[d];
        int idx = find_in_frame(frame, name);
        if (idx >= 0)
        {
            frame->variables[idx].active = false;
            frame->variables[idx].has_value = false;
            return;
        }
    }

    // Erase from globals
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
