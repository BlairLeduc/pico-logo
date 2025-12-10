//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "procedures.h"
#include "eval.h"
#include "variables.h"
#include "error.h"
#include "memory.h"
#include <string.h>
#include <strings.h>

// Maximum number of user-defined procedures
#define MAX_PROCEDURES 128

// Procedure storage
static UserProcedure procedures[MAX_PROCEDURES];
static int procedure_count = 0;

// Tail call state (global for trampoline)
static TailCall tail_call_state;

void procedures_init(void)
{
    procedure_count = 0;
    for (int i = 0; i < MAX_PROCEDURES; i++)
    {
        procedures[i].name = NULL;
        procedures[i].param_count = 0;
        procedures[i].body = NODE_NIL;
        procedures[i].buried = false;
    }
    proc_clear_tail_call();
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

// Execute procedure body and handle tail calls via trampoline
Result proc_call(Evaluator *eval, UserProcedure *proc, int argc, Value *args)
{
    // Trampoline loop for tail call optimization
    while (true)
    {
        // Validate argument count
        if (argc < proc->param_count)
        {
            return result_error_arg(ERR_NOT_ENOUGH_INPUTS, proc->name, NULL);
        }
        if (argc > proc->param_count)
        {
            return result_error_arg(ERR_TOO_MANY_INPUTS, proc->name, NULL);
        }

        // Push new scope for local variables
        var_push_scope();

        // Bind arguments to parameter names as local variables
        for (int i = 0; i < proc->param_count; i++)
        {
            var_set_local(proc->params[i], args[i]);
        }

        // Clear any pending tail call
        proc_clear_tail_call();

        // Track procedure depth for TCO detection
        eval->proc_depth++;

        // Execute the body with TCO enabled
        Result result = eval_run_list_with_tco(eval, proc->body, true);

        // Restore depth
        eval->proc_depth--;

        // Pop scope before handling tail call
        var_pop_scope();

        // Check for tail call
        TailCall *tc = proc_get_tail_call();
        if (tc->is_tail_call)
        {
            // Find the target procedure
            UserProcedure *target = proc_find(tc->proc_name);
            if (target)
            {
                // Set up for next iteration (tail call)
                proc = target;
                argc = tc->arg_count;
                // Copy args from tail call state
                for (int i = 0; i < argc; i++)
                {
                    args[i] = tc->args[i];
                }
                proc_clear_tail_call();
                continue; // Trampoline: restart loop instead of recursing
            }
            // Target not found - this would be caught during normal eval
            proc_clear_tail_call();
        }

        // Handle result
        if (result.status == RESULT_STOP)
        {
            // stop command - procedure returns nothing
            return result_none();
        }
        if (result.status == RESULT_OUTPUT)
        {
            // output command - procedure returns a value
            return result_ok(result.value);
        }
        if (result.status == RESULT_ERROR)
        {
            return result;
        }
        if (result.status == RESULT_THROW)
        {
            // throw - propagate to caller
            return result;
        }

        // Normal completion (no stop/output)
        return result_none();
    }
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
