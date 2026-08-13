//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
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
    // The error_proc field is left NULL and will be filled in by the evaluator
    // using the name the user actually typed (handles aliases like fd vs forward).

    // Suppress unused parameter warnings
    #define UNUSED(x) (void)(x)

    // Validate minimum argument count
    #define REQUIRE_ARGC(required) \
        do { if (argc < (required)) return result_error_arg(ERR_NOT_ENOUGH_INPUTS, NULL, NULL); } while(0)

    // Extract a number from an argument, returning error if not a number
    #define REQUIRE_NUMBER(arg, var) \
        float var; \
        do { if (!value_to_number(arg, &var)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(arg)); } while(0)

    // Validate that an argument is a word
    #define REQUIRE_WORD(arg) \
        do { if (!value_is_word(arg)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(arg)); } while(0)

    // Validate that an argument is a list
    #define REQUIRE_LIST(arg) \
        do { if (!value_is_list(arg)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(arg)); } while(0)

    // Validate that an argument is a word or list (object)
    #define REQUIRE_OBJECT(arg) \
        do { if (!value_is_word(arg) && !value_is_list(arg)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(arg)); } while(0)

    // Extract a non-empty word string from an argument.
    // A word whose atom could not be interned has no characters behind it, so
    // the pointer is NULL and every caller here would dereference it.
    #define REQUIRE_WORD_STR(arg, var) \
        const char *var; \
        do { if (!value_is_word(arg)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(arg)); \
            var = mem_word_ptr((arg).as.node); \
            if (!var) return result_error(ERR_OUT_OF_SPACE); } while(0)

    // Extract a non-empty list from an argument
    #define REQUIRE_LIST_NONEMPTY(arg, var) \
        Node var; \
        do { if (!value_is_list(arg)) \
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(arg)); \
            var = (arg).as.node; \
            if (mem_is_nil(var)) \
                return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, NULL); } while(0)

    // Extract a boolean from an argument, returning error if not a boolean word
    #define REQUIRE_BOOL(arg, var) \
        bool var; \
        do { \
            if (value_is_word(arg)) \
            { \
                const char *str = value_to_string(arg); \
                if (strcasecmp(str, "true") == 0) \
                    var = true; \
                else if (strcasecmp(str, "false") == 0) \
                    var = false; \
                else \
                    return result_error_arg(ERR_NOT_BOOL, NULL, str); \
            } \
            else \
            { \
                return result_error_arg(ERR_NOT_BOOL, NULL, value_to_string(arg)); \
            } \
        } while(0)


    // Primitive function signature
    typedef Result (*PrimitiveFunc)(Evaluator *eval, int argc, Value *args);

    // Primitive definition
    typedef struct Primitive
    {
        const char *name;
        int default_args; // Number of args to parse without parentheses
        PrimitiveFunc func;
    } Primitive;

    // Initialize all primitives
    void primitives_init(void);

    // Find a primitive by name (case-insensitive), returns NULL if not found
    const Primitive *primitive_find(const char *name);

    // Same, for a name that is not NUL-terminated (a word token straight out
    // of the lexer). Lets the evaluator look a name up without first copying
    // it into a fixed-size buffer, which truncated long names (B5).
    const Primitive *primitive_find_n(const char *name, size_t len);

    // Index of a registered primitive, or -1 if it is not one. The table is
    // append-only, so an index stays valid for the run and can be cached on
    // the atom of the name that resolved to it (core/atom_memo.h).
    int primitive_index_of(const Primitive *prim);
    const Primitive *primitive_by_index(int index);

    // Is this `output` or `op`? Their argument is in tail position, a check
    // the evaluator makes once per collected argument.
    bool primitive_is_output(const Primitive *prim);

    // Registration helper for primitive modules
    void primitive_register(const char *name, int default_args, PrimitiveFunc func);

    // Register an alias for an existing primitive
    // The alias_name should be an interned string (from mem_word_ptr)
    // Returns true on success, false if out of space or primitive not found
    bool primitive_register_alias(const char *alias_name, const Primitive *source);

    // Get the number of registered primitives
    int primitive_get_count(void);

    // Get a primitive by index (0-based), returns NULL if out of range
    const Primitive *primitive_get_by_index(int index);

    // Initialize primitive categories
    void primitives_arithmetic_init(void);
    void primitives_conditionals_init(void);
    void primitives_control_flow_init(void);
    void primitives_debug_control_init(void);
    void primitives_debug_init(void);
    void primitives_exceptions_init(void);
    void primitives_editor_init(void);
    void primitives_files_init(void);
    void primitives_files_directory_init(void);
    void primitives_files_load_save_init(void);
    void primitives_hardware_init(void);
    void primitives_sound_init(void);
    void primitives_logical_init(void);
    void primitives_bitwise_init(void);
    void primitives_outside_world_init(void);
    void primitives_procedures_init(void);
    void primitives_properties_init(void);
    void primitives_json_init(void);
    void primitives_text_init(void);
    void primitives_turtle_init(void);
    void primitives_events_init(void);
    void primitives_variables_init(void);
    void primitives_words_lists_init(void);
    void primitives_workspace_init(void);
    void primitives_list_processing_init(void);
    void primitives_wifi_init(void);
    void primitives_network_init(void);
    void primitives_http_init(void);
    void primitives_httpd_init(void);
    void primitives_time_init(void);
    void primitives_tilemap_init(void);

    // Stop autonomous motion and animation on every turtle (speed 0, anim
    // off). Turtle state, not demon state: called by `cs` alongside its
    // clear/home, and by demons_reset() on a full autonomous reset. No-op
    // when there is no device yet (boot). On devices that support turtle
    // selection this visits each turtle in turn and leaves turtle 0 selected.
    void turtle_stop_motion(void);

    // Route the device to the lowest turtle in the `tell` set, the one a
    // query answers for. Shared with the tile primitives, whose capture
    // happens at the turtle exactly as snapsh's does. No-op without a device.
    void turtle_select_first_active(void);

    // Forward declarations for I/O
    struct LogoIO;

    // Set the I/O manager for primitives (called once at startup)
    void primitives_set_io(struct LogoIO *io);

    // Get the shared I/O manager for primitives
    struct LogoIO *primitives_get_io(void);

    // Reset control flow test state (for testing purposes)
    void primitives_control_reset_test_state(void);

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
