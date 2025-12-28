//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "procedures.h"
#include "eval.h"
#include "variables.h"
#include "error.h"
#include "memory.h"
#include "primitives.h"
#include "devices/io.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

// Maximum number of user-defined procedures
#define MAX_PROCEDURES 128

// Maximum procedure call stack depth (for current proc tracking)
#define MAX_CURRENT_PROC_DEPTH 32

// Procedure storage
static UserProcedure procedures[MAX_PROCEDURES];
static int procedure_count = 0;

// Tail call state (global for trampoline)
static TailCall tail_call_state;

// Current procedure stack (for pause prompt)
static const char *current_proc_stack[MAX_CURRENT_PROC_DEPTH];
static int current_proc_depth = 0;

// Helper to write output for trace/step
static void trace_write(const char *str)
{
    LogoIO *io = primitives_get_io();
    if (io)
    {
        logo_io_write(io, str);
    }
}

// Helper to print a single node element
static void print_node_element(Node elem)
{
    if (mem_is_word(elem))
    {
        trace_write(mem_word_ptr(elem));
    }
    else if (mem_is_list(elem))
    {
        trace_write("[");
        bool first = true;
        Node curr = elem;
        while (!mem_is_nil(curr))
        {
            if (!first)
                trace_write(" ");
            first = false;
            print_node_element(mem_car(curr));
            curr = mem_cdr(curr);
        }
        trace_write("]");
    }
}

// Helper to execute procedure body with step support
// Returns Result from execution
static Result execute_body_with_step(Evaluator *eval, Node body, bool enable_tco, bool stepped)
{
    if (!stepped)
    {
        // Normal execution without stepping
        return eval_run_list_with_tco(eval, body, enable_tco);
    }
    
    // Stepped execution - for simplicity, just execute normally
    // The stepping behavior would require more sophisticated instruction tracking
    // For now, just execute without step (user can use trace instead)
    // TODO: Implement proper step-through execution
    return eval_run_list_with_tco(eval, body, enable_tco);
}

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
        if (!var_push_scope())
        {
            // Out of scope space - this means TCO isn't working as expected
            return result_error(ERR_OUT_OF_SPACE);
        }

        // Bind arguments to parameter names as local variables
        for (int i = 0; i < proc->param_count; i++)
        {
            var_set_local(proc->params[i], args[i]);
        }

        // Clear any pending tail call
        proc_clear_tail_call();

        // Track procedure depth for TCO detection
        eval->proc_depth++;

        // Track current procedure name (for pause prompt)
        proc_push_current(proc->name);

        // Trace entry if traced
        if (proc->traced)
        {
            // Print indentation (2 spaces per depth level)
            for (int i = 0; i < eval->proc_depth; i++)
            {
                trace_write("  ");
            }
            trace_write(proc->name);
            
            // Print arguments
            for (int i = 0; i < argc; i++)
            {
                trace_write(" ");
                char buf[64];
                switch (args[i].type)
                {
                case VALUE_NUMBER:
                    format_number(buf, sizeof(buf), args[i].as.number);
                    trace_write(buf);
                    break;
                case VALUE_WORD:
                    trace_write(mem_word_ptr(args[i].as.node));
                    break;
                case VALUE_LIST:
                    trace_write("[");
                    {
                        bool first = true;
                        Node curr = args[i].as.node;
                        while (!mem_is_nil(curr))
                        {
                            if (!first)
                                trace_write(" ");
                            first = false;
                            print_node_element(mem_car(curr));
                            curr = mem_cdr(curr);
                        }
                    }
                    trace_write("]");
                    break;
                default:
                    break;
                }
            }
            trace_write("\n");
        }

        // Execute the body with step support if needed
        Result result = execute_body_with_step(eval, proc->body, true, proc->stepped);

        // Trace exit if traced
        if (proc->traced)
        {
            // Print indentation
            for (int i = 0; i < eval->proc_depth; i++)
            {
                trace_write("  ");
            }
            
            if (result.status == RESULT_OUTPUT)
            {
                // Print return value
                char buf[64];
                switch (result.value.type)
                {
                case VALUE_NUMBER:
                    format_number(buf, sizeof(buf), result.value.as.number);
                    trace_write(buf);
                    break;
                case VALUE_WORD:
                    trace_write(mem_word_ptr(result.value.as.node));
                    break;
                case VALUE_LIST:
                    trace_write("[...]");
                    break;
                default:
                    break;
                }
                trace_write("\n");
            }
            else
            {
                // Print "stopped"
                trace_write(proc->name);
                trace_write(" stopped\n");
            }
        }

        // Restore depth
        eval->proc_depth--;

        // Pop current procedure name
        proc_pop_current();

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
            // Propagate throws (e.g., throw "toplevel from pause)
            return result;
        }

        // Normal completion (no stop/output)
        return result_none();
    }
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
