//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "value.h"
#include <stdlib.h>
#include <ctype.h>

//==========================================================================
// Value Constructors
//==========================================================================

Value value_none(void)
{
    return (Value){.type = VALUE_NONE};
}

Value value_number(float n)
{
    return (Value){.type = VALUE_NUMBER, .as.number = n};
}

Value value_word(Node node)
{
    return (Value){.type = VALUE_WORD, .as.node = node};
}

Value value_list(Node node)
{
    return (Value){.type = VALUE_LIST, .as.node = node};
}

//==========================================================================
// Value Predicates
//==========================================================================

bool value_is_none(Value v)
{
    return v.type == VALUE_NONE;
}

bool value_is_number(Value v)
{
    return v.type == VALUE_NUMBER;
}

bool value_is_word(Value v)
{
    return v.type == VALUE_WORD;
}

bool value_is_list(Value v)
{
    return v.type == VALUE_LIST;
}

//==========================================================================
// Value Conversions
//==========================================================================

bool value_to_number(Value v, float *out)
{
    if (v.type == VALUE_NUMBER)
    {
        *out = v.as.number;
        return true;
    }
    if (v.type == VALUE_WORD)
    {
        // Try to parse word as number
        const char *str = mem_word_ptr(v.as.node);
        if (str == NULL)
            return false;

        char *end;
        float n = strtof(str, &end);
        // Must consume entire string
        if (*end == '\0' && end != str)
        {
            *out = n;
            return true;
        }
    }
    return false;
}

Node value_to_node(Value v)
{
    if (v.type == VALUE_WORD || v.type == VALUE_LIST)
    {
        return v.as.node;
    }
    return NODE_NIL;
}

//==========================================================================
// Result Constructors
//==========================================================================

Result result_ok(Value v)
{
    return (Result){.status = RESULT_OK, .value = v};
}

Result result_none(void)
{
    return (Result){.status = RESULT_NONE, .value = value_none()};
}

Result result_stop(void)
{
    return (Result){.status = RESULT_STOP, .value = value_none()};
}

Result result_output(Value v)
{
    return (Result){.status = RESULT_OUTPUT, .value = v};
}

Result result_error(int code)
{
    return (Result){.status = RESULT_ERROR, .error_code = code};
}

//==========================================================================
// Result Predicates
//==========================================================================

bool result_is_ok(Result r)
{
    return r.status == RESULT_OK;
}

bool result_is_returnable(Result r)
{
    return r.status == RESULT_OK || r.status == RESULT_OUTPUT;
}
