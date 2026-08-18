# P12 — Vi mode for the Logo Editor (design)

Status: **M1--M5 built 2026-08-18.** The mode is complete: normal mode, visual
mode, `f`/`t`/`%`, the ex command line, `setvimode`, the reference chapter,
`u` / `Ctrl` `R` over a tiered journal, and the word and bracket text objects
(§15). §14 records where the build departed from this design.

Three scoping decisions were taken with the user on 2026-08-17:

- **`Esc` belongs to vi.** In vi mode it returns to normal mode; it no longer
  accepts the buffer. Leaving the editor becomes `:wq`, `:x`, `ZZ`
  (accept) and `:q!`, `ZQ` (cancel); `:w` accepts too, except under `editfile`,
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
`:q!`, `ZQ` cancel; `:q` accepts only when the buffer is unmodified and
otherwise reports `E37: no write since last change` on the footer. `:w` accepts
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
heap the 2 x 24576 fallback editor buffers are taken from when there is no aux
region — so the journal is competing with the edit buffer itself, not with
slack. **Measure the free heap after `primitives_editor_init` before settling
`LOGO_VI_UNDO_SRAM_SIZE`**; 1 KB is a starting figure, not a budget. It belongs
in `limits.h` where it can be seen beside every other capacity.

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

M1 is the whole feature as far as a user is concerned; M2 is what makes it
pleasant, M4 is what stops it being annoying, and M5 is the one command a
board session asked for that the mode could not say (§15).

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
  refusing.
- **`G` and `:{n}` count the empty line a trailing newline leaves.** Vi does
  not; this editor does -- `editor_count_lines` counts it, the editor draws it,
  and the cursor keys go there. Agreeing with the editor beats agreeing with
  vi.
- **Two action kinds were added.** `VI_ACT_PASTE_OVER` for visual `p`, which is
  a delete and a paste that must not overwrite the copy buffer in between, and
  `VI_ACT_QUIT` for `:q`, so that the "have you changed anything" question is
  answered in `editor.c`, which knows, rather than in the state machine, which
  does not.
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
