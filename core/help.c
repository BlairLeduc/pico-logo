//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Help system: binary search over sorted help entries.
//

#include "help.h"
#include <stddef.h>
#include <strings.h>

bool help_check_sorted(void)
{
    for (int i = 1; i < help_entry_count; i++)
    {
        if (strcasecmp(help_entries[i - 1].name, help_entries[i].name) >= 0)
        {
            return false;
        }
    }
    return true;
}

bool help_contains_nocase(const char *haystack, const char *needle)
{
    if (!haystack || !needle || !needle[0])
    {
        return false;
    }

    size_t nlen = 0;
    while (needle[nlen]) nlen++;

    for (const char *h = haystack; *h; h++)
    {
        if (strncasecmp(h, needle, nlen) == 0)
        {
            return true;
        }
    }
    return false;
}

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
