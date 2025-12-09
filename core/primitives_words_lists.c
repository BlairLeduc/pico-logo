//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Words and Lists primitives: first, last, butfirst, butlast, item, member,
//  fput, list, lput, parse, sentence, word, ascii, before?, char, equal?,
//  list?, member?, number?, word?, lowercase, uppercase, count, emptyp
//

#include "primitives.h"
#include "error.h"
#include "eval.h"
#include "lexer.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>

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

// item integer object
// Outputs the element at position integer within object.
static Result prim_item(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    float index_f;
    if (!value_to_number(args[0], &index_f))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "item", value_to_string(args[0]));
    }
    int index = (int)index_f;
    if (index < 1)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "item", value_to_string(args[0]));
    }
    
    Value obj = args[1];
    
    if (value_is_number(obj))
    {
        Node word = number_to_word(obj.as.number);
        const char *str = mem_word_ptr(word);
        size_t len = strlen(str);
        if ((size_t)index > len)
        {
            return result_error_arg(ERR_TOO_FEW_ITEMS, "item", value_to_string(obj));
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
            return result_error_arg(ERR_TOO_FEW_ITEMS, "item", value_to_string(obj));
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
            return result_error_arg(ERR_TOO_FEW_ITEMS, "item", "[]");
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
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, "item", value_to_string(obj));
}

// Helper: compare two values for equality
static bool values_equal(Value a, Value b)
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
    return false;
}

// member object1 object2
// Outputs the part of object2 starting with object1.
static Result prim_member(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value obj1 = args[0];
    Value obj2 = args[1];
    
    if (value_is_word(obj2) || value_is_number(obj2))
    {
        // For words, obj1 must be a single character
        const char *str1;
        if (value_is_number(obj1))
        {
            Node word = number_to_word(obj1.as.number);
            str1 = mem_word_ptr(word);
        }
        else if (value_is_word(obj1))
        {
            str1 = mem_word_ptr(obj1.as.node);
        }
        else
        {
            // List cannot be member of word
            Node empty = mem_atom("", 0);
            return result_ok(value_word(empty));
        }
        
        const char *str2;
        size_t len2;
        if (value_is_number(obj2))
        {
            Node word = number_to_word(obj2.as.number);
            str2 = mem_word_ptr(word);
            len2 = strlen(str2);
        }
        else
        {
            str2 = mem_word_ptr(obj2.as.node);
            len2 = mem_word_len(obj2.as.node);
        }
        
        // Search for str1 in str2
        size_t len1 = strlen(str1);
        if (len1 == 0)
        {
            Node empty = mem_atom("", 0);
            return result_ok(value_word(empty));
        }
        
        for (size_t i = 0; i + len1 <= len2; i++)
        {
            if (strncasecmp(str2 + i, str1, len1) == 0)
            {
                Node result = mem_atom(str2 + i, len2 - i);
                return result_ok(value_word(result));
            }
        }
        
        Node empty = mem_atom("", 0);
        return result_ok(value_word(empty));
    }
    else if (value_is_list(obj2))
    {
        // Search for obj1 as element in list
        Node list = obj2.as.node;
        while (!mem_is_nil(list))
        {
            Node car = mem_car(list);
            Value car_val = mem_is_word(car) ? value_word(car) : value_list(car);
            if (values_equal(obj1, car_val))
            {
                return result_ok(value_list(list));
            }
            list = mem_cdr(list);
        }
        return result_ok(value_list(NODE_NIL));
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, "member", value_to_string(obj2));
}

// fput object list
// Outputs a new list with object at the beginning.
static Result prim_fput(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value obj = args[0];
    Value list_val = args[1];
    
    if (!value_is_list(list_val))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "fput", value_to_string(list_val));
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "fput", value_to_string(obj));
    }
    
    Node result = mem_cons(obj_node, list_val.as.node);
    return result_ok(value_list(result));
}

// list object1 object2 ...
// Outputs a list of the inputs.
static Result prim_list(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
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
    }
    return result_ok(value_list(result));
}

// lput object list
// Outputs a new list with object at the end.
static Result prim_lput(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value obj = args[0];
    Value list_val = args[1];
    
    if (!value_is_list(list_val))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "lput", value_to_string(list_val));
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "lput", value_to_string(obj));
    }
    
    // If list is empty, just return single-element list
    if (mem_is_nil(list_val.as.node))
    {
        Node result = mem_cons(obj_node, NODE_NIL);
        return result_ok(value_list(result));
    }
    
    // Copy list and add object at end
    Node result = NODE_NIL;
    Node tail = NODE_NIL;
    Node list = list_val.as.node;
    while (!mem_is_nil(list))
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
    
    // Add object at end
    Node last = mem_cons(obj_node, NODE_NIL);
    mem_set_cdr(tail, last);
    
    return result_ok(value_list(result));
}

// parse word
// Outputs a list parsed from word.
static Result prim_parse(Evaluator *eval, int argc, Value *args)
{
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "parse", value_to_string(obj));
    }
    
    // Use the evaluator to parse the string as a list
    Lexer lexer;
    lexer_init(&lexer, str);
    
    // Save old lexer state
    Lexer *old_lexer = eval->lexer;
    Token old_current = eval->current;
    bool old_has_current = eval->has_current;
    
    // Set up for parsing
    eval->lexer = &lexer;
    eval->has_current = false;
    
    // Parse tokens into a list
    Node result = NODE_NIL;
    Node tail = NODE_NIL;
    
    while (true)
    {
        Token t = lexer_next_token(&lexer);
        if (t.type == TOKEN_EOF)
            break;
        
        Node item = NODE_NIL;
        
        if (t.type == TOKEN_LEFT_BRACKET)
        {
            // Parse nested list - this is complex, so for now just store the bracket
            // In a full implementation, we'd recursively parse
            int depth = 1;
            const char *start = t.start;
            while (depth > 0 && !lexer_is_at_end(&lexer))
            {
                t = lexer_next_token(&lexer);
                if (t.type == TOKEN_LEFT_BRACKET) depth++;
                else if (t.type == TOKEN_RIGHT_BRACKET) depth--;
            }
            // For simplicity, just skip list content in parse
            continue;
        }
        else if (t.type == TOKEN_RIGHT_BRACKET)
        {
            break;
        }
        else if (t.type == TOKEN_WORD || t.type == TOKEN_NUMBER)
        {
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_QUOTED)
        {
            // Include the quote mark
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_COLON)
        {
            // Include the colon
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_PLUS || t.type == TOKEN_MINUS ||
                 t.type == TOKEN_UNARY_MINUS ||
                 t.type == TOKEN_MULTIPLY || t.type == TOKEN_DIVIDE ||
                 t.type == TOKEN_EQUALS || t.type == TOKEN_LESS_THAN ||
                 t.type == TOKEN_GREATER_THAN)
        {
            item = mem_atom(t.start, t.length);
        }
        else if (t.type == TOKEN_LEFT_PAREN || t.type == TOKEN_RIGHT_PAREN)
        {
            item = mem_atom(t.start, t.length);
        }
        else
        {
            continue;
        }
        
        Node new_cons = mem_cons(item, NODE_NIL);
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
    }
    
    // Restore old lexer state
    eval->lexer = old_lexer;
    eval->current = old_current;
    eval->has_current = old_has_current;
    
    return result_ok(value_list(result));
}

// sentence object1 object2 ...
// Outputs a flat list of the contents of its inputs.
static Result prim_sentence(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
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
            
            Node new_cons = mem_cons(obj_node, NODE_NIL);
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
        }
    }
    
    return result_ok(value_list(result));
}

// word word1 word2 ...
// Outputs a word made by concatenating inputs.
static Result prim_word(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    
    // Calculate total length needed
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
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, "word", value_to_string(args[i]));
        }
    }
    
    // Build concatenated string
    char buffer[256];  // Reasonable limit for words
    if (total_len >= sizeof(buffer))
    {
        total_len = sizeof(buffer) - 1;
    }
    
    char *p = buffer;
    size_t remaining = sizeof(buffer) - 1;
    
    for (int i = 0; i < argc && remaining > 0; i++)
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
        
        if (len > remaining) len = remaining;
        memcpy(p, str, len);
        p += len;
        remaining -= len;
    }
    *p = '\0';
    
    Node result = mem_atom_cstr(buffer);
    return result_ok(value_word(result));
}

// ascii character
// Outputs the ASCII code of character.
static Result prim_ascii(Evaluator *eval, int argc, Value *args)
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "ascii", value_to_string(obj));
    }
    
    if (str == NULL || str[0] == '\0')
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "ascii", "empty word");
    }
    
    return result_ok(value_number((float)(unsigned char)str[0]));
}

// before? word1 word2
// Outputs true if word1 comes before word2 lexicographically.
static Result prim_beforep(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "before?", value_to_string(args[0]));
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "before?", value_to_string(args[1]));
    }
    
    // Case-sensitive comparison (uppercase comes before lowercase in ASCII)
    int cmp = strcmp(str1, str2);
    
    Node result = mem_atom_cstr(cmp < 0 ? "true" : "false");
    return result_ok(value_word(result));
}

// char integer
// Outputs the character with ASCII code integer.
static Result prim_char(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    float n;
    if (!value_to_number(args[0], &n))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "char", value_to_string(args[0]));
    }
    
    int code = (int)n;
    if (code < 0 || code > 255)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "char", value_to_string(args[0]));
    }
    
    char buf[2] = { (char)code, '\0' };
    Node result = mem_atom(buf, 1);
    return result_ok(value_word(result));
}

// equal? object1 object2
// Outputs true if objects are equal.
static Result prim_equalp(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    bool equal = values_equal(args[0], args[1]);
    Node result = mem_atom_cstr(equal ? "true" : "false");
    return result_ok(value_word(result));
}

// list? object
// Outputs true if object is a list.
static Result prim_listp(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    bool is_list = value_is_list(args[0]);
    Node result = mem_atom_cstr(is_list ? "true" : "false");
    return result_ok(value_word(result));
}

// member? object1 object2
// Outputs true if object1 is an element of object2.
static Result prim_memberp(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    Value obj1 = args[0];
    Value obj2 = args[1];
    
    if (value_is_word(obj2) || value_is_number(obj2))
    {
        // For words, obj1 must be a single character
        const char *str1;
        size_t len1;
        
        if (value_is_number(obj1))
        {
            Node word = number_to_word(obj1.as.number);
            str1 = mem_word_ptr(word);
            len1 = strlen(str1);
        }
        else if (value_is_word(obj1))
        {
            str1 = mem_word_ptr(obj1.as.node);
            len1 = mem_word_len(obj1.as.node);
        }
        else
        {
            Node result = mem_atom_cstr("false");
            return result_ok(value_word(result));
        }
        
        const char *str2;
        size_t len2;
        
        if (value_is_number(obj2))
        {
            Node word = number_to_word(obj2.as.number);
            str2 = mem_word_ptr(word);
            len2 = strlen(str2);
        }
        else
        {
            str2 = mem_word_ptr(obj2.as.node);
            len2 = mem_word_len(obj2.as.node);
        }
        
        // Search for str1 in str2
        for (size_t i = 0; i + len1 <= len2; i++)
        {
            if (strncasecmp(str2 + i, str1, len1) == 0)
            {
                Node result = mem_atom_cstr("true");
                return result_ok(value_word(result));
            }
        }
        
        Node result = mem_atom_cstr("false");
        return result_ok(value_word(result));
    }
    else if (value_is_list(obj2))
    {
        Node list = obj2.as.node;
        while (!mem_is_nil(list))
        {
            Node car = mem_car(list);
            Value car_val = mem_is_word(car) ? value_word(car) : value_list(car);
            if (values_equal(obj1, car_val))
            {
                Node result = mem_atom_cstr("true");
                return result_ok(value_word(result));
            }
            list = mem_cdr(list);
        }
        Node result = mem_atom_cstr("false");
        return result_ok(value_word(result));
    }
    
    return result_error_arg(ERR_DOESNT_LIKE_INPUT, "member?", value_to_string(obj2));
}

// number? object
// Outputs true if object is a number.
static Result prim_numberp(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    bool is_number = value_is_number(args[0]);
    if (!is_number && value_is_word(args[0]))
    {
        // Check if word can be parsed as number
        float n;
        is_number = value_to_number(args[0], &n);
    }
    
    Node result = mem_atom_cstr(is_number ? "true" : "false");
    return result_ok(value_word(result));
}

// word? object
// Outputs true if object is a word.
static Result prim_wordp(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
    // Numbers are also words (self-quoting)
    bool is_word = value_is_word(args[0]) || value_is_number(args[0]);
    Node result = mem_atom_cstr(is_word ? "true" : "false");
    return result_ok(value_word(result));
}

// lowercase word
// Outputs word in all lowercase letters.
static Result prim_lowercase(Evaluator *eval, int argc, Value *args)
{
    (void)eval;
    (void)argc;
    
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "lowercase", value_to_string(obj));
    }
    
    char buffer[256];
    if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
    
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
    (void)eval;
    (void)argc;
    
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
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, "uppercase", value_to_string(obj));
    }
    
    char buffer[256];
    if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
    
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
