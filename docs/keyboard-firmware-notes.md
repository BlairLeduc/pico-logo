# The PicoCalc keyboard MCU firmware

The keyboard is not wired to the RP2350. An **STM32F103R8T6** on the mainboard
owns the key matrix, the two backlights, the battery gauge and the power rail,
and exposes all of it to us as an **I²C slave at address `0x1F`**. Everything
below the I²C bus is that MCU's firmware, not ours — which means a key
combination it decides not to send is one no amount of work in
[keyboard.c](../devices/picocalc/keyboard.c) can recover.

This document is what that firmware does, why it matters to Pico Logo, and how
to go and read it again.

## Where the source is

Upstream, in the ClockworkPi repository, as an Arduino sketch:

<https://github.com/clockworkpi/PicoCalc/tree/master/Code/picocalc_keyboard>

Sixteen small files, ~1,660 lines total. Read them with the GitHub CLI rather
than by cloning the whole repo:

```bash
# What is there
gh api repos/clockworkpi/PicoCalc/contents/Code/picocalc_keyboard --jq '.[].name'

# One file
gh api repos/clockworkpi/PicoCalc/contents/Code/picocalc_keyboard/keyboard.ino \
  --jq '.content' | base64 -d
```

| File | What is in it |
|---|---|
| `keyboard.ino` | **The one that matters.** The keymap tables, the modifier rules, the press/hold/release state machine, the matrix scan. |
| `keyboard.h` | Key codes and the `key_state` enum — the source our [keyboard.h](../devices/picocalc/keyboard.h) was copied from. |
| `picocalc_keyboard.ino` | `setup`/`loop`, the I²C slave callbacks (the register dispatch), the AXP2101 PMU handling. |
| `reg.h`, `reg.ino` | The register ids, the `CFG_*` bits, and the power-on defaults. |
| `fifo.h`, `fifo.ino` | The 31-entry event FIFO. |
| `conf_app.h` | `SLAVE_ADDRESS`, `FIFO_SIZE`, `BIOSVERSION`, backlight steps. |
| `pins.h`, `port.*` | Row/column/button GPIO assignment and a thin pin-config wrapper. |
| `backlight.*`, `battery.*` | Backlight PWM and the battery/charge plumbing. |

Flashing instructions (should we ever need to modify it) are in the upstream
wiki: *Setting Up Arduino Development for PicoCalc keyboard*. It needs the
stm32duino core and ClockworkPi's fork of XPowersLib. **We have never done
this**, and it is not a small step — the board has one keyboard MCU and a bad
flash leaves it unusable.

## The register interface

Write the register number, then read two bytes; OR `0x80` into the register
number to write to it. Our side is [southbridge.c](../devices/picocalc/southbridge.c).

| Reg | Name | Read gives | Notes |
|---|---|---|---|
| `0x01` | `VER` | `[0, 0x16]` | `BIOSVERSION` from `conf_app.h`; bump it there when firmware changes. |
| `0x02` | `CFG` | `[0, 0]` | **Not in the dispatch — unreachable.** See below. |
| `0x03` | `INT` | `[0, 0]` | Not in the dispatch. Bits are set internally but never delivered: **there is no interrupt line**, the pin code is commented out. |
| `0x04` | `KEY` | `[count \| caps \| num, 0]` | FIFO depth in the low 5 bits, `KEY_CAPSLOCK` = bit 5, `KEY_NUMLOCK` = bit 6. |
| `0x05` | `BKL` | LCD backlight | Writable, stepped to multiples of 16, clamped 16–240. |
| `0x06` | `DEB` | `[0, 0]` | Not in the dispatch. The stored debounce value is never used anyway. |
| `0x07` | `FRQ` | `[0, 0]` | Not in the dispatch. The scan rate is the hard-coded `KEY_POLL_TIME`. |
| `0x08` | `RST` | — | **Reads reset the MCU** after a 1 s delay; a write resets after `value` seconds. Never touch this register casually. |
| `0x09` | `FIF` | `[state, key]` | One event dequeued per read. Reading an empty FIFO gives `[0, 0]`, i.e. `KEY_STATE_IDLE`. |
| `0x0A` | `BK2` | keyboard backlight | Writable, steps of 32. |
| `0x0B` | `BAT` | percent, bit 7 = charging | Refreshed by the PMU poll **every 20 s**, not on demand. |
| `0x0C` | `C64_MTX` | 10 bytes | Raw matrix bitmap; the reply is longer than two bytes. |
| `0x0D` | `C64_JS` | joystick bits | Arrow buttons + Enter as a C64-style joystick. |
| `0x0E` | `OFF` | — | Write to power off; the value is clamped to ≥ 6 and used as a **delay in seconds** before `PMU.shutdown()`. |

`receiveEvent` has a case for ten registers only — `VER`, `FIF`, `BKL`, `BK2`,
`BAT`, `KEY`, `C64_MTX`, `C64_JS`, `OFF`, `RST` — and anything else falls to a
default that replies `[0, 0]`. **`CFG` is one of the unreachable ones**, so the
config is whatever `reg_init` chose at power-on and the host cannot change it.
That closes off the obvious workaround for the swallowed chords below: we
cannot clear `CFG_USE_MODS` to ask for raw keys plus modifier events.

Power-on config (`reg_init`) is `CFG_OVERFLOW_INT | CFG_KEY_INT | CFG_USE_MODS
| CFG_REPORT_MODS`. Two consequences we depend on:

- **`CFG_REPORT_MODS` is on**, so Alt/Sym/Shift/Ctrl arrive as their own
  press/release events (`KEY_MOD_*`, `0xA1`–`0xA5`) — that is what lets us
  latch modifier state host-side.
- **`CFG_OVERFLOW_ON` is off**, so when the 31-entry FIFO fills, the **newest**
  event is dropped and the backlog is kept. Our ring in `kbd_push` deliberately
  does the same thing, so the two ends agree about what a full queue means.

## How an event is produced

`loop()` calls `keyboard_process()` and then sleeps 10 ms; the scan itself
early-returns unless `KEY_POLL_TIME` (16 ms) has passed, so **the matrix is
scanned every ~26 ms** in practice. Up to `KEY_LIST_SIZE` (10) keys are tracked
at once. Per key, the state machine in `next_item_state` is:

```
IDLE --pressed--> PRESSED --300 ms (KEY_HOLD_TIME)--> HOLD --every 100 ms--> HOLD
                     |                                  |
                  released                           released
                     v                                  v
                 RELEASED --> IDLE (emits nothing)
```

The repeat cadence a held key produces — **nothing for 300 ms, then one event
per 100 ms** — is the number our driver's drain logic is sized against, and it
comes from here.

`transition_to` then decides what a `HOLD` looks like on the wire: for
printable ASCII, Enter, Tab, Del, Backspace **and the four arrows** it emits a
fresh `KEY_STATE_PRESSED` instead, so those keys auto-repeat. Everything else
(F-keys, Esc, Break, Home/End/PgUp/PgDn) emits a real `KEY_STATE_HOLD` that we
consume and ignore. There is no `IDLE` event: the entry pointer is cleared
before that transition and `transition_to` returns early on a null entry.

## Modifiers are resolved in the MCU — this is the important part

The keymap is two tables at the top of `keyboard.ino`. Every entry is
`{chr, symb}`: the character, and the character **shift** produces. `symb` is
zero when the initialiser omits it.

```c
if (shift && (chr < 'A' || chr > 'Z')) {
    chr = p_entry->symb;
}
...
if (chr != 0 && output == true) {   // ← a zero alternate is simply not sent
```

So per modifier:

| Modifier | What the MCU does |
|---|---|
| **Shift** | Substitutes `symb` for any non-letter. Letters are stored uppercase and pass through, so shift + letter = uppercase. **If `symb` is 0 the key is swallowed entirely.** |
| **Ctrl** | Read into a local and **never used**. The key is emitted unchanged (letters arrive lowercase) alongside the `KEY_MOD_CTRL` event, so every Ctrl chord is ours to define host-side. |
| **Alt** | Consumes Alt + `,` / `.` / Space / `B` for itself (LCD backlight down/up, keyboard backlight cycle, battery display) and emits nothing for them. Alt + `I` becomes Insert. Anything else passes through, but **skips the lowercasing branch**, so Alt + a letter arrives uppercase. |
| **Caps/Num Lock** | Toggled by chords, not keys: Shift-right + Alt sets caps lock, Shift-left + Alt sets num lock, and either shift alone clears them. Num lock is folded into the Alt test. |

The shifted alternates are also **the only way to reach several keys** — the
keyboard has no dedicated Home, End, PgUp, PgDn, Insert or Break:

| Chord | Sends | | Chord | Sends |
|---|---|---|---|---|
| Shift + Esc | `KEY_BREAK` (0xD0) | | Shift + Enter | `KEY_INSERT` (0xD1) |
| Shift + Tab | `KEY_HOME` (0xD2) | | Shift + Del | `KEY_END` (0xD5) |
| Shift + Up | `KEY_PAGE_UP` (0xD6) | | Shift + Down | `KEY_PAGE_DOWN` (0xD7) |
| Shift + F1…F5 | F6…F10 | | Alt + I | `KEY_INSERT` |

And these chords **send nothing at all**, because the key's `symb` is zero:

> **Shift + Left, Shift + Right, Shift + Space, Shift + Backspace,
> Shift + Caps Lock.**

That is not a bug we can work around — the byte never leaves the MCU, and the
`CFG` register that would turn the substitution off is not writable. It is why
the editor's word movement is bound to **Ctrl + Left/Right** and not to
Shift + Left/Right as first requested (roadmap, 2026-08-17). Before designing
any new key binding, check the two tables in `keyboard.ino`: **the keymap, not
the host, decides which modifier+key pairs exist.**

## The keymap

Matrix (`kbd_entries[7][8]`), as `chr` / `symb`. This is the electrical order,
not the physical layout:

| | c0 | c1 | c2 | c3 | c4 | c5 | c6 | c7 |
|---|---|---|---|---|---|---|---|---|
| **r0** | F5/F10 | F4/F9 | F3/F8 | F2/F7 | F1/F6 | `` ` ``/`~` | 3/# | 2/@ |
| **r1** | Bksp/— | Del/End | Caps/— | Tab/Home | Esc/Break | 4/$ | E | W |
| **r2** | P | =/+ | -/_ | \\/\| | //? | R | S | 1/! |
| **r3** | Enter/Ins | 8/* | 7/& | 6/^ | 5/% | F | X | Q |
| **r4** | ./> | I | U | Y | T | V | ;/: | A |
| **r5** | L | K | J | H | G | C | '/" | Z |
| **r6** | O | ,/< | M | N | B | D | Space/— | *(unused)* |

Buttons (`btn_entries[12]`), the four modifier keys and the cluster around the
arrows:

| Alt | Ctrl | ShiftL | ShiftR | 0/) | 9/( | ]/} | [/{ | **Right/—** | **Up/PgUp** | **Down/PgDn** | **Left/—** |
|---|---|---|---|---|---|---|---|---|---|---|---|

## What this means for our driver

[keyboard.c](../devices/picocalc/keyboard.c) mirrors the firmware closely, and
a few of its choices only make sense with the firmware in view:

- **Modifier folding.** We latch `KEY_MOD_*` press/release events and fold them
  into the following key — the only mechanism available, since Ctrl is not
  applied MCU-side. Ctrl + letter becomes a control character, Ctrl + `,` / `.`
  become `KEY_CTRL_COMMA` / `KEY_CTRL_PERIOD`, and Ctrl + Left/Right become
  `KEY_WORD_LEFT` / `KEY_WORD_RIGHT` (0xB8/0xB9, codes of our own — the MCU has
  no such thing).
- **Our shift handling for letters is redundant.** The MCU already sends
  uppercase when shift is held, so the `ch &= ~0x20` branch never fires in
  practice. It is harmless; leave it unless something else forces a rewrite.
- **Draining more than one event per poll.** A press/release pair per keystroke
  plus 100 ms repeats out of a 31-entry FIFO is a producer our old
  one-event-per-tick consumer could not keep up with (bugs B28/B29).
- **Key codes are the firmware's**, including the oddity that `KEY_F10` is
  `0x90` rather than the `0x8A` the F1–F9 run would suggest.

Two behaviours worth knowing about when timing changes:

- **The MCU resets its own I²C bus** if it sees neither a receive nor a request
  event for 2.5 s (after the first 10 s of uptime). Our 100 ms poll keeps well
  clear, but a game that stops polling for seconds will meet a slave that has
  just re-initialised.
- **`loop()` is not always prompt.** The PMU check runs every 20 s and prints
  over `Serial1`, and the power-off path deliberately blocks in `delay()`.
  Scan jitter of a few tens of milliseconds is normal.

## See also

- [hardware-notes.md §8](hardware-notes.md) — the PicoCalc's other peripherals,
  and the same keyboard facts from the host's side.
- [keyboard.h](../devices/picocalc/keyboard.h),
  [keyboard.c](../devices/picocalc/keyboard.c) — our driver.
- [test_keyboard.c](../tests/test_keyboard.c) — host tests against a fake
  southbridge FIFO; this is where to pin any new decoding behaviour.
