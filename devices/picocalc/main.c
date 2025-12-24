//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Implements a simple REPL for the Logo interpreter.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "devices/console.h"
#include "devices/io.h"
#include "devices/storage.h"
#include "devices/stream.h"
#include "devices/picocalc/picocalc_console.h"
#include "devices/picocalc/picocalc_storage.h"
#include "devices/picocalc/picocalc_hardware.h"
#include "devices/picocalc/picocalc.h"
#include "core/memory.h"
#include "core/lexer.h"
#include "core/eval.h"
#include "core/error.h"
#include "core/primitives.h"
#include "core/procedures.h"
#include "core/variables.h"

// Maximum line length for input
#define MAX_LINE_LENGTH 256

// Maximum procedure definition buffer (for multi-line TO...END)
#define MAX_PROC_BUFFER 4096


volatile bool user_interrupt;

// Forward declaration for the I/O setter
extern void primitives_set_io(LogoIO *io);

// Check if a line starts with "to " (case-insensitive)
static bool line_starts_with_to(const char *line)
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

// Check if a line is just "end" (case-insensitive)
static bool line_is_end(const char *line)
{
    // Skip leading whitespace
    while (*line && isspace((unsigned char)*line))
        line++;
    
    if (strncasecmp(line, "end", 3) != 0)
        return false;
    
    // Must be followed by whitespace, newline, or end of string
    char c = line[3];
    return c == '\0' || isspace((unsigned char)c);
}

// Count bracket balance in a line
static int count_bracket_balance(const char *line)
{
    int balance = 0;
    for (const char *p = line; *p; p++)
    {
        if (*p == '[') balance++;
        else if (*p == ']') balance--;
    }
    return balance;
}

int main(void)
{
    picocalc_init();

    // Initialise the console for I/O
    LogoConsole *console = logo_picocalc_console_create();
    if (!console)
    {
        fprintf(stderr, "Failed to create console\n");
        return EXIT_FAILURE;
    }

    // Initialise the storage for file I/O
    LogoStorage *storage = logo_picocalc_storage_create();
    if (!storage)
    {
        fprintf(stderr, "Failed to create storage\n");
        return EXIT_FAILURE;
    }

    // Initialise the hardware (abstraction layer)
    LogoHardware *hardware = logo_picocalc_hardware_create();
    if (!hardware)
    {
        fprintf(stderr, "Failed to create hardware\n");
        return EXIT_FAILURE;
    }

    // Initialize the I/O manager
    LogoIO io;
    logo_io_init(&io, console, storage, hardware);
    strcpy(io.prefix, "/Logo/"); // Default prefix
    

    // Initialize Logo subsystems
    mem_init();
    primitives_init();
    procedures_init();
    variables_init();
    primitives_set_io(&io);

    // Load startup file if it exists (uses default prefix)
    if (logo_io_file_exists(&io, "startup"))
    {
        Lexer startup_lexer;
        Evaluator startup_eval;
        lexer_init(&startup_lexer, "load \"startup");
        eval_init(&startup_eval, &startup_lexer);
        Result r = eval_instruction(&startup_eval);
        if (r.status == RESULT_ERROR)
        {
            logo_io_write_line(&io, error_format(r));
        }
    }

    // Print welcome banner
    logo_io_write_line(&io, "Copyright 2025 Blair Leduc");
    logo_io_write_line(&io, "Welcome to Pico Logo.");

    // Input buffers
    char line[MAX_LINE_LENGTH];
    char proc_buffer[MAX_PROC_BUFFER];
    size_t proc_len = 0;
    bool in_procedure_def = false;
    
    // Bracket expression buffer (reuse MAX_PROC_BUFFER size)
    char expr_buffer[MAX_PROC_BUFFER];
    size_t expr_len = 0;
    int bracket_depth = 0;

    // Main REPL loop
    while (1)
    {
        // Print prompt directly to console (not to writer, so setwrite doesn't redirect it)
        if (in_procedure_def)
        {
            logo_io_console_write(&io, ">");
        }
        else if (bracket_depth > 0)
        {
            logo_io_console_write(&io, "~");
        }
        else
        {
            logo_io_console_write(&io, "?");
        }
        logo_io_flush(&io);

        // Read input line (directly from console, not through setread)
        int len = logo_stream_read_line(&console->input, line, sizeof(line));
        if (len == LOGO_STREAM_INTERRUPTED)
        {
            // User pressed BRK at the prompt - show "Stopped!" and continue
            logo_io_write_line(&io, "Stopped!");
            continue;
        }
        if (len < 0)
        {
            // EOF or error - exit gracefully
            logo_io_write_line(&io, "");
            break;
        }

        // Skip empty lines
        if (line[0] == '\0')
        {
            continue;
        }

        // Handle multi-line procedure definitions
        if (!in_procedure_def && line_starts_with_to(line))
        {
            // Start collecting procedure definition
            in_procedure_def = true;
            proc_len = 0;
            
            // Copy the "to" line to buffer
            size_t line_len = strlen(line);
            // Need space for line + " \n " (space + newline marker + space) + room for "end" and null terminator
            if (line_len + 1 + NEWLINE_MARKER_LENGTH + 1 <= MAX_PROC_BUFFER - 10)
            {
                memcpy(proc_buffer, line, line_len);
                // Use a special newline marker: space + NEWLINE_MARKER + space
                proc_buffer[line_len] = ' ';
                memcpy(proc_buffer + line_len + 1, NEWLINE_MARKER, NEWLINE_MARKER_LENGTH);
                proc_buffer[line_len + 1 + NEWLINE_MARKER_LENGTH] = ' ';
                proc_len = line_len + 1 + NEWLINE_MARKER_LENGTH + 1;
            }
            else
            {
                // Initial "to" line does not fit in the procedure buffer
                logo_io_write_line(&io, "Procedure too long");
                in_procedure_def = false;
                proc_len = 0;
            }
            continue;
        }

        if (in_procedure_def)
        {
            if (line_is_end(line))
            {
                // Complete the procedure definition
                // Append "end" to close the definition
                if (proc_len + 4 < MAX_PROC_BUFFER)
                {
                    memcpy(proc_buffer + proc_len, "end", 3);
                    proc_len += 3;
                    proc_buffer[proc_len] = '\0';
                }
                
                in_procedure_def = false;

                // Parse and define the procedure
                Result r = proc_define_from_text(proc_buffer);
                if (r.status == RESULT_ERROR)
                {
                    logo_io_write_line(&io, error_format(r));
                }
                else if (r.status == RESULT_OK)
                {
                    // Procedure defined successfully
                    static char buf[256];
                    snprintf(buf, sizeof(buf), "%s defined", r.value.as.node ? mem_word_ptr(r.value.as.node) : "procedure");
                    logo_io_write_line(&io, buf);
                }
                
                proc_len = 0;
            }
            else
            {
                // Append line to procedure buffer with newline marker
                size_t line_len = strlen(line);
                // Need space for line + " \n " (space + newline marker + space) + room for "end" and null terminator
                if (proc_len + line_len + 1 + NEWLINE_MARKER_LENGTH + 1 <= MAX_PROC_BUFFER - 10)
                {
                    memcpy(proc_buffer + proc_len, line, line_len);
                    // Use a special newline marker: space + NEWLINE_MARKER + space
                    proc_buffer[proc_len + line_len] = ' ';
                    memcpy(proc_buffer + proc_len + line_len + 1, NEWLINE_MARKER, NEWLINE_MARKER_LENGTH);
                    proc_buffer[proc_len + line_len + 1 + NEWLINE_MARKER_LENGTH] = ' ';
                    proc_len += line_len + 1 + NEWLINE_MARKER_LENGTH + 1;
                }
                else
                {
                    logo_io_write_line(&io, "Procedure too long");
                    in_procedure_def = false;
                    proc_len = 0;
                }
            }
            continue;
        }

        // Handle multi-line bracket expressions
        if (bracket_depth > 0)
        {
            // We're accumulating a multi-line bracket expression
            // Append line to expression buffer with space separator
            size_t line_len = strlen(line);
            if (expr_len + line_len + 1 < MAX_PROC_BUFFER)
            {
                memcpy(expr_buffer + expr_len, line, line_len);
                expr_buffer[expr_len + line_len] = ' ';
                expr_len += line_len + 1;
                expr_buffer[expr_len] = '\0';  // Null-terminate for safety
                
                // Update bracket depth
                bracket_depth += count_bracket_balance(line);
                
                // If brackets are balanced (depth <= 0), evaluate the expression
                // Note: We accept depth <= 0 to handle cases where the user enters
                // too many closing brackets - this lets the parser report the error
                if (bracket_depth <= 0)
                {
                    expr_buffer[expr_len] = '\0';
                    
                    // Evaluate the complete expression
                    Lexer lexer;
                    Evaluator eval;
                    lexer_init(&lexer, expr_buffer);
                    eval_init(&eval, &lexer);

                    // Evaluate all instructions
                    while (!eval_at_end(&eval))
                    {
                        Result r = eval_instruction(&eval);
                        
                        if (r.status == RESULT_ERROR)
                        {
                            logo_io_write_line(&io, error_format(r));
                            break;
                        }
                        else if (r.status == RESULT_THROW)
                        {
                            // Uncaught throw - check for special cases
                            if (strcasecmp(r.throw_tag, "toplevel") == 0)
                            {
                                // throw "toplevel returns to top level silently
                                break;
                            }
                            else
                            {
                                // Other uncaught throws are errors
                                char msg[128];
                                snprintf(msg, sizeof(msg), "No one caught %s", r.throw_tag);
                                logo_io_write_line(&io, msg);
                                break;
                            }
                        }
                        else if (r.status == RESULT_OK)
                        {
                            // Expression returned a value - show "I don't know what to do with" error
                            char msg[128];
                            snprintf(msg, sizeof(msg), "I don't know what to do with %s", 
                                     value_to_string(r.value));
                            logo_io_write_line(&io, msg);
                            break;
                        }
                        // RESULT_NONE means command completed successfully - continue
                    }
                    
                    // Reset bracket state
                    bracket_depth = 0;
                    expr_len = 0;
                }
            }
            else
            {
                logo_io_write_line(&io, "Expression too long");
                bracket_depth = 0;
                expr_len = 0;
            }
            continue;
        }

        // Check if this line starts a multi-line bracket expression
        int line_balance = count_bracket_balance(line);
        if (line_balance > 0)
        {
            // Start accumulating bracket expression
            bracket_depth = line_balance;
            expr_len = 0;
            
            // Copy the line to buffer
            size_t line_len = strlen(line);
            if (line_len < MAX_PROC_BUFFER - 1)
            {
                memcpy(expr_buffer, line, line_len);
                expr_buffer[line_len] = ' ';  // Space separator instead of newline
                expr_len = line_len + 1;
                expr_buffer[expr_len] = '\0';  // Null-terminate for safety
            }
            else
            {
                logo_io_write_line(&io, "Expression too long");
                bracket_depth = 0;
                expr_len = 0;
            }
            continue;
        }

        // Regular instruction - evaluate it
        Lexer lexer;
        Evaluator eval;
        lexer_init(&lexer, line);
        eval_init(&eval, &lexer);

        // Evaluate all instructions on the line
        while (!eval_at_end(&eval))
        {
            Result r = eval_instruction(&eval);
            
            if (r.status == RESULT_ERROR)
            {
                logo_io_write_line(&io, error_format(r));
                break;
            }
            else if (r.status == RESULT_THROW)
            {
                // Uncaught throw - check for special cases
                if (strcasecmp(r.throw_tag, "toplevel") == 0)
                {
                    // throw "toplevel returns to top level silently
                    break;
                }
                else
                {
                    // Other uncaught throws are errors
                    char msg[128];
                    snprintf(msg, sizeof(msg), "No one caught %s", r.throw_tag);
                    logo_io_write_line(&io, msg);
                    break;
                }
            }
            else if (r.status == RESULT_OK)
            {
                // Expression returned a value - show "I don't know what to do with" error
                char msg[128];
                snprintf(msg, sizeof(msg), "I don't know what to do with %s", 
                         value_to_string(r.value));
                logo_io_write_line(&io, msg);
                break;
            }
            // RESULT_NONE means command completed successfully - continue
        }
    }

    // Cleanup
    logo_io_cleanup(&io);

    return EXIT_SUCCESS;
}
