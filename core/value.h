//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Value and Result types for Logo evaluation.
//

#pragma once

#include "memory.h"
#include <stdbool.h>

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
        VALUE_NONE,   // No value (for commands)
        VALUE_NUMBER, // Numeric value (float)
        VALUE_WORD,   // Word (atom node)
        VALUE_LIST    // List (cons or nil node)
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
        RESULT_GOTO    // go command, transfer control to label
    } ResultStatus;

    // Evaluation result (FP-style)
    typedef struct
    {
        ResultStatus status;
        Value value;           // Valid for RESULT_OK and RESULT_OUTPUT
        int error_code;        // Valid for RESULT_ERROR
        const char *error_proc;  // Procedure that caused error (e.g., "sum")
        const char *error_arg;   // Bad argument as string (e.g., "hello")
        const char *error_caller; // User procedure where error occurred (e.g., "add")
        const char *throw_tag;   // Tag for RESULT_THROW (e.g., "error", "toplevel")
        const char *goto_label;  // Label for RESULT_GOTO (e.g., "loop", "start")
    } Result;

    //==========================================================================
    // Value Constructors
    //==========================================================================

    Value value_none(void);
    Value value_number(float n);
    Value value_word(Node node);
    Value value_list(Node node);

    //==========================================================================
    // Value Predicates
    //==========================================================================

    bool value_is_none(Value v);
    bool value_is_number(Value v);
    bool value_is_word(Value v);
    bool value_is_list(Value v);

    //==========================================================================
    // Value Conversions
    //==========================================================================

    // Attempts to convert value to number, returns true on success
    bool value_to_number(Value v, float *out);

    // Get the node from a word or list value
    Node value_to_node(Value v);

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
    Result result_goto(const char *label);

    // Error with context: "proc doesn't like arg as input"
    Result result_error_arg(int code, const char *proc, const char *arg);

    // Set caller context on an existing error result
    Result result_error_in(Result r, const char *caller);

    //==========================================================================
    // Result Predicates
    //==========================================================================

    bool result_is_ok(Result r);
    bool result_is_returnable(Result r); // RESULT_OK or RESULT_OUTPUT

#ifdef __cplusplus
}
#endif
