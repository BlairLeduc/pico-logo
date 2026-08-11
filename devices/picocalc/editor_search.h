//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Incremental search over the editor buffer
//

#pragma once

#include <stdbool.h>
#include <stddef.h>

// Find needle in text, matching case-insensitively and cycling through all
// occurrences: a forward search that reaches the end continues from the start,
// a backward search that reaches the start continues from the end.
//
// text/text_len: the buffer to search (not null-terminated)
// needle/needle_len: the text to match
// from: forward searches start here; backward searches start just before here
// forward: true to search toward the end, false toward the start
// out_pos: set to the position of the match when one is found
//
// Returns true when a match is found.
bool editor_search_find(const char *text, size_t text_len,
                        const char *needle, size_t needle_len,
                        size_t from, bool forward, size_t *out_pos);
