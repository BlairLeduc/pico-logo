# Galaxian in Pico Logo (design)

Status: **design — not yet implemented.** Target deliverable is
[`logo/games/galaxian`](../logo/games/galaxian) (`load "galaxian` then
`galaxian`), a sibling of [`logo/games/invaders`](../logo/games/invaders).

This document describes only what Galaxian *changes* relative to the
Space Invaders port. Everything the two games share — the stamped-canvas
formation technique, the flat liveness list mutated in place with
`.setitem`, the sliced formation redraw, the `sync`-paced main loop, the
memory-model rules (`setx`/`sety` over `setpos`, `recycle`, no automatic
GC), the bitmap-anchor hit-tolerance argument, input handling, and the
iterative game-state loop — is inherited unchanged and documented in
[space-invaders-design.md](space-invaders-design.md); read that first.

The design brief is a **deliberately resource-reduced** Galaxian: fewer
aliens than the arcade's 46, sized for the 320×320 screen, and fitting
the same `MAX_TURTLES 8` / `MAX_DEMONS 8` budget Invaders proved out.

---

## 1. What Galaxian is, mechanically

Relative to Space Invaders, Galaxian **removes** two mechanics and
**adds** one:

| | Space Invaders | Galaxian |
|---|---|---|
| Formation motion | marches side-to-side **and descends** | sways side-to-side only, never descends |
| Shields | 4 destructible bunkers | **none** |
| Enemy fire | formation drops bombs | **only diving aliens fire** |
| The new thing | — | aliens **peel off and dive** at the player in curved swoops |

The removals matter as much as the addition: no shields means no
`fill`/erosion canvas work, and no formation fire means bomb logic is
tied to the (at most three) divers rather than scanning the formation.
The convoy itself is *cheaper* than the Invaders block — smaller, and
its sway never triggers the drop-and-flip descent. The whole per-frame
cost freed up is spent on the one new mechanic: steering divers.

## 2. The board

- Same screen model as Invaders: 320×320 steps, origin centre,
  `splitscreen`, HUD on the text rows via change-tracked
  `setcursor` + `print`.
- Player ship at y = −70, clamped to x ∈ [−150, 150].
- Convoy occupies y ∈ [80, 140] (4 rows) and sways within
  x ∈ [−110, 110]. It never descends, so there is **no invasion demon**
  and no `alien.bottom.y` machinery.
- Divers fly the space between; a diver crossing y < −85 has exited and
  rejoins the convoy (§5).
- No scrolling starfield in v1 (§9) — repainting canvas pixels every
  frame is the one arcade feature whose cost profile is wrong for this
  engine.

## 3. Object representation

The same central split as Invaders — turtles are the moving actors, the
canvas holds the crowd — but the slot map changes:

| Game object | Count | Representation | Why |
|---|---|---|---|
| Player ship | 1 | **turtle 0** | `touching?` with divers and bombs |
| Player shot | 1 | **turtle 1** | one at a time (classic); `setspeed` up; `touching?` divers; `colourunder` convoy |
| Divers | ≤3 | **turtles 2–4** | the new mechanic: autonomous `setspeed` + per-frame steering; must be real turtles for `touching?` |
| Enemy shots | ≤2 | **turtles 5 and 7** | dropped only by divers; `setspeed` down; `touching?` player |
| Formation stamper | — | **turtle 6**, always hidden | stamps/erases convoy cells, as in Invaders |
| Convoy | 20 | **canvas** (stamped) | sensed with `colourunder`; individually erasable |

Three diver turtles is exactly enough for Galaxian's signature move —
the **flagship dive with two escorts** — and it is what caps the game's
worst-case sprite load below Invaders' own worst case (Invaders flew
shot + UFO + 3 bombs = 5 autonomous sprites; Galaxian flies
shot + 3 divers + 2 bombs = 6, but with no march-descent redraw
pressure behind them).

### The demon budget is exactly 8

Every sprite–sprite collision routes through `when` demons, as in
Invaders (`touching?` names both turtles explicitly, so demons are
robust against the frame's `tell` churn):

```logo
when [touching? 1 2] [diver.shot 2]    ; player shot hits a diver
when [touching? 1 3] [diver.shot 3]
when [touching? 1 4] [diver.shot 4]
when [touching? 0 2] [player.hit]      ; diver rams the ship
when [touching? 0 3] [player.hit]
when [touching? 0 4] [player.hit]
when [touching? 0 5] [player.hit]      ; enemy shot hits the ship
when [touching? 0 7] [player.hit]
```

Eight demons, `MAX_DEMONS` is 8 — the design fits with **zero** slack.
(Invaders needed five and had three spare.) Dropping the invasion demon
is what pays for the extra collision pairs. Anything that later wants a
ninth demon — say a second shot — must either raise `MAX_DEMONS` or
move a check into the game loop.

## 4. The convoy

### Layout: 20 aliens in 4 ranks

The arcade convoy is 46 (1 row of 2 flagships, 6 red, 8 purple, 3×10
blue). The reduced convoy keeps one row per rank and shrinks each:

```logo
make "conv.rows 4
make "conv.cols 8          ; widest rank
make "colgap 20            ; 16x16 rendered costume + 4px gap, as Invaders
make "rowgap 20
; Row occupancy masks, row 1 = top. 0 cells are permanent holes:
; row 1: 2 flagships (cols 4,5)   row 2: 4 red (cols 3-6)
; row 3: 6 purple (cols 2-7)      row 4: 8 blue (cols 1-8)
```

- 20 aliens (2 + 4 + 6 + 8), a 140-px-wide bottom rank — comfortable
  sway room inside ±110 on a 320-px screen.
- The liveness list is the same flat, `.setitem`-mutated structure as
  Invaders, but 4×8 row-major with the rank-pyramid's corner cells
  **initialised to 0** (permanently dead). All the Invaders helpers
  (`alien.row`/`alien.col`, cell↔position formulas, `locate.alien`'s
  inverted-formula hit lookup with the ±10 tolerance) carry over with
  the new dimensions.
- **Liveness gains a third value: 2 = "out diving".** Draw loops test
  `1 =` (a diving alien's cell is empty on the canvas); the
  edge-tracking occupancy tests treat 1 *or* 2 as occupied, so a dive
  never widens/narrows the sway walls and a rejoin never has to
  re-derive them. Edges still only ever shrink — on true kills — so the
  Invaders `shrink.edges` amortisation argument still holds.

### One alien bitmap, four rank colours

Mono `putsh` costumes take their colour from the pen at `stamp` time,
so **one wing-flap pair serves blue, purple and red ranks** — the
stamper sets `setpc` per row as it walks the grid. Only the flagship
gets its own costume. Rank (= row) also keys the score table (§7).
This is why 15 shape slots are plenty (§8) despite four visually
distinct enemy ranks.

### Sway, not march

The convoy bounces between fixed walls at a constant gentle rate — no
descent, no speed-up-as-they-die cadence:

```logo
to step.sway
  ; every :sway.every frames, formx moves 2 steps; flip at the walls
  make "swayx :swayx + (2 * :swaydir)
  if or (:swayx > 40) (:swayx < -40) [make "swaydir 0 - :swaydir]
end
```

The redraw reuses Invaders' **sliced renderer** verbatim
(`march.begin` / `march.render.slice` / `march.if.due`, renamed
`sway.*`): the logical position advances at once, the stamping spreads
over frames at `:march.slice` cells each. With 20 cells instead of 55 a
full sway step finishes in ~4 frames at slice 6, so a 6-frame sway
cadence never overlaps its own render. Wing-flap frame toggling rides
the sway step exactly as it rode the march step.

The freed march budget is the headroom the divers spend.

## 5. Divers — the new mechanic

A dive moves an alien from the canvas to a turtle and back.

### Lifecycle

1. **Select** (`maybe.launch.dive`, one random-chance roll per frame,
   probability scaled by level): pick a live *flank* alien — walk in
   from `form.lo` / `form.hi`, alternating sides — matching the
   arcade's habit of pulling divers from the edges. A **flagship dive**
   (available whenever a flagship is alive and all three diver turtles
   are idle) takes the flagship plus up to two live red-rank escorts
   from adjacent columns, launching all three together.
2. **Detach**: mark the cell liveness 2, erase its stamp
   (`erase.alien.cell`), and give an idle diver turtle the alien's
   costume, rank colour, position and a starting heading; `st`,
   `setspeed 55`. Per-diver bookkeeping lives in three 3-element flat
   lists — `diver.cell`, `diver.phase`, `diver.timer` — indexed by
   (turtle − 1) and mutated with `.setitem`: zero allocation per dive,
   same argument as the liveness list.
3. **Steer** (`steer.divers`, in the game loop, every frame): the swoop
   is two phases of *clamped turning*, not a scripted path —
   `setspeed` supplies the motion, the loop only nudges the heading:
   - **peel** (~20 frames): heading starts at 180 ± 60 (away from the
     convoy, banking outward) and turns 6°/frame toward straight down —
     the outward loop.
   - **attack**: each frame compute the bearing to the player with
     `towards`, and turn toward it at most 4°/frame. The clamp is the
     whole feel of the game: too high and divers are unavoidable
     homing missiles, too low and they drift. These two constants
     (turn cap, `setspeed`) are the expected tuning ground.
4. **Fire**: an attacking diver rolls a small per-frame chance to drop
   an enemy shot from its position, aimed with `towards` at launch
   (then flying straight — `setspeed`, no further steering), using
   whichever of turtles 5/7 is idle.
5. **Exit / rejoin**: past y < −85 the diver hides, `setspeed 0`, its
   cell flips back to liveness 1, and the cell is re-stamped — the
   arcade's "loop off the bottom, reappear in the convoy", minus the
   fly-back-up animation.
6. **Killed in flight** (`diver.shot`, from the demon): hide diver and
   shot, cell liveness 2 → 0, decrement `:alive`, score at the
   *diving* (doubled) rate, run the edge-shrink checks.

Per-frame steering cost is small and bounded: ≤3 divers × (a `towards`,
a couple of comparisons, a `seth`). It replaces Invaders'
march-interval bookkeeping as the loop's "AI" line item, and is far
below the sliced redraw it shares the frame with.

### Player death and level end

`player.hit` freezes the world exactly as in Invaders. On respawn
(lives remain): bombs and divers are hidden and stopped, every
liveness-2 cell flips back to 1 (divers silently rejoin), the convoy
re-stamps, `thaw`. Level is clear when `:alive` reaches 0 — `:alive`
counts formation *and* diving aliens, so a level can end on a mid-air
kill; there is no "aliens win" condition besides losing all lives,
since the convoy never lands.

## 6. Shot vs. convoy: `colourunder`, not `over?`

Invaders tested `over? :alien.colour` — one colour, one test. The
convoy has **four** rank colours; four `over?` calls per frame is the
wrong shape. `colourunder` (same canvas-sensing family, answers for the
asked turtle, reference §*colourunder*) returns the palette slot in one
call:

```logo
ask 1 [if shown? [check.convoy.hit colourunder]]
```

where `check.convoy.hit` compares the slot against the four rank
colours (choosing palette slots distinct from every other game colour
makes this an exact test), then routes through `locate.alien` exactly
as Invaders did — same inverted cell formula, same ±10 tolerance
rationale (bitmap costumes anchor at their bottom row; see the
Invaders doc's hit-tolerance note).

Shot-vs-*diver* is not sensed this way — divers are sprites, invisible
to the canvas — that's what demon pairs 1–2/1–3/1–4 are for (§3).

## 7. Scoring

Rank-keyed, doubled in flight, flat bonus for a flagship killed while
diving — the arcade's escort-multiplier table collapsed to one rule:

| Rank | In convoy | Diving |
|---|---|---|
| Blue | 30 | 60 |
| Purple | 40 | 80 |
| Red | 50 | 100 |
| Flagship | 60 | 300 |

Rank is recovered from the row (convoy hits) or the diver's saved cell
(flight hits), so scoring allocates nothing.

## 8. Costumes and sound

Shape slots (15 available; Invaders used 8, this uses 8):

| Slot | Costume |
|---|---|
| 1, 2 | alien wing-flap A/B (mono; tinted per rank at stamp/wear time) |
| 3 | solid 16×16 eraser block (as Invaders) |
| 4, 5 | flagship A/B |
| 6 | player ship |
| 7 | player shot |
| 8 | enemy shot |

All `setrot "fixed`. Divers wear slots 1/2 (or 4/5) with
`setanim 1 2 150`, so the flap continues in flight for free while the
stamped convoy toggles frames on the sway step — the same
manual-toggle-vs-`setanim` split Invaders used for aliens vs. the UFO.

Sound stays `toot`-based: a two-note sway pulse on the sway step, a
short falling pitch sequence stepped once per frame while any diver is
in the attack phase (the arcade's dive shriek, coarsely), fire and
explosion blips. No new primitives.

## 9. Reduced-resource choices (the brief, made concrete)

| Arcade Galaxian | This port | Saving |
|---|---|---|
| 46-alien convoy | 20 aliens, 4×8 grid with rank pyramid | ~64% fewer cells per sway redraw |
| convoy up to ~10 wide (× 6 rows tall) | 8 × 4, 140 px wide | fits 320 px with ±40 sway |
| several concurrent divers + multi-ship flagship convoys | ≤3 divers (flagship + 2 escorts max) | fits turtles 2–4; bounds steering + demon cost |
| divers strafe continuously | ≤2 enemy shots alive at once | fits turtles 5 + 7 |
| scrolling starfield | none in v1 (static stamped stars a possible later polish) | no per-frame canvas repaint |
| escort-multiplier bonus table | flat diving-flagship bonus | no combo tracking |
| dive re-entry flies back to slot | teleport rejoin at bottom exit | no return-path steering |

None of these touch the interpreter; like Invaders, the game is pure
Logo against the P5 feature set.

## 10. Main loop

Same skeleton and `(setrefresh "sync 25)` pacing as Invaders — only the
`if not :paused` body changes:

```logo
until [:over] [
  poll.input
  if not :paused [
    sway.if.due          ; sliced convoy redraw (was march.if.due)
    steer.divers         ; NEW: per-frame clamped turning, <=3 turtles
    maybe.launch.dive    ; NEW: flank / flagship dive selection
    maybe.diver.fire     ; NEW: attack-phase shot drops
    check.shot           ; colourunder -> locate.alien on the convoy
    recycle.offscreen    ; shot past top; enemy shots past bottom
    recycle.divers       ; exit at bottom -> rejoin convoy
  ]
  draw.hud
  sync
  if :dying [handle.death]
  if :alive = 0 [make "over true]
]
```

Worst frame = sway slice (6 stamps) + 3 steered divers + 2 gliding
shots + HUD deltas. Every line item is at or below its Invaders
counterpart's proven cost except `steer.divers`, which replaces the
strictly heavier march bookkeeping. The 40 ms budget that held for
Invaders holds here with margin.

## 11. Risks / tuning expectations

- **Dive feel is the game.** The peel arc length, attack turn cap and
  diver speed will need on-hardware iteration before it reads as
  "Galaxian swoop" rather than "drifting descent". Constants are
  isolated at the top of the file for that reason.
- **Demon table is exactly full** (§3). Any added sprite-pair event
  costs an interpreter-limit bump or a loop-side check.
- **Rejoin pop.** The teleport rejoin (§5) is visible — a diver
  vanishes at the bottom and its cell refills at the top. Accepted for
  v1; the fix (steer a return path to the slot) costs only Logo code,
  no engine work.
