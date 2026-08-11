//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Incremental search over the editor buffer
//

#include "editor_search.h"

//
// Case-insensitive match of needle at a given position
//
static bool match_at(const char *text, const char *needle, size_t needle_len, size_t pos)
{
    for (size_t i = 0; i < needle_len; i++) {
        char a = text[pos + i];
        char b = needle[i];
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return false;
    }
    return true;
}

bool editor_search_find(const char *text, size_t text_len,
                        const char *needle, size_t needle_len,
                        size_t from, bool forward, size_t *out_pos)
{
    if (needle_len == 0 || needle_len > text_len) {
        return false;
    }

    size_t last = text_len - needle_len;  // Last position a match can start at

    if (forward) {
        // From `from` to the end, then wrap around to the positions before it
        for (size_t i = from; i <= last; i++) {
            if (match_at(text, needle, needle_len, i)) {
                *out_pos = i;
                return true;
            }
        }
        for (size_t i = 0; i < from && i <= last; i++) {
            if (match_at(text, needle, needle_len, i)) {
                *out_pos = i;
                return true;
            }
        }
    } else {
        // Just before `from` back to the start, then wrap around to the end
        for (size_t i = from > last + 1 ? last + 1 : from; i-- > 0; ) {
            if (match_at(text, needle, needle_len, i)) {
                *out_pos = i;
                return true;
            }
        }
        for (size_t i = last + 1; i-- > from; ) {
            if (match_at(text, needle, needle_len, i)) {
                *out_pos = i;
                return true;
            }
        }
    }

    return false;
}
