//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Help system: lookup primitive help text by name.
//

#ifndef LOGO_HELP_H
#define LOGO_HELP_H

typedef struct {
    const char *name;
    const char *text;
} HelpEntry;

// Generated data (from help_data.c)
extern const HelpEntry help_entries[];
extern const int help_entry_count;

// Lookup help text by primitive name (case-insensitive).
// Returns the help text string, or NULL if not found.
const char *help_lookup(const char *name);

#endif // LOGO_HELP_H
