# Pico Logo — Core Code Review Backlog

Date: 2026-04-29 • Branch: `improvements` • Scope: `core/` only (excludes `devices/picocalc/`, generated `help_data.c`, build artefacts).
Authoritative semantics reference: [reference/Pico_Logo_Reference.md](Pico_Logo_Reference.md).

Severity legend:
- **P0** — Correctness / data-loss / undefined behaviour. Fix soon.
- **P1** — Logo semantics divergence or major maintenance hazard.
- **P2** — Clarity / simplicity / code organisation.
- **P3** — Style or nit.

Axis tags: **C** = clarity, **M** = maintainability, **S** = simplicity, **L** = Logo semantics, **X** = correctness/safety.

---

## 1. Cross-cutting themes

These patterns recur across multiple modules and are worth fixing as a single sweep rather than per-file.

| Theme | Where it appears | Recommended action |
|---|---|---|
| **Silent overflow / silent truncation in fixed buffers** | [core/primitives_words_lists.c](../core/primitives_words_lists.c) (WORD/UPPERCASE/LOWERCASE 256-byte bufs), [core/primitives_arithmetic.c](../core/primitives_arithmetic.c) (`form` width), [core/primitives_files_load_save.c](../core/primitives_files_load_save.c) (`LOAD_MAX_PROC` 4096), [core/primitives_workspace.c](../core/primitives_workspace.c) (`WS_FORMAT_BUFFER_SIZE` 8192) | Adopt one rule: either grow dynamically or return `ERR_OUT_OF_SPACE`. Never truncate silently. |
| **Silent failure of bookkeeping limits** | `MAX_PROCEDURES`, `MAX_GLOBAL_VARIABLES`, `MAX_PROC_PARAMS`, `MAX_CURRENT_PROC_DEPTH`, frame-arena exhaustion, `mem_atom` >= 0x8000 | Centralise hard-limit `#define`s in one header with rationale; return a `Result` with `error_context` set, not bare `false`. |
| **Duplicated logic in two places** | `is_valid_number` / `is_number_word` ([core/lexer.c](../core/lexer.c), [core/token_source.c](../core/token_source.c)); `line_starts_with_to`/`line_is_end` ([core/repl.c](../core/repl.c), [core/primitives_files_load_save.c](../core/primitives_files_load_save.c)); `frame_find_binding` vs `frame_find_binding_in_chain` ([core/frame.c](../core/frame.c)); `REQUIRE_INTEGER` local in [core/primitives_bitwise.c](../core/primitives_bitwise.c) vs centralised macros in [core/primitives.h](../core/primitives.h); `extract_xy`/`extract_rgb` in [core/value.c](../core/value.c); operator dispatch duplicated three times in [core/eval_expr.c](../core/eval_expr.c) | Extract one helper; delete the copy. Particularly important for lexer/token_source parity. |
| **Atom interning + case folding inconsistency** | [core/frame.c](../core/frame.c#L277) uses `strcasecmp` on already-interned atoms; [core/primitives_logical.c](../core/primitives_logical.c#L20-L25) uses case-sensitive `strcmp` for `true`/`false` while [core/primitives.h](../core/primitives.h) `REQUIRE_BOOL` uses `strcasecmp` | Decide once: do atoms case-fold at intern time? If yes, switch all comparisons to pointer equality. If no, audit every `strcmp` usage. |
| **Op-stack / GC root coverage during evaluation is implicit** | [core/eval_expr.c](../core/eval_expr.c) uses local `Value args[16]` buffers that are not GC roots; [core/memory.c](../core/memory.c) `mem_newline_marker` is a singleton with no documented root contract; [core/primitives_list_processing.c](../core/primitives_list_processing.c) `malloc`/`free` paths are not exception-safe | Document the GC-root contract in `memory.h`. Run tests under ASan to flush latent issues. |
| **Magic numbers without `#define`** | `0x8000` cell-word marker ([core/memory.c](../core/memory.c)), `-10` buffer margin ([core/repl.c](../core/repl.c)), `16` max args (3 sites in [core/eval_expr.c](../core/eval_expr.c)), `8192` WS buffer, BP_* binding powers without comment | One named constant per concept. |
| **Lexer ↔ syntax-highlighter ↔ NodeIterator drift** | Number validation (P2-016), unary-minus rules (P2-010), `;` comment recognition (P2-001/P2-012), `SYNTAX_STRING` vs `TOKEN_QUOTED` naming (P6-009), word/colon handling (P6-010) | Single source of truth for token classification; add tests that round-trip text → tokens → list → tokens. |
| **Round-trip SAVE → LOAD fragility** | [core/format.c](../core/format.c#L565-L571) counts raw `[`/`]` tokens for indentation, mishandling `print "["`; [core/primitives_files_load_save.c](../core/primitives_files_load_save.c#L186-L195) uses pointer-equality to detect `startup` change | Format must be lexer-aware. Add an explicit `format(po(proc)) == proc` round-trip test. |
| **Help / primitive registration coverage** | [core/help.c](../core/help.c) binary search assumes sorted `help_entries[]` enforced only by `scripts/generate_help.awk`; no test that every primitive has a help entry | Add an `init`-time validation pass and a unit test counting primitives vs help entries. |

---

## 2. Findings — P0 (correctness / data loss)

> **Verification status (current pass):** P5a-002, P5a-003, P5c-001, P5c-002, and P1-001 verified real and **fixed** with regression tests. P4-008, P4-005, and P4-010 were **rechecked against the source and downgraded** — see the inline notes below; no code change applied.
>
> **P1 follow-up batches (4–7):** P5b-002/003, P5b-006, P5b-009, P5b-012, P5b-016, P5b-020, P5c-003, P6-007, P6-008, P2-016 — fixed or pinned with tests. P2-010 audited (no behavioural drift today, parity tests added). P5c-010 and P6-004 reviewed and intentionally not changed (current code is safe under existing invariants — see inline notes).

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| ~~**P4-008**~~ | X | [core/frame.c:408-434](../core/frame.c#L408-L434) | **FALSE POSITIVE.** `frame_add_local` already calls `arena_extend` *before* taking the values pointer (`get_values_ptr`). The arena allocator (`core/frame_arena.c`) only bumps `top`; its `base` pointer never relocates, so prior pointers cannot be invalidated. No fix required. | Keep the invariant explicit with a code comment if confusion recurs. |
| **P5a-002** ✅ | X | [core/primitives_arithmetic.c:282-297](../core/primitives_arithmetic.c#L282-L297) | `prim_form` writes `padding + fmt_len` bytes into a 64-byte stack buffer with no clamp on `width`. `form 1.5 1000 2` overflows the stack. | **Fixed:** introduced `FORM_MAX_WIDTH = 255`, sized buffer accordingly, reject larger widths with `ERR_DOESNT_LIKE_INPUT`. Test: `test_form_width_too_large_error`. |
| **P5a-003** ✅ | X | [core/primitives_bitwise.c:8-19](../core/primitives_bitwise.c#L8-L19) | `REQUIRE_INTEGER` casts `float` to `int32_t` without range check. Values outside `[INT32_MIN, INT32_MAX]` (or NaN/Inf) are UB per C standard. | **Fixed:** macro now checks `isfinite(f)` and the int32 range before the cast, returns `ERR_DOESNT_LIKE_INPUT` otherwise. Test: `test_bitand_error_out_of_range`. |
| **P5c-001** ✅ | L | [core/primitives_turtle.c:248-260](../core/primitives_turtle.c#L248-L260) | `heading` returned the device's raw heading without normalising to `[0, 360)`. | **Fixed:** added `normalize_heading()` helper, applied in both `prim_heading` and `prim_setheading` so behaviour is independent of device-side normalisation. Existing tests `test_heading_outputs_normalized_*` exercise the path. |
| **P5c-002** ✅ | L/C | [core/primitives_outside_world.c:148-285](../core/primitives_outside_world.c#L148-L285) | `parse_line_to_list` for `READLIST` did its own incomplete bracket parsing; nested brackets dropped sublists (broken `*tail = new_cell` against a stack temporary, malloc-then-recurse on substring). | **Fixed:** rewrote as `parse_list_body(Lexer*)` that recurses on the same lexer, with a real `head/tail` append helper and proper `NODE_MAKE_LIST(0)` empty-list encoding. Tests: `test_readlist_with_nested_list`, `test_readlist_with_deeply_nested_list`. |
| **P1-001** ✅ | X | [core/memory.c:271-300](../core/memory.c#L271-L300) | `mem_cons` previously called `node_to_index(car/cdr)` without checking the result; an atom with offset ≥ `0x8000` silently encoded as `0`, alias for `NODE_NIL`. | **Fixed:** `mem_cons` now distinguishes legitimate `NODE_NIL` from unencodable references and returns `NODE_NIL` (its standard failure signal) without consuming a cell. Test: `test_cons_rejects_word_with_offset_too_large`. |
| ~~**P4-005**~~ | X | [core/procedures.c:74-101](../core/procedures.c#L74-L101) | **OVERSTATED.** `proc_define` returns `true` for redefinitions (after updating the slot) and only returns `false` when the procedure table is full. There is no actual ambiguity in the caller path. | If a future caller needs to distinguish "redefining" vs "first define", add an `out_redefined` flag rather than reworking the return type. |
| ~~**P4-010**~~ | X | [core/frame.c:170-205](../core/frame.c#L170-L205), [core/eval_steps.c:1175-1186](../core/eval_steps.c#L1175-L1186) | **OVERSTATED.** `frame_reuse` returning `false` is the documented "decline TCO" signal; the call site immediately falls back to `frame_pop` + full `frame_push` and reports `ERR_OUT_OF_SPACE` if that fails. No silent failure exists in production. | Optional: rename to `frame_try_reuse` and document the contract in [core/frame.h](../core/frame.h) so the intent is explicit. |

---

## 3. Findings — P1 (semantics / major hazard)

> **Quick-win pass (current):** Fixed P5a-001, P5a-013, P5a-014, P5c-004, P5c-005, P5c-008, P3-021, P1-004 (the last with a bonus fix to a latent off-by-two bug in `alloc_cell` exposed by the new `_Static_assert`). Downgraded P5c-007 (the reference defines out-of-range `toot` as a rest, not an error). The remaining P1 items below are still open.

### Foundations & memory

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P1-002 | M | [core/memory.h:30-40](../core/memory.h#L30-L40), [core/memory.c:196](../core/memory.c#L196) | ✅ Added `mem_would_collide()` helper used by both `alloc_cell` and `mem_atom`. |
| P1-003 | M | [core/memory.c:199](../core/memory.c#L199) | ✅ Added `LOGO_ATOM_LIMIT`, `CELL_WORD_MARKER` constants + 3 `_Static_assert`s. |
| **P1-004** ✅ | X | [core/memory.c:158-200](../core/memory.c#L158-L200) | Node index assumes `LOGO_MEMORY_SIZE/4 <= 0xFFFF`; silent overflow if size grows. | **Fixed:** added `_Static_assert(LOGO_MEMORY_SIZE % 4 == 0)`, introduced `MAX_LIST_INDEX = 0x7FFE`, and tightened `alloc_cell` to refuse pool indices that collide with the empty-list (`0x7FFF`) or word-reference (`0x8000+`) marker bits. The previous bound (`> 0xFFFF`) was off-by-two and could have produced corrupt cells once the pool grew past 32766 nodes. |
| P1-005 / P1-006 | C | [core/memory.c:228-240](../core/memory.c#L228-L240) | ✅ Magic literals replaced with named constants throughout. |
| P1-009 | X | [core/memory.c:616-625](../core/memory.c#L616-L625) | ✅ `gc_mark_index` now bounds-checks against `MAX_LIST_INDEX` first. |
| P1-010 | M | [core/value.h:39-54](../core/value.h#L39-L54) | `Value`/`Result` unions have no compiler-enforced tag check; reading wrong field is UB. | Provide `value_get_*`/`result_get_*` accessor macros that assert tag. |
| P1-013 | X | [core/memory.c:165](../core/memory.c#L165) | ✅ Documented as a permanent atom (lives in atom region, not subject to sweep); `mem_gc()` now marks it unconditionally so callers cannot accidentally lose it. |
| P1-014 | X | [core/frame_arena.c:1-20](../core/frame_arena.c#L1-L20) | ✅ `arena_init` now rejects sizes smaller than one word so misconfigured arenas fail loudly; test `test_init_undersized_fails`. |
| P1-016 | M | [core/value.c:86-128](../core/value.c#L86-L128) | ✅ Extracted shared `extract_number_list()` helper; ~80 lines deduplicated. |
| P1-017 | C | [core/error.h:64-68](../core/error.h#L64-L68) | ✅ `CaughtError.proc`/`caller` ownership documented (non-owning). |

### Lexer / token source

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P2-001 | L | [core/lexer.c:115-130](../core/lexer.c#L115-L130) | ✅ Lexer now strips `;` to end-of-line in `skip_whitespace`; `;` is a word terminator; `prim_comment` removed (now unreachable). |
| ~~P2-002~~ | L | [core/lexer.c:118-130](../core/lexer.c#L118-L130) | **STALE.** `newline_count` is heavily used by `primitives_procedures.c`; line continuation lives correctly in the REPL via `REPL_FLAG_ALLOW_CONTINUATION`. No action required. |
| P2-003 | C/L | [core/lexer.c:396-413](../core/lexer.c#L396-L413) | `should_be_unary_minus` rules are subtle and partly contradict reference (e.g. binary minus after WORD); risks silent semantic divergence. | Rewrite with reference rules quoted in comments; add edge-case tests (`word-foo`, `(5+3) -2`). |
| P2-006 | X | [core/lexer.c:279, 388](../core/lexer.c#L279) | ✅ `looks_like_number` already rejects incomplete exponents (verified). Added regression tests `test_incomplete_exponent_e_is_word`, `..._with_sign_is_word`, `..._n_is_word`. |
| P2-005 | ✅ | [core/lexer.c:214-249](../core/lexer.c#L214-L249) | First-char-after-quote rules (brackets always escape, slash special-case) are correct but reasoning buried. | Quoted reference §3807/3873-3895 in `read_quoted` header comment; added regression tests `test_quoted_escaped_slash_first_char`, `test_quoted_first_char_delimiter_is_literal`, `test_quoted_first_char_bracket_always_terminates`, `test_quoted_unescaped_delimiter_in_middle_terminates`, `test_quoted_escaped_delimiter_in_middle_continues`. |
| P2-010 | L | [core/token_source.c:185-200](../core/token_source.c#L185-L200) | ✅ Behaviour audit shows lexer and NodeIterator produce the same user-visible result for common minus cases (binary with spaces, after `)`, negative literal, after operator, with variable). Pinning tests added in `tests/test_primitives_arithmetic.c` (`test_minus_text_vs_list_*`) so any future drift in either path fails immediately. Unifying into a shared `logo_classify_minus` remains a P2 cleanup. |
| P2-016 | L | [core/token_source.c:60-90](../core/token_source.c#L60-L90) | ✅ `is_number_word` now matches lexer: requires digit before exponent, requires digit after exponent, rejects sign after `n`/`N`. |
| P2-012 | L | [tests/test_lexer.c](../tests/test_lexer.c) | ✅ Added with P2-001. |
| P2-017 | ✅ | [core/lexer.c:214-260](../core/lexer.c#L214-L260) | Slash-in-quoted-word rule for file paths is asymmetric (first vs non-first char); no tests. | Documented the asymmetry plus the `/` exception in the `read_quoted` header comment (covered with P2-005). Existing `test_quoted_word_with_slash`, `test_quoted_word_slash_in_middle` and the new edge-case tests pin the behaviour. |

### Evaluator core

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P3-008 | L | [core/primitives_conditionals.c:22-37](../core/primitives_conditionals.c#L22-L37) | ✅ Added comment on the `default_args=2` registration explaining the parens requirement; tests `test_if_three_args_requires_parens` and `test_if_three_args_with_parens_runs_else`. |
| P3-009 ✅ | L | [core/eval_steps.c:480-510](../core/eval_steps.c#L480-L510), [core/eval.c:461-477](../core/eval.c#L461-L477) | **Verified/pinned:** `step_if` does forward `OP_FLAG_ENABLE_TCO` to the chosen branch's `OP_RUN_LIST_EXPR`, and `tests/test_primitives_procedures.c::test_very_deep_tail_recursion` exercises 10 000 tail-recursive calls through `if :n > 0 [tailcount10k difference :n 1]` without overflow. Backlog entry was stale. |
| P3-006 | S | [core/eval_expr.c:450-461, 614-628](../core/eval_expr.c#L450-L461) | Speculative `OP_PRIM_CALL` staging (push-then-discard if args defer) is hard to reason about. | Switch to eager evaluation of primitive args; defer only when a user proc is encountered. |
| P3-012 | ✅ | [core/eval_expr.c:619, 703](../core/eval_expr.c#L619-L703) | Argument collection conflates EOF and `]`/`)` bounds; verify no over-consumption past `]` in nested forms. | Verified/pinned: the loop in `eval_expr.c` checks `peek(eval).type == TOKEN_RIGHT_PAREN \|\| TOKEN_RIGHT_BRACKET` and breaks before the recursive `eval_expression` call, so argument collection stops at the closing bracket even though `eval_at_end` only returns true at EOF. Added `test_repeat_arg_collection_does_not_leak_past_bracket` and `test_repeat_inner_arg_collection_stops_at_closing_bracket` in `tests/test_primitives_control_flow.c`. |
| P3-017 | X | [core/eval.c:268-295](../core/eval.c#L268-L295) | `eval_run_list` re-entrancy under pause/freeze: `in_tail_position`/`proc_depth` save/restore vs single global `op_stack` is not obviously safe. | Document the re-entrancy contract; add a pause-during-tail-call test. |
| P3-016 | ✅ | [core/eval_expr.c:625-637](../core/eval_expr.c#L625-L637) | `output` forces tail position whenever inside a procedure, even when itself not in tail. May over-trigger TCO. | Verified/pinned: this is correct, not over-triggering. `output` always terminates the procedure, so a self-call inside its argument is a genuine tail call regardless of where `output` textually appears. The existing comment already says so. Added `test_deep_tail_recursion_through_output` (10000-deep self-call via `output sumto difference :n 1` placed on a non-last body line) to fail loudly if TCO ever stops being applied to `output`'s argument. |
| **P3-021** ✅ | X | [core/primitives_conditionals.c:22-37](../core/primitives_conditionals.c#L22-L37) | If `(if)` reaches the primitive with `argc==0` via varargs path, `args[0]` is UB. | **Fixed:** explicit `if (argc < 2)` guard returning `ERR_NOT_ENOUGH_INPUTS`. Tests: `test_if_zero_args_via_parens`, `test_if_one_arg_via_parens`. |

### Bindings

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P4-001 / P4-018 | C/X | [core/frame.c:277-285](../core/frame.c#L277-L285) | Variable lookup uses `strcasecmp` on interned atoms — both slower than pointer equality and risks false matches if interning isn't case-folding. | Decide interning policy; switch to `bindings[i].name == name`. |
| P4-003 | M | [core/frame.c:277-322](../core/frame.c#L277-L322) | `frame_find_binding` and `frame_find_binding_in_chain` duplicate the inner search. | Implement chain search as a loop over single-frame helper. |
| P4-004 | C | [core/procedures.c:16](../core/procedures.c#L16), [core/procedures.h:34](../core/procedures.h#L34), [core/variables.c:19](../core/variables.c#L19) | `MAX_PROCEDURES`, `MAX_PROC_PARAMS`, `MAX_GLOBAL_VARIABLES` defined in `.c` files with no rationale or error story. | Move to one shared header with comments; ensure overflow paths set `error_context`. |
| P4-006 | L | [core/variables.c:90-105](../core/variables.c#L90-L105) | Dynamic scoping behaviour (MAKE updates ancestor frames before falling back to global) is correct but very subtle. | Add a comment explaining the spec behaviour next to the chain walk. |
| P4-011 | X | [core/procedures.h:30-33](../core/procedures.h#L30-L33) | ✅ Tail-call setup in `eval_expr.c` now refuses TCO when `argc != param_count` or `argc > MAX_PROC_PARAMS`, falling through to the regular call path. |
| P4-012 | C | [core/properties.c:66-87](../core/properties.c#L66-L87) | Per-name property lookup walks list twice per iteration; not asymptotically wrong but worth documenting cost. | Add complexity comment. |
| P4-014 | L | [core/properties.c:40-53](../core/properties.c#L40-L53) | Numbers in property lists are stored via word-atom round-trip; possible precision loss is undocumented. | Either document or store numbers as a typed atom format. |
| P4-015 | M | [core/variables.c:435-445](../core/variables.c#L435-L445) | `var_get_test` fall-through from frame to global is implicit. | Restructure or comment as a single decision. |
| P4-016 | M | [core/procedures.h:52-58](../core/procedures.h#L52-L58) | Global `frame_stack` ownership is fuzzy (procedures.c owns it, variables.c uses it). | State ownership in header. |
| P4-020 | X | [core/procedures.c:265-315](../core/procedures.c#L265-L315) | Mutating procedure flags (`step`, `trace`, `bury`) is not re-entrant safe vs running procedures. | Document constraints, or store flags in execution state. |

### Primitives — numeric / logic

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| **P5a-001** ✅ | L | [core/primitives_logical.c:14-32](../core/primitives_logical.c#L14-L32) | `and`/`or`/`not` used case-sensitive `strcmp` for `true`/`false`. | **Fixed:** `get_bool_arg` now uses `strcasecmp`. Tests: `test_and_accepts_uppercase_bool`, `test_or_accepts_uppercase_bool`. |
| P5a-005 | L | [core/primitives_arithmetic.c:309-312](../core/primitives_arithmetic.c#L309-L312) | ✅ Variadic path covers `(sum)` (=0), `(sum n)` (=n), `(product)` (=1), `(product n)` (=n). Tests added in `test_primitives_arithmetic`. |
| P5a-010 | L | [core/primitives_conditionals.c:76-94](../core/primitives_conditionals.c#L76-L94) | ✅ Verified per-reference no-op behaviour; documented in code; covered by `test_iftrue_without_test`. |
| **P5a-013** ✅ | C | [core/primitives_arithmetic.c:24-30](../core/primitives_arithmetic.c#L24-L30) | `prim_abs` used `REQUIRE_NUMBER(args[0], n)` then `UNUSED(args)`; contradictory. | **Fixed:** dropped the `UNUSED(args)`. |
| **P5a-014** ✅ | X | [core/primitives_arithmetic.c:272-294](../core/primitives_arithmetic.c#L272-L294) | `prim_form` `fmt_buf[48]` could in principle overflow on `snprintf`. | **Fixed:** check `snprintf` return value; refuse with `ERR_DOESNT_LIKE_INPUT` on overflow. (In practice unreachable for finite single-precision floats — `multiplier` overflows to `inf` first — but the defensive check is cheap and keeps the contract explicit.) |

### Primitives — data / workspace

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P5b-001 ✅ | L | [core/primitives_words_lists.c](../core/primitives_words_lists.c), [core/parse_list.c](../core/parse_list.c) | **Fixed:** extracted the lexer-driven list parser used by `readlist` into a shared `core/parse_list.[ch]` module and routed `prim_parse` through it. `parse` now produces nested sublists (recursing through brackets and matching `readlist`'s shape) instead of silently dropping bracket content. Tests: `test_parse_list_from_string_nested`, `test_parse_list_from_string_deeply_nested`, `test_parse_list_from_string_empty_sublist`. |
| P5b-002 / P5b-003 ✅ | X | [core/primitives_words_lists.c](../core/primitives_words_lists.c) | **Fixed:** `prim_word` now refuses concatenation > 255 chars with `ERR_OUT_OF_SPACE` (matches the atom interner cap); `prim_lowercase`/`prim_uppercase` replaced their silent clamp with an `assert` (input atom length is guaranteed ≤ 255, so the 256-byte buffer is provably sufficient). Tests: `test_word_overflow_returns_error`, `test_word_at_atom_limit_succeeds`. |
| P5b-006 ✅ | L | [core/primitives_list_processing.c](../core/primitives_list_processing.c) | **Fixed/pinned:** added `test_reduce_word_is_right_fold` using a non-commutative `mark` procedure to prove right-to-left fold direction (`reduce "mark [a b c d]` → `aLbLcLdRRR`). |
| P5b-007 ✅ | X | [core/primitives_list_processing.c](../core/primitives_list_processing.c) (`map`, `filter`, `crossmap`, `map.se`) | `malloc`/`free` paths aren't exception-safe — error in callback can leak `element_storage`/`result_word`. | **Audited & pinned:** every error-return in `map`/`filter`/`reduce`/`crossmap` after the allocation point already calls `free` on `result_word`/`elements`/`element_storage` (including `RESULT_ERROR`/`THROW`/`STOP`, the `ERR_NOT_BOOL` and "lists cannot be concatenated into a word" branches, and the `realloc` failure path). `map.se` does no heap allocation. Added a contract comment at the top of [core/primitives_list_processing.c](../core/primitives_list_processing.c) and 6 error-path regression tests (`test_map_callback_error_word_output_freed`, `test_map_callback_returns_list_into_word_output`, `test_filter_callback_non_bool_freed`, `test_filter_callback_throw_freed`, `test_reduce_callback_throw_freed`, `test_crossmap_callback_throw_freed`) that hammer the failing branches and then run a follow-up call to confirm the interpreter is healthy. Validated under AddressSanitizer (`build-tests-asan`) — no use-after-free or heap corruption. |
| P5b-009 | X | [core/primitives_list_processing.c:240-280](../core/primitives_list_processing.c#L240-L280) | ✅ `invoke_proc_spec` now rejects lambdas with `param_count > MAX_PROC_PARAMS` with `ERR_TOO_MANY_INPUTS` before touching the fixed-size save/restore arrays. |
| P5b-010 | ✅ | [core/primitives_variables.c:24-42](../core/primitives_variables.c#L24-L42) | `thing` returns `ERR_NO_VALUE` on unknown; reference doesn't pin the behaviour. | Documented in `prim_thing` header that the unknown-name case returns `ERR_NO_VALUE` so `thing "x` matches `:x` (reference §1665 says they're equivalent). Added parity test `test_thing_and_colon_agree_on_unknown` alongside the existing `test_thing_unknown_raises_no_value`. |
| P5b-011 | ✅ | [core/primitives_procedures.c:452-500](../core/primitives_procedures.c#L452-L500) | `text` returns malformed `[[params] | NIL]` if procedure body is empty. | Verified/pinned: `prim_text` computes `mem_cons(params_list, proc->body)`; when body is `NIL` the result is the proper singleton `[[params]]`. Comment in code documents the design. Covered by `test_text_empty_body_is_well_formed` ([tests/test_primitives_procedures.c:679](../tests/test_primitives_procedures.c#L679)). |
| P5b-012 ✅ | X | [core/primitives_workspace.c](../core/primitives_workspace.c) (PO/POALL/PONS) | **Fixed:** consolidated the four 8 KB stack buffers into a single static `ws_format_buffer`. Workspace primitives are not reentrant (they don't run user code), so the static buffer is safe and removes the per-call stack pressure (~32 KB worst case across the helper chain). The buffer-overflow concern itself is also mitigated: `format_buffer_output` already returns false on overflow, falling back to streaming `ws_output`. |
| P5b-014 | ✅ | [core/primitives_words_lists.c:565-650](../core/primitives_words_lists.c#L565-L650) | `member`/`memberp` mishandle leading/trailing space and use `mem_is_nil` inconsistently. | Refactored to share `find_word_in_word`/`find_element_in_list` helpers; aligned empty-needle semantics so `member?` matches `member` (empty needle → not found / `false`). Added whitespace-needle tests in [tests/test_primitives_words_lists.c](../tests/test_primitives_words_lists.c). |
| P5b-016 | L | [core/primitives_words_lists.c:700-750](../core/primitives_words_lists.c#L700-L750) | ✅ Added explicit tests `test_item_is_one_indexed` and `test_item_zero_index_errors`. |
| P5b-018 | ✅ | [core/primitives_procedures.c:195-360](../core/primitives_procedures.c#L195-L360) | `proc_define_from_text` newline tracking across multi-line bracket bodies is untested and may insert/lose blank lines. | Added three regression tests in `tests/test_primitives_procedures.c`: `test_proc_define_from_text_multiline_bracket_body`, `test_proc_define_from_text_nested_brackets_across_lines`, `test_proc_define_from_text_blank_line_inside_bracket_body`. The first asserts `text` round-trips a well-formed list-of-lists; the others prove nested brackets and blank lines inside a bracket body don't corrupt execution. |
| P5b-020 | L | [core/primitives_text.c:85-110](../core/primitives_text.c#L85-L110) | ✅ Now rejects `column >= 40 \|\| row >= 32` per reference's fixed 40×32 screen. |

### Primitives — world / files / debug

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P5c-003 | X | [core/primitives_files_load_save.c:110-115](../core/primitives_files_load_save.c#L110-L115) | ✅ Both truncation paths now return `ERR_OUT_OF_SPACE` instead of silent skip. |
| **P5c-004** ✅ | X | [core/primitives_files.c:285-365](../core/primitives_files.c#L285-L365) | `pos_f` cast to `long` without range validation — UB outside `[LONG_MIN, LONG_MAX]`. | **Fixed:** both `setreadpos` and `setwritepos` now reject non-finite values and values outside `[0, LONG_MAX]` before the cast. Test: `test_setreadpos_extreme_value`. |
| **P5c-005** ✅ | C | [core/primitives_turtle.c:347-358](../core/primitives_turtle.c#L347-L358) | `towards` normalised angle via `while` loop — pathological for huge magnitudes. | **Fixed:** delegate to `normalize_heading()` (uses `fmodf`). |
| ~~P5c-007~~ | ✅ | [core/primitives_hardware.c:72-79](../core/primitives_hardware.c#L72-L79) | **DOWNGRADED.** Reference §2811 explicitly states out-of-range `toot` frequencies behave as a rest, not an error. The current behaviour is correct; only the inline comment's range was misleading. | Updated the `prim_toot` header comment to match reference (100–2000 Hz, out-of-range = rest, by convention 0 Hz). |
| **P5c-008** ✅ | L | [core/primitives_network.c:97-110](../core/primitives_network.c#L97-L110) | NTP timezone offset not validated. | **Fixed:** require `timezone in [-12, +14]` hours; NaN naturally rejected by the comparison. Test: `test_ntp_rejects_out_of_range_timezone`. |
| P5c-010 | C | [core/primitives_files_load_save.c:186-195](../core/primitives_files_load_save.c#L186-L195) | `startup` change detection compares node pointers, not values. | Use semantic equality, or document the limitation. |

### REPL / format / help / highlight

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P6-002 ✅ | M | [core/repl.c:33-60](../core/repl.c#L33-L60), [core/primitives_files_load_save.c](../core/primitives_files_load_save.c), [core/primitives_editor.c](../core/primitives_editor.c) | **Fixed:** removed the duplicate `line_starts_with_to`/`line_is_end` static helpers from `primitives_files_load_save.c` and `primitives_editor.c`; both call sites now use the public `repl_line_starts_with_to` / `repl_line_is_end` from `core/repl.h`. (There were three copies, not two.) |
| P6-004 | L/X | [core/format.c:565-571](../core/format.c#L565-L571) | Procedure-definition formatter counts raw `[`/`]` tokens for indentation; `print "["` derails it and breaks SAVE→LOAD round-trip. | Use a lexer-aware depth counter; add a round-trip test. |
| P6-007 | M | [core/help.c:7-25](../core/help.c#L7-L25) | ✅ Documented requirement in `help.h`; added `help_check_sorted()` and `tests/test_help.c` to fail loudly if the table ever becomes unsorted. |
| P6-008 ✅ | L | [core/help.c](../core/help.c), [core/primitives.c](../core/primitives.c) | **Fixed:** added `tests/test_primitive_help_coverage.c` — iterates every registered primitive and fails if `help_lookup` returns null, printing all gaps. Wired into `tests/CMakeLists.txt`. |

---

## 4. Findings — P2 (clarity / simplicity)

Grouped by module; full details in the per-phase reports under [/memories/session/](../).

**Foundations** — P1-007 (dead `NODE_TYPE_MARK` enum), P1-008 (atom layout invariant doc), P1-011 (string-comparison helpers duplicated), P1-012 (list-validity helper missing), P1-015 (float equality limitation undocumented).

**Lexer / token source** — P2-004 (number-validation duplication; see also Theme), P2-007 (no docs on `TOKEN_MINUS` vs `TOKEN_UNARY_MINUS`), P2-008 (backwards comment in `classify_word`), P2-009 (dead negative-number code path), P2-011 (`is_delimiter_token` non-exhaustive switch), P2-014 (`read_number` exponent logic convoluted), P2-015 (lexer state fields undocumented), P2-018 (no token length cap), P2-019 (NodeIterator mixes token classification + sublist handling).

**Evaluator** — P3-001 (split `eval_expr.c`, see §6 below), P3-002 (BP_* constants undocumented), P3-003 (3 inline infix loops; extract `eval_infix_continuation`), P3-004 (3 arg-collection paths; extract helpers), P3-005 (`paren_depth` is dead bookkeeping), P3-007 (centralise operator dispatch in `apply_binary_op`), P3-010 (no test for complex precedence), P3-011 (`(0-arg-prim+expr)` semantics unverified), P3-013 (`MAX_EXPR_OPS=4` vs 3 levels), P3-014 (TCO lookahead doc), P3-018 (no central operator-precedence table), P3-020 (op-kind dispatch could be a function table).

**Bindings** — P4-002 (TEST reset rationale missing), P4-007 (misleading "local at top level" comment), P4-009 (`frame_add_local` top-frame-only constraint undocumented), P4-013 (entry mutation cases unclear), P4-017 (`VALUE_WORDS` macro purpose), P4-019 (iteration semantics inconsistent vs `var_exists`).

**Primitives — numeric/logic** — P5a-004 (move `REQUIRE_INTEGER` to header — folds into P5a-003), P5a-006 (`for` uses bespoke `eval_to_number`), P5a-007 (`for` loop var restoration on throw/stop), P5a-008 (`do.while` arg comments), P5a-009 (`if` no-branch case test), P5a-011 (float div-by-zero exact-equality check), P5a-012 (use `M_PI`).

**Primitives — data/workspace** — P5b-004 (`member` case sensitivity unspecified), P5b-005 (`beforep` ASCII order doc), P5b-008 (lambda vs procedure-text heuristic fragile), P5b-013 (`primitives` collection verbose), P5b-015 (`filter` true/false case sensitivity), P5b-017 (`parse_bracket_contents` newline tracking doc), P5b-019 (`map` realloc strategy doc).

**Primitives — world/files/debug** — P5c-006 (date/time list parsing duplication), P5c-009 (`parse_line_to_list` complexity).

**REPL / format / help** — P6-001 (`-10` magic margin), P6-003 (bracket-continuation space joining), P6-005 (custom float rounding assumption), P6-006 (nested-list indent depth tracking), P6-010 (highlighter accepts `:` mid-word), P6-011 (highlighter doesn't track quotes inside `;` comments).

---

## 5. Findings — P3 (nits / style)

P2-020 (rename `is_number_char` → `is_number_continuation_char`); P3-015 (`MAX_PRIMITIVE_ARGS` magic 16); P3-019 (operators-in-lists doc); P5c-011 (`atan2f(dx, dy)` swapped args comment); P5c-012 (8 KB editor buffer rationale); P6-009 (`SYNTAX_STRING` vs `TOKEN_QUOTED` naming); P6-012 (TO-line depth-reset comment).

---

## 6. Specific recommendation: split `eval_expr.c`

[core/eval_expr.c](../core/eval_expr.c) (~950 lines) is the single largest file in `core/` and concentrates four loosely-related concerns. Recommended split (Phase 3 source — see P3-001):

| New file | Approx lines | Contents |
|---|---|---|
| `eval_expr.c` (kept) | ~300 | `apply_binary_op`, `eval_expr_bp`, `eval_expression`, number parsing |
| `eval_primary_atoms.c` | ~150 | Atoms (NUMBER, QUOTED, COLON, unary MINUS), simple list parsing |
| `eval_paren.c` | ~250 | Both grouped expressions `(expr)` and varargs `(prim a b c)` paths |
| `eval_proc_call.c` | ~200 | Primitive and user-procedure calls and arg staging/deferral |

Pre-requisite refactors: extract one shared `eval_infix_continuation` (P3-003), one `collect_*_args` helper (P3-004), and centralise operator dispatch in `apply_binary_op` (P3-007).

---

## 7. Suggested fix order

1. **Stop the bleeding (P0).** P4-008 first (memory corruption); then P5a-002, P5a-003, P5c-001, P5c-002, P1-001, P4-005, P4-010.
2. **Logo-semantics gaps users will hit.** P2-001 (`;` comments) is the last open item. P5b-001 (`parse` nested brackets), P5b-002/003 (silent truncation), P5c-003 (load truncation), and P3-009 (TCO through `if`) are resolved.
3. **Cross-cutting cleanup sweep.** Themes table in §1 — buffer policy, hard-limit centralisation, lexer/token-source dedup, atom-interning policy.
4. **Refactor P2 backlog.** Split `eval_expr.c` (§6), extract REPL/load duplication (P6-002), centralise operator/precedence definitions.
5. **Polish P3 nits** opportunistically as files are touched.

---

## 8. Verification checklist

- [ ] Every P0 fix lands with a failing-test-then-fix commit (Logo-input reproducer included in each finding).
- [ ] After §3 sweep, run `cmake --build --preset=tests && ctest --preset=tests` to confirm green baseline.
- [x] Re-run with ASan/UBSan (host build) to validate the leak/UB findings (P5b-007 in particular). All 43 test executables pass under AddressSanitizer (`build-tests-asan`, host clang). macOS ASan does not support `detect_leaks=1`, but use-after-free, double-free, and heap-corruption checks are clean. Re-run with `cmake --build build-tests-asan && (cd build-tests-asan && ctest)`.
- [ ] Add a primitive ↔ help-entry coverage test (P6-008).
- [ ] Add a SAVE→LOAD round-trip test for procedures containing string literals with `[` and `]` (P6-004).
