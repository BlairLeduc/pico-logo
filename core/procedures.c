//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "procedures.h"
#include "eval.h"
#include "variables.h"
#include "error.h"
#include "format.h"
#include "memory.h"
#include "primitives.h"
#include "frame.h"
#include "devices/io.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

// Maximum number of user-defined procedures
#define MAX_PROCEDURES 128

// Maximum procedure call stack depth (for current proc tracking)
#define MAX_CURRENT_PROC_DEPTH 32

// Frame stack arena size (configurable per platform)
// Default: 32KB for host/testing, can be adjusted via cmake
#ifndef FRAME_STACK_SIZE
#define FRAME_STACK_SIZE (32 * 1024)
#endif

// Procedure storage
static UserProcedure procedures[MAX_PROCEDURES];
static int procedure_count = 0;

// Tail call state (global for trampoline)
static TailCall tail_call_state;

// Current procedure stack (for pause prompt)
static const char *current_proc_stack[MAX_CURRENT_PROC_DEPTH];
static int current_proc_depth = 0;

// Global frame stack for procedure calls
static uint8_t frame_stack_memory[FRAME_STACK_SIZE];
static FrameStack global_frame_stack;


void procedures_init(void)
{
    procedure_count = 0;
    for (int i = 0; i < MAX_PROCEDURES; i++)
    {
        procedures[i].name = NULL;
        procedures[i].param_count = 0;
        procedures[i].body = NODE_NIL;
        procedures[i].buried = false;
        procedures[i].stepped = false;
        procedures[i].traced = false;
    }
    proc_clear_tail_call();
    current_proc_depth = 0;
    
    // Initialize the global frame stack
    frame_stack_init(&global_frame_stack, frame_stack_memory, sizeof(frame_stack_memory));
}

// Find procedure index, returns -1 if not found
static int find_procedure_index(const char *name)
{
    for (int i = 0; i < procedure_count; i++)
    {
        if (procedures[i].name && strcasecmp(procedures[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

bool proc_define(const char *name, const char **params, int param_count, Node body)
{
    // Check if already exists
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        // Redefine existing procedure
        procedures[idx].param_count = param_count;
        for (int i = 0; i < param_count && i < MAX_PROC_PARAMS; i++)
        {
            procedures[idx].params[i] = params[i];
        }
        procedures[idx].body = body;
        return true;
    }

    // Find free slot
    for (int i = 0; i < MAX_PROCEDURES; i++)
    {
        if (procedures[i].name == NULL)
        {
            procedures[i].name = name;
            procedures[i].param_count = param_count;
            for (int j = 0; j < param_count && j < MAX_PROC_PARAMS; j++)
            {
                procedures[i].params[j] = params[j];
            }
            procedures[i].body = body;
            procedures[i].buried = false;
            procedures[i].stepped = false;
            procedures[i].traced = false;
            if (i >= procedure_count)
                procedure_count = i + 1;
            return true;
        }
    }
    return false; // Out of space
}

UserProcedure *proc_find(const char *name)
{
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        return &procedures[idx];
    }
    return NULL;
}

bool proc_exists(const char *name)
{
    return find_procedure_index(name) >= 0;
}

void proc_erase(const char *name)
{
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        procedures[idx].name = NULL;
        procedures[idx].param_count = 0;
        procedures[idx].body = NODE_NIL;
    }
}

void proc_erase_all(bool check_buried)
{
    for (int i = 0; i < procedure_count; i++)
    {
        if (procedures[i].name != NULL)
        {
            if (!check_buried || !procedures[i].buried)
            {
                procedures[i].name = NULL;
                procedures[i].param_count = 0;
                procedures[i].body = NODE_NIL;
            }
        }
    }
}

TailCall *proc_get_tail_call(void)
{
    return &tail_call_state;
}

void proc_clear_tail_call(void)
{
    tail_call_state.is_tail_call = false;
    tail_call_state.proc_name = NULL;
    tail_call_state.arg_count = 0;
}

int proc_count(bool include_buried)
{
    int count = 0;
    for (int i = 0; i < procedure_count; i++)
    {
        if (procedures[i].name != NULL)
        {
            if (include_buried || !procedures[i].buried)
            {
                count++;
            }
        }
    }
    return count;
}

UserProcedure *proc_get_by_index(int index)
{
    int count = 0;
    for (int i = 0; i < procedure_count; i++)
    {
        if (procedures[i].name != NULL)
        {
            if (count == index)
            {
                return &procedures[i];
            }
            count++;
        }
    }
    return NULL;
}

void proc_bury(const char *name)
{
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        procedures[idx].buried = true;
    }
}

void proc_unbury(const char *name)
{
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        procedures[idx].buried = false;
    }
}

void proc_bury_all(void)
{
    for (int i = 0; i < procedure_count; i++)
    {
        if (procedures[i].name != NULL)
        {
            procedures[i].buried = true;
        }
    }
}

void proc_unbury_all(void)
{
    for (int i = 0; i < procedure_count; i++)
    {
        if (procedures[i].name != NULL)
        {
            procedures[i].buried = false;
        }
    }
}

void proc_step(const char *name)
{
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        procedures[idx].stepped = true;
    }
}

void proc_unstep(const char *name)
{
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        procedures[idx].stepped = false;
    }
}

bool proc_is_stepped(const char *name)
{
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        return procedures[idx].stepped;
    }
    return false;
}

void proc_trace(const char *name)
{
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        procedures[idx].traced = true;
    }
}

void proc_untrace(const char *name)
{
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        procedures[idx].traced = false;
    }
}

bool proc_is_traced(const char *name)
{
    int idx = find_procedure_index(name);
    if (idx >= 0)
    {
        return procedures[idx].traced;
    }
    return false;
}
//==========================================================================
// Current Procedure Tracking (for pause prompt)
//==========================================================================

void proc_set_current(const char *name)
{
    if (current_proc_depth > 0)
    {
        current_proc_stack[current_proc_depth - 1] = name;
    }
}

const char *proc_get_current(void)
{
    if (current_proc_depth > 0)
    {
        return current_proc_stack[current_proc_depth - 1];
    }
    return NULL;
}

void proc_push_current(const char *name)
{
    if (current_proc_depth < MAX_CURRENT_PROC_DEPTH)
    {
        current_proc_stack[current_proc_depth++] = name;
    }
}

void proc_pop_current(void)
{
    if (current_proc_depth > 0)
    {
        current_proc_depth--;
    }
}

// Reset all procedure execution state
// Call this after errors or when returning to top level unexpectedly
void proc_reset_execution_state(void)
{
    proc_clear_tail_call();
    current_proc_depth = 0;
    frame_stack_reset(&global_frame_stack);
}

ProcExecSnapshot proc_save_execution_state(void)
{
    return (ProcExecSnapshot){
        .frame_snapshot = frame_stack_snapshot(&global_frame_stack),
        .proc_depth = current_proc_depth,
    };
}

void proc_restore_execution_state(ProcExecSnapshot snapshot)
{
    proc_clear_tail_call();
    frame_stack_restore(&global_frame_stack, snapshot.frame_snapshot);
    current_proc_depth = snapshot.proc_depth;
}

// Mark all procedure bodies as GC roots
void proc_gc_mark_all(void)
{
    for (int i = 0; i < procedure_count; i++)
    {
        if (procedures[i].name != NULL && !mem_is_nil(procedures[i].body))
        {
            mem_gc_mark(procedures[i].body);
        }
    }
}

// Get the global frame stack
FrameStack *proc_get_frame_stack(void)
{
    return &global_frame_stack;
}
