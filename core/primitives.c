//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//

#include "primitives.h"
#include "core/atom_memo.h"
#include "core/memory.h"
#include "random.h"
#include "frame_sync.h"
#include "devices/io.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_PRIMITIVES 512

// A resolved primitive is cached on the atom as an index into `primitives`
// (core/atom_memo.h), so the table may not outgrow the field that holds it.
_Static_assert(MAX_PRIMITIVES <= ATOM_MEMO_INDEX_LIMIT,
    "primitive index must fit the atom memo's binding field");

static Primitive primitives[MAX_PRIMITIVES];
static int primitive_count = 0;

// primitive_find runs for every word token evaluated, so a linear scan of
// ~300 names was the interpreter's largest constant overhead. Lookups
// binary-search `primitive_order`, a name-sorted (case-insensitive) array
// of indices into `primitives[]`. The table itself stays append-only, so
// `const Primitive *` pointers held by callers (e.g. staged OP_PRIM_CALLs)
// remain valid even when `copydef` registers an alias mid-evaluation.
// Sorting is deferred to the first lookup after a registration (lazy dirty
// flag), so registration order carries no contract.
static uint16_t primitive_order[MAX_PRIMITIVES];
static bool primitives_sorted = false;

// `output` and `op` put their argument in tail position, which the evaluator
// checks once per collected argument.  Resolving them by name there cost a
// strncasecmp per argument; caching the two entries here keeps the check to a
// pointer compare and keeps the cache with the table it points into, so
// primitives_init clears it rather than leaving it dangling.
static const Primitive *output_prim;
static const Primitive *op_prim;

static int primitive_order_compare(const void *a, const void *b)
{
    const Primitive *pa = &primitives[*(const uint16_t *)a];
    const Primitive *pb = &primitives[*(const uint16_t *)b];
    return strcasecmp(pa->name, pb->name);
}

// Shared I/O manager for all primitives (new)
static LogoIO *shared_io = NULL;

void primitives_set_io(LogoIO *io)
{
    shared_io = io;

    // The console arrives after primitives_init has run, so this is the first
    // moment the editor can be told anything (B34)
    primitives_editor_console_ready();
}

LogoIO *primitives_get_io(void)
{
    return shared_io;
}

void primitives_init(void)
{
    primitive_count = 0;
    primitives_sorted = false;
    output_prim = NULL;
    op_prim = NULL;
    logo_random_reset(); // default (device/TRNG) randomness until rerandom
    frame_sync_reset();  // start unpaced; setrefresh "sync opts in

    // Initialize all primitive categories
    primitives_arithmetic_init();
    primitives_conditionals_init();
    primitives_control_flow_init();
    primitives_debug_control_init();
    primitives_exceptions_init();
    primitives_logical_init();
    primitives_bitwise_init();
    primitives_variables_init();
    primitives_words_lists_init();
    primitives_procedures_init();
    primitives_workspace_init();
    primitives_outside_world_init();
    primitives_properties_init();
    primitives_json_init();
    primitives_debug_init();
    primitives_editor_init();
    primitives_files_init();
    primitives_files_directory_init();
    primitives_files_load_save_init();
    primitives_text_init();
    primitives_turtle_init();
    primitives_events_init();
    primitives_hardware_init();
    primitives_sound_init();
    primitives_speech_init();
    primitives_list_processing_init();
    primitives_wifi_init();
    primitives_network_init();
    primitives_http_init();
    primitives_httpd_init();
    primitives_time_init();
}

// Index of a registered primitive, or -1 if the pointer is not one of ours.
// The table is append-only, so an index stays valid for the run.
int primitive_index_of(const Primitive *prim)
{
    if (!prim || prim < primitives || prim >= primitives + primitive_count)
        return -1;
    return (int)(prim - primitives);
}

const Primitive *primitive_by_index(int index)
{
    if (index < 0 || index >= primitive_count)
        return NULL;
    return &primitives[index];
}

// Is this the `output` or `op` primitive? Identity, not name: an alias made by
// copydef is a separate entry and is not output, which is exactly what the
// evaluator's old comparison against the token text did.
bool primitive_is_output(const Primitive *prim)
{
    if (!prim)
        return false;
    if (!output_prim)
    {
        output_prim = primitive_find("output");
        op_prim = primitive_find("op");
    }
    return prim == output_prim || prim == op_prim;
}

void primitive_register(const char *name, int default_args, PrimitiveFunc func)
{
    if (primitive_count >= MAX_PRIMITIVES)
        return;

    primitives[primitive_count++] = (Primitive){
        .name = name,
        .default_args = default_args,
        .func = func};
    primitives_sorted = false;
}

// Order `name` (exactly `len` bytes, not NUL-terminated) against a registered
// primitive name the same way strcasecmp would, so the binary search below
// stays valid for both entry points.
//
// Reading `pname[len]` is in bounds: `name` holds no NUL in its `len` bytes
// (it is a word token, or a C string measured by strlen), so if `pname` were
// shorter, strncasecmp would have compared a real character against `pname`'s
// terminator and returned non-zero above. Reaching the last line therefore
// means strlen(pname) >= len, and index `len` is at worst the terminator.
static int primitive_name_compare(const char *name, size_t len, const char *pname)
{
    int cmp = strncasecmp(name, pname, len);
    if (cmp != 0)
        return cmp;
    // Equal over the first `len` bytes: `name` is shorter unless `pname` ends
    // there too.
    return pname[len] == '\0' ? 0 : -1;
}

const Primitive *primitive_find_n(const char *name, size_t len)
{
    if (!primitives_sorted)
    {
        for (int i = 0; i < primitive_count; i++)
        {
            primitive_order[i] = (uint16_t)i;
        }
        qsort(primitive_order, (size_t)primitive_count,
              sizeof(primitive_order[0]), primitive_order_compare);
        primitives_sorted = true;
    }

    int lo = 0;
    int hi = primitive_count - 1;
    while (lo <= hi)
    {
        int mid = lo + (hi - lo) / 2;
        const Primitive *p = &primitives[primitive_order[mid]];
        int cmp = primitive_name_compare(name, len, p->name);
        if (cmp == 0)
        {
            return p;
        }
        if (cmp < 0)
        {
            hi = mid - 1;
        }
        else
        {
            lo = mid + 1;
        }
    }
    return NULL;
}

const Primitive *primitive_find(const char *name)
{
    return primitive_find_n(name, strlen(name));
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
    primitives_sorted = false;
    // copydef can register an alias mid-evaluation, so a name that already
    // resolved to "neither a primitive nor a procedure" -- or to a user
    // procedure -- may now be a primitive. Drop the cached bindings.
    mem_atom_memo_mask_all(ATOM_MEMO_KEEP_CLASS);
    return true;
}

int primitive_get_count(void)
{
    return primitive_count;
}

const Primitive *primitive_get_by_index(int index)
{
    if (index < 0 || index >= primitive_count)
        return NULL;
    return &primitives[index];
}
