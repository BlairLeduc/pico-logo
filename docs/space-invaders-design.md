# Space Invaders in Pico Logo (design)

Status: **draft for review.** A worked example that exercises the P5
multi-sprite feature set ([design](multi-sprite-design.md)). The goal is a
playable, recognisably-faithful Space Invaders written entirely in Logo —
no interpreter changes — that doubles as an acceptance test for `tell`/
`each`, `setspeed`, `setanim`, `touching?`, `over?`, `when`, `stamp` and
the refresh primitives working together.

The design's one real constraint is `MAX_TURTLES 8` (`core/limits.h`).
Classic Invaders has 55 aliens, 4 shields, a cannon, a shot, up to three
bombs and a UFO — far more than eight objects. The whole design is
organised around **which objects must be turtles and which can live on the
canvas**, and that split turns out to match how the 1978 hardware actually
worked.

---

## 1. The board

- Screen is 320×320 turtle steps, origin at centre: x ∈ [−159, 160],
  y ∈ [−159, 160] (reference §*setpos*).
- We run in **`splitscreen`**: the top 24 text-lines of graphics stay
  visible (240 units tall) and the bottom text lines carry the HUD
  (score, lives, level). There is no draw-text-on-canvas primitive, so
  the HUD is ordinary `setcursor` + `type`/`pr` on the text screen.
- Visible play field is therefore roughly y ∈ [−80, 160]. Cannon sits at
  y ≈ −70, the alien formation starts near the top, bombs fall down-screen.

## 2. Object representation — the central decision

| Game object | Count | Representation | Why |
|---|---|---|---|
| Player cannon | 1 | **turtle 0** | needs `touching?` with bombs; player-driven motion |
| Player shot | 1 | **turtle 1** | one shot on screen (classic rule); autonomous `setspeed` up; `touching?` UFO; `over?` aliens/shields |
| UFO / mystery ship | 1 | **turtle 2** | autonomous `setspeed` across top; `touching?` shot; `setanim` shimmer |
| Alien bombs | ≤3 | **turtles 3–5** | autonomous `setspeed` down; `touching?` player; `over?` shields |
| — spare — | | turtles 6–7 | headroom (4th bomb, or a second UFO) |
| Alien formation | 55 | **canvas** (stamped) | can't be 55 turtles; sensed with `over?`; individually erasable |
| Shields / bunkers | 4 | **canvas** (drawn) | static, destructible pixel terrain; sensed + eroded via canvas |

**Turtles are the moving projectiles and special ships; the canvas holds
the formation and the destructible terrain.** This is exactly the P5
compositor model (§2.4 of the engine design): sprites are state composited
at blit time, the canvas is drawn pixels that `over?`/`colourunder` sense
and `stamp` writes into. It is also faithful to the arcade original, where
the shields were RAM bitmaps eroded pixel-by-pixel and the aliens were a
block the game redrew as it marched.

## 3. The alien formation on the canvas

State lives in Logo variables, not in turtles:

```logo
make "aliens  <list of 55 flags, 1 = alive, 0 = dead>   ; row-major, 5×11
make "formx   -90     ; x of column 1's centre
make "formy    140    ; y of row 1's centre
make "formdir  1      ; +1 marching right, -1 left
```

- `item (r - 1) * 11 + c :aliens` is alien (row r, col c). Killing one
  rebuilds the list (no in-place mutation in Logo; kills are human-paced,
  so O(55) is free).
- **Marching** is discrete, a few times per second — the classic
  "step, step, step" cadence, not smooth glide. Each step:
  1. advance `formx` by a column-fraction; if the block's live edge would
     cross the wall, instead drop `formy` and flip `formdir` (the signature
     zig-zag descent).
  2. **redraw the band**: paint the formation's y-band to background, then
     `stamp` every living alien at its new cell (a turtle wearing the alien
     costume, `setpos` + `stamp` per cell).
  3. `setanim` toggles the two alien frames each step, so the whole block
     waddles in lockstep — one `setanim`/costume-swap does the animation.
- **Speed-up curve** falls out for free: the march delay scales with the
  live-alien count, so as the player clears the board the redraw shortens
  and the block accelerates — the original's rising panic, reproduced by
  construction, not scripted.

Killing an alien immediately blanks its cell so it doesn't linger until the
next march: wear a solid 16×16 mono block costume (`putsh 15 [255 255 …]`),
set the pen to the background slot, `stamp`. One primitive, one cell erased.

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
  `setspeed`, so bombs and shots collide "for free":

  ```logo
  when [touching? 1 2] [ufo.hit]        ; shot hits UFO
  when [touching? 0 3] [player.hit]     ; bomb 3 hits cannon
  when [touching? 0 4] [player.hit]
  when [touching? 0 5] [player.hit]
  ```

- **Turtle-vs-canvas → explicit checks in the game loop.** Because `over?`
  reads the *active* turtle, we address the projectile explicitly with
  `ask`, which is deterministic and immune to whatever `tell` the rest of
  the frame did:

  ```logo
  ask 1 [if over? :alien.colour  [hit.alien]]      ; shot reached the block
  ask 1 [if over? :shield.colour [erode 1  kill.shot]]
  ask [3 4 5] [if over? :shield.colour [erode who  kill.bomb who]]
  ```

  `hit.alien` reads the shot's `xcor`/`ycor`, maps them through `formx`/
  `formy` to a grid cell, marks that alien dead, blanks the cell (§3),
  scores, and recycles the shot (`ask 1 [ht setspeed 0]`).

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
when [:aliens.min.y < :cannon.y] [invasion]   ; block reached the cannon line
```

`freeze`/`thaw` pause the whole living world (motion, animation, demons) in
one word — the natural pause key. `clearscreen` clears demons and stops all
autonomous motion, so the between-level reset can't leave a demon armed or
a bomb gliding.

## 6. Input

Arrow keys and space arrive from `readchar` as single bytes; compare with
`ascii` (from `devices/picocalc/keyboard.h`):

| Key | Code | `ascii rc` |
|---|---|---|
| ← | `KEY_LEFT` 0xB4 | 180 |
| → | `KEY_RIGHT` 0xB7 | 183 |
| space (fire) | `KEY_SPACE` 0x20 | 32 |

```logo
to poll.input
  if not key? [stop]
  make "k ascii readchar
  if :k = 180 [ask 0 [move.cannon -6]]
  if :k = 183 [ask 0 [move.cannon  6]]
  if :k =  32 [fire]
end
```

`move.cannon` clamps the cannon to the play-field edges. `fire` only
launches when the shot is idle (`shown? 1` false): position it at the
cannon, `st`, `setspeed` up — one shot at a time, as in the original. The
cannon itself has no `setspeed`; it moves only when a key says so.

## 7. Main loop and game states

`setrefresh "manual` + one `refresh` per frame gives a tear-free,
batched present (engine design §2.3): all the frame's motion, stamps and
erosions appear at once.

```logo
to play.level
  setup.level
  setrefresh "manual
  until [:over] [
    poll.input                       ; cannon + fire
    march.if.due                     ; discrete alien step + waddle
    ask 1 [if shown? [check.shot]]   ; shot vs aliens / shields (over?)
    maybe.drop.bomb                  ; arm an idle bomb turtle
    maybe.launch.ufo
    refresh
    wait 33                          ; ~30 fps; setspeed is Δt-honest
  ]
  setrefresh "auto
end
```

Bombs, shot and UFO advance on their own via `setspeed` between iterations
(the evaluator's motion tick), and the `when` demons fire whenever a
turtle-pair touches — so the loop body is just input, the march timer, the
canvas-sense checks, and spawning. Wall-clock-scaled `setspeed` keeps
speeds honest even when a heavy frame runs long.

Game states: **attract** (title on the text screen, demons disarmed) →
**play** (loop above) → **death** (freeze, explosion `toot`, decrement
lives) → **next level** (`cs`, rebuild formation lower/faster) →
**game over** (score, back to attract). `p` toggles `freeze`/`thaw`.

## 8. Costumes and sound

- **Costumes** via `putsh` bitmaps (the 16-number mono format): alien A
  two frames, alien B two frames, cannon, UFO, bomb, plus the solid eraser
  block. Slots 1–15 are plenty; the two-frame pairs drive `setanim`.
  `setrot "fixed` for all of them (aliens don't rotate). Colour costumes
  (`snapsh`) are unnecessary here — mono shapes painted in a pen colour are
  truer to the monochrome arcade look and cheaper.
- **Sound** via `toot _duration_ _freq_`: the four-note descending march
  loop (pitch stepping with march speed), the shot's *pew*, the explosion,
  and the UFO's warble. `toot` is blocking, so keep durations short (a few
  tens of ms) inside the loop; the march "music" is one note per march
  step, which the discrete cadence already spaces out.

## 9. What v1 simplifies (and why it's still Invaders)

| Faithful | Simplified in v1 | Note |
|---|---|---|
| 5×11 block, zig-zag descent, speed-up | fixed 55 start each level | classic |
| One player shot at a time | — | classic rule |
| Destructible shields | coarser erosion chunks | pixel-exact erosion is a later polish |
| ≤3 alien bombs | one bomb type, random column drops | targeted "aimed" bombs later |
| Mystery UFO with bonus | fixed bonus, timed appearance | score-position bonus later |
| Lives, levels, score | text-screen HUD | no on-canvas digits primitive |

None of these touch the interpreter; they're all Logo-level polish.

## 10. Why this is a good P5 acceptance test

It uses, in one program, every headline P5 primitive against the mock and
on hardware:

- `tell` / `ask` / `each` / `who` — addressing the projectile turtles.
- `setspeed` / `speed` — autonomous shot, bombs, UFO.
- `setanim` — alien waddle and UFO shimmer.
- `touching?` — every sprite-sprite hit.
- `over?` / `colourunder` — shot-vs-formation and shield erosion.
- `when` / `freeze` / `thaw` — collision demons and pause.
- `stamp` — building and erasing the formation and shields.
- `setrefresh` / `refresh` — the tear-free game loop.

If Invaders plays correctly, the milestone works end-to-end.

## 11. Deliverable and phasing

Ship as a single loadable program, `examples/space-invaders.logo`
(`load "space-invaders` then `invaders`), plus a short section in the
reference or an `examples/README`. Suggested build order, each a runnable
checkpoint:

1. **Cannon + shot + shields.** Move, fire, one shot, shield erosion via
   `over?`. Proves input, `setspeed`, canvas sensing.
2. **Alien formation.** Stamped block, discrete march, zig-zag descent,
   `setanim` waddle, shot-kills-alien with cell erase and speed-up curve.
3. **Bombs + UFO + demons.** Autonomous bombs, `when` collision demons,
   UFO fly-by, player death, lives.
4. **States + HUD + sound.** Attract/play/death/game-over, splitscreen
   score/lives, `toot` march music and effects, `freeze` pause.

## Open questions

- **Fire cadence / auto-repeat.** `readchar` gives key presses, not
  held-key state, so continuous cannon movement needs the player to repeat
  the arrow key (or key-repeat from the keyboard driver). Acceptable for
  v1; a held-key device query would smooth it but is out of scope.
- **Frame pacing.** `wait` is now in milliseconds, so a per-frame delay
  like `wait 33` (~30 fps) paces the loop directly. Alternatively drop the
  `wait` and let `setspeed`'s Δt-scaling plus the refresh limiter set the
  cadence.
- **Shield erosion fidelity.** Chunk-erase is simple; true per-pixel
  erosion (matching the projectile's footprint) is nicer but needs a
  small-footprint erase costume. Flagged for phase 1 polish.
