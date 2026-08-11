# The Snake Temple (design)

Status: **implemented.** The game is `logo/games/temple` and its tests are
`tests/test_temple.c`. Section 9 records what "faithful" could and could not
mean here.

The Snake Temple is a **port of RAX's 2022 BASIC 10Liner entry for the Oric
Atmos** — category PUR-80, written in Oric BASIC 1.1b, published on itch.io as
[*The Snake Temple (Oric) by RAX*](https://bunsen.itch.io/the-snake-temple-by-rax).
It targets the existing Pico Logo interpreter unchanged and runs with:

```logo
load "temple
temple
```

## 1. Provenance and attribution

Unlike [Turtle Trails](turtle-trails-design.md), which keeps a genre's
mechanics and originates all of its expression, this is a **port**: it carries
the original's title, its character (Bocco, RAX's own), and its rules across
to another machine. That is a deliberate choice — the whole point of a port is
to be the same game — but it means the expression is RAX's, not ours.

- The title screen credits RAX and the 2022 contest by name.
- No code was copied: the original listing was not obtainable (see section 3),
  and every line of `logo/games/temple` is written for this interpreter.
- **Publishing this port needs RAX's blessing.** Until then it is an exercise
  in the tree, not a release. If permission is not forthcoming, the fallback
  is the Turtle Trails route: keep the mechanics, rename the game and the
  character, and redraw the art.

## 2. Display and board geometry

The map is **40×40 cells of 8×8 pixels**, covering the PicoCalc's 320×320 LCD
exactly. Map cell `(col,row)` has its centre at

```text
x(col) = -156 + 8 × (col - 1)     col 1..40
y(row) =  156 - 8 × (row - 1)     row 1..40
```

so cell 1's outer edge is on -160 and cell 40's on +160.

The labyrinth is a grid of **19×17 chambers**. Chamber `(i,j)` sits at map
cell `(2i, 2j)`, so chambers occupy the even coordinates and the walls between
them occupy the odd ones. Everything outside that — column 1, column 39, row 1,
row 35 — stays wall.

That ring is load-bearing. **Movement does no bounds testing at all**: a
walker cannot reach the edge of the map, so no step can leave it, and
`open?` is one `tile` lookup rather than a lookup plus four comparisons.
`test_the_labyrinth_is_ringed_by_solid_wall` is what holds that up.

Rows 36–40 sit below the labyrinth's south wall and hold the HUD. Nothing
there is walkable, so a heart can never be picked up
(`test_the_hud_rows_are_not_walkable`).

The Oric original ran on a 40×28 character grid. At 8×8 tiles the PicoCalc
gives 40×40, so the playfield is comparable in density and needed no redesign;
the extra rows became the HUD.

## 3. What the published rules fix, and what they do not

The original's listing was **not obtainable** when this was written: itch.io,
the contest site and the blog mirrors are all outside the network policy this
work ran under. What *is* published, and is therefore what this port is
faithful to, is the game's own description:

> Among the labyrinth of tunnels is a treasure chest that you must find. The
> temple is full of snakes, so be careful!
> - A snake inflicts damage on 1 to 4 health points.
> - Each flask you find restores 2 health points.
> - Use the arrow keys to control Bocco.

Everything else is a choice made here. They are gathered in one place so a
later reading of the real listing can correct them one at a time:

| Choice | Value | Why |
|---|---:|---|
| Starting health | 10 | A ten-heart bar reads at a glance and survives two or three bad bites |
| Health cap | 10 | The HUD has ten hearts; a flask at full health cannot overflow it |
| Snakes | 6 | Enough that the labyrinth is not safe, few enough to route around |
| Flasks | 8 | Roughly one per two snakes' worth of expected damage |
| Labyrinth | depth-first carve, 20 walls reopened | "A labyrinth of tunnels"; a 10-liner would have generated, not stored, its maze |
| Bocco's speed | one cell per 4 frames | 6.25 cells/s at 25 fps |
| Snake speed | one cell per 6 frames | Slower than Bocco, so a corridor can be won |
| Mercy after a bite | 25 frames (1 s) | Without it a snake sharing a corridor drains ten points in ten frames |
| Chest distance | ≥ 20 chambers | The walk there is the game |

## 4. The map is the only source of truth

A map cell holds a bank slot, and that slot is both the picture and the rule:

| Slot | Meaning | Walkable |
|---:|---|---|
| 0 | nothing (background, and the HUD's empty heart) | no |
| 1 | wall | no |
| 2 | heart (HUD only) | no |
| 3 | snake (drawn only) | no |
| 4 | Bocco (drawn only) | no |
| 5 | floor | yes |
| 6 | flask | yes |
| 7 | chest | yes |

The order is load-bearing: everything a walker may stand on sorts above slot 4,
so `walkable?` is a single comparison
(`test_only_floor_flask_and_chest_are_walkable`).

**Actors are not in the map.** The map holds the world; an actor is drawn by
lending its cell the actor's slot for exactly one `stamptile` and handing the
cell straight back:

```logo
to draw.actor :c :r :slot
  local "w
  make "w (tile :c :r)
  settile :c :r :slot
  stamptile :c :r
  settile :c :r :w
end
```

So a `tile` lookup never answers with a snake where a floor tile is, and
standing on a flask cannot erase it. Erasing is the same repaint without the
loan — whatever the actor covered is still in the map, so it comes back by
itself. `test_drawing_an_actor_restores_the_cell` compares all 1,600 cells
before and after a draw/erase/draw cycle.

Every tile is drawn with the pen and picked up with `snaptile`, so the game
ships **without a picture asset** and the art cannot drift away from the code
that uses it.

## 5. Carving the labyrinth

A depth-first carve with an explicit stack. The map itself records which
chambers have been visited — a carved chamber is floor, an unvisited one is
still wall — so there is no second copy of the grid to keep in step, and the
maze that gets walked is the maze that got drawn.

A chamber rides the stack as one number, `(j-1) × 19 + i`, because a stack of
pairs would cons two cells per push. Neighbours are tried from a random
offset rather than a shuffled list, which allocates nothing per chamber.

Because chamber `(i,j)` is at map `(2i, 2j)`, the wall between two adjacent
chambers is at the **sum of their indices**: between `2i` and `2i+2` lies
`2i+1`, which is `i + (i+1)`. That is the whole of `carve`.

A perfect maze has exactly one route between any two chambers, which makes a
snake in the corridor ahead a dead end rather than a hazard, so `open.loops`
reopens twenty walls afterwards. It only ever opens cells *between* chambers:
a cell with both coordinates odd is a wall junction, and opening those would
dissolve the labyrinth into rooms
(`test_no_wall_junction_is_ever_opened`).

The property the game depends on and cannot check for itself — that every
chamber, and so the chest and every flask, is reachable from Bocco's corner —
is checked by a real flood fill in C over the real map
(`test_the_labyrinth_is_one_connected_network`).

## 6. Rules

- **Movement.** Arrows set an intent that persists: Bocco keeps walking until
  a wall stops him (which clears the intent, so he stands still rather than
  grinding against stone) or another arrow turns him.
- **Flasks.** Stepping onto one adds 2 health, capped at 10, and rewrites the
  cell to floor.
- **The chest.** Stepping onto it wins.
- **Snakebite.** A snake sharing Bocco's cell costs `1 + random 4` health and
  grants a second of mercy, during which he blinks and cannot be bitten again.
  The check runs twice a frame — after Bocco moves and again after the snakes
  move — which is what catches a head-on swap: Bocco steps onto the cell the
  snake has not left yet.
- **Snakes.** A snake keeps going while it can and turns at a junction one
  time in four. Reverse is the last resort, so a snake patrols a corridor
  instead of shuffling on the spot — but it *is* available, or a dead end
  would trap it for the rest of the game.

## 7. The frame

25 fps in `sync` mode. Input is drained first and answers the pause key alone
when paused — a paused game that could not read its own unpause key would be
stuck, and one that still read arrows would let a player line Bocco up for
free (the defect fixed in Galaxian and Invaders on 2026-08-10).

Actors are erased, moved and redrawn each frame; snakes are drawn before
Bocco so he is never hidden under one at the moment he is hit. The HUD is
redrawn only when the health count changes.

`recycle` runs once every 250 frames. Logo never collects on its own, and the
frame's own work allocates almost nothing (`.setitem` mutates in place), but a
game can run for minutes.

## 8. Testing

`tests/test_temple.c`, 46 tests. The suite loads the real game file with the
same hygiene checks Turtle Trails uses — a line too long for `load`, and a `;`
inside a bracketed list, are both silent corruptions — then:

- checks the labyrinth as a **graph** with a flood fill in C, rather than by
  trusting the game's own helpers;
- pins the two published damage rules directly: 200 bites all land in 1..4
  *and* every value in that range occurs, and a flask restores exactly 2;
- runs **every** rule procedure, including the title and end screens (each
  ends by waiting for space, so feeding a space runs the whole procedure) and
  a whole game from the title screen to the exit.

The parse hazards listed at the top of the game file are runtime errors that
reading the source does not catch, so a suite that only inspected data would
pass over an implementation that could not run a single frame.

## 9. Divergences

- **The listing was never read.** Section 3 lists every constant that is
  therefore a choice rather than a port. The mechanics in section 6 are the
  published rules; the numbers around them are not.
- **Presentation is Pico Logo's, not the Oric's.** Tiles are drawn with the
  pen at 8×8 rather than composed from Oric character cells, the palette is
  the stock one, and health is a heart bar rather than a printed number.
- **Pause and give-up keys** (`P`, `Q`) are added; a 10-liner would not have
  spent lines on them, and the sibling games here have both.
