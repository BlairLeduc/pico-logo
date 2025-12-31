//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Primitive procedure registration and lookup.
//

#pragma once

#include "value.h"
#include "error.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Forward declaration
    typedef struct Evaluator Evaluator;

    //==========================================================================
    // Argument Validation Macros
    //==========================================================================
    // These macros simplify common argument validation patterns in primitives.
    // They return early with appropriate errors if validation fails.

    // Suppress unused parameter warnings
    #define UNUSED(x) (void)(x)

    // Validate minimum argument count
    #define REQUIRE_ARGC(name, required) \
        do { if (argc < (required)) return result_error_arg(ERR_NOT_ENOUGH_INPUTS, name, NULL); } while(0)

    // Extract a number from an argument, returning error if not a number
    #define REQUIRE_NUMBER(name, arg, var) \
        float var; \
        do { if (!value_to_number(arg, &var)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, name, value_to_string(arg)); } while(0)

    // Validate that an argument is a word
    #define REQUIRE_WORD(name, arg) \
        do { if (!value_is_word(arg)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, name, value_to_string(arg)); } while(0)

    // Validate that an argument is a list
    #define REQUIRE_LIST(name, arg) \
        do { if (!value_is_list(arg)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, name, value_to_string(arg)); } while(0)

    // Validate that an argument is a word or list (object)
    #define REQUIRE_OBJECT(name, arg) \
        do { if (!value_is_word(arg) && !value_is_list(arg)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, name, value_to_string(arg)); } while(0)

    // Extract a non-empty word string from an argument
    #define REQUIRE_WORD_STR(name, arg, var) \
        const char *var; \
        do { if (!value_is_word(arg)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, name, value_to_string(arg)); \
            var = mem_word_ptr((arg).as.node); } while(0)

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

    // Register an alias for an existing primitive
    // The alias_name should be an interned string (from mem_word_ptr)
    // Returns true on success, false if out of space or primitive not found
    bool primitive_register_alias(const char *alias_name, const Primitive *source);

    // Initialize primitive categories
    void primitives_arithmetic_init(void);
    void primitives_conditionals_init(void);
    void primitives_control_flow_init(void);
    void primitives_debug_control_init(void);
    void primitives_debug_init(void);
    void primitives_exceptions_init(void);
    void primitives_editor_init(void);
    void primitives_files_init(void);
    void primitives_hardware_init(void);
    void primitives_logical_init(void);
    void primitives_outside_world_init(void);
    void primitives_procedures_init(void);
    void primitives_properties_init(void);
    void primitives_text_init(void);
    void primitives_turtle_init(void);
    void primitives_variables_init(void);
    void primitives_words_lists_init(void);
    void primitives_workspace_init(void);

    // Forward declarations for I/O
    struct LogoIO;

    // Set the I/O manager for primitives (called once at startup)
    void primitives_set_io(struct LogoIO *io);

    // Get the shared I/O manager for primitives
    struct LogoIO *primitives_get_io(void);

    // Reset control flow test state (for testing purposes)
    void primitives_control_reset_test_state(void);

    // Reset exceptions state (for testing purposes)
    void primitives_exceptions_reset_state(void);

    // Pause/Continue support
    // Check if continue has been requested (and reset the flag)
    bool pause_check_continue(void);
    // Request continue from pause (called by co primitive)
    void pause_request_continue(void);
    // Reset pause state (for testing)
    void pause_reset_state(void);

#ifdef __cplusplus
}
#endif
