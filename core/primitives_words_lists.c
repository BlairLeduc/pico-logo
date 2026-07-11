//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Words and Lists primitives: first, last, butfirst, butlast, item, member,
//  fput, list, lput, parse, sentence, word, ascii, before?, char, equal?,
//  list?, member?, number?, word?, lowercase, uppercase, count, emptyp
//

#include "primitives.h"
#include "format.h"
#include "error.h"
#include "eval.h"
#include "lexer.h"
#include "parse_list.h"
#include "random.h"
#include "devices/io.h"
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

//==========================================================================
// Element access: first, last, butfirst, butlast, item, replace, member
//==========================================================================

// Numbers are words in Logo: convert a numeric argument to its word form so
// the word branch of an element primitive handles both. Returns false when
// the atom table is exhausted (callers surface ERR_OUT_OF_SPACE).
static bool normalize_to_word(Value *obj)
{
    if (value_is_number(*obj))
    {
        Node word = number_to_word(obj->as.number);
        if (mem_is_nil(word))
        {
            return false;
        }
        *obj = value_word(word);
    }
    return true;
}

// first object
// Outputs the first element of object.
// For a word: outputs the first character as a word
// For a list: outputs the first element (word or list)
static Result prim_first(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value obj = args[0];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        if (str == NULL || str[0] == '\0')
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, value_to_string(obj));
        }
        // Return first character as a word
        Node first_char = mem_atom(str, 1);
        return result_ok(value_word(first_char));
    }
    else if (value_is_list(obj))
    {
        Node list = obj.as.node;
        if (mem_is_nil(list))
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, "[]");
        }
        Node first = mem_car(list);
        // Determine if it's a word or list
        if (mem_is_word(first))
        {
            return result_ok(value_word(first));
        }
        else
        {
            return result_ok(value_list(first));
        }
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// last object
// Outputs the last element of object.
static Result prim_last(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value obj = args[0];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        size_t len = mem_word_len(obj.as.node);
        if (len == 0)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, value_to_string(obj));
        }
        Node last_char = mem_atom(str + len - 1, 1);
        return result_ok(value_word(last_char));
    }
    else if (value_is_list(obj))
    {
        Node list = obj.as.node;
        if (mem_is_nil(list))
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, "[]");
        }
        // Find last element
        Node last = mem_car(list);
        while (!mem_is_nil(mem_cdr(list)))
        {
            list = mem_cdr(list);
            last = mem_car(list);
        }
        if (mem_is_word(last))
        {
            return result_ok(value_word(last));
        }
        else
        {
            return result_ok(value_list(last));
        }
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// butfirst object (bf)
// Outputs object with its first element removed.
static Result prim_butfirst(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value obj = args[0];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        size_t len = mem_word_len(obj.as.node);
        if (len == 0)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
        }
        if (len == 1)
        {
            Node empty = mem_atom("", 0);
            return result_ok(value_word(empty));
        }
        Node rest = mem_atom(str + 1, len - 1);
        return result_ok(value_word(rest));
    }
    else if (value_is_list(obj))
    {
        Node list = obj.as.node;
        if (mem_is_nil(list))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, "[]");
        }
        return result_ok(value_list(mem_cdr(list)));
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// butlast object (bl)
// Outputs object with its last element removed.
static Result prim_butlast(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value obj = args[0];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        size_t len = mem_word_len(obj.as.node);
        if (len == 0)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
        }
        if (len == 1)
        {
            Node empty = mem_atom("", 0);
            return result_ok(value_word(empty));
        }
        Node rest = mem_atom(str, len - 1);
        return result_ok(value_word(rest));
    }
    else if (value_is_list(obj))
    {
        Node list = obj.as.node;
        if (mem_is_nil(list))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, "[]");
        }
        // Build new list without last element
        if (mem_is_nil(mem_cdr(list)))
        {
            // Only one element, return empty list
            return result_ok(value_list(NODE_NIL));
        }
        // Copy all but last
        Node result = NODE_NIL;
        Node tail = NODE_NIL;
        while (!mem_is_nil(mem_cdr(list)))
        {
            if (!mem_list_append(&result, &tail, mem_car(list)))
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
            list = mem_cdr(list);
        }
        return result_ok(value_list(result));
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// count object
// Outputs the number of elements in object.
static Result prim_count(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    Value obj = args[0];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    if (value_is_word(obj))
    {
        return result_ok(value_number((float)mem_word_len(obj.as.node)));
    }
    else if (value_is_list(obj))
    {
        int count = 0;
        Node list = obj.as.node;
        while (!mem_is_nil(list))
        {
            count++;
            list = mem_cdr(list);
        }
        return result_ok(value_number((float)count));
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// emptyp object (empty?)
// Outputs TRUE if object is empty word or empty list.
static Result prim_emptyp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    Value obj = args[0];
    Node true_word = mem_true_node;
    Node false_word = mem_false_node;
    
    if (value_is_word(obj))
    {
        if (mem_word_len(obj.as.node) == 0)
            return result_ok(value_word(true_word));
        return result_ok(value_word(false_word));
    }
    else if (value_is_list(obj))
    {
        if (mem_is_nil(obj.as.node))
            return result_ok(value_word(true_word));
        return result_ok(value_word(false_word));
    }
    else if (value_is_number(obj))
    {
        // Numbers are never empty
        return result_ok(value_word(false_word));
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// item integer object
// Outputs the element at position integer within object.
static Result prim_item(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    float index_f;
    if (!value_to_number(args[0], &index_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    int index = (int)index_f;
    if (index < 1)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    Value obj = args[1];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        size_t len = mem_word_len(obj.as.node);
        if ((size_t)index > len)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, value_to_string(obj));
        }
        Node item_char = mem_atom(str + index - 1, 1);
        return result_ok(value_word(item_char));
    }
    else if (value_is_list(obj))
    {
        Node list = obj.as.node;
        for (int i = 1; i < index && !mem_is_nil(list); i++)
        {
            list = mem_cdr(list);
        }
        if (mem_is_nil(list))
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, "[]");
        }
        Node item = mem_car(list);
        if (mem_is_word(item))
        {
            return result_ok(value_word(item));
        }
        else
        {
            return result_ok(value_list(item));
        }
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// replace integer object value
// Returns a new list/word with element at position integer replaced with value.
static Result prim_replace(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    float index_f;
    if (!value_to_number(args[0], &index_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    int index = (int)index_f;
    if (index < 1)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    Value obj = args[1];
    Value replacement = args[2];
    
    if (value_is_number(obj))
    {
        // Convert number to word first
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        size_t len = strlen(str);
        if ((size_t)index > len || len == 0)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, value_to_string(obj));
        }
        
        // Get replacement as a single character
        const char *repl_str;
        if (value_is_number(replacement))
        {
            Node repl_word = number_to_word(replacement.as.number);
            repl_str = mem_word_ptr(repl_word);
        }
        else if (value_is_word(replacement))
        {
            repl_str = mem_word_ptr(replacement.as.node);
        }
        else
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(replacement));
        }
        
        if (repl_str[0] == '\0')
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(replacement));
        }
        
        // Build new word with replacement character
        char buffer[256];
        if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
        memcpy(buffer, str, len);
        buffer[index - 1] = repl_str[0];
        buffer[len] = '\0';
        
        Node result = mem_atom_cstr(buffer);
        return result_ok(value_word(result));
    }
    else if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        size_t len = mem_word_len(obj.as.node);
        if ((size_t)index > len || len == 0)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, value_to_string(obj));
        }
        
        // Get replacement as a single character
        const char *repl_str;
        if (value_is_number(replacement))
        {
            Node repl_word = number_to_word(replacement.as.number);
            repl_str = mem_word_ptr(repl_word);
        }
        else if (value_is_word(replacement))
        {
            repl_str = mem_word_ptr(replacement.as.node);
        }
        else
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(replacement));
        }
        
        if (repl_str[0] == '\0')
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(replacement));
        }
        
        // Build new word with replacement character
        char buffer[256];
        if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
        memcpy(buffer, str, len);
        buffer[index - 1] = repl_str[0];
        buffer[len] = '\0';
        
        Node result = mem_atom_cstr(buffer);
        return result_ok(value_word(result));
    }
    else if (value_is_list(obj))
    {
        Node list = obj.as.node;
        if (mem_is_nil(list))
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, "[]");
        }
        
        // Count elements to verify index is valid
        int count = 0;
        Node temp = list;
        while (!mem_is_nil(temp))
        {
            count++;
            temp = mem_cdr(temp);
        }
        if (index > count)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, "[]");
        }
        
        // Build new list with replacement at index
        Node result = NODE_NIL;
        Node tail = NODE_NIL;
        int pos = 1;
        while (!mem_is_nil(list))
        {
            Node element;
            if (pos == index)
            {
                // Use replacement value
                element = replacement.as.node;
            }
            else
            {
                element = mem_car(list);
            }

            if (!mem_list_append(&result, &tail, element))
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
            list = mem_cdr(list);
            pos++;
        }
        return result_ok(value_list(result));
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// Get the underlying word pointer (and length) for a number/word value.
// Returns false if the value is neither.
static bool value_as_word_str(Value v, const char **out_str, size_t *out_len)
{
    if (value_is_number(v))
    {
        Node word = number_to_word(v.as.number);
        if (mem_is_nil(word))
            return false;  // atom table exhausted: mem_word_ptr would be NULL
        *out_str = mem_word_ptr(word);
        *out_len = mem_word_len(word);
        return true;
    }
    if (value_is_word(v))
    {
        *out_str = mem_word_ptr(v.as.node);
        *out_len = mem_word_len(v.as.node);
        return true;
    }
    return false;
}

// Find the first case-insensitive occurrence of needle (length needle_len)
// in haystack (length haystack_len). Returns the offset into haystack, or
// SIZE_MAX if not found. An empty needle is treated as "not found" so that
// `member` and `member?` agree on empty-needle semantics for words.
static size_t find_word_in_word(const char *haystack, size_t haystack_len,
                                const char *needle, size_t needle_len)
{
    if (needle_len == 0 || needle_len > haystack_len)
        return SIZE_MAX;
    for (size_t i = 0; i + needle_len <= haystack_len; i++)
    {
        if (strncasecmp(haystack + i, needle, needle_len) == 0)
            return i;
    }
    return SIZE_MAX;
}

// Walk a list and return the cons cell whose car is values_equal to obj,
// or NODE_NIL if not found.
static Node find_element_in_list(Value obj, Node list)
{
    while (!mem_is_nil(list))
    {
        Node car = mem_car(list);
        Value car_val = mem_is_word(car) ? value_word(car) : value_list(car);
        if (values_equal(obj, car_val))
            return list;
        list = mem_cdr(list);
    }
    return NODE_NIL;
}

// Convert a member value (word, number, or list) to the node stored in a list
// cell, matching how fput/list build cells. Returns false and sets *out_err on
// an unstorable value (a non-object) or atom-table exhaustion. Shared by the
// destructive setters below.
static bool member_value_to_node(Value v, Node *out_node, Result *out_err)
{
    if (value_is_number(v))
    {
        Node w = number_to_word(v.as.number);
        if (mem_is_nil(w))
        {
            *out_err = result_error(ERR_OUT_OF_SPACE); // atom table exhausted
            return false;
        }
        *out_node = w;
        return true;
    }
    if (value_is_word(v) || value_is_list(v))
    {
        *out_node = v.as.node;
        return true;
    }
    *out_err = result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
    return false;
}

// .setfirst list value
// Destructively replaces the first member (car) of a non-empty list, in place,
// with value. Because lists share structure (butfirst returns the tail without
// copying), this mutates every list that shares the affected cell — the point
// is to update one element without allocating a fresh list. Dangerous, hence
// the leading dot: overwriting a cell that other structure relies on, or
// building a cyclic list, corrupts those references.
static Result prim_dsetfirst(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value list_val = args[0];
    if (!value_is_list(list_val) || mem_is_nil(list_val.as.node))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(list_val));
    }

    Node value_node;
    Result err;
    if (!member_value_to_node(args[1], &value_node, &err))
    {
        return err;
    }

    mem_set_car(list_val.as.node, value_node);
    return result_none();
}

// .setbf list value
// Destructively replaces the butfirst (cdr) of a non-empty list with value, in
// place; value must itself be a list, since the tail of a list is a list. Like
// .setfirst this mutates every list that shares the cell and can build a
// circular list if value points back into list — hence the leading dot.
static Result prim_dsetbf(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value list_val = args[0];
    if (!value_is_list(list_val) || mem_is_nil(list_val.as.node))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(list_val));
    }
    if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }

    mem_set_cdr(list_val.as.node, args[1].as.node);
    return result_none();
}

// .setitem index list value
// Destructively replaces the index-th member (1-based) of list with value, in
// place. Equivalent to walking to the index-th cell and .setfirst-ing it, but
// the walk happens in C. Errors like item: a non-positive index is bad input,
// an index past the end is too few items. Dangerous for the same reason as the
// other in-place setters.
static Result prim_dsetitem(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    float index_f;
    if (!value_to_number(args[0], &index_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    int index = (int)index_f;
    if (index < 1)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    if (!value_is_list(args[1]))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    Node cell = args[1].as.node;
    for (int i = 1; i < index && !mem_is_nil(cell); i++)
    {
        cell = mem_cdr(cell);
    }
    if (mem_is_nil(cell))
    {
        return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, "[]");
    }

    Node value_node;
    Result err;
    if (!member_value_to_node(args[2], &value_node, &err))
    {
        return err;
    }

    mem_set_car(cell, value_node);
    return result_none();
}

// member object1 object2
// Outputs the part of object2 starting with object1.
static Result prim_member(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value obj1 = args[0];
    Value obj2 = args[1];

    if (value_is_word(obj2) || value_is_number(obj2))
    {
        const char *str1;
        size_t len1;
        if (!value_as_word_str(obj1, &str1, &len1))
        {
            // List can never appear inside a word.
            return result_ok(value_word(mem_atom("", 0)));
        }

        const char *str2;
        size_t len2;
        (void)value_as_word_str(obj2, &str2, &len2);

        size_t pos = find_word_in_word(str2, len2, str1, len1);
        if (pos == SIZE_MAX)
            return result_ok(value_word(mem_atom("", 0)));
        return result_ok(value_word(mem_atom(str2 + pos, len2 - pos)));
    }
    else if (value_is_list(obj2))
    {
        Node found = find_element_in_list(obj1, obj2.as.node);
        return result_ok(value_list(found));
    }

    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj2));
}

//==========================================================================
// List/word construction: fput, list, lput, parse, sentence, word
//==========================================================================

// pick object
// Outputs a randomly chosen element of object (a character for a word).
static Result prim_pick(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value obj = args[0];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        size_t len = mem_word_len(obj.as.node);
        if (len == 0)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, value_to_string(obj));
        }
        Node ch = mem_atom(str + (logo_random_next(io) % len), 1);
        if (mem_is_nil(ch))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
        return result_ok(value_word(ch));
    }
    else if (value_is_list(obj))
    {
        size_t count = 0;
        for (Node n = obj.as.node; !mem_is_nil(n); n = mem_cdr(n))
        {
            count++;
        }
        if (count == 0)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, "[]");
        }

        Node list = obj.as.node;
        for (size_t i = logo_random_next(io) % count; i > 0; i--)
        {
            list = mem_cdr(list);
        }
        Node item = mem_car(list);
        if (mem_is_word(item))
        {
            return result_ok(value_word(item));
        }
        return result_ok(value_list(item));
    }

    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// reverse object
// Outputs object with its elements (characters for a word) in reverse order.
static Result prim_reverse(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value obj = args[0];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        size_t len = mem_word_len(obj.as.node);

        // Blob words (PSRAM-backed) can exceed the 255-byte atom limit, so
        // fall back to the heap for long inputs and build the result with
        // mem_word, which blobs long outputs.
        char stack_buf[256];
        char *buf = stack_buf;
        if (len > sizeof(stack_buf))
        {
            buf = malloc(len);
            if (!buf)
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
        }
        for (size_t i = 0; i < len; i++)
        {
            buf[i] = str[len - 1 - i];
        }
        Node rev = mem_word(buf, len);
        if (buf != stack_buf)
        {
            free(buf);
        }
        if (mem_is_nil(rev))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
        return result_ok(value_word(rev));
    }
    else if (value_is_list(obj))
    {
        // Prepending naturally reverses; no tail pointer needed.
        Node result = NODE_NIL;
        for (Node n = obj.as.node; !mem_is_nil(n); n = mem_cdr(n))
        {
            result = mem_cons(mem_car(n), result);
            if (mem_is_nil(result))
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
        }
        return result_ok(value_list(result));
    }

    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// shuffle object
// Outputs object with its elements (characters for a word) in random order.
static Result prim_shuffle(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value obj = args[0];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    LogoIO *io = primitives_get_io();
    if (!io)
    {
        return result_error_arg(ERR_UNSUPPORTED_ON_DEVICE, NULL, NULL);
    }

    if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        size_t len = mem_word_len(obj.as.node);

        // Blob words (PSRAM-backed) can exceed the 255-byte atom limit, so
        // fall back to the heap for long inputs and build the result with
        // mem_word, which blobs long outputs.
        char stack_buf[256];
        char *buf = stack_buf;
        if (len > sizeof(stack_buf))
        {
            buf = malloc(len);
            if (!buf)
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
        }
        memcpy(buf, str, len);
        // Fisher-Yates
        for (size_t i = len; i > 1; i--)
        {
            size_t j = logo_random_next(io) % i;
            char tmp = buf[i - 1];
            buf[i - 1] = buf[j];
            buf[j] = tmp;
        }
        Node shuffled = mem_word(buf, len);
        if (buf != stack_buf)
        {
            free(buf);
        }
        if (mem_is_nil(shuffled))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
        return result_ok(value_word(shuffled));
    }
    else if (value_is_list(obj))
    {
        size_t count = 0;
        for (Node n = obj.as.node; !mem_is_nil(n); n = mem_cdr(n))
        {
            count++;
        }
        if (count == 0)
        {
            return result_ok(value_list(NODE_NIL));
        }

        // Collect elements into a temporary array (the reduce/crossmap
        // precedent for bounded scratch space), shuffle, rebuild.
        Node *elements = malloc(count * sizeof(Node));
        if (!elements)
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
        size_t idx = 0;
        for (Node n = obj.as.node; !mem_is_nil(n); n = mem_cdr(n))
        {
            elements[idx++] = mem_car(n);
        }

        // Fisher-Yates
        for (size_t i = count; i > 1; i--)
        {
            size_t j = logo_random_next(io) % i;
            Node tmp = elements[i - 1];
            elements[i - 1] = elements[j];
            elements[j] = tmp;
        }

        Node result = NODE_NIL;
        Node tail = NODE_NIL;
        for (size_t i = 0; i < count; i++)
        {
            if (!mem_list_append(&result, &tail, elements[i]))
            {
                free(elements);
                return result_error(ERR_OUT_OF_SPACE);
            }
        }
        free(elements);
        return result_ok(value_list(result));
    }

    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// remove thing object
// Outputs a copy of object with every member equal to thing removed. A word's
// members are its characters, so thing only matches when it is that single
// character (equal? is case-insensitive); a list's elements are compared with
// equal? semantics.
static Result prim_remove(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value thing = args[0];
    Value obj = args[1];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        size_t len = mem_word_len(obj.as.node);

        // A character matches thing only when thing is exactly that one
        // character; a longer word or a list never matches a single char.
        const char *tstr;
        size_t tlen;
        bool thing_is_char = value_as_word_str(thing, &tstr, &tlen) && tlen == 1;

        char stack_buf[256];
        char *buf = stack_buf;
        if (len > sizeof(stack_buf))
        {
            buf = malloc(len);
            if (!buf)
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
        }
        size_t out = 0;
        for (size_t i = 0; i < len; i++)
        {
            if (thing_is_char && strncasecmp(&str[i], tstr, 1) == 0)
                continue;
            buf[out++] = str[i];
        }
        Node result = mem_word(buf, out);
        if (buf != stack_buf)
        {
            free(buf);
        }
        if (mem_is_nil(result))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
        return result_ok(value_word(result));
    }
    else if (value_is_list(obj))
    {
        Node result = NODE_NIL;
        Node tail = NODE_NIL;
        for (Node n = obj.as.node; !mem_is_nil(n); n = mem_cdr(n))
        {
            Node car = mem_car(n);
            Value car_val = mem_is_word(car) ? value_word(car) : value_list(car);
            if (values_equal(thing, car_val))
                continue;
            if (!mem_list_append(&result, &tail, car))
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
        }
        return result_ok(value_list(result));
    }

    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// remdup object
// Outputs a copy of object with duplicate members removed. When two or more
// members are equal, only the last is kept (UCB semantics). A word's members
// are its characters.
static Result prim_remdup(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value obj = args[0];
    if (!normalize_to_word(&obj))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    if (value_is_word(obj))
    {
        const char *str = mem_word_ptr(obj.as.node);
        size_t len = mem_word_len(obj.as.node);

        char stack_buf[256];
        char *buf = stack_buf;
        if (len > sizeof(stack_buf))
        {
            buf = malloc(len);
            if (!buf)
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
        }
        size_t out = 0;
        for (size_t i = 0; i < len; i++)
        {
            bool dup_later = false;
            for (size_t j = i + 1; j < len; j++)
            {
                if (strncasecmp(&str[i], &str[j], 1) == 0)
                {
                    dup_later = true;
                    break;
                }
            }
            if (!dup_later)
                buf[out++] = str[i];
        }
        Node result = mem_word(buf, out);
        if (buf != stack_buf)
        {
            free(buf);
        }
        if (mem_is_nil(result))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
        return result_ok(value_word(result));
    }
    else if (value_is_list(obj))
    {
        Node result = NODE_NIL;
        Node tail = NODE_NIL;
        for (Node n = obj.as.node; !mem_is_nil(n); n = mem_cdr(n))
        {
            Node car = mem_car(n);
            Value car_val = mem_is_word(car) ? value_word(car) : value_list(car);
            bool dup_later = false;
            for (Node m = mem_cdr(n); !mem_is_nil(m); m = mem_cdr(m))
            {
                Node c2 = mem_car(m);
                Value v2 = mem_is_word(c2) ? value_word(c2) : value_list(c2);
                if (values_equal(car_val, v2))
                {
                    dup_later = true;
                    break;
                }
            }
            if (dup_later)
                continue;
            if (!mem_list_append(&result, &tail, car))
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
        }
        return result_ok(value_list(result));
    }

    return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
}

// fput object list
// Outputs a new list with object at the beginning.
static Result prim_fput(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    Value obj = args[0];
    Value list_val = args[1];
    
    if (!value_is_list(list_val))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(list_val));
    }
    
    Node obj_node;
    if (value_is_word(obj))
    {
        obj_node = obj.as.node;
    }
    else if (value_is_list(obj))
    {
        obj_node = obj.as.node;
    }
    else if (value_is_number(obj))
    {
        // Convert number to word
        obj_node = number_to_word(obj.as.number);
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
    }
    
    Node result = mem_cons(obj_node, list_val.as.node);
    if (mem_is_nil(result))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    return result_ok(value_list(result));
}

// list object1 object2 ...
// Outputs a list of the inputs.
static Result prim_list(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    
    // Build list in reverse, then reverse
    Node result = NODE_NIL;
    for (int i = argc - 1; i >= 0; i--)
    {
        Node obj_node;
        if (value_is_word(args[i]))
        {
            obj_node = args[i].as.node;
        }
        else if (value_is_list(args[i]))
        {
            obj_node = args[i].as.node;
        }
        else if (value_is_number(args[i]))
        {
            obj_node = number_to_word(args[i].as.number);
        }
        else
        {
            continue;
        }
        result = mem_cons(obj_node, result);
        if (mem_is_nil(result))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
    }
    return result_ok(value_list(result));
}

// lput object list
// Outputs a new list with object at the end.
static Result prim_lput(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    Value obj = args[0];
    Value list_val = args[1];
    
    if (!value_is_list(list_val))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(list_val));
    }
    
    Node obj_node;
    if (value_is_word(obj))
    {
        obj_node = obj.as.node;
    }
    else if (value_is_list(obj))
    {
        obj_node = obj.as.node;
    }
    else if (value_is_number(obj))
    {
        obj_node = number_to_word(obj.as.number);
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
    }
    
    // Copy list and add object at end
    Node result = NODE_NIL;
    Node tail = NODE_NIL;
    Node list = list_val.as.node;
    while (!mem_is_nil(list))
    {
        if (!mem_list_append(&result, &tail, mem_car(list)))
        {
            return result_error(ERR_OUT_OF_SPACE);
        }
        list = mem_cdr(list);
    }

    // Add object at end
    if (!mem_list_append(&result, &tail, obj_node))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }

    return result_ok(value_list(result));
}

// parse word
// Outputs a list parsed from word. Nested bracketed groups produce
// sublists; operators and punctuation become single-character word
// elements (matching `readlist`'s behaviour).
static Result prim_parse(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;

    Value obj = args[0];
    const char *str;

    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        str = mem_word_ptr(word);
    }
    else if (value_is_word(obj))
    {
        str = mem_word_ptr(obj.as.node);
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
    }
    if (str == NULL)
    {
        return result_error(ERR_OUT_OF_SPACE); // atom table exhausted
    }

    ParseListResult parsed = parse_list_from_string(str);
    if (!parsed.success)
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    return result_ok(value_list(parsed.node));
}

// sentence object1 object2 ...
// Outputs a flat list of the contents of its inputs.
static Result prim_sentence(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    
    Node result = NODE_NIL;
    Node tail = NODE_NIL;
    
    for (int i = 0; i < argc; i++)
    {
        if (value_is_list(args[i]))
        {
            // Append all elements of the list
            Node list = args[i].as.node;
            while (!mem_is_nil(list))
            {
                if (!mem_list_append(&result, &tail, mem_car(list)))
                {
                    return result_error(ERR_OUT_OF_SPACE);
                }
                list = mem_cdr(list);
            }
        }
        else
        {
            // Add word or number as element
            Node obj_node;
            if (value_is_word(args[i]))
            {
                obj_node = args[i].as.node;
            }
            else if (value_is_number(args[i]))
            {
                obj_node = number_to_word(args[i].as.number);
            }
            else
            {
                continue;
            }

            if (!mem_list_append(&result, &tail, obj_node))
            {
                return result_error(ERR_OUT_OF_SPACE);
            }
        }
    }

    return result_ok(value_list(result));
}

// word word1 word2 ...
// Outputs a word made by concatenating inputs.
//
// Atoms are limited to 255 characters by the interner (1-byte length
// prefix). Rather than silently truncating an over-long concatenation
// (which historically produced a corrupted-looking partial result),
// return ERR_OUT_OF_SPACE so the user knows their data did not fit.
static Result prim_word(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);

    // Calculate total length needed and validate inputs.
    size_t total_len = 0;
    for (int i = 0; i < argc; i++)
    {
        if (value_is_number(args[i]))
        {
            Node word = number_to_word(args[i].as.number);
            const char *str = mem_word_ptr(word);
            if (str == NULL)
            {
                return result_error(ERR_OUT_OF_SPACE); // atom table exhausted
            }
            total_len += strlen(str);
        }
        else if (value_is_word(args[i]))
        {
            total_len += mem_word_len(args[i].as.node);
        }
        else
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[i]));
        }
    }

    // 255 is the hard atom-size limit; refuse rather than truncate.
    if (total_len > 255)
    {
        return result_error_arg(ERR_OUT_OF_SPACE, "word", NULL);
    }

    char buffer[256];
    char *p = buffer;

    for (int i = 0; i < argc; i++)
    {
        const char *str;
        size_t len;

        if (value_is_number(args[i]))
        {
            Node word = number_to_word(args[i].as.number);
            str = mem_word_ptr(word);
            if (str == NULL)
            {
                return result_error(ERR_OUT_OF_SPACE); // atom table exhausted
            }
            len = strlen(str);
        }
        else
        {
            str = mem_word_ptr(args[i].as.node);
            len = mem_word_len(args[i].as.node);
        }

        memcpy(p, str, len);
        p += len;
    }
    *p = '\0';

    Node result = mem_atom_cstr(buffer);
    if (mem_is_nil(result))
    {
        return result_error(ERR_OUT_OF_SPACE); // atom table exhausted
    }
    return result_ok(value_word(result));
}

//==========================================================================
// Character operations and predicates
//==========================================================================

// ascii character
// Outputs the ASCII code of character.
static Result prim_ascii(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    Value obj = args[0];
    const char *str;
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        str = mem_word_ptr(word);
    }
    else if (value_is_word(obj))
    {
        str = mem_word_ptr(obj.as.node);
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
    }
    
    if (str == NULL || str[0] == '\0')
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, "empty word");
    }
    
    return result_ok(value_number((float)(unsigned char)str[0]));
}

// before? word1 word2
// Outputs true if word1 comes before word2 lexicographically.
static Result prim_beforep(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    const char *str1;
    const char *str2;
    
    if (value_is_number(args[0]))
    {
        Node word = number_to_word(args[0].as.number);
        str1 = mem_word_ptr(word);
    }
    else if (value_is_word(args[0]))
    {
        str1 = mem_word_ptr(args[0].as.node);
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    if (value_is_number(args[1]))
    {
        Node word = number_to_word(args[1].as.number);
        str2 = mem_word_ptr(word);
    }
    else if (value_is_word(args[1]))
    {
        str2 = mem_word_ptr(args[1].as.node);
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }
    
    // Case-sensitive comparison (uppercase comes before lowercase in ASCII)
    int cmp = strcmp(str1, str2);
    
    Node result = (cmp < 0) ? mem_true_node : mem_false_node;
    return result_ok(value_word(result));
}

// char integer
// Outputs the character with ASCII code integer.
static Result prim_char(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    REQUIRE_NUMBER(args[0], n);
    
    int code = (int)n;
    if (code < 0 || code > 255)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }
    
    char buf[2] = { (char)code, '\0' };
    Node result = mem_atom(buf, 1);
    return result_ok(value_word(result));
}

// equal? object1 object2
// Outputs true if objects are equal.
static Result prim_equalp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    bool equal = values_equal(args[0], args[1]);
    Node result = (equal) ? mem_true_node : mem_false_node;
    return result_ok(value_word(result));
}

// list? object
// Outputs true if object is a list.
static Result prim_listp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    bool is_list = value_is_list(args[0]);
    Node result = (is_list) ? mem_true_node : mem_false_node;
    return result_ok(value_word(result));
}

// member? object1 object2
// Outputs true if object1 is an element of object2.
static Result prim_memberp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);

    Value obj1 = args[0];
    Value obj2 = args[1];

    bool found = false;

    if (value_is_word(obj2) || value_is_number(obj2))
    {
        const char *str1;
        size_t len1;
        if (!value_as_word_str(obj1, &str1, &len1))
        {
            // List is never an element of a word.
            return result_ok(value_bool(false));
        }

        const char *str2;
        size_t len2;
        (void)value_as_word_str(obj2, &str2, &len2);

        found = (find_word_in_word(str2, len2, str1, len1) != SIZE_MAX);
    }
    else if (value_is_list(obj2))
    {
        found = !mem_is_nil(find_element_in_list(obj1, obj2.as.node));
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj2));
    }

    return result_ok(value_bool(found));
}

// number? object
// Outputs true if object is a number.
static Result prim_numberp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    bool is_number = value_is_number(args[0]);
    if (!is_number && value_is_word(args[0]))
    {
        // Check if word can be parsed as number
        float n;
        is_number = value_to_number(args[0], &n);
    }
    
    Node result = (is_number) ? mem_true_node : mem_false_node;
    return result_ok(value_word(result));
}

// word? object
// Outputs true if object is a word.
static Result prim_wordp(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    // Numbers are also words (self-quoting)
    bool is_word = value_is_word(args[0]) || value_is_number(args[0]);
    Node result = (is_word) ? mem_true_node : mem_false_node;
    return result_ok(value_word(result));
}

// lowercase word
// Outputs word in all lowercase letters.
static Result prim_lowercase(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    Value obj = args[0];
    const char *str;
    size_t len;
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        str = mem_word_ptr(word);
        len = strlen(str);
    }
    else if (value_is_word(obj))
    {
        str = mem_word_ptr(obj.as.node);
        len = mem_word_len(obj.as.node);
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
    }

    // Atoms are capped at 255 chars by the interner, so a 256-byte
    // buffer is always sufficient for any valid input.
    assert(len < 256);
    char buffer[256];
    for (size_t i = 0; i < len; i++)
    {
        buffer[i] = (char)tolower((unsigned char)str[i]);
    }
    buffer[len] = '\0';

    Node result = mem_atom_cstr(buffer);
    return result_ok(value_word(result));
}

// uppercase word
// Outputs word in all uppercase letters.
static Result prim_uppercase(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    Value obj = args[0];
    const char *str;
    size_t len;
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        str = mem_word_ptr(word);
        len = strlen(str);
    }
    else if (value_is_word(obj))
    {
        str = mem_word_ptr(obj.as.node);
        len = mem_word_len(obj.as.node);
    }
    else
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
    }

    // Atoms are capped at 255 chars by the interner, so a 256-byte
    // buffer is always sufficient for any valid input.
    assert(len < 256);
    char buffer[256];
    for (size_t i = 0; i < len; i++)
    {
        buffer[i] = (char)toupper((unsigned char)str[i]);
    }
    buffer[len] = '\0';

    Node result = mem_atom_cstr(buffer);
    return result_ok(value_word(result));
}

void primitives_words_lists_init(void)
{
    // Basic element access
    primitive_register("first", 1, prim_first);
    primitive_register("last", 1, prim_last);
    primitive_register("butfirst", 1, prim_butfirst);
    primitive_register("bf", 1, prim_butfirst);
    primitive_register("butlast", 1, prim_butlast);
    primitive_register("bl", 1, prim_butlast);
    primitive_register("item", 2, prim_item);
    primitive_register("replace", 3, prim_replace);
    primitive_register(".setfirst", 2, prim_dsetfirst);
    primitive_register(".setbf", 2, prim_dsetbf);
    primitive_register(".setitem", 3, prim_dsetitem);
    primitive_register("member", 2, prim_member);
    primitive_register("pick", 1, prim_pick);
    primitive_register("reverse", 1, prim_reverse);
    primitive_register("shuffle", 1, prim_shuffle);
    primitive_register("remove", 2, prim_remove);
    primitive_register("remdup", 1, prim_remdup);
    
    // List construction
    primitive_register("fput", 2, prim_fput);
    primitive_register("list", 2, prim_list);
    primitive_register("lput", 2, prim_lput);
    primitive_register("sentence", 2, prim_sentence);
    primitive_register("se", 2, prim_sentence);
    
    // Word operations
    primitive_register("word", 2, prim_word);
    primitive_register("parse", 1, prim_parse);
    
    // Character operations
    primitive_register("ascii", 1, prim_ascii);
    primitive_register("char", 1, prim_char);
    primitive_register("lowercase", 1, prim_lowercase);
    primitive_register("uppercase", 1, prim_uppercase);
    
    // Predicates
    primitive_register("count", 1, prim_count);
    primitive_register("emptyp", 1, prim_emptyp);
    primitive_register("empty?", 1, prim_emptyp);
    primitive_register("equalp", 2, prim_equalp);
    primitive_register("equal?", 2, prim_equalp);
    primitive_register("listp", 1, prim_listp);
    primitive_register("list?", 1, prim_listp);
    primitive_register("memberp", 2, prim_memberp);
    primitive_register("member?", 2, prim_memberp);
    primitive_register("numberp", 1, prim_numberp);
    primitive_register("number?", 1, prim_numberp);
    primitive_register("wordp", 1, prim_wordp);
    primitive_register("word?", 1, prim_wordp);
    primitive_register("beforep", 2, prim_beforep);
    primitive_register("before?", 2, prim_beforep);
}
