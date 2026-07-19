//
//  Pico Logo
//  Copyright 2026 Blair Leduc. See LICENSE for details.
//
//  PicoCalc software PSG synthesizer (P8). Replaces the PIO square-wave
//  driver (audio.c/audio.pio). Eight voices -- three tone + one noise per
//  stereo ear -- are mixed in software on core 0 inside the DMA IRQ and
//  output through hardware PWM slice 5 (GPIO 26/27) as a DAC. See
//  docs/sound-design.md §6.
//
//  These are the backends for the LogoHardwareOps sound_* ops; the picocalc
//  hardware table wires them in.
//

#pragma once

#include "devices/hardware.h" // SoundEvent, SoundStatus

// Bring up the PWM + DMA output path and the mixer/sequencer. Call once at
// boot (from picocalc.c), before any sound op.
void sound_init(void);

// LogoHardwareOps sound_* backends (see devices/hardware.h for contracts).
void sound_gate(int voice, uint32_t freq_hz, uint32_t dur_ms, int vol);
int sound_queue(int voice, const SoundEvent *events, int n);
SoundStatus sound_status(int voice);
void sound_stop(uint32_t voice_mask);
void sound_env(int voice, uint32_t attack_ms, uint32_t decay_ms, int sustain, uint32_t release_ms);
void sound_wave(int voice, int wave, int duty);
