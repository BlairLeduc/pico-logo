//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  P16 M0 gate: "can we make a vowel?" (core/speech_synth.c). Formant
//  assertions via Goertzel (say-design.md §10) so the resonators are
//  checked without an ear, plus the M0 deliverable itself -- five sustained
//  vowels rendered to .wav in the build directory for the human listening
//  gate (say-design.md §12).
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
    const SpeechFormant *f = &speech_vowel_table[SPEECH_VOWEL_IY];
    speech_render_sustained_vowel(SPEECH_VOWEL_IY, PITCH_HZ, SPEECH_SAMPLE_RATE_HZ, render_buf, RENDER_SAMPLES);

    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, f->f1, f->f3 * 1.4f, f->f3 * 1.8f);
    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, f->f2, f->f3 * 1.4f, f->f3 * 1.8f);
}

void test_aa_has_formants_at_730_and_1090(void)
{
    const SpeechFormant *f = &speech_vowel_table[SPEECH_VOWEL_AA];
    speech_render_sustained_vowel(SPEECH_VOWEL_AA, PITCH_HZ, SPEECH_SAMPLE_RATE_HZ, render_buf, RENDER_SAMPLES);

    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, f->f1, f->f3 * 1.4f, f->f3 * 1.8f);
    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, f->f2, f->f3 * 1.4f, f->f3 * 1.8f);
}

void test_uw_has_formants_at_300_and_870(void)
{
    const SpeechFormant *f = &speech_vowel_table[SPEECH_VOWEL_UW];
    speech_render_sustained_vowel(SPEECH_VOWEL_UW, PITCH_HZ, SPEECH_SAMPLE_RATE_HZ, render_buf, RENDER_SAMPLES);

    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, f->f1, f->f3 * 1.4f, f->f3 * 1.8f);
    assert_formant(render_buf, RENDER_SAMPLES, SPEECH_SAMPLE_RATE_HZ, PITCH_HZ, f->f2, f->f3 * 1.4f, f->f3 * 1.8f);
}

//==========================================================================
// The M0 deliverable: five sustained vowels rendered to .wav for the
// human listening gate. A person listening picks iy eh aa ao uw out of a
// shuffled set of five (say-design.md §12) -- this test only proves the
// files exist and are not silent/clipped; it cannot pass the gate itself.
//==========================================================================

static void render_gate_vowel(const char *path, SpeechVowelId id)
{
    speech_render_sustained_vowel(id, PITCH_HZ, SPEECH_SAMPLE_RATE_HZ, render_buf, RENDER_SAMPLES);

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
    render_gate_vowel("speech_vowel_iy.wav", SPEECH_VOWEL_IY);
    render_gate_vowel("speech_vowel_eh.wav", SPEECH_VOWEL_EH);
    render_gate_vowel("speech_vowel_aa.wav", SPEECH_VOWEL_AA);
    render_gate_vowel("speech_vowel_ao.wav", SPEECH_VOWEL_AO);
    render_gate_vowel("speech_vowel_uw.wav", SPEECH_VOWEL_UW);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_iy_has_formants_at_270_and_2290);
    RUN_TEST(test_aa_has_formants_at_730_and_1090);
    RUN_TEST(test_uw_has_formants_at_300_and_870);
    RUN_TEST(test_all_five_gate_vowels_render_to_wav);
    return UNITY_END();
}
