//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Formant speech synthesizer core (P16), M0: "can we make a vowel?" See
//  docs/say-design.md §6 (the phoneme table) and §8 (the parallel formant
//  synthesizer). Pure math, no device -- this is the host-testable half the
//  M0 gate needs, and the mirror of notation.c for `play`.
//
//  Only the ten English monophthongs are tabled here. Stops, fricatives,
//  nasals, diphthong glides and the resonator's re-tune-on-clock hook all
//  arrive with M1/M3; M0 proves the resonator and the source against a
//  sustained vowel.
//

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Sample rate the resonators run at: the PSG mixer's 36.6 kHz mix rate
    // (sound-design.md §2), not a speech-typical 10-12 kHz -- see
    // say-design.md §8.4 for why a resampler is rejected.
#define SPEECH_SAMPLE_RATE_HZ 36600

    // A vowel's three formants: centre frequency and relative amplitude
    // (say-design.md §6). Order matches the table there.
    typedef enum SpeechVowelId
    {
        SPEECH_VOWEL_IY,
        SPEECH_VOWEL_IH,
        SPEECH_VOWEL_EH,
        SPEECH_VOWEL_AE,
        SPEECH_VOWEL_AH,
        SPEECH_VOWEL_AA,
        SPEECH_VOWEL_AO,
        SPEECH_VOWEL_UH,
        SPEECH_VOWEL_UW,
        SPEECH_VOWEL_ER,
        SPEECH_VOWEL_COUNT
    } SpeechVowelId;

    typedef struct SpeechFormant
    {
        float f1, f2, f3; // formant centres, Hz -- Peterson & Barney (1952) male-average
        float a1, a2, a3; // relative formant amplitude, 0..1
    } SpeechFormant;

    // const, say-design.md §9.1: must stay in flash, never .data, on target.
    extern const SpeechFormant speech_vowel_table[SPEECH_VOWEL_COUNT];

    // One two-pole resonator, Klatt's standard form (say-design.md §8.2):
    //   y[n] = a*x[n] + b*y[n-1] + c*y[n-2]
    typedef struct SpeechResonator
    {
        float a, b, c; // coefficients, re-derived on tune
        float y1, y2;  // state: y[n-1], y[n-2]
    } SpeechResonator;

    // (Re)derive a resonator's coefficients for a centre frequency and
    // bandwidth at the given sample rate. Called once per parameter update,
    // not per sample.
    void speech_resonator_tune(SpeechResonator *r, float freq_hz, float bandwidth_hz, float sample_rate_hz);

    // Filter one source sample through the resonator, advancing its state.
    float speech_resonator_step(SpeechResonator *r, float x);

    // Render `sample_count` samples of a sustained (steady-state) vowel into
    // `out`, voiced by an impulse-train source at `pitch_hz`. This is the M0
    // gate: three formant resonators in parallel over one source, no device,
    // no phoneme list, no transitions.
    void speech_render_sustained_vowel(SpeechVowelId id, float pitch_hz, float sample_rate_hz,
                                        int16_t *out, int sample_count);

#ifdef __cplusplus
}
#endif
