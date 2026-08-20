# P12 — Vi mode for the Logo Editor (design)

Status: **M1--M8 built 2026-08-18.** The mode is complete: normal mode, visual
mode, `f`/`t`/`%`, the ex command line, `setvimode`, the reference chapter,
`u` / `Ctrl` `R` over a tiered journal, the word and bracket text objects
(§15), patterns in `:s`, `/` and `?` (§16), `Ctrl` `G` (§17), and navigation --
`*`, `#`, the mark, `gd` and `zz` (§18). Every milestone has been checked on a
board. §14 records where the build departed from this design.

Three scoping decisions were taken with the user on 2026-08-17:

- **`Esc` belongs to vi.** In vi mode it returns to normal mode; it no longer
  accepts the buffer. Leaving the editor becomes `:wq`, `:x`, `ZZ`
  (accept) and `:q!`, `ZQ` (cancel), with `:q` cancelling when the buffer is
  unmodified and refusing when it is not (§21); `:w` accepts too, except under `editfile`,
  where it writes the file and stays (§14). `Brk` — which is Shift + `Esc` — keeps
  its unconditional cancel, in both modes, as the way out that never depends
  on which mode you are in.
- **Ship without undo**, then tier it: **one level** where the editor buffers
  fall back to SRAM, a **full journal** where they land in the aux/PSRAM
  region. Undo is M4 and does not gate the mode.
- **`setvimode true`**, one flag reaching all five entry points, **not** a
  parallel `vi` / `viall` / `vin` / `vins` / `vifile` primitive family.

## 1. Goal

Give the full-screen editor a modal, vi-style key layer, so that editing on
the PicoCalc's keyboard is done with unmodified letter keys instead of
control chords.

This is not a second editor. Everything the editor draws, scrolls, highlights
and remembers stays exactly as it is; only the interpretation of a keystroke
changes.

## 2. Why a mode, not a second editor

[`devices/picocalc/editor.c`](../devices/picocalc/editor.c) is 2,032 lines and
almost none of it is key handling. It is the scrolling region (which
[B30](bugs.md) and [B31](bugs.md) both came out of), dirty-rectangle tracking,
syntax highlighting, horizontal scroll with arrow glyphs, cursor-blink palette
packing, and the memoised line index. A `viall` that opened a *different*
editor would either fork all of that or drift from it — and the drift would
land on the parts that were hardest to get right.

The keystroke layer is the only thing that differs, and it is a few hundred
lines. So: one editor, two key layers.

The entry-point spelling is a separate question and is settled in §7.

## 3. The keyboard decides the bindings

[`keyboard-firmware-notes.md`](keyboard-firmware-notes.md) is the authority on
which modifier + key pairs physically exist: the MCU substitutes a shifted
key's `symb` and **swallows the key entirely when `symb` is zero**, which is
why Shift + Left/Right, Shift + Space and Shift + Backspace send nothing and
why word movement had to become Ctrl + arrow.

Every character vi needs was checked against the matrix in that document:

| vi needs | reachable as |
|---|---|
| `h j k l w b e x p u i a o r s J` and all letters | unmodified; shift gives uppercase (`A I O G C D P R S X Y J`) |
| `:` | Shift + `;` (r4c6) |
| `$` `%` `^` | Shift + 4 / 5 / 6 (r1c5, r3c4, r3c3) |
| `{` `}` | Shift + `[` / `]` (buttons) |
| `<` `>` | Shift + `,` / `.` (r6c1, r4c0) |
| `/` `?` | r2c4 and its shift |
| `~` | Shift + `` ` `` (r0c5) |
| `0` | button `0/)` |

**Nothing vi wants lands on a swallowed chord.** That is a better result than
the editor's existing chord bindings got, and it is the strongest argument
for the feature: on a keyboard where two modifier combinations do not exist at
all and Ctrl is an awkward reach, a command language built from unmodified
letters fits the hardware better than the control chords do.

### 3.1 The `Esc` conflict

Today `Esc` **accepts the buffer and runs its definitions**, and `Brk` is
Shift + `Esc`. In vi, `Esc` is the most-pressed key on the board. Left as it
is, muscle memory would exit the editor and execute the workspace several
times a minute.

Resolved per the decision above: in vi mode `Esc` is vi's. Insert and visual
modes return to normal on `Esc`; in normal mode with a pending count or
operator it clears the pending state; in normal mode with nothing pending it
does nothing. Exiting is explicit — `:wq`, `:x`, `ZZ` accept;
`:q!`, `ZQ` cancel; `:q` cancels too, but only when the buffer is unmodified,
and otherwise reports `E37: no write since last change` on the footer (§21).
`:w` accepts
as well, except under `editfile`, where there is a file to write and it writes
it without leaving (§14).

Shift + `Esc` (`Brk`) still cancels immediately from any mode. It is the one
key whose meaning does not depend on the mode, which is what makes the mode
safe to be wrong about.

Normal mode is not affected in the other direction either: with
`setvimode false` — the default — the editor behaves exactly as the reference
describes today, `Esc` included.

## 4. Modes

| Mode | Entered by | Cursor | Footer |
|---|---|---|---|
| Normal | vi-mode editor entry; `Esc` | block | `-- NORMAL --` |
| Insert | `i I a A o O s S c{motion} C` | underline | `-- INSERT --` |
| Visual | `v` (charwise), `V` (linewise) | block, selection in reverse video | `-- VISUAL --` / `-- VISUAL LINE --` |
| Command line | `:` `/` `?` | underline, in the footer | the typed line |

Normal mode adds a ruler: the cursor's line number, right-justified in the same
footer row, repainted whenever the cursor changes line. Only normal mode has the
room — the other three modes' footers can run the width of the row.

Two of these are already built. The cursor style is already switched between
block and underline by `lcd_set_cursor_style`, and block already means "a
selection is active" — so normal/insert maps onto it with no new drawing code.
Visual mode maps onto the existing `selecting` / `select_anchor` anchor
machinery **exactly**: `v` is `Ctrl` `B`, and `d` / `y` / `c` / `>` / `<` on a
visual selection are `editor_delete_selection`, `editor_copy_selection` and
`editor_decrease_indent` / `editor_increase_indent`, which all exist.

The command line is the footer prompt that incremental search and replace
already own (`editor_draw_footer`, `EDITOR_PROMPT_COLS`); `:` and `/` are two
more prompts in the same 40-column row, with the same field editing that
`Ctrl` `R`'s replacement field already implements.

## 5. The command set

Pinned here so the tests can be written against it. Every motion takes a
count; every operator takes a count and a motion.

### 5.1 M1 — normal mode

**Motions** — `h j k l`, `w b e W B E`, `0 ^ $`, `gg G`, `{ }` (to the
previous/next blank line), `Ctrl` `F` / `Ctrl` `B` (page), `Ctrl` `D` /
`Ctrl` `U` (half page), `:{n}` (go to line *n*).

`Ctrl` `F` is incremental search in the non-vi editor. In vi mode it pages,
and `/` searches; vi mode owns its keys and the two never coexist.

**Operators** — `d c y` and `> <`, each over a motion, each doubled for the
linewise form (`dd cc yy >> <<`).

**Single-key edits** — `x X`, `D C Y S`, `s`, `r{char}`, `J`, `~`, `p P`.
`D`/`C`/`Y` are the `d$`/`c$`/`yy` synonyms vi defines them as, not separate
code paths.

**Insert entry** — `i I a A o O`.

**Repeat** — `.`.

**Search** — `/` `?` `n` `N`, over `editor_search_find`, which is already a
wrapping case-insensitive matcher with a direction flag.

**Ex** — `:w :q :q! :wq :x :{n} :s/a/b/ :s/a/b/g :%s/a/b/ :%s/a/b/g`.
`:%s` is `editor_search_replace_all`, which already counts its matches before
moving a byte and refuses a rewrite that would not fit.

### 5.2 M2 — visual mode and the rest

`v` `V`, with `d y c x > < ~` and `p` over the selection; `o` to swap the
anchor and the cursor. Then `f F t T ; ,` (character search within the line)
and `%`.

**`%` is worth more here than in a typical vi.** Logo is brackets all the way
down, the editor already computes bracket nesting depth per line for syntax
highlighting (`editor_compute_depth_at_line`), and `d%` over a `[...]` is the
single most useful destructive edit in a Logo program.

### 5.3 Deliberately out of scope

Named registers (the unnamed register is the existing copy buffer, and there
is one), marks, macros (`q` / `@`), `Ctrl` `V` block visual, windows, and
`:e` / `:r` file commands — the editor's file relationship is fixed by which
primitive opened it. None of these earn their code on a 40 × 30 screen.

## 6. Architecture

`editor.c` has no host build — it wants the keyboard, the screensaver and the
screen driver — so anything implemented inside its main loop is untestable.
That is precisely why
[`editor_search.c`](../devices/picocalc/editor_search.c) and
[`editor_lines.c`](../devices/picocalc/editor_lines.c) were carved out. This
is the third instance of the same pattern, and the largest.

### 6.1 `editor_vi.c` is a pure state machine

New `devices/picocalc/editor_vi.{c,h}`. It touches no LCD, mutates no buffer,
and knows nothing about screen rows. It is fed a key and the buffer, and
returns *what should happen* as byte offsets:

```c
typedef enum { VI_NORMAL, VI_INSERT, VI_VISUAL, VI_VISUAL_LINE, VI_CMDLINE } ViMode;

typedef struct {
    ViMode  mode;
    int     count;          // digits typed so far, 0 = none
    char    pending_op;     // 0, 'd', 'c', 'y', '<', '>'
    int     op_count;       // count typed before the operator
    char    pending_prefix; // 0, 'g', 'Z', 'f', 'F', 't', 'T', 'r'
    ViAction last_change;   // the record '.' replays
    char    cmdline[EDITOR_VI_CMDLINE_MAX + 1];
    size_t  cmdline_len;
} ViState;

typedef enum {
    VI_ACT_NONE,        // key consumed, nothing to do (building a count, etc.)
    VI_ACT_MOVE, VI_ACT_DELETE, VI_ACT_YANK, VI_ACT_CHANGE,
    VI_ACT_PASTE_AFTER, VI_ACT_PASTE_BEFORE,
    VI_ACT_INDENT,      // count is +1 or -1 tab stops
    VI_ACT_INSERT_CHAR, VI_ACT_REPLACE_CHAR,
    VI_ACT_OPEN_BELOW, VI_ACT_OPEN_ABOVE, VI_ACT_JOIN, VI_ACT_TOGGLE_CASE,
    VI_ACT_SEARCH, VI_ACT_REPLACE_ALL,
    VI_ACT_ACCEPT, VI_ACT_CANCEL,
    VI_ACT_BEEP,        // unrecognised; footer message in `msg`
} ViActionKind;

typedef struct {
    ViActionKind kind;
    size_t start, end;      // half-open byte range, start <= end
    bool   linewise;        // range is whole lines including the newline
    char   ch;              // VI_ACT_INSERT_CHAR / VI_ACT_REPLACE_CHAR
    int    count;
    const char *msg;        // footer text for VI_ACT_BEEP
} ViAction;

// The whole interface. Pure: `buf` is read-only and nothing is mutated
// but `st`.
bool editor_vi_key(ViState *st, const char *buf, size_t len, size_t cursor,
                   char key, ViAction *out);
```

Motions are pure functions of `(buf, len, pos, count)` returning an offset;
operators pair the cursor with a motion's result into a range. The word
motions reuse `editor_word_left` / `editor_word_right`, already in
`editor_lines.c` and already host-tested, so `w` and `b` cross lines the way
Ctrl + arrow does today and need no new scanning code.

### 6.2 The dispatcher in `editor.c`

One branch near the top of the main loop:

```c
if (editor.vi_mode && editor_vi_key(&editor.vi, editor.buffer,
                                    editor.content_length,
                                    editor.cursor_pos, key, &act)) {
    editor_vi_apply(&act);   // then the existing dirty/redraw tail runs
    key = 0;
}
```

`editor_vi_apply` is the only new code in `editor.c` and is a switch that maps
each action onto operations the file already has — `editor_delete_selection`
for a range delete, `editor_copy_selection`, `editor_paste`, the indent pair,
`editor_search_find`. Where an action has no existing operation (a charwise
range delete that is not a selection, say), it is the same three lines
`editor_backspace` already uses: `memmove`, adjust `content_length`, call
`editor_lines_edit`.

**Every mutation must call `editor_lines_edit`**, or the line memo silently
returns wrong line numbers. That is the failure mode `test_editor_lines.c`'s
randomised differential test was written to catch, and §10 extends it.

The existing dirty-tracking tail of the loop is reused unchanged: an action
sets `dirty_flags` the same way a key case does today, and the scroll-by-one
optimisation, the h-scroll bookkeeping and the redraw all follow.

## 7. Logo surface

```
setvimode true
setvimode false
vimode?            ; outputs true or false
```

One flag, read by `run_editor_and_process` and `prim_editfile` in
[`core/primitives_editor.c`](../core/primitives_editor.c) and passed to the
editor as an argument, so **all five entry points get it** — `edit`, `edall`,
`edn`, `edns` and `editfile`. `editfile` is where it pays off most, since the
buffer is now 256 KB on a PSRAM board and the files opened in it are large.

The flag needs to reach the device layer without `editor.h` growing a
dependency on the interpreter. The existing `LogoConsoleEditor` vtable has one
`edit(buffer, size)` entry; it gains a second field rather than a wider
signature, so the mock device and the host build are unaffected:

```c
typedef struct {
    LogoEditorResult (*edit)(char *buffer, size_t buffer_size);
    void (*set_vi_mode)(bool on);   // optional; NULL on consoles without one
} LogoConsoleEditor;
```

`setvimode` is a no-op returning success on a console whose editor has no
`set_vi_mode`, which is what the mock and the host REPL do.

Persistence across sessions is **not** in this design. The flag is a session
setting like the palette, and a `startup` file can set it — worth revisiting
only if the user asks.

## 8. Undo (M4), tiered

Deferred past the mode itself, and tiered by where the editor buffers landed —
which is already a run-time decision, not a build one (`primitives_editor_init`
takes the aux region or falls back to an SRAM heap pair).

**The journal.** A ring of variable-length records, each enough to reverse one
change:

```c
typedef struct {
    uint32_t pos;            // where the change starts
    uint32_t inserted_len;   // bytes to remove to undo
    uint32_t deleted_len;    // bytes that follow, to re-insert
    // char deleted[deleted_len];
} ViUndoRecord;
```

Undoing is: remove `inserted_len` bytes at `pos`, insert the saved
`deleted_len` bytes there, move the cursor to `pos`, and call
`editor_lines_edit(pos)`. Redo is the same record read the other way, so `Ctrl`
`R` costs nothing extra once undo exists.

**An insert session is one record.** From `i` to `Esc` the typing coalesces
into a single record that grows in place. This is what vi does, and it is what
makes a one-level tier tolerable rather than useless — one undo step reverses
the whole insertion, not the last character.

**The two tiers:**

- **PSRAM.** `primitives_editor_init`'s single region block becomes
  `2 * LOGO_EDITOR_PSRAM_BUFFER_SIZE + LOGO_VI_UNDO_PSRAM_SIZE`, still taken
  all-or-nothing for the reason the comment there already gives. 64 KB is the
  starting figure — hundreds of edits on a large file, and negligible against
  8 MB.
- **SRAM.** One record, capped at `LOGO_VI_UNDO_SRAM_SIZE` (1 KB to start), in
  `core/limits.h` per the project's rule about fixed capacities. It covers
  `x`, `dw`, `dd`, an insert session and a `d}` over a normal procedure.

**When a change will not fit**, the journal is cleared and undo reports
`Nothing to undo` on the footer for that step, rather than half-reversing
anything. `:%s` over a large buffer is the obvious case. Recording that limit
in the reference matters more than raising it.

The journal is reset on editor entry, alongside `editor_lines_reset`.

## 9. Memory

`ViState` is roughly 40 bytes plus the command-line field (32), and `ViAction`
lives on the stack. Against `pico+2w` at **91.19 %** SRAM, M1–M3 are free.

M4's SRAM tier is the only real cost. The boards link at **91.19 %**
(`pico+2w`) and **92.52 %** (`pico2`) of 520 KB, and what is left is the same
heap the fallback editor buffers are taken from when there is no aux region — so
the journal is competing with the edit buffer itself, not with slack. **Measure
the free heap after `primitives_editor_init` before settling
`LOGO_VI_UNDO_SRAM_SIZE`**; 1 KB is a starting figure, not a budget. It belongs
in `limits.h` where it can be seen beside every other capacity.

**Measured 2026-08-20, and the warning above was well placed** — that heap was
already oversubscribed and nobody had checked: the fallback was *two* buffers of
`LOGO_EDITOR_BUFFER_SIZE` (24576 apiece from the presets), and two of those plus
this journal plus `repl_init`'s pair came to 58,368 bytes against a 56,644-byte
heap on `pico2w`, which panicked at boot ([B44](bugs.md)). The second buffer was
holding one procedure rather than the file, so it is now
`LOGO_EDITOR_PROC_BUFFER_SIZE` (4096) and the pair is no longer symmetric; the
presets were rebalanced against each board's heap at the same time. The journal
survived at its 1 KB starting figure — it was never what did not fit. Free heap
after `primitives_editor_init` is now 28,544 (`pico2`), 27,460 (`pico2w`) and
18,108 (`pico+2w` with its PSRAM down), so a larger SRAM journal is affordable
today if `u` on a board without PSRAM is ever worth more than that headroom.

M6 costs no SRAM at all — the pattern is interpreted out of `ViState`'s
existing fields — but it is the first part of vi mode to put real depth on the
**stack**, ~1.6 KB against `PICO_STACK_SIZE` of 4096 for a deep pattern. §16.10
has the breakdown; like M4's figure it is to be measured on a board, not
assumed.

## 10. Tests

New `tests/test_editor_vi.c`, built exactly as `test_editor_lines` and
`test_editor_search` are: `add_executable` over the one device source, no
LCD, no mock console.

- **Every motion**, with count 0, 1 and *n*, at the start of the buffer, the
  end of the buffer, the start and end of a line, on an empty line, and on the
  last line when it has no trailing newline.
- **The operator × motion matrix** — every operator against every motion, with
  the resulting range checked against a hand-computed offset pair.
- **Linewise vs charwise ranges**, especially `dd` on the last line (which must
  take the *preceding* newline, since there is no following one) and `yy` on a
  one-line buffer.
- **Degenerate ranges** — `d0` in column 0, `dw` at the end of the buffer, `G`
  past the last line, `3dd` with two lines left; all must produce an empty
  range or clamp, never an inverted one.
- **Mode transitions** from every mode on `Esc`, including with a count and an
  operator pending.
- **`.` reproduces the previous change** — the same action struct, applied at
  the new cursor.
- **Ex parsing**, including the malformed: `:s` with no delimiter, `:w` with
  trailing junk, `:999999` past the end, an unterminated `:%s/a`.
- **A randomised differential run** in the style of `test_editor_lines.c`:
  drive a buffer through thousands of random vi commands with a reference
  implementation of the buffer beside it, asserting after every step that the
  contents match, the cursor is within bounds, and the line memo agrees with a
  count-from-the-start reference. This is what catches a missed
  `editor_lines_edit`; the hand-written cases did not, last time.

What a host test cannot reach, as ever: the footer drawing, the cursor style
and the mode indicator. Those are a hardware check on the Pico Plus 2 W.

## 11. Milestones

| | Scope | Gate | |
|---|---|---|---|
| **M1** | `editor_vi.c` state machine + normal mode (§5.1), `setvimode`, dispatcher | `test_editor_vi.c` green; hardware check of the mode indicator, cursor style and the `Esc` contract | **built and checked on a board 2026-08-18** |
| **M2** | Visual mode, `f F t T ; ,`, `%` (§5.2) | the same, plus `d%` over nested brackets | **built and checked on a board 2026-08-18** (`d%` sent the manual back for a correction, not the code — B35) |
| **M3** | Reference manual chapter (§13) | — | **built 2026-08-18** |
| **M4** | Undo, both tiers (§8) | `u`/`Ctrl` `R` in the randomised differential run; SRAM tier verified on a `pico2` build | **built 2026-08-18**; the SRAM tier is a `malloc` that undo does without if it fails, so §9's measurement stopped gating it (§14) |
| **M5** | Text objects, words and brackets (§15) | `di[` from inside a nested group, `vi[` selecting one, both in the randomised run | **built and checked on a board 2026-08-18**; opened by B35, and it needed no `editor.c` change at all |
| **M6** | Patterns in `:s`, `/` and `?` (§16) | `/\<n\>` walked with `n`/`N` through the wrap, then `:%s//count/g` renaming every whole-word `n` and no `then`, and one `u` putting it back | **built and checked on a board 2026-08-18** ([`editor_pattern.c`](../devices/picocalc/editor_pattern.c)); the gate passed and its stack-measurement half found B36 instead — a hang, not the overflow §16.10 expected (§16.13) |

| **M7** | `Ctrl` `G`, `:.=`, `:=` (§17) | the report on a buffer longer than a screen, before and after a change | **built and checked on a board 2026-08-18** |
| **M8** | `*` `#`, `` ` `` `'`, `gd`, `zz` `zt` `zb` (§18) | `*` on a one-letter procedure name walking only whole words, `` ` `` back from a `G`, `gd` across an `edall` buffer, `zz` after a search | **built and checked on a board 2026-08-18** |
| **M9** | Ex ranges (§19) | `:2,7s`, `:.,+4s` and a `V` selection followed by `:` running over exactly the lines it covered | **built and checked on a board 2026-08-18** |
| **M10** | `.` repeats an insert (§20) | `cwfoo` then `.` on the next word, and `3.` after it | **built and checked on a board 2026-08-19**; probing it found B43 (§20.5) |
| **M11** | `:m` and `:t` (§22) | a procedure moved from the foot of an `edall` buffer to the top with `:'<,'>m0` and one `u` putting it back, and a `:t` of a block longer than the 1 KB copy buffer | built 2026-08-20, **not yet checked on a board** |
| **M12** | `:g` and `:v` (§23) | `:g/^;/d` and one `u` over an `edall` buffer, `:v/^to /d`, `:g/x/s//y/g`, and what the 1 KB journal does with a big one on a `pico2` | **designed 2026-08-20, not built** |

M1 is the whole feature as far as a user is concerned; M2 is what makes it
pleasant, M4 is what stops it being annoying, and M5 is the one command a
board session asked for that the mode could not say (§15). M6 is the only one
that adds no keys: it changes what the text inside four existing commands
means, and it is the first to break something that works today (§16.3).

## 12. Rejected alternatives

- **A `vi` / `viall` / `vin` / `vins` / `vifile` family.** Five more entries in
  a primitive table that P10 measured as a binary search over ~390, five more
  reference entries, and two names for every editing command a user has to
  remember. One flag reaches all five entry points for the cost of one.
- **A second editor implementation.** Forks 2,032 lines, including the scroll
  region that two bugs came out of.
- **Modal state inside `editor.c`'s main switch.** The obvious way to write
  it, and untestable — `editor.c` has no host build, which is the whole reason
  `editor_search.c` and `editor_lines.c` exist as separate files.
- **Keeping `Esc` as accept and binding normal mode elsewhere.** Every
  candidate is worse: it is the key vi users press most, and `Brk` is
  Shift + `Esc`, so the accept/cancel pair stays on one key either way.
- **Registers, marks and macros.** Cost without payoff at 40 × 30.
- **A full undo journal on every board.** On the SRAM tier the journal shares
  a heap with the fallback edit buffers (§9); a journal that can be exhausted
  by one `:%s` is worse than a bounded one that says so.

## 13. Reference manual changes

- A **Vi Mode** section in *Using the Logo Editor*, after *Incremental Search*:
  the mode table, the full command set from §5, and the exit contract.
- The existing editor chapter gains a sentence saying its key table describes
  the default mode.
- `setvimode` and `vimode?` in the primitives section.
- The undo limits from §8 recorded in *Supported Pico Boards*, beside the
  editor buffer sizes that are already tiered there.
- **M6**: one pattern table in the Vi Mode section covering `:s`, `/`, `?`,
  `n` and `N` together — one dialect wants one table — with the worked examples
  from §16.2, and a sentence saying plainly that `.` `*` `[` `^` `$` are no
  longer ordinary characters in a pattern and are escaped with a backslash. A
  change in what an existing command matches has to be written down where the
  command is (§16.3), and for `/` that is the entry a user reads most. The
  editor's *own* Ctrl+F search is unchanged and still literal, which the
  default-mode key table should now say, since the two are no longer the same
  thing.

## References

- [`Pico_Logo_Reference.md`](../reference/Pico_Logo_Reference.md) — *Using the
  Logo Editor*, and the editor key table
- [`keyboard-firmware-notes.md`](keyboard-firmware-notes.md) — the keymap, and
  which modifier + key pairs exist at all
- [`bugs.md`](bugs.md) — B30, B31 (the editor's scrolling region)
- [`roadmap.md`](roadmap.md) — the 2026-08-17 entries on the editor buffer, the
  line memo and Ctrl + arrow word movement

## 14. What the build changed

Written after M1--M3 landed on 2026-08-18. Everything not listed here was built
as designed.

- **`w`/`b`/`e` are vi's three character classes, not `editor_word_left` /
  `editor_word_right`.** §6.1 said reuse them. They split on blanks only, which
  is `W`/`B`/`E` -- and the argument §5.2 makes for `%` ("Logo is brackets all
  the way down") is the same argument against a `dw` that takes the `]` with
  the word before it. `W`, `B` and `E` are the blank-separated forms, and are
  where `editor_word_left`/`right`'s behaviour lives. Cost: about 40 lines.
- **`editor_vi_key` takes an `int`, not a `char`.** `char` is unsigned on
  arm-none-eabi and signed on the host, and the key codes the editor cares
  about (`KEY_ESC` is `0xB1`) are above `0x7F`. The dispatcher passes
  `(unsigned char)key`. The existing switch in `editor.c` has the same hazard
  and warns about it on the host; that is pre-existing and untouched.
- **`editor_vi_substitute` writes to the buffer**, which is the one exception
  to §6.1's "mutates nothing". A substitute is a loop with a capacity check and
  an off-by-one at every step -- exactly what wants a host test -- and
  expressing it as a byte range would need one action per match. It counts
  every match before moving a byte, as `editor_search_replace_all` does, so a
  result that will not fit leaves the text alone rather than half rewritten.
- **The command line's cursor stays in the content area**, where §4 put it in
  the footer. That is what incremental search already does, and moving it would
  have meant a second special case in `editor_position_cursor` alongside the
  replacement field's.
- **`.` does not replay an insert session.** §5.1 lists `.`, and it repeats
  every change that finishes on its own -- `x`, `dw`, `dd`, `p`, `r`, `~`, `J`,
  `>>` -- by replaying the recorded keys, so a repeat at a new cursor works out
  its own motion (which is what §10's test asks for). It does not repeat `cw`
  or `o`, because nothing records the text typed after them. That record is
  M4's journal; until it exists, repeating half a change would be worse than
  refusing. **Superseded by M10 (§20)**, which records the text without a
  journal: what was typed is the span the session left in the buffer.
- **`G` and `:{n}` count the empty line a trailing newline leaves.** Vi does
  not; this editor does -- `editor_count_lines` counts it, the editor draws it,
  and the cursor keys go there. Agreeing with the editor beats agreeing with
  vi.
- **Two action kinds were added.** `VI_ACT_PASTE_OVER` for visual `p`, which is
  a delete and a paste that must not overwrite the copy buffer in between, and
  `VI_ACT_QUIT` for `:q`, so that the "have you changed anything" question is
  answered in `editor.c`, which knows, rather than in the state machine, which
  does not. (`VI_ACT_QUIT` was removed in §21: the state machine does know —
  `st->modified` is the flag the `Ctrl` `G` ruler already reads — and `:q`
  cancels rather than accepting, so there was nothing left for `editor.c` to
  decide.)
- **`:w` writes where there is something to write to** (2026-08-18, a third
  action kind, `VI_ACT_WRITE`). **Found on a board**, editing a file: the design
  above made `:w` a synonym for `:wq`,
  which is right for `edit`/`edall`/`edn`/`edns` -- the buffer is the workspace,
  and the only way to "save" it is to leave and let the REPL read it back -- and
  wrong for `editfile`, where a file is sitting there and vi users type `:w` to
  save as they go. So the caller now hands `edit()` an optional
  `LogoEditorSave` write-back: `editfile` passes one that rewrites the file, the
  workspace entry points pass NULL, and `editor.c` turns `VI_ACT_WRITE` into a
  call to it (footer `written`, `modified` cleared) or, with no callback, into
  the accept it always was. The state machine still knows nothing about files.
- **`r` takes `Enter` and splits the line** (2026-08-18, also found on a board,
  B33). `VI_ACT_REPLACE_CHAR` was specified and built as an overwrite in place
  -- same length, same lines -- so `r` took printable characters only and the
  one replacement vi has that changes the buffer's length had nowhere to go.
  Rather than a fourth action kind, `ch` of `'\n'` means the split, and
  `editor.c` does it with the delete/insert pair that keeps the line memo
  honest. Worth noting as a pattern: like `:w`, the state machine had the
  general answer and `editor.c` had the one fact that made it specific.
- **The editor's screen teardown was factored out.** Accept and cancel were two
  identical fourteen-line blocks; vi's `:w`/`ZZ` and `:q!`/`ZQ` would have made
  them four. They are now `editor_restore_screen`.
- **The capacities went to `core/limits.h`**, per the project's rule, so
  `editor_vi.h` includes it the way `editor.c` includes
  `core/syntax_highlight.h`. `EDITOR_VI_PAGE_LINES` stayed in `editor_vi.h` --
  it is a layout constant, not a capacity -- with a `_Static_assert` in
  `editor.c` that it still matches `EDITOR_VISIBLE_ROWS`.

**Measured cost.** `ViState` is one struct inside `EditorState`: SRAM went from
91.19 % to **91.23 %** on `pico+2w` and 92.52 % to **92.56 %** on `pico2`,
about 208 bytes on each. §9's estimate held.

### 14.1 Undo (M4)

- **A record carries both sides of a change, not one.** §8 said "redo is the
  same record read the other way", which is not true of a record that holds only
  the deleted text: putting a change back needs the text that was *inserted*,
  and by then it is out of the buffer. Both sides are stored, and nearly every
  change has an empty one -- a delete inserts nothing, an insert deletes
  nothing. Only `r`, `~` and `:s` carry both, and they are short.
- **A full journal drops its oldest steps; it does not clear itself.** §8 said
  a change that will not fit clears the journal. That is unusable on the SRAM
  tier, where 1 KB overflows as a matter of routine rather than as an
  exception -- clearing would leave that tier with *no* undo, not the one level
  it was promised. So the oldest whole steps are dropped to make room, and only
  a single change larger than the entire journal clears it (nothing before such
  a change can be reversed either, since the buffer is about to leave every
  state the records describe). Whole steps: half a `>>` is worse than none of
  it.
- **`editor_undo.c` is a fourth carved-out module**, for the reason the other
  three exist. The journal is a splice and a stack with a coalescing rule, which
  is exactly the kind of arithmetic that wants a host test, and `editor.c` has
  no host build.
- **Every mutation in `editor.c` calls `editor_note_change`**, next to the
  `editor_lines_edit` it already called and for the same reason: a change the
  journal did not see leaves every record after it describing a buffer that
  never existed. The one rewrite that is *not* recorded is the default editor's
  `Ctrl` `R` replace-all, which vi cannot reach (`Ctrl` `R` is redo there and
  `Ctrl` `F` is a page); it resets the journal rather than lying to it.
- **A step is a keystroke, decided in `editor.c`.** The state machine does not
  know when a command begins -- it finds out on the last key -- so the editor
  calls `editor_undo_begin` before every key *except* while insert mode is
  running. That is what makes `cw` plus the word typed after it one undo, and it
  is the same "editor.c has the one fact that makes it specific" pattern as
  `:w` and `r` `Enter` above.
- **`:%s` records one record per match**, inside `editor_vi_substitute`, which
  therefore takes the journal. §8's obvious case -- a substitute too big to
  undo -- mostly is not one: the matches are far smaller than the span they sit
  in, so a `%s` over a large buffer stays undoable where recording the rewritten
  span whole would not have been.
- **Undo is vi's only.** The journal is handed to the editor as NULL outside vi
  mode, so the default key layer neither pays for recording nor gains a key it
  has nowhere to bind.
- **§9's measurement stopped gating the tier.** The SRAM journal is a `malloc`
  taken after the fallback edit buffers, not a static array, so a board that
  cannot spare 1 KB gets `NULL` and `u` says `Undo is not available` -- there is
  no boot panic to measure one's way around. The static cost is `EditorUndo`
  inside `EditorState`: **91.23 → 91.24 %** on `pico+2w`, **92.56 → 92.57 %** on
  `pico2`. Whether 1 KB is the right SRAM figure is still a board question, but
  it is now a tuning question rather than a blocking one.

- **The store is pushed when the console arrives, not when the editor
  initialises** (B34, found on a board the same day). `primitives_editor_init`
  runs inside `primitives_init`, which `main.c` calls *before*
  `primitives_set_io` — so there is no console to push to yet, and undo was
  unavailable on every board. `primitives_editor_console_ready()` now carries
  both the key layer and the store, from `primitives_set_io` as well as from
  init. The `set_vi_mode(false)` reset §7 specified had the same hole from the
  start and could not show it, since the editor's default is already false.

**Checked on a board, 2026-08-18.** The hardware check M1 and M2 gate on is
done: the mode indicator, the cursor style (block in normal, underline in
insert), the `Esc` contract, `d%`, and `u` / `Ctrl` `R`. Four things came back
from those sessions and every one of them is a case a *test* had pinned as
correct, which is the failure mode a host test cannot see past:

- `:w` under `editfile` should write, not leave (§14).
- `r` `Enter` should split the line ([B33](bugs.md)).
- Undo reached no board at all, because the console is registered after the
  primitives are ([B34](bugs.md)) -- and the test suite could not see it, since
  a stale console survives between tests.
- `d%` was reported as failing and was not: vim 9.1 does the same thing on the
  same line and column. **The manual was wrong**, describing `%` as matching the
  next *opening* bracket when it has always taken the first of all six
  ([B35](bugs.md)). The reading the report expected -- "the group I am inside"
  -- is `di[`, a text object, and this mode has none.

## 15. Text objects (M5)

Opened 2026-08-18, out of [B35](bugs.md). The `d%` report there was not a bug —
the editor and vim 9.1 agree byte for byte — but the reading the reporter
expected, *"the group I am inside"*, is a thing `%` structurally cannot say.
`%` is "the first bracket at or after the cursor on this line, then its match",
so from inside a group it runs backwards and takes half of it. The command that
means what was wanted is `di[`, and this mode had no text objects.

**This is worth more in Logo than in a typical vi**, for the same reason `%`
was (§5.2): the language is brackets all the way down, and *the group I am
standing in* is the unit a Logo program is actually edited in — a `repeat`
body, a `to`-line's parameter list, an `if` branch. `ci[` retypes a list;
`yi[` copies a body onto the clipboard to paste into the next procedure.

### 15.1 The set

| Object | Takes |
|---|---|
| `iw` `aw` | the word under the cursor — `i` the word alone, `a` the word with the blanks after it (or before it, when there are none after) |
| `iW` `aW` | the same, counting anything between blanks as one word |
| `i[` `a[` | the text between the enclosing `[` and `]` — `i` between them, `a` including them. `]` is a synonym |
| `i(` `a(` | the same for `(` `)`; `)` is a synonym |
| `i{` `a{` | the same for `{` `}`; `}` is a synonym |

An object is not a motion: it is only ever the second half of an operator
(`di[`, `ci[`, `yi[`, `>i[`) or a selection in visual mode (`vi[`). `i` and `a`
keep their insert-entry meaning everywhere else, which is the whole reason the
prefix is only recognised with an operator pending or in visual mode.

**Counts.** `2i[` goes out one more level of nesting, `3i[` two — the count is
how many enclosing pairs to climb. On words the count is how many chunks (`iw`)
or words-with-blanks (`aw`) to take, so `d3aw` is three words. Both fall out of
a loop; neither needed a special case.

**Charwise, always.** Vim promotes `dib` to linewise when the inner text
happens to span whole lines. That rule exists to make `dib` on a C block leave
the braces on their own lines, and Logo's groups are not laid out that way
often enough to pay for it.

**A word object stays on its line; a bracket object does not.** `diw` at the
end of a line must not take the break with it — that would join two lines,
which is `J`'s job and never what `diw` was asked for — and vi's own `iw` only
crosses a line because it treats the break as blank space. A group, on the
other hand, spans lines as a matter of routine in Logo: a `repeat` body is
usually written over several, and `di[` that stopped at the first break would
be useless on exactly the case it exists for.

### 15.2 Deliberately not in the set

- **`i"` / `a"`.** The one everybody misses, and it would be **wrong here**: in
  Logo `"` prefixes a word (`pr "connected`) and never closes, so the "pair" it
  would find is two unrelated words' quotes and `ci"` would eat the text
  between them. A language whose quote character is not a delimiter does not
  get a quote object. (`|...|` does pair, but nothing in the corpus uses it
  often enough to be worth a key.)
- **`ip` / `ap`.** A Logo paragraph is a procedure, and `dap` is a real
  command — but `{` and `}` already move by one and `d}` from the `to` line
  does the job, so the object buys a keystroke, not a capability.
- **`ib` / `iB`** as aliases for `i(` / `i{`. Two more keys to remember for
  characters this keyboard has on their own buttons (§3).
- **`it` / `at`.** No tags in Logo.

### 15.3 How it lands

Nothing new in `editor.c`, and **no new action kind**. An object is a byte
range, and every operator already takes one: `apply_operator` exists to turn a
cursor and a motion into `out->start` / `out->end`, and an object produces the
same pair more directly (it is absolute, not relative to the cursor, so it
skips `operator_range`'s inclusive/linewise adjustment entirely).

The dispatcher grows one branch beside the `Z r g f F t T` prefixes in
`normal_key`, taken **only** when an operator is pending or the mode is visual:

```c
if ((key == 'i' || key == 'a') && (st->pending_op != 0 || visual))
{
    st->pending_prefix = (char)key;
    ...
}
```

and `prefixed_key` gains the two cases. The object finders are two functions:

- `word_object` — `iw` is the run of one character class under the cursor
  (blanks are a class, so `diw` on a gap deletes the gap, as vi has it); `aw`
  adds the blanks after it, or before it when the object already ends the line.
- `enclosing_pair` — scan back for an unmatched opener of the wanted kind,
  then forward for its match, counting nesting of that kind only, exactly as
  `match_bracket` does. It is *not* `match_bracket`: that one scans forward
  along the line for a bracket to start from, which is the behaviour B35 is
  about. This one starts from the cursor and goes outwards, and it crosses
  lines, because a Logo group routinely does.

**Visual mode needs no new plumbing either**, which was the one thing that
looked like it would. `editor.c` already copies `editor.vi.anchor` into
`editor.select_anchor` after every action it applies (§6.2's tail), so an
object in visual mode sets `st->anchor` to the object's start and returns a
`VI_ACT_MOVE` to its last character, and the selection follows. The state
machine stays the only thing that knows what was selected.

`.` is free: it replays keystrokes, not ranges, so `di[` repeats at the new
cursor and finds that cursor's group. Undo is free for the same reason it was
for every other operator — the change is one range, applied by the code that
already records it.

A failed object — no enclosing pair, or a `di;` — is `beep` with
`E492: not an editor command`, which is what a failed `%` and a failed `f`
already produce.

**Empty is not a failure.** `di[` on `[]` yields an empty range and does
nothing; `ci[` on `[]` puts the cursor between the brackets and starts
inserting, which is the useful half of that case and needs no special code.

### 15.4 Tests and cost

`test_editor_vi.c` gains a section: each object on a typical Logo line, `a`
versus `i`, counts, nesting, the cursor sitting *on* either bracket, a group
spanning lines, the failure, the empty pair, and the visual forms asserting
both `anchor` and cursor. The randomised differential run gains `[ ] ( )` in
its key alphabet, so it types `di[` and `daw` of its own accord and the line
memo and the undo journal keep being checked against them.

Cost: no new `ViState` fields — `pending_prefix` gains two values — so the SRAM
figure of §14 is unchanged.

### 15.5 What the build changed

**Nothing.** Built 2026-08-18 as described: two finders and one branch in
`prefixed_key`, 19 tests, the whole suite green, and `editor.c` untouched — the
first milestone of this design that needed no departure entry, and the first
whose board session sent nothing back. (The other four each did: `:w`, `r`
`Enter`, undo reaching no board, and `%`.) Checked on hardware the same day and
it behaves as designed. The one thing
that looked like it would need plumbing, visual mode, did not: §6.2's tail
already copies `vi.anchor` out after every action, so setting the anchor in the
state machine was the whole of it.

## 16. Patterns in `:s` (M6)

Opened 2026-08-18, by request, and widened the same day to cover `/` and `?`
as well as `:s` (§16.5). `:s` matches a literal string today, which makes the
one thing a Logo programmer most wants a substitute for — **renaming a variable
or a procedure across a buffer** — the one thing it cannot safely do.
`:%s/n/count/g` rewrites the `n` inside `then`, `min` and `pen`, and there is
no way to say *the whole word `n`*. That single command, `:%s/\<n\>/count/g`,
is the case this milestone exists for; everything else the patterns buy is a
bonus on top of it.

The other four milestones each added keys. This one adds no keys at all: `:s`,
`/`, `?`, `n` and `N` are already bound, already parsed, already undoable.
What changes is what the text between the delimiters *means*, and it changes
for all of them at once — **one dialect, or the editor has two answers to
"what does `.` match" and the user has to remember which command they are
in.**

### 16.1 Why not `<regex.h>`

The obvious implementation is the one in the C library, and **it is not there.**
Checked against the toolchain this project actually builds with
(`~/.pico-sdk/toolchain/14_2_Rel1`, arm-none-eabi 14.2.1):

- `arm-none-eabi/include/regex.h` **exists** — the Henry Spencer / BSD header
  newlib carries. It does not even compile on its own; it needs `<sys/types.h>`
  ahead of it or `off_t` is undeclared.
- **No `libc.a` in the toolchain defines `regcomp`.** Every multilib was
  scanned; the count is zero. Newlib keeps `regcomp.c` and `regexec.c` under
  `libc/posix/`, which the ARM embedded build does not compile in.
- A test link for `cortex-m33` fails with `undefined reference to 'regcomp'`,
  `regexec`, `regfree`.

**The trap is that the host build links it fine**, because macOS libSystem has
it. A `:s` written against `<regex.h>` would leave the whole `ctest` suite
green and the firmware unable to link — a failure that would not appear until
`--preset=pico+2w`, long after the tests said yes. So: **`<regex.h>` may be
included from `tests/`, never from `devices/`.** §16.9 turns that asymmetry
into the strongest test in the milestone.

Two reasons hold even where the library exists, which is why this is not a
workaround being tolerated:

- **`regcomp` allocates.** It compiles to a heap-allocated NFA, and `regexec`
  can backtrack unboundedly on a hostile pattern. On a board where the SRAM
  tier of the undo journal is a `malloc` competing with the fallback edit
  buffers (§9), an unbounded allocator reached from a keystroke is the wrong
  shape.
- **POSIX is the wrong dialect.** `regexec` is leftmost-**longest**; vi is
  leftmost-then-greedy-backtracking, and the two disagree. Matching vi is the
  point, so the library would have to be fought as much as used.

### 16.2 The set

Classic vi/ed BRE, and deliberately only the part of it that earns its keys.
The same set is read by `:s`, by `/` and `?`, and by the `n`/`N` that repeat
them — one dialect (§16.5).

| In the pattern | Means |
|---|---|
| `^` | start of line — only as the first character, a literal `^` anywhere else |
| `$` | end of line — only as the last character, a literal `$` anywhere else |
| `.` | any character except the line break |
| `*` | zero or more of the atom before it; a literal `*` when it is first, or straight after `^` or `\(` |
| `[abc]` `[^abc]` `[a-z]` | a character in the set, or not in it. `]` first and `-` last are literals |
| `\<` `\>` | start / end of a word, zero width — the motivating case |
| `\(` `\)` | a group, up to nine, for `\1`..`\9` and for the replacement |
| `\1`..`\9` | the text an earlier group matched |
| `\.` `\*` `\[` `\\` … | the character itself |

| In the replacement | Means |
|---|---|
| `&` | the whole match |
| `\1`..`\9` | the text that group matched |
| `\&` `\\` | the character itself |

An atom is one character: a literal, `.`, or a class. **`*` never applies to a
group** — `\(ab\)*` is refused, not silently mis-parsed. That restriction is
what bounds the **recursion depth** by the number of atoms; a pattern is capped
at `LOGO_VI_TEXT_MAX` (32) by the ex parser already, so that bound is 32 frames,
not an unknown.

It does **not** bound the running time, and this section used to claim it did —
"removes the nested-quantifier shape that produces catastrophic backtracking
outright", worst case O(line × atoms). That was wrong, and a board found it
(B36): catastrophic backtracking needs only **sequential** quantifiers, not
nested ones, so `.*.*.*x` backtracks combinatorially with no group in sight —
O(line^stars), 189 million steps for three stars on a 256-char line. Refusing
`*` on a group was necessary and nowhere near sufficient. The real bound is the
guard this design talked itself out of: `LOGO_VI_PATTERN_STEPS_MAX` steps per
search call, past which the match is abandoned and reported as
`E486: pattern too complex` (§16.13).

Worked examples, all of them things this editor could not say yesterday:

| | |
|---|---|
| `:%s/\<n\>/count/g` | rename `n` everywhere it is named — `"n`, `:n` and bare — without touching `then` or `pen` |
| `:%s/\([":]\)n\>/\1count/g` | rename only the *variable* `n`, both spellings, leaving a procedure called `n` alone |
| `:%s/  */ /g` | collapse runs of spaces |
| `:%s/ *$//` | strip trailing blanks from this line |
| `:%s/\(to [a-z]*\).*/\1/` | cut a `to` line back to its name |
| `:%s/setpos \[\(.*\) \(.*\)\]/setxy \1 \2/` | reorder a call's arguments |

**`\<` and `\>` are Logo's word boundary, not vi's**, and this is the part the
rename case turns on. A variable is written two ways — `make "n 5` defines it,
`:n` reads it — and both have to be reachable by one pattern. That much comes
free: `"` and `:` are `CLASS_PUNCT` to `char_class`, so a boundary already
falls between the prefix and the name.

What does *not* come free is the other end. Vi's word is `[A-Za-z0-9_]`, and
**Logo's is much wider**: `read_variable` ([`lexer.c:355`](../core/lexer.c))
ends a variable name at whitespace, at `;`, and at one of the eleven
delimiters `[ ] ( ) + - * / = < >` — and nowhere else. So `.` `?` `!` `#` `%`
are all name characters in Logo and punctuation to vi, which makes vi's
boundary quietly wrong on the names Logo programs actually use:

| Text | Vi's `\<total\>` | Logo |
|---|---|---|
| `:total.count` | matches `total` | one name, must not match |
| `empty?` | matches `empty` | one name, must not match |
| `:total+1` | matches | one name then a delimiter — correct |

Renaming `total` and corrupting `total.count` is precisely the failure the
milestone exists to prevent, so **`\<` and `\>` take their class from
`is_delimiter` ([`lexer.c:15`](../core/lexer.c))**: a name character is
anything that is not blank, not `;`, not one of the eleven, and not `"` or `:`
(those two are prefixes, never part of the name, or `"n` would be one word and
`\<n\>` could not see into it).

**`w`, `b` and `e` keep vi's three classes.** The two are not the same question
and should not share an answer: `w` is cursor ergonomics, and §14 already
records why it splits on punctuation in a bracket language — a `dw` that took
the `]` with the word would be wrong more often than right. `\<` names a token.
One is about moving, the other about identifying.

**What no pattern can fix**, and the reference chapter has to say so: `\<n\>`
also matches a *bare* `n`, which in Logo is a procedure call in a different
namespace, while the variable-only `\([":]\)n\>` misses the bare name in
`local [n m]`. Neither is complete, because the language spells one variable
three ways. The working answer is the pair of commands, not a cleverer pattern:
`/\<n\>` and `n` to walk every hit and see what a rename would touch, then
`:%s//count/g`, then `u` if it reached too far. That is the whole argument for
§16.5 and for `:s//new/` sharing the search pattern, and it is why the
milestone gate is four commands rather than one.

(An aside found on the way, not ours to fix: `make "my-var 5` defines a
variable named `my-var`, because `-` is literal inside a quoted word, but `:my-var`
lexes as `:my - var`. Hyphenated variable names are already unusable in this
Logo; patterns neither help nor hurt.)

### 16.3 Escaping is a breaking change, and it stays

`.` `*` `[` `^` `$` `\` are ordinary characters in a `:s` or `/` pattern today
and become special. `:s/3.14/pi/` and `/list[1]` will stop meaning what they
mean now. This is accepted rather than mitigated: it is what vi does, escaping
is `\.`, and the alternative — a `nomagic` switch, or patterns only under a
second command — is more surface than the feature. It goes in the reference
chapter (§13) and in the roadmap entry, because a silent change in what an
existing command matches is exactly the kind of thing a board session finds the
hard way.

**`/` is where this will actually be noticed**, more than `:s`: searching for
`[` or for a `.` in a number is a thing one does by reflex mid-edit, and it
will now need a backslash. That is the price of one dialect, and it is the
right way round — a search that cannot say `\<n\>` is the weaker half of the
pair, since finding every whole-word `n` is how you decide whether the rename
is safe before you run it.

### 16.4 What patterns cost the substitute loop

This is the part with real risk in it, and it is not the matcher.

`editor_vi_substitute` ([`editor_vi.c:1871`](../devices/picocalc/editor_vi.c))
is built around a promise: **count everything first, check the capacity once,
and only then move a byte**, so a substitute that will not fit refuses instead
of leaving the buffer half rewritten. Three things patterns break in it.

**The length arithmetic dies.** Today the second pass is safe because
`new_len = n - count * pat_len + count * rep_len` — every match is the same
length and so is every replacement. With patterns, each match has its own
length and each replacement does too (`&` and `\1` expand). The counting pass
stops counting matches and accumulates `removed` and `added` instead, and the
check becomes `n - removed + added + 1 > capacity`. The return value stays a
match count, because that is what `editor.c` tests for zero
([`editor.c:2021`](../devices/picocalc/editor.c)).

**An empty match can match forever.** `:s/x*/-/g` on `abc` matches the empty
string at every position; `at += pat_len` with `pat_len` of zero never
advances. Vi's rule is the one to copy: after an empty match, emit the
replacement and then step one character. This is the bug that will be written
if the two passes are written separately — and the failure mode is not a hang
but something worse, a count and a rewrite that disagree, which is precisely
the half-rewritten buffer the capacity check exists to prevent.

So **the two passes walk with one shared function.** A `next_match` helper
takes the line bounds and a position and yields the next match's start, end and
groups, empty-match step included; the counting pass calls it to accumulate,
the rewriting pass calls it to splice. They cannot drift because there is only
one of them.

**The replacement needs to exist somewhere.** Undo records the old bytes and
the new bytes ([`editor_undo.c`](../devices/picocalc/editor_undo.c)), and the
old bytes are still in place before the `memmove` — that is why the existing
loop records first. The new bytes have to be materialised, so the rewriting
pass expands into a stack buffer of `LOGO_VI_SUB_EXPAND_MAX` (256) and hands
`editor_undo_record` a pointer into it, which is the same call shape as today.
**An expansion that overflows it is caught in the counting pass**, before
anything moves, and reported as `E486: substitution too long` — the
all-or-nothing property is kept, not weakened.

What does *not* change: `^` and `$` are per line, and the loop already walks
line by line with `line` and `end` in hand, so the anchors have their bounds
for free. Matching never crosses a line break, so `.` cannot eat one. The undo
record is still one per match, which is what keeps a `:%s` undoable on the 1 KB
SRAM tier.

### 16.5 `/` and `?`, and a rejection that was wrong

Search takes patterns too. **The reason this was first scoped out does not
survive contact with the code**, and the mistake is worth recording because the
rejection read plausibly: *"`/` is incremental, so it matches against a pattern
that is half-typed and therefore usually invalid, and it feeds the highlight
path in `editor.c`, which has no host build."* Every clause of that is about
the wrong code path.

- **Vi mode's `/` is not incremental.** `cmdline_key`
  ([`editor_vi.c:1159`](../devices/picocalc/editor_vi.c)) collects the pattern
  into the command line and emits `VI_ACT_SEARCH` on Return, exactly as `:s`
  emits `VI_ACT_SUBSTITUTE`. The incremental search is the editor's *other*
  one — `editor_search_apply` on the Ctrl+F path
  ([`editor.c:1426`](../devices/picocalc/editor.c)) — which vi mode never
  reaches, because vi owns the key layer. A half-typed pattern is never
  matched, so it never has to be valid.
- **Nothing highlights matches.** `editor.search_text` appears in the draw path
  once, in the footer of the non-vi search prompt. There is no match-highlight
  path to feed.
- **`editor_vi_search` gets shorter.** Its first four lines copy the vi pattern
  into `editor.search_text` purely so the `editor_search_find` two lines below
  can be handed it; nothing else in vi mode reads it back. Calling the pattern
  matcher directly deletes the copy — and with it the `EDITOR_SEARCH_MAX`
  truncation, which is a live hazard the moment the text is a pattern rather
  than a literal, since chopping `\<name\>` to fit leaves a dangling `\`.
  (It cannot fire today: `EDITOR_SEARCH_MAX` and `LOGO_VI_TEXT_MAX` are both
  32. It is the kind of coincidence that stops being true once.)

So `/` costs one function, and it is a search rather than a rewrite, so none of
§16.4's danger comes with it.

**Why a search needs its own walker.** `editor_search_find` scans the buffer as
one flat run of bytes, which a literal can afford and a pattern cannot: `^` and
`$` need line bounds, `\<` and `\>` need to see where a line starts, and `.`
must not swallow a break. So `editor_pattern.c` gains a line-aware find with
`editor_search_find`'s exact contract — wrapping, case-insensitive:

```c
bool editor_pattern_find(const char *pat, size_t pat_len,
                         const char *text, size_t text_len,
                         size_t from, bool forward, size_t *out_pos);
```

Forward: match within the line holding `from`, starting at `from`; then each
following line from its start; wrap at the end of the buffer and stop once
`from` is passed again. **Backward never matches backwards** — it scans the
line forward from its start and keeps the last match that begins before the
limit, then walks to previous lines and wraps. Lines are short and a rescan is
cheap, and it means one matcher direction covers both keys. That single
decision is most of why this is a small addition.

`n` and `N` are free: they re-emit `VI_ACT_SEARCH` with the pattern already in
`st->pattern` ([`editor_vi.c:1745`](../devices/picocalc/editor_vi.c)).

**Validation happens on Return**, in `cmdline_key`, beside the
`E35: no previous search` beep already there — `E486: bad pattern` for anything
`editor_pattern_valid` refuses. Non-incremental search is what makes that the
only place it can be needed.

**A zero-width pattern moves one character at a time.** `/x*` matches the empty
string wherever it is tried, and a forward search starts at `cursor + 1`, so
each `n` steps once. That is what vim does with the same pattern, and it needs
no code.

**`:s//new/` becomes worth having.** `st->pattern` is already shared by `/` and
`:s` — both fill it through `copy_field` — and now they share a dialect too, so
vi's rule that an empty `:s` pattern reuses the last search finally means
something: `/\<n\>` to see what a rename would hit, then `:%s//count/g` to do
it. It is the removal of one rejection in `parse_substitute`
([`editor_vi.c:985`](../devices/picocalc/editor_vi.c)), which today refuses an
empty pattern outright, guarded by `st->pattern_len > 0`. Included, because the
whole point of one dialect is that the two commands are talking about the same
text.

### 16.6 The matcher

A new file, `devices/picocalc/editor_pattern.c` — the fourth instance of the
`editor_search.c` / `editor_lines.c` / `editor_undo.c` pattern, and for the
same reason: it is pure, it has no idea a screen exists, and it gets a host
test that `editor.c` never can. It is **not** called `editor_regex.c`, because
what it implements is vi's dialect and not POSIX's, and a name that promised
`<regex.h>` semantics would be a lie with a well-known meaning.

**There is no compile step and no allocation.** The pattern is interpreted
directly out of the `st->pattern` bytes the ex parser already stored. Nothing
is built, so nothing is freed and nothing can fail to be built at match time.

```c
typedef struct { size_t start, end; } EditorPatternGroup;  // end == start: unset
typedef EditorPatternGroup EditorPatternGroups[10];        // [0] is the whole match

bool   editor_pattern_valid (const char *pat, size_t pat_len);
bool   editor_pattern_search(const char *pat, size_t pat_len,
                             const char *line, size_t line_len,
                             size_t from, EditorPatternGroups g,
                             bool *too_complex);           // B36, §16.13
size_t editor_pattern_expand(const char *rep, size_t rep_len,
                             const char *line, const EditorPatternGroups g,
                             char *out, size_t out_cap);   // SIZE_MAX = no fit
```

(Both departures from the sketch this section first carried are recorded in
§16.12 and §16.13: `editor_pattern_expand` needed an out-of-band overflow value
because an empty replacement is a legitimate length of zero, and
`editor_pattern_search` needed to distinguish a refusal from a miss.)

`editor_pattern_search` is leftmost: try to match at `from`, then `from + 1`,
and so on. Under it sits the recursive greedy matcher — the Pike shape, where
`*` loops over lengths and recurses once per length rather than once per
character, so depth follows atoms and not line length.

`editor_pattern_valid` runs **at parse time**, inside `parse_substitute`
([`editor_vi.c:985`](../devices/picocalc/editor_vi.c)), so `:s/a\(b/x/` beeps
`E486: bad substitute` on the Return that typed it, next to every other
malformed-`:s` case the test already covers. It rejects: a dangling `\`, an
unclosed `[`, an unclosed or unopened `\(`, more than nine groups, a `\1` with
no group 1, and a `*` after `\(...\)`.

`find_in_line` ([`editor_vi.c:1850`](../devices/picocalc/editor_vi.c)) is what
gets replaced. Its current job — hand `editor_search_find` a slice and reject a
match that wrapped — becomes a call to `editor_pattern_search` over the same
slice, which does not wrap at all.

### 16.7 Case

Matching stays **case-insensitive**, as it is today and as `/` is. Changing it
would be a second breaking change on top of §16.3, and it would be the wrong
way round for this language: Logo itself is case-insensitive about names, so
`:%s/\<Total\>/sum/g` finding `total` is the behaviour a Logo programmer
expects from a rename.

The honest consequence is that **`[A-Z]` means "a letter"**, not "a capital",
because a class folds case like everything else. That is a genuine wart and it
is written down rather than hidden. The alternative — case-sensitive `:s`
beside a case-insensitive `/` — is worse: two rules for one editor, and a user
who has to remember which command they are in.

### 16.8 Deliberately not in the set

- **`\r` in the replacement**, to split a line. The rewriting pass carries
  `end` and `limit` as byte offsets it adjusts after every splice; inserting a
  line break mid-loop invalidates the line walk it is standing in. It is not
  hard, it is just not one line, and `:s` is not how anyone splits a line at
  40 × 30. (`editor.c` already calls `editor_lines_reset` after a substitute,
  so the line memo would not be the obstacle.)
- **`\+` and `\?`.** `\+` is `xx*` written twice; `\?` genuinely has no
  equivalent and is still not worth a case in the matcher until something asks
  for it.
- **Alternation `\|`.** It is the feature that would force a real engine —
  leftmost-longest, or a backtracker with the blow-up `*`-on-atoms was
  restricted to avoid.
- **`[:alpha:]` and friends.** `.` and explicit ranges cover it.
- **`&` as a normal-mode command** (vi's "repeat the last `:s`"). One
  keystroke saved over `:s` and Return; it can come with `/` if it comes.
- **Interactive `c` flag.** A confirm-each-match prompt is a mode, and modes
  are the expensive thing in this design. **Its case is stronger after §16.2**
  than it was before: since no pattern can separate a variable `n` from a
  procedure `n` in every spelling, confirming each match is the only thing that
  would make an over-reaching rename safe *before* it happens rather than after.
  Still out, because `/` then `n` inspects the same matches with keys that
  already exist and `u` undoes the whole `:%s` in one keystroke — but this is
  the exclusion most likely to come back.

### 16.9 Tests

A new `tests/test_editor_pattern.c` over the one device source, built exactly
as `test_editor_lines` is, plus a section in `test_editor_vi.c` for `:s` end to
end.

- **The matcher, table-driven**: every construct, at the start of the line, at
  the end, on an empty line; `*` matching zero; the leading-`*`-is-literal
  rule; `]` first and `-` last in a class; `\<`/`\>` at both ends of a line and
  against a word touching punctuation; group capture and `\1` in the pattern.
- **Everything `editor_pattern_valid` must refuse**, each producing the beep
  rather than a match attempt.
- **The empty-match cases explicitly**: `:s/x*/-/g` on a line with no `x`
  terminates, changes each position once, and its count matches its rewrite.
- **The all-or-nothing property**, which is what a bad length accumulation
  breaks: a `:%s` whose result would exceed the capacity must leave the buffer
  byte-for-byte unchanged, and so must one whose expansion exceeds
  `LOGO_VI_SUB_EXPAND_MAX`.
- **A literal-pattern equivalence run**: for random patterns drawn from
  characters with no special meaning, the new substitute must produce exactly
  what the old literal one did. This is the regression net for the four
  milestones already on a board.
- **A differential run against POSIX** — the payoff from §16.1. Tests are host
  builds and macOS libSystem has a real `regcomp`, so
  `test_editor_pattern.c` may include `<regex.h>` and compare, over thousands
  of random pattern/line pairs, `editor_pattern_search` against
  `regcomp(REG_BASIC | REG_ICASE)` + `regexec`. It asserts **the extent of the
  whole match**, not the group boundaries: POSIX's subexpression rule is
  leftmost-longest and ours is greedy-backtracking, and the two are free to
  split `\(a*\)\(a*\)` differently. Any disagreement on extent is a bug in one
  of them and gets read, not asserted away.
- **`\<` and `\>` against Logo's names** (§16.2), which is the rename case and
  so the most load-bearing test in the file: `\<n\>` matches in `"n`, in `:n`
  and bare; it does **not** match inside `:total.count`, `empty?` or `n2`; it
  does match `:n+1`, `[:n]` and `:n-1`, where the next character is one of the
  eleven delimiters. The table is written from `is_delimiter`
  ([`lexer.c:15`](../core/lexer.c)) so that a change there shows up as a
  failure here.
- **`\([":]\)n\>` with `\1`** rewrites both spellings of the variable and
  leaves a bare `n` alone — the group-and-backreference idiom the reference
  chapter recommends, pinned so it keeps working.
- **`editor_pattern_find`, the search walker** (§16.5): a match on the cursor's
  own line ahead of the cursor and behind it, on a following line, wrapping off
  the end and off the start, and the no-match case. Backward search must find
  the **last** match on a line, not the first, which is the one thing its
  scan-forward-and-keep-the-last shape can get wrong. `^` and `$` must anchor
  per line and `.` must never cross a break — asserted on a buffer whose lines
  would join into a match if it did.
- **`/` and `n` against a zero-width pattern** terminate and step one character.
- **`:s//new/` reuses the last `/` pattern**, and still beeps when there has
  never been one.
- **The randomised differential run in `test_editor_vi.c`** gains nothing new —
  it types keys, and `:s` is not typed there — but the undo round-trip it ends
  each round with is what would catch a substitute whose records do not
  reverse it, so a hand-written `:%s`-then-`u` case goes beside it.

`<regex.h>` is host-only and that boundary is worth a comment in the test file
itself, since the whole reason the milestone exists is that a device build
cannot follow it.

### 16.10 Cost

No new `ViState` fields: `pattern`, `replacement` and `sub_global` are already
there, and the pattern is interpreted from them in place. **No static SRAM at
all.**

Stack, inside `editor_vi_substitute` only: `EditorPatternGroups` (80 bytes),
the expansion buffer (`LOGO_VI_SUB_EXPAND_MAX`, 256), and the matcher's
recursion, bounded by `LOGO_VI_TEXT_MAX` atoms at roughly 40 bytes a frame —
call it 1.6 KB against `PICO_STACK_SIZE` of 4096. That is the one number in
this milestone worth **measuring rather than assuming**, and the measurement is
a deep-pattern `:%s` on a board, not a calculation. If it is tight, the lever
is `LOGO_VI_SUB_EXPAND_MAX`, which is the largest piece and the least load
bearing.

**What the board run actually found was that this was the wrong thing to
worry about** (B36, §16.13). The deep-pattern `:%s` written to take this
measurement never returned: the stack was fine — depth really is bounded by the
atom count — and the *time* was unbounded. The figure above is still uncollected
and now matters much less, because the step budget caps how much work a match
can do before it gives up, and the frames it can do it in were never in doubt.

`/` adds nothing to either figure: `editor_pattern_find` walks lines with two
offsets and calls the same matcher, and `editor_vi_search` gets *shorter* by
the `editor.search_text` copy it no longer needs (§16.5).

Flash: one new file of roughly 350 lines, against 91.24 % (`pico+2w`) and
92.57 % (`pico2`) — to be recorded after the build, as M4's was.

### 16.11 Milestone gate

`/\<n\>` then `n` then `N` on a real procedure on a Pico Plus 2 W — one that
contains `make "n`, `:n`, a `then` and at least one name with a `.` or a `?` in
it — walking every spelling of `n` and stopping on nothing else, forwards and
backwards and across the wrap; then `:%s//count/g` to rename them through the
pattern the search left behind, and one `u` that puts all of it back.

That is the milestone's whole argument in four commands — the case it was
opened for, the search half that makes the rename checkable before it is run,
the shared pattern that ties them together, and the property most likely to be
wrong. **The name with a `.` or `?` in it is not decoration**: it is the
difference between vi's word boundary and Logo's (§16.2), and it is the one
thing on this list a host test could pass while a real buffer failed. The wrap and the backward walk are there because they are the parts a
host test asserts and a board has historically disagreed with.

### 16.12 What the build changed

Built 2026-08-18. Three places the code did not match the sketch, each forced
rather than chosen:

- **`editor_pattern_expand` signals overflow with `SIZE_MAX`, not `0`.** §16.6
  wrote "`0` = would not fit", but an empty replacement (`:s/x//`, a delete)
  legitimately expands to zero bytes, and the substitute loop has to tell "it
  fit, and it was empty" from "it did not fit". `SIZE_MAX` is the out-of-band
  value; `0` is a real length again.
- **The too-long case returns a substitution count of `0`.** §16.4 named a
  distinct `E486: substitution too long`, but `editor.c` already turns a count
  of `0` into `No substitution made` — the same all-or-nothing refusal — and
  wiring a second message would have meant touching the one substitute branch
  M6 otherwise leaves alone. The refusal is kept; the wording is the existing
  one.
- **The two passes share `sub_next`, not a live iterator.** §16.4 asked for one
  `next_match` walked by both, but the rewrite pass mutates the buffer and so
  cannot literally share the counting pass's positions. What is shared is the
  function — `editor_pattern_search` plus the empty-match "step one character"
  skip — called with each pass's own coordinates. Because the rewrite always
  resumes past the replacement, the tail it re-scans is the counting pass's
  original bytes shifted, so the two see the same sequence of matches and the
  count cannot drift from the rewrite.

Cost as built: **no static SRAM** (the pattern is interpreted from `ViState`'s
existing fields), and `pico+2w` links at **91.24 % RAM, unchanged** from M4 —
the whole matcher lives in `editor_vi_substitute`'s stack frame. The ~1.6 KB
deep-pattern stack figure (§16.10) is still the one number to take on a board
rather than calculate, and it is part of the outstanding board gate (§16.11).

### 16.13 The board gate found a hang, and the design's reasoning was the bug

Run 2026-08-18. The gate's functional half (§16.11) passed on a Pico Plus 2 W:
`/\<n\>` walked every spelling of `n` with `n`/`N`, forwards, backwards and
across the wrap, stopping on nothing else; `:%s//count/g` renamed them and one
`u` put it back. Logo's word boundary held against the `.` and `?` names that a
host test could pass while a real buffer failed.

The half meant to measure §16.10's stack figure did not. The pattern written to
be deep — `:%s/.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*x/y/` — **left the board
non-responsive**, and the first reading of that was wrong too: it looked like
the stack overflow §16.10 had been braced for. It was not a fault at all. The
device was still matching, and would have been for effectively ever.

**The defect was in this document's reasoning, not only in the code.** §16.2
and §16.6 both argued that refusing `*` on a group "removes catastrophic
backtracking by construction rather than by a guard". Catastrophic backtracking
does not require a nested quantifier — only sequential ones. Measured on the
host, `.*` repeated N times against a line that cannot match costs
O(line^N):

| stars | 256-char line |
|---|---|
| 1 | 33,410 steps |
| 2 | 2,895,619 |
| 3 | 188,939,204 |

The gate's fifteen extrapolate to about forty days on a 4 GHz desktop. An
RP2350 at 150 MHz was never coming back, and nothing in the key loop could
interrupt it.

So the guard goes in after all: `LOGO_VI_PATTERN_STEPS_MAX` (200,000) match
steps per `editor_pattern_search` call, charged per `match_here` entry, past
which the search is **abandoned rather than finished**. The number is measured
rather than picked — real patterns cost tens to hundreds of steps (`\<n\>` on a
69-character line is 13) and the worst legitimate case is a single star failing
on a full-width line at 33,410, so 200,000 is roughly six times the honest
worst case.

A refusal is deliberately **not** a miss. `editor_pattern_search` and
`editor_pattern_find` take a `too_complex` out-parameter,
`editor_vi_substitute` returns `SIZE_MAX` (the out-of-band value it already
used for an over-long expansion), and `editor.c` says `E486: pattern too
complex` where it would otherwise say `pattern not found` or `No substitution
made`. Telling a user their pattern was too dear to run is different news from
telling them nothing matched, and after a visible pause the difference is the
whole message. `editor_pattern_find` stops at the first line that trips the
budget rather than paying it again on every line below, and `:s` refuses in the
counting pass, before a byte moves, so the all-or-nothing property is untouched.

The honest cost: a legitimate but very expensive match can now be refused. That
is a real semantic change and the reason the budget has six times' headroom
over anything measured. Memoising failures over (atom, offset) would have made
the original O(line × atoms) claim true instead of guarded, but it costs ~1 KB
of the 4 KB stack and is unsound with `\1`..`\9`, since `match_here` mutates
capture state — a conditional fast path for backref-free patterns, and the
guard underneath it anyway for the rest. Not worth it for a matcher whose real
patterns cost thirteen steps.

## 17. Where the cursor is (M7)

Three commands that answer one question: `Ctrl` `G`, `:.=` and `:=`. On a
40 × 30 screen with no line numbers down the side and no status column, the
buffer gives no clue where in it you are, and `:{n}` is a command whose one
argument the mode could not tell you.

`Ctrl` `G` reports `[Modified] line 12 of 40 --30%--` — vi's report, minus the
file name. There is none to give: which procedures or which file the editor is
over was fixed by the primitive that opened it (§5.3 rules out `:e`), and
`editor.c` never learns a name. `[Modified]` is the flag `:q` already refuses
on, so the report doubles as the answer to "will `:q` let me out". `:.=` and
`:=` print the current and last line numbers on their own, as ex does.

**A composed message needed a channel.** Every footer text so far has been a
string literal — `ViAction.msg` is a `const char *` the editor holds until the
next keystroke — and these three have to be formatted. The text goes in
`ViState.msg` (`LOGO_VI_MSG_MAX`, 40 bytes, the width of the footer), which
lives exactly as long as the editor session the footer belongs to, and
`VI_ACT_MESSAGE` carries a pointer to it. It is a separate kind from
`VI_ACT_BEEP` because a report is not a complaint, even though `editor.c`
paints both the same way.

That aliasing is the one trap: `editor.c` repaints the footer when
`editor.vi_msg` *changes*, and every composed message points at the same
buffer, so the pointer can stay put while the text moves under it. The
dispatcher tests `act.kind == VI_ACT_MESSAGE` as well, rather than leaving a
stale report on the screen the day two of them run back to back.

Line numbers are counted by walking the buffer (`line_number_of`), not by
asking `editor_lines.c`: the state machine has no memo by design (§6.1), and
one scan per `Ctrl` `G` of a buffer this size is nothing next to the redraw
that follows it.

## 18. Navigating (M8)

Opened 2026-08-18. M1--M7 made the mode a complete way to *edit*; this is the
first milestone about *finding* things. The buffer under `edall` is the whole
workspace and under `editfile` it is 256 KB, and until now the only ways to
reach a place in it were `/` with the name typed out and `:{n}` with a line
number the mode had no way of telling you.

Five commands, and none of them changes a byte.

### 18.1 `*` and `#`

Search for the word the cursor is on, forwards and backwards. **M6 is what made
this cheap**: the pattern is built rather than typed --- `\<` + the word +
`\>` --- so it costs one string and reuses the whole matcher, and `\<`/`\>`
are what make it worth having, since a `*` on `n` that stopped on every `then`
would be useless.

It is the command that pays off most in an `edall` buffer, where every call site
of a procedure is in the same file as its definition.

Three details:

- **The word characters are never metacharacters** (§16.2's set is `^ $ . *
  [ ] \< \> \( \)`), so nothing needs escaping. A word longer than
  `LOGO_VI_TEXT_MAX - 4` is refused rather than truncated: a truncated
  `\<squar` would silently match the wrong thing, which is worse than a beep.
- **The search runs from the start of the word, not from the cursor.**
  Otherwise `#` from the middle of a word finds the word it is standing in.
  `VI_ACT_SEARCH` therefore carries an origin --- which cost nothing, because
  `editor_vi_key` already initialises `out->start` to the cursor and every
  existing emitter left it there.
- **No count.** `n` has never taken one either (§5.1 promised it and the build
  did not), and `3*` is `*nn`.

### 18.2 One mark, not twenty-six

§5.3 rejected marks and was right to: `m{a-z}` is a register file, and this
keyboard has 40 columns of footer to report it in. But *one* mark --- vi's
`` ` `` and `'`, the place the last jump started --- is a different thing, and
it is the half of the feature that was actually missing. `G` on a large buffer
is a one-way trip today, and the only way back is to have read `Ctrl` `G`
first, which is M7 papering over a missing motion.

It is a `size_t` and a `bool` in `ViState`. A jump sets it; jumping to it is
itself a jump, which is what makes the pair a **toggle** rather than a
one-way trip in the other direction.

**What counts as a jump** is `G`, `gg`, `{`, `}`, `%`, `/`, `?`, `n`, `N`, `*`,
`#`, `gd`, `:{n}` and `` ` ``/`'` themselves --- the movements that can leave
the screen. An operator's motion is not one: `d}` is an edit, and the place to
come back to is where the edit was.

**The mark is a byte offset and nothing adjusts it.** After an edit in front of
it, `` ` `` lands near where you were rather than exactly. Vim maintains its
marks through every splice; doing the same here means touching the mark from
every mutation in `editor.c`, which is the `editor_note_change` pattern again
for a tenth of the payoff. `vi_motion` clamps the mark to `len`, so a stale one
is never unsafe --- only approximate --- and the randomised run feeds `` ` ``
and `d`` ` `` among its keys precisely because a stored offset is the thing
most likely to go wrong. Written down in the manual, since it is a difference a
user can see.

### 18.3 `gd`

The definition of the word under the cursor. **Not a pattern search**: a Logo
definition is `to name` at the head of a line, and matching that shape directly
(`find_definition`) is exact where a pattern would need `\s\+` the dialect
does not have, and is shorter than the pattern that would approximate it. Case
folded, as the language is.

This is the same argument §15 made for text objects and §5.2 for `%`: the
command earns its key because of what Logo *is*. With `edall` the workspace is
one buffer, so `gd` is the whole of "go to that procedure", and `` ` `` is the
way back --- the two commands are one feature.

### 18.4 `zz`, `zt`, `zb`

The one part of M8 that had to negotiate with §6.1. The state machine knows
nothing about screen rows, and centring the view is entirely about screen rows.

It stays pure by making the *intent* the action: `VI_ACT_SCROLL` carries a
letter --- centre, top, bottom --- and `editor.c` does the arithmetic against
`EDITOR_VISIBLE_ROWS`. Rows travel one way, out of the machine, which is the
same direction `EDITOR_VI_PAGE_LINES` already travels; had `H`/`M`/`L` been in
scope they would have needed the view as an *input*, which is what keeps them
out.

It is worth the negotiation on a 30-row screen: a `/` or a `G` lands its line
wherever `editor_ensure_cursor_visible` happens to put it, which is often the
last row, with the block it belongs to off the top.

Two build notes:

- **The view never starts past the last screenful**, which is what
  `editor_page_down` already enforces, so `zt` near the end of the buffer moves
  less than it was asked to rather than showing a screen of nothing. Vim would
  scroll and fill with `~`; this editor has never drawn a row that is not a
  line.
- **`VI_ACT_SCROLL` marks everything dirty itself.** The redraw tail keys off
  `editor_ensure_cursor_visible`'s return, which is a delta the action has
  already applied by the time it runs --- so it reports 0 and the scroll would
  otherwise be invisible until the next keystroke.

### 18.5 Still out

`m{a-z}` and the jump list (`Ctrl` `O`/`Ctrl` `I`) --- one mark is the 90 % of
either that fits the screen. `H` `M` `L`, for the reason in §18.4. `Ctrl` `A` /
`Ctrl` `X` on the number under the cursor, which is a want rather than a gap.
`:g/pat/d`, which without a list pane is only a bulk delete that `:%s` and `dd`
already cover. And macros (`q`/`@`), still: `.` and a `:%s` that now takes
patterns absorbed most of what they would have bought.

### 18.6 Cost

`editor_vi.c` 2,107 → 2,331 lines; `editor.c` gains the one `VI_ACT_SCROLL`
case. `ViState` grows by a `size_t` and a `bool`: **91.24 → 91.25 %** of SRAM
on `pico+2w`, **92.57 → 92.58 %** on `pico2`. 19 new tests, and six new keys
added to the randomised run's key pool.

## 19. Ex ranges (M9)

`:s` could say "this line" or "the whole buffer" and nothing in between, which
is the wrong pair for the edit that wants it: renaming inside one procedure of
an `edall` buffer. The rest of ex's address grammar is small enough to be
worth having, so a range is now one address or two, in front of any command
that takes one.

### 19.1 The set

| Address | Is |
|---|---|
| *n* | line *n* |
| `.` | the line the cursor is on |
| `$` | the last line |
| `'<` `'>` | the first and last line of the last selection |
| `+`*n* `-`*n* | an offset — on its own from the cursor's line, or after any of the above; bare `+`/`-` is one line |

`%` stays what it was, and is now just `1,$`. Out-of-range addresses are pulled
back into the buffer rather than refused, which is what `:{n}` already did
through `goto_line`; a backwards range is `E493: backwards range` rather than
vi's "OK to swap (y/n)?", because the 40-column footer has no room to ask a
question and no way to take the answer.

Three commands take one: `:s`, `:=`, and the empty command — a range on its own
goes to its last line, which is exactly what `:{n}` is, so the old special case
for a line number disappeared into the general path along with `:.=`. Every
other command refuses one with `E481: no range allowed`. `:d`, `:y`, `:>` and
`:m` are deliberately still out: `dd`, `yy` and `>>` take counts and work over a
selection already, so a range would be a second spelling of a key that is one
keystroke away.

### 19.2 `'<` and `'>` are the selection, remembered

The one mark (§18.2) is a place in a line; these two are lines, and they are
not the mark — a selection and a jump are different things to want back. They
are a byte pair in `ViState`, taken again on every key visual mode sees, so the
key that *ends* visual mode leaves behind the selection it was given rather
than whatever the anchor decays to afterwards. That one snapshot covers both
the `:` typed inside visual mode and a `:'<,'>` typed long after the selection
was cancelled. The bytes are clamped against the buffer length when an address
resolves them, since the buffer may have been rewritten in between.

### 19.3 `:` in visual mode types the range for you

A `:` over a selection is nearly always about the lines it covers, so
`enter_cmdline` fills the command line with `:'<,'>` when it is entered from
either visual mode. Backspace rubs it out a character at a time and then leaves
the command line, as it always has, so nothing is forced. Vim does the same,
which is the strongest argument for it: the muscle memory already exists.

The selection highlight drops as the command line opens, because the editor
paints it from `ViState.mode` and the mode is now `VI_CMDLINE`. Vim keeps it
lit. Keeping it would mean a second piece of state saying "visual, but the keys
belong to the footer", and the range is now on the command line where you can
read it — the answer to "which lines?" is legible either way.

### 19.4 Cost

`editor_vi.c` 2,331 → 2,453 lines, of which the address parser is 90 and the
old `all_digits` helper gave 15 back. No change in `editor.c`: a range is
resolved to the byte range the actions already carry. `ViState` grows by two
`size_t` and a `bool`, which stays inside the rounding: **91.25 %** of SRAM on
`pico+2w`, **92.58 %** on `pico2`, both unmoved. 13 new tests.


## 20. Repeating an insert (M10)

`.` has repeated the changes that finish on their own since M3. It has never
repeated the ones that end in insert mode -- `i a I A o O`, `c s C S` -- and
M3's note above says why: nothing recorded the text typed after them, and
putting back half a change is worse than refusing. **The reason is gone**, and
not because M4's journal arrived. It was never needed.

### 20.1 The text is a span, not a key log

Vim records the keystrokes of an insert session. This editor cannot: in insert
mode `editor_vi_key` returns false and `editor.c` does its own handling, so the
state machine never sees a character, a backspace or a Return. Recording them
would mean a second entry point called on every keystroke the editor already
handles, and a replay that has to re-interpret backspaces.

It does not have to see the keys, because it sees the buffer. What was typed is

```
buf[insert_origin, cursor)
```

-- the span between where the cursor was when insert began and where it is at
the `Esc`. A backspace inside the session simply leaves less text; so does
retyping a word three times. Nothing has to be interpreted, and the record is
the same bytes the editor will type back.

The one thing the state machine cannot supply is `insert_origin`. For `i` it
would be the cursor it was handed, but for `o` it is past the auto-indent
`editor_vi_open_line` wrote and for `cw` it is where the deleted range started
-- editor decisions, taken after the action is applied. So the editor says:
`editor_vi_insert_began(st, cursor, len)`, one call in the main loop on the
transition into insert mode, and the third thing `editor.c` tells the vi layer
after `modified` and the key itself.

### 20.2 When the span is not the session

A session that only typed forwards satisfies

```
cursor >= origin  and  len - len0 == cursor - origin
```

Anything else -- an arrow key mid-insert, a backspace past the origin, a Del
that ate text that was already there -- moved somewhere the span does not
describe, and so does a session longer than `LOGO_VI_INSERT_MAX`. Those **drop
the record entirely**: `.` then says `Nothing to repeat` rather than repeating
the change before it.

That is the decision worth naming, because vim does the other thing -- it
splits the insert at the arrow and `.` repeats only the tail. On a keyboard
where the arrows are the easy keys and `Esc` is a reach, a `.` that quietly
puts back a piece of what you typed is the worse surprise. Refusing is
legible; a partial repeat is not.

### 20.3 Where the record is made

`commit` records a change when its keys are done. A change that ends in insert
mode is not done then, so `commit` leaves its keys in `stroke` -- nothing else
uses that buffer while insert mode runs -- and `record_insert`, called from the
`Esc`, promotes the pair together or drops both. There is no window in which
half a change is repeatable.

`replay` gains a tail rather than a mode: it feeds the recorded keys as before,
and if they left it in insert mode it puts the mode back and hands the text out
with the action, in two new `ViAction` fields. The editor types it after
carrying the action out -- the delete has made room for it, the open has made
the line -- and steps back off the last character the way `Esc` does. So a
repeat is one action, one undo step, and never leaves the user in insert mode
somewhere they did not ask to be.

### 20.4 The count goes to the command

`3.` after `cwfoo` is `3cw` and one `foo`, which is what vim does. It is not
three `foo`s: `i` takes no count in this editor, so its repeat has none to
take, and the count `replay` already carries is the command's.

### 20.5 Nothing from visual mode is recorded (B43)

Probing what the new recording did with visual mode found a defect that has
been there since M3. A visual command builds over several keys and *each one
completes a command of its own* -- `v` is a mode change, `l` is a motion -- so
each commits and clears the stroke buffer on its way through, and all that
survives of `vld` is the `d`. `commit` recorded that lone `d`, and `.` replayed
it in normal mode, where a bare `d` is an operator waiting for a motion: the
repeat did nothing and left `pending_op` set, so the next key typed was eaten
as its motion.

Recording the whole `vld` is not available as a fix: `replay` feeds every key
the same starting cursor, so a replayed `v`/`l` would select an empty range.
So `commit` records nothing made from visual mode -- `editor_vi_key` sets
`from_visual` before it dispatches -- and the last properly recorded change
stands. M10 made fixing it necessary rather than merely worthwhile: a visual
`c` records typed text too, and the replay would have inserted it without the
delete that makes room for it.

### 20.6 The board

Accepted on a Pico Plus 2 W, 2026-08-19. Worth its own line because the two
places `editor.c` changed -- the `editor_vi_insert_began` call and the block
that types the recorded text -- are the half of this that no host test reaches,
`editor.c` having no host build.

### 20.7 Cost

`editor_vi.c` 2,461 → 2,542 lines; `editor.c` 2,725 → 2,750, which is the two
places named above and nothing else. `ViState` grows by `LOGO_VI_INSERT_MAX`
plus two `size_t`, two `int` and three `bool` -- **104 bytes of SRAM, measured
`data+bss` before and after, the same on `pico+2w` and `pico2`**. 11 new tests;
9 of them fail with the code they cover stubbed out, which is the check that
they test the feature and not the harness.

## 21. `:q` does not accept the buffer (B46)

**Reported from a board**: `edall`, then `:q`, and the workspace was read back
line by line — every procedure redefined, every `make` re-run — from a buffer
the user had asked to leave without saving. §3 wrote `:q` as "accept, but only
when the buffer is unmodified", reasoning that accepting a buffer nobody had
changed puts the workspace back exactly as it was, so the difference did not
matter. **It does matter.** Accepting is not a no-op even when the text is
identical: the REPL executes what it reads, so it re-runs every `make` and
`pprop` line in the buffer, and — the point of the report — `:q` is the key a
vi user presses precisely *because* they want nothing to happen. The reference
already said what the primitives do on accept; nothing said `:q` was exempt,
and it wasn't.

`:q` now cancels. Modified, it still refuses with
`E37: no write since last change` — the change is which way an unmodified
buffer leaves, not whether a modified one may.

**`VI_ACT_QUIT` went with it.** §14's note gave the action kind a reason: the
state machine could not answer "have you changed anything", so it asked
`editor.c`. That was never true — `modified` lives in `ViState`, set by
`editor.c` as the buffer changes and read by `editor_vi.c` for the
`[Modified]` in the `Ctrl` `G` ruler — so `:q` is decided where it is parsed,
in `ex_command`, as a `VI_ACT_CANCEL` or the same beep every other refusal
uses. One action kind and one `editor.c` branch removed, and, because
`editor.c` has no host build and `editor_vi.c` does, the behaviour became
testable: `test_write_and_quit_commands` now pins both halves, and fails on the
old code.

## 22. Moving lines (M11)

§19.1 put `:d`, `:y`, `:>` and `:m` out of the range set together, on the
grounds that "`dd`, `yy` and `>>` take counts and work over a selection
already, so a range would be a second spelling of a key that is one keystroke
away". That is right for three of them and wrong for the fourth. Those three
act **where the cursor is**; `:m` names a **destination**, and no key in the
mode does. There is no `dd` for "put these lines over there".

What it costs to do without is the measure of it. Moving a procedure inside an
`edall` buffer today is `V`, select it, `d`, then get somewhere off-screen with
a `/` search or a `G`, then `p` — four steps, one of which is a navigation the
user has to get right before the text comes back. And the `d` goes through the
copy buffer, which is `LOGO_COPY_BUFFER_SIZE` — **1 KB, and it truncates
silently past it**: `editor_vi_yank_range` clamps the length and drops the
rest, as `editor_copy_selection` does for the editor's own `Ctrl` `C`. So on a
long procedure the workaround is not merely slower, it loses text. `:'<,'>m$`
is one command that cannot.

`:t` (`:co`) is the same code with the delete left out, and is how a procedure
is duplicated to be edited into a variant — which is most of how a Logo program
gets written on a device with no second window.

### 22.1 The set

| Command | Does |
|---|---|
| `[range]m{addr}` | move the range to after line *addr* |
| `[range]t{addr}`, `[range]co{addr}` | copy it there |

The range is the addresses of §19.1 and defaults to the line the cursor is on,
so `:m0` moves this line to the top. The destination is one address, and is
**where `0` becomes a line number that means something**: everywhere else in
the parser an address below 1 clamps to 1, but a destination of 0 is "above
line 1" and is the only way to say it. `$` is the destination for "the end",
and on a buffer ending in a newline it is the empty last line, which lands the
text in the same place either way.

The cursor goes to the first non-blank of the **last** line moved or copied, as
vi's does: after a `:t` that is the copy rather than the original, which is the
one you are about to edit.

A `:m` whose destination is inside its own range is `E134: cannot move into
itself`. So is one just before or just after it (`:2m1`) — vi lets that pass as
a no-op, and here it beeps, because on a 40 × 30 screen the footer is the only
thing that can say a command did nothing. A `:t` inside its own range is legal
and is how a block is doubled. The empty line past the buffer's last newline —
the one `G` lands on — has no text to take anywhere, and says `E16: invalid
range` rather than reporting a move that moved nothing.

### 22.2 The rewrite goes in `editor_vi.c`, beside the substitute

`editor_vi_substitute` is in the state machine rather than in `editor.c` for a
reason the header states: it is a splice with a capacity check and an
off-by-one at every step, `editor.c` has no host build, and describing it as a
byte range would need one action per match. Every word of that is true of a
move, so `editor_vi_move_lines` sits next to it with the same shape — buffer
in, journal in, cursor out — and `editor.c` gains one `case` that calls it and
marks the screen dirty. The whole of the tricky part is then under host test,
including the two cases below.

### 22.3 No scratch buffer, and one undo step

The copy buffer is not used, and this is the point rather than an
optimisation: it is 1 KB, which is the limit being fixed, and it is the user's
register, which a move has no business overwriting.

**The move is a rotation.** Moving `[start, end)` to a destination outside it
is exactly rotating the bytes between the two, which is three reversals in
place: no allocation, no bound on the size of the block but the buffer itself,
and a cost of two passes over the span between source and destination — tens of
KB of byte writes in the worst case an editor of this size can produce, which is
nothing beside the SPI wire it is about to redraw over.

**The journal is told before anything moves.** A move is recorded as two
records — a delete of the span at its old offset, and an insert of the same
bytes at the offset the delete leaves them at — and both are written *before*
the rotate, while the span is still contiguous and can be pointed at. That is
the whole trick that keeps a move off the copy buffer: `editor_undo_record`
needs the bytes, not a buffer to hold them, and they are still in the buffer at
the moment it is called. `:t` records one insert, and its bytes are contiguous
at record time even when the destination is inside the range.

Both records land in the one step `editor_undo_begin` opened for the keystroke,
so `u` puts a move back whole however far it went, and the journal cost is
twice the moved text rather than everything between the two places.

### 22.4 The buffer that does not end in a newline

A line is text and the newline that ends it, and the last line of a buffer that
does not end in one has no newline to carry. Every case in a move touches that:
moving the last line elsewhere leaves the line before it unterminated, and
moving anything *to* the end has nowhere to attach it.

Rather than four special cases, the operation **lends the buffer a newline**
when it touches the tail, does the uniform thing, and takes one back
afterwards. Both are journal records inside the same step, so this costs no
extra `u`, and the buffer is left the shape it was found in — which matters,
because whether the text ends in a newline is the caller's business, not the
editor's.

### 22.5 Tests and cost

The state machine's half is the parse — `:m` with no address, an address inside
the range, `0`, `$`, `'<,'>` — and the rewrite's half is the buffer, driven
through the same `ed_apply` harness the substitute uses, so a test types
`:2,3m0` and reads the text back. The cases that earn their keep are the two
in §22.4, a `:t` whose destination is inside its own range, a move that will
not fit, and `u` after each. The capacity check earned its own case: weighing
the copy *after* lending the buffer a newline left the newline behind on a
refusal, which is a buffer changed by a command that said it changed nothing.

**Cost**: `editor_vi.c` 2,548 → 2,724 lines, `editor.c` 2,749 → 2,766.
`ViAction` gains a `size_t dest`, which is a stack local per keystroke and not
SRAM: **RAM is unchanged to the byte** — 478,532 B on `pico+2w` and 451,712 B
on `pico2`, measured before and after — and flash grows about 1.8 KB on both.
20 new tests; 19 of them fail with the feature stubbed out, split between the
parse and the rewrite, which is the check that they test the feature and not
the harness. The one that passes either way is the copy that does not fit,
where "nothing changed" is what a missing feature looks like too.

### 22.6 Deliberately not with it

- **`:d`, `:y`, `:>` over a range.** §19.1's argument still stands for these.
- **`:g/pat/cmd`.** "Delete every line mentioning this" is the one thing `:%s`
  cannot say, and it is a real gap — it wants its own section rather than a
  ride on this one, and it has one now: **§23**, where the mark set this bullet
  originally priced turns out not to be needed at all.
- **`:w {name}`.** Save-as under `editfile`, decided against in §23.7.

## 23. The second tier (M12, designed and not built)

§22.6 named two more commands and took neither. This section is what was
decided about them, written down so the reasoning is not re-derived from
scratch the next time one of them is asked for. **Neither is built.** `:g` and
`:v` are M12 and have a gate; `:w {name}` is a decision rather than a
milestone.

### 23.1 `:g` and `:v` — the case

`:%s` conditions on the text it is replacing and nothing else. Every editing
job that conditions on *the line* is outside it:

```
:g/^;/d          throw away every comment line
:g/^ *$/d        and every blank one
:v/^to /d        keep only the `to` lines -- an index of the buffer
:g/^to /s//TO /  touch one thing on the lines that match another
```

The first two are what an `edall` buffer wants after a session of commenting
things out, and the third is the only way this editor can answer "what is in
this file" for a `editfile` buffer of 256 KB, where `Ctrl` `G` says where you
are and nothing says what is around you.

`:v` is `:g` with the test inverted, and `:g!` is a synonym for it.

### 23.2 The set

The command after the pattern is **`d` or `s`**, and nothing else:

| Form | Does |
|---|---|
| `[range]g/`*pat*`/d` | delete every line in the range that matches |
| `[range]g/`*pat*`/s/`*a*`/`*b*`/[g]` | substitute on every line that matches |
| `[range]v/`*pat*`/…` , `[range]g!/`*pat*`/…` | the same, on the lines that do not |

`:s` with an empty pattern already means "the last one searched for" (§16.5),
so `:g/x/s//y/g` says the common case — find the lines, rewrite the thing that
found them — without typing the pattern twice.

The range defaults to the **whole buffer**, not to the cursor's line as every
other command in this parser does. That is ex's rule and it is the useful one:
a `:g` over one line is a `:s` with extra steps. It is a wart, and it is the
kind that surprises nobody who has used ex and everybody who has not, so the
reference manual has to say it out loud.

### 23.3 One backwards pass, and no mark set at all

The obvious implementation is ex's: mark every matching line, then run the
command over the marks, because a command that deletes or resizes lines
invalidates the walk that found them. §22.6 assumed that and priced it as "a
two-pass mark-then-act loop with a bound on the mark set" — a bound that would
have had to exist, since a 256 KB `editfile` buffer holds thousands of lines
and `ViState` is SRAM that M10 measured in units of 104 bytes.

**Reversing the walk deletes the whole problem.** Go from the last line to the
first: every edit a line makes happens *below* the lines still to be visited,
so no offset above it ever moves, and there is nothing to remember between one
line and the next. No mark array, no bound, no second pass, and no limit on
how many lines a `:g` may touch. `editor_pattern_search` already matches
within a single line and never crosses a break, so the test is one call per
line over the slice the walk is standing on.

The one thing the reversal costs is **order**: ex runs its command top-down,
and for `d` and `s` the result is identical, while for `m`, `t` and `p` it is
not — `:g/^to /m0` reverses the procedures rather than gathering them. That is
the argument for the set in §23.2 being two commands rather than five: the set
is exactly the commands whose result does not depend on the direction of the
walk, and admitting a third kind would bring the mark set back with it.

### 23.4 Undo is one step, and a big `:g` can exhaust the journal

Every line the pass touches is a record, and all of them belong to the one
step `editor_undo_begin` opened for the `Return` — so `u` reverses a whole
`:g/^;/d`, which is the behaviour the command needs to be safe to try.

But §8's journal is 64 KB on PSRAM and **1 KB without it**, and a change
larger than the whole journal clears it: on a `pico2`, `:g/^;/d` over a long
buffer is an edit that cannot be undone *and takes the earlier history with
it*. That is a worse hazard than any other command in this mode, because it is
one line of typing, it can delete hundreds of lines, and the buffer it deletes
them from may be a file with no other copy.

So the footer says what happened — `12 fewer lines` — rather than returning
silently, and the reference manual chapter says the journal limit next to the
command rather than only in the undo section. Refusing the command when the
journal cannot hold it was considered and rejected: it would refuse the
`pico2` exactly the edit it most wants, and the mode already tells the truth
about undo elsewhere (`Undo is not available`).

### 23.5 Where it goes, and what it costs

`editor_vi_global()` in `editor_vi.c`, beside `editor_vi_substitute` and
`editor_vi_move_lines`, for the reason both of those are there: it rewrites
the buffer, and this file is the one with a host build. It calls
`editor_vi_substitute` per line for the `s` form rather than growing a second
substitute loop — the range for that call is the one line the walk is on.

One new action kind (`VI_ACT_GLOBAL`), the pattern and the nested command
parsed into `ViState` fields `:s` already has, and one `editor.c` case that
mirrors the substitute's. **Estimate: ~90 lines in `editor_vi.c`, ~15 in
`editor.c`, and no new `ViState` bytes** if the `:g` pattern reuses
`st->pattern` — which it can, because the nested `s` has its own and an empty
one there means "the pattern that found the line", which is what §23.2 wants
anyway.

### 23.6 The M12 gate

On a board, on an `edall` buffer longer than a screen: `:g/^;/d` and one `u`
putting every line back; `:v/^to /d` leaving the index; `:g/x/s//y/g` rewriting
only the lines that matched; and the same `:g/^;/d` run on a `pico2` build, to
see what the 1 KB journal does with it and to check that the footer says so.

### 23.7 `:w {name}` — the decision is no

Save-as under `editfile`. What it would take: the write destination is
`editor.save` and `editor.save_ctx`, fixed by the primitive that opened the
editor, so a name means a second path in `editor.c`, a filename buffer in
`ViState`, and an answer to "does a later bare `:w` write the new name or the
old one" — vim says the old one, and vim can afford a `:saveas` to say the
other thing.

Against that: `save "name` from the prompt already does it, `:wq` is one
command away from the prompt, and under `edall` the buffer is the workspace
rather than a file, so `:w {name}` would mean something different depending on
which primitive opened the editor. It stays out until `editfile` grows a "new
file" path of its own, which nothing has asked for.

### 23.8 The rest, and why each is out

- **`:noh`.** There is nothing to clear: a search leaves no highlight, only a
  cursor. `editor.c` highlights syntax, not matches.
- **`:r` and `:e`.** §5.3 fixed the editor's file relationship at the primitive
  that opened it, and that is still the right shape.
- **`:j`.** `J` takes a count and works over a selection.
- **`:set`.** The two options worth having are already decided rather than
  configurable: case folding is fixed by §16.7 because Logo itself is
  case-insensitive about names, and line numbers would cost four of forty
  columns on every row of a screen that is already narrow.
- **`:!`.** Running a Logo line from inside the editor is architecturally
  impossible here, not merely unwanted: `editor.c` is the device layer and has
  no interpreter handle, and what it would evaluate against is a workspace
  that does not contain the buffer's procedures until the buffer is accepted —
  which is what `:wq` is.
