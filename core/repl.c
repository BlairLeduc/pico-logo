//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  REPL (Read-Eval-Print Loop) shared implementation
//

#include "core/repl.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "core/error.h"
#include "core/eval.h"
#include "core/lexer.h"
#include "core/memory.h"
#include "core/primitives.h"
#include "core/procedures.h"
#include "devices/stream.h"

// Forward declaration for pause continue check
extern bool pause_check_continue(void);

// Helper functions

bool repl_line_starts_with_to(const char *line)
{
    // Skip leading whitespace
    while (*line && isspace((unsigned char)*line))
        line++;
    
    if (strncasecmp(line, "to", 2) != 0)
        return false;
    
    // Must be followed by whitespace or end of line
    char c = line[2];
    return c == '\0' || isspace((unsigned char)c);
}

bool repl_line_is_end(const char *line)
{
    // Skip leading whitespace
    while (*line && isspace((unsigned char)*line))
        line++;
    
    if (strncasecmp(line, "end", 3) != 0)
        return false;
    
    // Must be followed by whitespace or end of string
    line += 3;
    
    // Skip any trailing whitespace
    while (*line && isspace((unsigned char)*line))
        line++;
    
    // Line must be empty after "end" and whitespace
    return *line == '\0';
}

const char *repl_extract_proc_name(const char *line, char *buffer, size_t buffer_size)
{
    // Skip leading whitespace
    while (*line && isspace((unsigned char)*line))
        line++;
    
    // Skip "to"
    line += 2;
    
    // Skip whitespace after "to"
    while (*line && isspace((unsigned char)*line))
        line++;
    
    if (*line == '\0')
        return NULL;  // No name provided
    
    // Copy name until whitespace or end
    size_t i = 0;
    while (*line && !isspace((unsigned char)*line) && i < buffer_size - 1)
    {
        buffer[i++] = *line++;
    }
    buffer[i] = '\0';
    
    return i > 0 ? buffer : NULL;
}

int repl_count_bracket_balance(const char *line)
{
    int balance = 0;
    for (const char *p = line; *p; p++)
    {
        if (*p == '[') balance++;
        else if (*p == ']') balance--;
    }
    return balance;
}

// Initialize REPL state
void repl_init(ReplState *state, LogoIO *io, ReplFlags flags, const char *proc_prefix)
{
    state->io = io;
    state->flags = flags;
    state->proc_prefix = proc_prefix ? proc_prefix : "";
    
    state->proc_len = 0;
    state->in_procedure_def = false;
    state->expr_len = 0;
    state->bracket_depth = 0;
}

// Handle a single line evaluation
static Result repl_evaluate_line(ReplState *state, const char *input)
{
    Lexer lexer;
    Evaluator eval;
    lexer_init(&lexer, input);
    eval_init(&eval, &lexer);

    while (!eval_at_end(&eval))
    {
        Result r = eval_instruction(&eval);
        
        if (r.status == RESULT_ERROR)
        {
            logo_io_write_line(state->io, error_format(r));
            return result_none();  // Error handled, continue REPL
        }
        else if (r.status == RESULT_THROW)
        {
            if (strcasecmp(r.throw_tag, "toplevel") == 0)
            {
                // throw "toplevel exits the REPL
                return r;
            }
            else
            {
                // Other uncaught throws are errors
                char msg[128];
                snprintf(msg, sizeof(msg), "Can't find a catch for %s", r.throw_tag);
                logo_io_write_line(state->io, msg);
                return result_none();  // Error handled, continue REPL
            }
        }
        else if (r.status == RESULT_PAUSE)
        {
            // Nested pause - recursively run pause REPL
            ReplState pause_state;
            repl_init(&pause_state, state->io, REPL_FLAGS_PAUSE, r.pause_proc);
            
            logo_io_write_line(state->io, "Pausing...");
            
            Result pr = repl_run(&pause_state);
            if (pr.status == RESULT_THROW)
            {
                // Propagate throw
                return pr;
            }
        }
        else if (r.status == RESULT_OK)
        {
            // Expression returned a value - show "I don't know what to do with" error
            char msg[128];
            snprintf(msg, sizeof(msg), "I don't know what to do with %s", 
                     value_to_string(r.value));
            logo_io_write_line(state->io, msg);
            return result_none();  // Error handled, continue REPL
        }
        // RESULT_NONE, RESULT_STOP, RESULT_OUTPUT - continue evaluation
    }
    
    return result_none();
}

// Run the REPL loop
Result repl_run(ReplState *state)
{
    // Buffer for building prompts
    char prompt[128];
    
    while (1)
    {
        // Build and print appropriate prompt (prefix + suffix)
        if (state->in_procedure_def)
        {
            snprintf(prompt, sizeof(prompt), "%s>", state->proc_prefix);
        }
        else if (state->bracket_depth > 0)
        {
            snprintf(prompt, sizeof(prompt), "%s~", state->proc_prefix);
        }
        else
        {
            snprintf(prompt, sizeof(prompt), "%s?", state->proc_prefix);
        }
        logo_io_console_write(state->io, prompt);
        logo_io_flush(state->io);

        // Read input line
        int len = logo_stream_read_line(&state->io->console->input, state->line, sizeof(state->line));
        
        if (len == LOGO_STREAM_INTERRUPTED)
        {
            // User pressed BRK at the prompt
            logo_io_write_line(state->io, "Stopped!");
            continue;
        }
        
        if (len < 0)
        {
            // EOF or error
            if (state->flags & REPL_FLAG_EXIT_ON_EOF)
            {
                return result_none();
            }
            logo_io_write_line(state->io, "");
            continue;
        }

        // Dribble the input line (user's typed input appears on screen)
        logo_io_dribble_input(state->io, state->line);

        // Skip empty lines
        if (state->line[0] == '\0')
        {
            continue;
        }

        // Handle procedure definitions (if enabled)
        if ((state->flags & REPL_FLAG_ALLOW_PROC_DEF) && !state->in_procedure_def && repl_line_starts_with_to(state->line))
        {
            // Extract procedure name and check if it's a primitive
            char name_buf[64];
            const char *proc_name = repl_extract_proc_name(state->line, name_buf, sizeof(name_buf));
            if (proc_name && primitive_find(proc_name))
            {
                // Can't redefine a primitive
                Result r = result_error_arg(ERR_IS_PRIMITIVE, proc_name, NULL);
                logo_io_write_line(state->io, error_format(r));
                continue;
            }
            
            // Start collecting procedure definition
            state->in_procedure_def = true;
            state->proc_len = 0;
            
            // Copy the "to" line to buffer with newline
            size_t line_len = strlen(state->line);
            if (line_len + 2 <= REPL_MAX_PROC_BUFFER - 10)
            {
                memcpy(state->proc_buffer, state->line, line_len);
                state->proc_buffer[line_len] = '\n';
                state->proc_len = line_len + 1;
            }
            else
            {
                logo_io_write_line(state->io, "Procedure too long");
                state->in_procedure_def = false;
                state->proc_len = 0;
            }
            continue;
        }

        if (state->in_procedure_def)
        {
            if (repl_line_is_end(state->line))
            {
                // Complete the procedure definition
                if (state->proc_len + 4 < REPL_MAX_PROC_BUFFER)
                {
                    memcpy(state->proc_buffer + state->proc_len, "end", 3);
                    state->proc_len += 3;
                    state->proc_buffer[state->proc_len] = '\0';
                }
                
                state->in_procedure_def = false;

                // Parse and define the procedure
                Result r = proc_define_from_text(state->proc_buffer);
                if (r.status == RESULT_ERROR)
                {
                    logo_io_write_line(state->io, error_format(r));
                }
                else if (r.status == RESULT_OK)
                {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "%s defined", 
                             r.value.as.node ? mem_word_ptr(r.value.as.node) : "procedure");
                    logo_io_write_line(state->io, buf);
                }
                
                state->proc_len = 0;
            }
            else
            {
                // Append line to procedure buffer with newline
                size_t line_len = strlen(state->line);
                if (state->proc_len + line_len + 2 <= REPL_MAX_PROC_BUFFER - 10)
                {
                    memcpy(state->proc_buffer + state->proc_len, state->line, line_len);
                    state->proc_buffer[state->proc_len + line_len] = '\n';
                    state->proc_len += line_len + 1;
                }
                else
                {
                    logo_io_write_line(state->io, "Procedure too long");
                    state->in_procedure_def = false;
                    state->proc_len = 0;
                }
            }
            continue;
        }

        // Handle multi-line bracket expressions (if enabled)
        if ((state->flags & REPL_FLAG_ALLOW_CONTINUATION) && state->bracket_depth > 0)
        {
            size_t line_len = strlen(state->line);
            if (state->expr_len + line_len + 1 < REPL_MAX_PROC_BUFFER)
            {
                memcpy(state->expr_buffer + state->expr_len, state->line, line_len);
                state->expr_buffer[state->expr_len + line_len] = ' ';
                state->expr_len += line_len + 1;
                state->expr_buffer[state->expr_len] = '\0';
                
                state->bracket_depth += repl_count_bracket_balance(state->line);
                
                if (state->bracket_depth <= 0)
                {
                    state->expr_buffer[state->expr_len] = '\0';
                    
                    Result r = repl_evaluate_line(state, state->expr_buffer);
                    if (r.status == RESULT_THROW)
                    {
                        return r;
                    }
                    
                    state->bracket_depth = 0;
                    state->expr_len = 0;
                }
            }
            else
            {
                logo_io_write_line(state->io, "Expression too long");
                state->bracket_depth = 0;
                state->expr_len = 0;
            }
            continue;
        }

        // Check if this line starts a multi-line bracket expression
        if (state->flags & REPL_FLAG_ALLOW_CONTINUATION)
        {
            int line_balance = repl_count_bracket_balance(state->line);
            if (line_balance > 0)
            {
                state->bracket_depth = line_balance;
                state->expr_len = 0;
                
                size_t line_len = strlen(state->line);
                if (line_len < REPL_MAX_PROC_BUFFER - 1)
                {
                    memcpy(state->expr_buffer, state->line, line_len);
                    state->expr_buffer[line_len] = ' ';
                    state->expr_len = line_len + 1;
                    state->expr_buffer[state->expr_len] = '\0';
                }
                else
                {
                    logo_io_write_line(state->io, "Expression too long");
                    state->bracket_depth = 0;
                    state->expr_len = 0;
                }
                continue;
            }
        }

        // Regular instruction - evaluate it
        Result r = repl_evaluate_line(state, state->line);
        if (r.status == RESULT_THROW)
        {
            return r;
        }
        
        // Check if co was called (for pause REPL)
        if ((state->flags & REPL_FLAG_EXIT_ON_CO) && pause_check_continue())
        {
            return result_none();
        }
    }
}
