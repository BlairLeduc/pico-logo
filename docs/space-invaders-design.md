# Space Invaders in Pico Logo (design & implementation)

Status: **implemented.** See [`examples/invaders.logo`](../examples/invaders.logo)
(`load "invaders.logo` then `invaders`). A worked example that exercises the
P5 multi-sprite feature set ([design](multi-sprite-design.md)). The goal was a
playable, recognisably-faithful Space Invaders written entirely in Logo — no
interpreter changes for the *game itself* — that doubles as an acceptance test
for `tell`/`each`, `setspeed`, `setanim`, `touching?`, `over?`, `when`,
`stamp` and the refresh primitives working together. (Two small interpreter
fixes did fall out of building it — `ask` as an operation, and a `toot`
argument-coercion bug — see [Implementation notes](#implementation-notes).)

The design's one real constraint is `MAX_TURTLES 8` (`core/limits.h`).
Classic Invaders has 55 aliens, 4 shields, a cannon, a shot, up to three
bombs and a UFO — far more than eight objects. The whole design is
organised around **which objects must be turtles and which can live on the
canvas**, and that split turns out to match how the 1978 hardware actually
worked.

A second constraint turned out to matter just as much in practice: this
Logo has a fixed-size, garbage-collected-only-on-request cons-cell pool
(no automatic GC — see [Memory model](#memory-model)). The original board
layout has to account for both constraints, not just `MAX_TURTLES`.

---

## 1. The board

- Screen is 320×320 turtle steps, origin at centre: x ∈ [−159, 160],
  y ∈ [−159, 160] (reference §*setpos*).
- We run in **`splitscreen`**: the top 24 text-lines of graphics stay
  visible (240 units tall) and the bottom text lines carry the HUD
  (score, lives, level). There is no draw-text-on-canvas primitive, so
  the HUD is ordinary `setcursor` + `type`/`pr` on the text screen.
- Visible play field is therefore roughly y ∈ [−80, 160]. Cannon sits at
  y = −70, the alien formation starts at y ∈ [60, 140], bombs fall
  down-screen.

## 2. Object representation — the central decision

| Game object | Count | Representation | Why |
|---|---|---|---|
| Player cannon | 1 | **turtle 0** | needs `touching?` with bombs; player-driven motion |
| Player shot | 1 | **turtle 1** | one shot on screen (classic rule); autonomous `setspeed` up; `touching?` UFO; `over?` aliens/shields |
| UFO / mystery ship | 1 | **turtle 2** | autonomous `setspeed` across top; `touching?` shot; `setanim` shimmer |
| Alien bombs | ≤3 | **turtles 3–5** | autonomous `setspeed` down; `touching?` player; `over?` shields |
| Alien-formation stamper | — | **turtle 6**, always hidden (`ht`) | never shown; only `setx`/`sety`+`stamp`s the alien costume (or a solid eraser costume) onto the canvas. Also used to draw/fill the shields at level setup. |
| — spare — | | turtle 7 | unused headroom |
| Alien formation | 55 | **canvas** (stamped) | can't be 55 turtles; sensed with `over?`; individually erasable |
| Shields / bunkers | 4 | **canvas** (drawn + filled) | static, destructible pixel terrain; sensed + eroded via canvas |

**Turtles are the moving projectiles and special ships; the canvas holds
the formation and the destructible terrain.** This is exactly the P5
compositor model (§2.4 of the engine design): sprites are state composited
at blit time, the canvas is drawn pixels that `over?`/`colourunder` sense
and `stamp` writes into. It is also faithful to the arcade original, where
the shields were RAM bitmaps eroded pixel-by-pixel and the aliens were a
block the game redrew as it marched.

Turtle 6 is never `st`'d (shown) — `stamp` composites a turtle's costume
into the canvas regardless of that turtle's own visibility, so a
permanently-hidden turtle makes a perfectly good "stamping tool" without
ever appearing as a sprite of its own.

## 3. The alien formation on the canvas

### Layout

```logo
make "alien.rows 5
make "alien.cols 11
make "colgap 20      ; column spacing
make "rowgap 20      ; row spacing
make "formx -100     ; x of column 1's centre
make "formy 140      ; y of row 1's centre
make "formdir 1      ; +1 marching right, -1 left
```

- `alien.row`/`alien.col` map a 1-based cell index (row-major, 5×11) to
  its row/column; `alien.cell.x`/`alien.cell.y` map row/column to screen
  position.
- **Bitmap costumes render at 16×16 pixels, not 8×16.** `putsh`'s
  data format is 8 bits per row × 16 rows, but the device stores (and
  renders) each row pixel-doubled horizontally — see reference `setmag`:
  "shapes larger than 16 by 16 always appear at normal size," i.e. 16×16
  is the *native* render size, not just a magnification ceiling. `colgap`/
  `rowgap` of 20 were chosen deliberately larger than 16 so adjacent
  aliens have a visible 4px gap instead of touching or overlapping. An
  earlier draft used 14/16, which visually looked like one continuous
  block of aliens with no gaps at all — a good example of why the
  *rendered* sprite size, not the `putsh` data format, is what board
  layout has to be sized against.
- **Marching** is discrete, driven by a frame counter rather than
  wall-clock time (`march.if.due`/`march.interval`, counted in frames of
  the main loop's fixed `sync` cadence), a few times per second — the classic
  "step, step, step" cadence, not smooth glide. Each step
  (`march.step`):
  1. erase every currently-alive cell at its old position
     (`erase.all.aliens`);
  2. advance `formx` by a column-fraction; if the block's live edge would
     cross the wall, instead drop `formy` and flip `formdir`
     (`step.formx`/`drop.and.flip` — the signature zig-zag descent);
  3. redraw every living cell at its new position (`draw.aliens`);
  4. toggle the alien costume frame (`toggle.alien.frame`) and play one
     march note (`march.note`).
- **Speed-up curve** falls out for free: `march.interval` scales the
  march delay with the live-alien count (`1 + int (:alive / 4)`), so as
  the player clears the board the redraw shortens and the block
  accelerates — the original's rising panic, reproduced by construction,
  not scripted.

Killing an alien immediately blanks its cell so it doesn't linger until the
next march: wear a solid mono block costume (`putsh 3 [255 255 …]`,
16×16 fully "on"), set the pen to the background slot, `stamp`
(`erase.alien.cell`). One primitive, one cell erased — and because the
eraser costume and the alien costume are both plain (non-rotated,
non-magnified) mono bitmaps, they anchor identically, so the eraser's
solid 16×16 footprint is a strict superset of whatever the alien costume
touched at the same position.

### Alive/dead tracking: a flat list, mutated in place

The design originally tracked the 55 alive/dead flags in a single Logo
list rebuilt with 55 `lput` calls whenever an alien died (an
`aliens.without` helper). **This does not work on real hardware
memory.** This Logo's cons-cell pool has no automatic garbage collector
(see [Memory model](#memory-model)), and `lput` copies its *entire* input
list before appending — rebuilding a *k*-element list costs *k+1* new
cells, so building a 55-element list via 55 sequential `lput` calls costs
`sum(1..55) = 1540` cons cells per kill, of which only the final 55 are
actually kept; the other ~1485 are instant garbage, on top of the old
55-cell list itself. Against a real pool of a few tens of thousands of
cells, that's "out of space" after well under one full level's worth of
kills.

The fix: keep the flags in a single flat Logo list built **once** per
level, then mutate the dying cell **in place** with the destructive
setter `.setitem` (added in this branch alongside `.setfirst`/`.setbf`).
`.setitem` walks to the *idx*-th cons cell in C and overwrites its value —
no list is rebuilt, so a kill allocates **zero** cons cells:

```logo
to make.ones :n
  ; A flat list of :n ones, allocated once; thereafter mutated in place.
  local "lst
  make "lst []
  repeat :n [make "lst fput 1 :lst]
  output :lst
end

to set.alien :idx :val
  ; Flip one liveness flag in place. Because :aliens is shared, this call
  ; mutates it directly -- no rebuild, no garbage.
  .setitem :idx :aliens :val
end
```

Reads that walk the whole formation (drawing, the initial layout) advance
through the list with `first`/`butfirst`, which share structure and
allocate nothing; the few random-access reads use `item`. `setup.aliens`
pays the one-time `make.ones` cost at level start; every subsequent update
(every kill, every level reset) is free.

## 4. Collision routing — demons vs. the game loop

Two collision *kinds*, routed to the two mechanisms that fit each. The
split hinges on one subtlety worth stating up front:

> `over?` and `colourunder` answer for the **lowest active turtle** — they
> take no turtle argument. `touching?` and `distance` take **explicit
> turtle numbers**.

So:

- **Turtle-vs-turtle → `when` demons.** They name both turtles explicitly,
  so they are robust regardless of the current `tell` set, and they fire
  edge-triggered (once on contact). These run autonomously alongside
  `setspeed`, so bombs and shots collide "for free" (`arm.demons`):

  ```logo
  when [touching? 1 2] [ufo.hit]        ; shot hits UFO
  when [touching? 0 3] [player.hit]     ; bomb 3 hits cannon
  when [touching? 0 4] [player.hit]
  when [touching? 0 5] [player.hit]
  ```

- **Turtle-vs-canvas → explicit checks in the game loop.** Because `over?`
  reads the *active* turtle, we address the projectile explicitly with
  `ask`, which is deterministic and immune to whatever `tell` the rest of
  the frame did (`check.shot`/`check.bombs`):

  ```logo
  ask 1 [if over? :alien.colour [hit.alien]]
  ask 1 [if over? :shield.colour [erode xcor ycor  ht setspeed 0]]
  ```

  `hit.alien` reads the shot's `xcor`/`ycor`, and `locate.alien` maps them
  through `formx`/`formy` to the nearest grid cell within tolerance;
  `kill.alien` marks it dead, blanks the cell, scores, and recycles the
  shot.

  **Hit tolerance is wider than naive "half the grid spacing" math.**
  Bitmap costumes anchor at their *bottom row*, not their centre (see
  [Implementation notes](#implementation-notes)), so a shot's actual
  sensed pixels and an alien's actual sensed pixels are each offset from
  their own nominal grid-formula coordinate — `over?` can go true for a shot
  position several pixels away from the alien's grid-formula position.
  `locate.alien`'s tolerance is `< 10` on both axes (comfortably covering
  that offset, still safely under half of the 20-unit `colgap`/`rowgap`
  so it can never match a neighbouring cell) rather than the tighter `< 7`
  / `< 8` an on-paper grid-spacing argument would suggest; the tighter
  values caused real, intermittent "the shot passed right through it"
  misses.

`erode` paints a small background chunk at the projectile's position —
pixel-ish shield erosion, faithful and cheap.

Rationale for not forcing everything into demons: a demon condition like
`when [over? :alien.colour] […]` would silently depend on whoever the
current active turtle is, and the main loop retargets turtles constantly.
Keeping canvas sensing in `ask … [over? …]` makes the reader's turtle
unambiguous. Demons carry the events that are genuinely global and
edge-shaped.

## 5. Global events as demons

```logo
when [(alien.bottom.y) < -55] [invasion]   ; block reached the cannon line
```

`freeze`/`thaw` pause the whole living world (motion, animation, demons) in
one word — the natural pause key (`toggle.pause`, bound to `p`). At the top of
every level `setup.level` calls `cleardemons` then `cs`: `cleardemons` disarms
every demon so none survives from the previous level, and `cs` clears the
screen and stops all autonomous motion (bombs, shots) — `arm.demons` then
re-registers all five fresh, every level. (`cs` no longer clears demons on its
own — revisited 2026-07-18; demon teardown is `cleardemons`' job, see
[multi-sprite §7](multi-sprite-design.md).)

## 6. Input

Arrow keys and space arrive from `readchar` as single bytes; compare with
`ascii` (from `devices/picocalc/keyboard.h`):

| Key | Code | `ascii rc` |
|---|---|---|
| ← | `KEY_LEFT` 0xB4 | 180 |
| → | `KEY_RIGHT` 0xB7 | 183 |
| space (fire) | `KEY_SPACE` 0x20 | 32 |
| p (pause) | | 112 |

```logo
to poll.input
  if key? [
    make "k ascii readchar
    if :k = 180 [ask 0 [move.cannon -6]]
    if :k = 183 [ask 0 [move.cannon 6]]
    if :k = 32 [fire]
    if :k = 112 [toggle.pause]
  ]
end
```

`move.cannon` clamps the cannon to the play-field edges. `fire` only
launches when the shot is idle (`shot.active?` checks `shown?`): position
it at the cannon, `st`, `setspeed` up — one shot at a time, as in the
original. The cannon itself has no `setspeed`; it moves only when a key
says so.

## 7. Main loop and game states

`setrefresh "sync` + one `sync` per frame gives a tear-free, batched present
(engine design §2.3) *and* an even cadence: all the frame's motion, stamps and
erosions appear at once, and `sync` then waits to the next 30 fps frame
boundary. This replaced an earlier `setrefresh "manual` + `refresh` + `wait 33`
loop, whose period was *work + 33 ms* — so a heavy frame (a march step redraws
the whole block) lengthened the frame and made motion jerk. Pacing to a fixed
boundary instead of sleeping a fixed amount keeps the rate steady regardless of
per-frame work, the way a game locked to the TV's vertical blank did.

```logo
to play.level
  setup.level
  (setrefresh "sync 30)
  make "over false
  until [:over] [
    poll.input
    if not :paused [
      march.if.due
      check.shot
      check.bombs
      maybe.drop.bomb
      maybe.launch.ufo
      recycle.offscreen
      recycle.ufo
    ]
    draw.hud
    sync
    if :dying [handle.death]
    if :alive = 0 [make "over true]
  ]
  setrefresh "auto
end
```

Bombs, shot and UFO advance on their own via `setspeed` between iterations
(the evaluator's motion tick), and the `when` demons fire whenever a
turtle-pair touches — so the loop body is just input, the march timer, the
canvas-sense checks, and spawning. The whole per-frame simulation update
(everything except `poll.input` itself) is skipped while `:paused` is true,
so `p` genuinely freezes the game rather than just the autonomous motion.

`:over` becomes true either because the board is cleared (`:alive = 0`) or
because `:dying` resolved with no lives left (`handle.death`); a mid-level
death with lives remaining does **not** set `:over` — the same level
continues with the cannon respawned, matching the arcade's "lose a life,
keep playing" behaviour rather than resetting the board.

Game states — **attract** (title on the text screen) → **play** (loop
above, can end in a life lost-but-continuing, a level clear, or game over)
→ **game over** (final score, back to attract) — are driven by a flat
`forever [one.game]` loop at the top level (`invaders`), not a recursive
call chain:

```logo
to one.game
  attract.screen
  make "score 0
  make "lives 3
  make "level 1
  make "playing true
  until [not :playing] [
    play.level
    ifelse :lives < 1 [make "playing false] [make "level :level + 1]
  ]
  show.game.over
end
```

This is a deliberate deviation from a more natural-sounding recursive
state machine (attract → play → death → next level → game over → back to
attract, each calling the next): a long play session could run through
dozens of levels and replays, and this interpreter's fixed-size frame
stack (`core/limits.h`) has no tail-call elimination — a recursive cycle
would grow one frame per level/replay for the life of the session. The
iterative `forever`/`until` form does the same thing with flat, bounded
stack depth regardless of session length.

## 8. Costumes and sound

- **Costumes** via `putsh` bitmaps (the 16-number, 8-bit-per-row mono
  format, rendered at 16×16 — see §3): alien A/B (two `setanim`-free
  frames, toggled manually in step with the march so the whole block
  waddles in lockstep), the solid eraser block, cannon, UFO A/B (driven by
  real `setanim` since the UFO is a genuinely shown, gliding turtle),
  bomb, and shot. `setrot "fixed` for all of them (nothing here rotates).
  Colour costumes (`snapsh`) are unnecessary; mono shapes painted in a
  pen colour are truer to the monochrome arcade look and cheaper.
- **Sound** via `toot duration frequency`: the four-note descending march
  loop (one note per march step via `march.note`, which the discrete
  cadence already spaces out), the UFO's warble, and short explosion
  toots on death/UFO-hit. `toot`'s frequency/duration arguments accept
  numeric *words* (e.g. `item n list`, which does not hand back a plain
  `VALUE_NUMBER`) as well as literal numbers — this needed an interpreter
  fix; see [Implementation notes](#implementation-notes).

## 9. What v1 simplifies (and why it's still Invaders)

| Faithful | Simplified in v1 | Note |
|---|---|---|
| 5×11 block, zig-zag descent, speed-up | fixed 55 start each level | classic |
| One player shot at a time | — | classic rule |
| Destructible shields | coarser erosion chunks | pixel-exact erosion is a later polish |
| ≤3 alien bombs | one bomb type, random column drops | targeted "aimed" bombs later |
| Mystery UFO with bonus | fixed bonus, timed appearance | score-position bonus later |
| Lives, levels, score | text-screen HUD | no on-canvas digits primitive |
| Continuous cannon movement | key-repeat only | `readchar` gives presses, not held-key state; a held-key device query would smooth this but is out of scope |

None of these touch the interpreter; they're all Logo-level polish.

## 10. Why this is a good P5 acceptance test

It uses, in one program, every headline P5 primitive against the mock and
on hardware:

- `tell` / `ask` / `each` / `who` — addressing the projectile turtles.
- `setspeed` / `speed` — autonomous shot, bombs, UFO.
- `setanim` — UFO shimmer (manual frame-toggling for the stamped aliens).
- `touching?` — every sprite-sprite hit.
- `over?` / `colourunder` — shot-vs-formation and shield erosion.
- `when` / `freeze` / `thaw` — collision demons and pause.
- `stamp` — building and erasing the formation and shields.
- `setrefresh` / `refresh` — the tear-free game loop.
- `.setitem` — mutating a liveness flag in a flat list in place, which a
  rebuild-only Logo list can't provide affordably.
- `setx` / `sety` in preference to `setpos` — avoiding a per-call list
  allocation for every turtle move, the program's dominant memory cost
  until it was removed.
- `recycle` — reclaiming the small residual garbage from list *literals*
  (re-built every time their line of source runs) that remains even
  after `setpos` is gone.

If Invaders plays correctly, the milestone works end-to-end.

## 11. Deliverable

Shipped as a single loadable program, [`examples/invaders.logo`](../examples/invaders.logo)
(`load "invaders.logo` then `invaders`). Named without a hyphen or slash
deliberately — see [Implementation notes](#implementation-notes).

---

## Implementation notes

Things discovered while building this that aren't specific to Space
Invaders, but that anyone extending this file (or writing another
canvas-heavy Logo program) needs to know.

### Memory model

This Logo has a **fixed-size cons-cell pool with no automatic garbage
collection** (mark-and-sweep GC exists — the `recycle` primitive — but
it only runs when a program calls it). Reassigning a variable with `make`
does not free the old value's cells; they simply become unreachable
garbage that only `recycle` can reclaim. Two consequences that shaped
this program:

- **Prefer mutable-in-place structures over rebuild-only ones for
  per-frame or per-cell state.** Rebuilding a Logo list with `lput`/`fput`
  copies it; the destructive setters (`.setitem` / `.setfirst` / `.setbf`)
  overwrite a cons cell in place instead. See §3's `aliens.without` →
  `.setitem` rewrite for the concrete cost difference (1540 cons cells per
  kill → ~0).
- **Prefer `setx`/`sety` over `setpos` for turtles that only ever move
  along the grid/one axis at a time.** `setpos` takes a 2-element list —
  a fresh `(list x y)` allocates cons cells on *every* call, and every
  turtle-positioning call in this program (stamping/erasing a cell,
  launching a bomb/shot/UFO, drawing a shield's axis-aligned edges) only
  ever needs to set x and y independently anyway. Switching every
  `setpos (list x y)` in this file to `setx x sety y` removed what had
  been the program's dominant source of transient garbage (up to 110
  list allocations per march step, one erase-and-redraw pass over the
  whole formation) at no behavioural cost, since none of these moves
  need `setpos`'s single-call atomicity or pen-down diagonal-line
  behaviour (pen is always up during them, except `draw.shield`'s
  rectangle edges, which are already axis-aligned one-coordinate-at-a-time
  moves — identical either way).
- **`recycle` is still worth calling, just for a much smaller residual
  source.** Procedure bodies are re-lexed from source text on every call
  in this interpreter, so a bracketed list *literal* (e.g. `march.note`'s
  four-entry frequency table, or a HUD label like `[SCORE:]`) allocates
  fresh cons cells every time that line executes, even though it looks
  like a constant. That's a few cells per march step and a few per frame
  — orders of magnitude less than the eliminated `setpos` traffic, but
  still non-zero over a long session, so `march.step` still calls
  `recycle` once per march.

### `ask` as an operation

`ask turtle [commandlist]` was command-only in this interpreter — usable
for its side effects, but its list's result (if any) couldn't be used as
a value. `shot.active?`, `idle.bomb`, `fire`, and `maybe.launch.ufo` all
need to read one turtle's state (`shown?`, `xcor`, `ycor`) without
retargeting the ambient `tell`, e.g. `ask 1 [shown?]`. This required a
one-line interpreter fix (`core/primitives_turtle.c`, `prim_ask`: call
`eval_run_list_expr` instead of `eval_run_list`, the same mechanism `run`
already used for its own command-or-operation duality) — `ask` is now
documented in the reference as `command or operation`, matching `run`.

### `toot` argument coercion

`toot duration frequency` rejected a numeric value read back from a list
via `item` (e.g. `toot 40 item n freqs`, used by `march.note`'s note
table), even though the value printed and did arithmetic just fine. Root
cause: `prim_toot` checked `args[i].type == VALUE_NUMBER` directly instead
of using this codebase's standard `REQUIRE_NUMBER` macro (which coerces
via `value_to_number`) — `item` on a list hands back a numeric *word*, not
a `VALUE_NUMBER`, same as every other numeric primitive already tolerates.
Fixed in `core/primitives_hardware.c`.

### Bitmap costume anchor point

A `putsh` bitmap costume's on-screen anchor is its **bottom row**, not
its centre — `cur.y` is where row 15 (the last of the 16-row data format)
lands; the rest of the sprite extends from there. Combined with the
horizontal pixel-doubling (§3), this means a costume's actual rendered
footprint is offset from the coordinate you positioned it at, in a
direction and amount that depends on how the bitmap's "on" content is
distributed across its 16 rows. This is invisible until something needs
to reason about *where the pixels actually are* relative to where you
positioned the turtle — which is exactly what shot/alien hit-testing
does. See §4's hit-tolerance note.

### `fill` requires the turtle to not already be standing on the fill colour

`fill`'s flood-fill implementation bails out immediately if the pixel
under the turtle already equals the fill colour — a leak-prevention check
for the "already filled" case. The classic "draw a closed shape then
`fill` from wherever you ended up" idiom silently does nothing if you
end up back on the shape's own just-drawn boundary line (which is that
same colour). `draw.shield` draws its rectangle, then explicitly moves
(pen up) to the shield's centre before calling `fill`.

### Filenames and the lexer

`-` and `/` are lexer delimiters (same class as `+ * = < >`), so a quoted
word containing either — a natural filename like `"space-invaders.logo`,
or any path with a `/` in it — can't be typed without backslash-escaping
every delimiter character (`"space\-invaders.logo`, `"examples\/foo.logo`).
`load`'s own default prefix (`/Logo/` on-device) means users are expected
to `load` a short bareword filename anyway; `invaders.logo` was chosen
specifically to be typeable without any escaping.

## Known open issue

Some hardware screenshots have shown small alien-costumed marks — an
isolated "extra" cell, or a smeared/overlapping trail — that don't
correspond to any valid one of the 55 grid cells at the time. Extensive
host-side stress-testing (dozens of simulated `march.step` calls, checking
every computed grid position each time) shows the pure Logo-level grid
math is internally consistent and never produces this on its own, and a
review of the picocalc rendering path (stamp/erase compositing, dirty-tile
tracking, canvas-vs-sprite draw order) didn't turn up a mechanism either.
Current leading suspects, not yet confirmed on hardware:

- Screenshot capture happening asynchronously mid-`march.step` (which
  does up to 110 `setx`/`sety`+`stamp` calls before a single `refresh`),
  catching a genuinely half-erased/half-redrawn frame rather than a
  settled one.
- Stale copies of the file under test not reflecting the current
  `colgap`/`rowgap`/shield-fill fixes.

If you can reproduce this against the current file during normal
(unpaused, not-mid-loop) play, that would rule out both of the above and
point at a real remaining bug.
