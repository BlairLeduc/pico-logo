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
#include "compiler.h"
#include "vm.h"
#include "frame.h"
#include "devices/io.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

// Maximum number of user-defined procedures
#define MAX_PROCEDURES 128

// Maximum procedure call stack depth (for current proc tracking)
#define MAX_CURRENT_PROC_DEPTH 32

// Maximum recursion depth to prevent C stack overflow
// This limits how deep non-tail-recursive calls can go.
// On RP2350 with ~8KB stack and ~200 bytes per C call frame, this is conservative.
// Tail-recursive calls don't count against this limit (they reuse frames).
#ifndef MAX_RECURSION_DEPTH
#define MAX_RECURSION_DEPTH 128
#endif

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

// Helper to serialize a node element to buffer (for step display)
// Returns number of characters written, or 0 if buffer too small
static int serialize_step_node(Node elem, char *buf, int max_len, bool need_space)
{
    int pos = 0;
    
    // Need at least 1 char for null terminator
    if (max_len < 1)
        return 0;
    
    if (mem_is_word(elem))
    {
        const char *str = mem_word_ptr(elem);
        size_t len = mem_word_len(elem);
        int space_needed = (need_space ? 1 : 0) + (int)len;
        
        if (pos + space_needed < max_len)
        {
            if (need_space)
                buf[pos++] = ' ';
            memcpy(buf + pos, str, len);
            pos += (int)len;
        }
    }
    else if (mem_is_nil(elem))
    {
        // Empty list: need space for " []" (up to 3 chars)
        int space_needed = (need_space ? 1 : 0) + 2;
        if (pos + space_needed < max_len)
        {
            if (need_space)
                buf[pos++] = ' ';
            buf[pos++] = '[';
            buf[pos++] = ']';
        }
    }
    else if (mem_is_list(elem))
    {
        // Need at least " []" (up to 3 chars minimum)
        int space_needed = (need_space ? 1 : 0) + 2;
        if (pos + space_needed >= max_len)
            return pos;
        
        if (need_space)
            buf[pos++] = ' ';
        buf[pos++] = '[';
        
        Node inner = elem;
        bool first = true;
        while (!mem_is_nil(inner) && pos < max_len - 2)  // Reserve space for ']'
        {
            int written = serialize_step_node(mem_car(inner), buf + pos, max_len - pos - 1, !first);
            pos += written;
            first = false;
            inner = mem_cdr(inner);
        }
        
        if (pos < max_len)
            buf[pos++] = ']';
    }
    
    return pos;
}

static bool line_has_user_calls_or_labels(Node line_tokens)
{
    Node curr = line_tokens;
    while (!mem_is_nil(curr))
    {
        Node elem = mem_car(curr);
        if (mem_is_word(elem))
        {
            const char *word = mem_word_ptr(elem);
            (void)word;
        }
        curr = mem_cdr(curr);
    }
    return false;
}

static Node find_label_after(Node body, const char *label_name)
{
    Node search = body;
    while (!mem_is_nil(search))
    {
        Node search_line = mem_car(search);
        Node search_tokens = search_line;
        if (NODE_GET_TYPE(search_line) == NODE_TYPE_LIST)
        {
            search_tokens = NODE_MAKE_LIST(NODE_GET_INDEX(search_line));
        }

        if (!mem_is_nil(search_tokens))
        {
            Node first = mem_car(search_tokens);
            if (mem_is_word(first))
            {
                const char *word = mem_word_ptr(first);
                if (strcasecmp(word, "label") == 0)
                {
                    Node rest = mem_cdr(search_tokens);
                    if (!mem_is_nil(rest))
                    {
                        Node label_arg = mem_car(rest);
                        if (mem_is_word(label_arg))
                        {
                            const char *arg = mem_word_ptr(label_arg);
                            if (arg[0] == '"')
                                arg++;
                            if (strcasecmp(arg, label_name) == 0)
                                return mem_cdr(search);
                        }
                    }
                }
            }
        }
        search = mem_cdr(search);
    }

    return NODE_NIL;
}

static bool body_can_use_vm(Node body)
{
    Node curr = body;
    while (!mem_is_nil(curr))
    {
        Node line = mem_car(curr);
        Node line_tokens = line;
        if (NODE_GET_TYPE(line) == NODE_TYPE_LIST)
        {
            line_tokens = NODE_MAKE_LIST(NODE_GET_INDEX(line));
        }
        if (!mem_is_nil(line_tokens) && line_has_user_calls_or_labels(line_tokens))
            return false;
        curr = mem_cdr(curr);
    }
    return true;
}

static Result execute_body_vm(Evaluator *eval, Node body, bool enable_tco)
{
    Node curr = body;
    Result r = result_none();

    while (!mem_is_nil(curr))
    {
        Node line = mem_car(curr);
        Node next = mem_cdr(curr);
        bool is_last_line = mem_is_nil(next);

        Node line_tokens = line;
        if (NODE_GET_TYPE(line) == NODE_TYPE_LIST)
        {
            line_tokens = NODE_MAKE_LIST(NODE_GET_INDEX(line));
        }

        if (mem_is_nil(line_tokens))
        {
            curr = next;
            continue;
        }

        LogoIO *io = primitives_get_io();
        if (io && (logo_io_check_user_interrupt(io) ||
                   logo_io_check_freeze_request(io) ||
                   logo_io_check_pause_request(io)))
        {
            r = eval_run_list_with_tco(eval, line_tokens, enable_tco && is_last_line);
        }
        else
        {
            Instruction code_buf[256];
            Value const_buf[64];
            Bytecode bc = {
                .code = code_buf,
                .code_len = 0,
                .code_cap = sizeof(code_buf) / sizeof(code_buf[0]),
                .const_pool = const_buf,
                .const_len = 0,
                .const_cap = sizeof(const_buf) / sizeof(const_buf[0]),
                .arena = NULL
            };
            bc_init(&bc, NULL);

            Compiler c = {
                .eval = eval,
                .bc = &bc,
                .instruction_mode = true,
                .tail_position = false,
                .tail_depth = 0
            };

            Result cr = compile_list_instructions(&c, line_tokens, &bc, enable_tco && is_last_line);
            if (cr.status != RESULT_NONE && cr.status != RESULT_OK)
                return cr;

            bool saved_tail = eval->in_tail_position;
            VM vm;
            vm_init(&vm);
            vm.eval = eval;
            r = vm_exec(&vm, &bc);
            eval->in_tail_position = saved_tail;
        }

        if (r.status == RESULT_CALL)
        {
            FrameStack *frames = eval->frames;
            if (frames && !frame_stack_is_empty(frames))
            {
                FrameHeader *frame = frame_current(frames);
                if (frame)
                {
                    frame->body_cursor = curr;
                    frame->line_cursor = NODE_NIL;
                }
            }
            return r;
        }

        if (r.status == RESULT_GOTO)
            return r;

        if (r.status != RESULT_NONE && r.status != RESULT_OK)
            return r;

        if (r.status == RESULT_OK)
            return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(r.value));

        curr = next;
    }

    return r;
}

// Helper to execute procedure body with step support
// Body is a list of line-lists: [[line1-tokens] [line2-tokens] ...]
// Returns Result from execution
static Result execute_body_with_step(Evaluator *eval, Node body, bool enable_tco, bool stepped)
{
    LogoIO *io = stepped ? primitives_get_io() : NULL;
    Result r = result_none();
    
    // Check if we're resuming from a saved continuation
    FrameStack *frames = eval->frames;
    Node curr;
    bool is_continuation = false;
    
    if (frames && !frame_stack_is_empty(frames))
    {
        FrameHeader *frame = frame_current(frames);
        if (frame && !mem_is_nil(frame->body_cursor))
        {
            is_continuation = true;
            // Resuming from a continuation - start from saved position
            // The body_cursor points to the line that had the call
            // We need to resume from the NEXT line (call already completed)
            curr = mem_cdr(frame->body_cursor);
            
            // Also check if there's a saved line_cursor for mid-line resumption
            if (!mem_is_nil(frame->line_cursor))
            {
                // Resume mid-line - execute remaining tokens on the saved line
                Node saved_line = mem_car(frame->body_cursor);
                Node line_tokens = saved_line;
                if (NODE_GET_TYPE(saved_line) == NODE_TYPE_LIST)
                {
                    line_tokens = NODE_MAKE_LIST(NODE_GET_INDEX(saved_line));
                }
                
                // We need to resume from line_cursor position
                // For now, just continue to the next line
                // (Full mid-line resumption would need eval changes)
            }
            
            // Clear the continuation state since we're now resuming
            frame->body_cursor = NODE_NIL;
            frame->line_cursor = NODE_NIL;
        }
        else
        {
            // Fresh execution - start from the beginning
            curr = body;
        }
    }
    else
    {
        // No frame (shouldn't happen during normal execution) - start from beginning
        curr = body;
    }
    
    // Use VM for full body execution when safe (no stepping, no continuation, no control-flow)
#if EVAL_USE_VM_BODY
    if (!stepped && !is_continuation && body_can_use_vm(body))
    {
        r = execute_body_vm(eval, body, enable_tco);
        if (r.status == RESULT_GOTO)
        {
            Node after = find_label_after(body, r.goto_label);
            if (mem_is_nil(after))
                return result_error_arg(ERR_CANT_FIND_LABEL, NULL, r.goto_label);
            curr = after;
        }
        else
        {
            return r;
        }
    }
#endif

    // Iterate through body - each element is a line (a list of tokens)
    while (!mem_is_nil(curr))
    {
        Node line = mem_car(curr);
        Node next = mem_cdr(curr);
        bool is_last_line = mem_is_nil(next);
        
        // Handle line - it could be marked as LIST type or be a direct cons cell
        Node line_tokens = line;
        if (NODE_GET_TYPE(line) == NODE_TYPE_LIST)
        {
            // It's marked as a list reference
            line_tokens = NODE_MAKE_LIST(NODE_GET_INDEX(line));
        }
        
        // Skip empty lines during execution
        if (mem_is_nil(line_tokens))
        {
            curr = next;
            continue;
        }
        
        if (stepped)
        {
            // Stepped execution: print line, wait for key, then execute
            char line_buf[512];
            int pos = 0;
            bool first = true;
            Node n = line_tokens;
            
            while (!mem_is_nil(n) && pos < (int)sizeof(line_buf) - 1)
            {
                Node elem = mem_car(n);
                int remaining = (int)sizeof(line_buf) - pos - 1;
                if (remaining <= 0)
                    break;
                
                int written = serialize_step_node(elem, line_buf + pos, remaining, !first);
                pos += written;
                first = false;
                n = mem_cdr(n);
            }
            line_buf[pos] = '\0';
            
            // Print the line
            if (io && pos > 0)
            {
                logo_io_write(io, line_buf);
                logo_io_write(io, "\n");
                logo_io_flush(io);
                
                // Wait for a key press
                int ch = logo_io_read_char(io);
                if (ch == LOGO_STREAM_INTERRUPTED)
                {
                    return result_error(ERR_STOPPED);
                }
            }
        }
        
        // Execute this line
        r = eval_run_list_with_tco(eval, line_tokens, enable_tco && is_last_line);
        
        // Handle RESULT_CALL - save continuation state in frame and propagate
        if (r.status == RESULT_CALL)
        {
            // Save current line position for resumption after the call returns
            // line_cursor was already saved by eval_run_list_with_tco
            FrameStack *frames = eval->frames;
            if (frames && !frame_stack_is_empty(frames))
            {
                FrameHeader *frame = frame_current(frames);
                if (frame)
                {
                    // Save the current line (so we can resume here after call returns)
                    frame->body_cursor = curr;
                }
            }
            return r;  // Propagate to proc_call's main loop
        }
        
        // Handle RESULT_GOTO - search for label in body and restart from there
        if (r.status == RESULT_GOTO)
        {
            Node after = find_label_after(body, r.goto_label);
            if (mem_is_nil(after))
                return result_error_arg(ERR_CANT_FIND_LABEL, NULL, r.goto_label);
            curr = after;
            r = result_none();
            continue;  // Continue from the line after the label
        }
        
        // Handle result - propagate stop/output/error/throw immediately
        if (r.status != RESULT_NONE && r.status != RESULT_OK)
        {
            return r;
        }
        
        // If we got a value at top level of list, it's an error
        if (r.status == RESULT_OK)
        {
            return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(r.value));
        }
        
        curr = next;
    }
    
    return r;
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
// PROCEDURE EXECUTION WITH CPS AND TAIL-CALL OPTIMIZATION
//==========================================================================
//
// The trampoline loop implements both:
// 1. Tail-call optimization (TCO): reuse frame for tail calls
// 2. Continuation-passing style (CPS): handle nested calls without C recursion
//
// ALGORITHM:
// 1. Push frame for initial procedure
// 2. Execute procedure body
// 3. Check result:
//    - RESULT_CALL: push new frame for callee, continue loop (CPS)
//    - Tail call (RESULT_STOP + tc->is_tail_call): frame_reuse, restart (TCO)
//    - RESULT_STOP/RESULT_OUTPUT: pop frame, check for parent to resume
//    - Error/throw: pop frame, propagate
// 4. When resuming parent frame: continue from saved body_cursor/line_cursor
//
// BENEFITS:
// - Deep recursion limited only by frame stack memory, not C stack
// - Tail-recursive procedures run indefinitely (frame reuse)
// - proc_depth tracks logical depth for tracing
//

Result proc_call(Evaluator *eval, UserProcedure *proc, int argc, Value *args)
{
    FrameStack *frames = eval->frames;
    bool is_tail_call = false;        // True if this iteration is a tail call
    bool is_continuation = false;     // True if resuming from a saved continuation
    Value return_value = value_none(); // Return value from callee (for continuation)
    
    // Trampoline loop for both TCO and CPS
    while (true)
    {
        // Validate argument count (not needed for continuations)
        if (!is_continuation)
        {
            if (argc < proc->param_count)
            {
                return result_error_arg(ERR_NOT_ENOUGH_INPUTS, proc->name, NULL);
            }
            if (argc > proc->param_count)
            {
                return result_error_arg(ERR_TOO_MANY_INPUTS, proc->name, NULL);
            }
        }

        if (!is_tail_call && !is_continuation)
        {
            // New call: push a new frame
            word_offset_t frame_offset = frame_push(frames, proc, args, argc);
            if (frame_offset == OFFSET_NONE)
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
            
            // Track procedure depth for TCO detection and tracing
            eval->proc_depth++;
            
            // Track current procedure name (for pause prompt)
            proc_push_current(proc->name);
        }
        else if (is_tail_call)
        {
            // Tail call: try to reuse the current frame
            proc_pop_current();
            proc_push_current(proc->name);
            
            if (!frame_reuse(frames, proc, args, argc))
            {
                // Reuse failed - fall back to pop+push
                frame_pop(frames);
                word_offset_t frame_offset = frame_push(frames, proc, args, argc);
                if (frame_offset == OFFSET_NONE)
                {
                    return result_error(ERR_OUT_OF_SPACE);
                }
            }
            // Note: proc_depth stays the same for tail calls
        }
        // For is_continuation: we're resuming, frame is already set up

        // Clear tail call state
        proc_clear_tail_call();
        is_tail_call = false;
        is_continuation = false;

        // Trace entry if traced (only on fresh entry, not continuation)
        FrameHeader *frame = frame_current(frames);
        if (proc->traced && mem_is_nil(frame->body_cursor))
        {
            // Fresh entry (body_cursor is NIL) - trace entry
            // Print indentation (2 spaces per depth level)
            for (int i = 0; i < eval->proc_depth; i++)
            {
                trace_write("  ");
            }
            trace_write(proc->name);
            
            // Print arguments
            Binding *bindings = frame_get_bindings(frame);
            for (int i = 0; i < frame->param_count; i++)
            {
                trace_write(" ");
                char buf[64];
                Value v = bindings[i].value;
                switch (v.type)
                {
                case VALUE_NUMBER:
                    format_number(buf, sizeof(buf), v.as.number);
                    trace_write(buf);
                    break;
                case VALUE_WORD:
                    trace_write(mem_word_ptr(v.as.node));
                    break;
                case VALUE_LIST:
                    trace_write("[");
                    {
                        bool first = true;
                        Node curr = v.as.node;
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

        // Execute the body (or resume from continuation)
        Result result = execute_body_with_step(eval, proc->body, true, proc->stepped);

        // Handle RESULT_CALL (CPS nested call)
        if (result.status == RESULT_CALL)
        {
            // Push new frame for the callee and continue
            // The current frame's body_cursor/line_cursor were saved by execute_body_with_step
            proc = result.call_proc;
            argc = result.call_argc;
            for (int i = 0; i < argc; i++)
            {
                args[i] = result.call_args[i];
            }
            // Not a tail call, not a continuation - it's a new nested call
            continue;
        }

        // Trace exit if traced
        if (proc->traced)
        {
            for (int i = 0; i < eval->proc_depth; i++)
            {
                trace_write("  ");
            }
            
            if (result.status == RESULT_OUTPUT)
            {
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
                trace_write(proc->name);
                trace_write(" stopped\n");
            }
        }

        // Check for tail call BEFORE cleanup
        // IMPORTANT: Only apply TCO (frame reuse) for SELF-RECURSIVE tail calls.
        // Non-self-recursive tail calls must use the CPS path instead to preserve
        // dynamic scoping - callee needs access to caller's local variables.
        TailCall *tc = proc_get_tail_call();
        if (tc->is_tail_call)
        {
            UserProcedure *target = proc_find(tc->proc_name);
            if (target && target == proc)
            {
                // Self-recursive tail call - safe to reuse frame
                // (callee is same procedure, so it has same parameters)
                argc = tc->arg_count;
                for (int i = 0; i < argc; i++)
                {
                    args[i] = tc->args[i];
                }
                proc_clear_tail_call();
                is_tail_call = true;
                continue; // Trampoline: restart loop with frame reuse
            }
            // Non-self-recursive tail call - fall through to normal return
            // The tail call will be handled via CPS on the next iteration
            proc_clear_tail_call();
        }

        // Procedure completed - pop frame
        eval->proc_depth--;
        proc_pop_current();
        
        // Get parent frame before popping (to check for continuation)
        FrameHeader *current_frame = frame_current(frames);
        word_offset_t parent_offset = current_frame ? current_frame->prev_offset : OFFSET_NONE;
        
        frame_pop(frames);

        // Determine final result for this procedure
        Value proc_result = value_none();
        bool has_result = false;
        
        if (result.status == RESULT_STOP)
        {
            // stop command - no value
        }
        else if (result.status == RESULT_OUTPUT)
        {
            proc_result = result.value;
            has_result = true;
        }
        else if (result.status == RESULT_ERROR)
        {
            return result_error_in(result, proc->name);
        }
        else if (result.status == RESULT_THROW)
        {
            return result;
        }

        // Check if we should resume a parent frame (CPS continuation)
        if (parent_offset != OFFSET_NONE)
        {
            FrameHeader *parent = frame_at(frames, parent_offset);
            
            // Check if parent has a saved continuation (body_cursor set)
            if (parent && !mem_is_nil(parent->body_cursor))
            {
                // Resume parent procedure from saved position
                proc = parent->proc;
                
                // If callee returned a value, the caller might need it
                // For now, we don't support using the return value mid-expression
                // (that would require storing it on the value stack)
                // Just report error if value was returned and not consumed
                if (has_result)
                {
                    return result_error_arg(ERR_DONT_KNOW_WHAT, NULL, value_to_string(proc_result));
                }
                
                // Set up for continuation
                is_continuation = true;
                continue;
            }
        }

        // No parent or no continuation to resume - return to original caller
        if (has_result)
        {
            return result_ok(proc_result);
        }
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

// Reset all procedure execution state
// Call this after errors or when returning to top level unexpectedly
void proc_reset_execution_state(void)
{
    proc_clear_tail_call();
    current_proc_depth = 0;
    frame_stack_reset(&global_frame_stack);
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
