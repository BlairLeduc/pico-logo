//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Vi's regular-expression dialect for `:s`, `/` and `?` (docs/vi-mode-design.md
//  §16). Deliberately NOT called editor_regex.c: it implements classic vi/ed
//  BRE, not POSIX, and the toolchain's <regex.h> has no arm-none-eabi libc.a
//  behind it anyway (§16.1). Pure and allocation-free -- the pattern is
//  interpreted straight out of the ViState bytes the ex parser already stored,
//  which is why, like editor_search.c and editor_lines.c, it gets a host test
//  that editor.c never can.
//

#pragma once

#include <stdbool.h>
#include <stddef.h>

// One captured group. end == start means unset (or an empty capture, which the
// callers treat the same). [0] is always the whole match.
typedef struct
{
    size_t start;
    size_t end;
} EditorPatternGroup;

typedef EditorPatternGroup EditorPatternGroups[10];

// The set (§16.2): ^ $ . *  [...] [^...] [a-z]  \< \>  \( \)  \1..\9, and a
// backslash before any other character makes it literal. `*` applies to a
// literal, `.` or a class, never to a group; `^` is only an anchor first, `$`
// only last. Matching is case-insensitive throughout (§16.7).

// True when `pat` is a well-formed pattern. Rejects a dangling backslash, an
// unclosed `[`, an unopened or unclosed `\(`, more than nine groups, a `\k`
// with no group k, and a `*` immediately after a `\)`. Run at parse time so a
// bad pattern beeps on the Return that typed it, never at match time.
bool editor_pattern_valid(const char *pat, size_t pat_len);

// Leftmost match of `pat` in a single line, at or after `from`. Never crosses a
// line break -- `line`/`line_len` is one line, so `^`, `$` and `.` have their
// bounds for free. On a match, fills `g` (offsets relative to `line`) and
// returns true. `g[0]` is the whole match; unset groups are left {0,0}.
//
// The call spends at most LOGO_VI_PATTERN_STEPS_MAX match steps. Past that it
// abandons the search and returns false with `*too_complex` set (B36) -- the
// two are distinct: false alone is an honest miss, false with the flag is a
// refusal, and the caller is expected to stop rather than try the next line.
// `too_complex` may be NULL when the caller cannot act on the difference.
bool editor_pattern_search(const char *pat, size_t pat_len,
                           const char *line, size_t line_len,
                           size_t from, EditorPatternGroups g,
                           bool *too_complex);

// Expand a replacement, resolving `&` (the whole match) and `\1`..`\9` from
// `g`, with `\&` and `\\` as the literal characters. `line` is the buffer the
// groups index into. Returns the number of bytes written to `out`, or SIZE_MAX
// if the result would exceed `out_cap` (nothing usable is written then, and the
// substitute loop turns that into a refusal before it moves a byte -- §16.4).
size_t editor_pattern_expand(const char *rep, size_t rep_len,
                             const char *line, const EditorPatternGroups g,
                             char *out, size_t out_cap);

// Search a whole buffer, line by line, with editor_search_find's contract:
// case-insensitive, wrapping past the ends. Forward matches within the line
// holding `from` starting at `from`, then each following line from its start,
// then wraps. Backward never matches backwards -- it scans each line forward
// and keeps the last match beginning before `from`, then walks to earlier lines
// and wraps (§16.5), so one matcher direction serves `/`, `?`, `n` and `N`.
// Sets *out_pos to the match start and returns true when one is found.
// `too_complex` carries editor_pattern_search's refusal (B36) out to the
// caller: the walk stops at the first line that trips the budget rather than
// paying it again on every line that follows. May be NULL.
bool editor_pattern_find(const char *pat, size_t pat_len,
                         const char *text, size_t text_len,
                         size_t from, bool forward, size_t *out_pos,
                         bool *too_complex);
