//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//

#include "primitives.h"
#include "devices/io.h"
#include <string.h>
#include <strings.h>

#define MAX_PRIMITIVES 512

static Primitive primitives[MAX_PRIMITIVES];
static int primitive_count = 0;

// Shared I/O manager for all primitives (new)
static LogoIO *shared_io = NULL;

void primitives_set_io(LogoIO *io)
{
    shared_io = io;
}

LogoIO *primitives_get_io(void)
{
    return shared_io;
}

void primitives_init(void)
{
    primitive_count = 0;

    // Initialize all primitive categories
    primitives_arithmetic_init();
    primitives_conditionals_init();
    primitives_control_flow_init();
    primitives_debug_control_init();
    primitives_exceptions_init();
    primitives_logical_init();
    primitives_variables_init();
    primitives_words_lists_init();
    primitives_procedures_init();
    primitives_workspace_init();
    primitives_outside_world_init();
    primitives_properties_init();
    primitives_debug_init();
    primitives_editor_init();
    primitives_files_init();
    primitives_text_init();
    primitives_turtle_init();
    primitives_hardware_init();
    primitives_list_processing_init();
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

bool primitive_register_alias(const char *alias_name, const Primitive *source)
{
    if (!alias_name || !source)
        return false;
    if (primitive_count >= MAX_PRIMITIVES)
        return false;

    primitives[primitive_count++] = (Primitive){
        .name = alias_name,
        .default_args = source->default_args,
        .func = source->func};
    return true;
}
