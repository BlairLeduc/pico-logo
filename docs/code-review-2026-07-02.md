# Code Review — 2026-07-02

Scope: the full `core/` interpreter (~26.5k lines) at commit `3a45e2a`, reviewed for
efficiency, simplicity, and maintainability against the reference
(`reference/Pico_Logo_Reference.md`) and the unit tests, with RP2350 hardware limits
(SRAM budget, ~150 MHz Cortex-M33, single-precision FPU) as the operating constraints.
The deepest reading went into the memory manager, value/result types, the trampoline
evaluator, lexer/token source, frames/procedures/variables, and a sample of the
primitives files. All 52 test suites pass on the host (`ctest --preset=tests`).

---

## Executive summary

The architecture is fundamentally sound and well matched to the hardware: the
trampoline evaluator eliminates C-stack overflow, the frame arena gives cheap
procedure calls with TCO, the 32-bit cons-cell pool is memory-dense, the blob heap
cleanly tiers large values into PSRAM, and float discipline is consistently
single-precision. Code is unusually well commented where semantics are subtle
(minus-sign rules, TCO, re-entrancy).

The findings that matter most, in order:

1. **`recycle` corrupts live data (confirmed bug).** The GC root set omits procedure
   frames and all in-flight evaluator state. Reproduced two ways (details below).
2. **Every word token pays a linear scan of ~316 primitive names**, and every word
   interning pays a linear scan of the whole atom table. These are the two dominant
   interpreter hot-path costs and both have cheap fixes.
3. **Out-of-node conditions are silently swallowed** in list-building primitives,
   producing truncated results instead of `ERR_OUT_OF_SPACE`.
4. Two genuine reference-vs-tests/implementation conflicts (minus sign after `)`,
   and word-equality case sensitivity) need an explicit decision.

---

## 1. Confirmed bug: `recycle` sweeps reachable data

`recycle` (`core/primitives_workspace.c:741`) marks three root sets — globals
(`var_gc_mark_all`), procedure bodies (`proc_gc_mark_all`), and property lists — then
sweeps. Nothing marks:

- **Procedure frame bindings** (locals and parameters). `frame_gc_mark_all()` exists
  (`core/frame.c:666`, declared at `core/frame.h:357`) but is **never called** from
  anywhere. The comment at `core/variables.c:442` claims it is "called from memory.c's
  garbage collector", which is not true.
- **In-flight evaluator state**: the op stack's saved token-source positions, loop
  body `Node`s held in `RepeatState`/`WhileState`/etc., staged primitive argument
  `Value`s, and `PendingBinOp.left` operands in suspended expressions.
- The `TailCall.args` buffer.

Both failure modes were reproduced with a scratch Unity test against the current
build (test since removed; reproduction below):

```logo
; A: body list only reachable from the op stack — swept mid-loop
repeat 3 [recycle print "hello]
; observed: no output, then error 38 "You don't say what to do with []"

; B: local bound to a runtime-built list — cells swept, then reused
define "f [[] [local "x] [make "x list 1 2] [recycle]
            [make "junk1 [9 9 9 9 9 9]] [make "junk2 [8 8 8 8 8 8]]
            [output :x]]
print f
; observed: "[] [] []"  (expected "1 2")
```

Case B is silent memory corruption: `:x`'s cons cells were freed and reallocated to
the junk lists, so the local now aliases unrelated data.

Note the interaction with finding 3: because `recycle` is the *only* GC trigger in
the system (no automatic collection on allocation failure), users of long-running
programs are pushed toward exactly the unsafe call.

**Suggested direction.** Either (a) make the root set complete — mark the frame stack
and walk the op stack marking `saved_source` positions, state `Node`s, and staged
`Value`s (the op stack is a flat array, so this is a straightforward loop); or, as a
stopgap, (b) make `recycle` an error when `eval->proc_depth > 0` or the op stack is
non-empty at the point of the call, so it can only run from an idle top level.
Option (a) is also the prerequisite for ever adding automatic GC. Whichever is
chosen, add regression tests for both repros above.

## 2. Hot-path efficiency

### 2.1 `primitive_find` is a linear `strcasecmp` scan (top optimization candidate)

`core/primitives.c:74` scans up to ~316 registered names (314 `primitive_register`
calls + aliases) with `strcasecmp` for **every word token evaluated**
(`core/eval_expr.c:600` and `:310`), before user procedures are even considered
(`proc_find` then scans up to 128 more). The TCO lookahead pays it again: on a
procedure's last line, `skip_instruction` (`core/eval_steps.c:134`,`:150`) performs
the same lookups a second time per instruction.

A tight turtle loop like `repeat 1000 [fd 1 rt 1]` performs thousands of full-table
scans. On a 150 MHz M33 this is likely the single largest interpreter overhead.

Fix options, cheapest first:
- Sort `primitives[]` once at the end of `primitives_init()` and binary-search
  (~8–9 comparisons instead of ~158 average). Registration order is already
  init-time-only; `primitive_get_by_index` consumers (`help`, `primitives`) don't
  depend on registration order.
- Or bucket by lowercase first letter (26 offsets) before the linear scan.

The same treatment applies to `find_procedure_index` (`core/procedures.c:64`).

### 2.2 `find_atom` is a linear scan of the whole atom table

`core/memory.c:595` walks every atom entry on **every** `mem_atom` call. Interning
happens constantly: quoted words, list literals parsed from source
(`parse_list`, `core/eval_expr.c:176`), every character extracted by
`first`/`item`/etc., and every boolean result (`mem_atom_cstr("true")` in
`apply_binary_op`, `core/eval_expr.c:27`, and per-call in `prim_emptyp` and friends).
With a few thousand atoms interned, each new intern is O(table).

Fix options:
- A small open-addressing hash table of atom offsets (e.g. 1024 × `uint16_t` = 2 KB
  of SRAM) would make interning O(1) and is the highest-leverage 2 KB available.
- Independently and trivially: intern `true`/`false` (and other constants) once at
  init and reuse the cached `Node`s.

### 2.3 Smaller items

- `value_to_number` (`core/value.c:157`) runs `strchr` over every word for `n/N`
  notation before `strtof` on every numeric coercion. A quick first-character/digit
  check would skip the scan for the common case.
- The TCO lookahead cost (2.1) could be avoided by computing "is last instruction"
  once per line instead of re-skipping per instruction, but measure after fixing 2.1
  — it may stop mattering.

## 3. Robustness: `mem_cons` failures are silently ignored

`mem_cons` returns `NODE_NIL` on out-of-memory or unencodable operands, and the
dominant list-building idiom never checks it. Examples: `butlast` copy loop
(`core/primitives_words_lists.c:255`), `fput` (`:690` — OOM yields `result_ok([])`),
`lput` (`:771`), `sentence`/`parse` loops, and `parse_list` in
`core/eval_expr.c:214`. On node exhaustion these silently return truncated or empty
lists; `mem_set_cdr(tail, NODE_NIL)` even makes the truncation look well-formed.

`limits.h` documents an "overflow story" for every other capacity in the system;
the node pool deserves one too. Since there is no automatic GC, node exhaustion is a
realistic state, and "wrong answer" is the worst possible failure mode for it.

Suggested: extract the repeated build-with-tail-pointer loop into a small helper
(e.g. `bool list_append(Node *head, Node *tail, Node item)`) that reports failure,
and have callers surface `ERR_OUT_OF_SPACE`. This fixes ~10 duplicated loops and the
error handling in one change (see also 5.3).

## 4. Reference vs unit tests vs implementation — conflicts

These are the conflicts the review found between the three sources of truth.

### 4.1 Minus sign after `)` — deliberate, documented, but a literal conflict

Reference ("The Minus Sign", line 6244): a `-` preceding a number "and follows any
delimiter **except right parenthesis**" is negative. A strict reading makes
`(5+3) -2` a subtraction. The lexer deliberately deviates: the right-paren exception
is character-adjacent, not token-adjacent (`core/lexer.c:470` documents this as
"established Logo convention"), and `tests/test_lexer.c:642` locks in `-2` as a
negative literal for `(5+3) -2`.

The code comment is excellent, but the *reference* is the stated source of truth and
currently contradicts the tests. Recommend updating the reference's minus-sign
section to state the whitespace-sensitive convention explicitly.

### 4.2 Word equality case sensitivity — three-way inconsistency, needs a decision

- Reference `equal?` (line 1904): "identical words" — silent on case. (Line 91 makes
  *names* case-insensitive, which the implementation honours everywhere via
  `strcasecmp` lookups.)
- `core/memory.h:232` documents `mem_words_equal` as "case-insensitive".
- The implementation (`core/memory.c:1029`) compares interned atoms **by offset**,
  and interning is case-**sensitive** (`find_atom` uses `str_eq`), so
  `equal? "Hello "hello` is `false`.
- `tests/test_memory.c:356` locks in the case-sensitive behaviour ("Different atoms
  (even just case difference) are not equal at atom level").

So the header comment contradicts both the implementation and the test, and the
reference doesn't settle it. Note classic Logo dialects (UCB, Terrapin/LCSI) treat
`equal?` on words as case-insensitive by default, and the interpreter itself accepts
`TRUE`/`true` interchangeably as booleans (`strcasecmp` in `step_while` etc.) — so
today `equal? "TRUE "true` is `false` while both operate identically as predicates.

**Decision needed:** pick a semantic, state it in the reference under `equal?`, align
`mem_words_equal` (or its comment), and add a Logo-level test
(`equal? "Hello "hello`) to lock it. If case-insensitive is chosen, blob comparison
in `mem_words_equal` and the atom-offset fast path both need the change (offset
equality remains a valid fast path for *true*; a case-folded content compare handles
the rest).

### 4.3 No other conflicts found

Sampled semantics — `random` (non-negative, `< n`, errors on `n <= 0`), `remainder`,
loop constructs (`while`/`until`/`do.while`/`do.until`/`forever` all present in the
reference), `first`/`item` error behaviour, `n`-notation numbers, `form` — all agree
between reference, tests, and implementation.

## 5. Simplicity and maintainability

### 5.1 Duplicated infix-operator evaluation in `eval_expr.c`

The 0-arg-primitive-inside-parens path (`core/eval_expr.c:330–434`) contains a ~90
line inline reimplementation of `apply_binary_op` (operator-name table, numeric
coercion, per-operator switch — all duplicated from lines 21–71 of the same file).
It can collapse to a loop that calls `apply_binary_op`, removing ~70 lines and one
future divergence hazard (e.g. a new operator added to one copy only).

### 5.2 Four nearly identical loop steppers

`step_while`, `step_until`, `step_do_while`, `step_do_until`
(`core/eval_steps.c:602–906`, ~300 lines) differ only in (a) which of
predicate/body runs first and (b) which truth value continues the loop. One
parameterized stepper (flags: `body_first`, `continue_on`) plus four thin wrappers
would be ~100 lines. The four also repeat the same "value must be the word
true/false" check — worth extracting regardless (a fifth copy of that logic lives in
the conditionals primitives).

### 5.3 Repeated list-builder loop

The head/tail append loop appears ~10× across `primitives_words_lists.c`,
`primitives_list_processing.c`, `properties.c`, and `eval_expr.c`. Extracting the
helper proposed in §3 pays for itself in both robustness and line count.

### 5.4 Repeated number→word coercion in element primitives

`first`/`last`/`butfirst`/`butlast`/`item`/`count` each have a `VALUE_NUMBER` branch
that is a copy of the `VALUE_WORD` branch prefixed by `number_to_word`
(`core/primitives_words_lists.c:31–400`). Normalizing numbers to words once at the
top of each primitive halves those functions.

### 5.5 Dead code

Pre-existing dead code (per project convention, mentioning rather than removing):

- `serialize_list_to_buffer` / `serialize_node_to_buffer`
  (`core/eval_steps.c:22–95`) — explicitly "currently unused".
- `find_label_position` (`core/eval_steps.c:257–329`) — unused; `step_proc_call` has
  its own node-based label search.
- `OP_RUN` / `RunState` (`core/eval_ops.h:46`,`:125`) — enum value and state struct
  with no trampoline case and no producer.
- `frame_gc_mark_all` — "dead" only because of the bug in §1; it should become live.

### 5.6 Documentation drift in `memory.h`

- `core/memory.h:89`: "up to 65535 nodes (256KB)" — actual cap is
  `MAX_LIST_INDEX` = 32766 nodes (`core/memory.c:67`), because the high bit of a
  16-bit cell reference is the word marker.
- `core/memory.h:74`: atoms described with 29-bit offsets, but the cell encoding
  caps the atom table at 32 KB (`LOGO_ATOM_LIMIT`, `core/memory.c:71`). Worth
  stating loudly: of the 128 KB `LOGO_MEMORY_SIZE`, at most 32 KB can ever be atoms.
- `core/memory.h:232` — the `mem_words_equal` case comment (§4.2).
- `core/memory.h:31`: "RP2040 ... RP2350" SRAM figures don't match the actual
  128 KB configuration in `CMakePresets.json`; minor.

### 5.7 Small items

- `name_buf[64]` truncation when looking up identifiers
  (`core/eval_expr.c:303`,`:594`; `core/eval_steps.c:127`): words longer than 63
  chars are silently truncated for primitive/procedure lookup, so two long names
  can alias. Obscure, but a one-line length guard (reject or skip lookup) removes
  the surprise.
- `parse_list` silently drops unrecognized token types
  (`core/eval_expr.c:209–211` `advance; continue`) — consider an error instead.
- `mem_gc` clears `gc_marks` at entry and `mem_gc_sweep` clears again at exit —
  harmless double clear of 4 KB; the entry clear can go.

## 6. Memory / SRAM observations

Static allocations in the interpreter core (device buffers excluded):

| Structure | Size | Notes |
|---|---|---|
| `memory_block` | 128 KB | atoms capped at 32 KB of it; node region never shrinks back once grown |
| `OpStack` (`global_op_stack`) | ~57 KB on 64-bit host; roughly 35–40 KB on ARM32 at depth 256 | `EvalOp` is 224 B on host, dominated by `TokenSource` (88 B) + `Result` (48 B) per slot |
| `frame_stack_memory` | 32 KB | arena; snapshot/restore is clean |
| `gc_marks` | 4 KB | |
| procedure/variable/blob tables | ~10 KB combined | |

Points worth knowing (documenting rather than proposing action):

- **Atoms are permanent.** There is no atom GC; every distinct word ever created
  (including every character sliced out by `first`/`item`, every distinct number
  formatted into a word, every intermediate of a `word`-concatenation loop) consumes
  32 KB-capped atom space forever. A long-running string-building program will
  exhaust it. This is a defensible classic-Logo design, but it deserves a paragraph
  in the reference ("Space" or "How Logo Uses Memory" section) so users understand
  why `nodes` shrinks and never recovers.
- **The node region never shrinks.** After a transient list spike, freed cells are
  reusable for lists but the region floor (`node_bottom`) stays put, permanently
  reducing the space available to atoms. Same class of one-way ratchet as above.
- `EvalOp` could shed noticeable weight if it ever matters: `Result` embeds a full
  `Value` + 4-pointer union per op, and `TokenSource` is copied into every op as
  `saved_source`. Depth 256 × ~150 B is the price of the trampoline; acceptable
  today, but this is the knob if SRAM pressure rises.
- `malloc` appears in a handful of primitives (`reduce`, `crossmap`, `map`'s word
  builder, `primitives` listing). It's fine functionally, but it is the only heap
  dependency in an otherwise fixed-capacity design — worth keeping deliberate
  (bounded inputs) rather than letting the pattern spread.

## 7. Things that are in good shape

- **Trampoline evaluator**: genuinely eliminates C-stack recursion for control flow;
  the deferral/resume machinery (`OP_EXPR_EVAL`, speculative `OP_PRIM_CALL`) is
  intricate but consistently commented, and the re-entrancy contract in
  `core/eval.c:270` is exemplary.
- **TCO**: self-recursive tail calls reuse frames with a correct bare-vs-output
  distinction; pause-during-tail-call has a regression test.
- **Lexer**: the minus-sign and quoted-word rules cite the reference chapter and
  verse, including the deliberate deviation — this is how spec-adjacent code should
  be documented.
- **Blob/PSRAM tiering**: clean separation (atoms ≤255 B interned, larger values
  blob-allocated, `mem_cons` refuses blob operands so GC reachability stays simple).
- **`limits.h`**: every limit has a documented overflow story (§3 is the one gap).
- **Float discipline**: single-precision (`sinf`/`strtof`/`float`) throughout, with
  the one `double` cast being a deliberate `snprintf` requirement.
- **Test suite**: 52 suites, green, with mock device separation; low-level and
  Logo-level coverage both present.

## 8. Prioritized recommendations

| # | Action | Kind | Effort |
|---|---|---|---|
| 1 | Fix `recycle` root set (frames + op stack), or restrict it to idle top level; add the two regression tests from §1 | correctness | M |
| 2 | Binary-search (or first-letter-bucket) `primitive_find` / `proc_find` | performance | S |
| 3 | Hash-index `find_atom`; cache `true`/`false` nodes | performance | S–M |
| 4 | `list_append` helper with OOM propagation; adopt in the ~10 builder loops | correctness/simplicity | M |
| 5 | Decide word-equality case semantics; align reference + `memory.h` + implementation + add Logo-level test | spec | S (decision) |
| 6 | Update reference minus-sign section to match the implemented (tested) convention | spec/docs | S |
| 7 | Deduplicate: infix block in `eval_expr.c`, four loop steppers, number→word branches | maintainability | M |
| 8 | Correct `memory.h` node/atom capacity comments; document atom permanence in the reference | docs | S |
