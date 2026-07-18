# Design: `launch` background processes (P6)

Status: **v1 — gate closed 2026-07-12.** Q1–Q6 resolved with the user (all
recommendations accepted, §12); implementation may begin at M0
Target: all three boards (uniform SRAM process pool per Q1)
Author: design notes for review

## 1. Goals

1. **`launch [instrs]`** — run an instruction list as a background process
   while the foreground program (or the user at the prompt) keeps going.
   The MicroWorlds model: *a process is a running list, not a special kind
   of turtle* (multi-sprite design §8).
2. Processes compose with everything P5 shipped: a launched list can
   `tell`/`ask` turtles, arm demons, draw, and play sound.
3. Deterministic, classroom-safe semantics: one thread of control
   (cooperative round-robin, no preemption), one lifetime rule
   (*nothing acts on its own after a reset*), errors that stop cleanly.
4. Uniform across boards if the budget allows (Q1): the flagship game
   feature should not be PSRAM-gated without a fight.

Non-goals: preemptive scheduling or core-1 execution; process priorities;
inter-process channels beyond broadcast/shared globals; `launch` outputting
process handles for fine-grained control (revisit on demonstrated need).

## 2. Prior art (survey in multi-sprite-design.md §3/§8)

- **TRS-80 Color Logo `HATCH`** — each turtle an independent process, one
  command per turn, round-robin; `SEND`/`MAIL` mailboxes. The scheduler we
  want; the turtle-process fusion we don't (turtle 0 "runs out of work").
- **MicroWorlds `launch`/`forever`/`clickon`** — background instruction
  lists owned by the page, composing with `talkto`. The user-visible model
  adopted here. Their `forever` is subsumed by `launch [forever [...]]` —
  our `forever` already exists as a blocking loop, and composition beats a
  second spelling.
- **NetLogo `hatch`** — means *clone a turtle*; a naming collision worth
  avoiding. We keep `launch`.
- **Scratch `broadcast`/`when I receive`** — the inter-process signal that
  replaced mailboxes; §8 assigns it to this design (see §7).

## 3. The model

A **process** is a paused evaluation: an instruction list plus everything
needed to resume it — its own `Evaluator`, op stack, and frame arena. The
foreground REPL/program is *not* a process; it owns the display, keyboard,
and the existing global evaluator state, exactly as today.

- `launch [instrs]` copies nothing: the list Node is the program (same
  retention rule as demons; GC-marked while the process lives).
- Process slots come from a fixed table, `MAX_PROCESSES` in
  `core/limits.h` (capacity and backing store: Q1). Table full →
  `ERR_OUT_OF_SPACE`, the demons precedent.
- A process ends when its list runs out, when `stop` executes at its top
  level, when `halt` clears the table, or when the lifetime rules (§8)
  fire.

**Per-process state** (context-switched by the scheduler):

| State | Mechanism |
|---|---|
| Continuation | own `Evaluator` + own op stack + own frame arena |
| Active turtle set | `tell` state saved/restored per process; inherited from the launcher at `launch` time (Q6) |
| Sleep deadline | `wait` in a process sets `wake_ms` and yields; the scheduler skips it until due (§6) |
| Error context | per-`Evaluator` already (`error_code`/`error_context`) |

Global-by-design (shared, unguarded — one thread of control): the
workspace (procedures, variables, properties), the node pool, demons, the
display, sound. Two processes racing `make` on the same name is defined
behaviour (last write wins), same as two demon actions today.

## 4. Feasibility: what the evaluator already gives us, and the one gap

The multi-sprite doc's claim — "the evaluator is already a step
trampoline; one step per process per turn is Color Logo's scheduler
verbatim" — is *almost* true. Audit results (2026-07-12):

- **Op-driven (suspendable):** procedure bodies, TCO'd tail calls,
  `repeat`/`forever`/`while`/`for`/`if`/`catch`/`runresult`, and — the
  case that matters — user proc calls made *inside a procedure* when not
  collecting user-proc args (`eval_expr.c:755` pushes `OP_PROC_CALL`).
  All continuation state lives in the op stack and frame arena, both
  already offset-addressed (`word_offset_t`), so it survives between
  scheduler turns.
- **C-stack synchronous (not suspendable):** `eval_run_list` /
  `eval_run_list_expr` called from primitives (`map`/`filter`/`apply`
  lambdas, demon conditions/actions), and proc calls at a run's top level
  or inside user-proc arg collection (`eval_expr.c:795`).

**The yield rule that follows:** a process may yield only at an op
boundary of its *outermost* trampoline (C stack at the scheduler's entry
frame). `eval_trampoline` gains a step budget checked only when
`base_depth == 0` on a process evaluator; exhausting it returns a new
`RESULT_YIELD` with all state parked in the process's op stack/arena.
Nested sub-trampolines run to completion within one turn — acceptable,
because they are bounded computations (a `map` over a list, arg
collection), while the unbounded constructs (`forever`, recursion via
TCO, `repeat`) are all op-driven and yield freely. A process whose single
instruction genuinely blocks for a long time (e.g. a huge `map`) stalls
its peers for that turn, not forever — documented, not defended against.

No coroutine stacks, no `setjmp`: a suspended process holds **zero C
stack** by construction.

## 5. Memory budget — the crux

Measured on the host (64-bit; device figures will be smaller with 32-bit
pointers — re-measure at M0 and record here):

| Structure | Host size | Notes |
|---|---|---|
| `Evaluator` | 144 B | per process |
| `EvalOp` | 224 B | op stack entry |
| `OpStack` | 58,376 B | depth 256 + 128-value spill — the problem |
| `FrameStack` | 24 B | header; arena memory is separate |
| Global frame arena | 32 KB | `FRAME_STACK_SIZE`, single static |
| Free SRAM (pico2) | ~34 KB | RAM at 93.5 % after the 2026-07-11 wins |

A full-size op stack per process is out of the question. Two changes make
the numbers work:

1. **Runtime-sized `OpStack`.** Today `ops[MAX_OP_STACK_DEPTH]` is inline
   in the struct. Change to a pointer + capacity (the global one keeps its
   static backing; behaviour unchanged). Process op stacks can then be
   shallow — depth 24–32 — and the spill area small (2 spill calls, not
   8). A process hitting its ceiling gets `ERR_STACK_OVERFLOW`, exactly
   like the foreground does at depth 256.
2. **Per-process frame arenas** carved from one static pool (or PSRAM,
   Q1). Processes are animation loops and game logic, not deep recursion;
   2–4 KB each is the working hypothesis, validated at M3 with the game
   retrofit.

Indicative per-process cost on target (32-bit estimates, to be measured):
depth-32 op stack ≈ 4–5 KB, trimmed spill ≈ 0.5 KB, 3 KB arena, evaluator
+ bookkeeping ≈ 0.2 KB → **≈ 8 KB per process**. Four processes ≈ 32 KB —
right at the pico2 ceiling; two ≈ 16 KB — comfortable. Per Q1 the pool is
uniform SRAM on all boards, count fixed after M0's target measurements;
trimming the foreground op stack (256 → 192 frees ~9 KB device-side) is
the lever if the numbers land tight.

## 6. Scheduler

- **Poll sites:** exactly the demon sites — the top of `eval_instruction`
  and the device prompt idle loop — via a `processes_maybe_run()` sibling
  of `demons_maybe_poll()`. No new hook points in devices.
- **Turn:** round-robin over live, awake processes; each gets a burst of
  `PROCESS_STEPS_PER_TURN` trampoline steps (tune at M3; start ~32) or
  until it yields/ends/errors. Budget-gated like demons (`DEMON_POLL_MS`
  precedent) so tight foreground loops aren't taxed.
- **Re-entrancy:** a process step runs `eval_instruction` internally,
  whose poll sites must not recurse into the scheduler or fire demons
  mid-step: one shared guard (the `g_polling` pattern) covers both.
  Demons keep firing from foreground polls only; demon actions and
  process steps never nest each other.
- **`wait` in a process** sets the process's `wake_ms` and yields its
  turn — the process sleeps, everything else runs. (Foreground `wait` is
  unchanged.) This single special case covers the dominant classroom
  pattern (`launch [forever [... wait 100]]`) without inventing a
  general blocking framework.
- **Other blocking primitives in a process** (Q4): keyboard readers
  (`readlist`/`readchar`) error with `ERR_UNSUPPORTED_ON_DEVICE`-style
  messaging — the keyboard belongs to the foreground; `play` on a full
  queue yields like `wait` (recheck next turn) rather than spinning.

## 7. Signalling: broadcast (scope: Q3)

With one thread of control, shared globals + demons already signal:
`make "go true` … `when [:go] [...]`. The Scratch-shaped sugar, if wanted:

- `broadcast word` — latch the message for exactly one full scheduler
  round + demon poll, so every process and demon gets one chance to see
  it.
- `message? word` — reporter, true while the latch holds. Composes as a
  demon condition (`when [message? "go] [...]` — edge-triggered for free)
  and as a plain test inside a process loop.

Decision (Q3): **deferred** — the game retrofit (M3) will show whether
globals-as-signals feel clumsy in practice; the sugar above is the design
on file if they do.

## 8. Lifetime, errors, and control

One rule, extended: *nothing acts on its own after an error.*

- error/`throw "toplevel` unwind: **halt all processes**, alongside the
  existing demon-clearing and motion/animation stopping
  (`primitives_control_reset.c` toplevel path).
- `cs`/`draw` stop autonomous motion/animation but leave processes (and
  demons) running: a screen clear is a drawing matter, not an
  event-handler teardown, so a redraw must not kill background work
  (revisited 2026-07-18 alongside `cleardemons`, see
  [multi-sprite §7](multi-sprite-design.md)).
- **BREAK** stops foreground *and* all processes (the sound design's
  BREAK-silences-music precedent).
- `freeze` suspends processes along with demons and motion; `thaw`
  resumes them (no credit for frozen time on `wake_ms`).
- **`halt`** — new primitive: stop all background processes, touch
  nothing else (not demons, not motion — error-unwind remains the big
  hammer).
  `(launch)` with no inputs prints the live processes (the `(when)`
  precedent). Naming and scope decided as Q5.
- **`stop` / `output` in a launched list:** `stop` at process top level
  ends the process; `output` there is an error (nothing to receive it) —
  both mirror toplevel run-list rules today.
- **Error in a process** (Q2, decided): report and **unwind
  everything to toplevel**, killing all processes — the same rule as a
  demon action erroring. One rule everywhere beats per-process
  containment for debuggability on a 320×320 screen; revisit if games
  want crash-isolation.
- **`catch`/`throw`** are process-local (each process has its own op
  stack, so `OP_CATCH` scoping just works); a `throw` escaping a
  process's top level is an error, like demons/`pause`.
- **Workspace mutation while running:** `erase`/`to` redefining a
  procedure a process is executing — same exposure demons have today
  (Node lists are GC-marked, so nothing dangles; the process keeps
  running the old body it holds). Documented, not prevented.

## 9. Integration points

- **GC:** `processes_gc_mark_all()` marks each live process's program
  list plus every `Value` reachable from its op stack and frame arena
  (the demons and frame-sync precedents; the op-stack walk is new —
  needed anyway if atom reclamation ever lands).
- **Devices:** no device interface changes at all — the scheduler lives
  entirely in core. The host build runs processes fully (no graphics
  needed), which makes the whole feature unit-testable off-hardware.
- **P7/P8 coexistence:** HTTP-server demons and the sound sequencer poll
  at the same sites; the shared re-entrancy guard and per-site budgets
  keep the three from starving each other (validate at M3).

## 10. Milestones

- **M0 — evaluator groundwork.** Runtime-sized `OpStack`; `RESULT_YIELD`
  + step budget in `eval_trampoline` (outermost-only); re-measure §5
  numbers on target; no behaviour change for existing code (full ctest +
  firmware links green).
- **M1 — process table + scheduler.** `launch`, `halt`, `(launch)`
  print form; round-robin at the poll sites; per-process tell-set and
  `wait`-sleep; lifetime rules (error/BREAK/`freeze`/`halt`); GC marking;
  Unity tests on mock + host e2e.
- **M2 — polish.** Blocking-primitive rules (Q4) enforced; reference
  sections (`launch`, `halt`, updates to `wait`/`stop`/`freeze`).
  Broadcast stays out per Q3.
- **M3 — hardware validation + game retrofit.** Re-cut a Space
  Invaders/Galaxian subsystem (e.g. attack waves) onto `launch`; measure
  SRAM, frame pacing, and scheduler fairness; tune
  `PROCESS_STEPS_PER_TURN` and arena sizes; record results here.

## 11. Risks

- **SRAM.** The whole feature fits or it doesn't — M0's target
  measurements gate everything (Q1 fallback: PSRAM tier).
- **Sub-trampoline stalls.** A process doing a huge `map` hogs its turn;
  if real programs hit this, the answer is documentation ("long
  primitives run whole"), not engineering.
- **Poll-site congestion.** Demons + turtle ticks + sound + HTTP + the
  scheduler all hang off `eval_instruction`; M3 must confirm the budgets
  compose (each is individually cheap and budget-gated today).

## 12. Decisions (gate closed 2026-07-12)

All six resolved with the user; the recommendation was accepted in each
case.

- **Q1 — capacity & backing store:** small **uniform SRAM pool** on all
  boards (`MAX_PROCESSES` 2–4, final count sized after M0 target
  measurements). Uniformity is worth more than headcount; PSRAM tiering
  stays the fallback only if M0 says SRAM cannot fit two processes.
- **Q2 — error in a process:** **unwind everything to toplevel** (the
  demon rule), killing all processes. One rule everywhere.
- **Q3 — broadcast:** **deferred** — not in M2. Shared globals + demons
  are the signalling story; revisit if the M3 game retrofit shows
  globals-as-signals hurting.
- **Q4 — blocking primitives in a process:** **keyboard readers error;
  `play` on a full queue yields** (as §6).
- **Q5 — `halt`:** stops **background processes only** — demons and
  motion untouched; error-unwind (not `cs`) remains the big reset
  (revisited 2026-07-18: `cs` clears neither demons nor processes, only
  motion — see §8).
- **Q6 — `launch` turtle context:** the process **inherits the
  launcher's active turtle set** at `launch` time.

## 13. Alternatives rejected

| Alternative | Why not |
|---|---|
| Turtle-as-process (`HATCH`) | Fuses two orthogonal ideas; NetLogo naming collision; multi-sprite §8 already rejected it |
| Per-process C stacks (coroutines) | 2–4 KB × N of SRAM we don't have, unguarded overflow on deep recursion, platform-specific stack switching on three targets + host |
| Preemptive / core-1 scheduling | Locking across the node pool, workspace, and display for a classroom feature; cooperative determinism is a teaching asset |
| `forever [instrs]` as a launcher (MicroWorlds) | Collides with our blocking `forever`; `launch [forever [...]]` composes |
| Process ids + `halt n` | Handle bookkeeping for no demonstrated need; `halt`-all + lifetime rules cover the classroom cases |
| Mailboxes (`SEND`/`MAIL`) | Broadcast + shared globals subsume them with one thread of control |

## Sources

- [TRS-80 Color Logo manual (Tandy, 1982)](https://colorcomputerarchive.com/repo/Documents/Manuals/Programming/TRS-80%20Color%20Logo%20(Tandy).pdf) — HATCH, round-robin "a turn is a single turtle command", SEND/MAIL.
- [MicroWorlds vocabulary (LCSI)](http://www.lcsi.ca/pdf/microworldsex/microworlds-ex-vocabulary.pdf) — launch/forever/clickon/talkto process model.
- Scratch broadcast / when-I-receive semantics (scratch.mit.edu documentation).
- [`multi-sprite-design.md`](multi-sprite-design.md) §3 (prior-art survey), §8 (the deferral this design answers).
