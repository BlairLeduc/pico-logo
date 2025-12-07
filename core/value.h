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
        RESULT_ERROR   // Error occurred
    } ResultStatus;

    // Evaluation result (FP-style)
    typedef struct
    {
        ResultStatus status;
        Value value;     // Valid for RESULT_OK and RESULT_OUTPUT
        int error_code;  // Valid for RESULT_ERROR
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

    //==========================================================================
    // Result Constructors
    //==========================================================================

    Result result_ok(Value v);
    Result result_none(void);
    Result result_stop(void);
    Result result_output(Value v);
    Result result_error(int code);

    //==========================================================================
    // Result Predicates
    //==========================================================================

    bool result_is_ok(Result r);
    bool result_is_returnable(Result r); // RESULT_OK or RESULT_OUTPUT

#ifdef __cplusplus
}
#endif
