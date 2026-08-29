//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  PicoCalc speech back end (P16 M3). See speech.h and docs/say-design.md
//  §8.5.
//

#include "speech.h"

#include "pico/stdlib.h"
#include "hardware/sync.h"

#include "core/speech_synth.h"

#include <string.h>

// The whole back end: one engine, ~900 B of .bss, of which the phoneme queue
// is 516 (say-design.md §9.1). The 10 KB rule table is *not* here -- it runs
// in thread context at `say` time and stays in flash.
static SpeechEngine g_engine;

// One refill block of speech, rendered ahead of the mixer's frame loop. 256 B.
static int16_t g_block[SPEECH_BLOCK_FRAMES];

static bool g_ready;

void speech_init(uint32_t sample_rate_hz)
{
    speech_engine_init(&g_engine, (float)sample_rate_hz);
    memset(g_block, 0, sizeof g_block);
    g_ready = true;
}

void speech_reclock(uint32_t sample_rate_hz)
{
    if (!g_ready)
    {
        return;
    }
    uint32_t s = save_and_disable_interrupts();
    speech_engine_set_rate(&g_engine, (float)sample_rate_hz);
    restore_interrupts(s);
}

//==========================================================================
// Ops (thread context)
//==========================================================================
//
// The engine's queue is written here and read in the refill IRQ, so every
// op that touches it runs with interrupts off -- the same guard, for the
// same reason, that sound_queue uses.

int speech_queue(const SpeechFrame *frames, int n)
{
    if (!g_ready || !frames || n <= 0)
    {
        return 0;
    }
    uint32_t s = save_and_disable_interrupts();
    int accepted = speech_engine_queue(&g_engine, frames, n);
    restore_interrupts(s);
    return accepted;
}

SpeechStatus speech_status(void)
{
    SpeechStatus st = {false, 0};
    if (!g_ready)
    {
        return st;
    }
    uint32_t s = save_and_disable_interrupts();
    st.speaking = speech_engine_busy(&g_engine);
    int free = speech_engine_free_slots(&g_engine);
    restore_interrupts(s);
    st.free_slots = (uint8_t)(free > 255 ? 255 : free);
    return st;
}

void speech_stop(void)
{
    if (!g_ready)
    {
        return;
    }
    uint32_t s = save_and_disable_interrupts();
    speech_engine_stop(&g_engine);
    restore_interrupts(s);
}

void speech_voice(int pitch, int speed, int mouth, int throat)
{
    if (!g_ready)
    {
        return;
    }
    SpeechVoice v;
    v.pitch = (uint8_t)pitch;
    v.speed = (uint8_t)speed;
    v.mouth = (uint8_t)mouth;
    v.throat = (uint8_t)throat;

    uint32_t s = save_and_disable_interrupts();
    speech_engine_set_voice(&g_engine, &v);
    restore_interrupts(s);
}

//==========================================================================
// The mixer slot (IRQ context)
//==========================================================================

const int16_t *__not_in_flash_func(speech_mix_block)(int frames)
{
    if (!g_ready || !speech_engine_busy(&g_engine))
    {
        return NULL; // silent, which is nearly always, and costs nothing
    }
    if (frames > SPEECH_BLOCK_FRAMES)
    {
        frames = SPEECH_BLOCK_FRAMES;
    }
    speech_engine_render(&g_engine, g_block, frames);
    return g_block;
}
