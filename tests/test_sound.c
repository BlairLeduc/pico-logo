//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the sound primitives (core/primitives_sound.c) and the toot
//  reroute onto the sound engine (core/primitives_hardware.c), against the
//  scripted mock sound backend.
//

#include "test_scaffold.h"

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

static const MockDeviceState *snd(void)
{
    return mock_device_get_state();
}

//==========================================================================
// toot, rerouted through sound_gate on voices 0 and 4
//==========================================================================

void test_toot_gates_voices_0_and_4(void)
{
    Result r = run_string("toot 500 440");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_INT(2, s->sound.gate_count);
    TEST_ASSERT_EQUAL_INT(0, s->sound.gates[0].voice);
    TEST_ASSERT_EQUAL_UINT32(440, s->sound.gates[0].freq);
    TEST_ASSERT_EQUAL_UINT32(500, s->sound.gates[0].dur);
    TEST_ASSERT_EQUAL_INT(15, s->sound.gates[0].vol);
    TEST_ASSERT_EQUAL_INT(4, s->sound.gates[1].voice);
    TEST_ASSERT_EQUAL_UINT32(440, s->sound.gates[1].freq);
}

void test_toot_out_of_range_is_rest(void)
{
    // 50 Hz is below toot's 100-2000 Hz range: both voices gate a rest (0 Hz).
    Result r = run_string("toot 100 50");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_UINT32(0, s->sound.gates[0].freq);
    TEST_ASSERT_EQUAL_UINT32(0, s->sound.gates[1].freq);
}

void test_toot_stereo(void)
{
    Result r = run_string("(toot 1000 440 523)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_INT(0, s->sound.gates[0].voice);
    TEST_ASSERT_EQUAL_UINT32(440, s->sound.gates[0].freq);
    TEST_ASSERT_EQUAL_INT(4, s->sound.gates[1].voice);
    TEST_ASSERT_EQUAL_UINT32(523, s->sound.gates[1].freq);
}

//==========================================================================
// sound
//==========================================================================

void test_sound_gates_voice(void)
{
    Result r = run_string("sound 0 440 500");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_INT(1, s->sound.gate_count);
    TEST_ASSERT_EQUAL_INT(0, s->sound.gates[0].voice);
    TEST_ASSERT_EQUAL_UINT32(440, s->sound.gates[0].freq);
    TEST_ASSERT_EQUAL_UINT32(500, s->sound.gates[0].dur);
    TEST_ASSERT_EQUAL_INT(15, s->sound.gates[0].vol); // default current volume
}

void test_sound_with_volume(void)
{
    Result r = run_string("(sound 3 6000 80 12)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_INT(3, s->sound.gates[0].voice);
    TEST_ASSERT_EQUAL_UINT32(6000, s->sound.gates[0].freq);
    TEST_ASSERT_EQUAL_INT(12, s->sound.gates[0].vol);
}

void test_sound_fans_out_over_voice_list(void)
{
    Result r = run_string("(sound [3 7] 6000 80)");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_INT(2, s->sound.gate_count);
    TEST_ASSERT_EQUAL_INT(3, s->sound.gates[0].voice);
    TEST_ASSERT_EQUAL_INT(7, s->sound.gates[1].voice);
    TEST_ASSERT_EQUAL_UINT32(6000, s->sound.gates[1].freq);
}

void test_sound_bad_voice_errors(void)
{
    Result r = run_string("sound 8 440 500");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_sound_out_of_range_frequency_is_rest(void)
{
    Result r = run_string("sound 0 15 500"); // 15 Hz < 20 Hz floor
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_UINT32(0, snd()->sound.gates[0].freq);
}

//==========================================================================
// setenv / env
//==========================================================================

// Assert a Value list of numbers equals `expected` (n strings).
static void assert_number_list(Value v, const char **expected, int n)
{
    TEST_ASSERT_EQUAL(VALUE_LIST, v.type);
    Node l = v.as.node;
    for (int i = 0; i < n; i++)
    {
        TEST_ASSERT_FALSE(mem_is_nil(l));
        TEST_ASSERT_EQUAL_STRING(expected[i], mem_word_ptr(mem_car(l)));
        l = mem_cdr(l);
    }
    TEST_ASSERT_TRUE(mem_is_nil(l));
}

void test_env_default(void)
{
    Result r = eval_string("env 0");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    const char *expected[] = {"5", "0", "15", "30"};
    assert_number_list(r.value, expected, 4);
}

void test_setenv_env_roundtrip(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("setenv 1 [0 60 0 40]").status);

    // The device was told, too.
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_UINT32(0, s->sound.env[1].attack);
    TEST_ASSERT_EQUAL_UINT32(60, s->sound.env[1].decay);
    TEST_ASSERT_EQUAL_INT(0, s->sound.env[1].sustain);
    TEST_ASSERT_EQUAL_UINT32(40, s->sound.env[1].release);

    Result r = eval_string("env 1");
    const char *expected[] = {"0", "60", "0", "40"};
    assert_number_list(r.value, expected, 4);
}

void test_setenv_bad_sustain_errors(void)
{
    // Sustain must be 0..15.
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setenv 0 [0 0 20 0]").status);
}

//==========================================================================
// setwave / wave
//==========================================================================

static void assert_word(Result r, const char *expected)
{
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING(expected, mem_word_ptr(r.value.as.node));
}

void test_wave_defaults(void)
{
    assert_word(eval_string("wave 0"), "square"); // tone voice
    assert_word(eval_string("wave 3"), "white");  // noise voice
}

void test_setwave_tone_roundtrip(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("setwave 0 \"triangle").status);
    assert_word(eval_string("wave 0"), "triangle");
}

void test_setwave_pulse_with_duty(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("(setwave 1 \"pulse 25)").status);
    assert_word(eval_string("wave 1"), "pulse");
    TEST_ASSERT_EQUAL_INT(25, snd()->sound.wave[1].duty);
}

void test_setwave_noise_roundtrip(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("setwave 3 \"periodic").status);
    assert_word(eval_string("wave 3"), "periodic");
}

void test_setwave_tone_wave_on_noise_voice_errors(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setwave 3 \"square").status);
}

void test_setwave_noise_wave_on_tone_voice_errors(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setwave 0 \"white").status);
}

void test_setwave_unknown_waveform_errors(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setwave 0 \"sine").status);
}

//==========================================================================
// play / playing?
//==========================================================================

void test_play_queues_notes(void)
{
    Result r = run_string("play [c d e]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_INT(3, s->sound.queued_count);
    TEST_ASSERT_INT_WITHIN(1, 262, s->sound.queued[0].freq_hz); // c4
    TEST_ASSERT_INT_WITHIN(1, 294, s->sound.queued[1].freq_hz); // d4
    TEST_ASSERT_INT_WITHIN(1, 330, s->sound.queued[2].freq_hz); // e4
    // Voice 0 by default.
    TEST_ASSERT_EQUAL_INT(SOUND_QUEUE_LEN - 3, s->sound.free_slots[0]);
}

void test_play_voice_argument(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("(play 1 [c])").status);
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_INT(SOUND_QUEUE_LEN - 1, s->sound.free_slots[1]);
    TEST_ASSERT_EQUAL_INT(SOUND_QUEUE_LEN, s->sound.free_slots[0]); // voice 0 untouched
}

void test_play_appends(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("play [c]").status);
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("play [d]").status);
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_INT(2, s->sound.queued_count);
    TEST_ASSERT_EQUAL_INT(SOUND_QUEUE_LEN - 2, s->sound.free_slots[0]);
}

void test_play_fans_out(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("(play [0 4] [c])").status);
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_INT(SOUND_QUEUE_LEN - 1, s->sound.free_slots[0]);
    TEST_ASSERT_EQUAL_INT(SOUND_QUEUE_LEN - 1, s->sound.free_slots[4]);
}

void test_play_bad_notation_errors(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("play [h]").status);
}

void test_play_waits_for_queue_space(void)
{
    // Only two free slots, but five notes: play must wait (the mock drains a
    // slot per status poll) and eventually queue them all.
    mock_sound_set_status(0, false, 2);
    Result r = run_string("play [c d e f g]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_INT(5, snd()->sound.queued_count);
}

void test_play_queue_full_break(void)
{
    // Full queue and a pending BREAK: play stops with an error instead of
    // hanging.
    mock_sound_set_status(0, false, 0);
    mock_user_interrupt = true;
    Result r = run_string("play [c d e]");
    mock_user_interrupt = false;
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_playing_false_when_idle(void)
{
    Result r = eval_string("playing?");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_playing_true_after_play(void)
{
    run_string("play [c d e]");
    Result r = eval_string("playing?");
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_playing_single_voice(void)
{
    run_string("(play 1 [c])");
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(eval_string("(playing? 1)").value.as.node));
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(eval_string("(playing? 0)").value.as.node));
}

//==========================================================================
// stopsound
//==========================================================================

void test_stopsound_stops_all_voices(void)
{
    run_string("play [c d e]");
    Result r = run_string("stopsound");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    const MockDeviceState *s = snd();
    TEST_ASSERT_EQUAL_INT(1, s->sound.stop_count);
    TEST_ASSERT_EQUAL_UINT32(0xFFu, s->sound.last_stop_mask); // all 8 voices
    TEST_ASSERT_EQUAL_INT(SOUND_QUEUE_LEN, s->sound.free_slots[0]); // queue cleared
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_toot_gates_voices_0_and_4);
    RUN_TEST(test_toot_out_of_range_is_rest);
    RUN_TEST(test_toot_stereo);
    RUN_TEST(test_sound_gates_voice);
    RUN_TEST(test_sound_with_volume);
    RUN_TEST(test_sound_fans_out_over_voice_list);
    RUN_TEST(test_sound_bad_voice_errors);
    RUN_TEST(test_sound_out_of_range_frequency_is_rest);
    RUN_TEST(test_env_default);
    RUN_TEST(test_setenv_env_roundtrip);
    RUN_TEST(test_setenv_bad_sustain_errors);
    RUN_TEST(test_wave_defaults);
    RUN_TEST(test_setwave_tone_roundtrip);
    RUN_TEST(test_setwave_pulse_with_duty);
    RUN_TEST(test_setwave_noise_roundtrip);
    RUN_TEST(test_setwave_tone_wave_on_noise_voice_errors);
    RUN_TEST(test_setwave_noise_wave_on_tone_voice_errors);
    RUN_TEST(test_setwave_unknown_waveform_errors);
    RUN_TEST(test_play_queues_notes);
    RUN_TEST(test_play_voice_argument);
    RUN_TEST(test_play_appends);
    RUN_TEST(test_play_fans_out);
    RUN_TEST(test_play_bad_notation_errors);
    RUN_TEST(test_play_waits_for_queue_space);
    RUN_TEST(test_play_queue_full_break);
    RUN_TEST(test_playing_false_when_idle);
    RUN_TEST(test_playing_true_after_play);
    RUN_TEST(test_playing_single_voice);
    RUN_TEST(test_stopsound_stops_all_voices);
    return UNITY_END();
}
