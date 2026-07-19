//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Sound primitives (P8): the stereo PSG surface -- sound, setenv/env,
//  setwave/wave, play, playing?, stopsound. `toot` stays in
//  primitives_hardware.c but shares the same engine via sound_gate.
//
//  Semantics live here; the device engine renders. Argument validation,
//  the tell-style voice-list fan-out, the note-word parser (core/notation.c)
//  and the queue-full wait are all core; the six device ops (sound_gate,
//  sound_queue, sound_status, sound_stop, sound_env, sound_wave) do the
//  work. Timbre getters (env/wave) read a core-side shadow, since the
//  device ops are fire-and-forget setters. See docs/sound-design.md.
//

#include "primitives.h"
#include "memory.h"
#include "format.h"
#include "error.h"
#include "eval.h"
#include "notation.h"
#include "limits.h"
#include "devices/io.h"

#include <string.h>
#include <strings.h>
#include <stdio.h>

//==========================================================================
// Core-side per-voice timbre shadow (read back by env / wave)
//==========================================================================

typedef struct VoiceEnv
{
    uint32_t attack, decay, release; // ms
    int sustain;                     // 0..15
} VoiceEnv;

static VoiceEnv g_env[MAX_VOICES];
static int g_wave[MAX_VOICES];   // SoundWave
static int g_duty[MAX_VOICES];   // pulse duty 1..99
static int g_volume[MAX_VOICES]; // current immediate-layer volume 0..15

// Noise voices are the last voice of each ear (3 = left, 7 = right).
static bool is_noise_voice(int v)
{
    return v == 3 || v == 7;
}

// Reset the shadow to the engine's defaults. Called from primitives_sound_init
// so every session (and every test) starts from a known timbre, matching the
// device engine's own boot state.
static void sound_shadow_reset(void)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        // Organ-like, click-free default so an unshaped note sounds like the
        // old `toot`, only without the click (docs/sound-design.md §4).
        g_env[i].attack = 5;
        g_env[i].decay = 0;
        g_env[i].sustain = 15;
        g_env[i].release = 30;
        g_wave[i] = is_noise_voice(i) ? SOUND_WAVE_WHITE : SOUND_WAVE_SQUARE;
        g_duty[i] = 50;
        g_volume[i] = 15; // "current volume" for the 3-input `sound`
    }
}

//==========================================================================
// Helpers
//==========================================================================

static LogoHardwareOps *sound_ops(void)
{
    LogoIO *io = primitives_get_io();
    if (io && io->hardware && io->hardware->ops)
    {
        return io->hardware->ops;
    }
    return NULL;
}

// Parse a voice input (a number or a tell-style list of numbers) into an
// ascending, de-duplicated set. Out-of-range numbers error like `tell`.
static bool parse_voice_set(Value v, uint8_t *set, int *count, Result *error)
{
    *count = 0;

    Node list = NODE_NIL;
    bool single = (v.type != VALUE_LIST);
    if (!single)
    {
        list = v.as.node;
    }

    while (true)
    {
        Value item;
        if (single)
        {
            item = v;
        }
        else
        {
            if (mem_is_nil(list))
            {
                break;
            }
            Node node = mem_car(list);
            item = mem_is_word(node) ? value_word(node) : value_list(node);
            list = mem_cdr(list);
        }

        float num;
        if (!value_to_number(item, &num) ||
            num != (float)(int)num || num < 0 || num >= MAX_VOICES)
        {
            *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
            return false;
        }

        int n = (int)num;
        int pos = 0;
        while (pos < *count && set[pos] < n)
        {
            pos++;
        }
        if (pos == *count || set[pos] != n)
        {
            for (int j = *count; j > pos; j--)
            {
                set[j] = set[j - 1];
            }
            set[pos] = (uint8_t)n;
            (*count)++;
        }

        if (single)
        {
            break;
        }
    }

    if (*count == 0)
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
        return false;
    }

    return true;
}

// A getter's single voice: a plain number in range (a list is not allowed).
static bool single_voice(Value v, int *out, Result *error)
{
    float num;
    if (!value_to_number(v, &num) || num != (float)(int)num || num < 0 || num >= MAX_VOICES)
    {
        *error = result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(v));
        return false;
    }
    *out = (int)num;
    return true;
}

static int wave_from_word(const char *w, bool *is_noise)
{
    if (strcasecmp(w, "square") == 0) { *is_noise = false; return SOUND_WAVE_SQUARE; }
    if (strcasecmp(w, "pulse") == 0) { *is_noise = false; return SOUND_WAVE_PULSE; }
    if (strcasecmp(w, "triangle") == 0) { *is_noise = false; return SOUND_WAVE_TRIANGLE; }
    if (strcasecmp(w, "sawtooth") == 0) { *is_noise = false; return SOUND_WAVE_SAWTOOTH; }
    if (strcasecmp(w, "white") == 0) { *is_noise = true; return SOUND_WAVE_WHITE; }
    if (strcasecmp(w, "periodic") == 0) { *is_noise = true; return SOUND_WAVE_PERIODIC; }
    return -1;
}

static const char *word_from_wave(int w)
{
    switch (w)
    {
    case SOUND_WAVE_SQUARE: return "square";
    case SOUND_WAVE_PULSE: return "pulse";
    case SOUND_WAVE_TRIANGLE: return "triangle";
    case SOUND_WAVE_SAWTOOTH: return "sawtooth";
    case SOUND_WAVE_WHITE: return "white";
    case SOUND_WAVE_PERIODIC: return "periodic";
    default: return "square";
    }
}

//==========================================================================
// Immediate layer: sound
//==========================================================================

// sound voice frequency duration
// (sound voice frequency duration volume)
static Result prim_sound(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    if (argc < 3 || argc > 4)
    {
        return result_error_arg(argc < 3 ? ERR_NOT_ENOUGH_INPUTS : ERR_TOO_MANY_INPUTS, NULL, NULL);
    }

    uint8_t voices[MAX_VOICES];
    int nvoices;
    Result err;
    if (!parse_voice_set(args[0], voices, &nvoices, &err))
    {
        return err;
    }

    REQUIRE_NUMBER(args[1], freq_f);
    REQUIRE_NUMBER(args[2], dur_f);
    int dur = (int)dur_f;
    if (dur < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[2]));
    }

    // Out-of-range frequency acts as a rest (docs/sound-design.md Q6).
    int freq = (int)freq_f;
    uint32_t gate_freq = (freq >= 20 && freq <= 10000) ? (uint32_t)freq : 0;

    bool have_vol = (argc == 4);
    int vol = 0;
    if (have_vol)
    {
        REQUIRE_NUMBER(args[3], vol_f);
        vol = (int)vol_f;
        if (vol < 0 || vol > 15)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[3]));
        }
    }

    LogoHardwareOps *ops = sound_ops();
    for (int i = 0; i < nvoices; i++)
    {
        int v = voices[i];
        if (have_vol)
        {
            g_volume[v] = vol; // update this voice's "current volume"
        }
        if (ops && ops->sound_gate)
        {
            ops->sound_gate(v, gate_freq, (uint32_t)dur, g_volume[v]);
        }
    }
    return result_none();
}

//==========================================================================
// Timbre layer: setenv / env, setwave / wave
//==========================================================================

// setenv voice [attack decay sustain release]
static Result prim_setenv(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(2);

    uint8_t voices[MAX_VOICES];
    int nvoices;
    Result err;
    if (!parse_voice_set(args[0], voices, &nvoices, &err))
    {
        return err;
    }

    REQUIRE_LIST(args[1]);
    // Exactly four numbers: attack decay sustain release.
    int vals[4];
    Node l = args[1].as.node;
    for (int i = 0; i < 4; i++)
    {
        if (mem_is_nil(l) || !mem_is_word(mem_car(l)))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
        }
        float num;
        if (!value_to_number(value_word(mem_car(l)), &num) || num < 0)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
        }
        vals[i] = (int)num;
        l = mem_cdr(l);
    }
    if (!mem_is_nil(l) || vals[2] > 15) // extra items, or sustain out of 0..15
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[1]));
    }

    LogoHardwareOps *ops = sound_ops();
    for (int i = 0; i < nvoices; i++)
    {
        int v = voices[i];
        g_env[v].attack = (uint32_t)vals[0];
        g_env[v].decay = (uint32_t)vals[1];
        g_env[v].sustain = vals[2];
        g_env[v].release = (uint32_t)vals[3];
        if (ops && ops->sound_env)
        {
            ops->sound_env(v, g_env[v].attack, g_env[v].decay, g_env[v].sustain, g_env[v].release);
        }
    }
    return result_none();
}

// env voice -> [attack decay sustain release]
static Result prim_env(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    int v;
    Result err;
    if (!single_voice(args[0], &v, &err))
    {
        return err;
    }

    char b[4][32];
    format_number(b[0], sizeof(b[0]), (float)g_env[v].attack);
    format_number(b[1], sizeof(b[1]), (float)g_env[v].decay);
    format_number(b[2], sizeof(b[2]), (float)g_env[v].sustain);
    format_number(b[3], sizeof(b[3]), (float)g_env[v].release);

    Node n = mem_cons(mem_atom(b[3], strlen(b[3])), NODE_NIL);
    n = mem_cons(mem_atom(b[2], strlen(b[2])), n);
    n = mem_cons(mem_atom(b[1], strlen(b[1])), n);
    n = mem_cons(mem_atom(b[0], strlen(b[0])), n);
    return result_ok(value_list(n));
}

// setwave voice "square|pulse|triangle|sawtooth|white|periodic
// (setwave voice "pulse duty)
static Result prim_setwave(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    if (argc < 2 || argc > 3)
    {
        return result_error_arg(argc < 2 ? ERR_NOT_ENOUGH_INPUTS : ERR_TOO_MANY_INPUTS, NULL, NULL);
    }

    uint8_t voices[MAX_VOICES];
    int nvoices;
    Result err;
    if (!parse_voice_set(args[0], voices, &nvoices, &err))
    {
        return err;
    }

    REQUIRE_WORD_STR(args[1], wave_word);
    bool wave_is_noise;
    int wave = wave_from_word(wave_word, &wave_is_noise);
    if (wave < 0)
    {
        return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, wave_word);
    }

    // The waveform kind must match every target voice's kind.
    for (int i = 0; i < nvoices; i++)
    {
        if (wave_is_noise != is_noise_voice(voices[i]))
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, wave_word);
        }
    }

    int duty = -1; // keep each voice's current duty unless given
    if (argc == 3)
    {
        REQUIRE_NUMBER(args[2], duty_f);
        duty = (int)duty_f;
        if (duty < 1 || duty > 99)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(args[2]));
        }
    }

    LogoHardwareOps *ops = sound_ops();
    for (int i = 0; i < nvoices; i++)
    {
        int v = voices[i];
        g_wave[v] = wave;
        if (duty >= 0)
        {
            g_duty[v] = duty;
        }
        if (ops && ops->sound_wave)
        {
            ops->sound_wave(v, wave, g_duty[v]);
        }
    }
    return result_none();
}

// wave voice -> the waveform word
static Result prim_wave(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    REQUIRE_ARGC(1);
    int v;
    Result err;
    if (!single_voice(args[0], &v, &err))
    {
        return err;
    }
    const char *w = word_from_wave(g_wave[v]);
    return result_ok(value_word(mem_atom(w, strlen(w))));
}

//==========================================================================
// Music layer: play, playing?
//==========================================================================

// Queue one event on a voice, waiting (BREAK-able) for space if the queue is
// full. On host (no device ops) this is a no-op that reports success.
static Result queue_event_waiting(LogoIO *io, LogoHardwareOps *ops, int voice, const SoundEvent *ev)
{
    for (;;)
    {
        int accepted = (ops && ops->sound_queue) ? ops->sound_queue(voice, ev, 1) : 1;
        if (accepted >= 1)
        {
            return result_none();
        }
        // Queue full: wait for a slot to drain (docs/sound-design.md Q3).
        if (logo_io_check_user_interrupt(io))
        {
            return result_error(ERR_STOPPED);
        }
        if (ops->sound_status)
        {
            while (ops->sound_status(voice).free_slots == 0)
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

// Compile `notes` for one voice and queue it (append), waiting for space.
static Result play_one_voice(LogoIO *io, LogoHardwareOps *ops, int voice, Node notes)
{
    NotationState state;
    notation_state_init(&state);

    for (Node l = notes; !mem_is_nil(l); l = mem_cdr(l))
    {
        Node elem = mem_car(l);
        if (!mem_is_word(elem))
        {
            // A sublist inside a play list is not a note.
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, value_to_string(value_list(elem)));
        }
        const char *token = mem_word_ptr(elem);

        SoundEvent ev;
        NotationKind kind = notation_parse_token(&state, token, &ev);
        if (kind == NOTATION_ERROR)
        {
            return result_error_arg(ERR_DOESNT_LIKE_INPUT, NULL, token);
        }
        if (kind == NOTATION_CONTROL)
        {
            continue;
        }
        Result r = queue_event_waiting(io, ops, voice, &ev);
        if (r.status != RESULT_NONE)
        {
            return r; // BREAK
        }
    }
    return result_none();
}

// play [notes]
// (play voice [notes])
static Result prim_play(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    if (argc < 1 || argc > 2)
    {
        return result_error_arg(argc < 1 ? ERR_NOT_ENOUGH_INPUTS : ERR_TOO_MANY_INPUTS, NULL, NULL);
    }

    // Voice(s): default 0 when omitted.
    uint8_t voices[MAX_VOICES];
    int nvoices;
    Value notes_arg;
    if (argc == 2)
    {
        Result err;
        if (!parse_voice_set(args[0], voices, &nvoices, &err))
        {
            return err;
        }
        notes_arg = args[1];
    }
    else
    {
        voices[0] = 0;
        nvoices = 1;
        notes_arg = args[0];
    }

    REQUIRE_LIST(notes_arg);
    Node notes = notes_arg.as.node;

    LogoIO *io = primitives_get_io();
    LogoHardwareOps *ops = sound_ops();
    for (int i = 0; i < nvoices; i++)
    {
        Result r = play_one_voice(io, ops, voices[i], notes);
        if (r.status != RESULT_NONE)
        {
            return r;
        }
    }
    return result_none();
}

// playing?           -> true if any voice is sounding or has queued notes
// (playing? voice)    -> ask a single voice
static bool voice_is_active(LogoHardwareOps *ops, int v)
{
    if (!ops || !ops->sound_status)
    {
        return false;
    }
    SoundStatus s = ops->sound_status(v);
    return s.sounding || s.free_slots < SOUND_QUEUE_LEN;
}

static Result prim_playing(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    if (argc > 1)
    {
        return result_error_arg(ERR_TOO_MANY_INPUTS, NULL, NULL);
    }

    LogoHardwareOps *ops = sound_ops();
    if (argc == 1)
    {
        int v;
        Result err;
        if (!single_voice(args[0], &v, &err))
        {
            return err;
        }
        return result_ok(value_bool(voice_is_active(ops, v)));
    }

    for (int v = 0; v < MAX_VOICES; v++)
    {
        if (voice_is_active(ops, v))
        {
            return result_ok(value_bool(true));
        }
    }
    return result_ok(value_bool(false));
}

//==========================================================================
// stopsound
//==========================================================================

// stopsound -- silence every voice through its release and clear the queues.
// Timbre (envelopes/waveforms) is left untouched (docs/sound-design.md Q4).
static Result prim_stopsound(Evaluator *eval, int argc, Value *args)
{
    UNUSED(eval);
    UNUSED(argc);
    UNUSED(args);
    LogoHardwareOps *ops = sound_ops();
    if (ops && ops->sound_stop)
    {
        ops->sound_stop((1u << MAX_VOICES) - 1u); // all voices
    }
    return result_none();
}

//==========================================================================
// Registration
//==========================================================================

void primitives_sound_init(void)
{
    sound_shadow_reset();

    primitive_register("sound", 3, prim_sound);
    primitive_register("setenv", 2, prim_setenv);
    primitive_register("env", 1, prim_env);
    primitive_register("setwave", 2, prim_setwave);
    primitive_register("wave", 1, prim_wave);
    primitive_register("play", 1, prim_play);
    primitive_register("playing?", 0, prim_playing);
    primitive_register("stopsound", 0, prim_stopsound);
}
