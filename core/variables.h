//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Variable storage for Logo interpreter.
//
//  Logo has two kinds of variables: local and global.
//  - Global variables are created by 'make' at top level or when no local
//    declaration exists in the current scope chain.
//  - Local variables are created by the 'local' command or as procedure inputs.
//    They are accessible only to that procedure and procedures it calls.
//

#pragma once

#include "value.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Initialize variable storage
    void variables_init(void);

    // Push a new local scope (called when entering a procedure)
    void var_push_scope(void);

    // Pop the current local scope (called when leaving a procedure)
    void var_pop_scope(void);

    // Get current scope depth (0 = global/top level)
    int var_scope_depth(void);

    // Declare a variable as local in the current scope
    // This creates the variable with no value (unbound)
    void var_declare_local(const char *name);

    // Set a variable's value
    // If variable exists in scope chain, updates it there
    // If variable is declared local in current scope, updates it
    // Otherwise creates/updates a global variable
    void var_set(const char *name, Value value);

    // Set a local variable in current scope (for procedure inputs)
    // This both declares and sets in one operation
    void var_set_local(const char *name, Value value);

    // Get a variable's value, searching scope chain then globals
    // Returns false if not found
    bool var_get(const char *name, Value *out);

    // Check if variable exists (in scope chain or globals)
    bool var_exists(const char *name);

    // Erase a variable (from current scope or globals)
    void var_erase(const char *name);

    // Erase all global variables (local scopes should already be popped)
    void var_erase_all(void);

    // Erase all global variables (respects buried flag if check_buried is true)
    void var_erase_all_globals(bool check_buried);

    // Bury/unbury variable names
    void var_bury(const char *name);
    void var_unbury(const char *name);
    void var_bury_all(void);
    void var_unbury_all(void);

    // Iteration support for workspace display
    int var_global_count(bool include_buried);
    bool var_get_global_by_index(int index, bool include_buried,
                                  const char **name_out, Value *value_out);

    // Mark all variable values as GC roots
    void var_gc_mark_all(void);

    // Test state management (for test/iftrue/iffalse primitives)
    // Test state is local to procedure scope but accessible to superprocedures
    void var_set_test_result(bool result);
    bool var_get_test_result(bool *out_result);  // returns false if no test has been run

#ifdef __cplusplus
}
#endif
