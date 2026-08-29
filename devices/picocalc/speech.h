//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  PicoCalc speech back end (P16 M3). The formant synthesis itself is
//  core/speech_synth.c -- pure math, no device, and therefore host-testable;
//  this is the thin wrapper that gives it a place to run: one engine, the
//  utterance queue serialised against the refill IRQ, and a block of samples
//  handed to the mixer as its ninth and tenth source (docs/say-design.md
//  §8.5). It is deliberately not a ninth PSG voice: a `Voice` is a phase
//  accumulator plus an ADSR and a formant synthesizer shares none of that.
//
//  These are the backends for the LogoHardwareOps speech_* ops; the picocalc
//  hardware table wires them in.
//

#pragma once

#include "devices/hardware.h" // SpeechFrame, SpeechStatus

#include <stdint.h>

// Place the engine for the mixer's current rate. Called from sound_init,
// before any speech op.
void speech_init(uint32_t sample_rate_hz);

// Re-derive every resonator coefficient for a new mix rate. Every one of
// them is a function of the sample rate, so without this `hw.setcpu "fast`
// would retune the music and leave the voice speaking at half pitch
// (say-design.md §8.4). Called from sound_reclock.
void speech_reclock(uint32_t sample_rate_hz);

// LogoHardwareOps speech_* backends (see devices/hardware.h for contracts).
int speech_queue(const SpeechFrame *frames, int n);
SpeechStatus speech_status(void);
void speech_stop(void);
void speech_voice(int pitch, int speed, int mouth, int throat);

// IRQ context: render the next `frames` samples of the utterance and return
// them, or NULL when nothing is being said -- which is the common case, and
// the one the mixer should pay nothing for.
const int16_t *speech_mix_block(int frames);
