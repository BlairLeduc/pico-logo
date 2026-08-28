//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Speech primitives (P16), M1: sayphonemes and speaking?. `stopsound`'s
//  speech half stays in primitives_sound.c, because there is one "shut up"
//  in the language and it should mean all of it (docs/say-design.md §5.6).
//
//  This is the `play` split applied again: semantics here, rendering in the
//  device. A phoneme word is resolved to a table index by core/speech_synth.c
//  -- the mirror of core/notation.c resolving a note word to a SoundEvent --
//  and the queue-full wait is the one `play` uses, for the same reason.
//
//  `say` and `phonemes` are M2's: they need the NRL letter-to-sound rules in
//  front of this, and `say :text` is defined as `sayphonemes phonemes :text`.
//

#include "primitives.h"
#include "memory.h"
#include "error.h"
#include "eval.h"
#include "speech_synth.h"
#include "limits.h"
#include "devices/io.h"

static LogoHardwareOps *speech_ops(void)
{
    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops)
    {
        return io->hardware->ops;
    }
    return NULL;
}

// Queue one phoneme, waiting (BREAK-able) for space if the utterance queue is
// full -- docs/sound-design.md Q3's answer, which `play` already uses. On a
// device with no speech engine this is a no-op that reports success, so a
// Logo program that speaks still runs (say-design.md §4).
static Result queue_frame_waiting(LogoIO *io, LogoHardwareOps *ops, const SpeechFrame *f)
{
    for (;;)
    {
        int accepted = (ops && ops->speech_queue) ? ops->speech_queue(f, 1) : 1;
        if (accepted >= 1)
        {
            return result_none();
        }
        if (logo_io_check_user_interrupt(io))
        {
            return result_error(ERR_STOPPED);
        }
        if (ops->speech_status)
        {
            while (ops->speech_status().free_slots == 0)
            {
                if (logo_io_check_user_interrupt(io))
                {
                    return result_error(ERR_STOPPED);
                }
                logo_io_sleep(io, 2);
            }
        }
        else
        {
            logo_io_sleep(io, 2);
        }
    }
}

// sayphonemes [ih n t r uw d er]
//
// Appends to the utterance and returns at once, like `say` and like `play`
// (say-design.md §5.3). Every word must be one of the 41 phoneme names: this
// is the escape hatch for the rules getting a word wrong, so a typo here has
// to be an error rather than silence.
static Result prim_sayphonemes(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_LIST(args[0]);

    LogoIO *io = primitives_get_io();
    LogoHardwareOps *ops = speech_ops();

    for (Node l = mem_first_cell(args[0].as.node); !mem_is_nil(l); l = mem_next_cell(l))
    {
        Node elem = mem_car(l);
        if (!mem_is_word(elem))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(value_list(elem)));
        }
        const char *word = mem_word_ptr(elem);

        int id = speech_phoneme_from_name(word);
        if (id < 0)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, word);
        }

        // Everything but the phoneme is left at its table default; stress and
        // per-phoneme pitch are the front end's to set, and `setvoice`
        // (M3) scales duration engine-side.
        SpeechFrame f = {(uint8_t)id, 0, 0, 1};
        Result r = queue_frame_waiting(io, ops, &f);
        if (r.status != RESULT_NONE)
        {
            return r; // BREAK
        }
    }
    return result_none();
}

// speaking? -> true while an utterance is sounding or still queued.
//
// The same shape as `playing?`, and it composes with `when` the same way:
//   when [not speaking?] [next.taunt]
// which is why there is no blocking WaitForSpeech (say-design.md §5.4).
static Result prim_speaking(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    UNUSED(args);
    if (argc > 0)
    {
        return result_error_arg(ERR_TOO_MANY_INPUTS, NULL, NULL);
    }

    LogoHardwareOps *ops = speech_ops();
    if (!ops || !ops->speech_status)
    {
        return result_ok(value_bool(false)); // silent device: never speaking
    }
    SpeechStatus s = ops->speech_status();
    return result_ok(value_bool(s.speaking || s.free_slots < SPEECH_QUEUE_LEN));
}

void primitives_speech_init(void)
{
    primitive_register("sayphonemes", 1, prim_sayphonemes);
    primitive_register("speaking?", 0, prim_speaking);
    primitive_register("speakingp", 0, prim_speaking);
}
