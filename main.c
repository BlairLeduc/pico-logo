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

#include "devices/device.h"
#include "devices/host/host_device.h"
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

// Forward declaration for the device setter in primitives_output.c
extern void primitives_set_device(LogoDevice *device);
// Forward declaration for the device setter in primitives_workspace.c
extern void primitives_workspace_set_device(LogoDevice *device);

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

// Strip trailing newline/carriage return
static void strip_newline(char *line)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    {
        line[--len] = '\0';
    }
}

int main(void)
{
    // Initialize the host device for I/O
    LogoDevice *device = logo_host_device_create();
    if (!device)
    {
        fprintf(stderr, "Failed to create device\n");
        return EXIT_FAILURE;
    }

    // Initialize Logo subsystems
    mem_init();
    primitives_init();
    procedures_init();
    variables_init();
    primitives_set_device(device);
    primitives_workspace_set_device(device);

    // Print welcome banner
    logo_device_write_line(device, "Copyright 2025 Blair Leduc");
    logo_device_write_line(device, "Welcome to Pico Logo.");

    // Input buffers
    char line[MAX_LINE_LENGTH];
    char proc_buffer[MAX_PROC_BUFFER];
    size_t proc_len = 0;
    bool in_procedure_def = false;

    // Main REPL loop
    while (1)
    {
        // Print prompt
        if (in_procedure_def)
        {
            logo_device_write(device, ">");
        }
        else
        {
            logo_device_write(device, "?");
        }
        logo_device_flush(device);

        // Read input line
        if (!logo_device_read_line(device, line, sizeof(line)))
        {
            // EOF or error - exit gracefully
            logo_device_write_line(device, "");
            break;
        }

        strip_newline(line);

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
            size_t len = strlen(line);
            if (len < MAX_PROC_BUFFER - 1)
            {
                memcpy(proc_buffer, line, len);
                proc_buffer[len] = ' ';  // Space separator instead of newline
                proc_len = len + 1;
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
                    logo_device_write_line(device, error_format(r));
                }
                
                proc_len = 0;
            }
            else
            {
                // Append line to procedure buffer with space separator
                size_t len = strlen(line);
                if (proc_len + len + 1 < MAX_PROC_BUFFER)
                {
                    memcpy(proc_buffer + proc_len, line, len);
                    proc_buffer[proc_len + len] = ' ';
                    proc_len += len + 1;
                }
                else
                {
                    logo_device_write_line(device, "Procedure too long");
                    in_procedure_def = false;
                    proc_len = 0;
                }
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
                logo_device_write_line(device, error_format(r));
                break;
            }
            else if (r.status == RESULT_OK)
            {
                // Expression returned a value - show "I don't know what to do with" error
                char msg[128];
                snprintf(msg, sizeof(msg), "I don't know what to do with %s", 
                         value_to_string(r.value));
                logo_device_write_line(device, msg);
                break;
            }
            // RESULT_NONE means command completed successfully - continue
        }
    }

    // Cleanup
    logo_host_device_destroy(device);

    return EXIT_SUCCESS;
}
