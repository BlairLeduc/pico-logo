//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  REPL (Read-Eval-Print Loop) shared implementation
//

#ifndef LOGO_REPL_H
#define LOGO_REPL_H

#include <stdbool.h>
#include <stddef.h>

#include "devices/io.h"
#include "core/value.h"

// Maximum line length for input
#define REPL_MAX_LINE_LENGTH 256

// Maximum procedure definition buffer (for multi-line TO...END)
#define REPL_MAX_PROC_BUFFER 4096

// REPL configuration flags
typedef enum {
    REPL_FLAG_NONE              = 0,
    REPL_FLAG_ALLOW_PROC_DEF    = 1 << 0,  // Allow "to...end" procedure definitions
    REPL_FLAG_ALLOW_CONTINUATION = 1 << 1,  // Allow multi-line bracket continuation
    REPL_FLAG_EXIT_ON_EOF       = 1 << 2,  // Return RESULT_NONE on EOF instead of looping
    REPL_FLAG_EXIT_ON_CO        = 1 << 3,  // Check for co (continue) flag and exit
} ReplFlags;

// Full REPL: all features enabled
#define REPL_FLAGS_FULL (REPL_FLAG_ALLOW_PROC_DEF | REPL_FLAG_ALLOW_CONTINUATION | REPL_FLAG_EXIT_ON_EOF)

// Pause REPL: all features plus exit on co
#define REPL_FLAGS_PAUSE (REPL_FLAG_ALLOW_PROC_DEF | REPL_FLAG_ALLOW_CONTINUATION | REPL_FLAG_EXIT_ON_EOF | REPL_FLAG_EXIT_ON_CO)

// REPL state structure
typedef struct {
    // Input/output
    LogoIO *io;
    
    // Configuration
    ReplFlags flags;
    const char *proc_prefix;      // Procedure name prefix (empty string for top level)
    
    // Internal state
    char line[REPL_MAX_LINE_LENGTH];
    char proc_buffer[REPL_MAX_PROC_BUFFER];
    size_t proc_len;
    bool in_procedure_def;
    
    char expr_buffer[REPL_MAX_PROC_BUFFER];
    size_t expr_len;
    int bracket_depth;
} ReplState;

// Initialize REPL state with default values
// proc_prefix is the procedure name prefix for prompts (empty string for top level)
void repl_init(ReplState *state, LogoIO *io, ReplFlags flags, const char *proc_prefix);

// Run the REPL loop
// Returns:
//   RESULT_NONE - Normal exit (EOF, co, etc.)
//   RESULT_THROW - Throw was propagated (e.g., throw "toplevel)
//   RESULT_ERROR - Error occurred
Result repl_run(ReplState *state);

// Helper functions (exposed for use outside REPL if needed)

// Check if a line starts with "to " (case-insensitive)
bool repl_line_starts_with_to(const char *line);

// Check if a line is just "end" (case-insensitive)
bool repl_line_is_end(const char *line);

// Extract procedure name from a "to" line into buffer
// Returns pointer to the name in buffer, or NULL if no valid name found
const char *repl_extract_proc_name(const char *line, char *buffer, size_t buffer_size);

// Count bracket balance in a line (positive = more '[', negative = more ']')
int repl_count_bracket_balance(const char *line);

#endif // LOGO_REPL_H
