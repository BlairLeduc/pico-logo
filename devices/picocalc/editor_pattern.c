//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Vi's regular-expression dialect (docs/vi-mode-design.md §16). See the header
//  for the set. The matcher is the recursive greedy shape: `*` loops over
//  lengths and recurses once per length, so the stack depth follows the number
//  of atoms in the pattern (capped at LOGO_VI_TEXT_MAX) and not the line length,
//  and because `*` is refused on a group there is no nested quantifier to
//  produce catastrophic backtracking.
//

#include "editor_pattern.h"

#include <stdint.h>
#include <string.h>

static char to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool ci_eq(char a, char b)
{
    return to_lower(a) == to_lower(b);
}

// A "word" character for \< and \> is Logo's, not vi's: renaming `total` must
// not reach into `total.count` or `empty?`, so a name character is anything
// that is not blank, not `;`, not one of the eleven delimiters, and not the `"`
// or `:` a variable is prefixed with. This tracks is_delimiter (core/lexer.c);
// the pattern test is written from the same list so a change there fails here.
static bool is_name_char(char c)
{
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
    {
        return false;
    }
    if (c == ';' || c == '"' || c == ':')
    {
        return false;
    }
    // The eleven delimiters: [ ] ( ) + - * / = < >
    if (c == '[' || c == ']' || c == '(' || c == ')' ||
        c == '+' || c == '-' || c == '*' || c == '/' ||
        c == '=' || c == '<' || c == '>')
    {
        return false;
    }
    return true;
}

//
//  Validation (§16.6) -- run at parse time so a bad pattern beeps then, not on
//  a keystroke deep in a substitute.
//

bool editor_pattern_valid(const char *pat, size_t pat_len)
{
    int open_groups = 0;    // currently open \(
    int closed_groups = 0;  // \) seen, for a \k bound
    int total_groups = 0;   // \( seen, for the nine-group cap
    bool after_close = false;

    for (size_t i = 0; i < pat_len;)
    {
        char c = pat[i];
        if (c == '\\')
        {
            if (i + 1 >= pat_len)
            {
                return false;  // Dangling backslash
            }
            char d = pat[i + 1];
            if (d == '(')
            {
                if (++total_groups > 9)
                {
                    return false;
                }
                open_groups++;
                after_close = false;
            }
            else if (d == ')')
            {
                if (open_groups == 0)
                {
                    return false;  // Unopened \)
                }
                open_groups--;
                closed_groups++;
                i += 2;
                after_close = true;
                continue;
            }
            else if (d >= '1' && d <= '9')
            {
                if (d - '0' > closed_groups)
                {
                    return false;  // \k with no group k
                }
                after_close = false;
            }
            else
            {
                after_close = false;  // \<, \>, \., \\, ...
            }
            i += 2;
        }
        else if (c == '[')
        {
            size_t j = i + 1;
            if (j < pat_len && pat[j] == '^')
            {
                j++;
            }
            if (j < pat_len && pat[j] == ']')
            {
                j++;  // A `]` first in the class is a literal
            }
            while (j < pat_len && pat[j] != ']')
            {
                j++;
            }
            if (j >= pat_len)
            {
                return false;  // Unclosed [
            }
            i = j + 1;
            after_close = false;
        }
        else if (c == '*')
        {
            if (after_close)
            {
                return false;  // `*` never applies to a group
            }
            i++;
        }
        else
        {
            after_close = false;
            i++;
        }
    }
    return open_groups == 0;  // Unclosed \(
}

//
//  The matcher
//

typedef enum
{
    A_LIT,        // A literal character
    A_ANY,        // .
    A_CLASS,      // [...] / [^...]
    A_BOL,        // ^, first only
    A_EOL,        // $, last only
    A_WORDSTART,  // \<
    A_WORDEND,    // \>
    A_GOPEN,      // \(
    A_GCLOSE,     // \)
    A_BACKREF,    // \1..\9
} AtomKind;

typedef struct
{
    AtomKind kind;
    char lit;      // A_LIT
    size_t cs, ce; // A_CLASS: the bytes between [ and ]
    bool neg;      // A_CLASS
    int idx;       // A_GOPEN / A_GCLOSE / A_BACKREF
    size_t next;   // The pattern offset just past this atom (before any `*`)
} Atom;

typedef struct
{
    const char *pat;
    size_t pat_len;
    const char *line;
    size_t line_len;
    // Live capture state, mutated through the recursion; the accepted path's
    // writes are the most recent, which is what the greedy matcher relies on.
    size_t gs[10];
    size_t ge[10];
} PatCtx;

static bool class_match(const char *pat, size_t cs, size_t ce, bool neg, char c)
{
    bool found = false;
    for (size_t i = cs; i < ce;)
    {
        if (i + 2 < ce && pat[i + 1] == '-')
        {
            char lo = to_lower(pat[i]);
            char hi = to_lower(pat[i + 2]);
            char lc = to_lower(c);
            if (lo <= lc && lc <= hi)
            {
                found = true;
            }
            i += 3;
        }
        else
        {
            if (ci_eq(pat[i], c))
            {
                found = true;
            }
            i++;
        }
    }
    return neg ? !found : found;
}

// The group number of the \( at pattern offset pp: one more than the \( before
// it. Cheap -- pat_len <= 32 -- and it avoids threading a counter through the
// backtracking recursion.
static int group_of_open(const PatCtx *c, size_t pp)
{
    int n = 0;
    for (size_t i = 0; i < pp;)
    {
        if (c->pat[i] == '\\' && i + 1 < c->pat_len)
        {
            if (c->pat[i + 1] == '(')
            {
                n++;
            }
            i += 2;
        }
        else if (c->pat[i] == '[')
        {
            size_t j = i + 1;
            if (j < c->pat_len && c->pat[j] == '^') j++;
            if (j < c->pat_len && c->pat[j] == ']') j++;
            while (j < c->pat_len && c->pat[j] != ']') j++;
            i = (j < c->pat_len) ? j + 1 : c->pat_len;
        }
        else
        {
            i++;
        }
    }
    return n + 1;
}

// The group number a \) at offset pp closes: the innermost group still open.
static int group_of_close(const PatCtx *c, size_t pp)
{
    int stack[10];
    int depth = 0;
    int total = 0;
    for (size_t i = 0; i <= pp && i < c->pat_len;)
    {
        if (c->pat[i] == '\\' && i + 1 < c->pat_len)
        {
            if (c->pat[i + 1] == '(')
            {
                stack[depth++] = ++total;
            }
            else if (c->pat[i + 1] == ')')
            {
                if (i == pp)
                {
                    return depth > 0 ? stack[depth - 1] : 1;
                }
                if (depth > 0) depth--;
            }
            i += 2;
        }
        else if (c->pat[i] == '[')
        {
            size_t j = i + 1;
            if (j < c->pat_len && c->pat[j] == '^') j++;
            if (j < c->pat_len && c->pat[j] == ']') j++;
            while (j < c->pat_len && c->pat[j] != ']') j++;
            i = (j < c->pat_len) ? j + 1 : c->pat_len;
        }
        else
        {
            i++;
        }
    }
    return 1;
}

static Atom parse_atom(const PatCtx *c, size_t pp)
{
    Atom a;
    a.lit = 0;
    a.cs = a.ce = 0;
    a.neg = false;
    a.idx = 0;

    char ch = c->pat[pp];
    if (ch == '^' && pp == 0)
    {
        a.kind = A_BOL;
        a.next = pp + 1;
        return a;
    }
    if (ch == '$' && pp == c->pat_len - 1)
    {
        a.kind = A_EOL;
        a.next = pp + 1;
        return a;
    }
    if (ch == '.')
    {
        a.kind = A_ANY;
        a.next = pp + 1;
        return a;
    }
    if (ch == '[')
    {
        size_t j = pp + 1;
        a.neg = (j < c->pat_len && c->pat[j] == '^');
        if (a.neg) j++;
        a.cs = j;
        if (j < c->pat_len && c->pat[j] == ']') j++;  // `]` first is a literal
        while (j < c->pat_len && c->pat[j] != ']') j++;
        a.ce = j;
        a.kind = A_CLASS;
        a.next = (j < c->pat_len) ? j + 1 : c->pat_len;
        return a;
    }
    if (ch == '\\' && pp + 1 < c->pat_len)
    {
        char d = c->pat[pp + 1];
        a.next = pp + 2;
        if (d == '<') { a.kind = A_WORDSTART; return a; }
        if (d == '>') { a.kind = A_WORDEND;   return a; }
        if (d == '(') { a.kind = A_GOPEN;  a.idx = group_of_open(c, pp);  return a; }
        if (d == ')') { a.kind = A_GCLOSE; a.idx = group_of_close(c, pp); return a; }
        if (d >= '1' && d <= '9') { a.kind = A_BACKREF; a.idx = d - '0'; return a; }
        a.kind = A_LIT;  // \., \*, \\ ...
        a.lit = d;
        return a;
    }

    a.kind = A_LIT;  // A bare character, including `*` first / `^` not-first / a paren
    a.lit = ch;
    a.next = pp + 1;
    return a;
}

static bool atom_char_match(const PatCtx *c, const Atom *a, char ch)
{
    switch (a->kind)
    {
        case A_ANY:   return true;
        case A_LIT:   return ci_eq(a->lit, ch);
        case A_CLASS: return class_match(c->pat, a->cs, a->ce, a->neg, ch);
        default:      return false;
    }
}

static bool match_here(PatCtx *c, size_t pp, size_t tp);

// Greedy `*`: consume the atom as many times as it will go, then try the rest
// from the longest run down, so the whole match is the leftmost-longest.
static bool match_star(PatCtx *c, const Atom *a, size_t cont, size_t tp)
{
    size_t k = 0;
    while (tp + k < c->line_len && atom_char_match(c, a, c->line[tp + k]))
    {
        k++;
    }
    for (size_t i = k + 1; i-- > 0;)
    {
        if (match_here(c, cont, tp + i))
        {
            return true;
        }
    }
    return false;
}

static bool match_here(PatCtx *c, size_t pp, size_t tp)
{
    if (pp == c->pat_len)
    {
        c->ge[0] = tp;  // The whole match ends here
        return true;
    }

    Atom a = parse_atom(c, pp);
    switch (a.kind)
    {
        case A_BOL:
            return tp == 0 && match_here(c, a.next, tp);

        case A_EOL:
            return tp == c->line_len && match_here(c, a.next, tp);

        case A_WORDSTART:
        {
            bool before = tp > 0 && is_name_char(c->line[tp - 1]);
            bool at = tp < c->line_len && is_name_char(c->line[tp]);
            return !before && at && match_here(c, a.next, tp);
        }

        case A_WORDEND:
        {
            bool before = tp > 0 && is_name_char(c->line[tp - 1]);
            bool at = tp < c->line_len && is_name_char(c->line[tp]);
            return before && !at && match_here(c, a.next, tp);
        }

        case A_GOPEN:
            c->gs[a.idx] = tp;
            return match_here(c, a.next, tp);

        case A_GCLOSE:
            c->ge[a.idx] = tp;
            return match_here(c, a.next, tp);

        case A_BACKREF:
        {
            size_t s = c->gs[a.idx];
            size_t e = c->ge[a.idx];
            size_t bl = e > s ? e - s : 0;
            if (tp + bl > c->line_len)
            {
                return false;
            }
            for (size_t i = 0; i < bl; i++)
            {
                if (!ci_eq(c->line[s + i], c->line[tp + i]))
                {
                    return false;
                }
            }
            return match_here(c, a.next, tp + bl);
        }

        default:  // A_LIT, A_ANY, A_CLASS -- the repeatable atoms
            if (a.next < c->pat_len && c->pat[a.next] == '*')
            {
                return match_star(c, &a, a.next + 1, tp);
            }
            return tp < c->line_len && atom_char_match(c, &a, c->line[tp]) &&
                   match_here(c, a.next, tp + 1);
    }
}

bool editor_pattern_search(const char *pat, size_t pat_len,
                           const char *line, size_t line_len,
                           size_t from, EditorPatternGroups g)
{
    if (pat_len == 0 || from > line_len)
    {
        return false;
    }

    PatCtx c;
    c.pat = pat;
    c.pat_len = pat_len;
    c.line = line;
    c.line_len = line_len;

    for (size_t pos = from; pos <= line_len; pos++)
    {
        for (int k = 0; k < 10; k++)
        {
            c.gs[k] = 0;
            c.ge[k] = 0;
        }
        c.gs[0] = pos;
        if (match_here(&c, 0, pos))
        {
            for (int k = 0; k < 10; k++)
            {
                g[k].start = c.gs[k];
                g[k].end = c.ge[k];
            }
            return true;
        }
    }
    return false;
}

size_t editor_pattern_expand(const char *rep, size_t rep_len,
                             const char *line, const EditorPatternGroups g,
                             char *out, size_t out_cap)
{
    size_t o = 0;
    for (size_t i = 0; i < rep_len; i++)
    {
        char ch = rep[i];
        const char *src = NULL;
        size_t src_len = 0;
        char one;

        if (ch == '&')
        {
            src = line + g[0].start;
            src_len = g[0].end - g[0].start;
        }
        else if (ch == '\\' && i + 1 < rep_len)
        {
            char d = rep[++i];
            if (d >= '1' && d <= '9')
            {
                int k = d - '0';
                if (g[k].end > g[k].start)
                {
                    src = line + g[k].start;
                    src_len = g[k].end - g[k].start;
                }
                // An unset group expands to nothing
            }
            else
            {
                one = d;  // \&, \\ and any other escape are the character itself
                src = &one;
                src_len = 1;
            }
        }
        else
        {
            one = ch;
            src = &one;
            src_len = 1;
        }

        if (o + src_len > out_cap)
        {
            return SIZE_MAX;
        }
        for (size_t k = 0; k < src_len; k++)
        {
            out[o++] = src[k];
        }
    }
    return o;
}

//
//  The buffer walker for `/`, `?`, `n` and `N` (§16.5)
//

static size_t line_start_of(const char *text, size_t pos)
{
    while (pos > 0 && text[pos - 1] != '\n')
    {
        pos--;
    }
    return pos;
}

static size_t line_end_of(const char *text, size_t len, size_t ls)
{
    size_t e = ls;
    while (e < len && text[e] != '\n')
    {
        e++;
    }
    return e;
}

// The first match on line [ls, le) whose start is >= `min` (relative to ls),
// as an absolute offset in `out`.
static bool first_on_line(const char *text, size_t ls, size_t le, size_t min,
                          const char *pat, size_t pat_len, size_t *out)
{
    EditorPatternGroups g;
    if (editor_pattern_search(pat, pat_len, text + ls, le - ls, min, g))
    {
        *out = ls + g[0].start;
        return true;
    }
    return false;
}

// The last match on line [ls, le) whose start is < `cap` (relative to ls).
static bool last_on_line(const char *text, size_t ls, size_t le, size_t cap,
                         const char *pat, size_t pat_len, size_t *out)
{
    EditorPatternGroups g;
    bool any = false;
    size_t at = 0;
    while (editor_pattern_search(pat, pat_len, text + ls, le - ls, at, g))
    {
        size_t s = g[0].start;
        if (s >= cap)
        {
            break;
        }
        *out = ls + s;
        any = true;
        // Step past the match, one character at least, so an empty match on an
        // empty line terminates.
        at = (g[0].end > s) ? g[0].end : s + 1;
    }
    return any;
}

bool editor_pattern_find(const char *pat, size_t pat_len,
                         const char *text, size_t text_len,
                         size_t from, bool forward, size_t *out_pos)
{
    if (pat_len == 0)
    {
        return false;
    }
    if (from > text_len)
    {
        from = text_len;
    }

    size_t start_ls = line_start_of(text, from);

    if (forward)
    {
        // The line holding `from`, from `from` on
        size_t le = line_end_of(text, text_len, start_ls);
        if (first_on_line(text, start_ls, le, from - start_ls, pat, pat_len, out_pos))
        {
            return true;
        }
        // Following lines to the end of the buffer
        size_t ls = (le < text_len) ? le + 1 : text_len;
        while (ls < text_len)
        {
            le = line_end_of(text, text_len, ls);
            if (first_on_line(text, ls, le, 0, pat, pat_len, out_pos))
            {
                return true;
            }
            ls = (le < text_len) ? le + 1 : text_len;
        }
        // Wrap: from the top, up to and including the starting line's head
        ls = 0;
        while (ls <= start_ls)
        {
            le = line_end_of(text, text_len, ls);
            if (ls == start_ls)
            {
                if (first_on_line(text, ls, le, 0, pat, pat_len, out_pos) &&
                    *out_pos < from)
                {
                    return true;
                }
                break;
            }
            if (first_on_line(text, ls, le, 0, pat, pat_len, out_pos))
            {
                return true;
            }
            ls = le + 1;
        }
        return false;
    }

    // Backward never matches backwards: scan each line forward and keep the last
    // qualifying match. First the matches that begin before `from`, taking the
    // latest of them...
    {
        bool found = false;
        size_t best = 0;
        size_t ls = 0;
        while (ls <= start_ls)
        {
            size_t le = line_end_of(text, text_len, ls);
            size_t cap = (ls == start_ls) ? (from - ls) : (le - ls);
            size_t m;
            if (last_on_line(text, ls, le, cap, pat, pat_len, &m))
            {
                best = m;
                found = true;
            }
            if (ls == start_ls)
            {
                break;
            }
            ls = le + 1;
        }
        if (found)
        {
            *out_pos = best;
            return true;
        }
    }

    // ...then wrap: the latest match anywhere from the starting line to the end
    {
        bool found = false;
        size_t best = 0;
        size_t ls = start_ls;
        while (ls < text_len)
        {
            size_t le = line_end_of(text, text_len, ls);
            size_t m;
            if (last_on_line(text, ls, le, le - ls + 1, pat, pat_len, &m))
            {
                best = m;
                found = true;
            }
            ls = (le < text_len) ? le + 1 : text_len;
        }
        if (found)
        {
            *out_pos = best;
            return true;
        }
    }

    return false;
}
