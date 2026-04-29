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

### Foundations & memory

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P1-002 | M | [core/memory.h:30-40](../core/memory.h#L30-L40), [core/memory.c:196](../core/memory.c#L196) | Dual-pool collision invariant (`atom_next < node_bottom`) only documented in ASCII art; not asserted. | Add named `mem_would_collide()` helper plus assertion at allocation sites. |
| P1-003 | M | [core/memory.c:199](../core/memory.c#L199) | Atom-table cap of `0x8000` (high-bit reserved as word marker) only enforced in `mem_atom`, not in lookups. | Add `_Static_assert(LOGO_ATOM_LIMIT <= 0x8000)`; introduce `CELL_WORD_MARKER` `#define`. |
| P1-004 | X | [core/memory.c:279-286](../core/memory.c#L279-L286) | Node index assumes `LOGO_MEMORY_SIZE/4 <= 0xFFFF`; silent overflow if size grows. | `_Static_assert` at compile time. |
| P1-005 / P1-006 | C | [core/memory.c:228-240](../core/memory.c#L228-L240) | `node_to_index`/`index_to_node` encoding is dual (16-bit cell index vs 32-bit Node), explained only in scattered comments; `0x8000` magic literal repeated 3×. | Add a header section comment + `CELL_WORD_MARKER`. |
| P1-009 | X | [core/memory.c:616-625](../core/memory.c#L616-L625) | `gc_mark_index` validates pointer range but not cell index; corrupted root could mark unrelated memory. | Compute and check `max_index` at top of mark loop. |
| P1-010 | M | [core/value.h:39-54](../core/value.h#L39-L54) | `Value`/`Result` unions have no compiler-enforced tag check; reading wrong field is UB. | Provide `value_get_*`/`result_get_*` accessor macros that assert tag. |
| P1-013 | X | [core/memory.c:165](../core/memory.c#L165) | `mem_newline_marker` singleton atom isn't documented as a permanent GC root; any caller forgetting to mark it can lose the sentinel. | Document; mark unconditionally inside `mem_gc()`. |
| P1-014 | X | [core/frame_arena.c:1-20](../core/frame_arena.c#L1-L20) | `arena_init` clamps capacity but doesn't validate `size_bytes` matches; mismatch → OOB. | Reject `size_bytes < capacity_words * 4`. |
| P1-016 | M | [core/value.c:86-128](../core/value.c#L86-L128) | `value_extract_xy` and `value_extract_rgb` duplicate identical list-of-numbers extraction. | Single `extract_number_list(node, n, out, &err)` helper. |
| P1-017 | C | [core/error.h:64-68](../core/error.h#L64-L68) | `CaughtError` pointer lifetime not documented; risk of dangling references. | Doc-comment ownership rules. |

### Lexer / token source

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P2-001 | L | [core/lexer.c:115-130](../core/lexer.c#L115-L130) | `;` comments are not stripped; `; foo` is tokenised as a word, contradicting [Pico_Logo_Reference.md §3840-3892](Pico_Logo_Reference.md). | Skip from `;` to end-of-line in `skip_whitespace()`; add tests (P2-012). |
| P2-002 | L | [core/lexer.c:118-130](../core/lexer.c#L118-L130) | Line continuation (`~` prompt on unbalanced brackets) is not specified to live in lexer or REPL; `newline_count` field is dead. | Decide ownership and document. If REPL-only, delete unused lexer state. |
| P2-003 | C/L | [core/lexer.c:396-413](../core/lexer.c#L396-L413) | `should_be_unary_minus` rules are subtle and partly contradict reference (e.g. binary minus after WORD); risks silent semantic divergence. | Rewrite with reference rules quoted in comments; add edge-case tests (`word-foo`, `(5+3) -2`). |
| P2-006 | X | [core/lexer.c:279, 388](../core/lexer.c#L279) | `looks_like_number` returns `true` on incomplete exponent like `1e`; later `read_number` accepts the bad token. | Make lookahead the validator (return token-type, not bool) or fail-stop in `read_number`. |
| P2-005 | C | [core/lexer.c:201-220](../core/lexer.c#L201-L220) | First-char-after-quote rules (brackets always escape, slash special-case) are correct but reasoning buried. | Quote reference §3862-3876 in code; add tests for `"\/"` and `"path/file"`. |
| P2-010 | L | [core/token_source.c:185-200](../core/token_source.c#L185-L200) | NodeIterator's minus classification simpler than lexer's → list-mode and text-mode evaluate the same expression differently. | Unify via shared `logo_classify_minus`; add NodeIterator minus tests (currently absent). |
| P2-016 | L | [core/token_source.c:60-90](../core/token_source.c#L60-L90) | `n/N` exponent allows sign in `is_number_word` but lexer rejects sign on `n/N`. Same input parsed two different ways. | Align both to reference rule. |
| P2-012 | L | [tests/test_lexer.c](../tests/test_lexer.c) | No tests for `;` comments. | Add coverage as part of fixing P2-001. |
| P2-017 | L | [core/lexer.c:437-500](../core/lexer.c#L437-L500) | Slash-in-quoted-word rule for file paths is asymmetric (first vs non-first char); no tests. | Add tests; document in code. |

### Evaluator core

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P3-008 | L | [core/primitives_conditionals.c:22-37](../core/primitives_conditionals.c#L22-L37) | `if` registered with `default_args=2` but special-cases `argc>=3`; reference requires parens for the 3-arg form. Behaviour appears correct but the registration is misleading. | Add a clarifying test (`if x [a] [b]` without parens must error) and a comment on the registration. |
| P3-009 | L | [core/eval_steps.c:480-510](../core/eval_steps.c#L480-L510), [core/eval.c:461-477](../core/eval.c#L461-L477) | `eval_push_if` sets `OP_FLAG_ENABLE_TCO`, but `step_if` doesn't propagate it to the branch's `OP_RUN_LIST_EXPR` → tail-recursive `if :n>0 [recurse]` is not in tail position. | Forward the flag; add a test that exercises deep recursion through an `if`. |
| P3-006 | S | [core/eval_expr.c:450-461, 614-628](../core/eval_expr.c#L450-L461) | Speculative `OP_PRIM_CALL` staging (push-then-discard if args defer) is hard to reason about. | Switch to eager evaluation of primitive args; defer only when a user proc is encountered. |
| P3-012 | X | [core/eval_expr.c:458, 620, 750](../core/eval_expr.c#L458) | Argument collection conflates EOF and `]`/`)` bounds; verify no over-consumption past `]` in nested forms. | Add a test for `repeat 3 [ f 1 2 ]` style nesting; tighten bounds in `eval_at_end`. |
| P3-017 | X | [core/eval.c:268-295](../core/eval.c#L268-L295) | `eval_run_list` re-entrancy under pause/freeze: `in_tail_position`/`proc_depth` save/restore vs single global `op_stack` is not obviously safe. | Document the re-entrancy contract; add a pause-during-tail-call test. |
| P3-016 | L | [core/eval_expr.c:618-627](../core/eval_expr.c#L618-L627) | `output` forces tail position whenever inside a procedure, even when itself not in tail. May over-trigger TCO. | Confirm against reference; restrict to true tail position. |
| P3-021 | X | [core/primitives_conditionals.c:22-37](../core/primitives_conditionals.c#L22-L37) | If `(if)` reaches the primitive with `argc==0` via varargs path, `args[0]` is UB. | Explicit `REQUIRE_ARGC(>=2)` guard. |

### Bindings

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P4-001 / P4-018 | C/X | [core/frame.c:277-285](../core/frame.c#L277-L285) | Variable lookup uses `strcasecmp` on interned atoms — both slower than pointer equality and risks false matches if interning isn't case-folding. | Decide interning policy; switch to `bindings[i].name == name`. |
| P4-003 | M | [core/frame.c:277-322](../core/frame.c#L277-L322) | `frame_find_binding` and `frame_find_binding_in_chain` duplicate the inner search. | Implement chain search as a loop over single-frame helper. |
| P4-004 | C | [core/procedures.c:16](../core/procedures.c#L16), [core/procedures.h:34](../core/procedures.h#L34), [core/variables.c:19](../core/variables.c#L19) | `MAX_PROCEDURES`, `MAX_PROC_PARAMS`, `MAX_GLOBAL_VARIABLES` defined in `.c` files with no rationale or error story. | Move to one shared header with comments; ensure overflow paths set `error_context`. |
| P4-006 | L | [core/variables.c:90-105](../core/variables.c#L90-L105) | Dynamic scoping behaviour (MAKE updates ancestor frames before falling back to global) is correct but very subtle. | Add a comment explaining the spec behaviour next to the chain walk. |
| P4-011 | X | [core/procedures.h:30-33](../core/procedures.h#L30-L33) | `TailCall.args[MAX_PROC_PARAMS]` and `arg_count` invariants not asserted; size-mismatch silently breaks TCO. | Assert `arg_count == proc->param_count` when preparing the tail call. |
| P4-012 | C | [core/properties.c:66-87](../core/properties.c#L66-L87) | Per-name property lookup walks list twice per iteration; not asymptotically wrong but worth documenting cost. | Add complexity comment. |
| P4-014 | L | [core/properties.c:40-53](../core/properties.c#L40-L53) | Numbers in property lists are stored via word-atom round-trip; possible precision loss is undocumented. | Either document or store numbers as a typed atom format. |
| P4-015 | M | [core/variables.c:435-445](../core/variables.c#L435-L445) | `var_get_test` fall-through from frame to global is implicit. | Restructure or comment as a single decision. |
| P4-016 | M | [core/procedures.h:52-58](../core/procedures.h#L52-L58) | Global `frame_stack` ownership is fuzzy (procedures.c owns it, variables.c uses it). | State ownership in header. |
| P4-020 | X | [core/procedures.c:265-315](../core/procedures.c#L265-L315) | Mutating procedure flags (`step`, `trace`, `bury`) is not re-entrant safe vs running procedures. | Document constraints, or store flags in execution state. |

### Primitives — numeric / logic

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P5a-001 | L | [core/primitives_logical.c:20-25](../core/primitives_logical.c#L20-L25) | `and`/`or`/`not` use case-sensitive `strcmp` for `true`/`false`; rest of interpreter is case-insensitive. | Switch to `strcasecmp` (or use `REQUIRE_BOOL`). |
| P5a-005 | L | [core/primitives_arithmetic.c:309-312](../core/primitives_arithmetic.c#L309-L312) | `sum` and `product` registered with `default_args=2`; reference allows single-arg. Verify with parser whether `sum 5` works. | Either change registration or document why varargs path covers it; add a test. |
| P5a-010 | L | [core/primitives_conditionals.c:76-94](../core/primitives_conditionals.c#L76-L94) | `iftrue`/`iffalse` consult `var_get_test()` without checking whether a `test` has run in this procedure. | Make `test_valid==false` an explicit no-op or error per reference. |
| P5a-013 | C | [core/primitives_arithmetic.c:23-29](../core/primitives_arithmetic.c#L23-L29) | `prim_abs` uses `REQUIRE_NUMBER(args[0], n)` then `UNUSED(args)`; contradictory. | Drop the `UNUSED(args)`. |
| P5a-014 | X | [core/primitives_arithmetic.c:199-218](../core/primitives_arithmetic.c#L199-L218) | `prim_form` `fmt_buf[48]` could overflow on extreme values from `snprintf`. | Check `snprintf` return value. |

### Primitives — data / workspace

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P5b-001 | L | [core/primitives_words_lists.c:1005-1030](../core/primitives_words_lists.c#L1005-L1030) | `parse` silently drops nested brackets ("In a full implementation, we'd recursively parse"). | Implement with the lexer; or document the limitation and reject bracketed input with `ERR_DOESNT_LIKE_INPUT`. |
| P5b-002 / P5b-003 | X | [core/primitives_words_lists.c:942-975, 1097, 1129](../core/primitives_words_lists.c#L942-L975) | `word`, `lowercase`, `uppercase` truncate at 256 bytes silently. | Grow dynamically (using arena) or return `ERR_OUT_OF_SPACE`. |
| P5b-006 | L | [core/primitives_list_processing.c:1790-1830](../core/primitives_list_processing.c#L1790-L1830) | `reduce` fold direction not verified against reference. | Add test `reduce "sum [1 2 3 4]` and confirm against reference; add a comment. |
| P5b-007 | X | [core/primitives_list_processing.c](../core/primitives_list_processing.c) (`map`, `filter`, `crossmap`, `map.se`) | `malloc`/`free` paths aren't exception-safe — error in callback can leak `element_storage`/`result_word`. | Add `free` on error returns; consider arena allocation. Run with ASan. |
| P5b-009 | X | [core/primitives_list_processing.c:240-280](../core/primitives_list_processing.c#L240-L280) | `invoke_proc_spec` save/restore arrays sized at `MAX_PROC_PARAMS`; a proc with more params silently corrupts state. | Assert `param_count <= MAX_PROC_PARAMS`. |
| P5b-010 | L | [core/primitives_variables.c:21-26](../core/primitives_variables.c#L21-L26) | `thing` returns `ERR_NO_VALUE` on unknown; reference doesn't pin the behaviour. | Confirm against reference; document chosen behaviour and test. |
| P5b-011 | L | [core/primitives_procedures.c:355-400](../core/primitives_procedures.c#L355-L400) | `text` returns malformed `[[params] | NIL]` if procedure body is empty. | Guard / assert non-nil body. |
| P5b-012 | X | [core/primitives_workspace.c](../core/primitives_workspace.c) (PO/POALL/PONS) | Per-item 8 KB format buffer can overflow silently for large procedures. | Stream output line-by-line; remove the per-item buffer. |
| P5b-014 | X | [core/primitives_words_lists.c:780-810](../core/primitives_words_lists.c#L780-L810) | `member`/`memberp` mishandle leading/trailing space and use `mem_is_nil` inconsistently. | Add edge-case tests (`member "" [a b]`, `member "  " "a  b"`); normalise. |
| P5b-016 | L | [core/primitives_words_lists.c:700-750](../core/primitives_words_lists.c#L700-L750) | `item` 1-indexing matches reference; just confirm with a documented test. | Add explicit test. |
| P5b-018 | X | [core/primitives_procedures.c:195-280](../core/primitives_procedures.c#L195-L280) | `proc_define_from_text` newline tracking across multi-line bracket bodies is untested and may insert/lose blank lines. | Add tests for `to foo [...\n[...]\n...]`. |
| P5b-020 | L | [core/primitives_text.c:85-110](../core/primitives_text.c#L85-L110) | `setcursor` only checks `< 0`, not upper bound (assumed 40×32). | Validate against device-reported screen size; return error if out of range. |

### Primitives — world / files / debug

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P5c-003 | X | [core/primitives_files_load_save.c:110-115](../core/primitives_files_load_save.c#L110-L115) | Procedures > `LOAD_MAX_PROC` (4 KB) are silently truncated. | Return `ERR_OUT_OF_SPACE` and skip the procedure. |
| P5c-004 | X | [core/primitives_files.c:184](../core/primitives_files.c#L184) | `pos_f` cast to `long` without range validation — UB outside `[LONG_MIN, LONG_MAX]`. | Validate before cast. |
| P5c-005 | C | [core/primitives_turtle.c:375-383](../core/primitives_turtle.c#L375-L383) | `towards` normalises angle via `while` loop — pathological for huge magnitudes. | Replace with `fmodf`. |
| P5c-007 | L | [core/primitives_hardware.c:72-74](../core/primitives_hardware.c#L72-L74) | `toot` does not validate frequency against reference range 131–1976 Hz. | Range check; return `ERR_DOESNT_LIKE_INPUT`. |
| P5c-008 | L | [core/primitives_network.c:74-75](../core/primitives_network.c#L74-L75) | NTP timezone offset not validated. | Range-check ±14h. |
| P5c-010 | C | [core/primitives_files_load_save.c:186-195](../core/primitives_files_load_save.c#L186-L195) | `startup` change detection compares node pointers, not values. | Use semantic equality, or document the limitation. |

### REPL / format / help / highlight

| ID | Axis | File:line | Observation | Recommendation |
|---|---|---|---|---|
| P6-002 | M | [core/repl.c:33-60](../core/repl.c#L33-L60) vs [core/primitives_files_load_save.c:29-55](../core/primitives_files_load_save.c#L29-L55) | Two copies of `line_starts_with_to`/`line_is_end`. | Extract to a shared utility used by both. |
| P6-004 | L/X | [core/format.c:565-571](../core/format.c#L565-L571) | Procedure-definition formatter counts raw `[`/`]` tokens for indentation; `print "["` derails it and breaks SAVE→LOAD round-trip. | Use a lexer-aware depth counter; add a round-trip test. |
| P6-007 | M | [core/help.c:7-25](../core/help.c#L7-L25) | Binary-search assumes sorted `help_entries[]` — only enforced by an external `awk` script. | Add a one-time sort-order spot-check at first lookup and document the requirement in `help.h`. |
| P6-008 | L | [core/help.c](../core/help.c), [core/primitives.c](../core/primitives.c) | No coverage check that every registered primitive has a help entry. | Add a startup validation and a unit test. |

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
2. **Logo-semantics gaps users will hit.** P2-001 (`;` comments), P5b-001 (`parse`), P5b-002/003 (silent truncation), P5c-003 (load truncation), P3-009 (TCO through `if`).
3. **Cross-cutting cleanup sweep.** Themes table in §1 — buffer policy, hard-limit centralisation, lexer/token-source dedup, atom-interning policy.
4. **Refactor P2 backlog.** Split `eval_expr.c` (§6), extract REPL/load duplication (P6-002), centralise operator/precedence definitions.
5. **Polish P3 nits** opportunistically as files are touched.

---

## 8. Verification checklist

- [ ] Every P0 fix lands with a failing-test-then-fix commit (Logo-input reproducer included in each finding).
- [ ] After §3 sweep, run `cmake --build --preset=tests && ctest --preset=tests` to confirm green baseline.
- [ ] Re-run with ASan/UBSan (host build) to validate the leak/UB findings (P5b-007 in particular).
- [ ] Add a primitive ↔ help-entry coverage test (P6-008).
- [ ] Add a SAVE→LOAD round-trip test for procedures containing string literals with `[` and `]` (P6-004).
