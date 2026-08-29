# P16 — `say`: a speech synthesizer (design)

Status: **M0 and M1 built** (2026-08-28), both listening gates passed.
`core/speech_synth.c` is the whole 41-phoneme engine and `sayphonemes` /
`speaking?` are live on the host; M2's rule engine, and M3's board
integration, are not.

[Berzerk §14.3](berzerk-design.md) asked for this and deliberately did not
block on it: emulating the cabinet's speech part is the wrong target because
*the voice is in Stern's ROM, not in the decoder*, and that ROM cannot live in
an MIT repository. The alternative it named — a formant synthesizer and a `say`
primitive — is this document. Berzerk is the demonstrated need; the primitive
is for every Logo program.

The output path already exists. [sound-design.md](sound-design.md) §6 built a
software PSG that mixes eight voices in the DMA refill IRQ and pushes them
through PWM slice 5. **Speech is a ninth source into that mixer.** There is no
new hardware question anywhere in this document, which is why it is mostly
about phonetics and licences.

---

## 1. Deliverables

| | |
|---|---|
| Front end | `core/phonemes.c` / `.h` — English text → phoneme list. Pure, host-testable, the mirror of `core/notation.c` |
| Back end | `core/speech_synth.c` / `.h` — the resonators and the phoneme table; pure math, no device, host-testable (built at M0, below). `devices/picocalc/speech.c` / `.h` (M3) is the thin IRQ wrapper: block-boundary parameter updates, the mixer slot, `sound_reclock`. See the M0 note in §12 for why the split moved here from the single `devices/` file first sketched |
| Surface | `core/primitives_speech.c` — `say`, `sayphonemes`, `phonemes`, `speaking?`, `setvoice`/`voice`; `stopsound` extended |
| Tests | `tests/test_phonemes.c`, `tests/test_speech_synth.c`, `tests/test_primitives_speech.c` |
| Reference | six new sections in `# The Outside World`, beside `play` and `stopsound` |
| Design | this document |

All three boards. Speech needs neither a radio nor PSRAM, so unlike the network
tiers there is nothing to gate on `LOGO_HAS_WIFI` / `LOGO_HAS_TLS`.

## 2. Prior art

Survey scope as in [sound-design.md](sound-design.md) §3: the Logo family
first, then the 8-bit neighbours that solved the same problem, then the arcade
parts themselves.

### 2.1 Pico Logo today

Silent. There is no `say`, no `speak`, and nothing in the reference that
produces a voice — `toot`, `sound` and `play` are the whole audio surface. The
name `say` is unclaimed.

### 2.2 Terrapin Logo — the strongest precedent, and a correction

Terrapin Logo (the continuation of the Apple Logo line) has exactly the surface
this design wants, and its semantics are the ones to copy:

> **Say** *list* / **Say** *word* — Speaks its parameter using the operating
> system voice synthesizer. The command returns immediately — i.e., it does not
> wait for the speech to finish before continuing. If you need it to, use
> WaitForSpeech. You can change the voice used by Say by using the SetVoice
> command.

with `SetVoice`, `Voice`, `Voices` and `WaitForSpeech` around it. Terrapin's
web Logo adds the rule that matters most to us: *"On computers without a speech
synthesizer, SAY does nothing."*

**This corrects [Berzerk §14.3](berzerk-design.md), which says "Apple Logo had
`SAY`."** Apple Logo II is silent — [sound-design.md](sound-design.md) §3.2
already established that the Apple II had a click speaker and Apple Logo II
exposed no sound primitives at all, and speech on that machine needed a
third-party card. The precedent is real but it belongs to Terrapin and to
FMSLogo, both of which drive a host OS synthesizer, and neither of which is the
1982 machine the sentence was reaching for. **The name is well-precedented; the
attribution was wrong.**

### 2.3 TI-99/4A — `CALL SAY`, and the lesson in its vocabulary

The TI Speech Synthesizer's `CALL SAY` spoke words from a **resident ROM
vocabulary of about 300 words**; a word outside the list was spelled out
letter by letter, and lower case produced "UH OH". (It is a BASIC command, not
a Logo one — TI Logo II is not the precedent here either.)

The lesson is the one this design is built to avoid: **a vocabulary is a
ceiling, and the ceiling is where the fun stops.** A word-ROM voice can say
`INTRUDER` and cannot say the child's name. That is the difference between a
sound effect for one game and a primitive for the interpreter.

### 2.4 The arcade parts

- **TSI S14001A** — Berzerk's own chip, word-addressed, driven through port
  $44 with a busy poll ($1745–$1765). Its 30 words are compressed waveform
  data in an external ROM. Same ceiling as the TI, same licence problem as §3.
- **Votrax SC-01** — the *other* arcade voice of the same years (Wizard of Wor,
  Gorf, Q\*bert, Reactor), and architecturally the opposite: **64 allophones
  through an analog formant filter**, no word ROM at all. Feed it phonemes and
  it says anything. This is the design Berzerk's cabinet did not use and the
  one we are building, and it is why the era's robot voice is reachable
  without the era's data.

### 2.5 SAM (1982) — the size and the voice, and not the code

Software Automatic Mouth, Don't Ask Software, 1982: a complete text-to-speech
system in about 6 KB on a 1 MHz 6502, with a text-to-phoneme front end
(`RECITER`) and a formant back end. It is the right size target, the right
voice character, and the right architecture — the split this document uses in
§4 is SAM's split.

**Its code is not usable.** The widely circulated C port is a reverse
engineering of commercial software whose publisher no longer exists; its own
README states that as long as the abandonware status continues *"the code
cannot be put under any specific open source software license."* That is the
same objection Berzerk §14.3 made to the Stern ROM, and it does not stop
applying because the data would be convenient this time. SAM is a **reference
point**, cited in this document and copied from nowhere.

### 2.6 What we take

| From | What |
|---|---|
| Terrapin | the name `say`, word-or-list input, returns immediately, silent where there is no synthesizer |
| Votrax SC-01 | phonemes as the real interface; formants as the mechanism |
| SAM | the two-stage architecture and the size budget |
| TI | the argument against a word vocabulary |
| Pico Logo's own `play` | append-on-call queueing, `playing?`-shaped sensing, the parser/sequencer split |

## 3. The licence constraint *is* the design

The repository is MIT. Three otherwise obvious routes are closed, and closing
them is what selects everything in §7 and §8:

| Route | Why not |
|---|---|
| The Berzerk speech ROM | Stern's copyrighted data (Berzerk §14.3 option A) |
| SAM's C port, or its tables | Abandonware, unlicensed — §2.5 |
| eSpeak / eSpeak-ng | GPLv3 against an MIT tree |
| Recorded words as ADPCM | Berzerk §14.3 option C: ~60 KB of flash for one game's 30 words, and no generality |

What **is** usable, and it is enough:

- **NRL Report 7948** (Elovitz, Johnson, McHugh & Shore, *Automatic Translation
  of English Text to Phonetics by Means of Letter-to-Sound Rules*, Naval
  Research Laboratory, 1976). **329 rules**, claimed correct for about 90 % of
  the words in average text, published complete in the report — which also
  carries the IPA→Votrax translation, i.e. the exact mapping §2.4 wants. A US
  Government technical report: public domain, and citable rather than copied.
- **Published formant measurements.** Peterson & Barney's 1952 vowel data
  (F1/F2/F3 for the English vowels) are physical measurements in the acoustic
  phonetics literature — facts, not a creative work, and reproduced in every
  textbook since.
- **ARPABET**, the DARPA phoneme naming, unencumbered and the alphabet the NRL
  rules' output maps onto.

So the whole engine is derived from published descriptions and written here.
**Nothing is ported.** That is a real cost — it is the reason M0 is a listening
test rather than a build — and it is the only way this ships.

## 4. Architecture: the front end is core, the back end is the device

This is the split [sound-design.md](sound-design.md) §7 already made for
`play`, applied again:

```
  say [intruder alert]
        |
        |  core/phonemes.c      NRL rules, 329 of them.
        v                       Pure function. Runs on the host.
  [ih n t r uw d er]  [ax l er t]
        |
        |  core/primitives_speech.c    validation, queue-full wait,
        v                              SpeechFrame packing
  speech_queue(frames, n)      <-- the device-op boundary
        |
        |  devices/picocalc/speech.c   formant synthesis,
        v                              in the refill IRQ, per block
  ninth source into the §6 mixer  ->  PWM slice 5
```

Two properties fall out of it and both are load-bearing:

1. **The interesting half is host-testable.** The rule engine is a pure
   function from text to a phoneme list, so §10's 200-word accuracy test runs
   in `ctest` with no board and no ear. `core/notation.c` is 243 lines and
   this is the same kind of module, larger only because there are 329 rules.
2. **The host build is silent and correct.** The device op is NULL on the host,
   the primitives succeed and do nothing — exactly what `toot` does today, and
   exactly Terrapin's documented behaviour (§2.2). A Logo program that speaks
   still runs in CI.

## 5. The Logo surface

Six primitives. Each is justified below; §15 Q1 asks whether that is too many.

### 5.1 `say`

```logo
say [intruder alert]           ; a list — the sentence
say "chicken                   ; a word
```

`say text`. Non-blocking: the phonemes are queued and Logo continues at once
(Terrapin's contract, and `play`'s). Calling `say` again **appends** to the
utterance, so a sentence can be assembled a word at a time — the property
Berzerk needs and the same one that made `play`'s tuneblocks work.

Numbers are spoken as digits (`say 42` → "four two", not "forty two");
`.`, `!` and `?` insert a sentence pause; other punctuation is skipped. A
character the rules cannot place is skipped rather than erroring — the TI's
spell-it-out fallback is worse than silence.

### 5.2 `phonemes` — the operation

```logo
?show phonemes [hello]
[hh eh l ow]
```

`phonemes text` outputs the phoneme list the front end produces. It exists so
that the 10 % the rules get wrong is **fixable in Logo** rather than being a
defect report: print it, edit it, feed it back. It is also the teaching
surface — a child can see what the machine thinks a word is made of.

### 5.3 `sayphonemes`

```logo
?show phonemes [robot]
[r aa b aa t]
?sayphonemes [r ow b ax t]     ; the rules put the wrong vowels in
```

`sayphonemes phonemelist`. Skips the rules entirely. Appends, like `say`.

The two commands are related by an identity, and the reference should state it
as the definition:

```logo
say :text   is   sayphonemes phonemes :text
```

`sayphonemes` earns its place three times over: it is the escape hatch for
§5.2, it is how a game holds a fixed vocabulary without paying the rules at
runtime (§11), and it is the only way to spell something the rules have no
business knowing.

### 5.4 `speaking?` / `speakingp`

`speaking?` outputs `true` while an utterance is sounding or queued. Same shape
and same spelling convention as `playing?`, and it composes with the demon
machinery the same way:

```logo
when [not speaking?] [next.taunt]
```

This is the answer to Terrapin's `WaitForSpeech`, which we do not add for the
reason [sound-design.md](sound-design.md) §5.4 gave for not adding TI's
`PLAYNOTE`: a sensing operation plus `when` subsumes a blocking wait, and the
house already chose that trade once.

### 5.5 `setvoice` / `voice`

```logo
setvoice [64 72 128 128]       ; pitch speed mouth throat
show voice
```

SAM's four knobs, which are the ones that reach a robot in one line — `pitch`
is the fundamental, `speed` scales every phoneme duration, and `mouth` and
`throat` scale the formant frequencies against the table. A four-element
setter with a matching getter is the `setenv [a d s r]` / `env` pattern
exactly.

Berzerk's robots are then one line at startup, not a synthesizer project.

### 5.6 `stopsound`, extended

`stopsound` gains speech: it abandons the utterance and fades the speech source
out with the same release the voices get, and clears the phoneme queue. The
reference sentence changes from "silences every voice" to "silences every voice
and stops speaking". Its promise not to touch `setenv`/`setwave` state extends
to `setvoice`.

No separate `stopspeech`. There is one "shut up" in the language and it should
mean all of it.

### 5.7 Lifetime

The [sound-design.md](sound-design.md) §5.5 rules, unchanged and now including
speech: an utterance keeps going at the toplevel prompt, BREAK stops it, a
toplevel error unwind stops it, `cs` does not touch it.

## 6. The phoneme alphabet

ARPABET, 40 phonemes plus a pause. Case-insensitive on input; `phonemes`
outputs lower case, matching the reference's house style for `play`'s note
words.

| | |
|---|---|
| Vowels (16) | `iy ih ey eh ae aa ao ow uh uw ah ax er ay aw oy` |
| Stops (6) | `p b t d k g` |
| Affricates (2) | `ch jh` |
| Fricatives (9) | `f v th dh s z sh zh hh` |
| Nasals (3) | `m n ng` |
| Approximants (4) | `l r w y` |
| Pause (1) | `_` |

Each is one row of a `const` table in flash:

```c
typedef struct Phoneme
{
    uint16_t f1, f2, f3; // formant centres, Hz
    uint8_t a1, a2, a3;  // formant amplitudes
    uint8_t dur_ms;      // nominal steady duration
    uint8_t flags;       // voiced | stop | fricative | nasal | transition class
} Phoneme;               // 12 B; 41 entries = ~500 B of flash
```

and the ten monophthong rows are Peterson & Barney's male-average
measurements, entered as published:

| | F1 | F2 | F3 | | | F1 | F2 | F3 |
|---|---:|---:|---:|---|---|---:|---:|---:|
| `iy` | 270 | 2290 | 3010 | | `aa` | 730 | 1090 | 2440 |
| `ih` | 390 | 1990 | 2550 | | `ao` | 570 | 840 | 2410 |
| `eh` | 530 | 1840 | 2480 | | `uh` | 440 | 1020 | 2240 |
| `ae` | 660 | 1720 | 2410 | | `uw` | 300 | 870 | 2240 |
| `ah` | 640 | 1190 | 2390 | | `er` | 490 | 1350 | 1690 |

`ax`, the schwa, is a reduced `ah` and is the vowel the rules produce most
often (§7's default rule for `A`). The five diphthongs (`ey ay oy aw ow`) are
two of these rows and a glide between them, which is what they physically are,
so they cost a start index, an end index and a duration rather than a row of
their own.

## 7. The front end: 329 rules

The NRL algorithm is a left-to-right scan with context. A rule is

```
    left-context [ item ] right-context = phonemes
```

and the classic worked example is the letter `A`:

```
    [A]           =  ax        the default
     [AR]#        =  eh r      "care", "vary"    (# = a vowel)
     [AI]         =  ey        "rain"
     [A]^^        =  ax        "carbon"          (^ = a consonant)
    #:[ALLY]      =  ax l iy   "usually"
```

The first rule whose contexts match wins; the item is consumed and the scan
resumes after it. Rules are bucketed by the first letter of the item, so a
scan tries about thirteen of them per letter, not 329.

Two additions the report anticipates and every implementation needs:

- **An exception list** for the words the rules get wrong and that matter —
  the irregular hundred (`one`, `two`, `of`, `once`, `said`, `does`,
  `people`…). Checked whole-word before the rules run. Budget: about 100
  entries, and the list grows by test failure rather than by imagination.
- **Numbers and punctuation**, handled in §5.1's terms before the rules see
  the text.

**Accuracy is a stated number and therefore a test.** The report claims ~90 %
of words in average text. §10's 200-word table checks *our transcription of
the rules* against that, so a failure means we typed a rule wrong, not that
English is hard.

## 8. The back end: a parallel formant synthesizer

### 8.1 The source

Two excitation sources, mixed per phoneme by its `flags`:

- **Voiced**: an impulse train at the voice fundamental (`setvoice` pitch,
  default ~100 Hz), shaped so it has energy across the formant range.
- **Unvoiced**: the LFSR the noise voices already use
  ([sound-design.md](sound-design.md) §4) — the code is written and tested.

A voiced fricative (`v z zh dh`) runs both. A stop (`p b t d k g`) is a closure
of silence followed by a burst, which is a duration and an amplitude envelope
on the noise source, not a new mechanism.

### 8.2 The resonators

Three formant resonators plus one nasal, each a two-pole IIR — Klatt's standard
form, and the arithmetic is four coefficients and two state words:

```
    y[n] = a·x[n] + b·y[n-1] + c·y[n-2]
```

in Q15 fixed point. Coefficients are derived from (F, bandwidth, sample rate)
once per parameter update, not per sample.

### 8.3 The frame, and the transition

Parameters update **once per refill block — every 3.5 ms**, which is the
granularity the ADSR envelopes already use and for the same reason
([sound-design.md](sound-design.md) §6). Phonemes are 50–150 ms and transitions
20–60 ms, so a transition is 6–17 interpolation steps. That is smooth, and it
means the speech engine adds no new timing concept to the IRQ: it is another
thing the block boundary services.

Transitions are linear interpolation of F1/F2/F3 and the amplitudes between
adjacent phoneme rows, weighted by the transition class in `flags` — a vowel
into a nasal moves differently from a vowel into a stop. **This is where
intelligibility actually lives.** Steady-state vowels are easy and M0 proves
only that; the words in M1 are the real test, and if they fail it will be here.

### 8.4 Sample rate: the mixer's, with no resampler

The synth runs at the **36.6 kHz mix rate**, not at a speech-typical 10–12 kHz.

The tempting saving is to synthesize at 12.2 kHz (an exact third) and
interpolate up. It is rejected for two reasons: a 6.1 kHz Nyquist puts `s` and
`sh` in the same place, which is a specific and famous way for a synthesizer
to be unintelligible; and the resampler is code and state that buys ~1 % of a
core. The measured budget in §9.2 says we can afford the honest version.

**Consequence to remember**: the mix rate follows `clk_sys`, which is why
`sound_reclock()` exists (`devices/picocalc/sound.h`). Every resonator
coefficient is a function of the sample rate, so **the speech engine must
re-derive its coefficients from `sound_reclock` too** — otherwise `hw.setcpu
"fast` retunes the music and leaves the voice speaking at half pitch. This is
a one-line hook and an easy thing to forget.

### 8.5 The mixer slot

Speech is a **ninth and tenth source** — the same samples into both ears, with
one gain — summed at the existing saturation point. It is deliberately *not* a
ninth PSG voice: a `Voice` is a phase accumulator plus an ADSR and a formant
synthesizer shares none of that structure.

[sound-design.md](sound-design.md) §4 sized the accumulator so that "four
simultaneous full-volume voices cannot wrap". A fifth full-scale source per ear
can. The fix is to widen the accumulator's headroom by one bit, which is free
(the mix is already 32-bit and only the final scale changes) and costs ~2 dB of
per-voice loudness, **or** to leave the scale and let the existing saturation
handle it. Recommend the second, because in practice nothing runs four
full-volume voices under speech, and note it as something to listen for on the
board at M3 rather than something to decide from a spreadsheet.

## 9. Cost

### 9.1 Memory

| Item | Where | Size |
|---|---|---|
| NRL rule table (329 rules) | flash, `const` | ~8 KB |
| Exception list (~100 words) | flash, `const` | ~2 KB |
| Phoneme table (41 × 12 B) | flash, `const` | ~500 B |
| Engine + rules code | flash | ~3 KB |
| **Flash total** | | **~13.5 KB** |
| Phoneme queue, `SPEECH_QUEUE_LEN 128` × 4 B | SRAM | 512 B |
| Synth state (4 resonators, sources, frame) | SRAM | ~200 B |
| Voice parameters + queue bookkeeping | SRAM | ~32 B |
| **SRAM total** | | **~750 B** |

Berzerk §14.3 estimated ~10 KB of flash and "a few hundred bytes of SRAM". The
flash is 35 % over that estimate — the rule table is the whole difference — and
the SRAM is right.

Flash is not a constraint on any board (4 MB on the smallest). **SRAM is**, and
the presets link at 86–91 % today, so the one specific way this feature
reproduces the `repl_init` out-of-memory panic is **a table that isn't `const`**
and therefore lands in `.data` instead of flash. 10 KB of rules is exactly the
size of mistake that does it. Worth a test that checks the link map rather than
a comment that asks nicely.

`SPEECH_QUEUE_LEN` goes in `core/limits.h` beside `SOUND_QUEUE_LEN`, per the
house rule. 128 phonemes is about eight seconds — "the humanoid must not
escape" is 22.

### 9.2 CPU, in the IRQ

Per sample: four resonators at ~10 cycles each in Q15 with the M33's
single-cycle multiply, plus source generation and mixing, call it **~70
cycles**. At 36.6 kHz that is 2.6 M cycles/s — **1.7 % of core 0 at 150 MHz**,
0.85 % at 300, against the existing mixer's 7 %.

Per refill block: 128 samples × 70 = ~9,000 cycles ≈ **60 µs at 150 MHz**,
against the hard **3.5 ms** deadline that
[sound-design.md](sound-design.md) §12.3 established. That is 1.7 % of the
budget, which is comfortable — but §12.3's rule is that anything running in
that IRQ gets measured rather than estimated, and M3 measures it.

One flash caveat, from P10 M5: code and tables read in IRQ context pay XIP
cache misses, and the 500 B phoneme table *is* read there. If M3's measurement
comes back unexpectedly high, the phoneme table and the synth inner loop are
the `__not_in_flash_func` / SRAM candidates. **The 10 KB rule table never runs
in IRQ context** — it runs in thread context at `say` time — and stays in
flash regardless.

### 9.3 The front end, at call time

The rules are C, not Logo, so the 60–180× host→board ratios in the roadmap do
not apply; the cost is a bucketed scan of ~13 rules per letter, each a short
context compare. Estimated **~10 µs a letter on a board**, so
"the humanoid must not escape" (25 letters) is **~0.25 ms**.

Against Berzerk's 50 ms frame gate that is 0.5 %, so **Berzerk can call `say`
directly in its frame loop** — the concern that motivated `sayphonemes` as a
performance escape turns out not to bind. `sayphonemes` stays for §5.3's other
three reasons, and §11 uses it because it is *also* the faithful port.

This estimate is the least-supported number in the document and M2 measures it.

## 10. Core/device boundary and testing

**Device ops** (extend `hardware->ops`, picocalc implements, host leaves NULL):

| Op | Purpose |
|---|---|
| `speech_queue(const SpeechFrame *f, int n)` → accepted | Append phonemes to the utterance |
| `speech_status()` → `{speaking, free_slots}` | Backs `speaking?` and the queue wait |
| `speech_stop(void)` | Abandon and fade; called by `stopsound` |
| `speech_voice(pitch, speed, mouth, throat)` | The §5.5 knobs |

```c
typedef struct SpeechFrame
{
    uint8_t phoneme; // index into the §6 table
    uint8_t dur_ms;  // 0 = table default, scaled by voice speed
    uint8_t pitch;   // 0 = voice default, else Hz/2
    uint8_t stress;  // 0..3; scales amplitude and duration
} SpeechFrame;       // 4 B, beside SoundEvent's 6
```

**Mock** (`tests/mock_device.*`): `mock_speech_queue` logs every frame with a
timestamp, `mock_speech_set_status` scripts the queue-full path, mirroring what
the sound ops already do.

**Tests**:

- `tests/test_phonemes.c` — the rule engine as a pure function. A **200-word
  table with expected phoneme strings**, asserting ≥ 90 % exact match (§7);
  every rule context form (`#`, `^`, `:`, `+`, word boundaries) exercised by
  name; the exception list; digits, punctuation, empty input, a word of
  unknown characters.
- `tests/test_speech_synth.c` — **the formant assertion, and it is what makes
  a synthesizer testable without an ear.** Render a sustained `iy` into a
  buffer, run a Goertzel at 270 Hz and 2290 Hz and at two off-formant
  frequencies, and assert the formant bins are ≥ 20 dB above them and within
  10 % of the tabled centres. Repeat for `aa` and `uw`. A synthesizer that is
  merely *wrong* passes a "did it make noise" test and fails this one.
- `tests/test_primitives_speech.c` — the surface: argument validation, the
  `say` ≡ `sayphonemes phonemes` identity as an actual assertion over the mock
  log, append-on-second-call, `speaking?` against scripted status,
  `stopsound` clearing speech, `setvoice`/`voice` round-trip, and the host's
  NULL-op silence.
- E2e: Berzerk's five sentences (§11) as a golden mock log.

**Reference**: six new sections in `# The Outside World` — `say`,
`sayphonemes`, `phonemes`, `speaking?`, `setvoice`, `voice` — each being its
own `help` text per the recurring checklist; `stopsound` gains a sentence and
`toot`'s cross-reference list gains `say`.

## 11. What Berzerk does with it

Berzerk's ROM holds 30 words ($01–$1D) and assembles sentences from word
indices at runtime ([berzerk-design.md](berzerk-design.md) §14.2). The faithful
port is the same structure, and `sayphonemes`' append semantics make it
one loop:

```logo
to voc
  pprop "ph "intruder [ih n t r uw d er]
  pprop "ph "alert    [ax l er t]
  pprop "ph "humanoid [hh y uw m ax n oy d]
  pprop "ph "chicken  [ch ih k ax n]
  ; ... 30 in all
end

to speak :words                 ; a sentence is a list of word indices
  if empty? :words [stop]
  sayphonemes gprop "ph first :words
  speak butfirst :words
end
```

Then §14.2's five utterances are five calls, and the taunt generator —
`<get|charge|attack|destroy|shoot|kill>` plus
`<the chicken|it|the humanoid|the intruder>` — is `speak` over two `pick`s,
which is what `GENERATE_ROBOT_SPEECH` ($2B97) is.

Two notes for whoever writes that file:

- **Property lists, not procedures.** Berzerk is already close to the
  `MAX_PROCEDURES` ceiling of 128 that Battlezone hit exactly; thirty words as
  thirty procedures would blow it and the failure mode is that the last `to` in
  the file is silently dropped. One property list is one name.
- **The on-screen caption stays.** §14.2 ships text on line 26 whether or not
  this lands, and it should stay after it lands: the arcade's speech is
  famously hard to make out, the caption is what the Vectrex port did, and a
  player who cannot hear the room should still be told they are a chicken.

## 12. Milestones

- **M0 — Can we make a vowel? (the gate.)** Host only. The §8.2 resonator core
  and the §6 vowel rows, plus a test that renders five sustained vowels to
  `.wav` files in the build directory. **Gate: a person listening picks
  `iy eh aa ao uw` out of a shuffled set of five**, and the §10 formant
  assertion is green. If the vowels do not come out, this item stops here — for
  ~300 lines of C, rather than after the primitive surface, the reference
  chapter and the board integration.

  **Built 2026-08-28**, `core/speech_synth.c`/`.h` (float, not §8.2's Q15
  sketch — this project's math convention is single-precision float
  throughout, and the RP2350's FPU makes it free) and
  `tests/test_speech_synth.c`. It runs and is host-testable, which is why it
  moved out of `devices/picocalc/speech.c` and into `core/` — that file stays
  the M3 IRQ wrapper, not the resonator's home. **Both halves of the gate
  are green**: the Goertzel assertion passes, and a listen confirmed
  `iy eh aa ao uw` are all recognisable in the rendered `.wav` files.

  One real finding, not just a build: a naive parallel sum
  (`a1*R1 + a2*R2 + a3*R3`) put the measured spectral peak on the wrong side
  of a low F1 against 100 Hz pitch harmonics, because the three resonators'
  phase responses partially cancelled near the boundary between formants.
  Alternating the sign of the middle branch (`+ a1*R1 − a2*R2 + a3*R3`) fixed
  it — this is Klatt's own convention for a parallel formant synthesizer, and
  the actual reason the topology is called "parallel" rather than "summed."
  §10's "within 10%" also needed one refinement to be checkable at all: with
  phonemes rendered at a 100 Hz default pitch, no harmonic lands exactly on a
  tabled centre (270 Hz sits between 200 and 300), so the test locates the
  loudest harmonic and refines it by parabolic interpolation against its
  neighbours rather than reading a single bin. And the two off-formant probes
  that reliably clear 20 dB below every tested formant, for every vowel
  tried, are not an arbitrary pair — they are both above F3 (1.4× and 1.8×
  it); a probe below F1 or between F1 and F2 was sometimes within a few dB of
  the peak itself, since real vowels don't leave much room there (`aa`'s F1
  and F2 are only 360 Hz apart).
- **M1 — The inventory and `sayphonemes`.** All 41 phonemes, the transition
  classes, stops and fricatives; `sayphonemes`, `speaking?`, `stopsound`'s
  extension; device op and mock. **Gate: the ten Berzerk words, hand-typed as
  phonemes, are intelligible in a rendered WAV.**

  **Built 2026-08-28.** `core/speech_synth.c` grew from the ten monophthongs
  to the whole §6 table, plus the transition machinery of §8.3;
  `core/primitives_speech.c`, three device ops (`speech_queue`,
  `speech_status`, `speech_stop`), their mocks, and
  `tests/test_primitives_speech.c`. The gate renders `intruder alert
  humanoid chicken robot fight escape destroy shoot kill` and the sentence
  "INTRUDER ALERT! INTRUDER ALERT!" to `.wav`.

  §9.1's numbers came out right where it said: the table links at **492 B**
  of `.rodata` against the predicted ~500 B, and it is `.rodata` — the one
  failure mode §9.1 named did not happen. The engine's *code* is ~4.7 KB
  against a ~3 KB line item, which matters not at all against 4 MB of flash
  but is worth carrying forward into M2's estimate.

  **Four corrections to the design came out of building it**, all in §8's
  half and all found by measuring rather than by listening:

  1. **§8.1's "a voiced fricative runs both" cannot mean both at full
     level.** At equal levels `z` is the loudest phoneme in the inventory and
     clips, which it never is in speech: the voicing under an obstruent is a
     voice bar, a weak buzz behind the constriction. It runs at 0.3.
  2. **A stop's closure has to be silence, not a fade.** Ramping the
     amplitude down across the whole closure leaves no silent stretch at all,
     and the silence *is* the cue that says "stop". The tract now shuts in
     8 ms and the rest of the closure is nothing.
  3. **The noise source needs two levels.** §8.1's "an amplitude envelope on
     the noise source" for a stop turns out to be quantitative: a release
     burst is a transient and runs 4× the level sustained frication does, or
     `p` and `k` vanish under the vowels around them.
  4. **The two source gains are a measurement, not a ratio you can reason
     out.** Rendering all 41 rows and reading their RMS is the only way to
     find the balance; the obvious equal-gain pairing puts the fricatives
     about 10 dB *above* the vowels, which is backwards, and no amount of
     listening to one phoneme at a time reveals it.

  Two smaller things the table itself wanted. §6's diphthongs ("a start
  index, an end index and a duration") fit **inside** the 12 B row as a
  twelfth byte, `glide_to`, rather than needing the separate table the
  wording implies — 6 B of formants, 3 of amplitudes, duration, flags, glide.
  And the pause row holds the *neutral-tract* frequencies at zero amplitude
  rather than zero frequency, because a transition into 0 Hz sweeps the
  formants down to DC on the way out and that is audible.

  The automated companions to the ear, in `tests/test_speech_synth.c`: every
  row sounds and the pause does not; every row round-trips through its
  ARPABET name; `s` and `sh` land in different bands, which is §8.4's stated
  reason for the honest sample rate and therefore ought to be an assertion
  rather than a paragraph; `r`'s low F3 is measurable; a stop is silent
  before it bursts; and a diphthong's second half does not sound like its
  first.
- **M2 — The front end.** The 329 rules, the exception list, `say` and
  `phonemes`; §9.3's cost measured. **Gate: the 200-word table at ≥ 90 %.**

  **Built 2026-08-28**, `core/phonemes.c`/`.h` (the rules, the exception
  list and the §5.1 handling of digits and punctuation), `say` and
  `phonemes` in `core/primitives_speech.c`, and `tests/test_phonemes.c`.
  **The gate is green with room**: the table is written at 241 words rather
  than 200 and lands at **96.3 %**, or 94.2 % before the exception list had
  anything in it. The nine misses that remain are all one thing —
  unstressed vowels the rules do not reduce (`from` is "f r aa m", `second`
  is "s eh k aa n d") — which is §14 R5's fence, not a mistyped rule.

  The rules are stored the way the report writes them, one string a rule,
  `left[item]right=phonemes`, with §6's lower-case ARPABET on the right.
  A rule's right-hand side is therefore literally a Logo phoneme list, which
  makes a rule checkable against the report by eye and makes `phonemes`
  print exactly what the table says.

  **The findings, all from the accuracy table rather than from reading:**

  1. **The published table does not silence every doubled consonant.**
     `LL`, `MM`, `NN`, `SS` and `GG` are in it; `TT`, `FF`, `PP`, `BB`,
     `DD`, `RR` and `ZZ` are not, and without them "little" is
     "l ih t t ax l" and "off" is "ao f f" — a stop articulated twice, which
     is audibly wrong rather than subtly wrong. Seven rules.
  2. **The report's cluster of rules for a final S looks redundant and is
     not.** Collapsing them into "a vowel before a final S voices it" turns
     "us" into "uz" and "famous" into "famouz"; `U[S] ` needs its own line,
     which is exactly what the published table has.
  3. **Inside a left context, the order of `:` and `^` decides the past
     tense.** `:` is greedy and nothing backtracks, so `#:^E[D] ` matches
     "walked" (the `^` takes the K, the `:` takes the L) and `#^:E[D] `
     never matches anything at all — the `:` eats both consonants and
     leaves the `^` nothing. The two differ by a transposition and one of
     them silently costs every `-ed` its /t/.
  4. **The exception list is a tenth of §7's budget, and that is the
     rules' doing.** §7 sizes it at ~100 words, the irregular hundred; the
     published rules already carry most of them as rules (`[ONE]`, `[SAID]`,
     `[PEOP]`, `[TWO]`), so what the table actually earned was five —
     `father`, `house`, `live`, `river`, `study` — on top of the six the
     design names. Eleven entries, ~200 B, against a ~2 KB line item.

  Two things §4 and §5.1 did not say and the code had to decide. **A list
  is joined back into a sentence before the rules see it**, because some
  rules read across the space (`" [THE] #"` is "the" before a vowel), so
  `say [the apple]` is not `say "the` followed by `say "apple` — hence
  `SPEECH_TEXT_MAX` and the join buffer. And **`say 3.5` says "three,
  pause, five"**, which is what §5.1's two rules produce when they meet: a
  period is a sentence pause and digits are spoken one at a time.

  **The costs came in under §9.1 and §9.3.** The front end links at
  **10.3 KB** of flash (3.5 KB of code, 5.0 KB of rule strings, 1.8 KB of
  pointer arrays) against a ~13 KB line item, and its `.data` and `.bss` are
  both **zero** — R4 did not happen. `ctest` writes
  `speech_frontend_cost.txt`: **0.51 µs a letter** on the host, so even a
  20× host-to-board penalty lands on §9.3's ~10 µs estimate, and the
  conclusion it drew stands — Berzerk can call `say` in its frame loop.
- **M3 — On the board.** Mixer integration, §8.5's headroom decided by
  listening, the `sound_reclock` hook, `setvoice`/`voice`, the reference
  sections. **Gate: the refill cost measured against 3.5 ms (§9.2), and no
  click with `say` and `play` running together.**
- **M4 — Berzerk adopts it** (§11), which is the end-to-end exercise that
  [sound-design.md](sound-design.md) §9's M4 was for the PSG.

M0–M2 need no hardware, which is most of the work.

## 13. Rejected alternatives

- **S14001A emulation with a user-supplied ROM** — Berzerk §14.3 option A. An
  empty box, and a primitive nobody without a ROM can use.
- **Porting SAM's C** — §2.5. The Stern objection, applied to ourselves.
- **eSpeak / eSpeak-ng** — GPLv3 into an MIT tree.
- **Recorded words, ADPCM** — Berzerk §14.3 option C. ~60 KB for one game's
  vocabulary, and §2.3's ceiling.
- **LPC coefficients** (the TMS5220 route) — better voice per byte than
  formants, but the coefficients come from recordings, so it is option C with
  extra steps.
- **Diphone concatenation** — the best quality per line of code of anything
  here, and the units are recordings. Same wall.
- **A cloud TTS over the HTTP stack** — needs a radio (two boards of three), a
  network, and someone else's service; and it is not a primitive, it is an
  HTTP call a user can already write.
- **Speech as a ninth PSG voice** — §8.5. The structures have nothing in
  common.
- **A 12.2 kHz internal rate with a resampler** — §8.4.
- **`speak` as the name** — Scratch's `say` is a speech bubble and its speech
  extension is `speak`, but this is Logo, where Terrapin and FMSLogo both say
  `say`, and the name is free here.
- **`waitforspeech`** — §5.4. `speaking?` plus `when`, per the house's own
  precedent.

## 14. Risks

- **R1 — Intelligibility is a judgement, not an assertion.** §10's formant test
  proves the resonators are where the table says; it cannot prove a word is
  recognizable. Mitigated by making M0 and M1 explicit listening gates rather
  than pretending otherwise, and by doing them on the host where iteration is
  seconds.
- **R2 — 90 % correct means one word in ten is wrong**, and it will be a word
  someone cares about. `phonemes` + `sayphonemes` is the answer, which is why
  they are in the shipped surface and not in a debugging appendix.
- **R3 — The 3.5 ms deadline.** Speech is the second continuous consumer of the
  refill IRQ, and §12.3's history says this layer surprises people. Measured at
  M3; if it does not fit, §8.4's rejected resampler comes back before anything
  else changes.
- **R4 — A non-`const` table.** §9.1. The specific way this becomes an
  out-of-memory panic at `repl_init`.
- **R5 — Scope.** Text-to-speech is bottomless. The fence: **one voice, English,
  phoneme-level pitch and stress only, no sentence prosody, no dictionary
  beyond the exception list, no phoneme editing beyond `sayphonemes`.**
  Anything past that fence is a later roadmap item with its own gate.

## 15. Open questions

- **Q1 — Six primitives, or fewer?** `say` alone is shippable, and `say` +
  `speaking?` is arguably the whole job. `phonemes` and `sayphonemes` are the
  pair that make the 10 % survivable and give Berzerk its faithful structure;
  `setvoice`/`voice` is what makes it a robot rather than a narrator.
  **Recommend all six**, but M1 ships `sayphonemes` and `speaking?` first, so
  a decision to stop early has somewhere to stop.
- **Q2 — ARPABET, or the Votrax/SP0256 mnemonics?** The SC-01's names are the
  period-correct ones and this is a retro machine. **Recommend ARPABET**: it is
  what the NRL rules' output maps onto, it is documented everywhere, and its
  names are readable (`sh` beats `SH1`).
- **Q3 — Stereo.** Centre in both ears, or a `(say text ear)` form?
  **Recommend centre**; Berzerk's cabinet is mono and a per-ear voice is a
  feature looking for a user.
- **Q4 — Should `say` duck the music?** **Recommend no.** A game that wants
  that can `stopsound`, and automatic ducking is a policy the primitive should
  not hold.
- **Q5 — `setvoice`'s parameters.** SAM's four (pitch, speed, mouth, throat),
  or two (pitch, speed)? **Recommend four** — they are era-correct, they are
  what reaches a robot in one line, and `setenv`'s four-element list is the
  precedent that says the house is fine with it.
- **Q6 — Case of `phonemes`' output.** Lower case to match `play`'s note words,
  or upper to match every ARPABET table ever printed? **Recommend lower**,
  input case-insensitive either way.
- **Q7 — `say` on a build with no engine.** Silent, or an error? **Recommend
  silent** — `toot`'s behaviour on the host today, and Terrapin's documented
  rule (§2.2). It is what keeps a speaking program running in CI.
- **Q8 — Does this block Berzerk?** **Recommend no**, and Berzerk §14.3 already
  says no: the on-screen caption ships either way, and §11's sentence assembly
  is written once regardless of which end it drives.
