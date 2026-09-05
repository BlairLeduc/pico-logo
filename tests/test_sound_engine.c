//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the PSG mixer's envelope (devices/picocalc/sound.c), compiled
//  on the host against the stubs in tests/mocks. The mock device records
//  what a program ASKED for; this is the only place that checks what comes
//  out the other end, so it is where a note that is silent on a board and
//  perfect in the gate log gets caught.
//
//  The file under test is #included rather than linked: the envelope and
//  the mixer are static, and driving them directly is the whole point.
//

#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "hardware/dma.h"
#include "hardware/pwm.h"

// The engine calls into the speech mixer once a block. Nothing here says
// anything, so it is always silent.
const int16_t *speech_mix_block(int frames);
void speech_init(uint32_t rate);
void speech_reclock(uint32_t rate);
const int16_t *speech_mix_block(int frames) { (void)frames; return NULL; }
void speech_init(uint32_t rate) { (void)rate; }
void speech_reclock(uint32_t rate) { (void)rate; }

// The two hardware blocks sound.c writes through at init. Neither is read
// back by anything under test.
static pwm_hw_t g_fake_pwm;
static dma_hw_t g_fake_dma;
pwm_hw_t *const pwm_hw = &g_fake_pwm;
dma_hw_t *const dma_hw = &g_fake_dma;

#include "devices/picocalc/sound.c"

void setUp(void)
{
    g_ready = false;
    sound_init();
}

void tearDown(void)
{
}

//==========================================================================
// Helpers
//==========================================================================

// One refill block of audio, as loud as it got. The mixer biases silence to
// PWM_MID in both ears, so the peak excursion from that is what a speaker
// would move.
static int peak_of_one_block(void)
{
    mix_half(g_ring[0]);
    int peak = 0;
    for (int i = 0; i < HALF_SLOTS; i++)
    {
        int left = (int)(g_ring[0][i] & 0xFFFFu) - PWM_MID;
        int right = (int)(g_ring[0][i] >> 16) - PWM_MID;
        if (left < 0) left = -left;
        if (right < 0) right = -right;
        if (left > peak) peak = left;
        if (right > peak) peak = right;
    }
    return peak;
}

//==========================================================================
// A note shorter than one refill block
//==========================================================================

// The mixer advances envelopes once per refill block -- ~3.5 ms at the
// board's 150 MHz clock -- so a note shorter than that has exactly one
// block in which to be heard. B99: the block that retired the note also
// stepped the envelope, and the retirement went first, so the attack never
// ran and the note came out at amplitude zero. Every percussive effect in
// the ROM's sound table is written this way (SOUNDS.ASM starts its sounds
// at full amplitude and lets the envelope generator fall), which is why
// Daggorath's KLINK, CLANK, CHUCK and all three KLANKs were silent while
// the 83 ms WHOOSH beside them was not.
void test_a_note_shorter_than_a_refill_block_is_still_heard(void)
{
    TEST_ASSERT_TRUE_MESSAGE(g_block_us > 1000,
                             "the block is under a millisecond; this test proves nothing");

    sound_env(1, 0, 0, 15, 107); // Daggorath's KLINK: instant rise, long fall
    sound_gate(1, 3174, 1, 15);

    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, peak_of_one_block(),
                                         "a one-millisecond note made no sound at all");
}

// ...and it goes on sounding through its release, rather than being cut off
// with the gate. A 1 ms gate and a 107 ms release is 107 ms of audible
// note: about thirty blocks, and the amplitude falls the whole way.
void test_a_percussive_note_falls_through_its_release(void)
{
    sound_env(1, 0, 0, 15, 107);
    sound_gate(1, 3174, 1, 15);

    const int first = peak_of_one_block();
    TEST_ASSERT_GREATER_THAN_INT(0, first);

    int blocks = 1, last = first;
    while (g_v[1].stage != ENV_IDLE && blocks < 200)
    {
        last = peak_of_one_block();
        blocks++;
    }
    TEST_ASSERT_LESS_THAN_INT_MESSAGE(200, blocks, "the note never ended");
    // 107 ms of release at ~3.5 ms a block, plus the block the gate held.
    TEST_ASSERT_INT_WITHIN_MESSAGE(4, 32, blocks, "the release was not 107 ms long");
    TEST_ASSERT_LESS_THAN_INT_MESSAGE(first, last, "the note did not fade");
}

// The fix must not lengthen an ordinary note. A 90 ms gate is 25 blocks of
// hold before the release starts, the same as it ever was -- this is the
// reference's own percussive pluck (`setenv 1 [0 60 0 40]`).
void test_a_note_longer_than_a_block_holds_for_its_whole_duration(void)
{
    sound_env(1, 0, 60, 0, 40);
    sound_gate(1, 1800, 90, 15);

    int held = 0;
    while (g_v[1].stage != ENV_RELEASE && g_v[1].stage != ENV_IDLE && held < 200)
    {
        mix_half(g_ring[0]);
        held++;
    }
    // 90 ms / 3.5 ms, and the block that finds the hold expired is the
    // first release block, not the last held one.
    TEST_ASSERT_INT_WITHIN_MESSAGE(1, 26, held, "the gate did not hold for 90 ms");
}

// A rest is still silent: `sound` with a frequency outside 20..10000 Hz
// gates the voice off, and the shortest possible rest must not click.
void test_a_short_rest_makes_no_sound(void)
{
    sound_env(1, 0, 0, 15, 107);
    sound_gate(1, 0, 1, 15);

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, peak_of_one_block(), "a rest made a noise");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_note_shorter_than_a_refill_block_is_still_heard);
    RUN_TEST(test_a_percussive_note_falls_through_its_release);
    RUN_TEST(test_a_note_longer_than_a_block_holds_for_its_whole_duration);
    RUN_TEST(test_a_short_rest_makes_no_sound);
    return UNITY_END();
}
