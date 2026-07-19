//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  Note-word notation parser for the `play` music sequencer (P8).
//
//  A pure, streaming tokenizer: `play` walks its note list and hands each
//  word to notation_parse_token(), which either emits one resolved
//  SoundEvent (a note or a rest) or updates the running tempo/octave/
//  length/volume state (an in-list control word). Tempo, octave, length and
//  dots are resolved here, so the device sequencer queue holds nothing but
//  gates. See docs/sound-design.md §5.4/§7.
//

#pragma once

#include "devices/hardware.h" // SoundEvent

#ifdef __cplusplus
extern "C"
{
#endif

    // Running notation state. Defaults (notation_state_init): tempo 120
    // (quarter-notes/minute), octave 4, length 4 (quarter note), volume 12.
    typedef struct NotationState
    {
        int tempo;  // 40..300 quarter-notes per minute
        int octave; // 1..8 default octave
        int length; // 1..32 default note length (1 = whole, 4 = quarter)
        int volume; // 0..15
    } NotationState;

    typedef enum NotationKind
    {
        NOTATION_NOTE,    // *out holds a resolved note or rest
        NOTATION_CONTROL, // token updated the state; *out untouched
        NOTATION_ERROR,   // malformed token
    } NotationKind;

    // Reset state to the notation defaults.
    void notation_state_init(NotationState *s);

    // Parse one note-list word. A note is [len]letter[#|s|b][octave][.];
    // a rest is [len]r[.]; a control is t/o/l/v followed by digits. On
    // NOTATION_NOTE, *out is filled (rest => freq_hz 0). Case-insensitive.
    NotationKind notation_parse_token(NotationState *s, const char *token, SoundEvent *out);

#ifdef __cplusplus
}
#endif
