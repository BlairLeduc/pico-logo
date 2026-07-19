# P8 — Sound: a stereo PSG synthesizer (design)

Status: **v1.1 — gate closed, M1–M3 implemented** — this is the design
gate for
[P8 in the improvements roadmap](improvements-roadmap.md#p8--sound-stereo-psg-synthesizer-design-first).
All open questions (§11) were resolved with the user on 2026-07-10.
Hardware bring-up (2026-07-18/19) amended the engine constants and
surfaced three latency findings — see §12; the body text reflects the
as-built numbers.

Three decisions were made by the user before this draft (2026-07-10):

1. **ADSR envelopes** per voice (not the simpler AY-style decay-only).
2. **Core 0, DMA-IRQ rendering** — no core 1 for now. The engine is a
   self-contained module fed by a queue, so a later move to core 1 is a
   relocation, not a redesign.
3. **Four voices per stereo channel: 3 tones + 1 noise** — eight voices
   total, each bound to one ear. This is exactly the layout of the
   TI/Sega SN76489 sound chip, doubled — the same chip family TI Logo II
   and the BBC Micro drove, so the prior art in §3 maps directly.

---

## 1. What limits sound today

The current PicoCalc driver (`devices/picocalc/audio.c` + `audio.pio`)
uses one PIO state machine per ear as *the oscillator*: the SM counts out
a fixed-duty square wave at the requested frequency (100–2000 Hz). That
architecture is why sound is stuck where it is:

- **One tone per ear, square only.** The SM *is* the voice; there is no
  mixing point where a second voice could be added.
- **No volume, no envelope.** Duty cycle is fixed; every note is a flat
  full-amplitude gate. (The PIO program reserves the OSR for duty, but a
  duty change alters timbre, not loudness, without a filter model.)
- **No noise.** Explosions, drums, and hisses — the sounds games want
  most — cannot be approximated by two square waves.
- **`toot` is the whole surface.** The roadmap's `play [notes]` item,
  built on this driver, could only ever be a melody of beeps.

The fix is to invert the architecture: synthesize samples in software
(the RP2350 has cycles to spare) and use dumb, fast hardware PWM as a
DAC. §6 details the engine; the PIO program is deleted entirely.

## 2. The output hardware

The PicoCalc routes GPIO 26 (left) and GPIO 27 (right) through an RC
low-pass filter into the amplifier. Two facts make the output stage
almost free:

- GPIO 26 and 27 are channels **A and B of the same PWM slice** (slice
  5). One slice clocks both ears sample-locked, and the two compare
  registers share one 32-bit word — a single DMA write updates left and
  right together.
- The slice's wrap event is a DMA pacing signal (DREQ), so a DMA channel
  can stream compare values from a ring buffer with zero CPU per sample.

At the 150 MHz system clock, a wrap of 2048 gives a **73.2 kHz carrier**
(ultrasonic; the filter and speaker never pass it) with 11-bit sample
resolution. Every mixed stereo frame is written twice, giving a
**36.6 kHz mix rate** — an 18.3 kHz Nyquist limit, comfortably above the
speaker and the o1–o8 note range in §5. (The draft's ×3 oversample was
changed to ×2 during bring-up when the ring became a power of two —
§12.1.)

## 3. Prior art

Survey scope: the Logo family first (period dialects on real 80s sound
chips, then modern descendants), plus the adjacent 8-bit BASIC surfaces
that solved the same "expose a PSG to beginners" problem — several of
them on the *same* SN76489-family chip we are modelling.

### 3.1 Pico Logo today

[`toot`](../reference/Pico_Logo_Reference.md#toot): `toot duration
frequency`, `(toot duration leftfrequency rightfrequency)`. Non-blocking;
a second `toot` waits for the first to finish; frequencies outside
100–2000 Hz act as a rest. These semantics are load-bearing (reference
§toot) and are preserved verbatim in this design.

### 3.2 Apple Logo II (the model dialect)

Silent. The Apple II had a click speaker and Apple Logo II exposed no
sound primitives at all — `toot` is already a Pico Logo extension, so
this design owes no compatibility debt to the model dialect.

### 3.3 Atari Logo (1983, POKEY chip)

The strongest period precedent for the *immediate* layer
([Antic v2n8, "Sweet Toots"](https://www.atarimagazines.com/v2n8/);
[Wikipedia](https://en.wikipedia.org/wiki/Atari_Logo)):

- `TOOT voice freq vol dur` — voice 0 or 1, frequency **14–9000 Hz**,
  volume **0–15**, duration in 60ths of a second. Four inputs: the voice
  is addressed explicitly, and volume is per-note.
- **Background play**: "TOOT will play each note the exact time you give
  it while your program is off doing something else."
- **`SETENV`** — a per-voice envelope command (decay shaping), giving
  notes a percussive or sustained character. Atari Logo is the direct
  ancestor of this design's `setenv`; we extend decay-only to full ADSR
  (decision 1).

### 3.4 TI Logo II (1983, TMS9919/SN76489 — 3 tones + noise)

The strongest precedent for the *music* layer, and it ran on precisely
the chip model we are doubling. From Chapter 9 of the
[TI Logo manual](https://archive.org/details/tibook_ti-logo) (music by
way of Jeanne Bamberger's MIT work):

| Command | Semantics |
|---|---|
| `MUSIC [pitches] [durations]` | Append notes to a **music buffer**; chromatic pitch numbers, middle C = 0, range −15…24; durations in time units |
| `PLAYMUSIC` / `PM` | Play the buffer **in the background** — "you can execute other Logo commands while music is playing" |
| `LOOPMUSIC` | Same, repeating forever |
| `SETVOICE 1–4` | Voice for subsequent notes; **voice 4 is the noise generator**; clears the buffer |
| `NOTE dur pitch vol` | Single note with explicit volume |
| `REST dur` | Silence in the stream |
| `SETVOLUME 0–15` | Default note volume, **2 dB per step** |
| `SETTEMPO t` | A duration-`d` note lasts (60/t)·d seconds (default 300) |
| `STACCATO` / `LEGATO` | Inter-note gap policy |
| `DRUM [durations]` | Rhythm track; on voice 4 uses the noise generator |
| `PLAYNOTE` | Sync primitive: play the next buffered note and wait its duration (used to beat sprite wings to the music) |
| `MAJOR` / `CHROMATIC` | Pitch-number interpretation |

Buffer overflow errors with `OUT OF NOTES`. Design ideas taken: the
append-to-a-queue model (which enabled Bamberger's *tuneblocks* — tunes
built by composing procedures that each contribute a phrase), background
playback, per-ear noise as "voice 4", the 2 dB volume ladder, and
rests-in-the-stream for multi-voice rounds. Design ideas rejected: the
separate pitch/duration lists (opaque; see Q2) and the load-then-play
two-phase (our `play` starts immediately — §5.4).

### 3.5 LogoWriter (LCSI, 1986)

`tone frequency duration` — two inputs, one voice, foreground. The
floor, not the ceiling; noted for completeness
([Teacher's Manual](https://archive.org/details/logowriterteachersmanual)).

### 3.6 Terrapin Logo (modern)

[`PLAY`](https://resources.terrapinlogo.com/weblogo/manual/music) takes a
note list in compact word notation: note letters `c`–`b`, `#`/`b`
suffixes for accidentals, octave digit suffix (`c5`), duration *prefix*
where 4 = quarter note (`8g` is an eighth-note G), dotted notes, `P`
rests, and in-list control words — `T240` tempo, `O5` octave, `L8`
default length. This MML-derived notation (shared with MSX/IBM BASIC's
`PLAY` strings) is the model for our note words in §5.4, adapted to
Logo-lexable words.

### 3.7 Adjacent non-Logo surfaces

- **BBC BASIC** (SN76489, same chip): `SOUND chan, amp, pitch, dur` plus
  `ENVELOPE n, …` — fourteen parameters defining pitch *and* amplitude
  envelopes (attack/decay/sustain/release rates and targets). Proof that
  full ADSR was teachable to schoolchildren on this exact chip; also
  proof that a 14-input primitive is hostile — our `setenv` takes a
  four-element list.
- **Atari BASIC**: `SOUND voice, pitch, distortion, volume` — the
  "distortion" input selected noise polynomial modes; precedent for
  noise being just another voice with a mode knob.
- **MSX BASIC** (AY-3-8910): `PLAY "c d e"` — up to three MML strings,
  one per voice, played together; precedent for per-voice note lists as
  the harmony mechanism.
- **Scratch** (Logo's descendant): `play note 60 for 0.5 beats`,
  `set instrument`, `set tempo`, `rest` — confirms the modern pedagogical
  consensus: notes + named timbres + tempo, everything non-blocking.

### 3.8 What this design takes

| From | Taken |
|---|---|
| Atari Logo | Explicit voice input, per-note volume, background play, `setenv` (name and idea), wide frequency range |
| TI Logo II | 3-tone+noise voice model, append/queue semantics, rests for rounds, 2 dB volume steps, background sequencer, "out of notes" overflow story |
| Terrapin/MML | Note-word notation with duration prefix, octave suffix, in-list `t`/`o`/`l`/`v` controls |
| BBC BASIC | ADSR as the envelope model (simplified to one 4-element list) |
| Scratch | Everything non-blocking by default; simple mental model |

## 4. The instrument

Two identical synthesized PSGs, one per ear:

| Voice | Ear | Kind |
|---|---|---|
| 0, 1, 2 | left | tone |
| 3 | left | noise |
| 4, 5, 6 | right | tone |
| 7 | right | noise |

(Numbering is 0-based to match turtles — one rule to remember: left is
0–3, right is 4–7, noise is the last voice of each ear. Resolved as Q1.)

Everywhere a primitive takes a *voice* input (`sound`, `play`, `setenv`,
`setwave`, and their getters' set-forms), it accepts either a voice
number or a **list of voice numbers**, fanning the command out
`tell`-style (resolved as Q1b): `(play [0 4] [c d e c])` plays a melody
centred in both ears; `(sound [3 7] 6000 80)` fires both noise channels
for a big stereo explosion. Getters take a single voice only.
Out-of-range numbers error with `ERR_DOESNT_LIKE_INPUT`, matching
`tell`.

Per tone voice:

- **Waveform**: `square` (default), `pulse` (variable duty 1–99 %),
  `triangle`, `sawtooth`. Phase-accumulator oscillators.
- **Frequency**: 20 Hz – 10 kHz continuous (not quantized to a chip's
  divider ladder — one place we deliberately out-do the originals;
  range resolved as Q6, out-of-range acts as a rest).
- **ADSR envelope**: attack ms, decay ms, sustain level 0–15, release
  ms. Defaults to an organ-like `[5 0 15 30]` so unshaped notes sound
  like today's `toot`, only click-free.
- **Volume** 0–15 on the TI/Atari 2 dB-per-step ladder (a 16-entry
  table; level 15 ≈ full scale, level 1 ≈ −28 dB).

Per noise voice: the same envelope and volume machinery driving a
16-bit LFSR instead of an oscillator. `white` (default) and `periodic`
(SN76489's second mode — a buzzy, pitched rasp) selected by `setwave`.
The "frequency" input clocks the LFSR, so noise is *pitched*: low
values rumble (explosions, toms), high values hiss (snares, steam).
This mirrors TI Logo's "pitches on voice 4 pick different sounds".

Mixing: the three tones and one noise of each ear are envelope-scaled,
summed with 2-bit headroom, and saturated — four simultaneous
full-volume voices cannot wrap.

## 5. The Logo surface

### 5.1 `toot` — unchanged

`toot` keeps its reference semantics exactly (§toot: non-blocking, one
pending toot waits, 100–2000 Hz else rest). It is reimplemented as a
square wave at volume 15 with an instant envelope on voices 0 and 4.
The existing reference section does not change; the compatibility tests
in M1 hold the line.

### 5.2 Immediate layer — `sound`

For game effects: gate a voice now, return immediately.

```logo
sound 0 440 500              ; voice 0, 440 Hz, 500 ms, current volume
(sound 3 6000 80 12)         ; noise burst: voice 3, fast LFSR, 80 ms, vol 12
```

`sound voice frequency duration` / `(sound voice frequency duration
volume)`. Non-blocking; the ADSR shapes the note (release runs after
`duration`). Calling `sound` on a voice that is playing queued music
flushes that voice's queue — the effect wins, predictably. Frequency 0
is a rest (gates the voice off through its release), matching `toot`'s
rest convention.

A laser zap is then two lines of Logo, not a driver feature:

```logo
to zap
  setenv 1 [0 60 0 40]
  sound 1 1800 90
end
```

### 5.3 Timbre layer — `setenv`, `setwave`

```logo
setenv voice [attack decay sustain release]   ; ms ms 0-15 ms
env voice                                     ; outputs the list

setwave voice "square | "pulse | "triangle | "sawtooth   ; tone voices
(setwave voice "pulse duty)                              ; duty 1-99
setwave voice "white | "periodic                         ; noise voices
wave voice                                    ; outputs the word
```

Getter/setter pairing follows the house `setspeed`/`speed`,
`setrefresh`/`refreshmode` pattern. `setwave` with a tone waveform on a
noise voice (or vice versa) errors with `ERR_DOESNT_LIKE_INPUT`.

### 5.4 Music layer — `play`

The roadmap's `play [notes]` item, upgraded from "melody of beeps" to
the sequencer this engine deserves.

```logo
play [c d e c]                       ; Frère Jacques, phrase 1, voice 0
(play 1 [e f 2g])                    ; phrase 2, half-note g, on voice 1
```

**Notation** (one list of Logo words; grammar in §7 is parsed in core):

- **Note**: `[duration] letter [#|s|b] [octave] [.]` — duration prefix
  where 4 = quarter (`8g` eighth-note G, default from `l`), `#` or `s`
  for sharp and `b` for flat — both accepted, `#` canonical in the
  reference (`c#`/`cs` C♯, `db` D♭; `#` inside words is lexer-verified
  on the host; resolved as Q2a) — octave suffix digit 1–8 (default from
  `o`), trailing `.` dots the length. `8g#5.` is a dotted-eighth G♯ in
  octave 5. This is the only pitch notation; TI-style numeric lists
  were considered and rejected (resolved as Q2b, §10).
- **Rest**: `[duration] r [.]` — `2r` is a half rest.
- **Controls** (apply from that point in the list): `t40`–`t300` tempo
  in quarter-notes/minute (default `t120`), `o1`–`o8` default octave
  (default `o4`), `l1`–`l32` default length (default `l4`), `v0`–`v15`
  volume (default `v12`).

**Semantics**:

- `play` is **non-blocking**: notes are compiled into the voice's event
  queue and playback starts at once; Logo runs on (TI `PM`, Atari
  background toots).
- `play` on an already-playing voice **appends** — Bamberger's
  tuneblocks compose exactly as they did on the TI:

  ```logo
  to f1  play [c d e c]  end
  to f2  play [e f 2g]  end
  to f3  play [8g. 16a 8g 8f e c]  end
  to f4  play [c g3 2c]  end
  to frere  f1 f1 f2 f2 f3 f3 f4 f4  end
  ```

- **Rounds** use rests and per-voice lists, replacing TI's
  `SETVOICE`+`REST` dance:

  ```logo
  frere                                ; voice 0 leads
  (play 1 [1r 1r])  (play 1 ...)       ; voice 1 enters two bars late
  (play 3 [l4 c3 8c5 8c5])             ; noise drum: boom-cha-cha
  ```

- If a play list needs more queue slots than the voice has free, `play`
  **waits for space** (interruptible by BREAK) — the same
  wait-don't-error contract as a second `toot`, so long songs stream
  instead of dying with TI's `OUT OF NOTES` (resolved as Q3).

**Sensing**: `playing?` outputs `true` if any voice is sounding or has
queued notes; `(playing? voice)` asks one voice. With the existing demon
machinery this subsumes TI's `PLAYNOTE` synchronization:

```logo
when [not playing?] [next.verse]
```

**Stopping**: `stopsound` (resolved as Q4) silences all eight voices
through their releases and clears every queue. It does **not** reset
envelopes or waveforms — timbre the user deliberately set survives.

### 5.5 Lifetime

Rules (resolved as Q5): music keeps playing at the toplevel prompt (TI
precedent, and consistent with demons staying armed); BREAK stops all
sound and clears queues (it already stops `toot` songs); a toplevel
error unwind also silences (nobody wants the crash accompanied by a
stuck chord); `cs` does *not* touch sound (it is a graphics command).

## 6. The engine

All rendering happens on core 0 inside the DMA IRQ (decision 2).

- **Output**: PWM slice 5, wrap 2048 → 73.2 kHz carrier, 11-bit. Two
  DMA channels, paced by the slice DREQ and chained to each other,
  ping-pong 32-bit L|R compare pairs through a two-half ring buffer;
  each channel's completion IRQ refills its half. The ring is a
  power-of-2, aligned **2 KB** (2 × 256 slots × 4 B; each mixed frame
  written ×2 → 128 stereo frames per half ≈ 3.5 ms), with a hardware
  read-address ring-wrap so a starved IRQ replays the ring instead of
  reading past it (§12.1).
- **Mixer** (per half-buffer refill): for each of 8 voices — advance
  the 32-bit phase accumulator (or LFSR), look up/fold the waveform,
  scale by envelope × volume (int multiply), accumulate; saturate; ×2
  duplicate-store. Integer math throughout; per-voice envelopes advance
  in linear segments once per block (3.5 ms granularity — inaudible for
  ms-scale ADSR stages).
- **Sequencer**: runs at block boundaries in the same IRQ. Per-voice
  event queues of `{freq_hz u16, dur_ms u16, vol u8, flags u8}` (6 B) —
  tempo, octave, length, and dots are resolved by the parser at `play`
  time, so the queue holds nothing but gates. Timing granularity is
  3.5 ms, versus the 20 ms demon poll — this is why the sequencer lives
  in the audio callback and not in a demon.
- **CPU cost**: ~35 cycles/voice/frame × 8 voices × 36.6 kHz ≈ 10 M
  cycles ≈ **7 % of core 0**, in interrupt context, regardless of
  what Logo is doing. The old PIO driver's `pio0` state machines are
  freed.
- **IRQ latency budget**: the refill must run within 3.5 ms of a half
  draining, so the audio IRQ runs above the default NVIC priority and
  nothing else may mask interrupts for longer than that (§12.2, §12.3).
- **Flash-write caveat**: littlefs writes run with IRQs masked, so the
  refill IRQ is delayed and the DMA ring replays the last 3.5 ms half
  until the write completes — a brief stutter during `save` while music
  plays. Accepted (the alternative is core 1, deferred by decision 2;
  moving there trades the stutter for a `multicore_lockout` pause of
  similar size).

## 7. Core/device boundary and testing

The split follows the sprite work: semantics in core, hardware in the
device.

**In core** (`core/primitives_sound.c` + `core/notation.c`):
argument validation, the note-word parser (pure function: list →
event array — fully unit-testable on the host), envelope/wave state
bookkeeping for the getters, queue-full waiting with the BREAK check.

**Device ops** (extend `hardware->ops`; picocalc implements, host leaves
NULL → primitives silently succeed, exactly like `toot` today):

| Op | Purpose |
|---|---|
| `sound_gate(voice, freq, dur_ms, vol)` | Immediate note/rest (also `toot`'s backend) |
| `sound_queue(voice, events, n)` → accepted count | Append to a voice queue |
| `sound_status(voice)` → `{sounding, free_slots}` | Backs `playing?` and queue waiting |
| `sound_stop(mask)` | Release + flush |
| `sound_env(voice, a, d, s, r)` / `sound_wave(voice, wave, duty)` | Timbre |

**Mock** (`tests/mock_device.*`): records every op with arguments and
timestamps; scripted `sound_status` lets tests exercise the queue-full
wait path without hardware. Tests: parser (every notation form, error
cases, tempo math), primitive validation, toot-compatibility (same op
sequence as today's semantics), queue append/flush, `playing?`.
E2e golden scripts: a `play` melody's mock event log.

**Reference**: new sections (`sound`, `play`, `setenv`, `env`,
`setwave`, `wave`, `playing?`, `stopsound`) per the recurring checklist —
each is the `help` text; `toot`'s section gains only a cross-reference.

## 8. Memory budget

New fixed capacities, all in `core/limits.h`:

| Item | Size | Notes |
|---|---|---|
| DMA ring buffer | 2,048 B | 2 × 256 × 4 B, power-of-2 aligned for the ring-wrap (§6, §12.1) |
| Event queues | 3,072 B | `SOUND_QUEUE_LEN 64` events × 8 voices × 6 B |
| Voice + envelope state | ~200 B | 8 × ~24 B |
| Volume/waveform tables | flash | const, no SRAM |
| **Total SRAM** | **≈ 5.3 KB** | vs ~10 % headroom noted in the sprite design |

`SOUND_QUEUE_LEN 64` ≈ 16 bars of eighth notes per voice before `play`
has to wait; halving it to 32 saves 1.5 KB if link-time SRAM pressure
demands (the overflow story — waiting, not erroring — is unaffected).
The old driver's alarm/PIO state is retired against this.

## 9. Milestones

- **M1 — Engine swap.** PWM+DMA output path, 8 voices, ADSR, noise,
  mixer; `toot` rerouted through `sound_gate`; `audio.pio` and the alarm
  machinery deleted; toot-compat tests green; hardware A/B listen on the
  PicoCalc (no new Logo surface yet — independently shippable).
- **M2 — Immediate + timbre surface.** `sound`, `setenv`/`env`,
  `setwave`/`wave`, `stopsound`; device ops in the mock; reference
  sections; unit tests.
- **M3 — Music.** Notation parser, per-voice queues, `play`,
  `playing?`, queue-wait semantics; e2e golden song; Frère Jacques round
  as the acceptance demo.
- **M4 — Game validation.** Retrofit Space Invaders (#101) with real
  effects (noise explosions, descending alien tones) and score the
  Galaxian design's sound needs against the surface — the end-to-end
  exercise, as the Space Invaders game was for sprites.

## 10. Rejected alternatives

- **PIO as the synthesizer** (status quo extended): the SM-per-tone
  model has no mixing point; more SMs would buy more square waves, not
  volume, envelopes, or noise.
- **Core 1 rendering**: deferred by decision 2; the module boundary
  (§6) keeps it a relocation later. Revisit only if the flash-write
  stutter proves objectionable in practice.
- **Full SID/POKEY register-level emulation**: we control the Logo
  surface, so emulating a specific chip's register map and quirks buys
  authenticity nobody asked for at real complexity cost.
- **MIDI note numbers as the pitch surface** (Scratch's choice):
  `play [60 64 67]` is opaque next to `play [c e g]`; note words won.
- **TI-style numeric pitch/duration list pair** as a second input form
  (`(play 0 [0 2 4 0] [4 4 4 4])`, Bamberger pedagogy): two notations
  to document, test, and teach; rejected with Q2b — note words only.
- **TI's two-phase load-then-play** (`MUSIC` … `PM`): append-on-play
  keeps one primitive and still supports tuneblocks and rounds.
- **An I2S DAC path**: the PicoCalc has no I2S DAC; PWM is the
  hardware.

## 11. Resolved questions (user, 2026-07-10)

All six were resolved in review; the body text above reflects them.

- **Q1 — Voice map: by ear.** 0–2 tone + 3 noise left, 4–6 tone +
  7 noise right. Rejected: kind-grouped (tones 0–5, noise 6–7) and
  TI-style 1-based numbering.
- **Q1b — Voice lists fan out.** Voice inputs accept a number or a
  list, `tell`-style: `(play [0 4] […])` centres a melody,
  `(sound [3 7] …)` is a stereo explosion. Getters stay single-voice.
- **Q2a — Accidentals: both `#` and `s` accepted** for sharp (`b` for
  flat); `#` is canonical in the reference. `+`/`-` rejected (operator
  collision).
- **Q2b — Note words only,** duration prefix + octave suffix
  (`8g` / `g5` / `8g#5.`). TI numeric pitch lists rejected (§10).
- **Q3 — Queue full: `play` waits for space,** interruptible by BREAK
  (the second-`toot` contract). Erroring rejected.
- **Q4 — `stopsound`, stop-only.** Silences all voices through their
  releases and clears queues; envelope/waveform state survives.
  Rejected names: `quiet`, `silence`; rejected scope: full timbre
  reset.
- **Q5 — Lifetime as §5.5:** keeps playing at the prompt; BREAK and
  toplevel error-unwind silence everything; `cs` doesn't touch sound.
- **Q6 — `sound` range 20 Hz–10 kHz,** out-of-range acts as a rest;
  `toot` keeps its documented 100–2000 Hz convention. Rejected:
  Atari-exact 14–9000 Hz and status-quo 100–2000 Hz.

## 12. Hardware bring-up findings (2026-07-18/19)

First-board bring-up surfaced three latency problems the draft did not
anticipate. The common thread: the refill IRQ has a hard deadline of one
ring half (**3.5 ms**), and anything that delays it past that deadline
is audible. Each fix below hardened a different layer.

### 12.1 DMA read ring-wrap (engine, 2026-07-18)

**Symptom**: periodic clicks at idle (cursor blink) and a burst of
static on an F1–F3 screen switch. **Cause**: the first engine reset the
DMA read address *in the IRQ*; when the IRQ was masked past a half
draining, the chained DMA marched past the ring into adjacent RAM and
played garbage. **Fix**: the ring is power-of-2 sized and aligned, with
a hardware read-address wrap (`channel_config_set_ring`), so a starved
DMA cleanly *replays* the ring — silence at idle, a brief stutter under
a note — instead of reading garbage. This forced `SOUND_RING_HALF`
192 → 256 and OVERSAMPLE 3 → 2 (mix rate 24.4 → 36.6 kHz), the numbers
now in §2/§6/§8. The wrap is the engine's last-resort safety net; the
findings below remove the starvation itself.

### 12.2 LCD driver no longer masks interrupts (2026-07-19)

**Symptom**: audible stutter under a note during any large screen
update (a full redraw holds ~22 ms at 75 MHz SPI). **Cause**: the LCD
driver wrapped every screen operation in
`save_and_disable_interrupts()` — a reentrancy lock whose only purpose
was that the cursor-blink repeating timer drew to the LCD from IRQ
context. **Fix**: the blink timer now only sets a flag;
`lcd_cursor_blink()` services it from the keyboard wait loop, and all
interrupt masking was deleted from `lcd.c`. Invariant, documented at
the top of `lcd.c`: **the LCD is only ever touched from thread
context** — the ST7789P tolerates SPI clock pauses mid-window, so the
audio IRQ preempts blits freely. Never call `lcd_*` from an interrupt
handler.

### 12.3 Audio IRQ priority above default (2026-07-19)

**Symptom**: with 12.2 fixed, a sustained `toot` still clicked ~10×/s.
**Cause**: the keyboard poll timer blocks ~4–5 ms on the 10 kHz
southbridge I2C bus *inside the timer IRQ* every 100 ms, and
same-priority NVIC interrupts cannot preempt each other — every poll
made the refill miss its 3.5 ms deadline. **Fix**: `sound_init()` sets
`DMA_IRQ_0` to priority `0x40`, above the default `0x80`, so the refill
preempts the poll (the I2C peripheral runs autonomously and is
unharmed). Sound is the only `DMA_IRQ_0` user; the LCD DMA polls for
completion and raises no IRQ. **Rule for future device code**: any
handler that can run longer than 3.5 ms at default priority will click —
either keep handlers short (12.2's approach) or stay below the audio
IRQ's priority. Flash program/erase remains the one accepted violator
(§6 flash-write caveat).
