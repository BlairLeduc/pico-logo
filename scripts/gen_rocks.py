#!/usr/bin/env python3
"""Generate the Asteroids rock and ship outlines as Logo turtle walks.

docs/asteroids-design.md section 6.3: an outline is authored as N radii at
equal angular spacing around a centre, and this script converts that to the
walk a turtle makes -- segment lengths and exterior turns.  Hand-written
turns do not close, and an unclosed rock leaves a gap that `pe` still erases
correctly but that looks broken.

Three outlines, one per size, with the scale baked into the literals rather
than computed on the board: an arithmetic statement costs 43 us on hardware
and computing a vertex would cost more than drawing it.

It was nine -- three shapes at each size -- until M0 measured what choosing
between them costs: 370 us a rock, a fifth of everything a rock costs
(docs/asteroids-design.md section 3.4).  One outline per size takes the
dispatch down to a single three-way test.  Shape variety is a `SIZES` entry
away if a later measurement says it can be afforded.

Run it and paste the output into the Logo file:

    python3 scripts/gen_rocks.py

The walk drops the turn after the final segment -- `place` sets the heading
before every pass, so that turn is never read.  That is what makes the
statement counts 19/15/13 in the design's table.

M2 added the ship (section 6.4), which is the same problem: a closed polygon
whose first vertex sits straight ahead of the turtle, so the same `walk` and
the same closure check apply.  Its vertices are authored rather than jittered,
and there are two of them -- see SHIPS.
"""

import math
import random

# Segments and radius by size, from the design's table (section 6.3).  Segment
# counts fall with size, which is where the saving belongs: small rocks are the
# numerous ones.  Four is the floor -- three would be a triangle and read as a
# shard rather than a rock.
SIZES = [("l", 6, 22.0), ("m", 5, 14.0), ("s", 4, 8.0)]

# Radii vary between these fractions of the nominal radius.  The floor is high
# enough that the polygon stays star-shaped and roughly convex at every
# segment count, so the walk has no reflex vertex to turn back through.
JITTER = (0.74, 1.0)

# Vertices also slide around the circle by up to this fraction of one spacing.
# Radius jitter alone is not enough at four segments: four vertices 90 degrees
# apart at similar radii draw a square, which reads as a box and not a rock.
# Under half a spacing, so the vertices keep their order and cannot cross.
ANGLE_JITTER = 0.22

# The ship (design section 6.4).  Two outlines rather than a hull plus a
# separate flame: the flame is folded into the SAME closed walk, because
# drawing it on its own would need a second `pu setx sety seth` to get back to
# the ship's centre -- four statements, which is most of what the flame costs
# to draw in the first place.  So a thrusting ship is one dispatch and one
# walk, exactly like a still one.
#
# Both start at the nose, which is straight ahead of a turtle at heading 0, so
# the prologue is the single `fd` the rocks use.  Vertices run clockwise, the
# direction `rt` turns.  The notch is a reflex vertex; `walk` handles that.
# Tried at 0.85 on a board and REJECTED, which is worth recording because the
# first play report asked for it: a smaller ship reads tidier, and it also makes
# the game easier, because the thing the rocks have to hit is the thing you are
# steering.  Full size is the harder game and the one that ships.
SHIPS = [
    ("ship", [(0.0, 12.0), (9.0, -9.0), (0.0, -5.0), (-9.0, -9.0)]),
    ("ship.flame", [(0.0, 12.0), (9.0, -9.0), (3.0, -6.0),
                    (0.0, -16.0), (-3.0, -6.0), (-9.0, -9.0)]),
]


def vertices(n, radius, rng):
    """N points spaced around the centre at jittered angles and radii.

    Logo headings are clockwise from north, so a bearing t maps to
    (sin t, cos t) and vertex 0 sits straight ahead of a turtle at heading 0 --
    which is what lets the walk's prologue be a single `fd`.  Vertex 0 keeps
    its exact bearing for that reason; the rest slide.
    """
    pts = []
    for k in range(n):
        slide = 0.0 if k == 0 else rng.uniform(-ANGLE_JITTER, ANGLE_JITTER)
        t = math.radians((k + slide) * 360.0 / n)
        r = radius * rng.uniform(*JITTER)
        pts.append((r * math.sin(t), r * math.cos(t)))
    return pts


def bearing(a, b):
    """Logo heading, in degrees, that walks from point a to point b."""
    return math.degrees(math.atan2(b[0] - a[0], b[1] - a[1])) % 360.0


def turn_to(current, wanted):
    """Signed turn from one heading to another, in (-180, 180]."""
    d = (wanted - current) % 360.0
    return d - 360.0 if d > 180.0 else d


def walk(pts):
    """The turtle walk for a closed polygon: (prologue, [(len, turn), ...]).

    Every number is rounded to one decimal.  That is still one literal token,
    so it costs exactly what an integer costs to evaluate, and it holds the
    closure error under half a pixel -- integer turns alone would drift by
    two or three across eight segments.
    """
    n = len(pts)
    reach = round(math.hypot(*pts[0]), 1)
    heading = 0.0
    first_turn = round(turn_to(heading, bearing(pts[0], pts[1])), 1)
    heading += first_turn

    legs = []
    for k in range(n):
        a, b = pts[k], pts[(k + 1) % n]
        length = round(math.hypot(b[0] - a[0], b[1] - a[1]), 1)
        if k == n - 1:
            legs.append((length, None))  # no turn after the last segment
            break
        wanted = bearing(b, pts[(k + 2) % n])
        t = round(turn_to(heading, wanted), 1)
        heading += t
        legs.append((length, t))
    return reach, first_turn, legs


def closure_error(pts, reach, first_turn, legs):
    """How far the *emitted* walk lands from where it started, in pixels."""
    x = y = 0.0
    heading = 0.0
    x += reach * math.sin(math.radians(heading))
    y += reach * math.cos(math.radians(heading))
    start = (x, y)
    heading += first_turn
    for length, t in legs:
        x += length * math.sin(math.radians(heading))
        y += length * math.cos(math.radians(heading))
        if t is not None:
            heading += t
    return math.hypot(x - start[0], y - start[1])


def fmt(v):
    """Trim a trailing `.0` so the common case reads as an integer."""
    return f"{v:g}"


def step(kind, value):
    if value < 0:
        return f"{'lt' if kind == 'rt' else 'rt'} {fmt(-value)}"
    return f"{kind} {fmt(value)}"


def emit(name, pts):
    """The Logo procedure for a closed polygon whose first vertex is ahead."""
    reach, first_turn, legs = walk(pts)
    err = closure_error(pts, reach, first_turn, legs)

    body = []
    for length, t in legs:
        body.append(f"fd {fmt(length)}" + ("" if t is None else f"  {step('rt', t)}"))

    lines = [f"to {name}"]
    lines.append(f"  pu fd {fmt(reach)}  {step('rt', first_turn)}  pd")
    for i in range(0, len(body), 3):
        lines.append("  " + "  ".join(body[i:i + 3]))
    lines.append("end")

    statements = 4 + (2 * len(pts) - 1)
    return "\n".join(lines), err, statements


def report(name, pts):
    text, err, statements = emit(name, pts)
    print()
    print(f"; {len(pts)} segments, {statements} statements, "
          f"closes to {err:.2f} px")
    print(text)


def main():
    print("; Generated by scripts/gen_rocks.py -- do not hand-edit.")
    print("; Outlines close to within a pixel; see the design section 6.3.")
    for size, n, radius in SIZES:
        # Seeded per outline so a regeneration reproduces the file exactly.
        report(f"rock.{size}", vertices(n, radius, random.Random(size)))
    for name, pts in SHIPS:
        report(name, pts)


if __name__ == "__main__":
    main()
