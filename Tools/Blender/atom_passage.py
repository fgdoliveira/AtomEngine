"""The passage (M45): under the machiya, a stair down into an earthen
cellar, a low timber-shored tunnel that forks, and a ladder up to a
trapdoor in the windmill field's shed. A level for the flashlight: baked
dark but not black - a candle in a shrine niche, a battery lantern on a
shelf, an old exit lamp by the ladder - and chalk marks that only the beam
shows, pointing the way at the fork.

Blender Z-up; origin at the top of the stair, which runs north (+Y).
"""

import atom_kit as kit
from atom_street import Street

FLOOR = -2.4          # the cellar and tunnel floor
CELLAR = (-3.0, 4.4, 3.0, 10.4)   # x0, y0, x1, y1
TUNNEL_HALF = 0.6
TUNNEL_TOP = FLOOR + 1.95
FORK_Y = (17.0, 18.4)
DEAD_END_X = -5.0
EAST_X = (4.8, 6.0)
CHAMBER = (4.4, 24.0, 6.6, 26.2)  # the ladder's room, under the trapdoor


def _floor(m, material, x0, y0, x1, y1, z):
    tile = kit.tile_of(material)
    m.quad([(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], material,
           uvs=[(x0 / tile, y0 / tile), (x1 / tile, y0 / tile), (x1 / tile, y1 / tile), (x0 / tile, y1 / tile)])


def _ceiling(m, material, x0, y0, x1, y1, z):
    tile = kit.tile_of(material)
    m.quad([(x0, y1, z), (x1, y1, z), (x1, y0, z), (x0, y0, z)], material,
           uvs=[(x0 / tile, y1 / tile), (x1 / tile, y1 / tile), (x1 / tile, y0 / tile), (x0 / tile, y0 / tile)])


def _wall(m, material, a, b, z0, z1):
    """A wall from a to b (x, y), facing whoever sees a -> b run from left
    to right (counter-clockwise from that side; the other side is culled)."""
    tile = kit.tile_of(material)
    length = ((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5
    m.quad([(a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1)], material,
           uvs=[(0, z0 / tile), (length / tile, z0 / tile), (length / tile, z1 / tile), (0, z1 / tile)])


def _room(m, x0, y0, x1, y1, z0, z1, wall="dirt", floor="dirt", ceiling="wood_dark", openings=()):
    """Floor, ceiling and four inward walls; `openings` are (side, from, to)
    gaps the corridors come through ("s", "n", "w", "e"; along x or y)."""
    _floor(m, floor, x0, y0, x1, y1, z0)
    _ceiling(m, ceiling, x0, y0, x1, y1, z1)
    sides = {
        "s": ((x1, y0), (x0, y0)), "n": ((x0, y1), (x1, y1)),
        "w": ((x0, y0), (x0, y1)), "e": ((x1, y1), (x1, y0)),
    }
    for side, (a, b) in sides.items():
        gaps = sorted((g0, g1) for s, g0, g1 in openings if s == side)
        axis = 0 if side in "sn" else 1
        start, end = a, b
        cursor = start
        for g0, g1 in gaps if a[axis] < b[axis] else reversed(gaps):
            lo, hi = (g0, g1) if a[axis] < b[axis] else (g1, g0)
            stop = list(cursor)
            stop[axis] = lo
            _wall(m, wall, cursor, tuple(stop), z0, z1)
            cursor = list(cursor)
            cursor[axis] = hi
            cursor = tuple(cursor)
        _wall(m, wall, cursor, end, z0, z1)


def _frame(m, x, y, along_y, half, z0, z1):
    """A timber frame shoring the tunnel: two posts and a lintel."""
    if along_y:
        for px in (x - half + 0.08, x + half - 0.08):
            m.box((px, y, (z0 + z1) / 2), (0.14, 0.14, z1 - z0), "wood_dark", faces=kit.SIDES)
        m.box((x, y, z1 - 0.07), (2 * half, 0.16, 0.14), "wood_dark", faces=kit.SIDES + [(0, 0, -1)])
    else:
        for py in (y - half + 0.08, y + half - 0.08):
            m.box((x, py, (z0 + z1) / 2), (0.14, 0.14, z1 - z0), "wood_dark", faces=kit.SIDES)
        m.box((x, y, z1 - 0.07), (0.16, 2 * half, 0.14), "wood_dark", faces=kit.SIDES + [(0, 0, -1)])


def _chalk(m, corners, uvs):
    """Chalk only the flashlight shows (M44); corners already off the wall."""
    m.quad(corners, "reveal_marks", uvs=uvs)


def build_passage(pieces, collision, materials, collection):
    level = Street(pieces, collision, materials, collection)
    m = kit.MeshBuilder(grid=1.0)
    boxes = []
    off = kit.DECAL_OFFSET

    # --- The stair down from the house: a landing, ten steps, walls and a
    # sloping ceiling following them down.
    _floor(m, "wood_light", -0.6, -0.4, 0.6, 1.0, 0.0)
    for i in range(10):
        y0, z = 1.0 + 0.3 * i, -0.24 * (i + 1)
        m.box((0, y0 + 0.15, z + 0.12), (1.2, 0.3, 0.24), "wood_light", faces=[(0, 0, 1), (0, -1, 0)])
        boxes.append(((0, y0 + 0.15, (z + FLOOR - 0.1) / 2), (1.2, 0.3, z - FLOOR + 0.1)))
    _floor(m, "wood_light", -0.6, 4.0, 0.6, 4.4, FLOOR)  # the last step down to the cellar
    boxes.append(((0, 4.2, FLOOR - 0.25), (1.2, 0.5, 0.5)))
    _wall(m, "plaster", (0.6, -0.4), (-0.6, -0.4), 0.0, 2.3)         # behind the landing
    _wall(m, "plaster", (-0.6, -0.4), (-0.6, 4.4), FLOOR, 2.3)
    _wall(m, "plaster", (0.6, 4.4), (0.6, -0.4), FLOOR, 2.3)
    _ceiling(m, "wood_dark", -0.6, -0.4, 0.6, 1.0, 2.3)
    m.quad([(-0.6, 4.4, 0.0), (0.6, 4.4, 0.0), (0.6, 1.0, 2.3), (-0.6, 1.0, 2.3)], "wood_dark")  # faces down
    boxes += [((0, 0.3, -0.25), (1.2, 1.4, 0.5)),                     # the landing
              ((0, -0.55, 1.0), (1.4, 0.3, 4.0)),
              ((-0.75, 2.0, 0.0), (0.3, 5.0, 5.0)), ((0.75, 2.0, 0.0), (0.3, 5.0, 5.0))]

    # --- The cellar.
    x0, y0, x1, y1 = CELLAR
    _room(m, x0, y0, x1, y1, FLOOR, 0.0, wall="stone",
          openings=[("s", -0.6, 0.6), ("n", -TUNNEL_HALF, TUNNEL_HALF)])
    boxes += [((0, (y0 + y1) / 2, FLOOR - 0.25), (x1 - x0 + 0.4, y1 - y0 + 0.4, 0.5)),
              ((x0 - 0.15, (y0 + y1) / 2, FLOOR + 1.2), (0.3, y1 - y0, 2.6)),
              ((x1 + 0.15, (y0 + y1) / 2, FLOOR + 1.2), (0.3, y1 - y0, 2.6)),
              (((x0 - 0.6) / 2, y0 - 0.15, FLOOR + 1.2), (-0.6 - x0, 0.3, 2.6)),
              (((x1 + 0.6) / 2, y0 - 0.15, FLOOR + 1.2), (x1 - 0.6, 0.3, 2.6)),
              (((x0 - TUNNEL_HALF) / 2, y1 + 0.15, FLOOR + 1.2), (-TUNNEL_HALF - x0, 0.3, 2.6)),
              (((x1 + TUNNEL_HALF) / 2, y1 + 0.15, FLOOR + 1.2), (x1 - TUNNEL_HALF, 0.3, 2.6))]
    # Shelves along the west wall, jars on them, the battery lantern.
    for z in (FLOOR + 0.6, FLOOR + 1.3):
        m.box((x0 + 0.25, 7.4, z), (0.45, 4.0, 0.05), "wood_dark")
    for i in range(9):
        y = 5.7 + 0.42 * i
        m.cylinder((x0 + 0.25, y, FLOOR + 0.625 + 0.7 * (i % 2)), 0.13, 0.32 - 0.06 * (i % 3), "ceramic",
                   segments=10)
    m.box((x0 + 0.25, 9.0, FLOOR + 1.425), (0.16, 0.12, 0.2), "metal_dark", faces=kit.SIDES + [(0, 0, 1)])
    m.box((x0 + 0.25, 9.0, FLOOR + 1.47), (0.2, 0.16, 0.08), "lamp_glass", faces=kit.SIDES)
    boxes.append(((x0 + 0.25, 7.4, FLOOR + 0.9), (0.5, 4.0, 1.8)))
    # Crates.
    for (cx, cy, size) in ((1.9, 5.3, 0.7), (2.4, 6.1, 0.6), (1.6, 9.5, 0.8)):
        m.box((cx, cy, FLOOR + size / 2), (size, size, size), "wood_light", faces=kit.NO_BOTTOM)
        boxes.append(((cx, cy, FLOOR + size / 2), (size, size, size)))
    # The shrine niche in the east wall, a candle burning in it.
    m.box((x1 - 0.02, 7.4, FLOOR + 1.0), (0.04, 0.8, 0.04), "wood_dark", faces=kit.SIDES + [(0, 0, 1)])
    m.box((x1 - 0.2, 7.4, FLOOR + 1.0), (0.36, 0.7, 0.04), "wood_dark", faces=kit.SIDES + [(0, 0, 1)])
    m.cylinder((x1 - 0.2, 7.4, FLOOR + 1.02), 0.025, 0.11, "cloth_white", segments=8)
    m.box((x1 - 0.2, 7.4, FLOOR + 1.15), (0.02, 0.02, 0.035), "lantern_paper")
    m.quad([(x1 - off, 7.55, FLOOR + 1.1), (x1 - off, 7.25, FLOOR + 1.1),
            (x1 - off, 7.25, FLOOR + 1.6), (x1 - off, 7.55, FLOOR + 1.6)], "ofuda")

    # --- The tunnel north, shored with timber.
    t0, t1 = y1, FORK_Y[0]
    _floor(m, "dirt", -TUNNEL_HALF, t0, TUNNEL_HALF, t1, FLOOR)
    _ceiling(m, "dirt", -TUNNEL_HALF, t0, TUNNEL_HALF, t1, TUNNEL_TOP)
    _wall(m, "dirt", (-TUNNEL_HALF, t0), (-TUNNEL_HALF, t1), FLOOR, TUNNEL_TOP)
    _wall(m, "dirt", (TUNNEL_HALF, t1), (TUNNEL_HALF, t0), FLOOR, TUNNEL_TOP)
    m.quad([(-TUNNEL_HALF, y1 - off, TUNNEL_TOP), (TUNNEL_HALF, y1 - off, TUNNEL_TOP),
            (TUNNEL_HALF, y1 - off, 0.0), (-TUNNEL_HALF, y1 - off, 0.0)], "stone")  # cellar wall above
    for y in (11.5, 13.0, 14.5, 16.0):
        _frame(m, 0.0, y, True, TUNNEL_HALF, FLOOR, TUNNEL_TOP)
    boxes += [((0, (t0 + t1) / 2, FLOOR - 0.25), (1.6, t1 - t0 + 0.2, 0.5)),
              ((-TUNNEL_HALF - 0.15, (t0 + t1) / 2, FLOOR + 1.0), (0.3, t1 - t0, 2.2)),
              ((TUNNEL_HALF + 0.15, (t0 + t1) / 2, FLOOR + 1.0), (0.3, t1 - t0, 2.2))]

    # --- The fork: west a dead end, east the way on.
    f0, f1 = FORK_Y
    _floor(m, "dirt", DEAD_END_X, f0, EAST_X[1], f1, FLOOR)
    _ceiling(m, "dirt", DEAD_END_X, f0, EAST_X[1], f1, TUNNEL_TOP)
    _wall(m, "dirt", (DEAD_END_X, f1), (EAST_X[0], f1), FLOOR, TUNNEL_TOP)               # north
    _wall(m, "dirt", (EAST_X[1], f0), (TUNNEL_HALF, f0), FLOOR, TUNNEL_TOP)              # south, east part
    _wall(m, "dirt", (-TUNNEL_HALF, f0), (DEAD_END_X, f0), FLOOR, TUNNEL_TOP)            # south, west part
    _wall(m, "dirt", (DEAD_END_X, f0), (DEAD_END_X, f1), FLOOR, TUNNEL_TOP)              # the dead end
    for (rx, ry, size) in ((DEAD_END_X + 0.5, 17.4, 0.6), (DEAD_END_X + 0.4, 18.0, 0.5),
                           (DEAD_END_X + 0.9, 17.8, 0.4)):
        m.box((rx, ry, FLOOR + size / 2), (size, size, size), "stone", faces=kit.NO_BOTTOM)
    for x in (-2.0, -3.5, 2.0, 3.5):
        _frame(m, x, (f0 + f1) / 2, False, (f1 - f0) / 2, FLOOR, TUNNEL_TOP)
    boxes += [(((DEAD_END_X + EAST_X[1]) / 2, (f0 + f1) / 2, FLOOR - 0.25), (EAST_X[1] - DEAD_END_X + 0.4, f1 - f0 + 0.4, 0.5)),
              (((DEAD_END_X + EAST_X[0]) / 2, f1 + 0.15, FLOOR + 1.0), (EAST_X[0] - DEAD_END_X, 0.3, 2.2)),
              (((EAST_X[1] + TUNNEL_HALF) / 2, f0 - 0.15, FLOOR + 1.0), (EAST_X[1] - TUNNEL_HALF, 0.3, 2.2)),
              (((-TUNNEL_HALF + DEAD_END_X) / 2, f0 - 0.15, FLOOR + 1.0), (-TUNNEL_HALF - DEAD_END_X, 0.3, 2.2)),
              ((DEAD_END_X - 0.15, (f0 + f1) / 2, FLOOR + 1.0), (0.3, f1 - f0, 2.2)),
              ((DEAD_END_X + 0.6, 17.7, FLOOR + 0.4), (1.0, 1.2, 0.8))]
    # Chalk at the fork, on the north wall facing the tunnel: an arrow
    # pointing east, the way on (the texture's arrow points down; the UVs
    # turn it). Seen only in the beam.
    cy = f1 - off
    _chalk(m, [(1.0, cy, FLOOR + 0.9), (2.0, cy, FLOOR + 0.9), (2.0, cy, FLOOR + 1.6), (1.0, cy, FLOOR + 1.6)],
           uvs=[(1, 0), (1, 1), (0, 1), (0, 0)])

    # --- East and north to the ladder.
    e0, e1 = EAST_X
    n0, n1 = f1, CHAMBER[1]
    _floor(m, "dirt", e0, n0, e1, n1, FLOOR)
    _ceiling(m, "dirt", e0, n0, e1, n1, TUNNEL_TOP)
    _wall(m, "dirt", (e0, n0), (e0, n1), FLOOR, TUNNEL_TOP)
    _wall(m, "dirt", (e1, n1), (e1, f0), FLOOR, TUNNEL_TOP)
    for y in (19.8, 21.3, 22.8):
        _frame(m, (e0 + e1) / 2, y, True, (e1 - e0) / 2, FLOOR, TUNNEL_TOP)
    boxes += [(((e0 + e1) / 2, (n0 + n1) / 2, FLOOR - 0.25), (e1 - e0 + 0.2, n1 - n0 + 0.2, 0.5)),
              ((e0 - 0.15, (n0 + n1) / 2, FLOOR + 1.0), (0.3, n1 - n0, 2.2)),
              ((e1 + 0.15, (f0 + n1) / 2, FLOOR + 1.0), (0.3, n1 - f0, 2.2))]

    # --- The ladder chamber, the trapdoor above.
    c0x, c0y, c1x, c1y = CHAMBER
    _room(m, c0x, c0y, c1x, c1y, FLOOR, 0.0, openings=[("s", e0, e1)])
    # Above the way in, the chamber is higher than the tunnel: close the gap.
    m.quad([(e1, c0y, TUNNEL_TOP), (e0, c0y, TUNNEL_TOP), (e0, c0y, 0.0), (e1, c0y, 0.0)], "dirt")
    m.box(((c0x + c1x) / 2, (c0y + c1y) / 2, -0.03), (0.9, 0.9, 0.04), "wood_light", faces=[(0, 0, -1)])
    for rx in (5.15, 5.85):                                                              # ladder rails
        m.box((rx, c1y - 0.15, FLOOR / 2), (0.06, 0.06, -FLOOR), "wood_light")
    for i in range(7):
        m.box((5.5, c1y - 0.15, FLOOR + 0.3 + 0.3 * i), (0.7, 0.04, 0.04), "wood_light")
    # The old exit lamp over the way in, still glowing green.
    m.box((5.5, c0y + 0.05, -0.3), (0.4, 0.08, 0.15), "exit_sign",
          faces=[(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, 0, 1), (0, 0, -1)])  # no back: it's on the wall
    # Chalk by the ladder: an arrow pointing up, to the trapdoor.
    cx = c0x + off
    _chalk(m, [(cx, 24.8, FLOOR + 1.0), (cx, 25.6, FLOOR + 1.0), (cx, 25.6, FLOOR + 2.0), (cx, 24.8, FLOOR + 2.0)],
           uvs=[(0, 0), (1, 0), (1, 1), (0, 1)])
    boxes += [(((c0x + c1x) / 2, (c0y + c1y) / 2, FLOOR - 0.25), (c1x - c0x + 0.4, c1y - c0y + 0.4, 0.5)),
              ((c0x - 0.15, (c0y + c1y) / 2, FLOOR + 1.2), (0.3, c1y - c0y, 2.6)),
              ((c1x + 0.15, (c0y + c1y) / 2, FLOOR + 1.2), (0.3, c1y - c0y, 2.6)),
              (((c0x + c1x) / 2, c1y + 0.15, FLOOR + 1.2), (c1x - c0x, 0.3, 2.6)),
              (((c0x + e0) / 2, c0y - 0.15, FLOOR + 1.2), (e0 - c0x, 0.3, 2.6)),
              (((c1x + e1) / 2, c0y - 0.15, FLOOR + 1.2), (c1x - e1, 0.3, 2.6))]

    level.add_visual("passage", m)
    level.add_collider("passage_col", boxes)

    level.add_marker("spawn", "from_house", 0.0, 0.2, 0.0, z=0.0)
    level.add_marker("spawn", "from_shed", 5.5, 25.0, 180.0, z=FLOOR)
    level.add_marker("entity", "stairs_up", 0.0, -0.2, z=0.0)
    level.add_marker("entity", "ladder", 5.5, 25.95, z=FLOOR)
    level.add_marker("entity", "bolt", 5.5, 25.1, z=-0.1)
    level.add_marker("entity", "fork_marks", 1.5, FORK_Y[1] - 0.02, z=FLOOR)
    level.add_marker("entity", "candle", CELLAR[2] - 0.2, 7.4, z=FLOOR + 1.0)
    return level


# Bake-only lights (atom_lightmap): (location, rotation degrees, (w, h),
# watts, rgb). The candle, the lantern, the exit lamp - and a faint grey
# draft of daylight through the trapdoor's boards. Nothing else: away from
# them the passage stays dark, which is the point.
LIGHTS = [
    ((CELLAR[2] - 0.25, 7.4, FLOOR + 1.25), (0, 0, 0), (0.05, 0.05), 6.0, (1.0, 0.62, 0.3)),
    ((CELLAR[0] + 0.3, 9.0, FLOOR + 1.6), (0, 0, 0), (0.1, 0.1), 9.0, (0.85, 0.92, 1.0)),
    ((5.5, CHAMBER[1] + 0.2, -0.55), (90, 0, 0), (0.3, 0.1), 5.0, (0.35, 1.0, 0.45)),
    ((5.5, 25.1, -0.08), (0, 0, 0), (0.8, 0.8), 6.0, (0.75, 0.8, 0.85)),
]
