//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Tests for the speech primitives (core/primitives_speech.c) and the speech
//  half of `stopsound` (core/primitives_sound.c), against the scripted mock
//  speech backend. docs/say-design.md §10.
//
//  M2 added `say` and `phonemes` in front of them, so the identity
//  `say :text` == `sayphonemes phonemes :text` is assertable here -- over
//  the mock log, which is the only place it is observable.
//
//  M3 added `setvoice`/`voice`. What the knobs do to the sound is
//  test_speech_synth.c's business; what is testable here is the shape --
//  validation, the round trip through the core-side shadow, and that the
//  device hears about it.
//

#include "test_scaffold.h"
#include "core/speech_synth.h"

#include <string.h>

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
// say and phonemes (§5.1, §5.2)
//==========================================================================

// Join a mock log slice into a phoneme string, so a failure reads as words.
static void log_string(int from, int to, char *out, size_t size)
{
    const MockDeviceState *s = spk();
    out[0] = '\0';
    for (int i = from; i < to; i++)
    {
        if (i > from)
        {
            strncat(out, " ", size - strlen(out) - 1);
        }
        strncat(out, speech_phoneme_names[s->speech.queued[i].phoneme], size - strlen(out) - 1);
    }
}

// Run one instruction and read back only what it added to the log, so a
// test can say two things in a row.
static void assert_say(const char *logo, const char *expected)
{
    int before = spk()->speech.queued_count;
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string(logo).status);
    char got[256];
    log_string(before, spk()->speech.queued_count, got, sizeof got);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, got, logo);
}

void test_say_runs_a_list_through_the_rules(void)
{
    assert_say("say [hello]", "hh eh l ow");
}

void test_say_takes_a_word(void)
{
    assert_say("say \"chicken", "ch ih k eh n");
}

// A sentence, not a string of unrelated words: " [THE] #" reads the letter
// after the space, which only works because the list is joined back up.
void test_say_reads_across_the_space_between_words(void)
{
    assert_say("say [the apple]", "dh iy ae p ax l");
    assert_say("say [the book]", "dh ax b uh k");
}

void test_say_speaks_a_number_as_digits(void)
{
    assert_say("say 42", "f ao r t uw");
}

// Appending, like `sayphonemes` and like `play` -- Berzerk assembles a
// sentence a word at a time (§5.1).
void test_say_appends_on_a_second_call(void)
{
    assert_say("say \"go", "g ow");
    assert_say("say \"go", "g ow");
    TEST_ASSERT_EQUAL_INT(4, spk()->speech.queued_count);
}

void test_say_accepts_an_empty_list(void)
{
    Result r = run_string("say []");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.queued_count);
}

void test_say_requires_an_input(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("say").status);
}

// The identity the reference gives as the definition (§5.3), asserted over
// the mock log: the two halves must match phoneme for phoneme.
void test_say_is_sayphonemes_of_phonemes(void)
{
    run_string("say [the humanoid must not escape]");
    int half = spk()->speech.queued_count;
    TEST_ASSERT_GREATER_THAN_INT(0, half);

    run_string("sayphonemes phonemes [the humanoid must not escape]");
    TEST_ASSERT_EQUAL_INT(2 * half, spk()->speech.queued_count);

    char first[256], second[256];
    log_string(0, half, first, sizeof first);
    log_string(half, 2 * half, second, sizeof second);
    TEST_ASSERT_EQUAL_STRING(first, second);
}

// A sentence longer than the SPEECH_TEXT_MAX join buffer is translated a
// bufferful at a time, split between words, and nothing is lost.
void test_say_streams_a_sentence_longer_than_the_join_buffer(void)
{
    run_string("say [the humanoid must not escape the humanoid must not escape "
               "the humanoid must not escape the humanoid must not escape "
               "the humanoid must not escape]");
    int five = spk()->speech.queued_count;
    run_string("say [the humanoid must not escape]");
    int one = spk()->speech.queued_count - five;
    TEST_ASSERT_EQUAL_INT(5 * one, five);
}

void test_phonemes_outputs_a_list_of_phoneme_words(void)
{
    Result r = eval_string("phonemes [hello]");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_EQUAL(VALUE_LIST, r.value.type);

    const char *expected[] = {"hh", "eh", "l", "ow"};
    int i = 0;
    for (Node l = mem_first_cell(r.value.as.node); !mem_is_nil(l); l = mem_next_cell(l), i++)
    {
        TEST_ASSERT_LESS_THAN_INT(4, i);
        TEST_ASSERT_EQUAL_STRING(expected[i], mem_word_ptr(mem_car(l)));
    }
    TEST_ASSERT_EQUAL_INT(4, i);
}

// It says nothing: printing the phonemes is how you find out why a word
// came out wrong, and doing that should not also speak it (§5.2).
void test_phonemes_is_an_operation_and_makes_no_sound(void)
{
    eval_string("phonemes [hello]");
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.queued_count);
}

void test_phonemes_of_nothing_is_the_empty_list(void)
{
    Result r = eval_string("phonemes []");
    TEST_ASSERT_EQUAL(RESULT_OK, r.status);
    TEST_ASSERT_TRUE(mem_is_nil(r.value.as.node));
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
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("say [intruder alert]").status);
    TEST_ASSERT_EQUAL_STRING("false", mem_word_ptr(eval_string("speaking?").value.as.node));
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("stopsound").status);
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.queued_count);

    ops->speech_queue = saved_queue;
    ops->speech_status = saved_status;
    ops->speech_stop = saved_stop;
}

//==========================================================================
// setvoice / voice (M3, say-design.md §5.5)
//==========================================================================

void test_voice_starts_at_the_default(void)
{
    Value v = eval_string("voice").value;
    TEST_ASSERT_EQUAL_STRING("[50 128 128 128]", value_to_string(v));
}

void test_setvoice_sets_all_four_knobs(void)
{
    Result r = run_string("setvoice [30 150 200 90]");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("[30 150 200 90]", value_to_string(eval_string("voice").value));
}

void test_setvoice_reaches_the_device(void)
{
    run_string("setvoice [30 150 200 90]");

    const MockDeviceState *s = spk();
    TEST_ASSERT_EQUAL_INT(1, s->speech.voice_count);
    TEST_ASSERT_EQUAL_INT(30, s->speech.voice_pitch);
    TEST_ASSERT_EQUAL_INT(150, s->speech.voice_speed);
    TEST_ASSERT_EQUAL_INT(200, s->speech.voice_mouth);
    TEST_ASSERT_EQUAL_INT(90, s->speech.voice_throat);
}

// All four are 1..255, which is one rule rather than four.
void test_setvoice_rejects_a_knob_out_of_range(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setvoice [0 128 128 128]").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setvoice [50 256 128 128]").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setvoice [50 128 -1 128]").status);

    // And none of them got through.
    TEST_ASSERT_EQUAL_STRING("[50 128 128 128]", value_to_string(eval_string("voice").value));
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.voice_count);
}

void test_setvoice_wants_exactly_four_numbers(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setvoice [50 128 128]").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setvoice [50 128 128 128 128]").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setvoice [50 128 128 loud]").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setvoice []").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setvoice 50").status);
}

void test_voice_takes_no_input(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("show (voice 1)").status);
}

// §5.6: `stopsound`'s promise not to touch the timbre extends to the voice.
void test_stopsound_leaves_the_voice_alone(void)
{
    run_string("setvoice [30 150 200 90]");
    run_string("sayphonemes [ax l er t]");
    run_string("stopsound");
    TEST_ASSERT_EQUAL_STRING("[30 150 200 90]", value_to_string(eval_string("voice").value));
}

// A board with no speech engine still answers `voice`, because the shadow is
// core-side -- the same arrangement `env` has (primitives_sound.c).
void test_setvoice_is_silent_and_readable_with_no_device_op(void)
{
    LogoIO *io = primitives_get_io();
    LogoHardwareOps *ops = io->hardware->ops;
    void (*saved)(int, int, int, int) = ops->speech_voice;
    ops->speech_voice = NULL;

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("setvoice [30 150 200 90]").status);
    TEST_ASSERT_EQUAL_STRING("[30 150 200 90]", value_to_string(eval_string("voice").value));
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.voice_count);

    ops->speech_voice = saved;
}

//==========================================================================
// setsayvolume / sayvolume (say-design.md §5.8)
//==========================================================================

void test_sayvolume_starts_full(void)
{
    TEST_ASSERT_EQUAL_STRING("15", value_to_string(eval_string("sayvolume").value));

    // And nothing was sent to the device to make that true: full is what the
    // mixer slot already is.
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.volume_count);
}

void test_setsayvolume_sets_and_reaches_the_device(void)
{
    Result r = run_string("setsayvolume 8");
    TEST_ASSERT_EQUAL(RESULT_NONE, r.status);
    TEST_ASSERT_EQUAL_STRING("8", value_to_string(eval_string("sayvolume").value));

    const MockDeviceState *s = spk();
    TEST_ASSERT_EQUAL_INT(1, s->speech.volume_count);
    TEST_ASSERT_EQUAL_INT(8, s->speech.volume);
}

// 0 is silence, not an error: the same end of the scale `sound` has.
void test_setsayvolume_takes_both_ends_of_the_scale(void)
{
    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("setsayvolume 0").status);
    TEST_ASSERT_EQUAL_STRING("0", value_to_string(eval_string("sayvolume").value));
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.volume);

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("setsayvolume 15").status);
    TEST_ASSERT_EQUAL_STRING("15", value_to_string(eval_string("sayvolume").value));
    TEST_ASSERT_EQUAL_INT(15, spk()->speech.volume);
}

void test_setsayvolume_rejects_a_level_off_the_scale(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setsayvolume 16").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setsayvolume -1").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setsayvolume loud").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setsayvolume [8]").status);

    // And none of them got through.
    TEST_ASSERT_EQUAL_STRING("15", value_to_string(eval_string("sayvolume").value));
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.volume_count);
}

void test_setsayvolume_wants_one_input_and_sayvolume_none(void)
{
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("setsayvolume").status);
    TEST_ASSERT_EQUAL(RESULT_ERROR, run_string("show (sayvolume 1)").status);
}

// The volume is the mixer's, not the voice's: `setvoice` does not disturb it
// and `voice` does not report it.
void test_the_volume_and_the_voice_are_independent(void)
{
    run_string("setsayvolume 4");
    run_string("setvoice [30 150 200 90]");
    TEST_ASSERT_EQUAL_STRING("4", value_to_string(eval_string("sayvolume").value));
    TEST_ASSERT_EQUAL_STRING("[30 150 200 90]", value_to_string(eval_string("voice").value));
}

// §5.6 again: `stopsound` shuts the voice up, it does not turn it back up.
void test_stopsound_leaves_the_volume_alone(void)
{
    run_string("setsayvolume 4");
    run_string("sayphonemes [ax l er t]");
    run_string("stopsound");
    TEST_ASSERT_EQUAL_STRING("4", value_to_string(eval_string("sayvolume").value));
}

// A board with no speech engine still answers `sayvolume`, for the reason
// `voice` does.
void test_setsayvolume_is_silent_and_readable_with_no_device_op(void)
{
    LogoIO *io = primitives_get_io();
    LogoHardwareOps *ops = io->hardware->ops;
    void (*saved)(int) = ops->speech_volume;
    ops->speech_volume = NULL;

    TEST_ASSERT_EQUAL(RESULT_NONE, run_string("setsayvolume 4").status);
    TEST_ASSERT_EQUAL_STRING("4", value_to_string(eval_string("sayvolume").value));
    TEST_ASSERT_EQUAL_INT(0, spk()->speech.volume_count);

    ops->speech_volume = saved;
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
    RUN_TEST(test_say_runs_a_list_through_the_rules);
    RUN_TEST(test_say_takes_a_word);
    RUN_TEST(test_say_reads_across_the_space_between_words);
    RUN_TEST(test_say_speaks_a_number_as_digits);
    RUN_TEST(test_say_appends_on_a_second_call);
    RUN_TEST(test_say_accepts_an_empty_list);
    RUN_TEST(test_say_requires_an_input);
    RUN_TEST(test_say_is_sayphonemes_of_phonemes);
    RUN_TEST(test_say_streams_a_sentence_longer_than_the_join_buffer);
    RUN_TEST(test_phonemes_outputs_a_list_of_phoneme_words);
    RUN_TEST(test_phonemes_is_an_operation_and_makes_no_sound);
    RUN_TEST(test_phonemes_of_nothing_is_the_empty_list);
    RUN_TEST(test_speaking_false_when_idle);
    RUN_TEST(test_speaking_true_after_sayphonemes);
    RUN_TEST(test_speakingp_is_the_same_primitive);
    RUN_TEST(test_speaking_takes_no_input);
    RUN_TEST(test_stopsound_stops_speech_too);
    RUN_TEST(test_speech_is_silent_and_successful_with_no_device_ops);
    RUN_TEST(test_voice_starts_at_the_default);
    RUN_TEST(test_setvoice_sets_all_four_knobs);
    RUN_TEST(test_setvoice_reaches_the_device);
    RUN_TEST(test_setvoice_rejects_a_knob_out_of_range);
    RUN_TEST(test_setvoice_wants_exactly_four_numbers);
    RUN_TEST(test_voice_takes_no_input);
    RUN_TEST(test_stopsound_leaves_the_voice_alone);
    RUN_TEST(test_setvoice_is_silent_and_readable_with_no_device_op);
    RUN_TEST(test_sayvolume_starts_full);
    RUN_TEST(test_setsayvolume_sets_and_reaches_the_device);
    RUN_TEST(test_setsayvolume_takes_both_ends_of_the_scale);
    RUN_TEST(test_setsayvolume_rejects_a_level_off_the_scale);
    RUN_TEST(test_setsayvolume_wants_one_input_and_sayvolume_none);
    RUN_TEST(test_the_volume_and_the_voice_are_independent);
    RUN_TEST(test_stopsound_leaves_the_volume_alone);
    RUN_TEST(test_setsayvolume_is_silent_and_readable_with_no_device_op);
    return UNITY_END();
}
