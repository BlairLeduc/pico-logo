//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "value.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

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

// Helper: serialize list to string (recursive)
static void list_to_buf(Node node, char *buf, int *pos, int max)
{
    if (*pos >= max - 1) return;
    
    buf[(*pos)++] = '[';
    bool first = true;
    
    while (!mem_is_nil(node) && *pos < max - 2)
    {
        if (!first && *pos < max - 1) buf[(*pos)++] = ' ';
        first = false;
        
        Node car = mem_car(node);
        if (mem_is_word(car))
        {
            const char *word = mem_word_ptr(car);
            while (*word && *pos < max - 2)
            {
                buf[(*pos)++] = *word++;
            }
        }
        else if (mem_is_list(car) || mem_is_nil(car))
        {
            list_to_buf(car, buf, pos, max);
        }
        node = mem_cdr(node);
    }
    
    if (*pos < max - 1) buf[(*pos)++] = ']';
    buf[*pos] = '\0';
}

const char *value_to_string(Value v)
{
    static char buf[128];
    
    switch (v.type)
    {
    case VALUE_NONE:
        return "";
    case VALUE_NUMBER:
        snprintf(buf, sizeof(buf), "%g", v.as.number);
        return buf;
    case VALUE_WORD:
        return mem_word_ptr(v.as.node);
    case VALUE_LIST:
        {
            int pos = 0;
            list_to_buf(v.as.node, buf, &pos, sizeof(buf));
            return buf;
        }
    }
    return "";
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

Result result_throw(const char *tag)
{
    return (Result){
        .status = RESULT_THROW,
        .value = value_none(),
        .error_code = 0,
        .error_proc = NULL,
        .error_arg = NULL,
        .error_caller = NULL,
        .throw_tag = tag
    };
}

Result result_error(int code)
{
    return (Result){
        .status = RESULT_ERROR,
        .error_code = code,
        .error_proc = NULL,
        .error_arg = NULL,
        .error_caller = NULL,
        .throw_tag = NULL
    };
}

Result result_error_arg(int code, const char *proc, const char *arg)
{
    return (Result){
        .status = RESULT_ERROR,
        .error_code = code,
        .error_proc = proc,
        .error_arg = arg,
        .error_caller = NULL,
        .throw_tag = NULL
    };
}

Result result_error_in(Result r, const char *caller)
{
    if (r.status == RESULT_ERROR && r.error_caller == NULL)
    {
        r.error_caller = caller;
    }
    return r;
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
