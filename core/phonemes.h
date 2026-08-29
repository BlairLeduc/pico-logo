//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Letter-to-sound rules for `say` (P16 M2). See docs/say-design.md §7.
//
//  This is the front end: a pure function from English text to a list of
//  phoneme ids, and the sibling of core/notation.c -- notation.c turns note
//  words into SoundEvents for `play`, this turns letters into phonemes for
//  `say`. Both run on the host, which is the whole point of the split: the
//  interesting half of a speech synthesizer is testable in ctest with no
//  board and no ear (§4).
//
//  The rules are NRL Report 7948's (Naval Research Laboratory, 1976), which
//  is a US Government report and therefore public domain -- the licence
//  constraint that shaped the whole design (§3). They are transcribed here,
//  not ported.
//

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Translate English text to phoneme ids (SpeechPhonemeId, the §6
    // alphabet), starting at character `start`. Writes at most `max_out`
    // ids and stops early when the buffer is nearly full, so a caller with a
    // small buffer streams rather than failing: *next is set to the
    // character to resume at. The whole string is always passed, so the
    // rules still see the letters behind the seam and a resumed call keeps
    // its left context.
    //
    // `max_out` must be at least PHONEMES_MIN_OUT, which is what guarantees
    // progress: no single rule emits more than that (" [W] " spells out
    // "double-you", seven phonemes, and is the longest).
    //
    // Letters are case-insensitive. Digits are spoken one at a time
    // ("42" -> "four two"); `.`, `!` and `?` become a pause; any other
    // character is skipped, including one the rules cannot place, because
    // spelling it out is worse than silence (§5.1).
    int phonemes_translate(const char *text, int start, uint8_t *out, int max_out, int *next);

#define PHONEMES_MIN_OUT 8

    // The number of letter-to-sound rules, for the record and for the test
    // that asserts the transcription is complete.
    int phonemes_rule_count(void);

#ifdef __cplusplus
}
#endif
