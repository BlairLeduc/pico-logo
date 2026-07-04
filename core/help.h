//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Help system: lookup primitive help text by name.
//

#ifndef LOGO_HELP_H
#define LOGO_HELP_H

#include <stdbool.h>

typedef struct {
    const char *name;
    const char *text;
    unsigned char category;   // index into help_categories[]
} HelpEntry;

// Generated data (from help_data.c).
//
// REQUIREMENT: `help_entries[]` MUST be sorted in ascending case-insensitive
// order by `name`. The lookup is a binary search and silently misbehaves if
// the table is unsorted. The generator script (see `scripts/`) is responsible
// for emitting entries in the correct order; `help_lookup` validates the
// invariant once on the first call (debug builds only) via
// `help_check_sorted`.
extern const HelpEntry help_entries[];
extern const int help_entry_count;

// Category names (reference chapter titles, in reference order).
extern const char *const help_categories[];
extern const int help_category_count;

// Lookup help text by primitive name (case-insensitive).
// Returns the help text string, or NULL if not found.
const char *help_lookup(const char *name);

// Verify the sort-order invariant on `help_entries[]`. Returns true if the
// table is correctly sorted, false otherwise. Exposed for unit tests.
bool help_check_sorted(void);

// Case-insensitive substring test (portable strcasestr).
// Used by the help keyword search; an empty or NULL needle matches nothing.
bool help_contains_nocase(const char *haystack, const char *needle);

#endif // LOGO_HELP_H
