//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Incremental search and replace over the editor buffer
//

#include "editor_search.h"

#include <string.h>

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

size_t editor_search_replace_all(char *text, size_t *text_len, size_t capacity,
                                 const char *needle, size_t needle_len,
                                 const char *replacement, size_t replacement_len)
{
    size_t len = *text_len;

    if (needle_len == 0 || needle_len > len) {
        return 0;
    }

    // Count the matches first: the replacements can only start once the whole
    // result is known to fit, or a buffer that filled up part way through would
    // leave the text half replaced
    size_t count = 0;
    for (size_t i = 0; i + needle_len <= len; ) {
        if (match_at(text, needle, needle_len, i)) {
            count++;
            i += needle_len;
        } else {
            i++;
        }
    }

    if (count == 0) {
        return 0;
    }

    // Subtract before adding: the text always holds at least the matches it counted
    size_t new_len = len - count * needle_len + count * replacement_len;
    if (new_len + 1 > capacity) {
        return 0;
    }

    for (size_t i = 0; i + needle_len <= len; ) {
        if (match_at(text, needle, needle_len, i)) {
            memmove(text + i + replacement_len, text + i + needle_len,
                    len - i - needle_len);
            memcpy(text + i, replacement, replacement_len);
            len = len - needle_len + replacement_len;
            i += replacement_len;  // Skip the replacement, so it is never matched
        } else {
            i++;
        }
    }

    text[len] = '\0';
    *text_len = len;
    return count;
}
