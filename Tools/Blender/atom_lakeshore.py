"""The lakeshore lab (M48): a small lake in a meadow for stylized water and
weather presets. Outside the game's story, like the character lab:
reached with ATOM_START_LEVEL=lakeshore.

A round lake with a sandy shelf, reeds along the waterline, a jetty out
over the water, trees and rocks on the meadow, and low hills all round
beyond it. Lit live (the sun and its shadow map) with a sky bake for
occlusion only, so a weather preset can move the sun.

The water surface is one mesh at z = 0 whose first UV's u holds how deep
the water is there (0 at the shore, 1 at full depth), computed from the
same height function as the ground: the engine's water shader tints, foams
and fades it by that.

Blender Z-up; the lake's centre is north (+Y) of the spawn.
"""

import math

import atom_kit as kit
from atom_street import Street

CENTER = (0.0, 26.0)
RADIUS = 20.0          # the waterline
SHELF = 6.0            # metres from the waterline to full depth
DEPTH = 2.4
BEACH = 2.5            # metres from the waterline up to the meadow
MEADOW = 0.25          # the meadow's height
HILLS_FROM = 52.0      # distance from the centre where the hills start
GROUND = (-80.0, -54.0, 80.0, 106.0)
STEP = 1.5             # ground grid
JETTY_X = 1.0          # half width
JETTY_Y = (3.0, 18.0)
DECK = 0.5             # the deck's top
WALL_AT = RADIUS + 1.5 # the walkable edge, on the beach


def _smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def height(x, y):
    """The ground: the lake bowl, the beach, the meadow, the hills."""
    d = math.hypot(x - CENTER[0], y - CENTER[1])
    s = d - RADIUS
    if s < 0.0:
        return -DEPTH * _smooth(-s / SHELF)
    if s < BEACH:
        return MEADOW * _smooth(s / BEACH)
    hills = 0.0
    if d > HILLS_FROM:
        swell = 0.6 + 0.4 * math.sin(math.atan2(y - CENTER[1], x - CENTER[0]) * 5.0)
        hills = 7.0 * _smooth((d - HILLS_FROM) / 24.0) * swell
    return MEADOW + hills


def _grid(m, x0, y0, x1, y1, step, z_of, material_of, uv_of):
    """A grid sharing its vertices, so it shades smooth (and isn't split
    again by the builder's tessellation, which skips smooth faces). One
    piece for the lint: it never compares the grid with itself."""
    nx = int(round((x1 - x0) / step))
    ny = int(round((y1 - y0) / step))
    index = {}

    def vertex(i, j):
        if (i, j) not in index:
            x, y = x0 + i * step, y0 + j * step
            index[(i, j)] = m._add_vert((x, y, z_of(x, y)))
        return index[(i, j)]

    m._begin_piece()
    m._grouped = True
    for j in range(ny):
        for i in range(nx):
            cell = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]  # counter-clockwise from above
            points = [(x0 + a * step, y0 + b * step) for a, b in cell]
            material = material_of(points)
            if material is None:
                continue
            m.faces.append(([vertex(a, b) for a, b in cell], [uv_of(x, y, material) for x, y in points],
                            material, True))
    m._grouped = False
    m._tag_faces()


def _ground(m):
    def material(points):
        lowest = min(height(x, y) for x, y in points)
        return "sand" if lowest < MEADOW - 0.02 else "meadow"

    def uv(x, y, material):
        tile = kit.tile_of(material)
        return (x / tile, y / tile)

    _grid(m, *GROUND, STEP, height, material, uv)


def _water(m):
    """The surface, over every cell where the ground dips below it. Under
    the beach it carries on a little, hidden by the ground, so the
    waterline is exactly where they cross."""
    cx, cy = CENTER
    reach = RADIUS + 2.0

    def material(points):
        return "water" if min(height(x, y) for x, y in points) < 0.0 else None

    def uv(x, y, _):
        return (min(max(-height(x, y) / DEPTH, 0.0), 1.0), 0.0)

    step = 2.0
    _grid(m, cx - reach, cy - reach, cx + reach, cy + reach, step, lambda x, y: 0.0, material, uv)


def _jetty(m, boxes):
    y0, y1 = JETTY_Y
    length = y1 - y0
    m.box((0.0, (y0 + y1) / 2, DECK - 0.05), (2 * JETTY_X, length, 0.1), "wood_light")
    y = y0 + 1.0
    while y < y1:
        # Posts down into the lake bed, a cross beam under the deck.
        for x in (-JETTY_X + 0.1, JETTY_X - 0.1):
            bottom = min(height(x, y), 0.0) - 0.2
            m.cylinder((x, y, bottom), 0.09, DECK - 0.1 - bottom, "wood_dark", segments=8, caps=False)
        m.box((0.0, y, DECK - 0.17), (2 * JETTY_X + 0.1, 0.14, 0.12), "wood_dark")
        y += 2.5
    # A mooring post at the end, standing on the deck.
    m.cylinder((JETTY_X - 0.25, y1 - 0.3, DECK), 0.08, 0.7, "wood_dark", segments=8)
    boxes.append(((0.0, (y0 + y1) / 2, DECK - 0.75), (2 * JETTY_X, length, 1.5)))
    # Rails nobody sees: off the jetty is the lake.
    entry = CENTER[1] - WALL_AT
    for x in (-JETTY_X - 0.15, JETTY_X + 0.15):
        boxes.append(((x, (entry + y1) / 2, 1.5), (0.3, y1 - entry, 3.0)))
    boxes.append(((0.0, y1 + 0.15, 1.5), (2 * JETTY_X + 0.6, 0.3, 3.0)))


def _wall(a, b, height_=3.0, thickness=1.0):
    """A collision box standing on the segment a -> b (x, y), to its right."""
    length = math.hypot(b[0] - a[0], b[1] - a[1])
    angle = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    nx, ny = -(b[1] - a[1]) / length, (b[0] - a[0]) / length  # left of a -> b
    center = ((a[0] + b[0]) / 2 - nx * thickness / 2, (a[1] + b[1]) / 2 - ny * thickness / 2, height_ / 2)
    return (center, (length + 0.2, thickness, height_), kit.rot_z(angle))


def _shore_walls(boxes):
    """Around the lake, on the beach; open where the jetty starts."""
    cx, cy = CENTER
    segments = 24
    points = [(cx + WALL_AT * math.cos(math.radians(-90 + 15 * i)),
               cy + WALL_AT * math.sin(math.radians(-90 + 15 * i))) for i in range(segments)]
    # The points run counter-clockwise, so the lake is on each segment's
    # left: _wall(b, a) stands each box there, on the water side.
    for i in range(1, segments - 1):
        boxes.append(_wall(points[i + 1], points[i]))
    entry = points[0][1]
    boxes.append(_wall(points[1], (JETTY_X + 0.3, entry)))
    boxes.append(_wall((-JETTY_X - 0.3, entry), points[-1]))


def _reeds(m):
    """Clumps of tall grass along the waterline, swaying (the bake writes
    the grass's sway weights); none where the jetty meets the shore."""
    cx, cy = CENTER
    state = [4242]

    def rand():
        state[0] = (state[0] * 1103515245 + 12345) % 2 ** 31
        return state[0] / 2 ** 31

    for i in range(70):
        angle = 2.0 * math.pi * rand()
        if abs(math.degrees(angle) - 270.0) < 8.0:
            continue
        r = RADIUS + (rand() - 0.6) * 2.4
        x, y = cx + r * math.cos(angle), cy + r * math.sin(angle)
        z = height(x, y) - 0.05
        kit._cards(m, (x, y, z), 0.9 + 0.5 * rand(), 1.1 + 0.6 * rand(), "grass", count=3,
                   turn=360.0 * rand())


def _rocks(m):
    cx, cy = CENTER
    for angle, r, size, turn in ((200, RADIUS + 0.6, 1.1, 20), (215, RADIUS + 1.6, 0.6, 50),
                                 (20, RADIUS - 0.4, 1.4, 10), (35, RADIUS + 2.6, 0.7, 70),
                                 (120, RADIUS + 0.9, 0.9, 35), (320, RADIUS + 3.0, 1.2, 15)):
        x = cx + r * math.cos(math.radians(angle))
        y = cy + r * math.sin(math.radians(angle))
        z = height(x, y)
        m.box((x, y, z + size * 0.3), (size, size * 0.8, size * 0.75), "stone",
              rotation=kit.rot_z(turn) @ kit.rot_x(8), faces="all")


def build_lakeshore(pieces, collision, materials, collection):
    level = Street(pieces, collision, materials, collection)
    m = kit.MeshBuilder(grid=2.0)
    boxes = []
    _ground(m)
    _water(m)
    _jetty(m, boxes)
    _reeds(m)
    _rocks(m)
    level.add_visual("lakeshore", m)

    # Trees and bushes on the meadow, clear of the walkway to the jetty.
    for x, y, yaw in ((-14, 0, 30), (-21, 9, 80), (16, -2, 140), (24, 12, 200), (-25, 30, 10),
                      (27, 34, 250), (-12, 49, 60), (14, 50, 300), (-6, -10, 120), (9, -11, 0)):
        level.place("tree", x, y, yaw, z=MEADOW)
    for x, y, yaw in ((-8, 2, 0), (7, 1, 40), (-22, 20, 90), (25, 24, 10), (-4, 48, 30)):
        level.place("bush", x, y, yaw, z=MEADOW)

    level.add_marker("spawn", "start", 0.0, -6.0, 0.0, z=MEADOW)
    level.add_marker("spawn", "jetty_end", 0.0, JETTY_Y[1] - 1.0, 0.0, z=DECK)

    cx, cy = CENTER
    boxes.append(((cx, cy, MEADOW - 0.75), (200.0, 200.0, 1.5)))  # the meadow's floor
    _shore_walls(boxes)
    for a, b in (((-30, -14), (30, -14)), ((30, -14), (30, 54)), ((30, 54), (-30, 54)), ((-30, 54), (-30, -14))):
        boxes.append(_wall(a, b))  # the bounds, standing outside
    level.add_collider("lakeshore", boxes)
    return level
