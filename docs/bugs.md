# Bugs

Tracks defects in Pico Logo — past, present and future. A **bug** here is
behaviour that contradicts
[`reference/Pico_Logo_Reference.md`](../reference/Pico_Logo_Reference.md),
silently loses or corrupts data, or hangs/crashes the interpreter.

Sibling document: [`roadmap.md`](roadmap.md) tracks
*features* — new primitives, designs, milestones and performance work. If an
item adds capability it belongs there; if it means "this doesn't work as
documented", it belongs here.

**Status legend:** `open` · `in progress` · `fixed` · `won't fix`

**Severity:** `high` (data loss, crash, hang, unusable feature) · `medium`
(surprising wrong behaviour with a workaround) · `low` (cosmetic or narrow
edge case)

---

## Open

| ID | Bug | Area | Severity | Found | Status |
|---|---|---|---|---|---|
| [B5](#b5--name_buf64-identifier-truncation-aliasing) | `name_buf[64]` identifier truncation aliasing | lexer | low | 2026-07-02 | open |
| [B6](#b6--penreverse-ignores-pen-size-always-1-px) | `penreverse` ignores pen size (always 1 px) | graphics | low | 2026-07-18 | won't fix (documented) |
| [B7](#b7--a-user-procedure-call-as-the-left-operand-of-a-parenthesised-expression-corrupts-the-parse) | A user-procedure call as the left operand of a parenthesised expression corrupts the parse | parser/eval | high | 2026-07-28 | open |
| [B9](#b9--text-returns-newline-markers-as-ordinary-list-elements) | `text` returns newline markers as ordinary list elements | workspace/format | medium | 2026-07-31 | open |

### B5 — `name_buf[64]` identifier truncation aliasing

Identifiers are read into a 64-byte buffer, so two names longer than 63
characters that share a 63-character prefix alias to the same entry at lookup.

- **Found:** 2026-07-02, in the code review that produced PR #86.

### B6 — `penreverse` ignores pen size (always 1 px)

`penreverse` toggles pixels, so overlapping thick stamps would toggle shared
pixels twice and speckle the line. Thick reverse drawing is therefore left at
1 px and documented as a limitation rather than implemented.

- **Status:** won't fix for now. A future thick-reverse pass would delta-stamp
  only the pixels the previous stamp did not cover, or fill per-scanline spans.
- **Found:** 2026-07-18, with `setpensize` / `pensize`.

### B7 — A user-procedure call as the left operand of a parenthesised expression corrupts the parse

**Inside a procedure body**, a call to a *user-defined* procedure that appears
as the left operand within a parenthesised expression makes the evaluator
consume a closing paren that is not its own. It has two failure modes, and the
first is silent.

**Silent wrong answer.** Operands re-associate across the parentheses:

```
to f :x
output :x
end
to t6
output (item 1 [5 6]) + (3 * (f 2))
end
show t6
```

prints **21**, not `11`: it evaluates as `3 * (5 + 2)`. The same shape with a
multi-branch procedure returns 18 instead of 8. No error is raised, so a
program simply computes the wrong number.

**Spurious `) without (`.** A primitive whose argument is a parenthesised
expression *starting* with the call computes the right value and then trips
over the orphaned bracket:

```
to t2
pr ((f 2) * 3)
end
t2
```

prints `6`, then raises `) without ( in t2`.

Scope, all verified against `./build-host/logo` on 2026-07-28:

- Only inside a procedure body. Every form above is correct at the top level,
  because the deferral path that causes it needs `proc_depth > 0`.
- `output` is unaffected: `output ((f 2) * 3)` is correct.
- Primitives are unaffected: `((item 1 [5 6]) * 3)` is correct.
- The call is fine on the *right* of the operator: `(3 * (f 2))` is correct,
  as is `((f 2) + (f 3))`.

- **Cause:** `core/eval_expr.c`, the deferral branch in the user-procedure
  (`TOKEN_WORD`) case guarded by
  `eval->proc_depth > 0 && eval->user_arg_depth == 0`. Before deferring to the
  trampoline it consumes the following `)` so that infix parsing can continue
  past the call — which is right for `(f :x) + (g :y)`, where that bracket
  closes the call itself. When the `(` was a *grouping* paren instead, the
  grouping handler further out then consumes an outer `)` that is not its own,
  leaving the operands re-associated or a stray `)` in the stream.
- **Workaround:** bind the call to a `local` first and do the arithmetic on
  the variable. `logo/games/trails` is written this way throughout.
- **Fix:** only consume the closing paren when it belongs to the call, which
  means distinguishing the paren-call form from a grouping paren that merely
  happens to start with a call. Needs tests for `(f :x) + (g :y)`, the two
  shapes above, and the `output` path.
- **Found:** 2026-07-28, building `logo/games/trails`, where it stopped the
  game running a single frame.

### B9 — `text` returns newline markers as ordinary list elements

Line breaks inside a procedure body are stored as an invisible newline-marker
atom (`mem_newline_marker`, `\x01`). `prim_text` returns the body unchanged, so
those markers count as list elements even though nothing prints them. A program
that inspects or edits procedure text therefore gets a length that depends on
the source layout:

```
to mlb :n
if :n > 0 [
print :n
print 1
]
end
show count item 5 item 2 text "mlb    ; 7
```

against `4` for the same body written on one line. `show` and `po` render the
two identically, so the discrepancy is invisible until something counts.

Scope, verified against `./build-host/logo` on 2026-07-31:

- Multi-line `[...]` bodies have always done this — the markers sit inside the
  nested list (`parse_bracket_contents`).
- Multi-line `(...)` expressions do it too since B1 was fixed, where the
  markers sit at the body-line level.
- `text` → `define` round-trips correctly and keeps the layout; only element
  counting and indexing are affected.

- **Workaround:** none needed for round-tripping; a program that walks a body
  line must skip elements for which the marker is `\x01`.
- **Fix:** decide first whether markers should be invisible to `count` / `item`
  generally, or excluded from `text` with the layout carried elsewhere. The
  second must not regress `po` / `save` layout or the `text` → `define` round
  trip. Whatever is chosen has to cover the bracket and paren cases together.
- **Found:** 2026-07-31, raised by the Codex review on PR #127 (the B1 fix) and
  confirmed to predate it.

---

## Fixed

| Date | Bug | Area | Fix | Ref |
|---|---|---|---|---|
| 2026-07-31 | `parse_list` silently drops tokens (B4) | parser | `parse_list` (`core/eval_expr.c`) had two silent-drop paths. The one the review named — the `else` that advanced past an unrecognised token — turned out to be unreachable: the token kinds it can receive are exhaustively handled above it, `TOKEN_COMMENT` never reaches an evaluator (no eval lexer sets `preserve_comments`, and `classify_word` never produces it), and `TOKEN_ERROR` is never produced at all (`make_error_token` is dead code). The *reachable* drop was the sibling `TOKEN_EOF` case, which ended the list quietly, so an unterminated `[` silently truncated it: `show [a b` printed `[a b]` and `show [a ; b]` printed `[a]` — the comment runs to end of line and takes the `]` with it, exactly the "a typo silently changes the list's contents" symptom. `parse_list` now returns an error code instead of a bool: new error 72 `[ without ]` at end of input, `ERR_DONT_KNOW_WHAT` for an unrecognised token, `ERR_OUT_OF_SPACE` unchanged. Two test inputs relied on the old tolerance (`define "inner [[] [run [throw "toplevel]]` is short one `]`; `define "test.comment [[] [print 42 ; trailing comment]]` has both closers inside the comment) and were corrected. No shipped `.logo` program has an unbalanced top-level line; procedure bodies go through `parse_bracket_contents`, which is untouched | |
| 2026-07-31 | Demons fire during `load` (B3) | demons | `load` evaluates each line through `eval_instruction`, which polls demons, so a `when` armed by a file ran its action while the rest of the file was still being read — inside `load`'s reentrancy guard, where the action could neither `load` nor call a procedure defined further down the same file. New `demons_suspend` / `demons_resume` hold polling off for the duration of the load (checked in `demons_poll` itself, so the device idle loop is covered too); `load` resumes before `startup` runs and polls once on its way out, so an armed demon still gets its first chance promptly. Resume clears the motion-clock baseline like `thaw`, so a moving turtle does not jump by the load's duration. No `.logo` file arms a demon at file level — every `when` is inside a procedure — so nothing depended on the old mid-load firing | |
| 2026-07-31 | Single-line `to … end` definitions are not supported (B2) | loader/REPL | Only a standalone `end` line closed a definition, so `to f  play [c d e]  end` swallowed every procedure that followed until the next lone `end`. New `repl_find_end_token` lexes the line and accepts `end` as the *last* token outside brackets and parens — so `pr [the end]` and `pr "end` stay ordinary words, and closing a definition can never discard anything else on the line. The three copies of the accumulate/close loop (REPL, `load`, editor) were replaced by one shared `repl_proc_def_append`. Reference updated: it required `end` alone on a line while its own examples used one-liners | |
| 2026-07-31 | Multi-line `(…)` expressions inside a procedure body evaluate to empty (B1) | parser | `proc_define_from_text` split the body on every newline, so `op (list` on its own line evaluated as `(list)`. It now tracks `(` nesting and, while a paren is open, keeps the tokens on the same body line. The break is recorded as a newline marker rather than dropped, so a `;` comment inside the expression still ends at its own source line and `po` reproduces the layout (`core/format.c` renders a top-level marker as a line break, `core/eval_steps.c` skips it when printing a stepped line). `end` is still recognised at line start inside an open paren, so an unbalanced `(` cannot swallow the procedures that follow it | #127 |
| 2026-07-31 | A turtle command automatically switches to splitscreen (B8) | graphics | Deleted `screen_show_field()` and its 16 call sites in the PicoCalc turtle ops, so only `textscreen`/`splitscreen`/`fullscreen` and `F1`–`F3` choose the mode. Drawing under `textscreen` already worked — `screen_gfx_blit_dirty` skips the blit in text mode and keeps the dirty state, and a later mode switch marks all dirty and presents — so the picture now waits instead of stealing the screen. Reference updated to match (it had documented the old behaviour, inherited from LCSI Logo). Validated on hardware — the host tests cannot reach the PicoCalc device layer | #126 |
| 2026-07-22 | Atom GC freed storage still reachable from roots | memory | Corrected root handling in the atom collector | `1eaa4ae`, #120 |
| 2026-07-21 | WiFi: joining unreliable — steady retrying *sustains* the AP's rejection penalty | wifi | Retry policy changed to burst-then-escalating-rests (3 attempts at 2 s, then one probe per 30/60/120 s rest); plain exponential backoff was tried first and did not help. Diagnosed from on-device `wifi.log` traces | #116 |
| 2026-07-21 | WiFi: healthy-but-slow joins torn down mid-DHCP | wifi | Stall patience raised from 8 s to 30 s (DHCP measured at ~8.4 s on the user's network) | #116 |
| 2026-07-21 | WiFi: rejoin after a drained disconnect lost its ACTIVE flag (EV_DISASSOC race) | wifi | `cyw43_wifi_leave` only *queues* the disassoc; its event handler later zeroes `wifi_join_state`. `picocalc_wifi_disconnect` now drains (polls `cyw43_wifi_link_status != CYW43_LINK_JOIN`, bounded ≤1 s) before returning | #116 |
| 2026-07-21 | WiFi: `wifi.status` re-issued the join once per poll, resetting join state ~50×/s | wifi | Status mapping made pure; a rejoin is issued only when the state has *stalled* | #116 |
| 2026-07-21 | WiFi: startup-file connect latched `failed` on the transient `CYW43_LINK_NONET` | wifi | `NONET` treated as "scan still running" and the join re-issued, mirroring the SDK's blocking loop; only `FAIL`/`BADAUTH` stay terminal | #116 |
| 2026-07-16 | HTTP: uploads ≥ ~150 KB stalled or failed with `503` | httpd | Replaced `ERR_MEM` receive backpressure (refused segments only retried on lwIP's 250 ms timer) with deferred `altcp_recved` from the read path; `lfs_stream_write_bytes` now seeks only when the position actually differs, so LittleFS stops flushing its cache every 512 B | `ab69eb6`, #108 |
| 2026-07-16 | HTTP: a 20 MB upload corrupted, stalled to `503`, then bricked the server ("Can't open http server" on re-run) | httpd | Real receive backpressure (`ERR_MEM` refuse-when-full) with the ring raised to 4 KB (> `TCP_WND`); a connection abandoned without a response is now RST-closed so its port frees with no TIME_WAIT | `04c7b4c`, #108 |
| 2026-07-16 | HTTP: `http://picologo.local` returned `408` on a real board (ping worked) | httpd | The accept path parked the new PCB and wired its recv callback ~20 ms later, and lwIP frees inbound data delivered to a PCB with a NULL recv callback. The listener now pre-allocates a connection slot in `listen` and the accept callback wires recv immediately | `7296302`, #108 |
| 2026-07-16 | Crash when a device's sound ops are NULL; note-length prefix range wrong | sound | NULL-op guard plus a corrected range check | `921ca40`, #111 |
| 2026-07-06 | Default colour values wrong in the 24-bit and 16-bit palettes | graphics | Corrected palette entries | `b72712e` |

---

## Reporting a bug

1. Write a failing test first (`tests/test_<module>.c`, mock device) that
   reproduces it — see the Unit Testing rules in
   [`CLAUDE.md`](../CLAUDE.md).
2. Add a row to **Open** above with the next `B<n>` ID and a detail section.
3. Fix it, run `ctest --preset=tests`, then move the row to **Fixed** with the
   date and PR.

Bugs that cannot be reproduced on the host (real lwIP or cyw43 timing, PSRAM,
audio DMA) should say so explicitly and record how they were validated on
hardware instead.
