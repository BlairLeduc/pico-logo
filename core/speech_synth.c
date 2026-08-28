//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Formant speech synthesizer core (P16). See speech_synth.h.
//

#include "speech_synth.h"

#include <math.h>
#include <strings.h>

//==========================================================================
// The §6 phoneme table
//==========================================================================

// The vowel amplitude ladder (F1 loudest, F3 quietest) is the vocal tract's
// own spectral tilt with an impulse-train source. Peterson & Barney publish
// frequencies only, so the ladder is not their data; it is the value the M0
// listening gate was passed with, and every monophthong shares it.
#define VOWEL_AMPS 255, 128, 64

// Frequencies for the ten monophthongs are Peterson & Barney (1952)
// male-average measurements, entered as published. Everything else -- the
// consonant loci, the amplitudes, the durations -- is the textbook
// Klatt-style parameterisation, and is what M1's listening gate judges.
//
// The five diphthongs cost a start row, an end row and a duration rather
// than a row of their own (§6), which is `glide_to` below: `ey` is `eh`
// gliding to `iy`, `ay` is `aa` gliding to `iy`, and so on.
//
// The pause holds the neutral-tract frequencies at zero amplitude rather
// than zero frequency, so a transition into silence fades out instead of
// sweeping the formants down to DC on the way.
const SpeechPhoneme speech_phoneme_table[SPEECH_PH_COUNT] = {
    // Vowels
    [SPEECH_PH_IY] = {270, 2290, 3010, VOWEL_AMPS, 100, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_IH] = {390, 1990, 2550, VOWEL_AMPS, 70, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_EY] = {530, 1840, 2480, VOWEL_AMPS, 150, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_IY},
    [SPEECH_PH_EH] = {530, 1840, 2480, VOWEL_AMPS, 90, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_AE] = {660, 1720, 2410, VOWEL_AMPS, 110, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_AA] = {730, 1090, 2440, VOWEL_AMPS, 110, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_AO] = {570, 840, 2410, VOWEL_AMPS, 110, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_OW] = {570, 840, 2410, VOWEL_AMPS, 150, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_UW},
    [SPEECH_PH_UH] = {440, 1020, 2240, VOWEL_AMPS, 70, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_UW] = {300, 870, 2240, VOWEL_AMPS, 100, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_AH] = {640, 1190, 2390, VOWEL_AMPS, 90, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    // The schwa is a reduced `ah` (§6) -- reduced meaning centralised, so it
    // is the neutral tract: evenly spaced formants and a short duration.
    [SPEECH_PH_AX] = {500, 1500, 2500, VOWEL_AMPS, 50, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_ER] = {490, 1350, 1690, VOWEL_AMPS, 110, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_AY] = {730, 1090, 2440, VOWEL_AMPS, 150, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_IY},
    [SPEECH_PH_AW] = {730, 1090, 2440, VOWEL_AMPS, 150, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_UW},
    [SPEECH_PH_OY] = {570, 840, 2410, VOWEL_AMPS, 150, SPEECH_F_VOICED | SPEECH_TC_MEDIUM, SPEECH_PH_IY},

    // Stops: the row is the release burst -- the closure of silence in front
    // of it is SPEECH_CLOSURE_MS, and `dur_ms` is the burst alone. The three
    // places of articulation are three F2 loci: labial low, alveolar high
    // with the energy up at F3, velar in between.
    [SPEECH_PH_P] = {400, 1100, 2150, 200, 160, 120, 20, SPEECH_F_STOP | SPEECH_TC_ABRUPT, SPEECH_PH_NONE},
    [SPEECH_PH_B] = {400, 1100, 2150, 200, 160, 120, 15, SPEECH_F_VOICED | SPEECH_F_STOP | SPEECH_TC_ABRUPT, SPEECH_PH_NONE},
    [SPEECH_PH_T] = {400, 1800, 3000, 120, 200, 255, 20, SPEECH_F_STOP | SPEECH_TC_ABRUPT, SPEECH_PH_NONE},
    [SPEECH_PH_D] = {400, 1800, 2600, 120, 200, 200, 15, SPEECH_F_VOICED | SPEECH_F_STOP | SPEECH_TC_ABRUPT, SPEECH_PH_NONE},
    [SPEECH_PH_K] = {350, 1900, 2600, 150, 255, 200, 25, SPEECH_F_STOP | SPEECH_TC_ABRUPT, SPEECH_PH_NONE},
    [SPEECH_PH_G] = {350, 1900, 2600, 150, 255, 200, 18, SPEECH_F_VOICED | SPEECH_F_STOP | SPEECH_TC_ABRUPT, SPEECH_PH_NONE},

    // Affricates: a stop closure and then frication, which is what they are
    // (`ch` is `t` into `sh`), so both flags are set and the burst sustains
    // instead of decaying.
    [SPEECH_PH_CH] = {300, 2200, 3200, 40, 255, 200, 110, SPEECH_F_STOP | SPEECH_F_FRICATIVE | SPEECH_TC_ABRUPT, SPEECH_PH_NONE},
    [SPEECH_PH_JH] = {300, 2200, 3200, 60, 255, 200, 80, SPEECH_F_VOICED | SPEECH_F_STOP | SPEECH_F_FRICATIVE | SPEECH_TC_ABRUPT, SPEECH_PH_NONE},

    // Fricatives. The voiced ones run both sources (§8.1) and are shorter
    // than their voiceless partners, which is the main cue that tells them
    // apart. `s`/`z` put their energy above 4 kHz -- the specific thing
    // §8.4's honest sample rate exists to keep distinct from `sh`/`zh`.
    [SPEECH_PH_F] = {400, 1100, 2150, 60, 120, 180, 100, SPEECH_F_FRICATIVE | SPEECH_TC_SHORT, SPEECH_PH_NONE},
    [SPEECH_PH_V] = {400, 1100, 2150, 60, 120, 180, 60, SPEECH_F_VOICED | SPEECH_F_FRICATIVE | SPEECH_TC_SHORT, SPEECH_PH_NONE},
    [SPEECH_PH_TH] = {400, 1400, 2600, 50, 120, 200, 100, SPEECH_F_FRICATIVE | SPEECH_TC_SHORT, SPEECH_PH_NONE},
    [SPEECH_PH_DH] = {400, 1400, 2600, 50, 120, 200, 50, SPEECH_F_VOICED | SPEECH_F_FRICATIVE | SPEECH_TC_SHORT, SPEECH_PH_NONE},
    [SPEECH_PH_S] = {320, 4200, 5500, 20, 80, 255, 110, SPEECH_F_FRICATIVE | SPEECH_TC_SHORT, SPEECH_PH_NONE},
    [SPEECH_PH_Z] = {320, 4200, 5500, 20, 80, 255, 70, SPEECH_F_VOICED | SPEECH_F_FRICATIVE | SPEECH_TC_SHORT, SPEECH_PH_NONE},
    [SPEECH_PH_SH] = {300, 2200, 3200, 20, 255, 200, 110, SPEECH_F_FRICATIVE | SPEECH_TC_SHORT, SPEECH_PH_NONE},
    [SPEECH_PH_ZH] = {300, 2200, 3200, 20, 255, 200, 70, SPEECH_F_VOICED | SPEECH_F_FRICATIVE | SPEECH_TC_SHORT, SPEECH_PH_NONE},
    // `hh` is breath through a neutral tract: noise, no voicing, and the
    // transition into the vowel after it does the rest of the work.
    [SPEECH_PH_HH] = {500, 1500, 2500, 220, 200, 180, 60, SPEECH_F_FRICATIVE | SPEECH_TC_SHORT, SPEECH_PH_NONE},

    // Nasals: the murmur is a strong low nasal formant and very little above
    // it, which is the amplitude ladder rather than the frequencies.
    [SPEECH_PH_M] = {250, 1100, 2150, 255, 50, 30, 70, SPEECH_F_VOICED | SPEECH_F_NASAL | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_N] = {250, 1700, 2600, 255, 50, 30, 70, SPEECH_F_VOICED | SPEECH_F_NASAL | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},
    [SPEECH_PH_NG] = {250, 2000, 2600, 255, 50, 30, 70, SPEECH_F_VOICED | SPEECH_F_NASAL | SPEECH_TC_MEDIUM, SPEECH_PH_NONE},

    // Approximants are glides by nature, so they take the longest transition
    // class. `r`'s signature is its very low F3, not its F1 or F2.
    [SPEECH_PH_L] = {360, 1300, 2900, 255, 100, 60, 70, SPEECH_F_VOICED | SPEECH_TC_LONG, SPEECH_PH_NONE},
    [SPEECH_PH_R] = {310, 1060, 1380, 255, 150, 100, 70, SPEECH_F_VOICED | SPEECH_TC_LONG, SPEECH_PH_NONE},
    [SPEECH_PH_W] = {290, 610, 2150, 255, 100, 50, 60, SPEECH_F_VOICED | SPEECH_TC_LONG, SPEECH_PH_NONE},
    [SPEECH_PH_Y] = {260, 2070, 3020, 255, 100, 80, 60, SPEECH_F_VOICED | SPEECH_TC_LONG, SPEECH_PH_NONE},

    [SPEECH_PH_PAUSE] = {500, 1500, 2500, 0, 0, 0, 100, SPEECH_TC_ABRUPT, SPEECH_PH_NONE},
};

const char *const speech_phoneme_names[SPEECH_PH_COUNT] = {
    [SPEECH_PH_IY] = "iy", [SPEECH_PH_IH] = "ih", [SPEECH_PH_EY] = "ey", [SPEECH_PH_EH] = "eh",
    [SPEECH_PH_AE] = "ae", [SPEECH_PH_AA] = "aa", [SPEECH_PH_AO] = "ao", [SPEECH_PH_OW] = "ow",
    [SPEECH_PH_UH] = "uh", [SPEECH_PH_UW] = "uw", [SPEECH_PH_AH] = "ah", [SPEECH_PH_AX] = "ax",
    [SPEECH_PH_ER] = "er", [SPEECH_PH_AY] = "ay", [SPEECH_PH_AW] = "aw", [SPEECH_PH_OY] = "oy",
    [SPEECH_PH_P] = "p",   [SPEECH_PH_B] = "b",   [SPEECH_PH_T] = "t",   [SPEECH_PH_D] = "d",
    [SPEECH_PH_K] = "k",   [SPEECH_PH_G] = "g",   [SPEECH_PH_CH] = "ch", [SPEECH_PH_JH] = "jh",
    [SPEECH_PH_F] = "f",   [SPEECH_PH_V] = "v",   [SPEECH_PH_TH] = "th", [SPEECH_PH_DH] = "dh",
    [SPEECH_PH_S] = "s",   [SPEECH_PH_Z] = "z",   [SPEECH_PH_SH] = "sh", [SPEECH_PH_ZH] = "zh",
    [SPEECH_PH_HH] = "hh", [SPEECH_PH_M] = "m",   [SPEECH_PH_N] = "n",   [SPEECH_PH_NG] = "ng",
    [SPEECH_PH_L] = "l",   [SPEECH_PH_R] = "r",   [SPEECH_PH_W] = "w",   [SPEECH_PH_Y] = "y",
    [SPEECH_PH_PAUSE] = "_",
};

int speech_phoneme_from_name(const char *name)
{
    for (int i = 0; i < SPEECH_PH_COUNT; i++)
    {
        if (strcasecmp(name, speech_phoneme_names[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

//==========================================================================
// Resonators
//==========================================================================

// Formant bandwidths, Hz. Not vowel-specific data in say-design.md §6
// either; these are the values textbook Klatt-style synthesizers use
// (narrower at F1, widening up the spectrum). Noise-excited phonemes get the
// wider set: frication is a band of noise, not a whistle, and a 60 Hz
// bandwidth on a noise source makes `s` ring like a tuning fork.
static const float bw_voiced[3] = {60.0f, 90.0f, 120.0f};
static const float bw_noisy[3] = {180.0f, 250.0f, 350.0f};

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

//==========================================================================
// The synthesizer
//==========================================================================

// Transition lengths, ms, indexed by a row's transition class (§8.3).
// Phonemes are 50-150 ms and these are 6-17 parameter blocks, which is
// smooth at the §8.3 block cadence.
static const uint8_t transition_ms[4] = {0, 20, 40, 60};

// A stop's closure: the silence in front of the burst. It is not decoration
// -- the silence is the cue that distinguishes a stop from everything else,
// so it has to be silence and not a fade. The tract shuts in SPEECH_CUT_MS
// (one block or two, enough that the closing itself does not click) and the
// rest of the closure is nothing at all.
#define SPEECH_CLOSURE_MS 40
#define SPEECH_CUT_MS 8

// Source gains. The impulse train puts one full-scale sample into the
// resonators per pitch period; the noise source puts one into every sample,
// so it needs far less gain to reach the same loudness. They are set
// against each other by measurement, not by theory: rendering all 41 rows
// and reading their RMS is the only way to find the point where a vowel is
// the loudest thing in a word, which is what it is in speech and what a
// naive equal-gain pairing gets backwards by about 10 dB.
//
// There is no per-utterance normalisation: on the board this is one
// fixed-gain source in the mixer (§8.5), so the level a word comes out at
// here is the level it comes out at there.
#define SPEECH_VOICED_GAIN 40000.0f
#define SPEECH_NOISE_GAIN 500.0f

// A stop's release is a transient, not sustained frication, and is louder
// than a fricative for its instant -- so the same noise source is driven
// harder for it. This is §8.1's "amplitude envelope on the noise source".
#define SPEECH_BURST_LEVEL 4.0f

// What "a voiced fricative runs both" (§8.1) actually means: the voicing
// under an obstruent is a voice bar, a weak buzz behind the constriction,
// not the full glottal excitation a vowel gets. At full level `z` is louder
// than any vowel and clips -- which it never is in speech.
#define SPEECH_VOICEBAR_LEVEL 0.30f

// One interpolation target: everything that changes on a block boundary.
typedef struct SpeechParams
{
    float f1, f2, f3;
    float a1, a2, a3; // 0..1
    float voiced;     // impulse-train source level, 0..1
    float noise;      // noise source level, 0..1
    bool noisy;       // pick the wider bandwidth set
} SpeechParams;

typedef struct SpeechState
{
    SpeechResonator r1, r2, r3;
    float sample_rate;
    float pitch_hz;
    float phase;   // impulse-train phase, in samples
    uint16_t lfsr; // noise shift register, the one the PSG noise voices use
    int16_t *out;
    int written;
    int capacity;
} SpeechState;

static float amp_of(uint8_t a)
{
    return (float)a / 255.0f;
}

static SpeechParams params_of(const SpeechPhoneme *p, float stress_scale)
{
    SpeechParams s;
    s.f1 = (float)p->f1;
    s.f2 = (float)p->f2;
    s.f3 = (float)p->f3;
    s.a1 = amp_of(p->a1) * stress_scale;
    s.a2 = amp_of(p->a2) * stress_scale;
    s.a3 = amp_of(p->a3) * stress_scale;
    bool obstruent = (p->flags & (SPEECH_F_STOP | SPEECH_F_FRICATIVE)) != 0;
    s.voiced = (p->flags & SPEECH_F_VOICED) ? (obstruent ? SPEECH_VOICEBAR_LEVEL : 1.0f) : 0.0f;
    // An affricate is a stop *and* a fricative, and its noise sustains, so
    // it takes the fricative level rather than the burst's.
    if (p->flags & SPEECH_F_FRICATIVE)
    {
        s.noise = 1.0f;
    }
    else if (p->flags & SPEECH_F_STOP)
    {
        s.noise = SPEECH_BURST_LEVEL;
    }
    else
    {
        s.noise = 0.0f;
    }
    s.noisy = (s.noise > 0.0f);
    return s;
}

// The pause row at zero amplitude: what an utterance starts and ends on, and
// what a stop's closure holds.
static SpeechParams params_silent(void)
{
    SpeechParams s = params_of(&speech_phoneme_table[SPEECH_PH_PAUSE], 1.0f);
    s.voiced = 0.0f;
    s.noise = 0.0f;
    return s;
}

static SpeechParams params_lerp(const SpeechParams *a, const SpeechParams *b, float t)
{
    SpeechParams s;
    s.f1 = a->f1 + (b->f1 - a->f1) * t;
    s.f2 = a->f2 + (b->f2 - a->f2) * t;
    s.f3 = a->f3 + (b->f3 - a->f3) * t;
    s.a1 = a->a1 + (b->a1 - a->a1) * t;
    s.a2 = a->a2 + (b->a2 - a->a2) * t;
    s.a3 = a->a3 + (b->a3 - a->a3) * t;
    s.voiced = a->voiced + (b->voiced - a->voiced) * t;
    s.noise = a->noise + (b->noise - a->noise) * t;
    s.noisy = b->noisy;
    return s;
}

static int ms_to_samples(int ms, float sample_rate)
{
    return (int)((float)ms * sample_rate / 1000.0f);
}

// Render `count` samples while the parameters travel linearly from `from` to
// `to`, re-deriving the resonator coefficients once per block (§8.3) and
// holding them flat across it. Stops early, and silently, when the caller's
// buffer is full -- an utterance longer than the buffer is truncated, not
// wrapped.
static void render_segment(SpeechState *st, const SpeechParams *from, const SpeechParams *to, int count)
{
    for (int done = 0; done < count; done += SPEECH_BLOCK_FRAMES)
    {
        int block = count - done;
        if (block > SPEECH_BLOCK_FRAMES)
        {
            block = SPEECH_BLOCK_FRAMES;
        }
        if (st->written + block > st->capacity)
        {
            block = st->capacity - st->written;
        }
        if (block <= 0)
        {
            return;
        }

        // Parameters are those at the middle of the block, so a ramp is
        // centred on its nominal path rather than lagging it by a block.
        float t = (count > 1) ? ((float)done + (float)block * 0.5f) / (float)count : 1.0f;
        SpeechParams p = params_lerp(from, to, t);

        const float *bw = p.noisy ? bw_noisy : bw_voiced;
        speech_resonator_tune(&st->r1, p.f1, bw[0], st->sample_rate);
        speech_resonator_tune(&st->r2, p.f2, bw[1], st->sample_rate);
        speech_resonator_tune(&st->r3, p.f3, bw[2], st->sample_rate);

        float period = st->sample_rate / st->pitch_hz;
        for (int i = 0; i < block; i++)
        {
            // Voiced source: an impulse train at the fundamental. A discrete
            // impulse has energy at every harmonic up to Nyquist, which is
            // §8.1's "shaped so it has energy across the formant range".
            float voiced = 0.0f;
            st->phase += 1.0f;
            if (st->phase >= period)
            {
                st->phase -= period;
                voiced = SPEECH_VOICED_GAIN;
            }

            // Unvoiced source: the same 16-bit LFSR the PSG noise voices use
            // (sound-design.md §4), clocked every sample so it is white
            // across the whole band the formants sit in.
            uint16_t fb = (uint16_t)((st->lfsr ^ (st->lfsr >> 2) ^ (st->lfsr >> 3) ^ (st->lfsr >> 5)) & 1u);
            st->lfsr = (uint16_t)((st->lfsr >> 1) | (fb << 15));
            float noise = (st->lfsr & 1u) ? SPEECH_NOISE_GAIN : -SPEECH_NOISE_GAIN;

            float source = voiced * p.voiced + noise * p.noise;

            // Alternating sign on the parallel branches (Klatt's convention):
            // each two-pole resonator also shifts phase, and without the
            // alternation adjacent branches partially cancel near their
            // shared boundary and shift the composite peak onto the wrong
            // harmonic -- measured directly against the M0 Goertzel gate.
            float y = p.a1 * speech_resonator_step(&st->r1, source) -
                      p.a2 * speech_resonator_step(&st->r2, source) +
                      p.a3 * speech_resonator_step(&st->r3, source);

            if (y > 32767.0f)
            {
                y = 32767.0f;
            }
            else if (y < -32768.0f)
            {
                y = -32768.0f;
            }
            st->out[st->written++] = (int16_t)y;
        }
    }
}

static void state_init(SpeechState *st, float pitch_hz, float sample_rate_hz, int16_t *out, int max_samples)
{
    SpeechResonator zero = {0};
    st->r1 = zero;
    st->r2 = zero;
    st->r3 = zero;
    st->sample_rate = sample_rate_hz;
    st->pitch_hz = pitch_hz;
    st->phase = 0.0f;
    st->lfsr = 0xACE1u; // the PSG's noise seed
    st->out = out;
    st->written = 0;
    st->capacity = max_samples;
}

// Stress 0..3 scales amplitude and duration around an unmarked phoneme
// (stress 1), which is what a SpeechFrame's `stress` field means (§10).
static const float stress_amp[4] = {0.70f, 1.0f, 1.15f, 1.30f};
static const float stress_dur[4] = {0.80f, 1.0f, 1.20f, 1.40f};

int speech_render(const SpeechFrame *frames, int n, float pitch_hz, float sample_rate_hz,
                  int16_t *out, int max_samples)
{
    SpeechState st;
    state_init(&st, pitch_hz, sample_rate_hz, out, max_samples);

    SpeechParams silence = params_silent();
    SpeechParams prev = silence;
    int prev_class = 0;

    for (int i = 0; i < n && st.written < st.capacity; i++)
    {
        int id = frames[i].phoneme;
        if (id >= SPEECH_PH_COUNT)
        {
            continue; // an id we have no row for is skipped, not sounded
        }
        const SpeechPhoneme *p = &speech_phoneme_table[id];
        int stress = frames[i].stress & 3;

        st.pitch_hz = frames[i].pitch ? (float)frames[i].pitch * 2.0f : pitch_hz;

        SpeechParams tgt = params_of(p, stress_amp[stress]);
        int dur_ms = frames[i].dur_ms ? frames[i].dur_ms : p->dur_ms;
        int dur = ms_to_samples((int)((float)dur_ms * stress_dur[stress]), sample_rate_hz);

        // A stop closes first. The closure resets what the transition comes
        // from, which is why a vowel into a stop sounds nothing like a vowel
        // into a nasal even though both interpolate the same way.
        if (p->flags & SPEECH_F_STOP)
        {
            render_segment(&st, &prev, &silence, ms_to_samples(SPEECH_CUT_MS, sample_rate_hz));
            render_segment(&st, &silence, &silence, ms_to_samples(SPEECH_CLOSURE_MS - SPEECH_CUT_MS, sample_rate_hz));
            prev = silence;
        }

        // The transition takes the shorter of the two classes: the more
        // constrained phoneme of the pair decides how fast the tract can
        // move between them.
        int cls = (p->flags & SPEECH_F_TRANS_MASK) >> SPEECH_F_TRANS_SHIFT;
        int trans_ms = transition_ms[cls < prev_class ? cls : prev_class];
        int trans = ms_to_samples(trans_ms, sample_rate_hz);
        if (trans > dur)
        {
            trans = dur;
        }
        render_segment(&st, &prev, &tgt, trans);

        // Then the body of the phoneme: a glide for a diphthong, a decay for
        // a stop's burst, a hold for everything else.
        SpeechParams end = tgt;
        if (p->glide_to != SPEECH_PH_NONE)
        {
            end = params_of(&speech_phoneme_table[p->glide_to], stress_amp[stress]);
        }
        else if ((p->flags & SPEECH_F_STOP) && !(p->flags & SPEECH_F_FRICATIVE))
        {
            end.a1 = end.a2 = end.a3 = 0.0f; // a burst decays; frication does not
        }
        render_segment(&st, &tgt, &end, dur - trans);

        prev = end;
        prev_class = cls;
    }

    // Fade the tail out over one transition rather than cutting it, so an
    // utterance ends without a click.
    render_segment(&st, &prev, &silence, ms_to_samples(transition_ms[1], sample_rate_hz));
    return st.written;
}

void speech_render_sustained(SpeechPhonemeId id, float pitch_hz, float sample_rate_hz,
                             int16_t *out, int sample_count)
{
    SpeechState st;
    state_init(&st, pitch_hz, sample_rate_hz, out, sample_count);
    SpeechParams p = params_of(&speech_phoneme_table[id], 1.0f);
    render_segment(&st, &p, &p, sample_count);
}
