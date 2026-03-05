//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  Help system: binary search over sorted help entries.
//

#include "help.h"
#include <stddef.h>
#include <strings.h>

const char *help_lookup(const char *name)
{
    int lo = 0;
    int hi = help_entry_count - 1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = strcasecmp(name, help_entries[mid].name);
        if (cmp == 0) {
            return help_entries[mid].text;
        } else if (cmp < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }

    return NULL;
}
