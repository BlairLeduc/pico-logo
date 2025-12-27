//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Property list storage using Logo's node system.
//
//  Property lists are stored as an association list of the form:
//    [[name1 prop1 val1 prop2 val2 ...] [name2 prop1 val1 ...] ...]
//
//  Each entry is a list where:
//    - First element is the name (word)
//    - Remaining elements are property-value pairs
//

#include "properties.h"
#include "memory.h"
#include "value.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

// The master property list: [[name plist...] [name plist...] ...]
static Node property_lists = NODE_NIL;

void properties_init(void)
{
    property_lists = NODE_NIL;
}

// Helper: Convert a Value to a Node for storage in property list
static Node prop_value_to_node(Value v)
{
    if (value_is_word(v) || value_is_list(v))
    {
        return v.as.node;
    }
    else if (value_is_number(v))
    {
        // Store numbers as word atoms
        char buf[32];
        format_number(buf, sizeof(buf), v.as.number);
        return mem_atom_cstr(buf);
    }
    return NODE_NIL;
}

// Helper: Convert a Node back to a Value
static Value prop_node_to_value(Node n)
{
    if (mem_is_nil(n))
    {
        return value_list(NODE_NIL);
    }
    else if (mem_is_word(n))
    {
        // Check if it's a number
        const char *str = mem_word_ptr(n);
        char *endptr;
        float num = strtof(str, &endptr);
        if (*endptr == '\0' && str[0] != '\0')
        {
            return value_number(num);
        }
        return value_word(n);
    }
    else if (mem_is_list(n))
    {
        return value_list(n);
    }
    return value_list(NODE_NIL);
}

// Find the entry for a name in the property list
// Returns the entry list [name prop1 val1 ...], or NODE_NIL if not found
static Node find_entry(const char *name)
{
    Node curr = property_lists;
    while (!mem_is_nil(curr))
    {
        Node entry = mem_car(curr);
        if (!mem_is_nil(entry))
        {
            Node entry_name = mem_car(entry);
            if (mem_is_word(entry_name) && 
                strcasecmp(mem_word_ptr(entry_name), name) == 0)
            {
                return entry;
            }
        }
        curr = mem_cdr(curr);
    }
    return NODE_NIL;
}

// Find a property within an entry's plist (the cdr of the entry)
// Returns the cons cell where car is the property name, or NODE_NIL if not found
static Node find_property_in_entry(Node entry, const char *property)
{
    // Entry is [name prop1 val1 prop2 val2 ...]
    // Skip the name, then search pairs
    Node curr = mem_cdr(entry);  // Skip name
    while (!mem_is_nil(curr))
    {
        Node prop_node = mem_car(curr);
        if (mem_is_word(prop_node) && 
            strcasecmp(mem_word_ptr(prop_node), property) == 0)
        {
            return curr;  // Return the cell containing the property name
        }
        // Skip to value
        curr = mem_cdr(curr);
        if (!mem_is_nil(curr))
        {
            // Skip value to next property
            curr = mem_cdr(curr);
        }
    }
    return NODE_NIL;
}

void prop_put(const char *name, const char *property, Value value)
{
    Node name_atom = mem_atom_cstr(name);
    Node prop_atom = mem_atom_cstr(property);
    Node val_node = prop_value_to_node(value);
    
    // Find existing entry for this name
    Node entry = find_entry(name);
    
    if (mem_is_nil(entry))
    {
        // Create new entry: [name property value]
        Node new_entry = mem_cons(val_node, NODE_NIL);
        new_entry = mem_cons(prop_atom, new_entry);
        new_entry = mem_cons(name_atom, new_entry);
        
        // Add to front of property_lists
        property_lists = mem_cons(new_entry, property_lists);
    }
    else
    {
        // Entry exists, find or add property
        Node prop_cell = find_property_in_entry(entry, property);
        
        if (!mem_is_nil(prop_cell))
        {
            // Property exists, update the value (next cell after prop_cell)
            Node val_cell = mem_cdr(prop_cell);
            if (!mem_is_nil(val_cell))
            {
                mem_set_car(val_cell, val_node);
            }
        }
        else
        {
            // Property doesn't exist, add it to the entry
            // Entry is [name existing_props...]
            // We insert [property value] after the name
            Node rest = mem_cdr(entry);
            Node new_pair = mem_cons(val_node, rest);
            new_pair = mem_cons(prop_atom, new_pair);
            mem_set_cdr(entry, new_pair);
        }
    }
}

bool prop_get(const char *name, const char *property, Value *out)
{
    Node entry = find_entry(name);
    if (mem_is_nil(entry))
    {
        *out = value_list(NODE_NIL);
        return false;
    }
    
    Node prop_cell = find_property_in_entry(entry, property);
    if (mem_is_nil(prop_cell))
    {
        *out = value_list(NODE_NIL);
        return false;
    }
    
    // Value is in the next cell
    Node val_cell = mem_cdr(prop_cell);
    if (mem_is_nil(val_cell))
    {
        *out = value_list(NODE_NIL);
        return false;
    }
    
    *out = prop_node_to_value(mem_car(val_cell));
    return true;
}

void prop_remove(const char *name, const char *property)
{
    Node entry = find_entry(name);
    if (mem_is_nil(entry))
    {
        return;
    }
    
    // Find the property and remove the pair
    // Entry is [name prop1 val1 prop2 val2 ...]
    Node prev = entry;  // Start at name cell
    Node curr = mem_cdr(entry);  // First property
    
    while (!mem_is_nil(curr))
    {
        Node prop_node = mem_car(curr);
        if (mem_is_word(prop_node) && 
            strcasecmp(mem_word_ptr(prop_node), property) == 0)
        {
            // Found it - skip this property and its value
            Node val_cell = mem_cdr(curr);
            Node after_val = mem_is_nil(val_cell) ? NODE_NIL : mem_cdr(val_cell);
            mem_set_cdr(prev, after_val);
            return;
        }
        
        // Move to next pair
        Node val_cell = mem_cdr(curr);
        if (mem_is_nil(val_cell))
            break;
        prev = val_cell;
        curr = mem_cdr(val_cell);
    }
}

Node prop_get_list(const char *name)
{
    Node entry = find_entry(name);
    if (mem_is_nil(entry))
    {
        return NODE_NIL;
    }
    
    // Return the property-value pairs (skip the name)
    return mem_cdr(entry);
}

bool prop_has_properties(const char *name)
{
    Node entry = find_entry(name);
    if (mem_is_nil(entry))
    {
        return false;
    }
    // Has properties if there's anything after the name
    return !mem_is_nil(mem_cdr(entry));
}

void prop_erase_all(void)
{
    property_lists = NODE_NIL;
}

int prop_name_count(void)
{
    int count = 0;
    Node curr = property_lists;
    while (!mem_is_nil(curr))
    {
        Node entry = mem_car(curr);
        // Only count entries that have properties (not just a name)
        if (!mem_is_nil(entry) && !mem_is_nil(mem_cdr(entry)))
        {
            count++;
        }
        curr = mem_cdr(curr);
    }
    return count;
}

bool prop_get_name_by_index(int index, const char **name_out)
{
    int count = 0;
    Node curr = property_lists;
    while (!mem_is_nil(curr))
    {
        Node entry = mem_car(curr);
        // Only count entries that have properties
        if (!mem_is_nil(entry) && !mem_is_nil(mem_cdr(entry)))
        {
            if (count == index)
            {
                Node name_node = mem_car(entry);
                if (mem_is_word(name_node))
                {
                    *name_out = mem_word_ptr(name_node);
                    return true;
                }
            }
            count++;
        }
        curr = mem_cdr(curr);
    }
    return false;
}

void prop_gc_mark_all(void)
{
    // Mark the entire property list structure
    mem_gc_mark(property_lists);
}
