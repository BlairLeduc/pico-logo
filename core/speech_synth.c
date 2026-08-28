//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Formant speech synthesizer core (P16), M0. See speech_synth.h.
//

#include "speech_synth.h"

#include <math.h>

// Peterson & Barney (1952) male-average formant centres, say-design.md §6.
// Amplitudes are not published data -- the report gives frequencies only --
// so all ten rows share one falling ladder (F1 loudest, F3 quietest), which
// is the vocal tract's own spectral tilt with an impulse-train source. This
// is the value M1 tunes once the M0 listening gate says which way to move it.
#define SPEECH_A1 1.0f
#define SPEECH_A2 0.5f
#define SPEECH_A3 0.25f

const SpeechFormant speech_vowel_table[SPEECH_VOWEL_COUNT] = {
    [SPEECH_VOWEL_IY] = {270, 2290, 3010, SPEECH_A1, SPEECH_A2, SPEECH_A3},
    [SPEECH_VOWEL_IH] = {390, 1990, 2550, SPEECH_A1, SPEECH_A2, SPEECH_A3},
    [SPEECH_VOWEL_EH] = {530, 1840, 2480, SPEECH_A1, SPEECH_A2, SPEECH_A3},
    [SPEECH_VOWEL_AE] = {660, 1720, 2410, SPEECH_A1, SPEECH_A2, SPEECH_A3},
    [SPEECH_VOWEL_AH] = {640, 1190, 2390, SPEECH_A1, SPEECH_A2, SPEECH_A3},
    [SPEECH_VOWEL_AA] = {730, 1090, 2440, SPEECH_A1, SPEECH_A2, SPEECH_A3},
    [SPEECH_VOWEL_AO] = {570, 840, 2410, SPEECH_A1, SPEECH_A2, SPEECH_A3},
    [SPEECH_VOWEL_UH] = {440, 1020, 2240, SPEECH_A1, SPEECH_A2, SPEECH_A3},
    [SPEECH_VOWEL_UW] = {300, 870, 2240, SPEECH_A1, SPEECH_A2, SPEECH_A3},
    [SPEECH_VOWEL_ER] = {490, 1350, 1690, SPEECH_A1, SPEECH_A2, SPEECH_A3},
};

// Formant bandwidths, Hz. Not vowel-specific data in say-design.md §6 either;
// these are the values textbook Klatt-style synthesizers use (narrower at
// F1, widening up the spectrum).
#define SPEECH_BW1 60.0f
#define SPEECH_BW2 90.0f
#define SPEECH_BW3 120.0f

void speech_resonator_tune(SpeechResonator *r, float freq_hz, float bandwidth_hz, float sample_rate_hz)
{
    float t = 1.0f / sample_rate_hz;
    float c = -expf(-2.0f * (float)M_PI * bandwidth_hz * t);
    float b = 2.0f * expf(-(float)M_PI * bandwidth_hz * t) * cosf(2.0f * (float)M_PI * freq_hz * t);
    r->a = 1.0f - b - c;
    r->b = b;
    r->c = c;
}

float speech_resonator_step(SpeechResonator *r, float x)
{
    float y0 = r->a * x + r->b * r->y1 + r->c * r->y2;
    r->y2 = r->y1;
    r->y1 = y0;
    return y0;
}

void speech_render_sustained_vowel(SpeechVowelId id, float pitch_hz, float sample_rate_hz,
                                    int16_t *out, int sample_count)
{
    const SpeechFormant *f = &speech_vowel_table[id];

    SpeechResonator r1 = {0}, r2 = {0}, r3 = {0};
    speech_resonator_tune(&r1, f->f1, SPEECH_BW1, sample_rate_hz);
    speech_resonator_tune(&r2, f->f2, SPEECH_BW2, sample_rate_hz);
    speech_resonator_tune(&r3, f->f3, SPEECH_BW3, sample_rate_hz);

    // Impulse-train source (say-design.md §8.1): one full-scale sample per
    // pitch period, silence otherwise. A discrete impulse has energy across
    // every harmonic of pitch_hz up to Nyquist, which is the "shaped so it
    // has energy across the formant range" the design calls for -- no
    // separate spectral shaping needed on top of it.
    float period = sample_rate_hz / pitch_hz;
    float phase = 0.0f;

    const float gain = 20000.0f;
    for (int i = 0; i < sample_count; i++)
    {
        float source = 0.0f;
        phase += 1.0f;
        if (phase >= period)
        {
            phase -= period;
            source = 1.0f;
        }

        // Alternating sign on the parallel branches (Klatt's convention):
        // each two-pole resonator also shifts phase, and without the
        // alternation adjacent branches partially cancel near their shared
        // boundary and shift the composite peak onto the wrong harmonic --
        // measured directly against the M0 gate's Goertzel assertion.
        float y = f->a1 * speech_resonator_step(&r1, source) -
                   f->a2 * speech_resonator_step(&r2, source) +
                   f->a3 * speech_resonator_step(&r3, source);

        float sample = y * gain;
        if (sample > 32767.0f)
        {
            sample = 32767.0f;
        }
        else if (sample < -32768.0f)
        {
            sample = -32768.0f;
        }
        out[i] = (int16_t)sample;
    }
}
