# P12 — Vi mode for the Logo Editor (design)

Status: **M1--M3 built 2026-08-18. M4 (undo) not built.** The mode is complete
as far as a user is concerned: normal mode, visual mode, `f`/`t`/`%`, the ex
command line, `setvimode`, and the reference chapter. `u` and `Ctrl` `R` report
`Undo is not available`. §14 records where the build departed from this design.

Three scoping decisions were taken with the user on 2026-08-17:

- **`Esc` belongs to vi.** In vi mode it returns to normal mode; it no longer
  accepts the buffer. Leaving the editor becomes `:w`, `:wq`, `:x`, `ZZ`
  (accept) and `:q!`, `ZQ` (cancel). `Brk` — which is Shift + `Esc` — keeps
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
does nothing. Exiting is explicit — `:w`, `:wq`, `:x`, `ZZ` accept;
`:q!`, `ZQ` cancel; `:q` accepts only when the buffer is unmodified and
otherwise reports `E37: no write since last change` on the footer.

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
| **M1** | `editor_vi.c` state machine + normal mode (§5.1), `setvimode`, dispatcher | `test_editor_vi.c` green; hardware check of the mode indicator, cursor style and the `Esc` contract | **built 2026-08-18**; the hardware check is outstanding |
| **M2** | Visual mode, `f F t T ; ,`, `%` (§5.2) | the same, plus `d%` over nested brackets | **built 2026-08-18** |
| **M3** | Reference manual chapter (§13) | — | **built 2026-08-18** |
| **M4** | Undo, both tiers (§8) | `u`/`Ctrl` `R` in the randomised differential run; SRAM tier verified on a `pico2` build | **not built.** §9 requires a free-heap measurement after `primitives_editor_init` that only a board can give |

M1 is the whole feature as far as a user is concerned; M2 is what makes it
pleasant, and M4 is what stops it being annoying.

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

**Still owed.** The hardware check M1 and M2 gate on -- the mode indicator, the
cursor style, the `Esc` contract and `d%` over nested brackets on a real board
-- and M4.
