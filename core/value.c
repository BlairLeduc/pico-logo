//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "value.h"
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

// Format a number to a buffer using Logo conventions:
// - Remove trailing zeros after decimal point
// - Use 'e' for positive exponents (1e7), 'n' for negative exponents (1n6)
// Uses up to 6 significant digits for single-precision floats.
// Returns the number of characters written (excluding null terminator).
//
// This custom implementation avoids snprintf for efficiency on embedded systems.
int format_number(char *buf, size_t size, float n)
{
    if (size == 0)
        return 0;

    char *p = buf;
    char *end = buf + size - 1;  // Leave room for null terminator

    // Handle special cases
    if (n != n)  // NaN check
    {
        if (end - p >= 3)
        {
            *p++ = 'n';
            *p++ = 'a';
            *p++ = 'n';
        }
        *p = '\0';
        return p - buf;
    }

    // Handle sign
    if (n < 0)
    {
        if (p < end)
            *p++ = '-';
        n = -n;
    }

    // Handle infinity
    if (n > 3.4e38f)
    {
        if (end - p >= 3)
        {
            *p++ = 'i';
            *p++ = 'n';
            *p++ = 'f';
        }
        *p = '\0';
        return p - buf;
    }

    // Handle zero
    if (n == 0.0f)
    {
        if (p < end)
            *p++ = '0';
        *p = '\0';
        return p - buf;
    }

    // Determine the decimal exponent
    // We want to find exp10 such that 1 <= n * 10^(-exp10) < 10
    int exp10 = 0;
    float scaled = n;

    // Scale down large numbers
    if (scaled >= 10.0f)
    {
        // Use larger steps for efficiency
        while (scaled >= 1e32f) { scaled *= 1e-32f; exp10 += 32; }
        while (scaled >= 1e16f) { scaled *= 1e-16f; exp10 += 16; }
        while (scaled >= 1e8f)  { scaled *= 1e-8f;  exp10 += 8; }
        while (scaled >= 1e4f)  { scaled *= 1e-4f;  exp10 += 4; }
        while (scaled >= 10.0f) { scaled *= 0.1f;   exp10 += 1; }
    }
    // Scale up small numbers
    else if (scaled < 1.0f)
    {
        while (scaled < 1e-31f) { scaled *= 1e32f; exp10 -= 32; }
        while (scaled < 1e-15f) { scaled *= 1e16f; exp10 -= 16; }
        while (scaled < 1e-7f)  { scaled *= 1e8f;  exp10 -= 8; }
        while (scaled < 1e-3f)  { scaled *= 1e4f;  exp10 -= 4; }
        while (scaled < 1.0f)   { scaled *= 10.0f; exp10 -= 1; }
    }

    // Now scaled is in [1.0, 10.0) and exp10 is the exponent
    // Decide between fixed-point and scientific notation
    // Use fixed-point for exponents -4 to 5 (like %g behavior)
    bool use_scientific = (exp10 < -4 || exp10 > 5);

    // Extract up to 6 significant digits
    // Add small rounding factor
    scaled += 0.0000005f;
    if (scaled >= 10.0f)
    {
        scaled *= 0.1f;
        exp10++;
    }

    // Extract digits
    char digits[8];
    int num_digits = 0;
    float temp = scaled;
    for (int i = 0; i < 6 && temp > 0.000001f; i++)
    {
        int d = (int)temp;
        if (d > 9) d = 9;  // Clamp for safety
        digits[num_digits++] = '0' + d;
        temp = (temp - d) * 10.0f;
    }

    // Remove trailing zeros from significant digits
    while (num_digits > 1 && digits[num_digits - 1] == '0')
    {
        num_digits--;
    }

    if (use_scientific)
    {
        // Scientific notation: d.dddde±exp or d.ddddn±exp
        if (p < end)
            *p++ = digits[0];

        if (num_digits > 1)
        {
            if (p < end)
                *p++ = '.';
            for (int i = 1; i < num_digits && p < end; i++)
            {
                *p++ = digits[i];
            }
        }

        // Write exponent: 'e' for positive, 'n' for negative
        if (p < end)
            *p++ = (exp10 >= 0) ? 'e' : 'n';

        // Write absolute exponent value
        int abs_exp = (exp10 >= 0) ? exp10 : -exp10;
        if (abs_exp >= 100)
        {
            if (p < end) *p++ = '0' + (abs_exp / 100);
            abs_exp %= 100;
            if (p < end) *p++ = '0' + (abs_exp / 10);
            if (p < end) *p++ = '0' + (abs_exp % 10);
        }
        else if (abs_exp >= 10)
        {
            if (p < end) *p++ = '0' + (abs_exp / 10);
            if (p < end) *p++ = '0' + (abs_exp % 10);
        }
        else
        {
            if (p < end) *p++ = '0' + abs_exp;
        }
    }
    else
    {
        // Fixed-point notation
        // Position of decimal point: after digit at index exp10
        // e.g., exp10=2 means ###.### (3 digits before decimal)
        //       exp10=-1 means 0.0### (decimal before first digit)

        if (exp10 >= 0)
        {
            // Digits before decimal point
            int before_decimal = exp10 + 1;

            for (int i = 0; i < before_decimal && p < end; i++)
            {
                if (i < num_digits)
                    *p++ = digits[i];
                else
                    *p++ = '0';
            }

            // Digits after decimal point (if any significant ones remain)
            if (num_digits > before_decimal)
            {
                if (p < end)
                    *p++ = '.';
                for (int i = before_decimal; i < num_digits && p < end; i++)
                {
                    *p++ = digits[i];
                }
            }
        }
        else
        {
            // exp10 is negative: need leading "0." and possibly zeros
            if (p < end)
                *p++ = '0';
            if (p < end)
                *p++ = '.';

            // Leading zeros after decimal point
            int leading_zeros = -exp10 - 1;
            for (int i = 0; i < leading_zeros && p < end; i++)
            {
                *p++ = '0';
            }

            // Significant digits
            for (int i = 0; i < num_digits && p < end; i++)
            {
                *p++ = digits[i];
            }
        }
    }

    *p = '\0';
    return p - buf;
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
