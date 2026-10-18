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


class _Rand:
    """A fixed linear congruential sequence: the same shore every build."""

    def __init__(self, seed):
        self.state = seed

    def __call__(self):
        self.state = (self.state * 1103515245 + 12345) % 2 ** 31
        return self.state / 2 ** 31


def _rock(m, x, y, size, rand, squash=0.65):
    """A boulder, sea-worn: rings of jittered points - one buried in the
    sand, the widest a little above it, a rounder shoulder, then a crown -
    so it has no flat faces or right angles."""
    ground = height(x, y)
    n = 9
    turn = rand() * 2 * math.pi
    lean = (rand() - 0.5) * size * 0.4  # the crown off-centre: weathered, not lathed
    rings = []
    for share, radius, lift in ((0.0, 0.85, -0.25), (0.35, 1.0, None), (0.75, 0.7, None)):
        ring = []
        for i in range(n):
            a = turn + 2 * math.pi * (i + 0.5 * share) / n
            r = size * radius * (0.78 + 0.32 * rand())
            z = ground + (lift if lift is not None else size * squash * share * (0.8 + 0.4 * rand()))
            ring.append((x + r * math.cos(a) + lean * share, y + r * 0.8 * math.sin(a), z))
        rings.append(ring)
    top = (x + lean, y + size * (rand() - 0.5) * 0.25, ground + size * squash * (1.0 + 0.2 * rand()))
    tile = kit.tile_of("rock")
    uv = lambda p: (p[0] / tile + p[2] / tile, p[1] / tile + p[2] / tile)
    for lower, upper in zip(rings, rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            # Two triangles, not a quad: the jitter makes quads bend.
            for tri in ((lower[i], lower[j], upper[j]), (lower[i], upper[j], upper[i])):
                m.tri(list(tri), "rock", [uv(p) for p in tri])
    crown = rings[-1]
    for i in range(n):
        cap = [crown[i], crown[(i + 1) % n], top]
        m.tri(cap, "rock", [uv(p) for p in cap])


def _rocks(m):
    """Clusters at both ends of the beach, a few strays on the sand and
    one standing in the shallows."""
    rand = _Rand(9137)
    for cx, cy, count, big in ((-31.0, 18.0, 7, 2.0), (30.0, 17.0, 8, 2.4), (-12.0, 25.0, 3, 0.9),
                               (19.0, 6.0, 2, 0.6), (-24.0, 4.0, 2, 0.7)):
        for k in range(count):
            size = big * (0.35 + 0.65 * rand()) if k else big
            _rock(m, cx + (rand() - 0.5) * big * 3.0, cy + (rand() - 0.5) * big * 2.2, size, rand)


def _log(m, a, b, radius, segments=7):
    """A round log lying from a to b (x, y, z), bark outside, pale ends."""
    ax, ay, az = a
    bx, by, bz = b
    dx, dy, dz = bx - ax, by - ay, bz - az
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    ux, uy = -dy / math.hypot(dx, dy), dx / math.hypot(dx, dy)  # across, level
    tile = kit.tile_of("driftwood")
    ring = []
    for k in range(segments):
        t = 2 * math.pi * k / segments
        c, s = math.cos(t), math.sin(t)
        ring.append((ux * c * radius, uy * c * radius, s * radius))
    for k in range(segments):
        p, q = ring[k], ring[(k + 1) % segments]
        side = [(ax + p[0], ay + p[1], az + p[2]), (ax + q[0], ay + q[1], az + q[2]),
                (bx + q[0], by + q[1], bz + q[2]), (bx + p[0], by + p[1], bz + p[2])]
        u0, u1 = k / segments, (k + 1) / segments
        m.quad(side, "driftwood", uvs=[(u0, 0.0), (u1, 0.0), (u1, length / tile), (u0, length / tile)])
    for end, sign in ((a, -1), (b, 1)):
        ex, ey, ez = end
        points = [(ex + p[0], ey + p[1], ez + p[2]) for p in ring]
        if sign < 0:
            points.reverse()
        for k in range(1, segments - 1):
            m.tri([points[0], points[k], points[k + 1]], "wood_light", [(0.5, 0.5), (0.5, 0.0), (1.0, 0.5)])


def _driftwood(m):
    """Logs along the tideline, bleached ends, a broken branch on some."""
    for x, y, length, turn, radius in ((-14.0, 15.0, 3.4, 20, 0.16), (-4.0, 17.2, 2.2, -35, 0.12),
                                       (16.0, 14.0, 2.8, 70, 0.18), (23.0, 16.5, 1.8, 10, 0.1),
                                       (-21.0, 12.5, 2.6, -60, 0.14), (4.5, 13.5, 1.4, 50, 0.08)):
        a = math.radians(turn)
        hx, hy = math.cos(a) * length / 2, math.sin(a) * length / 2
        za = height(x - hx, y - hy) + radius * 0.7
        zb = height(x + hx, y + hy) + radius * 0.7
        _log(m, (x - hx, y - hy, za), (x + hx, y + hy, zb), radius)
        if radius > 0.13:
            # A branch stub off the far third, angled up.
            bx, by = x + hx * 0.4, y + hy * 0.4
            _log(m, (bx, by, (za + zb) / 2 + radius * 0.5),
                 (bx - hy * 0.35, by + hx * 0.35, (za + zb) / 2 + radius + 0.35), radius * 0.45, segments=5)


def _boat(m, boxes, x, y, yaw, length=5.2, beam=1.5, depth=0.6, roll=8.0):
    """A small wooden fishing boat drawn up on the sand, keel down, listing.
    Open: planks outside, darker inside."""
    a = math.radians(yaw)
    ca, sa = math.cos(a), math.sin(a)
    lean = math.radians(roll)
    ground = height(x, y)

    def place(u, v, w):
        # u along the boat, v across, w up; listing about the keel line.
        v, w = v * math.cos(lean) - w * math.sin(lean), v * math.sin(lean) + w * math.cos(lean)
        return (x + u * ca - v * sa, y + u * sa + v * ca, ground + 0.05 + w)

    # Each station across: gunwale, chine, keel, chine, gunwale. A flat-ish
    # bottom up to the chine, then near-vertical topsides - the part that
    # catches the light (a plain V faced the ground and drew black).
    stations = 9
    sections = []
    for i in range(stations):
        t = i / (stations - 1)
        half = beam / 2 * math.sin(math.pi * t) ** 0.55          # pointed at both ends
        rise = depth * (1.0 + 0.35 * (2 * t - 1) ** 4)            # the sheer: higher at bow and stern
        u = (t - 0.5) * length
        sections.append([place(u, -half, rise), place(u, -half * 0.85, depth * 0.3), place(u, 0.0, 0.0),
                         place(u, half * 0.85, depth * 0.3), place(u, half, rise)])
    for s0, s1 in zip(sections, sections[1:]):
        # Port strips (inner index, outer index), then starboard.
        for inner, outer, material in ((2, 1, "wood_dark"), (1, 0, "wood_light")):
            m.quad([s0[inner], s1[inner], s1[outer], s0[outer]], material)     # out
            m.quad([s0[outer], s1[outer], s1[inner], s0[inner]], "wood_dark")  # in
        for inner, outer, material in ((2, 3, "wood_dark"), (3, 4, "wood_light")):
            m.quad([s1[inner], s0[inner], s0[outer], s1[outer]], material)
            m.quad([s1[outer], s0[outer], s0[inner], s1[inner]], "wood_dark")
    # A thwart (the seat) across the middle.
    seat = place(0.0, 0.0, depth * 0.7)
    m.box(seat, (0.25, beam * 0.8, 0.05), "wood_dark", rotation=kit.rot_z(yaw))
    boxes.append(((x, y, ground + 0.4), (length * 0.9, beam, 0.8), kit.rot_z(yaw)))


def _hut(m, boxes, x, y):
    """A beach house shut for the season: plank walls, the front closed with
    shutters, a low tin roof, a bench outside."""
    z = PLATEAU
    width, depth, wall = 5.0, 3.2, 2.4
    front = y + depth / 2  # facing the sea (+Y)
    m.box((x, y - depth / 2, z + wall / 2), (width, 0.1, wall), "wood_dark")
    for sx in (-1, 1):
        m.box((x + sx * width / 2, y, z + wall / 2), (0.1, depth, wall), "wood_dark")
    # The front: shutters, panel by panel, a hair apart.
    inside = width - 0.2  # between the side walls
    for k in range(5):
        px = x - inside / 2 + inside * (k + 0.5) / 5
        m.box((px, front, z + wall / 2), (inside / 5 - 0.04, 0.08, wall - 0.1), "wood_light")
    m.box((x, front + 0.06, z + wall - 0.12), (width + 0.2, 0.12, 0.24), "wood_dark")
    # The roof, high at the front, sloping to the back.
    m.box((x, y - 0.1, z + wall + 0.18), (width + 0.6, depth + 0.9, 0.06), "metal_dark",
          rotation=kit.rot_x(7))
    for sx in (-1.6, 0.0, 1.6):
        m.box((x + sx, front + 0.55, z + 0.25), (1.2, 0.35, 0.06), "wood_dark")  # bench boards
    # A caged lamp over the shutters (shore.json's halo and light).
    m.box((x, front + 0.12, z + 2.12), (0.06, 0.24, 0.06), "metal_dark")
    m.box((x, front + 0.28, z + 2.05), (0.18, 0.18, 0.22), "lamp_glass")
    m.box((x, front + 0.55, z + 0.12), (4.2, 0.08, 0.24), "wood_dark")
    boxes.append(((x, y, z + wall / 2), (width + 0.1, depth + 0.1, wall)))


def _dune_fence(m, boxes):
    """A sand fence along the grass's edge: leaning posts, slats between,
    broken here and there; open at the road and the path to the jetty."""
    rand = _Rand(5501)
    y = -1.2
    x = -32.0
    while x < 32.0:
        if abs(x) < ROAD_HALF + 2.0 or abs(x - JETTY_X) < 2.2 or abs(x - 17.0) < 1.5:  # road, jetty, hut
            x += 1.5
            continue
        z = height(x, y)
        tilt = (rand() - 0.5) * 10.0
        m.box((x, y, z + 0.35), (0.08, 0.08, 0.7), "wood_dark", rotation=kit.rot_x(tilt))
        if rand() > 0.2:
            for k in range(4):
                m.box((x + 0.4 + k * 0.28, y, z + 0.28 + 0.06 * (rand() - 0.5)), (0.06, 0.03, 0.55 - 0.15 * rand()),
                      "wood_light")
        x += 1.5


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
    _rocks(m)
    _driftwood(m)
    _boat(m, boxes, -18.0, 12.5, 25.0)
    _boat(m, boxes, -13.0, 11.0, 40.0, length=4.4, beam=1.3, roll=-11.0)
    _hut(m, boxes, 17.0, -6.0)
    _dune_fence(m, boxes)
    _dune_grass(m)
    level.add_visual("shore", m)

    # Poles bring the road's wires to the turnaround, and stop; a lamp
    # over the turnaround (shore.json lights it).
    for y in (-40.0, -24.0, -10.0):
        level.place("utility_pole", -(ROAD_HALF + 2.0), y, -90.0, z=height(-(ROAD_HALF + 2.0), y))
    level.place("street_lamp", TURN_AT[0] + TURN_RADIUS + 0.6, TURN_AT[1], 180.0, z=PLATEAU)
    # Stone lanterns at the foot of the jetty.
    for side in (-1, 1):
        lx, ly = JETTY_X + side * 2.0, JETTY_Y[0] - 1.8
        level.place("toro", lx, ly, 0.0, z=height(lx, ly))

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
