//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "primitives.h"
#include <string.h>
#include <strings.h>

#define MAX_PRIMITIVES 128

static Primitive primitives[MAX_PRIMITIVES];
static int primitive_count = 0;

void primitives_init(void)
{
    primitive_count = 0;

    // Initialize all primitive categories
    primitives_arithmetic_init();
    primitives_control_init();
    primitives_variables_init();
    primitives_output_init();
    primitives_words_lists_init();
    primitives_procedures_init();
}

void primitive_register(const char *name, int default_args, PrimitiveFunc func)
{
    if (primitive_count >= MAX_PRIMITIVES)
        return;

    primitives[primitive_count++] = (Primitive){
        .name = name,
        .default_args = default_args,
        .func = func};
}

const Primitive *primitive_find(const char *name)
{
    for (int i = 0; i < primitive_count; i++)
    {
        if (strcasecmp(primitives[i].name, name) == 0)
        {
            return &primitives[i];
        }
    }
    return NULL;
}
