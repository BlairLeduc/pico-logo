//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Variable storage for Logo interpreter.
//

#pragma once

#include "value.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Initialize variable storage
    void variables_init(void);

    // Set a variable (creates if doesn't exist)
    void var_set(const char *name, Value value);

    // Get a variable's value, returns false if not found
    bool var_get(const char *name, Value *out);

    // Check if variable exists
    bool var_exists(const char *name);

    // Erase a variable
    void var_erase(const char *name);

    // Erase all variables
    void var_erase_all(void);

#ifdef __cplusplus
}
#endif
