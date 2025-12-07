//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Words and Lists primitives: first, last, butfirst, butlast, etc.
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include <string.h>
#include <stdio.h>

// Helper: convert a number value to its word representation
static Node number_to_word(float n)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", n);
    return mem_atom_cstr(buf);
}

// first object
// Outputs the first element of object.
// For a word: outputs the first character as a word
// For a list: outputs the first element (word or list)
static Result prim_first(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value obj = args[0];
    
    if (value_is_number(obj))
    {
        // Convert number to word first
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        if (str == NULL || str[0] == '\0')
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, "first", value_to_string(obj));
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
            return result_error_arg(ERR_TOO_FEW_ITEMS, "first", value_to_string(obj));
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
            return result_error_arg(ERR_TOO_FEW_ITEMS, "first", "[]");
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
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, "first", value_to_string(obj));
}

// last object
// Outputs the last element of object.
static Result prim_last(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value obj = args[0];
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        size_t len = strlen(str);
        if (len == 0)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, "last", value_to_string(obj));
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
            return result_error_arg(ERR_TOO_FEW_ITEMS, "last", value_to_string(obj));
        }
        Node last_char = mem_atom(str + len - 1, 1);
        return result_ok(value_word(last_char));
    }
    else if (value_is_list(obj))
    {
        Node list = obj.as.node;
        if (mem_is_nil(list))
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, "last", "[]");
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
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, "last", value_to_string(obj));
}

// butfirst object (bf)
// Outputs object with its first element removed.
static Result prim_butfirst(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value obj = args[0];
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        size_t len = strlen(str);
        if (len <= 1)
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
        if (len <= 1)
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
            return result_error_arg(ERR_TOO_FEW_ITEMS, "butfirst", "[]");
        }
        return result_ok(value_list(mem_cdr(list)));
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, "butfirst", value_to_string(obj));
}

// butlast object (bl)
// Outputs object with its last element removed.
static Result prim_butlast(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value obj = args[0];
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        size_t len = strlen(str);
        if (len <= 1)
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
        if (len <= 1)
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
            return result_error_arg(ERR_TOO_FEW_ITEMS, "butlast", "[]");
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
            Node new_cons = mem_cons(mem_car(list), NODE_NIL);
            if (mem_is_nil(result))
            {
                result = new_cons;
                tail = new_cons;
            }
            else
            {
                mem_set_cdr(tail, new_cons);
                tail = new_cons;
            }
            list = mem_cdr(list);
        }
        return result_ok(value_list(result));
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, "butlast", value_to_string(obj));
}

// count object
// Outputs the number of elements in object.
static Result prim_count(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
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
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, "count", value_to_string(obj));
}

// emptyp object (empty?)
// Outputs TRUE if object is empty word or empty list.
static Result prim_emptyp(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value obj = args[0];
    Node true_word = mem_atom_cstr("true");
    Node false_word = mem_atom_cstr("false");
    
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
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, "emptyp", value_to_string(obj));
}

void primitives_words_lists_init(void)
{
    primitive_register("first", 1, prim_first);
    primitive_register("last", 1, prim_last);
    primitive_register("butfirst", 1, prim_butfirst);
    primitive_register("bf", 1, prim_butfirst);
    primitive_register("butlast", 1, prim_butlast);
    primitive_register("bl", 1, prim_butlast);
    primitive_register("count", 1, prim_count);
    primitive_register("emptyp", 1, prim_emptyp);
    primitive_register("empty?", 1, prim_emptyp);
}
