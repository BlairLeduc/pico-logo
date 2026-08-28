//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  P16 gates for core/speech_synth.c, both halves of each.
//
//  M0 -- "can we make a vowel?": formant assertions via Goertzel
//  (say-design.md §10) so the resonators are checked without an ear, plus
//  five sustained vowels rendered to .wav for the human listening gate.
//
//  M1 -- the full 41-phoneme inventory and the §8.3 transitions: every row
//  sounds, the sibilants stay apart, stops close before they burst,
//  diphthongs move, and the ten Berzerk words are rendered to .wav for the
//  M1 listening gate (say-design.md §12).
//
//  The .wav files land in the build directory ctest runs from.
//

#include "unity.h"
#include "core/speech_synth.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

#define RENDER_MS 500
#define RENDER_SAMPLES (RENDER_MS * SPEECH_SAMPLE_RATE_HZ / 1000)
#define PITCH_HZ 100.0f

static int16_t render_buf[RENDER_SAMPLES];

// Six seconds, which holds the longest utterance below with room to spare.
#define WORD_SAMPLES (6 * SPEECH_SAMPLE_RATE_HZ)
static int16_t word_buf[WORD_SAMPLES];

// Goertzel power of `samples` at `freq_hz`, sampled at `fs`.
static float goertzel_power(const int16_t *samples, int n, float freq_hz, float fs)
{
    float k = 0.5f + ((float)n * freq_hz) / fs;
    float w = (2.0f * (float)M_PI / (float)n) * k;
    float cosine = cosf(w);
    float coeff = 2.0f * cosine;
    float q0 = 0.0f, q1 = 0.0f, q2 = 0.0f;

    for (int i = 0; i < n; i++)
    {
        q0 = coeff * q1 - q2 + (float)samples[i];
        q2 = q1;
        q1 = q0;
    }

    float real = q1 - q2 * cosine;
    float imag = q2 * sinf(w);
    return real * real + imag * imag;
}

static void write_wav_mono16(const char *path, const int16_t *samples, int n, int sample_rate)
{
    FILE *f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);

    uint32_t data_bytes = (uint32_t)n * 2;
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1;
    uint16_t num_channels = 1;
    uint32_t sr = (uint32_t)sample_rate;
    uint32_t byte_rate = sr * 2;
    uint16_t block_align = 2;
    uint16_t bits_per_sample = 16;
    uint32_t chunk_size = 36 + data_bytes;

    fwrite("RIFF", 1, 4, f);
    fwrite(&chunk_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fwrite(&fmt_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sr, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_bytes, 4, 1, f);
    fwrite(samples, 2, (size_t)n, f);
    fclose(f);
}

// An impulse-train source only puts energy at multiples of the pitch: the
// signal's spectrum is a harmonic comb, not a continuum. A Goertzel probe
// between two harmonics reads mostly leakage from whichever is closer, not
// the resonator's true response -- so every probe below is snapped to the
// nearest harmonic of `pitch_hz` before it is read.
static float nearest_harmonic(float hz, float pitch_hz)
{
    return roundf(hz / pitch_hz) * pitch_hz;
}

// Search the harmonics between `lo_hz` and `hi_hz` for the loudest one, then
// refine its frequency by parabolic interpolation (in log power) against its
// two neighbouring harmonics. The comb is only 100 Hz dense, coarse enough
// that the true resonance can sit closer to one harmonic than its
// nearest-bin power alone would suggest; interpolating is what keeps a
// correctly-tuned low F1 (e.g. 270 Hz against 100 Hz harmonics) from reading
// as more than 10% off just because no harmonic lands on it exactly.
static float find_peak_harmonic(const int16_t *samples, int n, float fs, float pitch_hz,
                                 float lo_hz, float hi_hz, float *peak_freq_out)
{
    int lo_k = (int)ceilf(lo_hz / pitch_hz);
    int hi_k = (int)floorf(hi_hz / pitch_hz);
    float best_power = -1.0f;
    int best_k = lo_k;

    for (int k = lo_k; k <= hi_k; k++)
    {
        float p = goertzel_power(samples, n, pitch_hz * (float)k, fs);
        if (p > best_power)
        {
            best_power = p;
            best_k = k;
        }
    }

    // Neighbours are fetched regardless of the search window's own bounds --
    // a peak found at either edge of [lo_hz, hi_hz] still has a real
    // harmonic just outside it, and that is exactly the case (a low F1
    // against a widely-spaced comb) this interpolation exists for.
    float peak_freq = pitch_hz * (float)best_k;
    float p_minus = goertzel_power(samples, n, pitch_hz * (float)(best_k - 1), fs);
    float p_plus = goertzel_power(samples, n, pitch_hz * (float)(best_k + 1), fs);
    float y_minus = logf(p_minus + 1.0f);
    float y_0 = logf(best_power + 1.0f);
    float y_plus = logf(p_plus + 1.0f);
    float denom = y_minus - 2.0f * y_0 + y_plus;
    if (fabsf(denom) > 1e-6f)
    {
        float offset = 0.5f * (y_minus - y_plus) / denom;
        peak_freq += offset * pitch_hz;
    }

    *peak_freq_out = peak_freq;
    return best_power;
}

// Assert `formant_hz` is a real resonance: the loudest harmonic within 30%
// of the tabled centre is itself within 10% of it (say-design.md §10's
// "within 10% of the tabled centres"), and that harmonic outpowers two
// harmonics well clear of any of this vowel's three formants by >= 20 dB
// (a 100x power ratio).
static void assert_formant(const int16_t *samples, int n, float fs, float pitch_hz, float formant_hz,
                            float off_hz_a, float off_hz_b)
{
    float peak_freq;
    float p_center = find_peak_harmonic(samples, n, fs, pitch_hz, formant_hz * 0.7f, formant_hz * 1.3f, &peak_freq);
    float rel_err = fabsf(peak_freq - formant_hz) / formant_hz;
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.10f, rel_err);

    float p_off_a = goertzel_power(samples, n, nearest_harmonic(off_hz_a, pitch_hz), fs);
    float p_off_b = goertzel_power(samples, n, nearest_harmonic(off_hz_b, pitch_hz), fs);
    TEST_ASSERT_GREATER_THAN_FLOAT(100.0f * (p_off_a + 1.0f), p_center);
    TEST_ASSERT_GREATER_THAN_FLOAT(100.0f * (p_off_b + 1.0f), p_center);
}

//==========================================================================
// The §10 formant assertion, over iy, aa, uw
//==========================================================================

void test_iy_has_formants_at_270_and_2290(void)
{
    const SpeechPhoneme *f = &speech_phoneme_table[SPEECH_PH_IY];
    speech_render_sustained(SPEECH_PH_IY, PITCH_HZ, SPEECH_SAMPLE_RATE_HZ, render_buf, RENDER_SAMPLES);

    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, (float)f->f1, (float)f->f3 * 1.4f, (float)f->f3 * 1.8f);
    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, (float)f->f2, (float)f->f3 * 1.4f, (float)f->f3 * 1.8f);
}

void test_aa_has_formants_at_730_and_1090(void)
{
    const SpeechPhoneme *f = &speech_phoneme_table[SPEECH_PH_AA];
    speech_render_sustained(SPEECH_PH_AA, PITCH_HZ, SPEECH_SAMPLE_RATE_HZ, render_buf, RENDER_SAMPLES);

    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, (float)f->f1, (float)f->f3 * 1.4f, (float)f->f3 * 1.8f);
    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, (float)f->f2, (float)f->f3 * 1.4f, (float)f->f3 * 1.8f);
}

void test_uw_has_formants_at_300_and_870(void)
{
    const SpeechPhoneme *f = &speech_phoneme_table[SPEECH_PH_UW];
    speech_render_sustained(SPEECH_PH_UW, PITCH_HZ, SPEECH_SAMPLE_RATE_HZ, render_buf, RENDER_SAMPLES);

    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, (float)f->f1, (float)f->f3 * 1.4f, (float)f->f3 * 1.8f);
    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, (float)f->f2, (float)f->f3 * 1.4f, (float)f->f3 * 1.8f);
}

//==========================================================================
// The M0 deliverable: five sustained vowels rendered to .wav for the
// human listening gate. A person listening picks iy eh aa ao uw out of a
// shuffled set of five (say-design.md §12) -- this test only proves the
// files exist and are not silent/clipped; it cannot pass the gate itself.
//==========================================================================

static void render_gate_vowel(const char *path, SpeechPhonemeId id)
{
    speech_render_sustained(id, PITCH_HZ, SPEECH_SAMPLE_RATE_HZ, render_buf, RENDER_SAMPLES);

    int16_t peak = 0;
    for (int i = 0; i < RENDER_SAMPLES; i++)
    {
        int16_t s = render_buf[i];
        int16_t mag = (int16_t)(s < 0 ? -s : s);
        if (mag > peak)
        {
            peak = mag;
        }
    }
    TEST_ASSERT_GREATER_THAN_INT16_MESSAGE(1000, peak, path);
    TEST_ASSERT_LESS_OR_EQUAL_INT16_MESSAGE(32767, peak, path);

    write_wav_mono16(path, render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ);
}

void test_all_five_gate_vowels_render_to_wav(void)
{
    render_gate_vowel("speech_vowel_iy.wav", SPEECH_PH_IY);
    render_gate_vowel("speech_vowel_eh.wav", SPEECH_PH_EH);
    render_gate_vowel("speech_vowel_aa.wav", SPEECH_PH_AA);
    render_gate_vowel("speech_vowel_ao.wav", SPEECH_PH_AO);
    render_gate_vowel("speech_vowel_uw.wav", SPEECH_PH_UW);
}

//==========================================================================
// M1: the inventory
//==========================================================================

// Turn a hand-typed phoneme string ("ih n t r uw d er") into frames, the way
// `sayphonemes [ih n t r uw d er]` will. Everything is left at its table
// default: duration 0, voice pitch, unmarked stress.
static int frames_from_string(const char *text, SpeechFrame *out, int max)
{
    int n = 0;
    const char *p = text;
    while (*p && n < max)
    {
        while (*p == ' ')
        {
            p++;
        }
        char name[8];
        int len = 0;
        while (*p && *p != ' ' && len < (int)sizeof(name) - 1)
        {
            name[len++] = *p++;
        }
        if (len == 0)
        {
            break;
        }
        name[len] = '\0';

        int id = speech_phoneme_from_name(name);
        TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, id, name);
        out[n].phoneme = (uint8_t)id;
        out[n].dur_ms = 0;
        out[n].pitch = 0;
        out[n].stress = 1;
        n++;
    }
    return n;
}

static int render_words(const char *phonemes)
{
    SpeechFrame frames[64];
    int n = frames_from_string(phonemes, frames, 64);
    return speech_render(frames, n, PITCH_HZ, SPEECH_SAMPLE_RATE_HZ, word_buf, WORD_SAMPLES);
}

static int16_t peak_of(const int16_t *s, int from, int to)
{
    int16_t peak = 0;
    for (int i = from; i < to; i++)
    {
        int16_t mag = (int16_t)(s[i] < 0 ? -s[i] : s[i]);
        if (mag > peak)
        {
            peak = mag;
        }
    }
    return peak;
}

// Every one of the 41 rows must be reachable by its ARPABET name, and the
// name must come back. A row whose name is missing is a row `sayphonemes`
// can never address.
void test_all_41_phonemes_round_trip_by_name(void)
{
    for (int i = 0; i < SPEECH_PH_COUNT; i++)
    {
        TEST_ASSERT_NOT_NULL(speech_phoneme_names[i]);
        TEST_ASSERT_EQUAL_INT(i, speech_phoneme_from_name(speech_phoneme_names[i]));
    }
    TEST_ASSERT_EQUAL_INT(41, SPEECH_PH_COUNT);
    TEST_ASSERT_EQUAL_INT(SPEECH_PH_IY, speech_phoneme_from_name("IY")); // case-insensitive
    TEST_ASSERT_EQUAL_INT(-1, speech_phoneme_from_name("qq"));
}

// Every row must sound, and the pause must not. A silent row is a table
// typo -- a zero amplitude, or formants the resonators cannot reach -- and
// it would otherwise only show up as a word with a hole in it.
void test_every_phoneme_sounds_except_the_pause(void)
{
    for (int i = 0; i < SPEECH_PH_COUNT; i++)
    {
        SpeechFrame f = {(uint8_t)i, 0, 0, 1};
        int n = speech_render(&f, 1, PITCH_HZ, SPEECH_SAMPLE_RATE_HZ, word_buf, WORD_SAMPLES);
        TEST_ASSERT_GREATER_THAN_INT(0, n);

        int16_t peak = peak_of(word_buf, 0, n);
        if (i == SPEECH_PH_PAUSE)
        {
            TEST_ASSERT_EQUAL_INT16_MESSAGE(0, peak, "the pause must be silent");
        }
        else
        {
            TEST_ASSERT_GREATER_THAN_INT16_MESSAGE(1000, peak, speech_phoneme_names[i]);
        }
        TEST_ASSERT_LESS_THAN_INT16_MESSAGE(32767, peak, speech_phoneme_names[i]);
    }
}

// say-design.md §8.4 rejects a 12 kHz synthesis rate specifically so that
// `s` and `sh` do not land in the same place -- "a specific and famous way
// for a synthesizer to be unintelligible". That is a claim about the
// rendered audio, so it is a test: `s` must dominate above 4 kHz and `sh`
// must dominate in the 2-3 kHz band where its own energy sits.
void test_s_and_sh_are_not_the_same_sound(void)
{
    int n_s = render_words("s");
    float s_high = goertzel_power(word_buf, n_s, 5000.0f, SPEECH_SAMPLE_RATE_HZ);
    float s_mid = goertzel_power(word_buf, n_s, 2500.0f, SPEECH_SAMPLE_RATE_HZ);

    int n_sh = render_words("sh");
    float sh_high = goertzel_power(word_buf, n_sh, 5000.0f, SPEECH_SAMPLE_RATE_HZ);
    float sh_mid = goertzel_power(word_buf, n_sh, 2500.0f, SPEECH_SAMPLE_RATE_HZ);

    TEST_ASSERT_GREATER_THAN_FLOAT(s_mid, s_high);   // `s` is a high hiss
    TEST_ASSERT_GREATER_THAN_FLOAT(sh_high, sh_mid); // `sh` is a mid hush
}

// `r`'s signature is a very low F3, not its F1 or F2 -- the one formant
// value in the table that is doing perceptual work on its own.
void test_r_has_its_low_f3(void)
{
    TEST_ASSERT_LESS_THAN_UINT16(2000, speech_phoneme_table[SPEECH_PH_R].f3);

    int n = render_words("r");
    float at_f3 = goertzel_power(word_buf, n, 1400.0f, SPEECH_SAMPLE_RATE_HZ);
    float above = goertzel_power(word_buf, n, 2500.0f, SPEECH_SAMPLE_RATE_HZ);
    TEST_ASSERT_GREATER_THAN_FLOAT(above * 10.0f, at_f3);
}

// A stop is a closure of silence and then a burst (§8.1). The silence is the
// cue, so it is the thing to assert: after a vowel, a `t` goes quiet before
// it makes any noise at all.
void test_a_stop_closes_before_it_bursts(void)
{
    int n = render_words("aa t");
    int16_t vowel_peak = peak_of(word_buf, 0, n);
    TEST_ASSERT_GREATER_THAN_INT16(3000, vowel_peak);

    // Slide a 20 ms window (comfortably inside the closure) across the
    // render and take the quietest reading. The closure is silence, not
    // merely a dip, so the quietest window must be flat zero.
    int win = SPEECH_SAMPLE_RATE_HZ * 20 / 1000;
    int16_t quietest = 32767;
    for (int start = 0; start + win < n; start += win / 4)
    {
        int16_t p = peak_of(word_buf, start, start + win);
        if (p < quietest)
        {
            quietest = p;
        }
    }
    TEST_ASSERT_EQUAL_INT16(0, quietest);
}

// A diphthong is two rows and a glide between them (§6), so its second half
// must not sound like its first: `ay` starts at `aa` (F2 1090) and ends at
// `iy` (F2 2290).
void test_a_diphthong_moves(void)
{
    int n = render_words("ay");
    int third = n / 3;

    float first_low = goertzel_power(word_buf, third, 1100.0f, SPEECH_SAMPLE_RATE_HZ);
    float first_high = goertzel_power(word_buf, third, 2300.0f, SPEECH_SAMPLE_RATE_HZ);
    float last_low = goertzel_power(word_buf + 2 * third, third, 1100.0f, SPEECH_SAMPLE_RATE_HZ);
    float last_high = goertzel_power(word_buf + 2 * third, third, 2300.0f, SPEECH_SAMPLE_RATE_HZ);

    TEST_ASSERT_GREATER_THAN_FLOAT(first_high, first_low); // starts as `aa`
    TEST_ASSERT_GREATER_THAN_FLOAT(last_low, last_high);   // ends as `iy`
}

//==========================================================================
// The M1 deliverable: the ten Berzerk words, hand-typed as phonemes.
//
// The gate is a person listening and understanding them
// (say-design.md §12); this test only proves each renders, is not silent
// and does not clip. The transcriptions are the ones §11 gives, extended
// to ten from berzerk-design.md §14.2's thirty-word ROM vocabulary.
//==========================================================================

static const struct
{
    const char *word;
    const char *phonemes;
} berzerk_words[] = {
    {"intruder", "ih n t r uw d er"},
    {"alert", "ax l er t"},
    {"humanoid", "hh y uw m ax n oy d"},
    {"chicken", "ch ih k ax n"},
    {"robot", "r ow b aa t"},
    {"fight", "f ay t"},
    {"escape", "ih s k ey p"},
    {"destroy", "d ih s t r oy"},
    {"shoot", "sh uw t"},
    {"kill", "k ih l"},
};

void test_ten_berzerk_words_render_to_wav(void)
{
    for (int i = 0; i < (int)(sizeof(berzerk_words) / sizeof(berzerk_words[0])); i++)
    {
        int n = render_words(berzerk_words[i].phonemes);
        TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, n, berzerk_words[i].word);

        int16_t peak = peak_of(word_buf, 0, n);
        TEST_ASSERT_GREATER_THAN_INT16_MESSAGE(3000, peak, berzerk_words[i].word);
        TEST_ASSERT_LESS_THAN_INT16_MESSAGE(32767, peak, berzerk_words[i].word);

        char path[64];
        snprintf(path, sizeof(path), "speech_word_%s.wav", berzerk_words[i].word);
        write_wav_mono16(path, word_buf, n, SPEECH_SAMPLE_RATE_HZ);
    }
}

// And the sentence the words are for, since a word list is not the thing
// Berzerk says: "INTRUDER ALERT! INTRUDER ALERT!" ($2C4A). The pauses are
// the sentence punctuation §5.1 will insert for `.` and `!`.
void test_intruder_alert_renders_to_wav(void)
{
    int n = render_words("ih n t r uw d er _ ax l er t _ _ ih n t r uw d er _ ax l er t");
    TEST_ASSERT_GREATER_THAN_INT(0, n);
    TEST_ASSERT_LESS_THAN_INT(WORD_SAMPLES, n); // the buffer was not the limit
    TEST_ASSERT_LESS_THAN_INT16(32767, peak_of(word_buf, 0, n));
    write_wav_mono16("speech_intruder_alert.wav", word_buf, n, SPEECH_SAMPLE_RATE_HZ);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_iy_has_formants_at_270_and_2290);
    RUN_TEST(test_aa_has_formants_at_730_and_1090);
    RUN_TEST(test_uw_has_formants_at_300_and_870);
    RUN_TEST(test_all_five_gate_vowels_render_to_wav);
    RUN_TEST(test_all_41_phonemes_round_trip_by_name);
    RUN_TEST(test_every_phoneme_sounds_except_the_pause);
    RUN_TEST(test_s_and_sh_are_not_the_same_sound);
    RUN_TEST(test_r_has_its_low_f3);
    RUN_TEST(test_a_stop_closes_before_it_bursts);
    RUN_TEST(test_a_diphthong_moves);
    RUN_TEST(test_ten_berzerk_words_render_to_wav);
    RUN_TEST(test_intruder_alert_renders_to_wav);
    return UNITY_END();
}
