//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Incremental search and replace over the editor buffer
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

// Replace every occurrence of needle with replacement, matching case-insensitively
// as editor_search_find does. Occurrences do not overlap: matching resumes after
// each replacement, so replacing "aa" in "aaaa" changes two occurrences and a
// replacement that contains the needle is not matched again.
//
// text/text_len: the buffer to rewrite; text_len is updated to the new length
// capacity: the size of text, including room for the terminating NUL
// needle/needle_len: the text to match
// replacement/replacement_len: the text to put in its place (may be empty)
//
// Returns the number of occurrences replaced. Nothing is changed when there is
// no match, or when the result would not fit in capacity.
size_t editor_search_replace_all(char *text, size_t *text_len, size_t capacity,
                                 const char *needle, size_t needle_len,
                                 const char *replacement, size_t replacement_len);
