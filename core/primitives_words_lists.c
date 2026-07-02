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
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

//==========================================================================
// Element access: first, last, butfirst, butlast, item, replace, member
//==========================================================================

// first object
// Outputs the first element of object.
// For a word: outputs the first character as a word
// For a list: outputs the first element (word or list)
static Result prim_first(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval); UNUSED(argc);
    
    Value obj = args[0];
    
    if (value_is_number(obj))
    {
        // Convert number to word first
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        if (str == NULL || str[0] == '\0')
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, value_to_string(obj));
        }
        // Return first character as a word
        Node first_char = mem_atom(str, 1);
        return result_ok(value_word(first_char));
    }
    else if (value_is_word(obj))
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
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        size_t len = strlen(str);
        if (len == 0)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, value_to_string(obj));
        }
        Node last_char = mem_atom(str + len - 1, 1);
        return result_ok(value_word(last_char));
    }
    else if (value_is_word(obj))
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
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        size_t len = strlen(str);
        if (len == 0)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(obj));
        }
        if (len == 1)
        {
            // Return empty word
            Node empty = mem_atom("", 0);
            return result_ok(value_word(empty));
        }
        Node rest = mem_atom(str + 1, len - 1);
        return result_ok(value_word(rest));
    }
    else if (value_is_word(obj))
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
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        size_t len = strlen(str);
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
    else if (value_is_word(obj))
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
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        return result_ok(value_number((float)strlen(mem_word_ptr(word))));
    }
    else if (value_is_word(obj))
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
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        size_t len = strlen(str);
        if ((size_t)index > len)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, NULL, value_to_string(obj));
        }
        Node item_char = mem_atom(str + index - 1, 1);
        return result_ok(value_word(item_char));
    }
    else if (value_is_word(obj))
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
        *out_str = mem_word_ptr(word);
        *out_len = strlen(*out_str);
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
            total_len += strlen(mem_word_ptr(word));
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
    primitive_register("member", 2, prim_member);
    
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
