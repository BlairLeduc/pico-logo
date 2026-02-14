## Plan: Explicit Evaluator Stack (Operation-Stack VM)

**TL;DR** — Replace C-recursive control-flow evaluation with an explicit operation stack allocated in the existing FrameArena. Control-flow primitives (`repeat`, `forever`, `if`, `while`, `until`, `do.while`, `do.until`, `for`, `run`, `catch`, `iftrue`, `iffalse`) become **evaluator operations** dispatched by a trampoline loop, instead of C functions that call back into eval. Other primitives and the Pratt parser remain unchanged. Work is incremental: one primitive at a time, with tests at each step. The FrameArena gains a new "operation frame" type that stores per-operation state (loop counters, phase flags, saved token sources).

**Key decisions:**
- Operation-stack VM approach (not full continuation-passing)
- Pratt parser stays recursive (bounded depth)
- Incremental conversion, one control-flow primitive at a time
- FrameArena extended with operation frames (no separate stack)

---

### Architecture

**New concept: `EvalOp`** — a tagged union representing "what to do next." Each operation frame lives in the FrameArena (LIFO), alongside existing procedure `FrameHeader`s. The evaluator's main loop pops the top operation, executes one step, and either pushes new operations or continues.

```
EvalOp = {
  kind: enum (OP_RUN_LIST, OP_REPEAT, OP_FOREVER, OP_IF, OP_WHILE, OP_UNTIL,
              OP_DO_WHILE, OP_DO_UNTIL, OP_FOR, OP_RUN, OP_CATCH, OP_PROC_CALL, ...),
  phase: uint8_t,           // sub-step within the operation
  saved_source: TokenSource, // token source to restore when this op completes
  union { repeat_state, for_state, while_state, catch_state, ... }
}
```

**Main trampoline** — replaces the current pattern where `eval_run_list` calls `eval_instruction` in a C loop:

```
while (op_stack not empty) {
    op = peek top operation
    switch (op->kind) {
        case OP_REPEAT:    step_repeat(eval, op);    break;
        case OP_FOREVER:   step_forever(eval, op);   break;
        case OP_IF:        step_if(eval, op);        break;
        ...
        case OP_RUN_LIST:  step_run_list(eval, op);  break;
    }
}
```

Each `step_*` function executes **one phase** of the operation (e.g., one iteration of a repeat loop, or the predicate evaluation of a while loop), then either:
- Advances its phase and pushes a sub-operation (e.g., `OP_RUN_LIST` for the body)
- Pops itself when complete
- Propagates a non-normal result (STOP, OUTPUT, THROW, ERROR) by unwinding operations

---

### Steps

**Phase 0 — Foundation (operation stack infrastructure)**

1. Define `EvalOpKind` enum and `EvalOp` struct in a new file `core/eval_ops.h`. Include per-operation state unions for each control-flow construct. Start with just `OP_RUN_LIST` (the simplest — runs a list node as a sequence of instructions).

2. Add an operation stack to `Evaluator` in `core/eval.h` — a small fixed-capacity stack of `EvalOp` pointers (or arena offsets). Since operations nest but are LIFO, use arena allocation. Add `eval_op_push()`, `eval_op_pop()`, `eval_op_peek()` helpers in `core/eval_ops.c`.

3. Implement `eval_trampoline(Evaluator *eval)` in `core/eval.c` — the main dispatch loop. Initially it only handles `OP_RUN_LIST`, which calls `eval_instruction()` for each token in the list. This replaces the inner loop of `eval_run_list()`.

4. Rewrite `eval_run_list()` to push an `OP_RUN_LIST` operation and call `eval_trampoline()` instead of looping directly. **The external API (`eval_run_list`, `eval_run_list_expr`) stays the same** — callers don't need to change. The trampoline is an internal detail.

5. Add tests: verify that simple instruction sequences (`fd 100 rt 90 fd 100`) still work through the new trampoline path. Verify that `RESULT_STOP`, `RESULT_OUTPUT`, `RESULT_THROW`, `RESULT_ERROR` correctly unwind past `OP_RUN_LIST` operations.

**Phase 1 — One-shot primitives (`run`, `if`, `iftrue`, `iffalse`)**

6. Convert `prim_run` — instead of calling `eval_run_list_expr()` directly, it returns a new result status `RESULT_EVAL_LIST` (or pushes an `OP_RUN` operation). The trampoline handles it by pushing `OP_RUN_LIST` for the body. Since `run` is one-shot with no local state, this is the simplest conversion.

7. Convert `prim_if` — the primitive evaluates the condition (already done via args), then instead of calling `eval_run_list_expr(chosen_branch)`, it pushes `OP_RUN_LIST` for the chosen branch. No loop state needed.

8. Convert `prim_iftrue` / `prim_iffalse` — same pattern as `if`. Trivial.

9. Tests: existing `test_primitives_conditionals.c` and `test_primitives_control_flow.c` tests pass unchanged. Add specific tests for nested `if` / `run` to verify stack behavior.

**Phase 2 — Simple loops (`repeat`, `forever`)**

10. Define `RepeatState` struct: `{ Node body; int count; int i; int previous_repcount; }`. Define `ForeverState`: `{ Node body; int previous_repcount; int iteration; }`.

11. Convert `prim_repeat` — instead of looping in C, the primitive pushes an `OP_REPEAT` operation with its state and returns `RESULT_NONE`. The trampoline's `step_repeat()`:
    - If `i < count`: set `eval->repcount = i + 1`, push `OP_RUN_LIST` for body, advance `i`.
    - When body completes with `RESULT_NONE`: trampoline returns to `OP_REPEAT`, which checks next iteration.
    - On `RESULT_STOP`/`RESULT_OUTPUT`/`RESULT_THROW`/`RESULT_ERROR`: restore `repcount` and pop.
    - On loop completion: restore `repcount` and pop.

12. Convert `prim_forever` — same pattern but no count limit.

13. Tests: `repeat 5 [fd 10]`, `repeat 3 [repeat 2 [fd 1]]` (nested), `repeat 1000000 [fd 1]` (deep iteration without C stack growth — **new test**), `forever` with `STOP`.

**Phase 3 — Predicate loops (`while`, `until`, `do.while`, `do.until`)**

14. Define state structs with a `phase` field (0 = evaluate predicate, 1 = execute body, or reversed for do-variants). The trampoline alternates between phases.

15. Convert `prim_while` / `prim_until` — push `OP_WHILE`/`OP_UNTIL`. Step function:
    - Phase 0: push `OP_RUN_LIST` for predicate (as expression). On return, check bool result. If continue: set phase=1. If done: pop.
    - Phase 1: push `OP_RUN_LIST` for body. On return: set phase=0 (next iteration).

16. Convert `prim_do_while` / `prim_do_until` — same but body runs first (phase 0 = body, phase 1 = predicate).

17. Tests: existing tests plus deeply nested while loops.

**Phase 4 — `catch`**

18. Define `CatchState`: `{ const char *tag; }`. Push `OP_CATCH` operation. The trampoline's `step_catch()`:
    - Phase 0: push `OP_RUN_LIST` for body.
    - On body completion: if `RESULT_THROW` with matching tag → swallow. If `RESULT_ERROR` and tag is "error" → catch. If "toplevel" → always propagate. Otherwise propagate.

19. The key insight: catch frames on the operation stack serve as **unwind targets**. When a `RESULT_THROW` propagates, the trampoline pops operations until it finds a matching `OP_CATCH`.

20. Tests: `catch "err [throw "err]`, nested catch, catch "error for runtime errors, throw "toplevel always propagates.

**Phase 5 — `for` (most complex)**

21. Define `ForState`: `{ const char *varname; float start, limit, step, current; Node body; Value saved_value; bool had_value; uint8_t phase; }`.

22. Convert `prim_for` — push `OP_FOR`. Step function phases:
    - Phase 0: evaluate start → phase 1
    - Phase 1: evaluate limit → phase 2
    - Phase 2: evaluate step (if provided) → phase 3
    - Phase 3: loop body iteration (set var, push `OP_RUN_LIST`, advance `current`)
    - Cleanup: restore variable on completion

23. Tests: `for [i 1 10] [fd :i]`, nested for loops, for with list arguments for start/limit/step.

**Phase 6 — User procedure calls through the trampoline**

24. The existing `RESULT_CALL` / CPS path already avoids C recursion for user→user procedure calls. Integrate this with the operation stack: instead of `RESULT_CALL` propagating up the C stack to be caught by `eval_run_list`, the trampoline directly handles it by pushing an `OP_PROC_CALL` operation.

25. This eliminates the need for `RESULT_CALL` to be caught at multiple points (`eval_run_list`, `eval_run_list_expr`, `eval_run_list_with_tco`, `proc_call` trampoline). The operation stack becomes the single dispatch point.

26. Preserve TCO: when a tail-position self-recursive call is detected, use `frame_reuse` as today.

27. Tests: deeply recursive user procedures (e.g., `to countdown :n if :n = 0 [stop] countdown :n - 1 end; countdown 100000`) — should succeed without C stack overflow. **This is the key validation test.**

**Phase 7 — Cleanup and hardening**

28. Remove the old `eval_run_list` / `eval_run_list_expr` C-loop internals (now just thin wrappers that push ops and call the trampoline).

29. Add a configurable operation stack depth limit (e.g., 1024 ops) with a clear error message: `"Stack overflow — too many nested operations"`. This replaces the undefined C stack overflow with a graceful error.

30. Remove `MAX_RECURSION_DEPTH` (128, currently unenforced) and replace with the operation stack depth check.

31. Update `primitive_arg_depth` handling: when collecting args for a primitive, user procedure calls still need to produce a value synchronously. In Phase 6, ensure this case pushes `OP_PROC_CALL` and runs the trampoline to completion (sub-trampoline), rather than falling back to C recursion.

32. Memory audit: measure actual per-operation frame sizes. With `EvalOp` ≈ 60–80 bytes (TokenSource + state union), a 32KB arena supports ~400–500 nested operations. Verify this is sufficient; if not, increase `FRAME_STACK_SIZE` (the RP2350 has 420KB available).

---

### Operation Frame Memory Layout (in FrameArena)

```
EvalOp (estimated 16-20 words = 64-80 bytes):
├── kind         : uint8_t
├── phase        : uint8_t  
├── flags        : uint8_t (e.g., is_expression, enable_tco)
├── _pad         : uint8_t
├── prev_offset  : word_offset_t  (link to previous op)
├── saved_source : TokenSource (~12 words)
├── result       : Result (~4 words, for inter-phase value passing)
├── union {
│     RepeatState  { body:Node, count:int, i:int, prev_repcount:int }     // 4 words
│     ForeverState { body:Node, prev_repcount:int, iteration:int }        // 3 words
│     WhileState   { predicate:Node, body:Node }                          // 2 words
│     ForState     { varname:ptr, start:f, limit:f, step:f, current:f,    // ~10 words
│                    body:Node, saved_value:Value, had_value:bool }
│     CatchState   { tag:ptr }                                            // 1 word
│     RunListState { /* nothing extra */ }                                 // 0 words
│   }
```

Worst case (`for`): ~30 words = 120 bytes. Typical (`repeat`): ~20 words = 80 bytes. With 32KB arena shared with procedure frames, budget allows ~200+ nested control-flow operations — more than enough given the 4K C stack previously limited us to ~20-30 levels.

---

### Verification

- **Unit tests**: All existing tests in `tests/test_primitives_control_flow.c`, `tests/test_primitives_conditionals.c`, `tests/test_primitives_exceptions.c`, `tests/test_eval.c` must pass unchanged after each phase.
- **New tests per phase**: Added alongside each conversion (see steps above).
- **Key regression test**: `repeat 10000 [fd 1]` — must not crash (currently would overflow 4K C stack on Pico).
- **Key recursion test**: Deep user procedure recursion (1000+ levels) — must succeed with explicit stack, fail gracefully with stack depth error at limit.
- **Build**: `cmake --build --preset=tests && ctest --preset=tests` after each phase.
- **Memory validation**: Check `FrameArena` usage under deep nesting to ensure we stay within budget on RP2040 (164KB total) and RP2350 (420KB total).

---

### Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| `primitive_arg_depth > 0` still needs synchronous eval | Sub-trampoline: run the op stack to completion inline when collecting primitive args |
| `for` pre-loop phases (eval start/limit/step as lists) add complexity | Phase enum with 5+ states; test each phase transition |
| Interaction with GOTO (label jumps in procedures) | GOTO only affects `execute_body_with_step`; keep that logic in `OP_PROC_CALL` step function |
| PAUSE (nested REPL) interaction | `RESULT_PAUSE` propagates up the op stack like STOP; the outer REPL re-enters the trampoline on resume |
| Debugging/tracing harder with explicit stack | Add op-stack dump to debug primitives (`primitives_debug.c`) |
