# P10 — Interpreter throughput (design)

Status: **v1 design, drafted 2026-08-01.** Opened by P9's M0 measurement,
which failed its gate and found the display was never the bottleneck. Like
P9, this design gates on measurement: M0 below is a benchmark harness and a
baseline, and no optimisation lands without a before-and-after number from it.

## 1. Goal

Make the interpreter fast enough that a Logo game loop fits in a frame.

Concretely: **Turtle Trails' `play.frame` must come in under 40 ms on a
Pico 2**, from 87.3 ms today. That is the smallest target that turns a
shipped game from ~9 fps into the 25 fps it was written for, and it is the
measurement this design is steered by.

This is not a rewrite. The evidence in §2 says roughly half of runtime is
spent rediscovering facts that cannot change, and the design is to stop doing
that.

## 2. Evidence

All from P9's M0 (`tilemap-scrolling-design.md` §3.3–§3.5), 2026-08-01.

### 2.1 The games miss their budgets, and it is not the screen

| | Present | Frame body | Actual rate |
|---|---:|---:|---:|
| Turtle Trails | 25.6 ms | 87.3 ms | ~9 fps |
| Checkpoint Run | 21.1 ms | 258.6 ms | ~4 fps |

Both ship at `(setrefresh "sync 25)`. `sync` does not wait when a frame
overruns, so they degrade quietly instead of failing — which is why this went
unnoticed until something measured it.

The host build runs the same interpreter against the mock device, where
drawing is a recorded command rather than a rasterised one. So the host:Pico
ratio separates interpreting from plotting:

| | Host | Pico 2 | Ratio |
|---|---:|---:|---:|
| Turtle Trails `play.frame` (draws almost nothing) | 0.96 ms | 87.3 ms | 91× |
| Turtle Trails `draw.board` (draws everything) | 55.1 ms | 5,916 ms | 107× |
| Checkpoint Run `play.frame` | 3.42 ms | 258.6 ms | 76× |
| Checkpoint Run `draw.sector` | 18.0 ms | 1,346 ms | 75× |

The ratio barely moves between a frame that draws nothing and a board build
that draws everything. **Rasterisation is a minor term.** What the Pico is
slow at is interpreting.

### 2.2 The profile

Sampling a pure `repeat [make "x (:x + 1)]` loop — no device work at all:

| Cost | Share |
|---|---:|
| Re-lexing and classifying words on every evaluation | 34 % |
| Name resolution by case-insensitive string compare | 14 % |
| Cons-cell walk and index→pointer indirection | 20 % |
| `memmove`/`memset` | 8 % |
| Actual evaluation and everything else | 24 % |

`classify_word` is the largest single leaf in the whole interpreter.

## 3. Diagnosis

### 3.1 Every evaluation re-lexes the list

A procedure body and a `repeat` body are Node lists of interned word atoms.
`token_source.c` walks that list on every pass and, for each word, calls
`classify_word()` — which re-reads the characters to decide quoted / variable
/ number / operator / bracket / plain word, calling `is_number_word()` to
re-parse digits, an exponent and a sign.

Nothing about a word's characters changes between passes. Atoms are
**interned and immutable** (`mem_atom`, `core/memory.c`): the same word is the
same atom offset every time. So the classification is a pure function of the
atom, recomputed on every single evaluation of every word in every loop.

### 3.2 Every call resolves its name by string compare

`eval_expr.c` and `eval_steps.c` resolve a call by calling
`primitive_find_n()` — a binary search over ~390 registered primitives, so
about nine `strncasecmp` calls — and then, on a miss, `proc_find_n()`, which
is a **linear scan** of the procedure table doing `strncasecmp` per entry
(`find_procedure_index_n`, `core/procedures.c`). Both games define around a
hundred procedures.

Measured by M0's benchmark on the host, a user-procedure call costs 0.43 µs
when it is the only procedure defined and 2.09 µs with the table full at
`MAX_PROCEDURES` (128, target defined last) — a **4.8× spread**, steeper
than the ad-hoc 0.57 → 0.96 µs figure this section first carried, which also
cited a 200-procedure workspace no stock build can hold. Both games sit near
a hundred procedures, so real calls pay most of the scan, on every call in
every frame.

The same hot path also does `strncasecmp(t.start, "output", 6)` inline on
every primitive call, to spot `output`/`op` for tail position.

Again the input is an interned atom and the answer changes only when the
procedure table changes.

### 3.3 Cons-cell access is indirect

`mem_car`/`mem_cdr` reach a cell through `index_to_node`/`get_node_ptr`,
which recompute a pointer from a pool index with bounds checks on each
access. At 20 % of samples this is the second-largest group, and unlike
§3.1–§3.2 it is not a memoisation problem — it is the cost of the
representation, and it needs its own measurement before anyone touches it.

## 4. The design: resolve once, on the atom

One idea covers §3.1 and §3.2. **An interned atom is an identity, so anything
derivable from its characters should be derived once and stored with it.**

The atom table (`core/memory.c`) already gives us the place. An entry is:

```
[next:2][len:1][chars:len][nul:1][padding to 4]
```

`next` already carries flag bits (`ATOM_LINK_FREE`, `ATOM_LINK_MARK`), so the
header is an established home for per-atom metadata.

### 4.1 What gets cached

- **Word class** — the `TokenType` `classify_word` would return.
- **Parsed numeric value** — for atoms that are numbers, so `is_number_word`
  and the later `strtof` both disappear.
- **Resolved binding** — primitive index, user-procedure index, or unbound.

### 4.2 The one context-dependent case

Classification is *not* quite context-free: a lone `-` is `TOKEN_MINUS` or
`TOKEN_UNARY_MINUS` depending on whether the previous token was a delimiter,
and a non-number word like `-foo` is a plain word in one position and a unary
minus in another (`classify_word`, `prev_was_delimiter`). Note `-5` is *not*
such a case: `is_number_word` accepts a leading sign before the delimiter
check is ever reached, so a signed number atom classifies as `TOKEN_NUMBER`
in every position.

This is the only exception, and it is confined to atoms whose first character
is `-`. Those get a distinguished cached class meaning **"consult
`prev_was_delimiter`"**, and keep exactly today's logic. Every other atom is
answered from the cache without looking at its characters. The rule stays
one line, and the semantics are unchanged by construction.

### 4.3 Invalidation

Word class and numeric value never need invalidating — atoms are immutable.
Storage reuse is the one hazard: when the GC frees an atom, its bytes go on a
free list (`atom_free_add`) and a later `mem_atom` may hand them to a
different word. Nothing invalidates lazily on collection, so **`mem_atom`
must initialise the cache fields every time it writes an entry — on both its
allocation paths**, the fresh-region one and the free-list-reuse one. That is
an obligation on the implementation, not a happy accident; §9 pins it.

Bindings do need invalidating. Primitives are registered once at startup and
never change, so only the procedure table moves. A **generation counter**
covers it — a global counter bumped inside `procedures.c`'s low-level
mutators (`proc_define`, `proc_erase`, and the table reset), so that every
caller — `define`, `copydef`, `erase`, `erall`, `load`, the planned `.reset`
— is covered by construction rather than by enumeration. One byte per atom
records the generation its binding was resolved under; a mismatch means
re-resolve. On counter wrap, one sweep of the atom region (≤ 32 KB, walkable
via `atom_entry_size`) clears every cached binding, which is correct and
rare.

Definition happens at load time, not in frame loops, so the cost lands where
nobody is counting milliseconds.

### 4.4 How a cached answer reaches the evaluator

Two plumbing facts bound the scope, and both must be explicit:

- **The cache serves the list path only.** Tokens come from two sources:
  the node iterator (procedure bodies, `repeat` bodies — everything a frame
  loop runs) and the lexer (raw text typed at the REPL). Lexer tokens point
  into the input line buffer, not at atoms, so they cannot hit the cache and
  keep today's path unchanged. That is the right trade: the hot 87.3 ms is
  entirely list-sourced.
- **A `Token` carries no atom identity.** The resolution sites —
  `eval_expr.c:309`/`:547`/`:687` and `eval_steps.c:52`/`:68` — see only
  `{type, start, length}`. So `Token` grows a `Node` field holding the source
  atom, set by `node_iter_next` and `NODE_NIL` on the lexer path; the sites
  consult the cache when it is set and fall back to string resolution when it
  is not. The inline `output`/`op` compare (§3.2) becomes an identity check
  against the resolved binding instead of a `strncasecmp`.

### 4.5 What this does *not* change

- **No semantic change anywhere.** Every cached answer is exactly what the
  function computes today; the tests that pin behaviour must pass untouched.
- **No change to the Logo surface.** No new primitive, no new syntax.
- **No new device dependency.** This is `core/`, so it is natively testable
  and benefits every board equally.

## 5. Memory budget

SRAM is at 95.6 % on `pico2`, so the shape of the cost matters more than its
size.

The atom cache costs **no new `bss`**. Atoms live inside the existing
`LOGO_MEMORY_SIZE` block (128 KB), growing up from the bottom while the node
pool grows down; a wider atom header consumes pool that is already allocated,
it does not enlarge the static array. What it does trade is *atoms against
nodes*, and that trade must be measured, not assumed:

| | Today | With a 1-byte class | + numeric payload |
|---|---|---|---|
| Entry size | `ALIGN4(len + 4)` | `ALIGN4(len + 5)` | `ALIGN4(len + 5) + 4`, numeric atoms only |
| Effect | — | +4 bytes on the 1 in 4 atoms where `len ≡ 0 (mod 4)`, +0 otherwise | +4 bytes per number atom |

So roughly **+1 byte per atom on average** for the class, plus 4 bytes on
each atom that classifies as a number — and numeric atoms are the distinct
digit literals of loaded programs, dozens rather than thousands.

The variable-width entry needs a flag, and the header has exactly one spare
bit: entries are 4-aligned so stored offsets leave bits 0 and 1 free, and
offsets fit 15 bits under `LOGO_ATOM_LIMIT` (32 KB) — `ATOM_LINK_FREE` uses
bit 0 and `ATOM_LINK_MARK` the top bit, leaving **bit 1** to mean "carries a
numeric payload". Two invariants make it safe: `atom_live_size` must include
the payload (or the GC's free-list arithmetic corrupts the region), and the
chain walks (`atom_chain_next`, `atom_free_next`) must mask the new bit. The
payload is read/written with `memcpy`, as the header fields already are,
since its offset is not 4-aligned.

M2's binding fields are wider than a byte but not much: kind (primitive /
procedure / unbound, 2 bits) plus an index (~390 primitives → 9 bits;
`MAX_PROCEDURES` is 128 → 7 bits) packs into 16 bits, plus the 1-byte
generation — 3 bytes, some of it landing in padding already bought. Whether
they belong in the atom header or in a small side cache is a decision for M2
with M1's numbers in hand — §8 records both options.

The acceptance check is the one P9 uses: all three firmware presets still link
and boot, and free node count after loading each shipped game does not fall
materially (the atom region's high-water mark recorded alongside it).

## 6. Milestones

- **M0 — benchmark and baseline. Done 2026-08-01.**
  `tests/test_bench_throughput.c` runs in ctest: the pure `repeat` loop,
  a user-procedure call at workspace sizes 1/64/128, both games'
  `play.frame` on the mock, and the hardware script executed end to end.
  It prints `BENCH` lines for the record and asserts only **relative**
  ratios — each scenario against an in-process calibration loop that slows
  down with the machine, plus the 128:1 workspace-scan ratio — so the guard
  does not flap on a loaded box. `logo/tests/p10m0` takes the same
  scenarios on a Pico 2 (the p9m0 pattern; frame bodies stay with
  `p9m0.trails`/`p9m0.checkrun`). Baseline in §6.1. **Nothing below lands
  without a before-and-after from this.**
- **M1 — cached word class.** Class and parsed numeric value on the atom;
  `classify_word` becomes a lookup with the `-` exception of §4.2, and the
  leading-`;` comment peek in `node_iter_next` folds into the same lookup.
  Targets the 34 %.
- **M2 — cached name binding.** Resolve primitive/procedure once per atom
  with the §4.3 generation counter; kill the inline `output`/`op` compare
  too. Targets the 14 %. Placement decided with M1's numbers.
- **M3 — re-measure and decide.** Run M0's benchmark and P9's `p9m0`
  scripts. If Turtle Trails is under 40 ms, P10 has met §1 and stops here.
  If not, M4.
- **M4 — representation, only if M3 says so.** The 20 % in `mem_car`/
  `mem_cdr` indirection and the 8 % in `memmove`/`memset`. Deliberately last:
  it is the invasive one, it touches the GC's assumptions, and §7 may make it
  unnecessary.

### 6.1 M0 baseline (2026-08-01)

Host is the benchmark's machine of record; the Pico 2 column is filled by
running `p10m0` on hardware (the game rows reuse P9's `p9m0` numbers).

| Scenario | Host | Pico 2 |
|---|---:|---:|
| `repeat [make "x (:x + 1)]` | 1.07 µs/iter | 92.4 µs/iter |
| user-proc call, 1 defined | 0.43 µs | - |
| user-proc call, 64 defined | 1.24 µs | - |
| user-proc call, 128 defined (full table, target last) | 2.09 µs | 127.6 µs |
| Turtle Trails `play.frame` | 0.84 ms | 87.3 ms (§2.1) |
| Checkpoint Run `play.frame` | 2.73 ms | 258.6 ms (§2.1) |

The frame rows sit slightly under §2.1's 0.96 / 3.42 ms because the method
differs — M0 times a bare `repeat [play.frame]` in manual refresh, where the
p9m0 harness collected per-frame min/max around each call. Before/afters
must compare against *this* table, taken by *this* benchmark.

## 7. Expected outcome, honestly

M1 and M2 together target 48 % of runtime. Removing it *entirely* would be
about **1.9×** — which is an upper bound, not a promise, because a cache
lookup is not free and the shares will shift once the top of the profile
moves.

One further discount is known now: part of the 14 % is not procedure-name
resolution at all. `make "x` and `:x` resolve through
`frame_find_binding_in_chain` and `find_global` — linear scans doing a
`strcasecmp` per entry (with a pointer-equality fast path that only fires on
an exact-case match) — and the profiled loop does two such resolutions per
iteration. Variable bindings are dynamically scoped, so they **cannot** be
cached on the atom; M2's capture of the 14 % is partial by construction. The
canonical-lowercase-atom idea (§8) is the recorded follow-on if M3's profile
shows variable lookup hot.

| | Today | At 1.9× | Needed |
|---|---:|---:|---:|
| Turtle Trails `play.frame` | 87.3 ms | ~46 ms | < 40 ms |
| Checkpoint Run `play.frame` | 258.6 ms | ~136 ms | < 40 ms |

So the honest reading is:

- **Turtle Trails gets close but may not clear 40 ms on M1+M2 alone.** M4, or
  P9's C map (below), or a modest amount of game-side work should finish it.
- **Checkpoint Run does not get there on interpreter work alone.** It needs
  P9's tile map and probably game-side change as well; P10 is necessary but
  not sufficient for it.

Nobody should expect one lever to fix a 6.5× shortfall. M0's baseline is what
turns these estimates into decisions.

## 8. Rejected alternatives

| Alternative | Why not |
|---|---|
| Compile bodies to bytecode / a pre-parsed AST | The real fix, and far too large a change for the evidence in hand: it touches the GC, `format.c`'s printing of procedure text, the editor, and Logo's "a program is a list" semantics. Memoisation gets much of the win for a fraction of the risk. Revisit only if M3 falls well short. |
| Cache the classification in the *list cell* rather than the atom | Same word in two places would be classified twice, and cells are 32 bits with no room. The atom is the identity; the cell is just a reference to it. |
| A side memo table keyed by atom offset | Costs new `bss` on a board at 95.6 %, and needs its own invalidation. The atom header is already there and already carries flag bits. Kept as the M2 fallback if the binding fields prove too wide for the header. |
| Sort the procedure table and binary search it | Turns a linear scan into ~7 `strncasecmp` calls, when the point is to do **no** string compare at all. It also does nothing for §3.1, which is the bigger half. |
| Make atom interning case-insensitive so names compare by identity | Changes what `print "Hello` prints. Logo names are case-insensitive but words preserve case; the interner is deliberately case-sensitive. A canonical lowercase key alongside the atom is the M2-compatible version of this idea — and the only lever recorded here that also reaches *variable* lookup (§7): store the canonical atom in each binding and `strcasecmp`-per-entry becomes an identity compare. |
| Raise the clock / overclock the RP2350 | Buys maybe 1.5× against a 2.2–6.5× shortfall, costs power and thermal headroom, and hides the actual defect rather than fixing it. |
| Lower the games to 15 fps and declare victory | A legitimate fallback for Turtle Trails, not a fix: it does not help Checkpoint Run at 4 fps, and every future game inherits the same ceiling. |

## 9. Tests

- **Behaviour is the acceptance criterion.** The existing suite is the
  contract: 66 test binaries, including both games' full simulations. None of
  them may change. A performance change that needs a test edited is a
  semantic change in disguise.
- **New unit tests** for the cache itself: class agreed with `classify_word`
  for every token shape; the `-` cases in both contexts (§4.2); binding
  invalidation across `proc_define`/`proc_erase`/`erall`; atom collection and
  offset reuse not resurrecting a stale entry; numeric caching agreeing with
  today's parse, including the `n`-notation exponent forms.
- **A differential test** is the strongest guard available: for a corpus of
  words, assert the cached answer equals the freshly computed one. That
  catches the whole class of bug this design can introduce.
- **One caution for the differential corpus:** the number grammar exists in
  three copies — the lexer's `is_valid_number`, `token_source.c`'s
  `is_number_word`, and `eval_expr.c`'s `is_number_string` — and reviewing
  this design caught them drifted: `1e` passed `is_number_string` and
  silently evaluated as `1` (B10, fixed 2026-08-01 by adding the
  digit-after-exponent rule to the third copy). The cache must mirror
  `classify_word`/`parse_number` exactly; unifying the three copies is not
  P10's job, but the corpus should include `1e`, `1n`, `1e+` and relatives
  so any future drift is caught, not cached.
- **The benchmark from M0 runs in CI as a regression guard**, on relative
  numbers rather than absolute ones so it does not flap on a loaded machine.

## 10. Relationship to P9

P9's M0 opened this item, and the two now interlock:

- P9's **scrolling half** is blocked on P10 — a game that cannot reach 25 fps
  cannot scroll smoothly at 25 fps.
- P9's **bake half** (`stampmap`/`stamptile`) is not blocked and should
  proceed independently: it replaces `draw.board` (5,916 ms) and
  `draw.sector` (1,346 ms) with a C loop, the two largest stalls measured
  anywhere in this project, and it needs no frame budget at all.
- P9's **C map** (`tile`/`settile`) is complementary to P10 rather than
  redundant: it removes the 36+28 cons-cell walk per `tile.at` from inside
  `step.bugs`, which is 59 % of a Turtle Trails frame.

## References

- [Roadmap P10](roadmap.md#p10--interpreter-throughput) — the item this
  design serves.
- [Tile map design](tilemap-scrolling-design.md) §3.3–§3.5 — the measurement
  that opened this, and the failed gate.
- `core/token_source.c` (`classify_word`, `node_iter_next`),
  `core/memory.c` (atom table, `mem_atom`, `find_atom`, `atom_free_add`),
  `core/primitives.c` (`primitive_find_n`),
  `core/procedures.c` (`find_procedure_index_n`),
  `core/eval_expr.c` / `core/eval_steps.c` (the call-resolution sites),
  `core/variables.c` / `core/frame.c` (variable resolution — the part of
  the 14 % M2 does not reach, §7).
