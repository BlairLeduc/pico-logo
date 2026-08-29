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
//  intelligibility actually lives. M3 turned the renderer inside out: what
//  was a loop that wrote a whole utterance into a buffer is now SpeechEngine,
//  a resumable state machine that hands out any number of samples at a time
//  and pulls phonemes from its own queue as it needs them, because that is
//  what a refill IRQ can call. `speech_render` below is the same engine run
//  to completion offline, which is all the .wav gates ever needed.
//

#pragma once

#include "devices/hardware.h" // SpeechFrame
#include "limits.h"          // SPEECH_QUEUE_LEN

#include <stdbool.h>
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

    //======================================================================
    // The voice: say-design.md §5.5's four knobs
    //======================================================================

    // SAM's four knobs, which are the ones that reach a robot in one line.
    // All four are 1..255 so `setvoice` validates them with one rule; the
    // nominal for the three scales is 128, which is the phoneme table
    // unmodified. Pitch is in the SpeechFrame unit -- Hz/2 -- so a frame's
    // own pitch and the voice default are the same kind of number.
    typedef struct SpeechVoice
    {
        uint8_t pitch;  // fundamental, Hz/2: 50 is 100 Hz
        uint8_t speed;  // duration scale, 128/speed: larger is faster
        uint8_t mouth;  // scales F2 and F3, the front cavity
        uint8_t throat; // scales F1, the back cavity
    } SpeechVoice;

#define SPEECH_VOICE_PITCH_DEFAULT 50 // 100 Hz, the M0/M1 gates' fundamental
#define SPEECH_VOICE_NOMINAL 128      // speed/mouth/throat: the table as tabled

    //======================================================================
    // The engine
    //======================================================================

    // One interpolation target: everything that changes on a block boundary
    // (§8.3). Public only because SpeechEngine holds several of them and the
    // engine is placed by its owner rather than allocated.
    typedef struct SpeechParams
    {
        float f1, f2, f3;
        float a1, a2, a3; // 0..1
        float voiced;     // impulse-train source level, 0..1
        float noise;      // noise source level, 0..1
        bool noisy;       // pick the wider bandwidth set
    } SpeechParams;

    // Where the engine is inside the phoneme it is rendering. A stop closes
    // before it bursts, so it visits CUT and CLOSURE first; everything else
    // starts at TRANSITION. FADE is the tail after the queue runs dry.
    typedef enum SpeechStage
    {
        SPEECH_STAGE_IDLE = 0,
        SPEECH_STAGE_CUT,
        SPEECH_STAGE_CLOSURE,
        SPEECH_STAGE_TRANSITION,
        SPEECH_STAGE_BODY,
        SPEECH_STAGE_FADE
    } SpeechStage;

    // The whole speech back end, in one struct its owner places: ~900 B, of
    // which the phoneme queue is 516 (§9.1). The board keeps one in .bss and
    // drives it from the refill IRQ; the host tests keep one on the stack.
    //
    // Producer/consumer: `speech_engine_queue` (thread context) writes at
    // `tail`, `speech_engine_render` (IRQ context) reads at `head`. The
    // device wrapper serialises the two the way sound_queue does.
    typedef struct SpeechEngine
    {
        SpeechResonator r1, r2, r3;
        float sample_rate;
        // Source levels for this sample rate (B52). SPEECH_VOICED_GAIN and
        // SPEECH_NOISE_GAIN are the levels at SPEECH_SAMPLE_RATE_HZ, where
        // these are exactly those; at another rate they compensate for the
        // resonator's input coefficient, which falls as 1/fs^2.
        float voiced_gain;
        float noise_gain;
        float base_pitch_hz; // the voice's fundamental
        float pitch_hz;      // in force now: a frame's own pitch overrides
        float phase;         // impulse-train phase, in samples
        uint16_t lfsr;       // noise shift register, the PSG noise voices'
        SpeechVoice voice;

        // The segment being rendered: parameters travel from `seg_from` to
        // `seg_to` across `seg_len` samples.
        SpeechParams seg_from, seg_to;
        int seg_len, seg_done;
        uint8_t stage; // SpeechStage

        // Held flat across a block, which is what makes the rendered samples
        // depend on the segment structure alone and not on how many samples
        // the caller happens to ask for (§8.3).
        SpeechParams cur;
        int block_left;

        // The phoneme being rendered, and the one before it.
        SpeechParams tgt, end, prev;
        int dur, trans; // samples
        int cls, prev_class;

        // Utterance queue. One slot is reserved so full and empty are
        // distinguishable, exactly as the PSG sequencer's ring is.
        SpeechFrame q[SPEECH_QUEUE_LEN + 1];
        volatile uint16_t head, tail;
    } SpeechEngine;

    // Place an engine: silent, empty, default voice, coefficients derived
    // for `sample_rate_hz`.
    void speech_engine_init(SpeechEngine *e, float sample_rate_hz);

    // Re-derive everything that depends on the sample rate. `hw.setcpu`
    // moves the mix rate, and every resonator coefficient is a function of
    // it (§8.4); like sound_reclock this abandons what is in flight rather
    // than letting it slide, because the ring already holds samples made at
    // the old rate.
    void speech_engine_set_rate(SpeechEngine *e, float sample_rate_hz);

    // The §5.5 knobs. Takes effect on the next phoneme, not mid-vowel.
    void speech_engine_set_voice(SpeechEngine *e, const SpeechVoice *v);
    SpeechVoice speech_engine_get_voice(const SpeechEngine *e);

    // Append phonemes to the utterance. Returns how many were accepted,
    // which is short of `n` when the queue fills.
    int speech_engine_queue(SpeechEngine *e, const SpeechFrame *frames, int n);
    int speech_engine_free_slots(const SpeechEngine *e);

    // True while something is sounding or still queued.
    bool speech_engine_busy(const SpeechEngine *e);

    // Abandon the utterance: clear the queue and fade out over one
    // transition rather than cutting, which would click (§5.6).
    void speech_engine_stop(SpeechEngine *e);

    // Render exactly `count` samples, pulling phonemes as it needs them and
    // writing silence when there are none. The only call the refill IRQ
    // makes, and the sound it produces does not depend on `count`.
    void speech_engine_render(SpeechEngine *e, int16_t *out, int count);

    // Render one utterance offline, to completion: the engine above driven
    // by a loop instead of by an IRQ. This is what the .wav gates use.
    // `pitch_hz` is rounded to the voice's Hz/2 resolution. Returns the
    // number of samples written, which stops short of the whole utterance
    // if `max_samples` runs out.
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
