//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Speech primitives (P16): say, phonemes, sayphonemes, speaking? and the
//  setvoice/voice pair.
//  `stopsound`'s speech half stays in primitives_sound.c, because there is
//  one "shut up" in the language and it should mean all of it
//  (docs/say-design.md §5.6).
//
//  This is the `play` split applied again: semantics here, rendering in the
//  device. A phoneme word is resolved to a table index by core/speech_synth.c
//  -- the mirror of core/notation.c resolving a note word to a SoundEvent --
//  and the queue-full wait is the one `play` uses, for the same reason.
//
//  `say` and `phonemes` share everything but their last step: both run the
//  text through core/phonemes.c's letter-to-sound rules, and then one queues
//  the phonemes and the other conses them into a Logo list. That shared path
//  is the identity the reference states as the definition (§5.3):
//
//      say :text   is   sayphonemes phonemes :text
//
//  `setvoice`/`voice` keep a core-side shadow of the four knobs so `voice`
//  reads back on a device with no engine, which is `setenv`/`env`'s
//  arrangement exactly (primitives_sound.c).
//

#include "primitives.h"
#include "memory.h"
#include "error.h"
#include "eval.h"
#include "speech_synth.h"
#include "phonemes.h"
#include "format.h"
#include "limits.h"
#include "devices/io.h"

#include <string.h>

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

//==========================================================================
// The front end: text in, phonemes out
//==========================================================================

// What to do with each phoneme the rules produce: `say` queues it,
// `phonemes` conses it onto a list.
typedef Result (*PhonemeSink)(void *ctx, uint8_t id);

// Run a stretch of text through the rules. The engine streams -- it fills a
// small buffer, says where to resume, and keeps its left context across the
// seam because it is handed the whole string every time -- so a sentence of
// any length costs 32 phonemes of stack here and nothing else.
static Result translate_text(const char *text, PhonemeSink sink, void *ctx)
{
    uint8_t buf[32];
    int pos = 0;

    while (text[pos])
    {
        int next = pos;
        int n = phonemes_translate(text, pos, buf, (int)sizeof buf, &next);
        if (next <= pos)
        {
            break; // no progress: nothing left the rules can place
        }
        for (int i = 0; i < n; i++)
        {
            Result r = sink(ctx, buf[i]);
            if (r.status != RESULT_NONE)
            {
                return r;
            }
        }
        pos = next;
    }
    return result_none();
}

// A list is joined back into one stretch of text before the rules see it,
// rather than translated a word at a time, because some rules look across
// the space: " [THE] #" is "the" before a vowel. A list longer than the
// buffer is translated a bufferful at a time, split between words, which is
// the seam `say` already has when a program calls it twice.
typedef struct TextJoin
{
    char buf[SPEECH_TEXT_MAX];
    int len;
    PhonemeSink sink;
    void *ctx;
} TextJoin;

static Result join_flush(TextJoin *j)
{
    if (j->len == 0)
    {
        return result_none();
    }
    j->buf[j->len] = '\0';
    j->len = 0;
    return translate_text(j->buf, j->sink, j->ctx);
}

static Result join_word(TextJoin *j, const char *word)
{
    int n = (int)strlen(word);

    // A word too long to join with anything is already a string of its own.
    if (n + 1 >= (int)sizeof j->buf)
    {
        Result r = join_flush(j);
        return (r.status != RESULT_NONE) ? r : translate_text(word, j->sink, j->ctx);
    }
    if (j->len + n + 1 >= (int)sizeof j->buf)
    {
        Result r = join_flush(j);
        if (r.status != RESULT_NONE)
        {
            return r;
        }
    }
    if (j->len > 0)
    {
        j->buf[j->len++] = ' ';
    }
    memcpy(j->buf + j->len, word, (size_t)n);
    j->len += n;
    return result_none();
}

static Result join_list(TextJoin *j, Node list)
{
    for (Node l = mem_first_cell(list); !mem_is_nil(l); l = mem_next_cell(l))
    {
        Node elem = mem_car(l);
        Result r;
        if (mem_is_word(elem))
        {
            const char *w = mem_word_ptr(elem);
            r = w ? join_word(j, w) : result_error(ERR_OUT_OF_SPACE);
        }
        else
        {
            r = join_list(j, elem); // a nested list is just more words
        }
        if (r.status != RESULT_NONE)
        {
            return r;
        }
    }
    return result_none();
}

// Translate `say`/`phonemes`'s argument -- a number, a word or a list --
// into phonemes.
static Result translate_value(Value v, PhonemeSink sink, void *ctx)
{
    if (v.type == VALUE_NUMBER)
    {
        // `say 42` is "four two", and the digits are the rules' business:
        // all this has to do is spell the number the way `print` does.
        char num[32];
        format_number(num, sizeof num, v.as.number);
        return translate_text(num, sink, ctx);
    }
    if (value_is_word(v))
    {
        const char *w = mem_word_ptr(v.as.node);
        return w ? translate_text(w, sink, ctx) : result_error(ERR_OUT_OF_SPACE);
    }

    if (!value_is_list(v))
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
    }

    TextJoin j;
    j.len = 0;
    j.sink = sink;
    j.ctx = ctx;
    Result r = join_list(&j, v.as.node);
    return (r.status != RESULT_NONE) ? r : join_flush(&j);
}

//==========================================================================
// The primitives
//==========================================================================

typedef struct SayContext
{
    LogoIO *io;
    LogoHardwareOps *ops;
} SayContext;

static Result say_sink(void *ctx, uint8_t id)
{
    SayContext *c = (SayContext *)ctx;
    // Stress 1 and the table's own duration and pitch: the fence in §14 R5
    // is one voice with no sentence prosody, so the rules do not invent one.
    SpeechFrame f = {id, 0, 0, 1};
    return queue_frame_waiting(c->io, c->ops, &f);
}

// say [intruder alert]
// say "chicken
//
// The letter-to-sound rules, then `sayphonemes`. Non-blocking and appending,
// like everything else that makes a sound (§5.1).
static Result prim_say(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    SayContext ctx = {primitives_get_io(), speech_ops()};
    return translate_value(args[0], say_sink, &ctx);
}

typedef struct ListContext
{
    Node head;
    Node tail;
} ListContext;

static Result list_sink(void *ctx, uint8_t id)
{
    ListContext *c = (ListContext *)ctx;
    Node word = mem_atom_cstr(speech_phoneme_names[id]);
    if (mem_is_nil(word) || !mem_list_append(&c->head, &c->tail, word))
    {
        return result_error(ERR_OUT_OF_SPACE);
    }
    return result_none();
}

// show phonemes [hello]  ->  [hh eh l ow]
//
// The rules' output, as a Logo list. It exists so that the words the rules
// get wrong are fixable in Logo rather than reportable as a defect: print
// it, edit it, hand it back to `sayphonemes` (§5.2).
static Result prim_phonemes(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);

    ListContext ctx = {NODE_NIL, NODE_NIL};
    Result r = translate_value(args[0], list_sink, &ctx);
    if (r.status != RESULT_NONE)
    {
        return r;
    }
    return result_ok(value_list(ctx.head));
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

//==========================================================================
// The voice: say-design.md §5.5
//==========================================================================

// Core-side shadow of the four knobs, read back by `voice`. The device is
// told about a change but is never asked what it holds, so a board with no
// speech engine still answers `voice` correctly.
static SpeechVoice g_voice;

static void voice_shadow_reset(void)
{
    g_voice.pitch = SPEECH_VOICE_PITCH_DEFAULT;
    g_voice.speed = SPEECH_VOICE_NOMINAL;
    g_voice.mouth = SPEECH_VOICE_NOMINAL;
    g_voice.throat = SPEECH_VOICE_NOMINAL;
}

// setvoice [64 72 128 128]      ; pitch speed mouth throat
//
// SAM's four knobs, which are the ones that reach a robot in one line. All
// four are 1..255, so this is one rule rather than four, and the shape is
// `setenv [a d s r]`'s.
static Result prim_setvoice(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    REQUIRE_LIST(args[0]);

    int vals[4];
    Node l = mem_first_cell(args[0].as.node);
    for (int i = 0; i < 4; i++)
    {
        if (mem_is_nil(l) || !mem_is_word(mem_car(l)))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
        }
        float num;
        if (!value_to_number(value_word(mem_car(l)), &num) || num < 1 || num > 255)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
        }
        vals[i] = (int)num;
        l = mem_next_cell(l);
    }
    if (!mem_is_nil(l)) // more than four
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[0]));
    }

    g_voice.pitch = (uint8_t)vals[0];
    g_voice.speed = (uint8_t)vals[1];
    g_voice.mouth = (uint8_t)vals[2];
    g_voice.throat = (uint8_t)vals[3];

    LogoHardwareOps *ops = speech_ops();
    if (ops && ops->speech_voice)
    {
        ops->speech_voice(vals[0], vals[1], vals[2], vals[3]);
    }
    return result_none();
}

// voice -> [pitch speed mouth throat]
static Result prim_voice(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    UNUSED(args);
    if (argc > 0)
    {
        return result_error_arg(ERR_TOO_MANY_INPUTS, NULL, NULL);
    }

    char b[4][16];
    format_number(b[0], sizeof(b[0]), (float)g_voice.pitch);
    format_number(b[1], sizeof(b[1]), (float)g_voice.speed);
    format_number(b[2], sizeof(b[2]), (float)g_voice.mouth);
    format_number(b[3], sizeof(b[3]), (float)g_voice.throat);

    Node n = mem_cons(mem_atom(b[3], strlen(b[3])), NODE_NIL);
    n = mem_cons(mem_atom(b[2], strlen(b[2])), n);
    n = mem_cons(mem_atom(b[1], strlen(b[1])), n);
    n = mem_cons(mem_atom(b[0], strlen(b[0])), n);
    return result_ok(value_list(n));
}

void primitives_speech_init(void)
{
    voice_shadow_reset();

    primitive_register("say", 1, prim_say);
    primitive_register("phonemes", 1, prim_phonemes);
    primitive_register("sayphonemes", 1, prim_sayphonemes);
    primitive_register("speaking?", 0, prim_speaking);
    primitive_register("speakingp", 0, prim_speaking);
    primitive_register("setvoice", 1, prim_setvoice);
    primitive_register("voice", 0, prim_voice);
}
