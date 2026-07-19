//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the `play` note-word notation parser (core/notation.c).
//

#include "unity.h"
#include "core/notation.h"

void setUp(void) {}
void tearDown(void) {}

// Convenience: parse one token from a fresh-ish state, asserting the kind.
static SoundEvent note(NotationState *s, const char *tok)
{
    SoundEvent ev = {0};
    NotationKind k = notation_parse_token(s, tok, &ev);
    TEST_ASSERT_EQUAL_MESSAGE(NOTATION_NOTE, k, tok);
    return ev;
}

//==========================================================================
// Pitch
//==========================================================================

void test_middle_c(void)
{
    NotationState s;
    notation_state_init(&s);
    SoundEvent ev = note(&s, "c"); // c4
    TEST_ASSERT_INT_WITHIN(1, 262, ev.freq_hz);
    TEST_ASSERT_EQUAL_UINT16(500, ev.dur_ms); // quarter note at tempo 120
    TEST_ASSERT_EQUAL_UINT8(12, ev.vol);      // default volume
}

void test_octave_suffix(void)
{
    NotationState s;
    notation_state_init(&s);
    TEST_ASSERT_INT_WITHIN(1, 523, note(&s, "c5").freq_hz);
    TEST_ASSERT_INT_WITHIN(1, 392, note(&s, "g").freq_hz); // g4
}

void test_accidentals_sharp_and_s_equivalent(void)
{
    NotationState s;
    notation_state_init(&s);
    uint16_t f_hash = note(&s, "c#").freq_hz;
    uint16_t f_s = note(&s, "cs").freq_hz;
    TEST_ASSERT_INT_WITHIN(1, 277, f_hash);
    TEST_ASSERT_EQUAL_UINT16(f_hash, f_s);
}

void test_flat_matches_enharmonic(void)
{
    NotationState s;
    notation_state_init(&s);
    // Db is the same pitch as C#.
    TEST_ASSERT_INT_WITHIN(1, 277, note(&s, "db").freq_hz);
}

void test_full_form(void)
{
    NotationState s;
    notation_state_init(&s);
    // 8g#5. = dotted eighth-note G sharp in octave 5.
    SoundEvent ev = note(&s, "8g#5.");
    TEST_ASSERT_INT_WITHIN(1, 831, ev.freq_hz);
    TEST_ASSERT_EQUAL_UINT16(375, ev.dur_ms); // dotted eighth at 120 = 250 * 1.5
}

//==========================================================================
// Duration and rests
//==========================================================================

void test_length_prefix(void)
{
    NotationState s;
    notation_state_init(&s);
    TEST_ASSERT_EQUAL_UINT16(250, note(&s, "8g").dur_ms);  // eighth
    TEST_ASSERT_EQUAL_UINT16(1000, note(&s, "2g").dur_ms); // half
    TEST_ASSERT_EQUAL_UINT16(2000, note(&s, "1g").dur_ms); // whole
}

void test_dotted(void)
{
    NotationState s;
    notation_state_init(&s);
    TEST_ASSERT_EQUAL_UINT16(750, note(&s, "g.").dur_ms); // dotted quarter
}

void test_rest(void)
{
    NotationState s;
    notation_state_init(&s);
    SoundEvent ev = note(&s, "r");
    TEST_ASSERT_EQUAL_UINT16(0, ev.freq_hz); // rest is silence
    TEST_ASSERT_EQUAL_UINT16(500, ev.dur_ms);

    TEST_ASSERT_EQUAL_UINT16(1000, note(&s, "2r").dur_ms); // half rest
}

//==========================================================================
// Control words
//==========================================================================

void test_tempo_control(void)
{
    NotationState s;
    notation_state_init(&s);
    SoundEvent ev = {0};
    TEST_ASSERT_EQUAL(NOTATION_CONTROL, notation_parse_token(&s, "t240", &ev));
    TEST_ASSERT_EQUAL_INT(240, s.tempo);
    // Quarter note now lasts half as long.
    TEST_ASSERT_EQUAL_UINT16(250, note(&s, "c").dur_ms);
}

void test_octave_control(void)
{
    NotationState s;
    notation_state_init(&s);
    SoundEvent ev = {0};
    TEST_ASSERT_EQUAL(NOTATION_CONTROL, notation_parse_token(&s, "o5", &ev));
    TEST_ASSERT_EQUAL_INT(5, s.octave);
    TEST_ASSERT_INT_WITHIN(1, 523, note(&s, "c").freq_hz); // c5
}

void test_length_control(void)
{
    NotationState s;
    notation_state_init(&s);
    SoundEvent ev = {0};
    TEST_ASSERT_EQUAL(NOTATION_CONTROL, notation_parse_token(&s, "l8", &ev));
    TEST_ASSERT_EQUAL_INT(8, s.length);
    TEST_ASSERT_EQUAL_UINT16(250, note(&s, "c").dur_ms); // now defaults to eighth
}

void test_volume_control(void)
{
    NotationState s;
    notation_state_init(&s);
    SoundEvent ev = {0};
    TEST_ASSERT_EQUAL(NOTATION_CONTROL, notation_parse_token(&s, "v5", &ev));
    TEST_ASSERT_EQUAL_INT(5, s.volume);
    TEST_ASSERT_EQUAL_UINT8(5, note(&s, "c").vol);
}

void test_case_insensitive(void)
{
    NotationState s;
    notation_state_init(&s);
    TEST_ASSERT_INT_WITHIN(1, 523, note(&s, "C5").freq_hz);
    SoundEvent ev = {0};
    TEST_ASSERT_EQUAL(NOTATION_CONTROL, notation_parse_token(&s, "T240", &ev));
    TEST_ASSERT_EQUAL_INT(240, s.tempo);
}

//==========================================================================
// Errors
//==========================================================================

static void assert_error(const char *tok)
{
    NotationState s;
    notation_state_init(&s);
    SoundEvent ev = {0};
    TEST_ASSERT_EQUAL_MESSAGE(NOTATION_ERROR, notation_parse_token(&s, tok, &ev), tok);
}

void test_bad_letter(void)
{
    assert_error("h");
    assert_error("x");
}

void test_octave_out_of_range(void)
{
    assert_error("c9");
    assert_error("c0");
}

void test_tempo_out_of_range(void)
{
    assert_error("t500");
    assert_error("t10");
}

void test_trailing_junk(void)
{
    assert_error("cxy");
    assert_error("8gg");
}

void test_empty_token(void)
{
    assert_error("");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_middle_c);
    RUN_TEST(test_octave_suffix);
    RUN_TEST(test_accidentals_sharp_and_s_equivalent);
    RUN_TEST(test_flat_matches_enharmonic);
    RUN_TEST(test_full_form);
    RUN_TEST(test_length_prefix);
    RUN_TEST(test_dotted);
    RUN_TEST(test_rest);
    RUN_TEST(test_tempo_control);
    RUN_TEST(test_octave_control);
    RUN_TEST(test_length_control);
    RUN_TEST(test_volume_control);
    RUN_TEST(test_case_insensitive);
    RUN_TEST(test_bad_letter);
    RUN_TEST(test_octave_out_of_range);
    RUN_TEST(test_tempo_out_of_range);
    RUN_TEST(test_trailing_junk);
    RUN_TEST(test_empty_token);
    return UNITY_END();
}
