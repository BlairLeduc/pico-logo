//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the speech primitives (core/primitives_speech.c) and the speech
//  half of `stopsound` (core/primitives_sound.c), against the scripted mock
//  speech backend. docs/say-design.md §10.
//
//  `say`, `phonemes` and the `say :text` == `sayphonemes phonemes :text`
//  identity arrive with the rule engine at M2.
//

#include "test_scaffold.h"
#include "core/speech_synth.h"

void setUp(void)
{
    test_scaffold_setUp();
}

void tearDown(void)
{
    test_scaffold_tearDown();
}

static const MockDeviceState *spk(void)
{
    return mock_device_get_state();
}

//==========================================================================
// sayphonemes
//==========================================================================

void test_sayphonemes_queues_each_phoneme_in_order(void)
{
    Result r = run_string("sayphonemes [ih n t r uw d er]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *s = spk();
    TEST_ASSERT_EQUAL_INT(7, s->speech.queued_count);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_IH, s->speech.queued[0].phoneme);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_N, s->speech.queued[1].phoneme);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_T, s->speech.queued[2].phoneme);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_R, s->speech.queued[3].phoneme);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_UW, s->speech.queued[4].phoneme);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_D, s->speech.queued[5].phoneme);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_ER, s->speech.queued[6].phoneme);

    // Everything but the phoneme is left at the table default.
    TEST_ASSERT_EQUAL_UINT8(0, s->speech.queued[0].dur_ms);
    TEST_ASSERT_EQUAL_UINT8(0, s->speech.queued[0].pitch);
}

void test_sayphonemes_is_case_insensitive(void)
{
    Result r = run_string("sayphonemes [IH N]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_IH, spk()->speech.queued[0].phoneme);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_N, spk()->speech.queued[1].phoneme);
}

// The append contract (§5.3): a sentence can be assembled a word at a time,
// which is what Berzerk's sentence assembly needs.
void test_sayphonemes_appends_on_a_second_call(void)
{
    run_string("sayphonemes [ih n t r uw d er]");
    run_string("sayphonemes [ax l er t]");

    const MockDeviceState *s = spk();
    TEST_ASSERT_EQUAL_INT(11, s->speech.queued_count);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_AX, s->speech.queued[7].phoneme);
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_T, s->speech.queued[10].phoneme);
}

void test_sayphonemes_takes_the_pause(void)
{
    run_string("sayphonemes [ax _ ax]");
    TEST_ASSERT_EQUAL_UINT8(SPEECH_PH_PAUSE, spk()->speech.queued[1].phoneme);
}

// A word that is not a phoneme is an error, not silence: this primitive is
// the escape hatch for a mispronunciation, so a typo in it has to say so.
void test_sayphonemes_rejects_an_unknown_phoneme(void)
{
    Result r = run_string("sayphonemes [ih n zzz]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_sayphonemes_rejects_a_sublist(void)
{
    Result r = run_string("sayphonemes [ih [n]]");
    TEST_ASSERT_EQUAL(RESULT_ERROR, r.status);
}

void test_sayphonemes_requires_a_list(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("sayphonemes \"ih").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("sayphonemes").status);
}

void test_sayphonemes_accepts_an_empty_list(void)
{
    Result r = run_string("sayphonemes []");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.queued_count);
}

// The queue-full path: `sayphonemes` waits for slots rather than erroring,
// exactly as `play` does (§5.3, limits.h SPEECH_QUEUE_LEN).
void test_sayphonemes_waits_for_a_full_queue(void)
{
    mock_speech_set_status(true, 0);
    Result r = run_string("sayphonemes [ax l er t]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_INT(4, spk()->speech.queued_count);
}

//==========================================================================
// speaking?
//==========================================================================

void test_speaking_false_when_idle(void)
{
    Result r = eval_string("speaking?");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_WORD, r.value.type);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(r.value.as.node));
}

void test_speaking_true_after_sayphonemes(void)
{
    run_string("sayphonemes [ax l er t]");
    Result r = eval_string("speaking?");
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(r.value.as.node));
}

void test_speakingp_is_the_same_primitive(void)
{
    run_string("sayphonemes [ax]");
    TEST_ASSERT_EQUAL_STRING("true", mem_word_ptr(eval_string("speakingp").value.as.node));
}

void test_speaking_takes_no_input(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("(speaking? 0)").status);
}

//==========================================================================
// stopsound, extended (§5.6)
//==========================================================================

void test_stopsound_stops_speech_too(void)
{
    run_string("sayphonemes [ax l er t]");
    Result r = run_string("stopsound");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);

    const MockDeviceState *s = spk();
    TEST_ASSERT_EQUAL_INT(1, s->speech.stop_count);
    TEST_ASSERT_EQUAL_INT(SPEECH_QUEUE_LEN, s->speech.free_slots); // queue cleared
    TEST_ASSERT_EQUAL_INT(1, s->sound.stop_count);                 // and the voices
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(eval_string("speaking?").value.as.node));
}

//==========================================================================
// The silent device (§4): a Logo program that speaks still runs in CI.
//==========================================================================

void test_speech_is_silent_and_successful_with_no_device_ops(void)
{
    LogoIO *io = primitives_get_io();
    LogoHardwareOps *ops = io->hardware->ops;
    int (*saved_queue)(const SpeechFrame *, int) = ops->speech_queue;
    SpeechStatus (*saved_status)(void) = ops->speech_status;
    void (*saved_stop)(void) = ops->speech_stop;

    ops->speech_queue = NULL;
    ops->speech_status = NULL;
    ops->speech_stop = NULL;

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("sayphonemes [ax l er t]").status);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(eval_string("speaking?").value.as.node));
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stopsound").status);
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.queued_count);

    ops->speech_queue = saved_queue;
    ops->speech_status = saved_status;
    ops->speech_stop = saved_stop;
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sayphonemes_queues_each_phoneme_in_order);
    RUN_TEST(test_sayphonemes_is_case_insensitive);
    RUN_TEST(test_sayphonemes_appends_on_a_second_call);
    RUN_TEST(test_sayphonemes_takes_the_pause);
    RUN_TEST(test_sayphonemes_rejects_an_unknown_phoneme);
    RUN_TEST(test_sayphonemes_rejects_a_sublist);
    RUN_TEST(test_sayphonemes_requires_a_list);
    RUN_TEST(test_sayphonemes_accepts_an_empty_list);
    RUN_TEST(test_sayphonemes_waits_for_a_full_queue);
    RUN_TEST(test_speaking_false_when_idle);
    RUN_TEST(test_speaking_true_after_sayphonemes);
    RUN_TEST(test_speakingp_is_the_same_primitive);
    RUN_TEST(test_speaking_takes_no_input);
    RUN_TEST(test_stopsound_stops_speech_too);
    RUN_TEST(test_speech_is_silent_and_successful_with_no_device_ops);
    return UNITY_END();
}
