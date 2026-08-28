//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Formant speech synthesizer core (P16). See docs/say-design.md §6 (the
//  phoneme table) and §8 (the parallel formant synthesizer). Pure math, no
//  device -- this is the host-testable half, and the mirror of notation.c
//  for `play`: notation.c turns note words into SoundEvents, this turns
//  phonemes into samples.
//
//  M0 built the resonator and the ten monophthongs. M1 is the full 41-phoneme
//  inventory of §6 -- stops, affricates, fricatives, nasals, approximants and
//  the five diphthongs -- plus the §8.3 transition classes, which is where
//  intelligibility actually lives. Still missing, and M3's: the mixer slot,
//  the block-at-a-time IRQ wrapper, and re-deriving coefficients from
//  `sound_reclock` (§8.4).
//

#pragma once

#include "devices/hardware.h" // SpeechFrame

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Sample rate the resonators run at: the PSG mixer's 36.6 kHz mix rate
    // (sound-design.md §2), not a speech-typical 10-12 kHz -- see
    // say-design.md §8.4 for why a resampler is rejected.
#define SPEECH_SAMPLE_RATE_HZ 36600

    // Parameter update granularity, in samples: one PSG refill block
    // (SOUND_RING_HALF / OVERSAMPLE = 128 frames, ~3.5 ms at the mix rate).
    // Formant frequencies and amplitudes are re-derived on this boundary and
    // held flat across it, which is the cadence say-design.md §8.3 fixes and
    // the one the ADSR envelopes already use.
#define SPEECH_BLOCK_FRAMES 128

    // The §6 alphabet: ARPABET, 40 phonemes plus a pause. The order here is
    // the order of that table, and the numbering is the wire format of
    // SpeechFrame.phoneme, so rows may be corrected but not reordered.
    typedef enum SpeechPhonemeId
    {
        // Vowels (16). The last five are diphthongs: two of these rows and a
        // glide between them, which is what they physically are (§6).
        SPEECH_PH_IY,
        SPEECH_PH_IH,
        SPEECH_PH_EY,
        SPEECH_PH_EH,
        SPEECH_PH_AE,
        SPEECH_PH_AA,
        SPEECH_PH_AO,
        SPEECH_PH_OW,
        SPEECH_PH_UH,
        SPEECH_PH_UW,
        SPEECH_PH_AH,
        SPEECH_PH_AX,
        SPEECH_PH_ER,
        SPEECH_PH_AY,
        SPEECH_PH_AW,
        SPEECH_PH_OY,
        // Stops (6)
        SPEECH_PH_P,
        SPEECH_PH_B,
        SPEECH_PH_T,
        SPEECH_PH_D,
        SPEECH_PH_K,
        SPEECH_PH_G,
        // Affricates (2)
        SPEECH_PH_CH,
        SPEECH_PH_JH,
        // Fricatives (9)
        SPEECH_PH_F,
        SPEECH_PH_V,
        SPEECH_PH_TH,
        SPEECH_PH_DH,
        SPEECH_PH_S,
        SPEECH_PH_Z,
        SPEECH_PH_SH,
        SPEECH_PH_ZH,
        SPEECH_PH_HH,
        // Nasals (3)
        SPEECH_PH_M,
        SPEECH_PH_N,
        SPEECH_PH_NG,
        // Approximants (4)
        SPEECH_PH_L,
        SPEECH_PH_R,
        SPEECH_PH_W,
        SPEECH_PH_Y,
        // Pause (1)
        SPEECH_PH_PAUSE,
        SPEECH_PH_COUNT
    } SpeechPhonemeId;

    // `glide_to` when a row is not a diphthong.
#define SPEECH_PH_NONE 0xFF

    // Phoneme flags. The low nibble is the manner of articulation (which
    // decides the excitation source, §8.1); the high nibble is the transition
    // class (§8.3), an index into a table of glide lengths -- a vowel into a
    // nasal moves differently from a vowel into a stop.
#define SPEECH_F_VOICED 0x01    // impulse-train source at the fundamental
#define SPEECH_F_STOP 0x02      // a closure of silence, then a burst
#define SPEECH_F_FRICATIVE 0x04 // noise source through the resonators
#define SPEECH_F_NASAL 0x08     // murmur: strong low F1, weak above it
#define SPEECH_F_TRANS_SHIFT 4
#define SPEECH_F_TRANS_MASK 0x30

    // Transition classes, shifted into place for the table below.
#define SPEECH_TC_ABRUPT (0 << SPEECH_F_TRANS_SHIFT) // stops, affricates, pause
#define SPEECH_TC_SHORT (1 << SPEECH_F_TRANS_SHIFT)  // fricatives
#define SPEECH_TC_MEDIUM (2 << SPEECH_F_TRANS_SHIFT) // vowels, nasals
#define SPEECH_TC_LONG (3 << SPEECH_F_TRANS_SHIFT)   // approximants, which are glides

    // One row of the §6 table: 12 B, 41 rows, ~500 B of flash. Must stay
    // `const` -- §9.1's one specific way this feature reproduces the
    // `repl_init` out-of-memory panic is a table that lands in .data.
    typedef struct SpeechPhoneme
    {
        uint16_t f1, f2, f3; // formant centres, Hz
        uint8_t a1, a2, a3;  // relative formant amplitude, 0..255
        uint8_t dur_ms;      // nominal steady duration
        uint8_t flags;       // manner | transition class
        uint8_t glide_to;    // diphthongs: the row glided to; else SPEECH_PH_NONE
    } SpeechPhoneme;

    extern const SpeechPhoneme speech_phoneme_table[SPEECH_PH_COUNT];

    // ARPABET names, lower case, indexed by SpeechPhonemeId. The pause is "_".
    extern const char *const speech_phoneme_names[SPEECH_PH_COUNT];

    // Look a phoneme word up, case-insensitively. Returns its id, or -1 if
    // the word is not one of the 41.
    int speech_phoneme_from_name(const char *name);

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

    // Render one utterance. `frames` is the phoneme list the front end (or
    // `sayphonemes`) produced; `pitch_hz` is the voice fundamental that a
    // frame's own `pitch` overrides. Returns the number of samples written,
    // which stops short of the whole utterance if `max_samples` runs out.
    int speech_render(const SpeechFrame *frames, int n, float pitch_hz, float sample_rate_hz,
                      int16_t *out, int max_samples);

    // Render `sample_count` samples of one phoneme held steady, with no
    // transition in or out. This is the M0 gate's entry point and stays as
    // its regression: it is what the §10 formant assertion measures.
    void speech_render_sustained(SpeechPhonemeId id, float pitch_hz, float sample_rate_hz,
                                 int16_t *out, int sample_count);

#ifdef __cplusplus
}
#endif
