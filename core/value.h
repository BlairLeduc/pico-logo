//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Value and Result types for Logo evaluation.
//

#pragma once

#include "memory.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    //==========================================================================
    // Value Types
    //==========================================================================

    // Logo value types
    typedef enum
    {
        VALUE_NONE,    // No value (for commands)
        VALUE_NUMBER,  // Numeric value (float)
        VALUE_WORD,    // Word (atom node)
        VALUE_LIST,    // List (cons or nil node)
        VALUE_NEWLINE  // Newline marker (for procedure formatting)
    } ValueType;

    // A Logo object
    typedef struct
    {
        ValueType type;
        union
        {
            float number;
            Node node; // For VALUE_WORD and VALUE_LIST
        } as;
    } Value;

    //==========================================================================
    // Result Types
    //==========================================================================

    // Result status for evaluation
    typedef enum
    {
        RESULT_OK,     // Operation completed, has value
        RESULT_NONE,   // Command completed, no value
        RESULT_STOP,   // stop command, exit procedure
        RESULT_OUTPUT, // output command, has value, exit procedure
        RESULT_ERROR,  // Error occurred
        RESULT_THROW,  // throw command, propagate to catch
        RESULT_PAUSE,  // pause command, enter nested REPL
        RESULT_GOTO,   // go command, jump to label
        RESULT_EOF     // End of input, exit REPL
    } ResultStatus;

    // Evaluation result (FP-style)
    //
    // Uses a union for status-specific fields to minimize size.
    typedef struct
    {
        ResultStatus status;
        Value value;           // Valid for RESULT_OK, RESULT_OUTPUT, RESULT_THROW
        union
        {
            struct                       // RESULT_ERROR
            {
                int error_code;
                const char *error_proc;  // Procedure that caused error (e.g., "sum")
                const char *error_arg;   // Bad argument as string (e.g., "hello")
                const char *error_caller; // User procedure where error occurred
            };
            const char *throw_tag;   // RESULT_THROW (e.g., "error", "toplevel")
            const char *pause_proc;  // RESULT_PAUSE
            const char *goto_label;  // RESULT_GOTO
        };
    } Result;

    //==========================================================================
    // Value Constructors
    //==========================================================================

    Value value_none(void);
    Value value_number(float n);
    Value value_word(Node node);
    Value value_list(Node node);
    Value value_newline(void);

    //==========================================================================
    // Value Predicates
    //==========================================================================

    bool value_is_none(Value v);
    bool value_is_number(Value v);
    bool value_is_word(Value v);
    bool value_is_list(Value v);
    bool value_is_newline(Value v);

    //==========================================================================
    // Value Comparisons
    //==========================================================================

    // Compare two values for equality (works with numbers, words, and lists)
    bool values_equal(Value a, Value b);

    //==========================================================================
    // Value Conversions
    //==========================================================================

    // Attempts to convert value to number, returns true on success
    bool value_to_number(Value v, float *out);

    // Get the node from a word or list value
    Node value_to_node(Value v);

    // Extract [x y] position list into two floats
    // Returns true on success, sets *error on failure
    bool value_extract_xy(Value list, float *x, float *y, Result *error);

    // Extract [r g b] RGB list into three uint8 values (clamped to 0-255)
    // Returns true on success, sets *error on failure
    bool value_extract_rgb(Value list, uint8_t *r, uint8_t *g, uint8_t *b, Result *error);

    // Convert value to string for error messages (returns static buffer)
    const char *value_to_string(Value v);

    //==========================================================================
    // Result Constructors
    //==========================================================================

    Result result_ok(Value v);
    Result result_none(void);
    Result result_stop(void);
    Result result_output(Value v);
    Result result_error(int code);
    Result result_throw(const char *tag);
    Result result_pause(const char *proc_name);
    Result result_goto(const char *label);
    Result result_eof(void);

    // Error with context: "proc doesn't like arg as input"
    Result result_error_arg(int code, const char *proc, const char *arg);

    // Set caller context on an existing error result
    Result result_error_in(Result r, const char *caller);

    // Set error_proc on an error result if not already set
    // Returns the result (possibly modified) for chaining
    Result result_set_error_proc(Result r, const char *proc);

    //==========================================================================
    // Result Predicates
    //==========================================================================

    bool result_is_ok(Result r);
    bool result_is_returnable(Result r); // RESULT_OK or RESULT_OUTPUT

    //==========================================================================
    // Strict (tag-checked) accessors                                  (P1-010)
    //
    // The `Value` and `Result` payloads are stored in a C union, which the
    // compiler cannot tag-check for you. Two families exist for reading the
    // payload, and they have different contracts:
    //
    //   * `value_to_*` / `value_*` predicates and the existing `value_to_node`
    //     are COERCING: they tolerate the wrong tag and either return a
    //     fall-back (e.g. `NODE_NIL`) or convert (`value_to_number` parses
    //     a numeric word). Use these on UNTRUSTED values arriving at a
    //     primitive boundary, where you intend to recover from a mismatch.
    //
    //   * `value_get_*` / `result_get_*` (this section) are STRICT: they
    //     `assert()` that the tag matches and return the raw payload.
    //     Use these in INTERNAL code where you have already established
    //     the tag (e.g. inside an `if (value_is_number(v))` branch, or
    //     immediately after a `REQUIRE_NUMBER(args[0])` macro). The
    //     assertion catches programming mistakes (e.g. a refactor that
    //     dropped the type check) early in debug builds; the call compiles
    //     to a bare field read in release builds.
    //
    // Reading the wrong union field is undefined behaviour in C. These
    // accessors are the project's escape hatch from that hazard.
    //==========================================================================

    static inline float value_get_number(Value v)
    {
        assert(v.type == VALUE_NUMBER);
        return v.as.number;
    }

    // Strict accessor for the node payload of a VALUE_WORD or VALUE_LIST.
    // Use `value_to_node` (in value.c) instead if you need a coercing read
    // that returns NODE_NIL on type mismatch.
    static inline Node value_get_node(Value v)
    {
        assert(v.type == VALUE_WORD || v.type == VALUE_LIST);
        return v.as.node;
    }

    static inline Value result_get_value(Result r)
    {
        assert(r.status == RESULT_OK || r.status == RESULT_OUTPUT ||
               r.status == RESULT_THROW);
        return r.value;
    }

    static inline int result_get_error_code(Result r)
    {
        assert(r.status == RESULT_ERROR);
        return r.error_code;
    }

    static inline const char *result_get_throw_tag(Result r)
    {
        assert(r.status == RESULT_THROW);
        return r.throw_tag;
    }

    static inline const char *result_get_pause_proc(Result r)
    {
        assert(r.status == RESULT_PAUSE);
        return r.pause_proc;
    }

    static inline const char *result_get_goto_label(Result r)
    {
        assert(r.status == RESULT_GOTO);
        return r.goto_label;
    }

#ifdef __cplusplus
}
#endif
