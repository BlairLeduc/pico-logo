# Turtle Trails (design)

Status: **design only; implementation not started.**

Turtle Trails is an original maze-chase game for Pico Logo, *inspired by* the
1980s arcade genre but themed entirely around Logo itself: the player is the
Logo turtle, and the goal is to **paint every path of a garden maze with the
turtle's own pen** while four garden bugs hunt it down. It targets the
existing Pico Logo interpreter unchanged. The implementation lives at
`logo/games/trails`, with the static board at `logo/games/garden.bmp`, and
runs with:

```logo
load "trails
trails
```

The game keeps the *mechanics* that made the genre great — grid movement with
buffered turns and cornering, four pursuers with distinct targeting
personalities, alternating patrol/hunt modes, a temporary turn-the-tables
power-up, a central nest with release rules, side tunnels, escalating bonus
items, lives, and level progression. Game mechanics are not copyrightable;
what *is* protected is expression, and every piece of expression here is
original: the name, the maze layout, the characters and their names, all art,
all text, and all sound. No Namco names, maze data, artwork, or melodies may
appear anywhere — including in code identifiers, comments, or test names.

## 1. Theme

- **The turtle.** The player is the Logo turtle, pen down. Moving through an
  unpainted corridor lays down a visible pen trail — the core Logo idea made
  into a game. A level is cleared when every path tile has been painted.
- **The bugs.** Four bugs live in a nest at the centre of the garden and hunt
  the turtle. Each has a personality:

  | Bug | Colour | Personality |
  |---|---|---|
  | Dart | red | relentless — heads straight for the turtle |
  | Swoop | magenta | ambusher — aims ahead of the turtle |
  | Echo | cyan | flanker — mirrors Dart to trap the turtle in a pincer |
  | Moss | orange | timid — hunts from afar, retreats when close |

- **Power blossoms.** Four flowers in the maze corners. Eating one makes the
  bugs *dizzy* (blue, slow, reversed); a dizzy bug can be eaten by the turtle
  — turtles do eat insects — and its wings flutter back to the nest to grow a
  new body.
- **Bonus shapes.** Regular polygons — the most Logo object there is —
  appear at the garden gate and score escalating points by level.
- **Death.** A caught turtle withdraws into its shell.

## 2. Display and board geometry

The board is a **28×36 grid of 8×8-pixel tiles** (224×288 pixels), centred
unscaled on the PicoCalc's 320×320 LCD:

| Axis | Board content | LCD margin |
|---|---:|---:|
| horizontal | 224 px | 48 px left + 48 px right |
| vertical | 288 px | 16 px top + 16 px bottom |

The integer tile grid is what makes trail spacing, tunnel positions, actor
centres, and bug decisions exact. Tile coordinates are 1-based in Logo. Their
turtle coordinates are:

```text
x(col) = -108 + 8 × (col - 1)    ; col 1..28
y(row) =  140 - 8 × (row - 1)    ; row 1..36
```

This places the first and last tile centres at x = −108/+108 and
y = +140/−140. The content occupies x = −112..111 and y = −144..143,
subject to the display's pixel-coordinate convention.

Row usage:

- the top rows contain `SCORE`, `TOP`, and their values;
- the middle 31 rows contain the hedge maze, nest, paths, and actors;
- the bottom rows contain spare lives (small turtle stamps) and the
  recent-bonus-shape history.

The side bars and top/bottom margins are black.

## 3. The maze

The maze is an **original layout**, designed fresh for this game. It must not
reproduce the arcade maze. Required structural features (these are gameplay
affordances, not copied expression):

- hedge walls on a 28×31 tile field with rounded corridor junctions;
- one connected path network the turtle can fully paint;
- a central **nest** with a one-way door tile;
- **two** wrap-around side tunnels on *different* rows — a deliberate
  divergence from the single-tunnel classic that changes escape routes and
  bug-avoidance play;
- four power blossoms near the corners;
- a bonus-shape spawn tile (the "garden gate") below the nest;
- a small table of "calm rows" where hunting bugs may not choose to turn
  upward (keeps the area above the nest from being trivially camped).

The exact path-tile and blossom counts fall out of the reviewed map; the map
is the source of truth and tests assert internal consistency (see §13), not
magic totals. Target roughly 230–250 paintable tiles so pacing lands near the
classic feel.

## 4. Deliverables and source of truth

| File | Purpose |
|---|---|
| `logo/games/trails` | all game state, movement, AI, rules, sound, and loop |
| `logo/games/garden.bmp` | 320×320, 8-bit indexed BMP containing the centred maze, labels, and blossoms |

`loadpic` loads an 8-bit indexed BMP onto the graphics screen and switches
the active palette to the BMP's palette. A roughly 103 KB asset is acceptable
on all three target boards and far cheaper at runtime than drawing curved
hedge walls in Logo. (Confirm free space in the internal flash filesystem on
the 4 MB boards before finalising; shrink the palette or trim other assets if
tight.)

The Logo file contains the authoritative 28×36 gameplay map as 36 encoded
28-character words — **one `make` per line**; the parser drops multi-line
parenthesised expressions inside procedure bodies, so the map must never be
written as one wrapped expression. The BMP is generated from the same
reviewed map during development. A validation test must reject a BMP/map pair
whose blossom centres, door, tunnels, or board placement disagree.

The asset uses fixed palette slots; game logic compares palette *slots*,
never RGB values:

| Slot | Use |
|---:|---|
| 255 | black background |
| 248 | red / Dart |
| 249 | magenta / Swoop |
| 250 | cyan / Echo |
| 251 | orange / Moss |
| 252 | hedge green (maze walls) |
| 253 | dizzy blue |
| 254 | text white |
| 247 | trail yellow (pen colour) |
| 246 | unpainted-path grey specks |

## 5. Representation

### 5.1 Turtle allocation

Six of `MAX_TURTLES 8` are used. The compositor draws lower-numbered turtles
on top:

| Turtle | Object | Notes |
|---:|---|---|
| 0 | the turtle | lowest number, therefore on top; pen **down** in trail colour |
| 1 | Dart | red |
| 2 | Swoop | magenta |
| 3 | Echo | cyan |
| 4 | Moss | orange |
| 5 | bonus shape | hidden when inactive |
| 6 | spare | reserved for later score-pop-up polish |
| 7 | canvas helper | always hidden; stamps blossom erasers and HUD art |

The maze and trail are canvas pixels, not turtles. An eaten bug keeps its
turtle and changes to the wings costume while returning to the nest.

**Open item:** verify that `stamp` composites while the stamping turtle is
hidden. If it does not, turtle 7 shows, stamps, and re-hides inside a single
`sync`-mode frame — the intermediate state is never presented.

### 5.2 Tile map

At the start of each level, the 36 encoded words are decoded into a nested
Logo list: 36 mutable row lists, each containing 28 small integer tile codes.

| Code | Meaning |
|---:|---|
| 0 | dead space / hedge |
| 1 | painted (or intrinsically empty) path |
| 2 | unpainted path |
| 3 | power blossom |
| 4 | tunnel path |
| 5 | nest floor |
| 6 | nest door |

The nested layout makes access bounded and cheap:

```logo
to tile.at :col :row
  output item :col item :row :tiles
end

to set.tile :col :row :value
  .setitem :col item :row :tiles :value
end
```

A flat 1,008-member list would make every junction lookup walk hundreds of
cons cells; the nested form walks at most 36 + 28. Painting a tile mutates
code 2 or 3 to code 1 in place with `.setitem` and allocates nothing.

The encoded words remain immutable. Before building a new level, the old
`:tiles` value is made unreachable and `recycle` runs; decoding then creates
about 1,050 live list cells. A lost life does **not** rebuild the map,
because painted paths must stay painted.

### 5.3 Actor state

Turtle position on screen is the rendered result, not the authoritative
simulation state. The five moving actors use short, fixed lists indexed by
turtle number + 1:

- `actor.col`, `actor.row` — current logical tile;
- `actor.dir` — up, left, down, or right;
- `actor.offset` — signed pixel progress from the current tile centre;
- `actor.nextdir` — buffered player turn or preselected bug turn;
- `actor.state` — player/hunting/nest/leaving/dizzy/wings;
- `actor.reverse` — pending forced reversal;
- `actor.speed` — pixels per second for the current level and state.

All hot updates use `.setitem`. Direction deltas come from small procedures
rather than rebuilding `[dc dr]` lists in the frame loop. Target columns and
rows are passed as separate numbers for the same reason.

## 6. Movement and input

### 6.1 A deterministic 25 fps simulation

Use `(setrefresh "sync 25)` and advance actors explicitly in the Logo loop.
Do not use `setspeed` for maze travel: a wall-clock glide can cross a tile
centre between Logo instructions, which makes buffered turns and bug choices
load-dependent.

Each actor advances `speed / 25` pixels per frame. Fractional progress stays
in `actor.offset`; when progress crosses a tile centre, the actor enters the
next tile and any remaining distance carries forward. `place.actor` converts
tile + offset to `setx`/`sety` without allocating a position list. For
turtle 0 those `setx`/`sety` moves draw the pen trail — painting costs
nothing beyond the movement itself.

This gives repeatable movement on all three boards, no dependence on how long
bug AI took that frame, exact tile-centre events for painting and decisions,
and speeds that stay easy to express in pixels/second.

Initial practical values: about 64 px/s for the turtle and 56–60 px/s for
hunting bugs, tuned on hardware. A compact per-level profile table owns all
speed multipliers (paint pause, tunnel, dizzy, wings, frenzy — see §7.3)
rather than scattering constants through the code.

### 6.2 Controls and cornering

Arrow keys use the existing PicoCalc byte codes. Left 180 and right 183 are
confirmed by the shipped Galaxian source; **verify up/down on hardware**
before relying on them:

| Key | `ascii readchar` |
|---|---:|
| up | 181 (verify) |
| down | 182 (verify) |
| left | 180 |
| right | 183 |
| `p` (pause) | 112 |
| space | 32 |

`poll.input` drains every queued arrow byte each frame and retains the most
recent legal desire in the turtle's `actor.nextdir`. A reverse is accepted
immediately. A perpendicular turn stays buffered until its corridor opens.

The turtle may start a requested turn up to four pixels before the tile
centre and finish it up to four pixels after it. On acceptance, the
perpendicular axis snaps to the corridor centre and the unspent movement
continues in the new direction. This **cornering** window is what makes
grid movement feel crisp; forcing turns at the exact centre feels sluggish.

If the current path ends and the buffered turn is still blocked, the turtle
stops at the centre while facing the requested direction. Its walk animation
pauses until movement resumes.

### 6.3 Tunnels

Both tunnel rows wrap. Each row's left and right cells are adjacent in the
logical map. When an actor centre passes a portal, its logical column and x
coordinate are translated by 224 pixels before the frame is presented, with
the player's pen **up** across the teleport so no trail is drawn across the
screen (the pen also lifts for death and respawn repositioning). Bugs use
their tunnel-speed multiplier; the turtle does not.

Sprites can extend a few pixels into the black side margin while straddling a
portal because the compositor has no 224-pixel clipping window. The board
and margin share the same black slot, so this reads as edge clipping and is
accepted for v1.

## 7. Painting, blossoms, and bonus shapes

### 7.1 Painting

Unpainted path tiles show a faint 2×2 grey speck at their centre so the
player can see what remains. The turtle's 2-pixel pen trail passes straight
through the tile centre, so painting covers the speck with no extra drawing.
When the turtle reaches the centre of an unpainted tile:

1. inspect the tile code;
2. add 10 points for code 2 or 50 for code 3;
3. mutate it to code 1;
4. decrement `tiles.left`;
5. reset the nest inactivity timer.

Painting a fresh tile withholds one simulation movement quantum from the
turtle; a blossom withholds three. These short pauses let a pursuing bug gain
ground, which keeps clearing the last corridors tense.

A blossom is a larger disc in the BMP, so covering it needs more than the
trail: turtle 7 stamps the blossom-erase mask (slot 8, mono, painted in
background colour) over it. The mono `putsh` format renders its 8×16 bitmap
at 16×16 by doubling pixels horizontally, which covers the disc exactly;
transparent bits leave the hedge untouched.

### 7.2 Power blossoms

Eating a blossom:

- forces every hunting bug to reverse at its next tile centre;
- makes eligible bugs dizzy (nest, leaving, and wings bugs are immune);
- pauses the patrol/hunt schedule;
- resets the bug-chain score to 200;
- starts the level's dizzy timer and final flashing phase.

### 7.3 Patrol, hunt, and frenzy

Bugs alternate global modes:

```text
patrol, hunt, patrol, hunt, patrol, hunt, patrol, hunt forever
```

The first level uses 7/20, 7/20, 5/20, 5/∞ seconds; later level groups
shorten the final patrols. Dizzy time pauses this schedule. A patrol↔hunt
change sets a pending reversal on every hunting bug.

During patrol each bug loops near its home corner: patrol targets sit in
dead space beyond the four corners, so bugs orbit without arriving. When few
unpainted tiles remain, Dart enters **frenzy** (two threshold stages from the
level profile): faster, and targeting the turtle even during patrol.

The per-level profile table owns: turtle, bug, dizzy, tunnel, and frenzy
speeds; dizzy duration and flash count; patrol/hunt timings; frenzy tile
thresholds; bonus-shape identity and score; nest release limits. Group the
table by behavioural ranges rather than 21 near-identical rows.

### 7.4 Bug targeting

One tile before a junction, a hunting bug picks the exit tile with the
smallest squared Euclidean distance to its target:

```text
distance² = (candidate.col - target.col)² + (candidate.row - target.row)²
```

Bugs cannot reverse voluntarily. Ties break in fixed priority **up, left,
down, right**. A pending mode reversal overrides that rule once, at the next
tile centre. Hunting bugs may not choose upward in the calm rows; dizzy bugs
ignore the restriction. Nest, door, and tunnel access are enforced by actor
state, not treated as ordinary paths.

Hunt targets, per personality:

| Bug | Hunt target |
|---|---|
| Dart | the turtle's current tile |
| Swoop | four tiles ahead of the turtle's heading |
| Echo | the tile two ahead of the turtle, then double the vector from Dart to that tile |
| Moss | the turtle while at least eight tiles away; otherwise Moss's own corner |

The "ahead" calculations use plain vector arithmetic in all four directions —
the classic's famous facing-up overflow bug is **not** reproduced; there are
no memorised patterns to stay faithful to.

Dizzy bugs pick a pseudo-random initial direction, then try up/left/down/
right until a legal non-reverse exit is found. `rerandom` at level start and
after a lost life makes dizzy paths repeatable for tests.

### 7.5 The nest

At a level or life start:

- Dart waits above the door;
- Swoop, Echo, and Moss begin inside;
- Swoop leaves immediately;
- Echo and Moss leave according to personal painted-tile counters;
- a no-painting timer forces the preferred waiting bug out after four
  seconds (three from level five).

After a lost life, a global release counter replaces the personal counters
(Swoop at 7 tiles, Echo at 17, Moss at 32). These rules prevent stalling
with bugs trapped in the nest.

Nest movement is a short scripted state machine, not pathfinding:

```text
nest -> align with door -> leave upward -> hunting
wings -> target door -> enter downward -> regrow -> leave upward -> hunting
```

An eaten bug becomes wings: harmless, faster, and allowed through the door.
On reaching its home spot it regrows its body colour and exits immediately.

## 8. Collisions and scoring

Collision is checked after all five actors move for the frame. The turtle
collides with a bug when both actor centres occupy the same logical tile.
There is deliberately no swept test: the rare opposite-direction tile-swap
pass-through remains possible, and is kept as a deterministic, testable rule.

For each collision:

- `wings`, `nest`, and `leaving` bugs are harmless;
- a dizzy bug is eaten, changes to wings, and scores 200/400/800/1,600 for
  the current blossom chain;
- a hunting bug starts the shell-withdraw death sequence.

Bonus-shape collision uses the same-tile comparison, keeping all gameplay
collisions deterministic.

| Event | Score |
|---|---:|
| painted tile | 10 |
| power blossom | 50 |
| dizzy bugs in one chain | 200, 400, 800, 1,600 |
| bonus shape | 100–5,000 by level |
| first extra life | 10,000 |

Bonus shapes appear at the garden gate after 70 and 170 tiles painted in the
level, remain for a bounded pseudo-random interval near nine seconds, and
score by level:

| Levels | Shape | Points |
|---|---|---:|
| 1 | triangle | 100 |
| 2 | square | 300 |
| 3–4 | pentagon | 500 |
| 5–6 | hexagon | 700 |
| 7–8 | octagon | 1,000 |
| 9–10 | star | 2,000 |
| 11–12 | spiral | 3,000 |
| 13+ | gem | 5,000 |

The extra life is awarded once. High score is session-local in v1;
filesystem persistence is optional later and must write only at game over to
avoid unnecessary flash wear.

## 9. Costumes, animation, HUD, and sound

### 9.1 Shape slots

All sprites fit the 15 `putsh` slots (mono 8×16 bitmaps rendered 16×16,
painted in each turtle's pen colour):

| Slots | Use |
|---|---|
| 1–3 | turtle walk cycle (paddling legs) |
| 4–5 | common bug body animation, tinted per turtle's pen colour |
| 6 | returning wings, rotated by heading |
| 7 | current bonus shape, redefined at level setup |
| 8 | blossom erase mask |
| 9 | spare |
| 10–15 | shell-withdraw death sequence and polish |

The turtle uses `setrot "full`, so one walk cycle serves all four headings.
Bugs share the mono body frames with distinct pen colours; dizzy bugs reuse
them in dizzy-blue/white. Wings use one rotatable bitmap. No full-colour
costume pool is required.

Walk and bug animation run on `setanim`. Dizzy flashing is driven by the
dizzy timer, not extra frames. At death, slots 10–15 show the turtle drawing
its head and legs into its shell; movement and collisions are suspended
until the sequence completes.

To draw the bottom bonus history, setup walks the last seven levels,
redefines slot 7, stamps each shape onto the canvas, then restores the
current shape. The bonus turtle subsequently wears that slot.

### 9.2 HUD and transitions

The background BMP contains static labels. Dynamic score and top score are
drawn with `write`: old value first rewritten in background colour, new
value in white. Lives (small turtle stamps) and the bonus history are canvas
stamps redrawn only at setup or when a life changes.

Game states are iterative, never recursively chained:

```text
attract -> ready -> playing
                 -> death -> ready (if lives remain)
                 -> level flash -> next level
                 -> game over -> attract
```

The attract screen shows the title `TURTLE TRAILS`, the four bugs with their
names and colours, the score table, and `PRESS SPACE`. `READY!`, death, the
level-clear hedge flash, and `GAME OVER` all occur on the graphics screen
without discarding the painted map.

### 9.3 PSG sound plan

Every effect is centred with a left/right voice pair (the synth has tone
voices 0–2/4–6 and noise 3/7):

| Voices | Purpose | Suggested timbre |
|---|---|---|
| `[0 4]` | alternating paint dabs | short pulse notes |
| `[1 5]` | bug drone / dizzy loop | sustained square, pitch stepped by game state |
| `[2 6]` | bonus, bug-eaten, extra-life, ready jingle | triangle or sawtooth phrases |
| `[3 7]` | shell-withdraw sweep | periodic noise or low pulse sweep |

`setup.sound` sets envelopes and waveforms once. Per-frame code changes only
the drone pitch when its state changes; it never enqueues a note every
frame. `stopsound` runs on game over; BREAK/error handling already silences
the synthesizer, and `cs` is not relied upon to stop sound.

All phrases are **original compositions** written for `sound`/`play`. No
arcade melody, jingle, or motif may be imitated.

## 10. Main loop and state order

One frame has a fixed order:

```logo
to play.frame
  poll.input
  if not :paused [
    step.mode.clock
    step.nest.clock
    step.player
    paint.tile
    step.bugs
    step.bonus
    check.collisions
    update.drone
  ]
  draw.changed.hud
  sync
end
```

Ordering rules:

- eating a blossom changes bug state before that frame's collision test;
- all bugs move before any collision is resolved;
- a death or level-clear request is handled after `sync`, outside the hot
  update body;
- pause freezes simulation clocks and animation together;
- at most one state transition is committed per frame.

The surrounding level loop:

```logo
to play.level
  setup.level
  (setrefresh "sync 25)
  until [or :level.done :game.over] [
    play.frame
    if :dying [handle.death]
    if :tiles.left = 0 [handle.level.clear]
  ]
  setrefresh "auto
end
```

The top-level `trails` procedure uses `forever [one.game]`, and `one.game`
iterates levels. No level, death, or attract state calls the next state
recursively, so a long session does not grow the interpreter frame stack.

## 11. Memory and performance budget

The hot path is lighter than the shipped Galaxian game:

- five moving actors, no moving formation to restamp;
- at most four candidate exits per bug junction;
- a static canvas changed only by the pen trail, one blossom erase, or a HUD
  field;
- six visible sprites at worst;
- short actor-state lists mutated in place;
- no per-frame `setpos (list ...)`, `lput`, `fput`, or sentence building.

The main persistent allocation is the decoded map (about 1,050 cons cells)
plus compact level tables and procedures. Stay below 100 procedures and 100
global variables, leaving margin under `MAX_PROCEDURES 128` and
`MAX_GLOBAL_VARIABLES 128`.

Logo never collects garbage on its own — running out of space stops the
program — so run `recycle`:

- after discarding the previous level map, before decoding the next one;
- once before entering the frame loop;
- during `READY!` between lives if `nodes` measurements show a need.

Do not run it every frame. The acceptance soak test must show free nodes
stable across multiple level clears and lost lives; Galaxian and Invaders
run indefinitely with the same mutate-in-place discipline, which is the
precedent this budget relies on.

At 25 fps the frame budget is 40 ms. The expected worst ordinary frame is
four bug decisions plus a trail segment and HUD update. Profile with
`ticks`; an occasional transition-frame overrun is acceptable, but ordinary
play must not overrun.

## 12. Design boundaries

Intentional for v1:

- mechanics openly inspired by the classic maze-chase genre: buffered
  cornering movement, four pursuit personalities, patrol/hunt/dizzy modes,
  nest releases, tunnels, chain scoring, escalating bonuses;
- all expression original: name, maze, characters, art, text, and music;
  the classic's facing-up targeting bug is deliberately not reproduced;
- two tunnels, a paint-the-trail objective, and polygon bonuses as visible
  points of divergence from the source of inspiration;
- 25 fps simulation with fractional movement, not 60 Hz emulation;
- same-tile collision including the tile-swap pass-through;
- session-local high score.

## 13. Tests

Add `tests/test_trails.c`, following `test_galaxian.c`: load the Logo source
under the mock device and call pure helpers directly.

Required tests:

1. **Map invariants**
   - 36 rows × 28 columns;
   - decoded tile counts match the encoded words (paintable, blossoms);
   - one connected path network covering every code-2/3 tile;
   - correct tunnel adjacency on both tunnel rows, nest door, starts,
     gate tile, and calm rows.
2. **Coordinates**
   - all four corner tile centres;
   - tile↔pixel round trips;
   - tunnel translation on both rows, pen up across the teleport.
3. **Player movement**
   - buffered blocked turn;
   - early cornering window;
   - immediate reverse;
   - wall stop and fractional carry.
4. **Painting and bonuses**
   - in-place tile mutation and scores;
   - blossom state transition;
   - bonus shape at 70/170 painted tiles and timeout;
   - level completion at zero remaining.
5. **Bug targeting**
   - all four hunt targets in every turtle direction;
   - Moss's eight-tile boundary;
   - tie order and reverse exclusion;
   - tunnel and calm-row restrictions.
6. **Modes and nest**
   - patrol/hunt schedule and delayed reversal;
   - dizzy pause/restore/flashing;
   - personal and post-death global release counters;
   - wings return, regrow, and exit.
7. **Collisions and scoring**
   - lethal, dizzy, and harmless wings/nest cases;
   - 200/400/800/1,600 chain;
   - same-tile swap remains non-colliding;
   - one 10,000-point extra life.
8. **State-machine soak**
   - repeated deaths do not erase painted tiles;
   - several level rebuilds keep procedure depth and free-node count stable;
   - game over returns to attract without recursion.
9. **Asset validation**
   - BMP is 320×320 indexed colour;
   - board bounds are x = 48..271 and y = 16..303;
   - unpainted-path specks and blossom centres use the expected palette
     slots;
   - no residual third-party artwork: the BMP is generated from this
     project's map data only.

After implementation, run the full native suite:

```bash
cmake --preset=tests
cmake --build --preset=tests
ctest --preset=tests
```

Then build all firmware presets and play through on at least one non-PSRAM
board; the Pico 2 is the tightest common target.

## 14. Implementation milestones

1. **Board and player:** validated map/BMP, turtle sprite, pen trail,
   buffered turns, painting, score, tunnels, and level clear.
2. **One bug:** Dart path selection, hunt/patrol reversal, collision,
   death, and respawn.
3. **Four bugs:** personality targets, nest release, dizzy mode, wings
   return, frenzy, and calm-row/tunnel rules.
4. **Game structure:** bonus shapes, lives, extra life, level profiles,
   attract, ready, transitions, game over, and HUD history.
5. **Presentation:** final animation, PSG effects, level-clear flash,
   hardware tuning, soak tests, and all three firmware builds.
6. **Release check:** confirm no third-party names, maze data, art, or
   melodies survive anywhere in code, assets, tests, or docs.

No milestone should require a new C primitive. If implementation discovers
otherwise, stop and revise this design before changing the interpreter.

## Open verification items

- Arrow-key byte codes for up (181?) and down (182?) — confirm on hardware
  or in the keyboard driver before wiring `poll.input`.
- Whether `stamp` composites while the turtle is hidden (fallback in §5.1).
- Free flash-filesystem space for the ~103 KB BMP on the 4 MB boards.

## References

- [Pico Logo Reference](../reference/Pico_Logo_Reference.md), especially
  turtle graphics, multi-sprite sensing, refresh modes, sound, keyboard
  input, and `recycle`.
- [Multi-sprite design](multi-sprite-design.md) for compositor and costume
  constraints (mono shapes tinted by pen colour, lower turtle number on
  top, `stamp`).
- [Space Invaders design](space-invaders-design.md) and
  [Galaxian design](galaxian-design.md) for the proven pure-Logo game loop,
  mutable-list, HUD, testing, and memory patterns.
- [The Pac-Man Dossier](https://www.gamedeveloper.com/design/the-pac-man-dossier)
  as a *mechanics* study of the genre (tile model, pursuit targeting,
  schedules, releases). Mechanics only — no expression from it may be
  copied.
