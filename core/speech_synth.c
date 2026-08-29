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

// Both of the above are the levels **at SPEECH_SAMPLE_RATE_HZ**, and neither
// survives a change of sample rate on its own (B52). The resonator is
// normalised to unit gain at DC, so its input coefficient `a = 1 - b - c`
// falls as 1/fs^2 -- 0.01563 at 36.6 kHz against 0.00392 at 73.2 -- and the
// same excitation deposits a quarter as much at twice the rate. Since
// `hw.setcpu "fast` doubles the mix rate, that is a voice that loses 6 dB
// the moment a program overclocks, which is what the board reported.
//
// The two sources need different compensation because they are different
// kinds of signal, and both of these are measured against the ratios above
// rather than taken on faith:
//
//   * The impulse train fires at a rate fixed in Hz, so what matters is the
//     energy per impulse: scale it with fs, and the measured loudness comes
//     back to within a percent across a 4x span of rates.
//   * The noise source fires every sample, so doubling the rate spreads the
//     same per-sample variance over twice the bandwidth and a fixed-Hz
//     resonator sees half the power: scale it with the square root of fs.
//     Measurement puts the true exponent nearer 0.59 than 0.5, so this is
//     right to about 6 % rather than exactly -- worth a bound in the test
//     and not worth a fudge factor in the code.
//
// At the stock rate both factors are exactly 1, so every .wav the M0 and M1
// listening gates were judged on is unchanged.
static void source_gains_for_rate(float sample_rate, float *voiced, float *noise)
{
    float ratio = sample_rate / (float)SPEECH_SAMPLE_RATE_HZ;
    *voiced = SPEECH_VOICED_GAIN * ratio;
    *noise = SPEECH_NOISE_GAIN * sqrtf(ratio);
}

// A stop's release is a transient, not sustained frication, and is louder
// than a fricative for its instant -- so the same noise source is driven
// harder for it. This is §8.1's "amplitude envelope on the noise source".
#define SPEECH_BURST_LEVEL 4.0f

// What "a voiced fricative runs both" (§8.1) actually means: the voicing
// under an obstruent is a voice bar, a weak buzz behind the constriction,
// not the full glottal excitation a vowel gets. At full level `z` is louder
// than any vowel and clips -- which it never is in speech.
#define SPEECH_VOICEBAR_LEVEL 0.30f

static float amp_of(uint8_t a)
{
    return (float)a / 255.0f;
}

static float knob(uint8_t v)
{
    return (float)v / (float)SPEECH_VOICE_NOMINAL;
}

// The §6 table's parameters for one phoneme, with the stress scale and the
// §5.5 voice knobs applied. `mouth` and `throat` scale the formants because
// that is what the two cavities do -- F1 is the back cavity's resonance and
// F2/F3 are the front's -- so one knob each is not a simplification, it is
// the acoustics.
static SpeechParams params_of(const SpeechPhoneme *p, float stress_scale, const SpeechVoice *voice)
{
    SpeechParams s;
    s.f1 = (float)p->f1 * knob(voice->throat);
    s.f2 = (float)p->f2 * knob(voice->mouth);
    s.f3 = (float)p->f3 * knob(voice->mouth);
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
static SpeechParams params_silent(const SpeechVoice *voice)
{
    SpeechParams s = params_of(&speech_phoneme_table[SPEECH_PH_PAUSE], 1.0f, voice);
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

// Stress 0..3 scales amplitude and duration around an unmarked phoneme
// (stress 1), which is what a SpeechFrame's `stress` field means (§10).
static const float stress_amp[4] = {0.70f, 1.0f, 1.15f, 1.30f};
static const float stress_dur[4] = {0.80f, 1.0f, 1.20f, 1.40f};

//==========================================================================
// The engine
//==========================================================================
//
// A phoneme is two to four segments -- optionally a closure, then a
// transition, then a body -- and a segment is a straight line between two
// SpeechParams. The engine is that walk made resumable: it knows which
// segment it is on and how far into it, so it can be asked for 128 samples
// at a time by a refill IRQ, or for a whole utterance at once by a test.
//
// Coefficients are re-derived every SPEECH_BLOCK_FRAMES samples and at every
// segment start, and never in between (§8.3). Because that cadence is the
// engine's own and not the caller's, the samples that come out are the same
// whatever size the caller asks in -- which is the property that lets the
// .wav gates keep judging what the board will play.

// The ring holds SPEECH_QUEUE_LEN usable slots; one more is reserved so
// "full" and "empty" are distinguishable, as the PSG sequencer's ring is.
#define SPEECH_QRING (SPEECH_QUEUE_LEN + 1)

// A phoneme is at most four segments, so this many advances always either
// reaches a segment with samples in it or runs the queue dry.
#define SPEECH_ADVANCE_GUARD 8

static const SpeechVoice speech_voice_default = {
    SPEECH_VOICE_PITCH_DEFAULT, SPEECH_VOICE_NOMINAL, SPEECH_VOICE_NOMINAL, SPEECH_VOICE_NOMINAL};

static bool queue_pop(SpeechEngine *e, SpeechFrame *out)
{
    if (e->head == e->tail)
    {
        return false;
    }
    *out = e->q[e->head];
    e->head = (uint16_t)((e->head + 1) % SPEECH_QRING);
    return true;
}

static void begin_segment(SpeechEngine *e, const SpeechParams *from, const SpeechParams *to,
                          int len, SpeechStage stage)
{
    e->seg_from = *from;
    e->seg_to = *to;
    e->seg_len = len < 0 ? 0 : len;
    e->seg_done = 0;
    e->stage = (uint8_t)stage;
    e->block_left = 0; // a new segment always retunes
}

// Where the parameters are right now, mid-segment: what an interruption
// (`stopsound`) has to fade from if it is not to click.
static SpeechParams params_now(const SpeechEngine *e)
{
    float t = (e->seg_len > 0) ? (float)e->seg_done / (float)e->seg_len : 1.0f;
    return params_lerp(&e->seg_from, &e->seg_to, t);
}

// Pop the next phoneme and lay out its segments. With the queue dry this
// starts the tail fade instead, or does nothing if the engine is already
// idle.
static void engine_start_phoneme(SpeechEngine *e)
{
    SpeechFrame f;
    const SpeechPhoneme *p = NULL;
    while (queue_pop(e, &f))
    {
        if (f.phoneme < SPEECH_PH_COUNT)
        {
            p = &speech_phoneme_table[f.phoneme];
            break; // an id we have no row for is skipped, not sounded
        }
    }

    if (!p)
    {
        if (e->stage == SPEECH_STAGE_IDLE)
        {
            e->seg_len = e->seg_done = 0;
            return;
        }
        // Fade the tail out over one transition rather than cutting it, so
        // an utterance ends without a click. A `say` that arrives during the
        // fade waits for it -- 20 ms, which is a word boundary, not a gap.
        SpeechParams sil = params_silent(&e->voice);
        begin_segment(e, &e->prev, &sil, ms_to_samples(transition_ms[1], e->sample_rate),
                      SPEECH_STAGE_FADE);
        return;
    }

    int stress = f.stress & 3;
    e->pitch_hz = f.pitch ? (float)f.pitch * 2.0f : e->base_pitch_hz;

    // `speed` scales the transitions with the phonemes: speaking faster is
    // moving the tract faster, not holding vowels briefly and gliding
    // between them at leisure.
    float speed = knob(e->voice.speed);
    float dur_scale = stress_dur[stress] / (speed > 0.0f ? speed : 1.0f);

    e->tgt = params_of(p, stress_amp[stress], &e->voice);
    int dur_ms = f.dur_ms ? f.dur_ms : p->dur_ms;
    e->dur = ms_to_samples((int)((float)dur_ms * dur_scale), e->sample_rate);
    if (e->dur < 1)
    {
        e->dur = 1;
    }

    // The transition takes the shorter of the two classes: the more
    // constrained phoneme of the pair decides how fast the tract can move
    // between them.
    e->cls = (p->flags & SPEECH_F_TRANS_MASK) >> SPEECH_F_TRANS_SHIFT;
    int trans_ms = transition_ms[e->cls < e->prev_class ? e->cls : e->prev_class];
    e->trans = ms_to_samples((int)((float)trans_ms * dur_scale), e->sample_rate);
    if (e->trans > e->dur)
    {
        e->trans = e->dur;
    }

    // The body of the phoneme: a glide for a diphthong, a decay for a stop's
    // burst, a hold for everything else.
    e->end = e->tgt;
    if (p->glide_to != SPEECH_PH_NONE)
    {
        e->end = params_of(&speech_phoneme_table[p->glide_to], stress_amp[stress], &e->voice);
    }
    else if ((p->flags & SPEECH_F_STOP) && !(p->flags & SPEECH_F_FRICATIVE))
    {
        e->end.a1 = e->end.a2 = e->end.a3 = 0.0f; // a burst decays; frication does not
    }

    // A stop closes first. The closure resets what the transition comes
    // from, which is why a vowel into a stop sounds nothing like a vowel
    // into a nasal even though both interpolate the same way.
    if (p->flags & SPEECH_F_STOP)
    {
        SpeechParams sil = params_silent(&e->voice);
        begin_segment(e, &e->prev, &sil, ms_to_samples(SPEECH_CUT_MS, e->sample_rate),
                      SPEECH_STAGE_CUT);
    }
    else
    {
        begin_segment(e, &e->prev, &e->tgt, e->trans, SPEECH_STAGE_TRANSITION);
    }
}

// The segment that just finished decides the next one.
static void engine_advance(SpeechEngine *e)
{
    switch ((SpeechStage)e->stage)
    {
    case SPEECH_STAGE_CUT:
        // The rest of the closure is silence, and it has to be silence
        // rather than a fade: the silence is the cue that says "stop".
        begin_segment(e, &e->seg_to, &e->seg_to,
                      ms_to_samples(SPEECH_CLOSURE_MS - SPEECH_CUT_MS, e->sample_rate),
                      SPEECH_STAGE_CLOSURE);
        return;

    case SPEECH_STAGE_CLOSURE:
        e->prev = params_silent(&e->voice);
        begin_segment(e, &e->prev, &e->tgt, e->trans, SPEECH_STAGE_TRANSITION);
        return;

    case SPEECH_STAGE_TRANSITION:
        begin_segment(e, &e->tgt, &e->end, e->dur - e->trans, SPEECH_STAGE_BODY);
        return;

    case SPEECH_STAGE_BODY:
        e->prev = e->end;
        e->prev_class = e->cls;
        engine_start_phoneme(e);
        return;

    case SPEECH_STAGE_FADE:
        e->stage = SPEECH_STAGE_IDLE;
        e->seg_len = e->seg_done = 0;
        e->prev = params_silent(&e->voice);
        e->prev_class = 0;
        return;

    case SPEECH_STAGE_IDLE:
    default:
        engine_start_phoneme(e);
        return;
    }
}

void speech_engine_init(SpeechEngine *e, float sample_rate_hz)
{
    SpeechResonator zero = {0, 0, 0, 0, 0};
    e->r1 = zero;
    e->r2 = zero;
    e->r3 = zero;
    e->sample_rate = sample_rate_hz;
    source_gains_for_rate(sample_rate_hz, &e->voiced_gain, &e->noise_gain);
    e->voice = speech_voice_default;
    e->base_pitch_hz = (float)e->voice.pitch * 2.0f;
    e->pitch_hz = e->base_pitch_hz;
    e->phase = 0.0f;
    e->lfsr = 0xACE1u; // the PSG's noise seed
    e->stage = SPEECH_STAGE_IDLE;
    e->seg_len = e->seg_done = 0;
    e->block_left = 0;
    e->dur = e->trans = 0;
    e->cls = e->prev_class = 0;
    e->prev = e->tgt = e->end = e->seg_from = e->seg_to = e->cur = params_silent(&e->voice);
    e->head = e->tail = 0;
}

void speech_engine_set_rate(SpeechEngine *e, float sample_rate_hz)
{
    // Everything in flight was generated at the old rate and every
    // coefficient is a function of the new one, so the utterance is
    // abandoned rather than left to slide -- which is exactly what
    // sound_reclock does to the voices, and for the same reason (§8.4).
    SpeechVoice v = e->voice;
    speech_engine_init(e, sample_rate_hz);
    e->voice = v;
    e->base_pitch_hz = (float)v.pitch * 2.0f;
    e->pitch_hz = e->base_pitch_hz;
}

void speech_engine_set_voice(SpeechEngine *e, const SpeechVoice *v)
{
    e->voice = *v;
    if (e->voice.speed == 0)
    {
        e->voice.speed = 1; // a zero here would divide the duration scale
    }
    e->base_pitch_hz = (float)(e->voice.pitch ? e->voice.pitch : SPEECH_VOICE_PITCH_DEFAULT) * 2.0f;
    if (e->stage == SPEECH_STAGE_IDLE)
    {
        e->pitch_hz = e->base_pitch_hz;
    }
}

SpeechVoice speech_engine_get_voice(const SpeechEngine *e)
{
    return e->voice;
}

int speech_engine_free_slots(const SpeechEngine *e)
{
    return (int)((uint16_t)(e->head - e->tail - 1 + SPEECH_QRING) % SPEECH_QRING);
}

int speech_engine_queue(SpeechEngine *e, const SpeechFrame *frames, int n)
{
    if (!frames || n <= 0)
    {
        return 0;
    }
    int accepted = 0;
    for (int i = 0; i < n; i++)
    {
        if (speech_engine_free_slots(e) == 0)
        {
            break;
        }
        e->q[e->tail] = frames[i];
        e->tail = (uint16_t)((e->tail + 1) % SPEECH_QRING);
        accepted++;
    }
    return accepted;
}

bool speech_engine_busy(const SpeechEngine *e)
{
    return e->stage != SPEECH_STAGE_IDLE || e->head != e->tail;
}

void speech_engine_stop(SpeechEngine *e)
{
    e->head = e->tail = 0;
    if (e->stage == SPEECH_STAGE_IDLE || e->stage == SPEECH_STAGE_FADE)
    {
        return;
    }
    SpeechParams now = params_now(e);
    SpeechParams sil = params_silent(&e->voice);
    begin_segment(e, &now, &sil, ms_to_samples(transition_ms[1], e->sample_rate),
                  SPEECH_STAGE_FADE);
}

void speech_engine_render(SpeechEngine *e, int16_t *out, int count)
{
    int done = 0;

    while (done < count)
    {
        for (int guard = 0; e->seg_done >= e->seg_len && guard < SPEECH_ADVANCE_GUARD; guard++)
        {
            engine_advance(e);
            if (e->stage == SPEECH_STAGE_IDLE)
            {
                break;
            }
        }
        if (e->seg_done >= e->seg_len)
        {
            break; // idle (or nothing the guard could reach): silence from here
        }

        // A chunk runs to whichever comes first: the end of the segment, the
        // end of the parameter block, or the end of what was asked for.
        if (e->block_left <= 0)
        {
            // Parameters at the middle of the block, so a ramp is centred on
            // its nominal path rather than lagging it by a block.
            int span = e->seg_len - e->seg_done;
            if (span > SPEECH_BLOCK_FRAMES)
            {
                span = SPEECH_BLOCK_FRAMES;
            }
            float t = (e->seg_len > 1)
                          ? ((float)e->seg_done + (float)span * 0.5f) / (float)e->seg_len
                          : 1.0f;
            e->cur = params_lerp(&e->seg_from, &e->seg_to, t);
            const float *bw = e->cur.noisy ? bw_noisy : bw_voiced;
            speech_resonator_tune(&e->r1, e->cur.f1, bw[0], e->sample_rate);
            speech_resonator_tune(&e->r2, e->cur.f2, bw[1], e->sample_rate);
            speech_resonator_tune(&e->r3, e->cur.f3, bw[2], e->sample_rate);
            e->block_left = SPEECH_BLOCK_FRAMES;
        }

        int chunk = e->seg_len - e->seg_done;
        if (chunk > e->block_left)
        {
            chunk = e->block_left;
        }
        if (chunk > count - done)
        {
            chunk = count - done;
        }

        const SpeechParams *p = &e->cur;
        float period = e->sample_rate / e->pitch_hz;
        for (int i = 0; i < chunk; i++)
        {
            // Voiced source: an impulse train at the fundamental. A discrete
            // impulse has energy at every harmonic up to Nyquist, which is
            // §8.1's "shaped so it has energy across the formant range".
            float voiced = 0.0f;
            e->phase += 1.0f;
            if (e->phase >= period)
            {
                e->phase -= period;
                voiced = e->voiced_gain;
            }

            // Unvoiced source: the same 16-bit LFSR the PSG noise voices use
            // (sound-design.md §4), clocked every sample so it is white
            // across the whole band the formants sit in.
            uint16_t fb = (uint16_t)((e->lfsr ^ (e->lfsr >> 2) ^ (e->lfsr >> 3) ^ (e->lfsr >> 5)) & 1u);
            e->lfsr = (uint16_t)((e->lfsr >> 1) | (fb << 15));
            float noise = (e->lfsr & 1u) ? e->noise_gain : -e->noise_gain;

            float source = voiced * p->voiced + noise * p->noise;

            // Alternating sign on the parallel branches (Klatt's convention):
            // each two-pole resonator also shifts phase, and without the
            // alternation adjacent branches partially cancel near their
            // shared boundary and shift the composite peak onto the wrong
            // harmonic -- measured directly against the M0 Goertzel gate.
            float y = p->a1 * speech_resonator_step(&e->r1, source) -
                      p->a2 * speech_resonator_step(&e->r2, source) +
                      p->a3 * speech_resonator_step(&e->r3, source);

            if (y > 32767.0f)
            {
                y = 32767.0f;
            }
            else if (y < -32768.0f)
            {
                y = -32768.0f;
            }
            out[done++] = (int16_t)y;
        }
        e->seg_done += chunk;
        e->block_left -= chunk;
    }

    while (done < count)
    {
        out[done++] = 0;
    }
}

//==========================================================================
// Offline rendering: the engine, run to completion
//==========================================================================

int speech_render(const SpeechFrame *frames, int n, float pitch_hz, float sample_rate_hz,
                  int16_t *out, int max_samples)
{
    // ~900 B of stack, which is why this is the host's entry point and not
    // the board's: the board drives the same engine from .bss (§9.1).
    SpeechEngine e;
    speech_engine_init(&e, sample_rate_hz);
    SpeechVoice v = speech_voice_default;
    v.pitch = (uint8_t)(pitch_hz / 2.0f + 0.5f);
    speech_engine_set_voice(&e, &v);

    int written = 0;
    int queued = 0;
    while (written < max_samples)
    {
        queued += speech_engine_queue(&e, frames + queued, n - queued);
        if (!speech_engine_busy(&e))
        {
            break;
        }
        int want = max_samples - written;
        if (want > SPEECH_BLOCK_FRAMES)
        {
            want = SPEECH_BLOCK_FRAMES;
        }
        speech_engine_render(&e, out + written, want);
        written += want;
    }
    return written;
}

void speech_render_sustained(SpeechPhonemeId id, float pitch_hz, float sample_rate_hz,
                             int16_t *out, int sample_count)
{
    SpeechEngine e;
    speech_engine_init(&e, sample_rate_hz);
    SpeechVoice v = speech_voice_default;
    v.pitch = (uint8_t)(pitch_hz / 2.0f + 0.5f);
    speech_engine_set_voice(&e, &v);

    // One segment, the phoneme held flat: no transition in and none out,
    // which is what the §10 formant assertion needs to measure.
    SpeechParams p = params_of(&speech_phoneme_table[id], 1.0f, &e.voice);
    begin_segment(&e, &p, &p, sample_count, SPEECH_STAGE_BODY);
    speech_engine_render(&e, out, sample_count);
}
