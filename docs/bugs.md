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
| [B1](#b1--multi-line--expressions-inside-a-procedure-body-evaluate-to-empty) | Multi-line `(…)` expressions inside a procedure body evaluate to empty | parser | high | 2026-07-22 | open |
| [B2](#b2--single-line-to--end-definitions-are-not-supported) | Single-line `to … end` definitions are not supported | loader/REPL | high | 2026-07-19 | open |
| [B3](#b3--demons-fire-during-load) | Demons fire during `load` | demons | medium | 2026-07-21 | open |
| [B4](#b4--parse_list-silently-drops-unknown-tokens) | `parse_list` silently drops unknown tokens | parser | medium | 2026-07-02 | open |
| [B5](#b5--name_buf64-identifier-truncation-aliasing) | `name_buf[64]` identifier truncation aliasing | lexer | low | 2026-07-02 | open |
| [B6](#b6--penreverse-ignores-pen-size-always-1-px) | `penreverse` ignores pen size (always 1 px) | graphics | low | 2026-07-18 | won't fix (documented) |
| [B7](#b7--a-user-procedure-call-as-the-left-operand-of-a-parenthesised-expression-corrupts-the-parse) | A user-procedure call as the left operand of a parenthesised expression corrupts the parse | parser/eval | high | 2026-07-28 | open |

### B1 — Multi-line `(…)` expressions inside a procedure body evaluate to empty

The parser is line-oriented: a parenthesised expression split across source
lines inside a `to … end` body silently yields an empty or wrong result — an
unclosed `(` drops the rest of the line.

Minimal repro (inside a procedure body):

```
op (list
"|a|
"|b|
)
```

returns `count 0`, while the single-line `op (list "|a| "|b|)` returns `2`.

Affects both the REPL and `load` / `proc_define_from_text`. Silent — no error
is raised, so a program simply computes the wrong answer.

- **Workaround:** keep every `(…)` expression on one line; accumulate long
  lists with `localmake` / `lput`.
- **Fix:** treat newlines inside an open paren as whitespace during proc-body
  assembly (`core/repl.c` line buffering plus `core/parse_list.c` /
  `core/lexer.c`), with tests.
- **Found:** 2026-07-22, building `logo/fileserver`.

### B2 — Single-line `to … end` definitions are not supported

The REPL and loader only close a definition on a standalone `end` line
(`repl_line_is_end`, `core/repl.c`), so a one-liner such as
`to f  play [c d e]  end` silently swallows every following procedure until the
next lone `end`.

- **Workaround:** always put `end` on its own line.
- **Fix:** recognise an inline `end` on the `to` line, in `core/repl.c` and
  `core/primitives_files_load_save.c`, with tests.
- **Found:** 2026-07-19, via `logo/tests/sndaccept` failing with "I don't know
  how to sndtune" (`sndtune` and the tuneblocks above it had been absorbed into
  `f1`). Worked around by expanding that file's tuneblocks to multi-line.

### B3 — Demons fire during `load`

`load` evaluates each line through `eval_instruction`, which polls demons, so a
`when` armed by a file runs its action while the rest of the file is still
being read — inside `load`'s reentrancy guard. The action therefore cannot call
`load` ("No more file buffers", `core/primitives_files_load_save.c:52`) and
cannot call a procedure defined *later* in the same file.

Newly reachable now that `wifi.start` + `when` is the documented startup-file
idiom. Characterised by `test_demon_armed_by_load_fires_during_the_load`.

- **Likely fix:** suspend demon polling for the duration of a `load` and poll
  once it completes — but check first whether any file arms demons and then
  does top-level work in the same file, which that would change.
- **Found:** 2026-07-21.

### B4 — `parse_list` silently drops unknown tokens

Tokens the parser does not recognise inside a `[...]` literal are discarded
without an error, so a typo silently changes the list's contents.

- **Fix:** consider erroring inside `[...]` literals rather than dropping.
- **Found:** 2026-07-02, in the code review that produced PR #86
  ([`code-review-2026-07-02.md`](code-review-2026-07-02.md)).

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

---

## Fixed

| Date | Bug | Area | Fix | Ref |
|---|---|---|---|---|
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
