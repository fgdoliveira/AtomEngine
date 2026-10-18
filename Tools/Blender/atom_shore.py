"""The shore (demo vertical): where the coast road ends. The road comes up
between low dunes to a turnaround above a wide beach; the sand runs down
to the sea, a wooden jetty goes out over it, and drift logs and rocks lie
along the tideline. Night, the moon low over the water. Walked on foot;
the bike waits at the turnaround to ride back (shore.json).

The water is one surface at z = 0 whose first UV's u holds the depth, as
the lakeshore's (atom_lakeshore). The ground is solid as it is shaped: its
collision is a coarser copy of the same height function, so the sand and
the dunes are walked on, not floated over.
Blender Z-up; the sea is north (+Y), the road comes from the south.
"""

import math

import atom_kit as kit
from atom_lakeshore import _grid, _smooth, _wall
from atom_street import Street

PLATEAU = 0.6        # the turnaround and the grass above the beach
WATERLINE = 20.0     # y where the sand meets the sea
DEPTH = 3.0
SHELF = 25.0         # metres from the waterline to full depth
DUNES_FROM = -14.0   # y where the dunes start rising, southward
ROAD_HALF = 3.5
TURN_AT = (0.0, -6.0)
TURN_RADIUS = 7.0
ROAD_Z = PLATEAU + 0.06
GROUND = (-60.0, -46.0, 60.0, 70.0)
JETTY_X = 9.0
JETTY_HALF = 1.0
JETTY_Y = (WATERLINE - 4.0, WATERLINE + 22.0)
DECK = 0.55
WALK_LIMIT = WATERLINE + 3.0   # wade a little, no further
BOUNDS = (-34.0, -34.0, 34.0, WALK_LIMIT)


def height(x, y):
    """Dunes to the south (cut by the road), the flat above the beach, the
    sand sloping into the sea."""
    if y >= WATERLINE:
        return -DEPTH * _smooth((y - WATERLINE) / SHELF)
    if y >= 0.0:
        return PLATEAU * (1.0 - _smooth(y / WATERLINE))
    if y >= DUNES_FROM or abs(x) < ROAD_HALF + 2.5:
        return PLATEAU
    swell = 0.6 + 0.4 * math.sin(x * 0.17) * math.cos(y * 0.11)
    rise = 2.8 * _smooth((DUNES_FROM - y) / 14.0) * swell
    # The road's cutting: the dunes ease down to it.
    return PLATEAU + rise * _smooth((abs(x) - ROAD_HALF - 2.5) / 6.0)


def _ground(m):
    def material(points):
        lowest = min(height(x, y) for x, y in points)
        if min(y for _, y in points) >= -1.0 or lowest < PLATEAU - 0.05:
            return "sand"
        return "meadow"

    def uv(x, y, material):
        tile = kit.tile_of(material)
        return (x / tile, y / tile)

    _grid(m, *GROUND, 2.0, height, material, uv)


def _water(m):
    def material(points):
        return "water" if min(height(x, y) for x, y in points) < 0.0 else None

    def uv(x, y, _):
        return (min(max(-height(x, y) / DEPTH, 0.0), 1.0), 0.0)

    _grid(m, GROUND[0], WATERLINE - 4.0, GROUND[2], GROUND[3], 4.0, lambda x, y: 0.0, material, uv)


def _road(m):
    """Up from the south between the dunes to a round turnaround."""
    tile = kit.tile_of("asphalt")
    y0, y1 = GROUND[1], TURN_AT[1]
    m.quad([(-ROAD_HALF, y0, ROAD_Z), (ROAD_HALF, y0, ROAD_Z), (ROAD_HALF, y1, ROAD_Z), (-ROAD_HALF, y1, ROAD_Z)],
           "asphalt", uvs=[(0.0, y0 / tile), (2 * ROAD_HALF / tile, y0 / tile),
                           (2 * ROAD_HALF / tile, y1 / tile), (0.0, y1 / tile)])
    cx, cy = TURN_AT
    segments = 16
    for i in range(segments):
        a0, a1 = 2 * math.pi * i / segments, 2 * math.pi * (i + 1) / segments
        p0 = (cx + TURN_RADIUS * math.cos(a0), cy + TURN_RADIUS * math.sin(a0))
        p1 = (cx + TURN_RADIUS * math.cos(a1), cy + TURN_RADIUS * math.sin(a1))
        # A fan, a hair above the straight so the two never fight.
        z = ROAD_Z + 0.006
        m.quad([(cx, cy, z), (p0[0], p0[1], z), (p1[0], p1[1], z), (cx, cy, z)], "asphalt",
               uvs=[(cx / tile, cy / tile), (p0[0] / tile, p0[1] / tile), (p1[0] / tile, p1[1] / tile),
                    (cx / tile, cy / tile)])


def _jetty(m, boxes):
    y0, y1 = JETTY_Y
    length = y1 - y0
    m.box((JETTY_X, (y0 + y1) / 2, DECK - 0.05), (2 * JETTY_HALF, length, 0.1), "wood_light")
    y = y0 + 1.0
    while y < y1:
        for x in (JETTY_X - JETTY_HALF + 0.1, JETTY_X + JETTY_HALF - 0.1):
            bottom = min(height(x, y), 0.0) - 0.3
            m.cylinder((x, y, bottom), 0.1, DECK - 0.1 - bottom, "wood_dark", segments=8, caps=False)
        m.box((JETTY_X, y, DECK - 0.17), (2 * JETTY_HALF + 0.1, 0.14, 0.12), "wood_dark")
        y += 2.5
    m.cylinder((JETTY_X + JETTY_HALF - 0.25, y1 - 0.3, DECK), 0.08, 0.7, "wood_dark", segments=8)
    # A step up from the sand, half the deck's height.
    step = ((JETTY_X, y0 - 0.45, DECK / 4), (2 * JETTY_HALF, 0.9, DECK / 2))
    m.box(*step, "wood_dark")
    boxes.append(step)
    # The deck to walk on, and rails nobody sees: off it is the sea.
    boxes.append(((JETTY_X, (y0 + y1) / 2, DECK - 0.75), (2 * JETTY_HALF, length, 1.5)))
    for x in (JETTY_X - JETTY_HALF - 0.15, JETTY_X + JETTY_HALF + 0.15):
        boxes.append(((x, (WALK_LIMIT + y1) / 2, 1.5 + DECK), (0.3, y1 - WALK_LIMIT, 3.0)))
    boxes.append(((JETTY_X, y1 + 0.15, 1.5 + DECK), (2 * JETTY_HALF + 0.6, 0.3, 3.0)))


def _driftwood(m):
    """Logs along the tideline and rocks at the beach's ends."""
    for x, y, length, turn in ((-14.0, 15.0, 3.2, 20), (-4.0, 17.5, 2.1, -35), (16.0, 14.0, 2.7, 70),
                               (23.0, 16.5, 1.8, 10), (-22.0, 12.5, 2.4, -60)):
        z = height(x, y) + 0.12
        m.box((x, y, z), (length, 0.26, 0.24), "wood_dark", rotation=kit.rot_z(turn), faces="all")
    for x, y, size, turn in ((-30.0, 18.0, 1.8, 15), (-27.5, 21.0, 1.1, 50), (29.0, 17.0, 1.5, 30),
                             (31.0, 21.5, 2.2, 5), (-12.0, 24.0, 0.9, 70)):
        z = height(x, y)
        m.box((x, y, z + size * 0.3), (size, size * 0.8, size * 0.75), "stone",
              rotation=kit.rot_z(turn) @ kit.rot_x(8), faces="all")


def _dune_grass(m):
    state = [777]

    def rand():
        state[0] = (state[0] * 1103515245 + 12345) % 2 ** 31
        return state[0] / 2 ** 31

    for _ in range(140):
        x, y = rand() * 100 - 50, -rand() * 30 - 2
        if abs(x) < ROAD_HALF + 1.5 or math.hypot(x - TURN_AT[0], y - TURN_AT[1]) < TURN_RADIUS + 1.0:
            continue
        kit._cards(m, (x, y, height(x, y) - 0.05), 0.7 + 0.4 * rand(), 0.6 + 0.5 * rand(), "grass",
                   count=3, turn=360.0 * rand())


def build_shore(pieces, collision, materials, collection):
    level = Street(pieces, collision, materials, collection)
    m = kit.MeshBuilder(grid=2.0)
    boxes = []
    _ground(m)
    _water(m)
    _road(m)
    _jetty(m, boxes)
    _driftwood(m)
    _dune_grass(m)
    level.add_visual("shore", m)

    # Poles bring the road's wires to the turnaround, and stop.
    for y in (-40.0, -24.0, -10.0):
        level.place("utility_pole", -(ROAD_HALF + 2.0), y, -90.0, z=height(-(ROAD_HALF + 2.0), y))

    level.add_marker("spawn", "from_road", TURN_AT[0] - 2.0, TURN_AT[1] - 1.0, 0.0, z=PLATEAU)
    level.add_marker("spawn", "jetty_end", JETTY_X, JETTY_Y[1] - 1.0, 180.0, z=DECK)

    # Solid as shaped: the same height function on the same grid.
    solid = kit.MeshBuilder()
    _grid(solid, *GROUND, 2.0, height, lambda points: "sand", lambda x, y, material: (0.0, 0.0))
    level.colliders.append(solid.build("shore_ground_col", materials, collection))
    x0, y0, x1, y1 = BOUNDS
    entry = (JETTY_X - JETTY_HALF - 0.3, JETTY_X + JETTY_HALF + 0.3)
    # Counter-clockwise, so each box stands on its segment's right: outside.
    for a, b in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (entry[1], y1)),
                 ((entry[0], y1), (x0, y1)), ((x0, y1), (x0, y0))):
        boxes.append(_wall(a, b))
    level.add_collider("shore", boxes)
    return level
