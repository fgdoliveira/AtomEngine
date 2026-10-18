"""The coast road (demo vertical): the Kawasaki's ride out of town. A
two-lane road along a cliff over the sea, at night in the fog: a guardrail
on the sea side, power poles on the land side, hills rising inland. Nobody
walks here - the level is a ride (coast_road.json's "ride") - so it has a
floor to stand the bike on and nothing else solid.

The road's centre line is the ride's path: written into the markers file
as "ride_path" (game axes), so the bike rides exactly the road built here.
Blender Z-up; the road runs north (+Y), the sea is east (+X).
"""

import math

import atom_kit as kit
from atom_lakeshore import _grid
from atom_street import Street

# The road's centre line, south to north, one point per 100 m or so: gentle
# S-bends. Always increasing in y, so the centre at any y is one value.
CONTROL = [(0.0, -10.0), (0.0, 60.0), (-14.0, 170.0), (-26.0, 280.0), (-10.0, 390.0),
           (18.0, 500.0), (30.0, 610.0), (14.0, 720.0), (-6.0, 820.0), (0.0, 900.0), (0.0, 925.0)]
SAMPLES = 16             # per span: the same as the game's RoadPath
HALF_ROAD = 3.5          # two lanes
SHOULDER = 1.5
RAIL_AT = HALF_ROAD + 0.9
ROAD_Z = 0.06            # the asphalt over its flat bed
SEA_LEVEL = -9.0
CLIFF_FOOT = -14.0
GROUND = (-90.0, -40.0, 230.0, 950.0)  # x0, y0, x1, y1
STEP = 4.0
POLE_EVERY = 40.0


def _catmull(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return tuple(0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3)
                 for a, b, c, d in zip(p0, p1, p2, p3))


def centre_line():
    """The road sampled as the game samples it (uniform Catmull-Rom,
    clamped ends): a list of (x, y)."""
    n = len(CONTROL)
    at = lambda i: CONTROL[min(max(i, 0), n - 1)]
    points = []
    for i in range(n - 1):
        for k in range(SAMPLES):
            points.append(_catmull(at(i - 1), at(i), at(i + 1), at(i + 2), k / SAMPLES))
    points.append(CONTROL[-1])
    return points


LINE = centre_line()


def centre_x(y):
    """The road's centre at `y` (the line only ever goes north)."""
    if y <= LINE[0][1]:
        return LINE[0][0]
    for (x0, y0), (x1, y1) in zip(LINE, LINE[1:]):
        if y <= y1:
            t = (y - y0) / (y1 - y0) if y1 > y0 else 0.0
            return x0 + (x1 - x0) * t
    return LINE[-1][0]


def _smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def height(x, y):
    """Flat along the road; a cliff falling to the sea east of it; hills
    rising west of it, swelling and dipping along the way."""
    s = x - centre_x(y)  # metres east of the centre
    # Flat a grid cell beyond the shoulder: a cell straddling the road's
    # edge would otherwise lift its corners onto the hill or the cliff and
    # come up through the asphalt.
    edge = HALF_ROAD + SHOULDER + STEP
    if abs(s) <= edge:
        return 0.0
    if s > 0.0:
        return CLIFF_FOOT * _smooth((s - edge) / 14.0)
    swell = 0.65 + 0.35 * math.sin(y * 0.021) * math.cos(y * 0.007 + 1.3)
    return 16.0 * _smooth((-s - edge) / 34.0) * swell


def _ground(m):
    def material(points):
        lowest = min(height(x, y) for x, y in points)
        highest = max(height(x, y) for x, y in points)
        if highest - lowest > 2.5:
            return "stone"          # the cliff face, the cuts
        if lowest < SEA_LEVEL + 0.5:
            return "sand"           # the cliff's foot, under and at the water
        if abs(lowest) < 0.01 and abs(highest) < 0.01:
            return "gravel"         # the road's bed and shoulders
        return "meadow"

    def uv(x, y, material):
        tile = kit.tile_of(material)
        return (x / tile, y / tile)

    _grid(m, *GROUND, STEP, height, material, uv)


def _water(m):
    """The sea, east of the cliff, its first UV's u the depth (0 at the
    waterline, 1 at full depth) for the water shader."""
    def material(points):
        return "water" if min(height(x, y) for x, y in points) < SEA_LEVEL else None

    def uv(x, y, _):
        return (min(max((SEA_LEVEL - height(x, y)) / (SEA_LEVEL - CLIFF_FOOT), 0.0), 1.0), 0.0)

    _grid(m, -40.0, GROUND[1], GROUND[2], GROUND[3], 8.0, lambda x, y: SEA_LEVEL, material, uv)


def _frames():
    """Along the road: (point, unit direction, unit right) per sample."""
    frames = []
    for i, (x, y) in enumerate(LINE):
        a = LINE[max(i - 1, 0)]
        b = LINE[min(i + 1, len(LINE) - 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy) or 1.0
        dx, dy = dx / length, dy / length
        frames.append(((x, y), (dx, dy), (dy, -dx)))  # right of north is east
    return frames


def _road(m):
    """Asphalt between the shoulders, a dashed centre line."""
    frames = _frames()
    travelled = 0.0
    for (p, d, r), (q, e, s) in zip(frames, frames[1:]):
        corners = [(p[0] - r[0] * HALF_ROAD, p[1] - r[1] * HALF_ROAD, ROAD_Z),
                   (p[0] + r[0] * HALF_ROAD, p[1] + r[1] * HALF_ROAD, ROAD_Z),
                   (q[0] + s[0] * HALF_ROAD, q[1] + s[1] * HALF_ROAD, ROAD_Z),
                   (q[0] - s[0] * HALF_ROAD, q[1] - s[1] * HALF_ROAD, ROAD_Z)]
        span = math.hypot(q[0] - p[0], q[1] - p[1])
        tile = kit.tile_of("asphalt")
        v0, v1 = travelled / tile, (travelled + span) / tile
        m.quad(corners, "asphalt", uvs=[(0.0, v0), (2 * HALF_ROAD / tile, v0),
                                         (2 * HALF_ROAD / tile, v1), (0.0, v1)])
        # A dash on the first third of every 9 m (spans are about 6 m).
        if travelled % 9.0 < 3.0:
            w = 0.08
            m.quad([(p[0] - r[0] * w, p[1] - r[1] * w, ROAD_Z + 0.01), (p[0] + r[0] * w, p[1] + r[1] * w, ROAD_Z + 0.01),
                    (q[0] + s[0] * w, q[1] + s[1] * w, ROAD_Z + 0.01), (q[0] - s[0] * w, q[1] - s[1] * w, ROAD_Z + 0.01)],
                   "road_paint")
        travelled += span


def _guardrail(m):
    """On the sea side: a post every 4 m, a beam between them."""
    frames = _frames()
    last = None
    travelled = 0.0
    for i, ((x, y), d, r) in enumerate(frames):
        if i:
            travelled += math.hypot(x - frames[i - 1][0][0], y - frames[i - 1][0][1])
        if last is not None and travelled - last < 4.0 and i != len(frames) - 1:
            continue
        px, py = x + r[0] * RAIL_AT, y + r[1] * RAIL_AT
        m.box((px, py, 0.4), (0.12, 0.12, 0.8), "metal_dark")
        if last is not None:
            ax, ay = prev
            length = math.hypot(px - ax, py - ay)
            angle = math.degrees(math.atan2(py - ay, px - ax))
            m.box(((ax + px) / 2, (ay + py) / 2, 0.62), (length, 0.05, 0.3), "metal_white",
                  rotation=kit.rot_z(angle))
        prev = (px, py)
        last = travelled


def _poles(level):
    """Power poles on the land side, every 40 m; a sign now and then."""
    frames = _frames()
    travelled = 0.0
    next_pole = 20.0
    count = 0
    for i, ((x, y), d, r) in enumerate(frames):
        if i:
            travelled += math.hypot(x - frames[i - 1][0][0], y - frames[i - 1][0][1])
        if travelled < next_pole:
            continue
        next_pole += POLE_EVERY
        side = HALF_ROAD + SHOULDER + 0.8
        px, py = x - r[0] * side, y - r[1] * side
        heading = math.degrees(math.atan2(d[1], d[0]))
        level.place("utility_pole", px, py, heading - 90.0, z=height(px, py))
        count += 1
        if count % 5 == 2:
            sx, sy = x - r[0] * (side - 0.6), y - r[1] * (side - 0.6)
            level.place("signpost", sx, sy, heading - 90.0, z=height(sx, sy))


def build_coast_road(pieces, collision, materials, collection):
    level = Street(pieces, collision, materials, collection)
    m = kit.MeshBuilder(grid=4.0)
    _ground(m)
    _water(m)
    _road(m)
    _guardrail(m)
    level.add_visual("coast", m)
    _poles(level)

    start, end = LINE[0], LINE[-1]
    level.add_marker("spawn", "from_street", start[0], start[1], 0.0)
    level.add_marker("spawn", "from_shore", end[0], end[1], 180.0)
    # The bike has no marker: the ride places it every frame, and the level
    # file's yaw for it is only how its model is turned (it faces +-X).

    # A floor for the bike (and anyone who gets off); the ride needs no more.
    level.add_collider("coast", [((0.0, 450.0, -0.5), (400.0, 1000.0, 1.0))])

    # The ride's road, in game axes (x, z, -y), from the points the game
    # will sample the same way.
    level.markers_extra = {"ride_path": [[round(x, 3), 0.0, round(-y, 3)] for x, y in CONTROL]}
    return level
