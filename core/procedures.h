//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  User-defined procedure storage and execution.
//
//  Logo procedures are defined with 'to' and consist of:
//  - A name
//  - Zero or more input parameter names
//  - A body (list of instructions)
//
//  Tail recursion optimization:
//  When a procedure's last action is to call itself (or another procedure),
//  we can reuse the current stack frame instead of creating a new one.
//  This allows recursive procedures to run indefinitely without stack overflow.
//

#pragma once

#include "value.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Forward declaration
    typedef struct Evaluator Evaluator;

    // Maximum parameters per procedure
    #define MAX_PROC_PARAMS 16

    // User-defined procedure
    typedef struct UserProcedure
    {
        const char *name;           // Procedure name (interned)
        const char *params[MAX_PROC_PARAMS];  // Parameter names (interned)
        int param_count;            // Number of parameters
        Node body;                  // Body as a list of instructions
        bool buried;                // If true, hidden from poall/erall/etc
    } UserProcedure;

    // Tail call information for optimization
    typedef struct TailCall
    {
        bool is_tail_call;          // True if this is a tail call
        const char *proc_name;      // Name of procedure to call
        Value args[MAX_PROC_PARAMS]; // Arguments for tail call
        int arg_count;              // Number of arguments
    } TailCall;

    // Initialize procedure storage
    void procedures_init(void);

    // Define a new procedure (returns false if already exists)
    bool proc_define(const char *name, const char **params, int param_count, Node body);

    // Find a procedure by name (case-insensitive), returns NULL if not found
    UserProcedure *proc_find(const char *name);

    // Check if a procedure exists
    bool proc_exists(const char *name);

    // Erase a procedure
    void proc_erase(const char *name);

    // Erase all procedures (respects buried flag if check_buried is true)
    void proc_erase_all(bool check_buried);

    // Call a user procedure with arguments
    // Handles scope push/pop and tail call optimization
    Result proc_call(Evaluator *eval, UserProcedure *proc, int argc, Value *args);

    // Get/set tail call info (for TCO)
    TailCall *proc_get_tail_call(void);
    void proc_clear_tail_call(void);

    // Iteration helpers for poall, pots, etc.
    int proc_count(bool include_buried);
    UserProcedure *proc_get_by_index(int index);

    // Bury/unbury procedures
    void proc_bury(const char *name);
    void proc_unbury(const char *name);
    void proc_bury_all(void);
    void proc_unbury_all(void);

    // Parse and define a procedure from text: "to name :param ... body ... end"
    // Returns a Result - RESULT_NONE on success, RESULT_ERROR on failure
    Result proc_define_from_text(const char *text);

    // Mark all procedure bodies as GC roots
    void proc_gc_mark_all(void);

#ifdef __cplusplus
}
#endif
