//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "value.h"
#include "error.h"
#include "format.h"
#include <ctype.h>
#include <stdlib.h>

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
// Value Comparisons
//==========================================================================

bool values_equal(Value a, Value b)
{
    if (a.type != b.type)
    {
        // Allow number-word comparison if word is numeric
        if (value_is_number(a) && value_is_word(b))
        {
            float n;
            if (value_to_number(b, &n))
            {
                return a.as.number == n;
            }
        }
        else if (value_is_word(a) && value_is_number(b))
        {
            float n;
            if (value_to_number(a, &n))
            {
                return n == b.as.number;
            }
        }
        return false;
    }
    
    if (value_is_number(a))
    {
        // Direct floating-point comparison is intentional for Logo semantics.
        // Classic Logo uses exact equality, matching user expectations in an
        // educational context. Floating-point precision issues (e.g., 0.1 + 0.2
        // not equaling 0.3) are inherent to IEEE 754 and would already be visible
        // in arithmetic results before comparison.
        return a.as.number == b.as.number;
    }
    else if (value_is_word(a))
    {
        return mem_words_equal(a.as.node, b.as.node);
    }
    else if (value_is_list(a))
    {
        // Compare lists element by element
        Node la = a.as.node;
        Node lb = b.as.node;
        while (!mem_is_nil(la) && !mem_is_nil(lb))
        {
            Node car_a = mem_car(la);
            Node car_b = mem_car(lb);
            Value va = mem_is_word(car_a) ? value_word(car_a) : value_list(car_a);
            Value vb = mem_is_word(car_b) ? value_word(car_b) : value_list(car_b);
            if (!values_equal(va, vb))
            {
                return false;
            }
            la = mem_cdr(la);
            lb = mem_cdr(lb);
        }
        return mem_is_nil(la) && mem_is_nil(lb);
    }
    // VALUE_NONE is not a valid Logo object, so two NONE values are not equal.
    // This case should not occur in normal Logo evaluation.
    return false;
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

bool value_extract_xy(Value list, float *x, float *y, const char *proc_name, Result *error)
{
    if (list.type != VALUE_LIST)
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, proc_name, value_to_string(list));
        return false;
    }

    Node node = list.as.node;
    if (mem_is_nil(node))
    {
        *error = result_error_arg(ERR_TOO_FEW_ITEMS_LIST, proc_name, NULL);
        return false;
    }

    Node x_node = mem_car(node);
    node = mem_cdr(node);
    
    if (mem_is_nil(node))
    {
        *error = result_error_arg(ERR_TOO_FEW_ITEMS_LIST, proc_name, NULL);
        return false;
    }
    
    Node y_node = mem_car(node);

    // Convert to numbers
    Value x_val = mem_is_word(x_node) ? value_word(x_node) : value_list(x_node);
    Value y_val = mem_is_word(y_node) ? value_word(y_node) : value_list(y_node);
    
    float x_num, y_num;
    if (!value_to_number(x_val, &x_num) || !value_to_number(y_val, &y_num))
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, proc_name, value_to_string(list));
        return false;
    }

    *x = x_num;
    *y = y_num;
    return true;
}

bool value_extract_rgb(Value list, uint8_t *r, uint8_t *g, uint8_t *b, const char *proc_name, Result *error)
{
    if (list.type != VALUE_LIST)
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, proc_name, value_to_string(list));
        return false;
    }

    Node node = list.as.node;
    if (mem_is_nil(node))
    {
        *error = result_error_arg(ERR_TOO_FEW_ITEMS_LIST, proc_name, NULL);
        return false;
    }

    Node r_node = mem_car(node);
    node = mem_cdr(node);
    
    if (mem_is_nil(node))
    {
        *error = result_error_arg(ERR_TOO_FEW_ITEMS_LIST, proc_name, NULL);
        return false;
    }
    
    Node g_node = mem_car(node);
    node = mem_cdr(node);
    
    if (mem_is_nil(node))
    {
        *error = result_error_arg(ERR_TOO_FEW_ITEMS_LIST, proc_name, NULL);
        return false;
    }
    
    Node b_node = mem_car(node);

    // Convert to numbers
    Value r_val = mem_is_word(r_node) ? value_word(r_node) : value_list(r_node);
    Value g_val = mem_is_word(g_node) ? value_word(g_node) : value_list(g_node);
    Value b_val = mem_is_word(b_node) ? value_word(b_node) : value_list(b_node);
    
    float r_num, g_num, b_num;
    if (!value_to_number(r_val, &r_num) || 
        !value_to_number(g_val, &g_num) || 
        !value_to_number(b_val, &b_num))
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, proc_name, value_to_string(list));
        return false;
    }

    // Clamp to 0-255 range
    if (r_num < 0) r_num = 0;
    if (r_num > 255) r_num = 255;
    if (g_num < 0) g_num = 0;
    if (g_num > 255) g_num = 255;
    if (b_num < 0) b_num = 0;
    if (b_num > 255) b_num = 255;

    *r = (uint8_t)r_num;
    *g = (uint8_t)g_num;
    *b = (uint8_t)b_num;
    return true;
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
        format_number(buf, sizeof(buf), v.as.number);
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
    return (Result){.status = RESULT_OK, .value = v, .throw_tag = NULL};
}

Result result_none(void)
{
    return (Result){.status = RESULT_NONE, .value = value_none(), .throw_tag = NULL};
}

Result result_stop(void)
{
    return (Result){.status = RESULT_STOP, .value = value_none(), .throw_tag = NULL};
}

Result result_output(Value v)
{
    return (Result){.status = RESULT_OUTPUT, .value = v, .throw_tag = NULL};
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

Result result_throw(const char *tag)
{
    return (Result){
        .status = RESULT_THROW,
        .throw_tag = tag,
        .value = value_none(),
        .pause_proc = NULL,
        .error_code = 0,
        .error_proc = NULL,
        .error_arg = NULL,
        .error_caller = NULL
    };
}

Result result_pause(const char *proc_name)
{
    return (Result){
        .status = RESULT_PAUSE,
        .pause_proc = proc_name,
        .value = value_none(),
        .error_code = 0,
        .error_proc = NULL,
        .error_arg = NULL,
        .error_caller = NULL,
        .throw_tag = NULL,
        .goto_label = NULL
    };
}

Result result_goto(const char *label)
{
    return (Result){
        .status = RESULT_GOTO,
        .goto_label = label,
        .value = value_none(),
        .error_code = 0,
        .error_proc = NULL,
        .error_arg = NULL,
        .error_caller = NULL,
        .throw_tag = NULL,
        .pause_proc = NULL
    };
}

Result result_eof(void)
{
    return (Result){
        .status = RESULT_EOF,
        .value = value_none(),
        .error_code = 0,
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
