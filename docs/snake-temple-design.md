# The Snake Temple (design)

Status: **implemented.** The game is `logo/games/temple` and its tests are
`tests/test_temple.c`. Section 10 records what "faithful" could and could not
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

## 3. What the game fixes, and what it does not

The original's listing was **not obtainable** when this was written: itch.io,
the contest site and the blog mirrors are all outside the network policy this
work ran under. What is published is the game's own description:

> Among the labyrinth of tunnels is a treasure chest that you must find. The
> temple is full of snakes, so be careful!
> - A snake inflicts damage on 1 to 4 health points.
> - Each flask you find restores 2 health points.
> - Use the arrow keys to control Bocco.

Two further rules were supplied by the project owner from knowledge of the
original, and they are what the game turns on:

- **The temple is dark**, and is revealed as the player moves.
- **The snakes do not move.**

Both are load-bearing, not decoration. Static snakes make each one a *toll* on
the route past it rather than a chase, and darkness is what stops a player
planning that route in advance. Together they turn the published damage rule
into the game's economy: health is the currency you spend to explore.

Everything else is a choice made here, gathered in one place so a later
reading of the real listing can correct them one at a time:

| Choice | Value | Why |
|---|---:|---|
| Starting health | 10 | A ten-heart bar reads at a glance and survives two or three bad bites |
| Health cap | 10 | The HUD has ten hearts; a flask at full health cannot overflow it |
| Reveal | the 3×3 block around Bocco | Chosen by the project owner from three candidates: it gives one step of warning, so a snake is a toll to pay or route around rather than an ambush |
| Snakes | 20 | They do not move, so the temple must be genuinely full of them to be dangerous |
| Flasks | 10 | Roughly one per two snakes' worth of expected damage |
| Labyrinth | depth-first carve, 8 walls reopened | "A labyrinth of tunnels"; a 10-liner would have generated, not stored, its maze |
| Loops | few | A perfect maze makes every snake on the route an unavoidable toll; a handful of loops gives a way round *some* of them, and no more |
| Bocco's speed | one cell per 4 frames | 6.25 cells/s at 25 fps |
| Chest distance | ≥ 20 chambers | The walk there is the game |

## 4. The map is the only source of truth

A map cell holds a bank slot, and that slot is both the picture and the rule:

| Slot | Meaning | Walkable |
|---:|---|---|
| 0 | nothing (background, and the HUD's empty heart) | no |
| 1 | wall | no |
| 2 | heart (HUD only) | no |
| 3 | Bocco (drawn only) | no |
| 4 | floor | yes |
| 5 | flask | yes |
| 6 | chest | yes |
| 7 | snake | yes |

The order is load-bearing: everything a walker may stand on sorts above slot 3,
so `walkable?` is a single comparison. **A snake is walkable** — paying its
toll is a move the player is allowed to make, not a wall
(`test_a_snake_is_walkable_and_stone_is_not`).

Because the snakes do not move, they are simply map cells like the flasks and
the chest. There is no snake list, no snake movement and no per-frame snake
drawing anywhere in the game.

**Bocco is the one thing not in the map.** His cell lends him its slot for
exactly one `stamptile` and takes it straight back:

```logo
to draw.bocco
  local "w
  make "w (tile :st.col :st.row)
  settile :st.col :st.row :sl.bocco
  stamptile :st.col :st.row
  settile :st.col :st.row :w
end
```

So a `tile` lookup never answers with Bocco where a flask is, and stepping off
him restores whatever he was standing on.
`test_drawing_bocco_leaves_the_world_alone` compares all 1,600 cells across a
draw cycle.

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

A perfect maze has exactly one route between any two chambers, so every snake
between Bocco and the chest is a toll he has no choice but to pay. `open.loops`
reopens **eight** walls afterwards to give him somewhere to go instead — but
only eight, or a temple full of snakes would cost nothing to cross. It only
ever opens cells *between* chambers: a cell with both coordinates odd is a
wall junction, and opening those would dissolve the labyrinth into rooms
(`test_no_wall_junction_is_ever_opened`, `test_a_few_loops_are_opened_and_no_more`).

The property the game depends on and cannot check for itself — that every
chamber, and so the chest and every flask, is reachable from Bocco's corner —
is checked by a real flood fill in C over the real map
(`test_the_labyrinth_is_one_connected_network`).

## 6. The dark

Nothing is painted when a game starts: the screen is cleared and the map stays
where it is, unseen. Each step stamps the **3×3 block of cells around Bocco**,
and the picture accumulates, so what is on the screen is the map he has walked.

Because stamping is idempotent there is **no record of what has been revealed
to keep**. A cell revealed three steps ago is simply stamped again with the
same tile; a "seen" set would be a second source of truth for no gain. That is
the whole of it:

```logo
to reveal
  local "r
  repeat 3 [
    make "r :st.row + repcount - 2
    repeat 3 [stamptile (:st.col + repcount - 2) :r]
  ]
end
```

The block is always inside the map, because Bocco can only stand on chambers
and the gaps between them and the ring of wall is what keeps him there — so
the reveal needs no bounds test either.

Darkness is the one thing the map cannot show: an unrevealed cell and a
revealed one hold the same slot and differ only in what reached the canvas.
Its tests therefore assert on **pixels**, against a bank staged with flat
colours — the mock does not rasterise the pen, so the game's own art is
indistinguishable on it. They pin that the temple starts dark, that a revealed
cell shows the tile its own map slot names, that a step lights the block and
nothing further, that ground already walked stays lit, and that a step into a
wall lights nothing.

## 7. Rules

- **Movement.** Arrows set an intent that persists: Bocco keeps walking until
  a wall stops him (which clears the intent, so he stands still rather than
  grinding against stone) or another arrow turns him.
- **Flasks.** Stepping onto one adds 2 health, capped at 10, and rewrites the
  cell to floor.
- **The chest.** Stepping onto it wins.
- **Snakes.** They do not move. A snake is a walkable map cell: stepping onto
  it costs `1 + random 4` health and the snake **stays coiled where it lies**,
  so the toll is paid again by anyone who comes back this way. The reveal
  shows a snake one step before Bocco reaches it, so the choice — pay, or go
  round if the labyrinth offers a way round — is the game.

There is no mercy window and no invulnerability, because there is nothing to
be merciful about: damage is charged on *entering* a cell, not per frame, so
a snake cannot drain health while Bocco stands still.

## 8. The frame

25 fps in `sync` mode. Input is drained first and answers the pause key alone
when paused — a paused game that could not read its own unpause key would be
stuck, and one that still read arrows would let a player line Bocco up for
free (the defect fixed in Galaxian and Invaders on 2026-08-10).

Because the snakes do not move, **most frames draw nothing at all**. A step
repaints the cell Bocco is leaving, reveals the block around where he lands
and draws him there; frames between steps only poll input, refresh the HUD if
the health count changed, and `sync`. The HUD is redrawn only on a change.

`recycle` runs once every 250 frames. Logo never collects on its own, and a
game can run for minutes.

## 9. Testing

`tests/test_temple.c`, 47 tests. The suite loads the real game file with the
same hygiene checks Turtle Trails uses — a line too long for `load`, and a `;`
inside a bracketed list, are both silent corruptions — then:

- checks the labyrinth as a **graph** with a flood fill in C, rather than by
  trusting the game's own helpers;
- checks the dark on the **canvas**, since it does not exist in the map
  (section 6);
- pins the published rules directly: 200 bites all land in 1..4 *and* every
  value in that range occurs, a flask restores exactly 2, and a bitten snake
  is still there afterwards;
- runs **every** rule procedure, including the title and end screens (each
  ends by waiting for space, so feeding a space runs the whole procedure) and
  a whole game from the title screen to the exit.

The parse hazards listed at the top of the game file are runtime errors that
reading the source does not catch, so a suite that only inspected data would
pass over an implementation that could not run a single frame.

## 10. Divergences

- **The listing was never read.** Section 3 lists every constant that is
  therefore a choice rather than a port. The rules in section 7 are the
  published ones plus the two the project owner supplied; the numbers around
  them are not.
- **Presentation is Pico Logo's, not the Oric's.** Tiles are drawn with the
  pen at 8×8 rather than composed from Oric character cells, the palette is
  the stock one, and health is a heart bar rather than a printed number.
- **Pause and give-up keys** (`P`, `Q`) are added; a 10-liner would not have
  spent lines on them, and the sibling games here have both.
