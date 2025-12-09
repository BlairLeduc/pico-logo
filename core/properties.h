//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Property list storage for Logo interpreter.
//
//  In Logo, any word can have a property list associated with it.
//  A property list consists of property-value pairs.
//  Properties are accessed by name using gprop/pprop/remprop.
//

#pragma once

#include "value.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Initialize property list storage
    void properties_init(void);

    // Put a property value on a name's property list
    // pprop name property value
    void prop_put(const char *name, const char *property, Value value);

    // Get a property value from a name's property list
    // Returns true if found, false if not (out is empty list if not found)
    bool prop_get(const char *name, const char *property, Value *out);

    // Remove a property from a name's property list
    void prop_remove(const char *name, const char *property);

    // Get the entire property list for a name as a Logo list
    // Returns [prop1 val1 prop2 val2 ...]
    Node prop_get_list(const char *name);

    // Check if a name has any properties
    bool prop_has_properties(const char *name);

    // Erase all properties from the workspace
    void prop_erase_all(void);

    // Iteration support for pps (print property lists)
    int prop_name_count(void);
    bool prop_get_name_by_index(int index, const char **name_out);

    // Mark all property values as GC roots
    void prop_gc_mark_all(void);

#ifdef __cplusplus
}
#endif
