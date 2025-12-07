//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "variables.h"
#include <string.h>
#include <strings.h>

// Simple linear storage for now (optimize later if needed)
#define MAX_VARIABLES 256

typedef struct
{
    const char *name; // Points to interned atom string
    Value value;
    bool active;
} Variable;

static Variable variables[MAX_VARIABLES];
static int variable_count = 0;

void variables_init(void)
{
    variable_count = 0;
    for (int i = 0; i < MAX_VARIABLES; i++)
    {
        variables[i].active = false;
    }
}

// Find variable index, returns -1 if not found
static int find_variable(const char *name)
{
    for (int i = 0; i < variable_count; i++)
    {
        if (variables[i].active && strcasecmp(variables[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

void var_set(const char *name, Value value)
{
    int idx = find_variable(name);
    if (idx >= 0)
    {
        variables[idx].value = value;
        return;
    }

    // Find free slot or append
    for (int i = 0; i < MAX_VARIABLES; i++)
    {
        if (!variables[i].active)
        {
            variables[i].name = name;
            variables[i].value = value;
            variables[i].active = true;
            if (i >= variable_count)
                variable_count = i + 1;
            return;
        }
    }
    // Out of space - silently fail for now
}

bool var_get(const char *name, Value *out)
{
    int idx = find_variable(name);
    if (idx >= 0)
    {
        *out = variables[idx].value;
        return true;
    }
    return false;
}

bool var_exists(const char *name)
{
    return find_variable(name) >= 0;
}

void var_erase(const char *name)
{
    int idx = find_variable(name);
    if (idx >= 0)
    {
        variables[idx].active = false;
    }
}

void var_erase_all(void)
{
    variables_init();
}
