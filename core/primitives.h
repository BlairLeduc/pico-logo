//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Primitive procedure registration and lookup.
//

#pragma once

#include "value.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Forward declaration
    typedef struct Evaluator Evaluator;

    // Primitive function signature
    typedef Result (*PrimitiveFunc)(Evaluator *eval, int argc, Value *args);

    // Primitive definition
    typedef struct
    {
        const char *name;
        int default_args; // Number of args to parse without parentheses
        PrimitiveFunc func;
    } Primitive;

    // Initialize all primitives
    void primitives_init(void);

    // Find a primitive by name (case-insensitive), returns NULL if not found
    const Primitive *primitive_find(const char *name);

    // Registration helper for primitive modules
    void primitive_register(const char *name, int default_args, PrimitiveFunc func);

    // Initialize primitive categories
    void primitives_arithmetic_init(void);
    void primitives_control_init(void);
    void primitives_variables_init(void);
    void primitives_words_lists_init(void);
    void primitives_procedures_init(void);
    void primitives_workspace_init(void);
    void primitives_outside_world_init(void);
    void primitives_properties_init(void);
    void primitives_debug_init(void);

    // Forward declarations for I/O
    struct LogoDevice;
    struct LogoIO;

    // Set the device for I/O primitives (called once at startup)
    // Legacy API - use primitives_set_io() for new code
    void primitives_set_device(struct LogoDevice *device);

    // Get the shared device for primitives that need I/O
    // Legacy API - use primitives_get_io() for new code
    struct LogoDevice *primitives_get_device(void);

    // Set the I/O manager for primitives (called once at startup)
    void primitives_set_io(struct LogoIO *io);

    // Get the shared I/O manager for primitives
    struct LogoIO *primitives_get_io(void);

    // Reset control flow test state (for testing purposes)
    void primitives_control_reset_test_state(void);

#ifdef __cplusplus
}
#endif
