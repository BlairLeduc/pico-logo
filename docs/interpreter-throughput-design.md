# P10 — Interpreter throughput (design)

Status: **v1 design, drafted 2026-08-01; M0–M3 done 2026-08-01, M4 declined.**
Opened by P9's M0 measurement,
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

**Corrected by M1 (§6.2):** this profile is a *board* profile and does not
transfer. On the host, classification is a few per cent, not 34 %, and
removing it entirely bought 1.5 %. Read this table as describing the Pico,
where a character loop out of flash costs far more than it does here.

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

- **Word class** — the `TokenType` `classify_word` would return. *Done in M1.*
- **Parsed numeric value** — for atoms that are numbers, so `is_number_word`
  and the later `strtof` both disappear. *Dropped: §6.2 measured the whole of
  number parsing at 2.2 %, too little for the variable-width entry it needs.*
- **Resolved binding** — primitive index, user-procedure index, or unbound.
  *Done in M2.*

The memo is **16 bits per atom**, laid out in `core/atom_memo.h`: 5 bits of
class, 2 bits of binding kind, 9 bits of table index. Making it one word
rather than two fields is what kept M2's storage cost to nothing — the entry
went from `ALIGN4(len + 5)` to `ALIGN4(len + 6)`, which only grows the one
length class in four where that crosses an alignment boundary.

`memory.c` owns the storage and knows none of the layout; `mem_word_view`
hands out the characters, the length and a pointer to the memo in a single
walk of the entry, which §6.2 records as the difference between M1 working
and M1 being a wash.

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

**M1 took only the first column**, so the paragraph below is unspent design:
the numeric payload was dropped (§6.2) and the entry stayed fixed-width at
`ALIGN4(len + 5)`. Measured cost after loading each game is in §6.2 — 560 and
740 atom bytes, 0.6–0.8 % of free nodes, `pico2` RAM unchanged at 95.62 %.
It is kept because M2 may still want a variable-width entry.

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
- **M1 — cached word class. Done 2026-08-01.** Class on the atom's memo byte;
  `classify_word` became a lookup with the `-` exception of §4.2, and the
  leading-`;` comment peek in `node_iter_next` folded into the same lookup.
  Parsed numeric value was **not** cached — §6.2 explains why the measurement
  redirected the milestone. Host result: **1.25× on the profiled loop**,
  Turtle Trails' frame −11 %, Checkpoint Run's −12 % (§6.1).
- **M2 — cached name binding. Done 2026-08-01.** Primitive and procedure
  resolved once per atom and kept in the same memo word as the class; the
  inline `output`/`op` compare became `primitive_is_output`, an identity
  check. **No generation counter** — §6.3 explains why a sweep on table
  mutation replaced it. Host result: the workspace-scan spread collapsed from
  **4.97× to 1.00×**, a full-table call 2.03 → 0.32 µs, and both game frames
  gained a further 1.17×.
- **M3 — re-measure and decide. Done 2026-08-01.** Turtle Trails is
  **73.4 ms**, not under 40, so §1 is not met by M1+M2 (§6.4). The decision is
  nevertheless **not M4**: M4 targets the cons-cell walk, which is the same
  cost P9's C map removes outright from `tile.at`, and P9's version is already
  designed and cheaper. P10 stops here on the games; the one live lead is the
  unexplained board regression in §6.4 and the unused 16 KB of XIP_RAM that
  may explain it.
- **M4 — representation. Not being done.** The 20 % in `mem_car`/`mem_cdr`
  indirection and the 8 % in `memmove`/`memset`. M3 declined it: it is the
  invasive option, it touches the GC's assumptions, and it attacks the same
  milliseconds P9's C map removes from the data side for less risk. Revisit
  only if P9 lands and Turtle Trails is still short.

### 6.1 M0 baseline (2026-08-01)

Host is the benchmark's machine of record. Both Pico 2 columns are measured
on hardware by `p10m0` (the game rows reuse P9's `p9m0` numbers).

| Scenario | Host M0 | Host M1 | Host M2 | Pico 2 M0 | Pico 2 M1 | Pico 2 M2 |
|---|---:|---:|---:|---:|---:|---:|
| `repeat [make "x (:x + 1)]` | 1.07 µs/iter | 0.86 µs/iter | **0.80 µs/iter** | 92.4 µs/iter | 65.9 µs/iter | **108.1 µs/iter** |
| user-proc call, small workspace | 0.43 µs | 0.40 µs | **0.32 µs** | - | 32.6 µs | **24.4 µs** |
| user-proc call, 64 defined | 1.24 µs | 1.21 µs | **0.32 µs** | - | - | - |
| user-proc call, full table (target last) | 2.09 µs | 2.03 µs | **0.32 µs** | 127.6 µs | 128.3 µs | **24.0 µs** |
| workspace-scan spread (full : small) | 4.86× | 4.97× | **1.00×** | - | 3.94× | **0.98×** |
| Turtle Trails `play.frame` | 0.84 ms | 0.75 ms | **0.64 ms** | 87.3 ms (§2.1) | 73.2 ms | **73.4 ms** |
| Turtle Trails `draw.board` | 55.1 ms (§2.1) | - | - | 5,916 ms (§2.1) | 5,054 ms | **5,213 ms** |
| Checkpoint Run `play.frame` | 2.73 ms | 2.40 ms | **2.06 ms** | 258.6 ms (§2.1) | 232.7 ms | **232.6 ms** |
| Checkpoint Run `draw.sector` | 18.0 ms (§2.1) | - | - | 1,346 ms (§2.1) | 1,173 ms | **1,241 ms** |

The frame rows sit slightly under §2.1's 0.96 / 3.42 ms because the method
differs — M0 times a bare `repeat [play.frame]` in manual refresh, where the
p9m0 harness collected per-frame min/max around each call. Before/afters
must compare against *this* table, taken by *this* benchmark.

Two readings matter. **The loop is 1.40× faster on the board against 1.25× on
the host** — the board gains more, which is the direction §2.2 predicts. And
the full-table procedure call is **unchanged**, 127.6 → 128.3 µs, 0.5 % apart:
M1 does not touch name resolution, so this row is a control rather than a
result, and reproducing M0's number to within noise is good evidence that both
the baseline and the method are sound. The small-workspace row has no M0
baseline, so 32.6 µs is a first measurement, not a comparison; taken against
the full table it puts the board's workspace-scan spread at **3.9×**, close
enough to the host's 5.1× to confirm M2 has the same problem to solve on both.

**Both games gained, and both gained less than the synthetic loop:**

| | Loop | Trails frame | Trails `draw.board` | Checkrun frame | Checkrun `draw.sector` |
|---|---:|---:|---:|---:|---:|
| Gain | 1.40× | 1.19× | 1.17× | 1.11× | 1.15× |

Real code spends a share of itself on work M1 does not touch — `tile.at`'s
cons-cell walks above all — so a synthetic loop of nothing but tokens is the
optimistic end, not the representative one. Trails' 0.85 ratio of frame gain
to loop gain is close to the 0.90 the host predicted, so the host is a fair
guide to the *shape* of a change even where it is wrong about the absolute
shares. Checkpoint Run gains least of the five, which fits: it is the game
whose frame is most dominated by things other than tokenising.

### 6.2 What M1 measured (2026-08-01)

M1 was scoped by §2.2's claim that 34 % of runtime is re-lexing and
classifying words. **On the board that claim held.** Removing classification
took the profiled loop from 92.4 to 65.9 µs/iter — 28.7 % of runtime gone,
against a ceiling of 34 % if the work vanished for free, the gap being what
the memo lookup costs to read. §2.2 is confirmed, and M1 delivered on the
hardware what the design promised.

The host is the machine that disagrees, and understanding why is what saved
the milestone. The first cut of M1 made classification a perfect memo —
instrumented at 3,079,993 cache hits against 7 misses over the profiled loop —
and the host loop got **1.5 % faster**, which is noise. Two findings follow.

**The 34 % is a board figure and does not transfer.** A leaf profile of the
post-M1 build (macOS ARM64, `sample`, 4,942 leaf samples) puts the costs at:

| Cost centre | Share |
|---|---:|
| Cons-cell walk and index→pointer indirection | 16.6 % |
| The memo lookup itself, as first written | 12.7 % |
| `memmove`/`memset` | 9.8 % |
| Name resolution (`primitive_find_n`, `strncasecmp`, variable lookup) | 7.6 % |
| Re-interning the quoted word `"x` on every evaluation | 5.9 % |
| `is_delimiter_token` | 4.2 % |
| `mem_word_ptr` / `mem_word_len` | 4.0 % |
| Number parsing (`parse_number`, `strtof`, `is_number_string`) | 2.2 % |
| Everything else (`eval_primary`, `eval_expr_bp`, the trampoline, …) | 37.0 % |

Classification was never a third of *host* runtime. On the Pico, code runs
from flash through the XIP cache and a character loop costs far more relative
to everything else — 1.40× against the host's 1.25× is that difference showing
up. **The standing lesson is that host shares cannot be used to size a board
milestone, in either direction**: this design's central number was right about
the hardware and would have been abandoned on host evidence alone.

**The memo has to be cheaper than the thing it replaces.** The first cut
cost 12.7 % on the host — about what classification cost there — because
`mem_atom_memo` was a separate out-of-line call that re-walked the entry (type
check, blob check, bounds check, free check, length read) on top of the walks
`mem_word_ptr` and `mem_word_len` were already doing for the same element.
Collapsing all three into one `mem_word_view` call is what turned M1 from
1.5 % into 1.25× on the host, and it is very likely part of why the board
reached 1.40 %. The lesson generalises to M2: a lookup that reaches into
`memory.c` per token must be *one* call, or the memo pays for itself and no
more. That the board-visible win survived a host-visible one is the reason
this was worth chasing on the machine that could be profiled.

**Numeric caching was dropped from M1**: number parsing is 2.2 % of host
runtime, and capturing it needs a variable-width atom entry, a new header flag
bit with two GC invariants (§5), and a wider `Token`. Given the host/board
divergence above, 2.2 % is weak evidence either way — but the cost is
concrete and the benefit is not, so it stays dropped until a board profile
argues for it.

**Two further targets surfaced**, neither in the original plan and both
bigger on the host than what M1 caught there: `eval_primary` re-interns `"x`
on every evaluation of a quoted word (5.9 % — hash, chain walk and compare,
for an answer that is a pure function of the atom, exactly the §4 argument),
and `is_delimiter_token` re-derives from the token type per token (4.2 %) when
it could fall out of the cached class. Both are M2-shaped and both are pure
memoisation, the pattern the board has now shown pays better there than here.

**Cost, measured** (§5's acceptance check). All three firmware presets link;
`pico2` RAM is unchanged at 95.62 %, confirming the memo costs no new `bss`.
Inside the shared block, after loading each game:

| | Free nodes before | after | Atom bytes spent |
|---|---:|---:|---:|
| Turtle Trails | 23,946 | 23,806 (−0.6 %) | 560 |
| Checkpoint Run | 22,567 | 22,382 (−0.8 %) | 740 |

That is ~+1 byte per atom as §5 predicted, and well inside "does not fall
materially".

### 6.3 What M2 measured (2026-08-01)

M2 hit its target on the host and, unusually for this item, cost negative
memory. The headline is the row the design predicted it would flatten:

| | M1 | M2 |
|---|---:|---:|
| user-proc call, small workspace | 0.40 µs | 0.32 µs |
| user-proc call, full table | 2.03 µs | 0.32 µs |
| workspace-scan spread | 4.97× | **1.00×** |

**A call no longer cares how large the workspace is.** That was §3.2's whole
complaint, and it is gone: the binary search over ~390 primitives and the
linear scan of the procedure table are both replaced by one read of the memo.
Both game frames gained a further 1.17× on top of M1 (Trails 0.75 → 0.64 ms,
Checkpoint Run 2.40 → 2.06 ms).

Three departures from the design as written, all forced by measurement:

**No generation counter.** §4.3 proposed a byte per atom recording the
generation a binding was resolved under. Instead every mutator in
`procedures.c` sweeps the atom region and drops all bindings
(`mem_atom_memo_mask_all`). This is simpler, removes a field, and is covered
by construction rather than by enumerating callers — and the cost lands where
§4.3 said it would, at definition time rather than in a frame loop. It also
made room for the whole binding to fit beside the class in 16 bits.
`copydef` sweeps too, since `primitive_register_alias` can turn a name that
already resolved to "neither" into a primitive mid-evaluation.

**`Token` had to shrink to grow.** §4.4 called for a `Node` field on `Token`
so the resolution sites can reach the atom, and noted only that a `Token`
carries no atom identity. What it did not say is that a `Token` is embedded
twice in every `TokenSource`, which is embedded in every `EvalOp`, in a
768-deep static op stack — so four bytes on `Token` cost **6,144 bytes of
`bss`** on a board at 95.6 %. Narrowing `type` and `length` to fit the atom
for free was measured and **rejected**: it costs 16 % on the profiled loop and
8 % on a game frame, giving back most of what M2 buys, and leaves the loop
*slower than M1*. What paid instead was deleting `NodeIterator`'s
`has_peeked`/`peeked_token`, a one-token lookahead buffer that nothing ever
set to true — dead state larger than the atom that replaced it. Net result:
`pico2` RAM **95.62 % → 93.86 %**, 9,208 bytes returned, and free nodes after
loading each game down only 194 (Trails) and 258 (Checkpoint Run).

**The lesson from §6.2 held twice.** M1's finding was that a memo must be
cheaper than what it replaces, and that a lookup reaching into `memory.c` must
be *one* call. M2 obeyed both from the start — `resolve_word` makes a single
`mem_word_view` call that yields characters, error-message name and memo
together — and needed no second attempt.

### 6.4 M2 on hardware: goal met, games unmoved (2026-08-01)

M2 did on the board exactly what it was designed to do, and it did not help
the games at all.

| | Pico 2 M1 | Pico 2 M2 | |
|---|---:|---:|---|
| user-proc call, full table | 128.3 µs | **24.0 µs** | 5.35× |
| user-proc call, small workspace | 32.6 µs | **24.4 µs** | 1.34× |
| workspace-scan spread | 3.94× | **0.98×** | flattened |
| `repeat` loop | 65.9 µs/iter | **108.1 µs/iter** | **0.61× — a regression** |
| Turtle Trails `play.frame` | 73.2 ms | 73.4 ms | unchanged |
| Checkpoint Run `play.frame` | 232.7 ms | 232.6 ms | unchanged |

**The win is real and is the one §3.2 asked for.** A call costs the same
whether one procedure is defined or the table is full — 128.3 µs collapses to
24.0. That removes a scalability cliff every growing Logo program was walking
towards, and it is a bigger board effect than the host's already-large one.

**The loss is real too, and unexplained.** The profiled loop went 1.64×
*slower*, and the two games came out flat while the host predicted 1.17× for
both. `draw.board` and `draw.sector` each slipped 3–6 %.

What the shape of it says: on the loop the host:board ratio went from 77× at
M1 to 135× at M2, a **1.76× board-specific penalty with no host counterpart**.
M2 did not add work to that loop — the loop resolves exactly one name per
iteration (`make`, a primitive), and it replaced a nine-step binary search
plus a re-intern with one memo read. Nothing in the algorithm accounts for
42 µs an iteration. A cost that appears only on the board, only after a code
change, and not in the algorithm points at instruction fetch: the RP2350 runs
from external flash through the XIP cache, and M2 moved and grew the hot path.

**This is a hypothesis, not a finding** — it has not been measured, and
measuring it needs a board. One concrete lead is on the record: `pico2`
reports **XIP_RAM 0 B of 16 KB, 0 % used**. The Pico SDK can place chosen
functions in that RAM (`__not_in_flash_func`), and the interpreter's token
and resolution path is the obvious candidate. If instruction fetch is the
cause, that would recover this regression and likely more besides — it is the
first genuinely new lever this item has turned up since M0.

**Should M2 stay?** On the evidence: yes, but the call is finely balanced and
belongs to whoever owns the roadmap.

- *For:* the workspace-scan cliff is gone, which matters for every program
  that grows past a handful of procedures; it returns 9 KB of SRAM; and the
  two shipped games are neutral, not worse.
- *Against:* token-heavy code that calls few procedures is 1.64× slower on
  the board, and the games — the thing §1 steers by — gained nothing.

Reverting is defensible if the loop regression cannot be explained. Keeping it
and chasing the XIP_RAM lead is the better bet, because the lead applies to
M1's gains as well.

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

**Measured after M1, on hardware.** The estimates above can now be replaced
with a board number for the first half:

| | M0 | After M1 (board) | Still needed |
|---|---:|---:|---:|
| `repeat` loop | 92.4 µs/iter | 65.9 µs/iter (1.40×) | — |
| Turtle Trails `play.frame` | 87.3 ms | **73.2 ms** (1.19×) | 1.83× (33.2 ms) |
| Checkpoint Run `play.frame` | 258.6 ms | **232.7 ms** (1.11×) | 5.82× (192.7 ms) |

M2 has now been run on hardware too (§6.4), and it moved neither game:

| | M0 | After M1 | After M2 | Still needed |
|---|---:|---:|---:|---:|
| Turtle Trails `play.frame` | 87.3 ms | 73.2 ms | **73.4 ms** | 1.84× (33.4 ms) |
| Checkpoint Run `play.frame` | 258.6 ms | 232.7 ms | **232.6 ms** | 5.81× (192.6 ms) |

So the honest end state is that **P10 delivered 1.19× for Turtle Trails and
1.11× for Checkpoint Run, all of it in M1**, and the target in §1 is not met.
M2 bought a large structural win (a call no longer scales with workspace size)
that neither shipped game was positioned to collect, and its board-side
regression on the profiled loop (§6.4) is unexplained.

§2.1's original reading — that Checkpoint Run needs P9 and game-side work as
well, and that P10 is necessary but not sufficient — is now measured rather
than predicted, and it was right for both games, not just the harder one.

**What closes the gap is P9, not P10.** P9's M0 attributes 59 % of a Trails
frame to `tile.at` inside `step.bugs` — 43.2 ms of the 73.2 — and P9's C map
(`tile`/`settile`) deletes exactly that walk. A frame without it is around
30 ms, under budget with room to spare. So the honest ordering is that P10 M2
is worth having and will not be decisive, while P9's map is decisive; the two
are complementary as §10 says, but the weight sits with P9.

This should inform M3's decision, and it argues against M4: representation
work (the cons-cell walk, §3.3) attacks the same milliseconds P9's C map
removes outright, and the C map is both cheaper and already designed.

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
- **M1's tests are in `tests/test_token_source.c`** (6 added, all passing
  alongside the untouched 67-binary suite): a 29-shape corpus classified cold
  and then warm and asserted identical, the `-` / `-foo` atoms classified in
  both contexts within one run, `-5` asserted context-*free*, comment skipping
  over two passes of the same list, and an atom-reuse test that asserts the
  collected offset really was handed back before checking the class did not
  come with it. That last one was mutation-checked: deleting the memo-clearing
  line in `mem_atom` makes it fail.
- **M2's tests are in `tests/test_primitives_procedures.c`** (5 added): every
  one runs the call from inside a list, because a call typed at the REPL is
  lexed from raw text and has no atom, so it is the one path the cache does
  not serve. They cover a slot freed by `erase` and refilled by the next
  definition (the hazard the sweep exists for), a name defined after it
  already resolved to "neither", `erall`, an alias created by `copydef`
  mid-run, the plain repeated call, and redefinition through both the lexer
  and the cached list path.
- **One harness trap, recorded because it cost an hour and a retracted bug
  report.** `output_buffer` in `tests/test_scaffold.c` is written at a
  separate cursor, `output_pos`. Clearing it with `output_buffer[0] = 0`
  leaves that cursor where it was, so a *second* `print` in the same test
  lands past the visible string and every later assertion reads an empty
  buffer — which looks exactly like the interpreter having stopped working.
  Use `reset_output()`. A first draft of the redefinition test did this and
  produced a convincing false positive: a filed bug (B11) claiming
  redefinition left a procedure defined but empty. Redefinition is fine.
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
