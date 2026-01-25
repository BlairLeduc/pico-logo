//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "eval.h"
#include "error.h"
#include "primitives.h"
#include "procedures.h"
#include "repl.h"
#include "variables.h"
#include "frame.h"
#include "devices/io.h"
#include "compiler.h"
#include "vm.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>


void eval_init(Evaluator *eval, Lexer *lexer)
{
    token_source_init_lexer(&eval->token_source, lexer);
    eval->frames = NULL;  // Caller sets this if needed
    eval->paren_depth = 0;
    eval->error_code = 0;
    eval->error_context = NULL;
    eval->in_tail_position = false;
    eval->proc_depth = 0;
    eval->repcount = -1;
    eval->primitive_arg_depth = 0;
}

void eval_set_frames(Evaluator *eval, FrameStack *frames)
{
    eval->frames = frames;
}

FrameStack *eval_get_frames(Evaluator *eval)
{
    return eval->frames;
}

bool eval_in_procedure(Evaluator *eval)
{
    return eval->frames != NULL && !frame_stack_is_empty(eval->frames);
}

// Get current token, fetching if needed
static Token peek(Evaluator *eval)
{
    return token_source_peek(&eval->token_source);
}

// Consume current token
static void advance(Evaluator *eval)
{
    // Consume the peeked token by getting next
    token_source_next(&eval->token_source);
}

// Check if at end of input
bool eval_at_end(Evaluator *eval)
{
    Token t = peek(eval);
    return t.type == TOKEN_EOF;
}


// Try to parse a number from a string
static bool is_number_string(const char *str, size_t len)
{
    if (len == 0)
        return false;
    size_t i = 0;

    if (str[i] == '-' || str[i] == '+')
        i++;
    if (i >= len)
        return false;

    bool has_digit = false;
    while (i < len && isdigit((unsigned char)str[i]))
    {
        has_digit = true;
        i++;
    }
    if (i < len && str[i] == '.')
    {
        i++;
        while (i < len && isdigit((unsigned char)str[i]))
        {
            has_digit = true;
            i++;
        }
    }
    if (i < len && (str[i] == 'e' || str[i] == 'E' || str[i] == 'n' || str[i] == 'N'))
    {
        i++;
        if (i < len && (str[i] == '-' || str[i] == '+'))
            i++;
        while (i < len && isdigit((unsigned char)str[i]))
            i++;
    }
    return has_digit && i == len;
}

static float parse_number(const char *str, size_t len)
{
    // Create null-terminated copy for strtof
    char buf[64];
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, str, len);
    buf[len] = '\0';

    // Handle 'n' notation: 1n4 = 0.0001
    char *n_pos = strchr(buf, 'n');
    if (!n_pos)
        n_pos = strchr(buf, 'N');

    if (n_pos)
    {
        *n_pos = '\0';
        float mantissa = strtof(buf, NULL);
        float exp = strtof(n_pos + 1, NULL);
        float result = mantissa;
        for (int i = 0; i < (int)exp; i++)
        {
            result /= 10.0f;
        }
        return result;
    }
    return strtof(buf, NULL);
}

Result eval_expression(Evaluator *eval)
{
    if (eval_at_end(eval))
    {
        return result_none();
    }
    TokenSource saved_source;
    token_source_copy(&saved_source, &eval->token_source);
    Lexer saved_lexer;
    bool saved_lexer_valid = false;
    if (eval->token_source.type == TOKEN_SOURCE_LEXER && eval->token_source.lexer)
    {
        saved_lexer = *eval->token_source.lexer;
        saved_lexer_valid = true;
    }
    int saved_paren_depth = eval->paren_depth;
    int saved_primitive_depth = eval->primitive_arg_depth;

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
        .instruction_mode = false
    };

    Result cr = compile_expression(&c, &bc);
    if (cr.status != RESULT_OK)
    {
        token_source_copy(&eval->token_source, &saved_source);
        if (saved_lexer_valid && eval->token_source.type == TOKEN_SOURCE_LEXER && eval->token_source.lexer)
        {
            *eval->token_source.lexer = saved_lexer;
        }
        eval->paren_depth = saved_paren_depth;
        eval->primitive_arg_depth = saved_primitive_depth;
        return cr;
    }

    VM vm;
    vm_init(&vm);
    vm.eval = eval;
    return vm_exec(&vm, &bc);
}

Result eval_instruction(Evaluator *eval)
{
    // Check for user interrupt at the start of each instruction
    LogoIO *io = primitives_get_io();
    if (io && logo_io_check_user_interrupt(io))
    {
        return result_error(ERR_STOPPED);
    }

    // Check for freeze request (F4 key) - temporarily pause until key pressed
    if (io && logo_io_check_freeze_request(io))
    {
        // Wait for any key to be pressed (except F4)
        // During freeze: F9 triggers pause, Brk stops execution, F4 is ignored
        bool freeze_done = false;
        while (!freeze_done)
        {
            // Check for user interrupt (Brk) while waiting
            if (logo_io_check_user_interrupt(io))
            {
                return result_error(ERR_STOPPED);
            }
            // Check for pause request (F9) while waiting
            if (logo_io_check_pause_request(io))
            {
                const char *proc_name = proc_get_current();
                if (proc_name == NULL && eval->frames && !frame_stack_is_empty(eval->frames))
                {
                    FrameHeader *frame = frame_current(eval->frames);
                    if (frame && frame->proc)
                    {
                        proc_name = frame->proc->name;
                    }
                }
                if (proc_name != NULL)
                {
                    logo_io_clear_pause_request(io);
                    logo_io_write_line(io, "Pausing...");
                    
                    ReplState state;
                    repl_init(&state, io, REPL_FLAGS_PAUSE, proc_name);
                    Result r = repl_run(&state);
                    
                    if (r.status != RESULT_OK && r.status != RESULT_NONE)
                    {
                        return r;
                    }
                }
                // After pause REPL exits, continue execution
                freeze_done = true;
            }
            // Eat any additional F4 presses while frozen
            else if (logo_io_check_freeze_request(io))
            {
                // Just consume it and keep waiting
            }
            else if (logo_io_key_available(io))
            {
                // Consume the key that ended the freeze
                logo_io_read_char(io);
                freeze_done = true;
            }
            else
            {
                logo_io_sleep(io, 10);  // Small sleep to avoid busy waiting
            }
        }
    }

    // Check for pause request (F9 key) - only works inside a procedure
    // We check if the flag is set, but only clear it if we actually pause
    if (io && logo_io_check_pause_request(io))
    {
        const char *proc_name = proc_get_current();
        if (proc_name == NULL && eval->frames && !frame_stack_is_empty(eval->frames))
        {
            FrameHeader *frame = frame_current(eval->frames);
            if (frame && frame->proc)
            {
                proc_name = frame->proc->name;
            }
        }
        if (proc_name != NULL)
        {
            // Clear the flag now that we're actually pausing
            logo_io_clear_pause_request(io);
            
            // Run the pause REPL - this blocks until co is called or throw "toplevel
            logo_io_write_line(io, "Pausing...");
            
            ReplState state;
            repl_init(&state, io, REPL_FLAGS_PAUSE, proc_name);
            Result r = repl_run(&state);
            
            // If pause REPL exited with throw or error, propagate it
            if (r.status != RESULT_OK && r.status != RESULT_NONE)
            {
                return r;
            }
            // Otherwise continue execution
        }
        // Don't clear flag if at top level - defer to when we're inside a procedure
    }

    if (eval_at_end(eval))
    {
        return result_none();
    }
    TokenSource saved_source;
    token_source_copy(&saved_source, &eval->token_source);
    Lexer saved_lexer;
    bool saved_lexer_valid = false;
    if (eval->token_source.type == TOKEN_SOURCE_LEXER && eval->token_source.lexer)
    {
        saved_lexer = *eval->token_source.lexer;
        saved_lexer_valid = true;
    }
    int saved_paren_depth = eval->paren_depth;
    int saved_primitive_depth = eval->primitive_arg_depth;

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
        .instruction_mode = true
    };

    Result cr = compile_instruction(&c, &bc);
    if (cr.status == RESULT_OK)
    {
        VM vm;
        vm_init(&vm);
        vm.eval = eval;
        return vm_exec(&vm, &bc);
    }

    token_source_copy(&eval->token_source, &saved_source);
    if (saved_lexer_valid && eval->token_source.type == TOKEN_SOURCE_LEXER && eval->token_source.lexer)
    {
        *eval->token_source.lexer = saved_lexer;
    }
    eval->paren_depth = saved_paren_depth;
    eval->primitive_arg_depth = saved_primitive_depth;
    return cr;
}

// Evaluate a list as procedure body with tail call optimization
// The last instruction in the list is evaluated in tail position
Result eval_run_list_with_tco(Evaluator *eval, Node list, bool enable_tco)
{
    TokenSource saved_source;
    token_source_copy(&saved_source, &eval->token_source);
    int saved_paren_depth = eval->paren_depth;
    bool saved_tail = eval->in_tail_position;

    token_source_init_list(&eval->token_source, list);

    Result r = result_none();
    while (!token_source_at_end(&eval->token_source))
    {
        if (enable_tco)
        {
            TokenSource lookahead = eval->token_source;
            bool last_instruction = compiler_skip_instruction(&lookahead) && token_source_at_end(&lookahead);
            eval->in_tail_position = last_instruction;
        }
        else
        {
            eval->in_tail_position = false;
        }

        r = eval_instruction(eval);

        if (r.status == RESULT_CALL)
        {
            FrameStack *frames = eval->frames;
            if (frames && !frame_stack_is_empty(frames))
            {
                FrameHeader *frame = frame_current(frames);
                if (frame)
                {
                    frame->line_cursor = token_source_get_position(&eval->token_source);
                }
            }
            break;
        }

        if (r.status == RESULT_GOTO)
            break;

        if (r.status != RESULT_NONE && r.status != RESULT_OK)
            break;

        if (r.status == RESULT_OK)
            break;
    }

    token_source_copy(&eval->token_source, &saved_source);
    eval->paren_depth = saved_paren_depth;
    eval->in_tail_position = saved_tail;
    return r;
}

Result eval_run_list(Evaluator *eval, Node list)
{
    // Increment primitive_arg_depth to disable CPS during this call
    // This ensures nested procedure calls complete before returning to caller
    // (CPS would return RESULT_CALL which primitives don't handle)
    eval->primitive_arg_depth++;
    Result r = eval_run_list_with_tco(eval, list, false);
    eval->primitive_arg_depth--;
    return r;
}

// Run a list as an expression - allows the list to output a value
// Used by 'run' and 'if' when they act as operations
// Propagates tail position for TCO
Result eval_run_list_expr(Evaluator *eval, Node list)
{
    bool enable_tco = eval->in_tail_position && eval->proc_depth > 0;
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
        .instruction_mode = false
    };

    bool saved_tail = eval->in_tail_position;
    eval->primitive_arg_depth++;
    Result cr = compile_list_instructions_expr(&c, list, &bc, enable_tco);
    if (cr.status == RESULT_NONE || cr.status == RESULT_OK)
    {
        VM vm;
        vm_init(&vm);
        vm.eval = eval;
        Result r = vm_exec(&vm, &bc);
        eval->primitive_arg_depth--;
        eval->in_tail_position = saved_tail;
        return r;
    }
    eval->primitive_arg_depth--;
    eval->in_tail_position = saved_tail;
    return cr;
}
