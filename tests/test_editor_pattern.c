//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the vi pattern matcher (devices/picocalc/editor_pattern.c), the
//  dialect `:s`, `/` and `?` read once M6 landed (docs/vi-mode-design.md §16).
//
//  The matcher is device code, but this test is a HOST build, and it uses that:
//  it includes <regex.h> and runs a differential against POSIX. That is the one
//  thing a device build cannot do -- no arm-none-eabi libc.a defines regcomp
//  (§16.1) -- which is exactly why the matcher is vi's own and not the library's.
//

#include "unity.h"
#include "editor_pattern.h"

#include <regex.h>   // Host-only. See the note above: the device cannot link it.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

//
//  Small helpers over null-terminated strings
//

// Leftmost match of `pat` in `line`; returns the start or -1, and the length
// through *out_len when it matches.
static long match(const char *pat, const char *line, size_t *out_len)
{
    EditorPatternGroups g;
    if (!editor_pattern_search(pat, strlen(pat), line, strlen(line), 0, g, NULL))
    {
        return -1;
    }
    if (out_len)
    {
        *out_len = g[0].end - g[0].start;
    }
    return (long)g[0].start;
}

// Does `pat` match anywhere in `line`?
static bool matches(const char *pat, const char *line)
{
    size_t len;
    return match(pat, line, &len) >= 0;
}

// Substitute the whole leftmost match of `pat` in `line` with the expansion of
// `rep`, into `out`. Returns false when there is no match.
static bool sub_once(const char *pat, const char *rep, const char *line, char *out)
{
    EditorPatternGroups g;
    if (!editor_pattern_search(pat, strlen(pat), line, strlen(line), 0, g, NULL))
    {
        return false;
    }
    char mid[64];
    size_t m = editor_pattern_expand(rep, strlen(rep), line, g, mid, sizeof(mid));
    TEST_ASSERT_NOT_EQUAL(SIZE_MAX, m);
    size_t o = 0;
    for (size_t i = 0; i < g[0].start; i++) out[o++] = line[i];
    for (size_t i = 0; i < m; i++)          out[o++] = mid[i];
    for (size_t i = g[0].end; line[i]; i++) out[o++] = line[i];
    out[o] = '\0';
    return true;
}

//
//  The atoms (§16.2)
//

void test_a_literal_matches_where_it_appears(void)
{
    size_t len;
    TEST_ASSERT_EQUAL_INT(3, match("square", "to square", &len));
    TEST_ASSERT_EQUAL_UINT(6, len);
}

void test_matching_is_case_insensitive(void)
{
    TEST_ASSERT_TRUE(matches("Total", "the total"));
    TEST_ASSERT_TRUE(matches("total", "The TOTAL"));
}

void test_dot_matches_any_character(void)
{
    size_t len;
    TEST_ASSERT_EQUAL_INT(0, match("a.c", "axc", &len));
    TEST_ASSERT_EQUAL_UINT(3, len);
    TEST_ASSERT_TRUE(matches("a.c", "a c"));
}

void test_star_matches_zero_or_more(void)
{
    size_t len;
    TEST_ASSERT_EQUAL_INT(0, match("ab*c", "ac", &len));   // zero b's
    TEST_ASSERT_EQUAL_UINT(2, len);
    TEST_ASSERT_EQUAL_INT(0, match("ab*c", "abbbc", &len)); // and many
    TEST_ASSERT_EQUAL_UINT(5, len);
}

void test_star_is_greedy(void)
{
    size_t len;
    TEST_ASSERT_EQUAL_INT(0, match("a.*b", "axbxb", &len));
    TEST_ASSERT_EQUAL_UINT(5, len);  // through the last b, not the first
}

void test_a_leading_star_is_a_literal(void)
{
    size_t len;
    TEST_ASSERT_EQUAL_INT(1, match("*b", "a*b", &len));
    TEST_ASSERT_EQUAL_UINT(2, len);
}

void test_a_star_after_the_caret_is_a_literal(void)
{
    // `^` is an anchor first, so the `*` after it has nothing to repeat
    TEST_ASSERT_EQUAL_INT(0, match("^*a", "*a", NULL));
}

void test_caret_anchors_to_the_start(void)
{
    TEST_ASSERT_EQUAL_INT(0, match("^ab", "abc", NULL));
    TEST_ASSERT_EQUAL_INT(-1, match("^bc", "abc", NULL));
}

void test_dollar_anchors_to_the_end(void)
{
    TEST_ASSERT_EQUAL_INT(1, match("bc$", "abc", NULL));
    TEST_ASSERT_EQUAL_INT(-1, match("ab$", "abc", NULL));
}

void test_caret_and_dollar_are_literal_when_not_at_the_ends(void)
{
    TEST_ASSERT_EQUAL_INT(0, match("a^b", "a^b", NULL));  // ^ mid-pattern is a literal
    TEST_ASSERT_EQUAL_INT(0, match("a$b", "a$b", NULL));  // $ likewise
}

void test_a_character_class(void)
{
    TEST_ASSERT_EQUAL_INT(2, match("[xyz]", "a y b", NULL));
    TEST_ASSERT_EQUAL_INT(-1, match("[xyz]", "abc", NULL));
}

void test_a_negated_class(void)
{
    TEST_ASSERT_EQUAL_INT(0, match("[^0-9]", "a1", NULL));
    TEST_ASSERT_EQUAL_INT(-1, match("[^0-9]", "12345", NULL));
}

void test_a_range_class(void)
{
    TEST_ASSERT_EQUAL_INT(2, match("[a-f]", "12c34", NULL));
}

void test_a_closing_bracket_first_in_a_class_is_a_literal(void)
{
    TEST_ASSERT_EQUAL_INT(1, match("[]x]", "a]b", NULL));
    TEST_ASSERT_EQUAL_INT(2, match("[]x]", "abx", NULL));
}

void test_a_dash_last_in_a_class_is_a_literal(void)
{
    TEST_ASSERT_EQUAL_INT(1, match("[a-]", "q-r", NULL));
}

void test_a_class_can_be_starred(void)
{
    size_t len;
    TEST_ASSERT_EQUAL_INT(0, match("[0-9]*", "42x", &len));
    TEST_ASSERT_EQUAL_UINT(2, len);
}

void test_an_escaped_metacharacter_is_a_literal(void)
{
    TEST_ASSERT_EQUAL_INT(1, match("a\\.b", "xa.by", NULL));
    TEST_ASSERT_EQUAL_INT(-1, match("a\\.b", "axby", NULL));  // now `.` is not any
    TEST_ASSERT_EQUAL_INT(1, match("a\\*", "ba*c", NULL));
}

void test_a_construct_at_the_start_and_end_of_a_line(void)
{
    TEST_ASSERT_EQUAL_INT(0, match("^.*$", "anything", NULL));
    TEST_ASSERT_EQUAL_INT(0, match("^$", "", NULL));       // empty line
    TEST_ASSERT_EQUAL_INT(-1, match("^$", "x", NULL));
}

//
//  Groups and back-references
//

void test_a_back_reference_matches_the_captured_text(void)
{
    TEST_ASSERT_EQUAL_INT(0, match("\\(ab\\)\\1", "abab", NULL));
    TEST_ASSERT_EQUAL_INT(-1, match("\\(ab\\)\\1", "abxy", NULL));
}

void test_a_back_reference_is_case_insensitive(void)
{
    TEST_ASSERT_EQUAL_INT(0, match("\\(ab\\)\\1", "abAB", NULL));
}

void test_nested_groups_capture_independently(void)
{
    EditorPatternGroups g;
    const char *line = "hello";
    TEST_ASSERT_TRUE(editor_pattern_search("\\(h\\(el\\)\\)", strlen("\\(h\\(el\\)\\)"),
                                           line, strlen(line), 0, g, NULL));
    TEST_ASSERT_EQUAL_UINT(0, g[1].start);  // (hel)
    TEST_ASSERT_EQUAL_UINT(3, g[1].end);
    TEST_ASSERT_EQUAL_UINT(1, g[2].start);  // (el)
    TEST_ASSERT_EQUAL_UINT(3, g[2].end);
}

//
//  The replacement (§16.2)
//

void test_ampersand_in_the_replacement_is_the_whole_match(void)
{
    char out[64];
    TEST_ASSERT_TRUE(sub_once("[0-9][0-9]*", "(&)", "n42x", out));
    TEST_ASSERT_EQUAL_STRING("n(42)x", out);
}

void test_group_references_in_the_replacement(void)
{
    char out[64];
    TEST_ASSERT_TRUE(sub_once("\\(a\\)\\(b\\)", "\\2\\1", "xaby", out));
    TEST_ASSERT_EQUAL_STRING("xbay", out);
}

void test_escaped_ampersand_and_backslash_in_the_replacement(void)
{
    char out[64];
    TEST_ASSERT_TRUE(sub_once("b", "\\&", "abc", out));
    TEST_ASSERT_EQUAL_STRING("a&c", out);
    TEST_ASSERT_TRUE(sub_once("b", "\\\\", "abc", out));
    TEST_ASSERT_EQUAL_STRING("a\\c", out);
}

void test_an_expansion_that_would_not_fit_reports_size_max(void)
{
    EditorPatternGroups g;
    const char *line = "abcdef";
    TEST_ASSERT_TRUE(editor_pattern_search(".", 1, line, strlen(line), 0, g, NULL));
    char out[4];
    // "&&&&&" expands the one-char match five times -> 5 bytes, out_cap is 4
    size_t m = editor_pattern_expand("&&&&&", 5, line, g, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT(SIZE_MAX, m);
}

//
//  Empty matches (§16.4) -- the case that can loop forever if mishandled
//

void test_a_star_pattern_matches_empty_at_the_start(void)
{
    size_t len;
    TEST_ASSERT_EQUAL_INT(0, match("x*", "abc", &len));
    TEST_ASSERT_EQUAL_UINT(0, len);  // no x: the empty match at position 0
}

void test_an_empty_pattern_never_matches(void)
{
    EditorPatternGroups g;
    TEST_ASSERT_FALSE(editor_pattern_search("", 0, "abc", 3, 0, g, NULL));
}

//
//  Logo's word boundary, not vi's (§16.2) -- the rename case, the most
//  load-bearing table in the file. The name characters are written from
//  is_delimiter (core/lexer.c) so a change there fails here.
//

void test_word_boundary_matches_the_whole_word(void)
{
    TEST_ASSERT_TRUE(matches("\\<n\\>", "\"n"));   // make "n
    TEST_ASSERT_TRUE(matches("\\<n\\>", ":n"));    // :n
    TEST_ASSERT_TRUE(matches("\\<n\\>", "add n here"));  // bare
}

void test_word_boundary_does_not_reach_into_a_longer_logo_name(void)
{
    // `.`, `?` and digits are all name characters in Logo, punctuation to vi
    TEST_ASSERT_FALSE(matches("\\<total\\>", ":total.count"));
    TEST_ASSERT_FALSE(matches("\\<empty\\>", "empty?"));
    TEST_ASSERT_FALSE(matches("\\<n\\>", "n2"));
}

void test_word_boundary_stops_at_a_delimiter(void)
{
    // The eleven delimiters end a Logo name, so the boundary falls there
    TEST_ASSERT_TRUE(matches("\\<n\\>", ":n+1"));
    TEST_ASSERT_TRUE(matches("\\<n\\>", "[:n]"));
    TEST_ASSERT_TRUE(matches("\\<n\\>", ":n-1"));
}

void test_word_boundary_at_the_ends_of_a_line(void)
{
    TEST_ASSERT_EQUAL_INT(0, match("\\<ab\\>", "ab", NULL));   // both ends of the line
    TEST_ASSERT_EQUAL_INT(4, match("\\<ab\\>", "xy  ab", NULL));
}

void test_rename_only_the_variable_leaves_the_procedure_alone(void)
{
    // \([":]\)n\> with \1 in the replacement -- the group-and-backref idiom the
    // reference recommends. It rewrites both spellings and skips a bare n.
    char out[64];
    TEST_ASSERT_TRUE(sub_once("\\([\":]\\)n\\>", "\\1count", ":n", out));
    TEST_ASSERT_EQUAL_STRING(":count", out);
    TEST_ASSERT_TRUE(sub_once("\\([\":]\\)n\\>", "\\1count", "\"n", out));
    TEST_ASSERT_EQUAL_STRING("\"count", out);
    TEST_ASSERT_FALSE(sub_once("\\([\":]\\)n\\>", "\\1count", "print n", out));
}

//
//  Validation (§16.6) -- each rejected form beeps at parse time
//

static bool valid(const char *pat)
{
    return editor_pattern_valid(pat, strlen(pat));
}

void test_valid_patterns_pass(void)
{
    TEST_ASSERT_TRUE(valid("\\<n\\>"));
    TEST_ASSERT_TRUE(valid("^a.*b$"));
    TEST_ASSERT_TRUE(valid("\\(a\\)\\1"));
    TEST_ASSERT_TRUE(valid("[]a-]"));
    TEST_ASSERT_TRUE(valid("[^abc]*"));
}

void test_a_dangling_backslash_is_refused(void)
{
    TEST_ASSERT_FALSE(valid("ab\\"));
}

void test_an_unclosed_class_is_refused(void)
{
    TEST_ASSERT_FALSE(valid("[abc"));
}

void test_an_unbalanced_group_is_refused(void)
{
    TEST_ASSERT_FALSE(valid("\\(ab"));    // unclosed
    TEST_ASSERT_FALSE(valid("ab\\)"));    // unopened
}

void test_more_than_nine_groups_is_refused(void)
{
    const char *ten = "\\(\\)\\(\\)\\(\\)\\(\\)\\(\\)\\(\\)\\(\\)\\(\\)\\(\\)\\(\\)";
    TEST_ASSERT_FALSE(editor_pattern_valid(ten, strlen(ten)));
}

void test_a_back_reference_to_no_group_is_refused(void)
{
    TEST_ASSERT_FALSE(valid("\\1"));
    TEST_ASSERT_FALSE(valid("\\(a\\)\\2"));
}

void test_a_star_after_a_group_is_refused(void)
{
    TEST_ASSERT_FALSE(valid("\\(ab\\)*"));
}

//
//  The buffer walker for /, ?, n and N (§16.5)
//

static long find(const char *pat, const char *text, size_t from, bool forward)
{
    size_t pos;
    if (!editor_pattern_find(pat, strlen(pat), text, strlen(text), from, forward, &pos, NULL))
    {
        return -1;
    }
    return (long)pos;
}

void test_forward_finds_a_match_ahead_on_the_same_line(void)
{
    TEST_ASSERT_EQUAL_INT(4, find("c", "a b c\nd", 2, true));
}

void test_forward_finds_a_match_on_a_following_line(void)
{
    TEST_ASSERT_EQUAL_INT(6, find("xy", "abc\n  xy", 0, true));
}

void test_forward_wraps_to_the_start(void)
{
    // Starting past the only match, forward wraps around to find it
    TEST_ASSERT_EQUAL_INT(0, find("abc", "abc\ndef", 4, true));
}

void test_backward_finds_the_previous_match(void)
{
    TEST_ASSERT_EQUAL_INT(0, find("abc", "abc\ndef\nabc", 5, false));
}

void test_backward_keeps_the_last_match_on_a_line(void)
{
    // Two x's on the first line: from the second line, backward finds the later
    // one (index 4), which the scan-forward-keep-the-last shape can get wrong
    TEST_ASSERT_EQUAL_INT(4, find("x", "x a x\ny", 6, false));
}

void test_backward_wraps_to_the_end(void)
{
    // From position 0 nothing precedes the cursor, so backward wraps to the last
    TEST_ASSERT_EQUAL_INT(8, find("abc", "abc\ndef\nabc", 0, false));
}

void test_anchors_are_per_line_in_the_buffer(void)
{
    // `^d` must find d at the start of the second line, not fail
    TEST_ASSERT_EQUAL_INT(4, find("^d", "abc\ndef", 0, true));
    // `c$` matches c at the end of the first line
    TEST_ASSERT_EQUAL_INT(2, find("c$", "abc\ndef", 0, true));
}

void test_dot_never_crosses_a_line_break(void)
{
    // `b.c` would match across the break only if `.` ate the newline
    TEST_ASSERT_EQUAL_INT(-1, find("b.c", "ab\ncd", 0, true));
}

void test_a_zero_width_pattern_steps_one_character(void)
{
    // /x* from cursor+1 lands on the next character each time, as vim does
    TEST_ASSERT_EQUAL_INT(1, find("x*", "abc", 1, true));
    TEST_ASSERT_EQUAL_INT(2, find("x*", "abc", 2, true));
}

//
//  B36: the matcher must bound its own work.
//
//  Sequential stars backtrack combinatorially -- no nested quantifier needed,
//  which is the assumption M6 shipped on and got wrong. `.*.*.*x` on a 256-char
//  line is 189 million match steps and every further `.*` multiplies by the line
//  length again, so on a board this is a wedge no keystroke can interrupt. These
//  tests are the reproduction: without the budget they do not fail, they hang.
//

void test_a_pathological_pattern_is_refused_not_run(void)
{
    static char line[257];
    memset(line, 'a', 256);

    EditorPatternGroups g;
    bool too_complex = false;
    // Fifteen `.*` and a trailing `x` the line never contains -- the pattern
    // from the M6 board gate that wedged the device.
    const char *pat = ".*.*.*.*.*.*.*.*.*.*.*.*.*.*.*x";
    TEST_ASSERT_TRUE(editor_pattern_valid(pat, strlen(pat)));
    TEST_ASSERT_FALSE(editor_pattern_search(pat, strlen(pat), line, 256, 0, g,
                                            &too_complex));
    TEST_ASSERT_TRUE(too_complex);
}

void test_a_refusal_is_distinct_from_an_honest_miss(void)
{
    EditorPatternGroups g;
    bool too_complex = true;  // Poisoned: a plain miss must clear it
    TEST_ASSERT_FALSE(editor_pattern_search("zz", 2, "abc", 3, 0, g, &too_complex));
    TEST_ASSERT_FALSE(too_complex);
}

void test_the_budget_does_not_refuse_a_real_pattern(void)
{
    // The worst legitimate case measured: one star failing on a full-width
    // line, 33,410 steps against a budget of 200,000. If this ever starts
    // failing the budget has been cut too far, not the pattern gone wrong.
    static char line[257];
    memset(line, 'a', 256);

    EditorPatternGroups g;
    bool too_complex = false;
    TEST_ASSERT_FALSE(editor_pattern_search(".*x", 3, line, 256, 0, g, &too_complex));
    TEST_ASSERT_FALSE(too_complex);

    // And the rename pattern the milestone exists for, on a real line
    const char *code = "    if :n > 3 [print sentence :n :n.total]";
    TEST_ASSERT_TRUE(editor_pattern_search("\\<n\\>", strlen("\\<n\\>"),
                                           code, strlen(code), 0, g, &too_complex));
    TEST_ASSERT_FALSE(too_complex);
}

void test_a_pathological_pattern_is_refused_by_the_buffer_walker(void)
{
    // editor_pattern_find pays the budget per line, so it has to stop at the
    // first refusal rather than pay it again on every line below.
    static char text[1024];
    for (int i = 0; i < 4; i++)
    {
        memset(text + i * 256, 'a', 255);
        text[i * 256 + 255] = '\n';
    }
    text[1023] = '\0';

    size_t pos = 0;
    bool too_complex = false;
    const char *pat = ".*.*.*.*.*.*.*.*.*.*.*.*.*.*.*x";
    TEST_ASSERT_FALSE(editor_pattern_find(pat, strlen(pat), text, strlen(text),
                                          0, true, &pos, &too_complex));
    TEST_ASSERT_TRUE(too_complex);
}

//
//  A literal-pattern equivalence run: for patterns with no metacharacters, the
//  matcher must agree with a plain substring search. The regression net for the
//  four milestones already on a board.
//

static long literal_find(const char *needle, const char *hay)
{
    const char *p = strstr(hay, needle);
    return p ? (long)(p - hay) : -1;
}

void test_literal_patterns_match_like_a_substring_search(void)
{
    static const char *words[] = {"to", "make", "n", "count", "pen", "repeat", "xyz"};
    static const char *lines[] = {
        "to count make pen repeat", "n and pen", "nothing here", "count to ten",
    };
    for (size_t w = 0; w < sizeof(words) / sizeof(words[0]); w++)
    {
        for (size_t l = 0; l < sizeof(lines) / sizeof(lines[0]); l++)
        {
            size_t len;
            long mine = match(words[w], lines[l], &len);
            long lit = literal_find(words[w], lines[l]);
            TEST_ASSERT_EQUAL_INT(lit, mine);
            if (mine >= 0)
            {
                TEST_ASSERT_EQUAL_UINT(strlen(words[w]), len);
            }
        }
    }
}

//
//  The differential against POSIX (§16.1, §16.9). Host-only: macOS libSystem
//  and glibc both have a real BRE engine, so thousands of random pattern/line
//  pairs can pin the matcher against it. It asserts the EXTENT of the whole
//  match, not the group split -- POSIX is leftmost-longest inside a group and
//  ours is greedy, and the two are entitled to disagree there. `\<`/`\>` are
//  left out on purpose: POSIX's word is narrower than Logo's, which is the very
//  difference §16.2 is about, and it is asserted directly above instead.
//

static unsigned rng_state = 0x1234abcdu;
static unsigned rng(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 8;
}

// Concatenate a few valid tokens, so both engines get a pattern they accept.
static void random_pattern(char *out)
{
    static const char *tokens[] = {
        "a", "b", "c", ".", "a*", "b*", ".*", "[ab]", "[^a]", "[a-c]", "[ab]*",
    };
    size_t o = 0;
    if (rng() % 4 == 0) out[o++] = '^';
    int n = 1 + (int)(rng() % 3);
    for (int i = 0; i < n; i++)
    {
        const char *t = tokens[rng() % (sizeof(tokens) / sizeof(tokens[0]))];
        for (size_t k = 0; t[k]; k++) out[o++] = t[k];
    }
    if (rng() % 4 == 0) out[o++] = '$';
    out[o] = '\0';
}

static void random_line(char *out)
{
    static const char alpha[] = "abcd  ";
    int n = (int)(rng() % 8);
    for (int i = 0; i < n; i++) out[i] = alpha[rng() % (sizeof(alpha) - 1)];
    out[n] = '\0';
}

void test_differential_against_posix_bre(void)
{
    for (int iter = 0; iter < 20000; iter++)
    {
        char pat[32], line[16];
        random_pattern(pat);
        random_line(line);

        regex_t re;
        if (regcomp(&re, pat, REG_ICASE) != 0)  // REG_ICASE alone selects BRE
        {
            continue;  // A shape POSIX rejects; ours is exercised elsewhere
        }
        regmatch_t rm;
        int r = regexec(&re, line, 1, &rm, 0);
        regfree(&re);

        size_t len = 0;
        long mine = match(pat, line, &len);

        if (r == 0)
        {
            if (mine != rm.rm_so || (long)len != rm.rm_eo - rm.rm_so)
            {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "pat=/%s/ line=\"%s\": posix [%d,%d) ours [%ld,%ld)",
                         pat, line, (int)rm.rm_so, (int)rm.rm_eo, mine, mine + (long)len);
                TEST_FAIL_MESSAGE(msg);
            }
        }
        else
        {
            if (mine >= 0)
            {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "pat=/%s/ line=\"%s\": posix no match, ours [%ld,%ld)",
                         pat, line, mine, mine + (long)len);
                TEST_FAIL_MESSAGE(msg);
            }
        }
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_a_literal_matches_where_it_appears);
    RUN_TEST(test_matching_is_case_insensitive);
    RUN_TEST(test_dot_matches_any_character);
    RUN_TEST(test_star_matches_zero_or_more);
    RUN_TEST(test_star_is_greedy);
    RUN_TEST(test_a_leading_star_is_a_literal);
    RUN_TEST(test_a_star_after_the_caret_is_a_literal);
    RUN_TEST(test_caret_anchors_to_the_start);
    RUN_TEST(test_dollar_anchors_to_the_end);
    RUN_TEST(test_caret_and_dollar_are_literal_when_not_at_the_ends);
    RUN_TEST(test_a_character_class);
    RUN_TEST(test_a_negated_class);
    RUN_TEST(test_a_range_class);
    RUN_TEST(test_a_closing_bracket_first_in_a_class_is_a_literal);
    RUN_TEST(test_a_dash_last_in_a_class_is_a_literal);
    RUN_TEST(test_a_class_can_be_starred);
    RUN_TEST(test_an_escaped_metacharacter_is_a_literal);
    RUN_TEST(test_a_construct_at_the_start_and_end_of_a_line);

    RUN_TEST(test_a_back_reference_matches_the_captured_text);
    RUN_TEST(test_a_back_reference_is_case_insensitive);
    RUN_TEST(test_nested_groups_capture_independently);

    RUN_TEST(test_ampersand_in_the_replacement_is_the_whole_match);
    RUN_TEST(test_group_references_in_the_replacement);
    RUN_TEST(test_escaped_ampersand_and_backslash_in_the_replacement);
    RUN_TEST(test_an_expansion_that_would_not_fit_reports_size_max);

    RUN_TEST(test_a_star_pattern_matches_empty_at_the_start);
    RUN_TEST(test_an_empty_pattern_never_matches);

    RUN_TEST(test_word_boundary_matches_the_whole_word);
    RUN_TEST(test_word_boundary_does_not_reach_into_a_longer_logo_name);
    RUN_TEST(test_word_boundary_stops_at_a_delimiter);
    RUN_TEST(test_word_boundary_at_the_ends_of_a_line);
    RUN_TEST(test_rename_only_the_variable_leaves_the_procedure_alone);

    RUN_TEST(test_valid_patterns_pass);
    RUN_TEST(test_a_dangling_backslash_is_refused);
    RUN_TEST(test_an_unclosed_class_is_refused);
    RUN_TEST(test_an_unbalanced_group_is_refused);
    RUN_TEST(test_more_than_nine_groups_is_refused);
    RUN_TEST(test_a_back_reference_to_no_group_is_refused);
    RUN_TEST(test_a_star_after_a_group_is_refused);

    RUN_TEST(test_forward_finds_a_match_ahead_on_the_same_line);
    RUN_TEST(test_forward_finds_a_match_on_a_following_line);
    RUN_TEST(test_forward_wraps_to_the_start);
    RUN_TEST(test_backward_finds_the_previous_match);
    RUN_TEST(test_backward_keeps_the_last_match_on_a_line);
    RUN_TEST(test_backward_wraps_to_the_end);
    RUN_TEST(test_anchors_are_per_line_in_the_buffer);
    RUN_TEST(test_dot_never_crosses_a_line_break);
    RUN_TEST(test_a_zero_width_pattern_steps_one_character);

    RUN_TEST(test_a_pathological_pattern_is_refused_not_run);
    RUN_TEST(test_a_refusal_is_distinct_from_an_honest_miss);
    RUN_TEST(test_the_budget_does_not_refuse_a_real_pattern);
    RUN_TEST(test_a_pathological_pattern_is_refused_by_the_buffer_walker);
    RUN_TEST(test_literal_patterns_match_like_a_substring_search);
    RUN_TEST(test_differential_against_posix_bre);

    return UNITY_END();
}
