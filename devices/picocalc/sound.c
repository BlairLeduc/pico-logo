//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  PicoCalc software PSG synthesizer (P8). See sound.h and
//  docs/sound-design.md §6.
//
//  Output: PWM slice 5 wraps at 2048 -> ~73.2 kHz carrier (ultrasonic),
//  11-bit. One 32-bit compare word holds both channels (A = GPIO 26 left,
//  B = GPIO 27 right). Two DMA channels ping-pong through a two-half ring,
//  each paced by the slice's wrap DREQ and chained to the other so playback
//  never gaps. When a half drains, its channel's IRQ refills it -- mixing the
//  eight voices and advancing the note sequencer -- on core 0. Each mixed
//  frame is written OVERSAMPLE (2) times, giving a ~36.6 kHz mix rate.
//

#include "sound.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"

#include "core/limits.h"

#include <string.h>

#define AUDIO_LEFT_PIN 26
#define AUDIO_RIGHT_PIN 27

#define PWM_WRAP 2048 // 11-bit
#define PWM_MID 1024
#define PWM_PEAK 1023 // per-ear headroom before the 4-voice sum

#define OVERSAMPLE 2
#define HALF_SLOTS SOUND_RING_HALF                  // carrier samples per half
#define BLOCK_FRAMES (SOUND_RING_HALF / OVERSAMPLE) // mixed frames per half

// The whole ring (both halves) is a power-of-2, aligned buffer so the DMA can
// wrap the READ address in hardware. This makes the engine robust to IRQ
// starvation: flash program/erase (picocalc_flash.c) masks interrupts with
// XIP offline for tens of milliseconds, so the refill IRQ can be delayed past
// a half draining. Without a hardware wrap the DMA would march past the
// buffer and read garbage -- an audible burst of static. With the wrap it
// cleanly replays the ring instead (silence at idle, a brief stutter under a
// note). RING_WRAP_BITS = log2(2 * HALF_SLOTS * sizeof(uint32_t)).
#define RING_WRAP_BITS 11 // 2 * 256 * 4 = 2048 bytes

#define ENV_ONE (1 << 15) // full envelope amplitude (fixed point)

// Sequencer ring holds SOUND_QUEUE_LEN usable slots; one extra slot is
// reserved so "full" and "empty" are distinguishable.
#define QRING (SOUND_QUEUE_LEN + 1)

// Envelope stages.
enum
{
    ENV_IDLE = 0,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
};

typedef struct Voice
{
    // Timbre (set by the ops).
    uint8_t wave;       // SoundWave
    uint8_t duty;       // pulse duty %
    uint16_t attack_ms; // ADSR times
    uint16_t decay_ms;
    uint16_t release_ms;
    uint8_t sustain; // 0..15

    // Current-note runtime.
    uint32_t phase;     // tone phase accumulator
    uint32_t phase_inc; // per-mix-sample increment
    uint16_t lfsr;      // noise shift register
    uint8_t vol;        // 0..15
    bool is_rest;       // current gate is a rest

    // Envelope.
    uint8_t stage;
    int32_t env; // 0..ENV_ONE
    int32_t attack_step, decay_step, release_step, sustain_level;
    int32_t hold_us_left; // gated time remaining before release

    // Sequencer queue (SPSC: ops append at tail, IRQ pops at head).
    SoundEvent q[QRING];
    volatile uint16_t head, tail;
} Voice;

static Voice g_v[MAX_VOICES];

static uint g_slice;
static int g_dma_a = -1;
static int g_dma_b = -1;
// Aligned to the whole-ring size so the DMA read-address ring-wrap is valid.
static uint32_t g_ring[2][HALF_SLOTS] __attribute__((aligned(1 << RING_WRAP_BITS)));
static uint32_t g_mix_rate;  // Hz
static uint32_t g_block_us;  // microseconds of audio per refill block
static volatile bool g_ready; // engine initialised

// 2 dB-per-step volume ladder (TI/Atari), scaled to 0..256. Level 0 is off.
static const int32_t g_gain[16] = {
    0, 10, 13, 16, 20, 26, 32, 41, 51, 64, 81, 102, 128, 162, 203, 256};

static bool is_noise_voice(int v)
{
    return v == 3 || v == 7;
}

static uint16_t queue_free(const Voice *v)
{
    // One slot reserved to distinguish full from empty.
    return (uint16_t)((v->head - v->tail - 1 + QRING) % QRING);
}

static bool queue_empty(const Voice *v)
{
    return v->head == v->tail;
}

//==========================================================================
// Note start / envelope setup (called from ops and from the IRQ sequencer)
//==========================================================================

static int32_t steps_for(uint32_t ms)
{
    // Number of refill blocks a segment of `ms` spans (at least one).
    int64_t us = (int64_t)ms * 1000;
    int32_t blocks = (int32_t)(us / (int64_t)g_block_us);
    return blocks < 1 ? 1 : blocks;
}

static void start_note(Voice *v, uint32_t freq, uint32_t dur_ms, int vol)
{
    v->is_rest = (freq == 0);
    v->vol = (uint8_t)(vol < 0 ? 0 : (vol > 15 ? 15 : vol));
    v->phase = 0;
    v->phase_inc = (uint32_t)(((uint64_t)freq << 32) / g_mix_rate);
    if (v->lfsr == 0)
    {
        v->lfsr = 0xACE1u; // seed the noise LFSR
    }

    v->sustain_level = (int32_t)v->sustain * ENV_ONE / 15;
    v->attack_step = ENV_ONE / steps_for(v->attack_ms);
    v->decay_step = (ENV_ONE - v->sustain_level) / steps_for(v->decay_ms);
    v->release_step = ENV_ONE / steps_for(v->release_ms);
    if (v->attack_step < 1) v->attack_step = 1;
    if (v->decay_step < 1) v->decay_step = 1;
    if (v->release_step < 1) v->release_step = 1;

    v->env = 0;
    v->stage = ENV_ATTACK;
    v->hold_us_left = (int32_t)dur_ms * 1000;
}

//==========================================================================
// Mixer + sequencer (IRQ context, in RAM to survive flash writes)
//==========================================================================

// Advance one voice's envelope and note timing by one refill block; when the
// note fully releases, pop the next queued event.
static void __not_in_flash_func(voice_advance_block)(Voice *v)
{
    if (v->stage == ENV_IDLE)
    {
        if (!queue_empty(v))
        {
            SoundEvent e = v->q[v->head];
            v->head = (uint16_t)((v->head + 1) % QRING);
            start_note(v, e.freq_hz, e.dur_ms, e.vol);
        }
        return;
    }

    v->hold_us_left -= (int32_t)g_block_us;
    if (v->hold_us_left <= 0 && v->stage != ENV_RELEASE)
    {
        v->stage = ENV_RELEASE;
    }

    switch (v->stage)
    {
    case ENV_ATTACK:
        v->env += v->attack_step;
        if (v->env >= ENV_ONE)
        {
            v->env = ENV_ONE;
            v->stage = ENV_DECAY;
        }
        break;
    case ENV_DECAY:
        v->env -= v->decay_step;
        if (v->env <= v->sustain_level)
        {
            v->env = v->sustain_level;
            v->stage = ENV_SUSTAIN;
        }
        break;
    case ENV_SUSTAIN:
        break;
    case ENV_RELEASE:
        v->env -= v->release_step;
        if (v->env <= 0)
        {
            v->env = 0;
            v->stage = ENV_IDLE;
        }
        break;
    default:
        break;
    }
}

// One raw oscillator sample in [-PWM_PEAK, PWM_PEAK] for the voice's current
// phase; advances phase/LFSR by one mix sample.
static int32_t __not_in_flash_func(voice_sample)(Voice *v)
{
    uint32_t prev = v->phase;
    v->phase += v->phase_inc;

    switch (v->wave)
    {
    case SOUND_WAVE_SQUARE:
        return (v->phase & 0x80000000u) ? PWM_PEAK : -PWM_PEAK;
    case SOUND_WAVE_PULSE:
    {
        uint32_t thresh = (uint32_t)v->duty * 256u / 100u; // 0..255
        return ((v->phase >> 24) < thresh) ? PWM_PEAK : -PWM_PEAK;
    }
    case SOUND_WAVE_SAWTOOTH:
        return (int32_t)((int32_t)v->phase >> 21); // -1024..1023
    case SOUND_WAVE_TRIANGLE:
    {
        int32_t s = (int32_t)(v->phase >> 20); // 0..4095
        if (s >= 2048)
        {
            s = 4095 - s;
        }
        return s - 1024; // -1024..1023
    }
    case SOUND_WAVE_WHITE:
    case SOUND_WAVE_PERIODIC:
    {
        // Clock the LFSR each time the phase wraps.
        if (v->phase < prev)
        {
            if (v->wave == SOUND_WAVE_WHITE)
            {
                uint16_t fb = (uint16_t)((v->lfsr ^ (v->lfsr >> 2) ^ (v->lfsr >> 3) ^ (v->lfsr >> 5)) & 1u);
                v->lfsr = (uint16_t)((v->lfsr >> 1) | (fb << 15));
            }
            else
            {
                // Periodic: rotate a single bit -> a buzzy pitched rasp.
                v->lfsr = (uint16_t)((v->lfsr >> 1) | ((v->lfsr & 1u) << 15));
            }
        }
        return (v->lfsr & 1u) ? PWM_PEAK : -PWM_PEAK;
    }
    default:
        return 0;
    }
}

// Refill one ring half: BLOCK_FRAMES mixed stereo frames, each written
// OVERSAMPLE times.
static void __not_in_flash_func(mix_half)(uint32_t *half)
{
    // Advance envelopes/sequencer once per block (per design granularity).
    for (int i = 0; i < MAX_VOICES; i++)
    {
        voice_advance_block(&g_v[i]);
    }

    uint32_t *out = half;
    for (int f = 0; f < BLOCK_FRAMES; f++)
    {
        int32_t left = 0, right = 0;
        for (int i = 0; i < MAX_VOICES; i++)
        {
            Voice *v = &g_v[i];
            if (v->stage == ENV_IDLE || v->is_rest || v->vol == 0)
            {
                continue;
            }
            int32_t s = voice_sample(v);
            s = (s * v->env) >> 15;          // envelope
            s = (s * g_gain[v->vol]) >> 8;   // volume ladder
            if (i < 4)
            {
                left += s;
            }
            else
            {
                right += s;
            }
        }

        // 4 voices summed -> >>2 headroom, saturate to +-PWM_MID, bias.
        left >>= 2;
        right >>= 2;
        if (left > PWM_MID - 1) left = PWM_MID - 1;
        if (left < -PWM_MID) left = -PWM_MID;
        if (right > PWM_MID - 1) right = PWM_MID - 1;
        if (right < -PWM_MID) right = -PWM_MID;

        uint32_t la = (uint32_t)(left + PWM_MID);
        uint32_t ra = (uint32_t)(right + PWM_MID);
        uint32_t word = la | (ra << 16); // chan A (26) low, chan B (27) high

        for (int o = 0; o < OVERSAMPLE; o++)
        {
            *out++ = word;
        }
    }
}

static void __not_in_flash_func(dma_irq_handler)(void)
{
    // Re-arm the completed channel for its next chain trigger. Both the read
    // address (advanced to the end of its half) AND the transfer count
    // (decremented to 0) must be reset -- a chain trigger does not reload
    // them. Resetting only the address would leave count at 0, so the next
    // chain would transfer nothing and complete instantly, storming the IRQ.
    // Do not trigger here; the sibling channel's chain restarts us.
    if (dma_hw->ints0 & (1u << g_dma_a))
    {
        dma_hw->ints0 = 1u << g_dma_a;
        mix_half(g_ring[0]);
        dma_channel_set_read_addr(g_dma_a, g_ring[0], false);
        dma_channel_set_trans_count(g_dma_a, HALF_SLOTS, false);
    }
    if (dma_hw->ints0 & (1u << g_dma_b))
    {
        dma_hw->ints0 = 1u << g_dma_b;
        mix_half(g_ring[1]);
        dma_channel_set_read_addr(g_dma_b, g_ring[1], false);
        dma_channel_set_trans_count(g_dma_b, HALF_SLOTS, false);
    }
}

//==========================================================================
// Init
//==========================================================================

void sound_init(void)
{
    if (g_ready)
    {
        return;
    }

    memset(g_v, 0, sizeof(g_v));
    for (int i = 0; i < MAX_VOICES; i++)
    {
        g_v[i].wave = is_noise_voice(i) ? SOUND_WAVE_WHITE : SOUND_WAVE_SQUARE;
        g_v[i].duty = 50;
        g_v[i].attack_ms = 5;
        g_v[i].decay_ms = 0;
        g_v[i].sustain = 15;
        g_v[i].release_ms = 30;
        g_v[i].lfsr = 0xACE1u;
        g_v[i].stage = ENV_IDLE;
    }

    g_mix_rate = clock_get_hz(clk_sys) / PWM_WRAP / OVERSAMPLE;
    g_block_us = (uint32_t)((uint64_t)BLOCK_FRAMES * 1000000u / g_mix_rate);

    // Pre-fill both halves with silence (mid level).
    for (int h = 0; h < 2; h++)
    {
        for (int i = 0; i < HALF_SLOTS; i++)
        {
            g_ring[h][i] = (uint32_t)PWM_MID | ((uint32_t)PWM_MID << 16);
        }
    }

    // PWM: both ears on one slice, wrapping to make the carrier.
    gpio_set_function(AUDIO_LEFT_PIN, GPIO_FUNC_PWM);
    gpio_set_function(AUDIO_RIGHT_PIN, GPIO_FUNC_PWM);
    g_slice = pwm_gpio_to_slice_num(AUDIO_LEFT_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, PWM_WRAP - 1);
    pwm_init(g_slice, &cfg, false);
    pwm_set_both_levels(g_slice, PWM_MID, PWM_MID);

    // Two DMA channels ping-pong through the ring, chained so playback is
    // gapless; each writes the 32-bit compare word paced by the slice DREQ.
    g_dma_a = dma_claim_unused_channel(true);
    g_dma_b = dma_claim_unused_channel(true);

    volatile void *cc = &pwm_hw->slice[g_slice].cc;

    dma_channel_config ca = dma_channel_get_default_config(g_dma_a);
    channel_config_set_transfer_data_size(&ca, DMA_SIZE_32);
    channel_config_set_read_increment(&ca, true);
    channel_config_set_write_increment(&ca, false);
    channel_config_set_ring(&ca, false, RING_WRAP_BITS); // wrap the read addr
    channel_config_set_dreq(&ca, pwm_get_dreq(g_slice));
    channel_config_set_chain_to(&ca, g_dma_b);
    dma_channel_configure(g_dma_a, &ca, (void *)cc, g_ring[0], HALF_SLOTS, false);

    dma_channel_config cb = dma_channel_get_default_config(g_dma_b);
    channel_config_set_transfer_data_size(&cb, DMA_SIZE_32);
    channel_config_set_read_increment(&cb, true);
    channel_config_set_write_increment(&cb, false);
    channel_config_set_ring(&cb, false, RING_WRAP_BITS); // wrap the read addr
    channel_config_set_dreq(&cb, pwm_get_dreq(g_slice));
    channel_config_set_chain_to(&cb, g_dma_a);
    dma_channel_configure(g_dma_b, &cb, (void *)cc, g_ring[1], HALF_SLOTS, false);

    dma_channel_set_irq0_enabled(g_dma_a, true);
    dma_channel_set_irq0_enabled(g_dma_b, true);
    irq_add_shared_handler(DMA_IRQ_0, dma_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    // The refill must meet a ~3.5 ms deadline (one ring half). Other handlers
    // at the default priority can run longer than that -- the keyboard poll
    // timer blocks on 10 kHz southbridge I2C for ~4-5 ms every 100 ms -- and
    // same-priority IRQs cannot preempt each other, which turned every poll
    // into an audible click. Run the audio IRQ above the default priority so
    // the refill preempts them. (Sound is the only DMA_IRQ_0 user; the LCD
    // DMA polls for completion and raises no IRQ.)
    irq_set_priority(DMA_IRQ_0, 0x40);
    irq_set_enabled(DMA_IRQ_0, true);

    g_ready = true;

    // Start the pipeline: kick channel A, then run the PWM slice.
    dma_channel_start(g_dma_a);
    pwm_set_enabled(g_slice, true);
}

//==========================================================================
// Ops
//==========================================================================

void sound_gate(int voice, uint32_t freq_hz, uint32_t dur_ms, int vol)
{
    if (voice < 0 || voice >= MAX_VOICES || !g_ready)
    {
        return;
    }
    uint32_t s = save_and_disable_interrupts();
    Voice *v = &g_v[voice];
    v->head = v->tail = 0; // an immediate gate flushes queued music
    start_note(v, freq_hz, dur_ms, vol);
    restore_interrupts(s);
}

int sound_queue(int voice, const SoundEvent *events, int n)
{
    if (voice < 0 || voice >= MAX_VOICES || !events || n <= 0 || !g_ready)
    {
        return 0;
    }
    Voice *v = &g_v[voice];
    int accepted = 0;
    for (int i = 0; i < n; i++)
    {
        uint32_t s = save_and_disable_interrupts();
        if (queue_free(v) == 0)
        {
            restore_interrupts(s);
            break;
        }
        v->q[v->tail] = events[i];
        v->tail = (uint16_t)((v->tail + 1) % QRING);
        restore_interrupts(s);
        accepted++;
    }
    return accepted;
}

SoundStatus sound_status(int voice)
{
    SoundStatus st = {false, 0};
    if (voice < 0 || voice >= MAX_VOICES || !g_ready)
    {
        return st;
    }
    Voice *v = &g_v[voice];
    st.sounding = (v->stage != ENV_IDLE) || !queue_empty(v);
    uint16_t free = queue_free(v);
    st.free_slots = (uint8_t)(free > 255 ? 255 : free);
    return st;
}

void sound_stop(uint32_t voice_mask)
{
    if (!g_ready)
    {
        return;
    }
    uint32_t s = save_and_disable_interrupts();
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (voice_mask & (1u << i))
        {
            Voice *v = &g_v[i];
            v->head = v->tail = 0;
            if (v->stage != ENV_IDLE && v->stage != ENV_RELEASE)
            {
                v->stage = ENV_RELEASE; // fade out through the release
                v->hold_us_left = 0;
            }
        }
    }
    restore_interrupts(s);
}

void sound_env(int voice, uint32_t attack_ms, uint32_t decay_ms, int sustain, uint32_t release_ms)
{
    if (voice < 0 || voice >= MAX_VOICES)
    {
        return;
    }
    Voice *v = &g_v[voice];
    v->attack_ms = (uint16_t)attack_ms;
    v->decay_ms = (uint16_t)decay_ms;
    v->sustain = (uint8_t)(sustain < 0 ? 0 : (sustain > 15 ? 15 : sustain));
    v->release_ms = (uint16_t)release_ms;
}

void sound_wave(int voice, int wave, int duty)
{
    if (voice < 0 || voice >= MAX_VOICES)
    {
        return;
    }
    Voice *v = &g_v[voice];
    v->wave = (uint8_t)wave;
    if (duty >= 1 && duty <= 99)
    {
        v->duty = (uint8_t)duty;
    }
}
