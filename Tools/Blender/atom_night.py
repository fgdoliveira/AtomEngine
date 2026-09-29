"""Level E: the night city street (M25).

One short street that implies a city. The walkable part is four cells -
the bus stop, the main street, a narrow alley and the pachinko front - and
each cell is its own mesh, exported as a chunk with its own night
lightmap. The layout hides what isn't drawn: the alley's far end is a
wall, the plaza opens off it to the side, and an elevated railway crosses
over the street between the bus stop and the shops.

Same conventions as atom_street: Blender Z-up, glTF (x, y, z) -> game
(x, z, -y); fronts are written explicitly per building.

Light is almost all baked (street lamps, shop windows, the pachinko front,
neon spilling from the signs, a faint moon). What moves or flickers is
live and lives in the level file: the train and one failing sign.
"""

import math

import atom_kit as kit
import atom_lightmap
from atom_city import _style_uv as facade_uv
from atom_street import Street

SIDEWALK = 0.12  # kerb height, low enough to step onto
OFF = kit.DECAL_OFFSET

# Walkable cells on the ground plane, Blender (x0, y0, x1, y1); the level
# file repeats them in game coordinates (x, -y).
CELLS = {
    "bus_stop": (-34.0, -5.0, -18.0, 5.0),
    "main_street": (-18.0, -5.0, 22.0, 5.0),
    "alley": (4.5, 5.0, 7.5, 21.0),
    "pachinko_front": (7.5, 17.0, 26.0, 30.0),
}

# Lightmap size per cell (texels a side).
LIGHTMAP_SIZES = {"bus_stop": 512, "main_street": 1024, "alley": 512, "pachinko_front": 1024}
SAMPLES = 192

# Street lamps: (x, y, which way the arm points in degrees). The head
# hangs 1.05 m out along the arm (kit street_lamp).
LAMPS = [(-31.0, -3.4, 90), (-22.0, 3.4, -90), (-10.0, -3.4, 90), (0.0, 3.4, -90),
         (10.0, -3.4, 90), (19.0, 3.4, -90), (10.5, 20.0, 180), (23.0, 26.0, 180)]
LAMP_HEAD = (1.05, 4.3)  # out along the arm, height of the glass

WARM = (1.0, 0.78, 0.52)


def _flat(m, material, x0, y0, x1, y1, z, tile=None):
    tile = tile or kit.tile_of(material)
    m.quad([(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], material,
           uvs=[(x0 / tile, y0 / tile), (x1 / tile, y0 / tile), (x1 / tile, y1 / tile), (x0 / tile, y1 / tile)])


def _wall(m, material, a, b, z0, z1, uvs=None):
    """A vertical quad over the ground line a -> b, between heights z0 and
    z1. Its front is on the right of a -> b seen from above; u runs from a
    to b (a is on the left seen from the front)."""
    (ax, ay), (bx, by) = a, b
    if uvs is None:
        tile = kit.tile_of(material)
        length = math.hypot(bx - ax, by - ay)
        uvs = [(0, z0 / tile), (length / tile, z0 / tile), (length / tile, z1 / tile), (0, z1 / tile)]
    m.quad([(ax, ay, z0), (bx, by, z0), (bx, by, z1), (ax, ay, z1)], material, uvs=uvs)


# The four sides of a block: the ground-line segment each face runs along,
# walked so that the outside is on the right (counter-clockwise from above).
def _sides(x0, x1, y0, y1):
    return {
        "-y": ((x0, y0), (x1, y0)),
        "+x": ((x1, y0), (x1, y1)),
        "+y": ((x1, y1), (x0, y1)),
        "-x": ((x0, y1), (x0, y0)),
    }


class Block:
    """A building: `front` gets shops on the ground floor (shopfront atlas
    quadrants in `shops`, one per unit) and lit windows above (facade
    atlas quadrant `upper`); the `plain` sides get concrete. Other sides
    are never seen and aren't built. Also its collision box."""

    def __init__(self, x0, x1, y0, y1, height, front, shops=(), upper=0, plain=(), base=0.0):
        self.x0, self.x1, self.y0, self.y1 = x0, x1, y0, y1
        self.height, self.front, self.shops, self.upper, self.plain = height, front, shops, upper, plain
        self.base = base

    def build(self, m):
        sides = _sides(self.x0, self.x1, self.y0, self.y1)
        a, b = sides[self.front]
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        ground = 4.0
        z0 = self.base

        def at(t):
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)

        # Ground floor: one shop unit per entry in `shops`.
        units = max(1, len(self.shops))
        for i in range(units):
            style = self.shops[i] if self.shops else 3
            u0, v0 = (style % 2) * 0.5, (style // 2) * 0.5
            _wall(m, "shopfront_atlas", at(i / units), at((i + 1) / units), z0, ground,
                  uvs=[(u0, v0), (u0 + 0.5, v0), (u0 + 0.5, v0 + 0.5), (u0, v0 + 0.5)])
        # Upper floors: lit windows, a quadrant of the facade atlas per
        # stretch of up to 8 m, about 3 m a floor.
        if self.height > ground:
            stretches = max(1, math.ceil(length / 8.0 - 1e-6))
            fu0, fv0, fu1, fv1 = facade_uv(self.upper)
            top_v = fv0 + (fv1 - fv0) * min(1.0, (self.height - ground) / 24.0)
            for i in range(stretches):
                _wall(m, "facade_atlas", at(i / stretches), at((i + 1) / stretches), ground, self.height,
                      uvs=[(fu0, fv0), (fu1, fv0), (fu1, top_v), (fu0, top_v)])
        for side in self.plain:
            pa, pb = sides[side]
            _wall(m, "concrete", pa, pb, z0, self.height)
        # A parapet along the front, so the roofline has some depth.
        nx, ny = (b[1] - a[1]) / length, -(b[0] - a[0]) / length  # outward normal
        cx, cy = (a[0] + b[0]) / 2 + nx * 0.15, (a[1] + b[1]) / 2 + ny * 0.15
        size = (abs(b[0] - a[0]) + 0.3 * abs(nx), abs(b[1] - a[1]) + 0.3 * abs(ny), 0.4)
        m.box((cx, cy, self.height + 0.2), size, "concrete", faces=[(round(nx), round(ny), 0), (0, 0, 1)])

    def collider(self):
        return (((self.x0 + self.x1) / 2, (self.y0 + self.y1) / 2, self.height / 2),
                (self.x1 - self.x0, self.y1 - self.y0, self.height))


def _vertical_sign(m, x, y, facing_y, material, z0=3.4, z1=6.6):
    """A vertical sign sticking out from a wall that faces +Y (facing_y=1)
    or -Y (-1): lettered on both faces, on a bracket."""
    out0, out1 = y + facing_y * 0.35, y + facing_y * 0.95
    ya, yb = min(out0, out1), max(out0, out1)
    for x_face, flip in ((x - 0.03, True), (x + 0.03, False)):
        ys = (yb, ya) if flip else (ya, yb)
        m.quad([(x_face, ys[0], z0), (x_face, ys[1], z0), (x_face, ys[1], z1), (x_face, ys[0], z1)], material)
    m.box((x, (ya + yb) / 2, z1 + 0.03), (0.08, yb - ya + 0.04, 0.06), "metal_dark")
    m.box((x, (ya + yb) / 2, z0 - 0.03), (0.08, yb - ya + 0.04, 0.06), "metal_dark")
    m.box((x, y + facing_y * 0.18, z1 + 0.1), (0.06, 0.36, 0.06), "metal_dark")


def _reflection(m, x, y_from, y_to, width, column, z=0.0):
    """A neon reflection decal on wet ground: a streak from under the light
    (y_from) outward (y_to), in one colour column of the texture."""
    u0, u1 = column / 4.0, (column + 1) / 4.0
    ya, yb = sorted((y_from, y_to))
    flip = y_from > y_to
    uvs = [(u0, 1), (u1, 1), (u1, 0), (u0, 0)] if flip else [(u0, 0), (u1, 0), (u1, 1), (u0, 1)]
    m.quad([(x - width / 2, ya, z + OFF), (x + width / 2, ya, z + OFF),
            (x + width / 2, yb, z + OFF), (x - width / 2, yb, z + OFF)], "neon_reflection", uvs=uvs)


def _puddle(m, x, y, sx, sy, z=0.0):
    m.quad([(x - sx, y - sy, z + 2 * OFF), (x + sx, y - sy, z + 2 * OFF),
            (x + sx, y + sy, z + 2 * OFF), (x - sx, y + sy, z + 2 * OFF)], "puddle")


def _street_floor(m, x0, x1):
    """Road and raised sidewalks between x0 and x1, with kerbs."""
    _flat(m, "wet_asphalt", x0, -3.0, x1, 3.0, 0.0)
    for side in (-1, 1):
        y_in, y_out = 3.0 * side, 5.0 * side
        ya, yb = sorted((y_in, y_out))
        _flat(m, "concrete", x0, ya, x1, yb, SIDEWALK)
        # Kerb face toward the road.
        if side > 0:
            _wall(m, "concrete", (x0, 3.0), (x1, 3.0), 0.0, SIDEWALK)
        else:
            _wall(m, "concrete", (x1, -3.0), (x0, -3.0), 0.0, SIDEWALK)


# --------------------------------------------------------------------------
# The street
# --------------------------------------------------------------------------

# Buildings per cell. Main-street rows: south fronts face +y at y = -5,
# north fronts face -y at y = 5. Shop styles: 0 ramen, 1 convenience
# store, 2 bar, 3 shuttered; upper: facade atlas quadrant 0-2.
BLOCKS = {
    "bus_stop": [
        Block(-48, -30, -15, -5, 8, "+y", shops=(3, 3, 3, 1), upper=1, plain=("-x",)),
        Block(-30, -21, -15, -5, 10, "+y", shops=(1, 3), upper=2, plain=("+x",)),
        Block(-48, -28, 5, 14, 9, "-y", shops=(3, 2, 3, 3), upper=1, plain=("-x",)),
        Block(-28, -21, 5, 14, 7, "-y", shops=(3,), upper=0, plain=("+x",)),
    ],
    "main_street": [
        Block(-15, -6, -15, -5, 12, "+y", shops=(0, 0), upper=0, plain=("-x",)),
        Block(-6, 2, -15, -5, 9, "+y", shops=(1, 1), upper=1),
        Block(2, 10, -15, -5, 14, "+y", shops=(2, 3), upper=2),
        Block(10, 22, -15, -5, 11, "+y", shops=(0, 2, 3), upper=0),
        Block(-15, -5, 5, 14, 11, "-y", shops=(2, 0), upper=2, plain=("-x",)),
        Block(-5, 4.5, 5, 14, 13, "-y", shops=(1, 3), upper=0, plain=("+x",)),
        Block(7.5, 15, 5, 14, 12, "-y", shops=(0, 2), upper=1, plain=("-x",)),
        Block(15, 22, 5, 14, 10, "-y", shops=(3, 2), upper=2),
        # The east end: a narrow, taller building across the street.
        Block(22, 30, -5, 5, 14, "-x", shops=(2, 0, 1), upper=1, plain=("-y", "+y")),
    ],
    "alley": [
        Block(-6, 7.5, 21, 34, 10, "+x", shops=(3, 0), upper=2, plain=("-y",)),   # its south side ends the alley
        Block(-6, 4.5, 14, 21, 8, "+x", shops=(), upper=1),                       # alley west wall
        Block(7.5, 26, 14, 17, 7, "+y", shops=(3, 2, 3, 0), upper=0, plain=("-x",)),
    ],
    "pachinko_front": [
        Block(26, 36, 14, 40, 11, "-x", shops=(2, 3, 0, 3, 1), upper=1),
    ],
}


def _bus_stop(m):
    _street_floor(m, -34.0, -18.0)
    # Under the railway: a fenced lot on each side.
    for y0, y1 in ((5.0, 14.0), (-14.0, -5.0)):
        _flat(m, "concrete", -21.0, y0, -15.0, y1, 0.0)
    # The shelter: back panel with posters, a roof, a bench, and the
    # back-lit timetable on its own post.
    x0, x1, yb = -28.0, -24.0, -4.75
    m.box(((x0 + x1) / 2, yb, SIDEWALK + 1.2), (x1 - x0, 0.06, 2.2), "metal_white")
    for x in (x0, x1):
        m.box((x, yb + 0.45, SIDEWALK + 1.25), (0.08, 1.0, 2.5), "metal_dark", faces=kit.SIDES)
    m.box(((x0 + x1) / 2, yb + 0.5, SIDEWALK + 2.45), (x1 - x0 + 0.3, 1.2, 0.1), "metal_dark")
    m.box(((x0 + x1) / 2, yb + 0.3, SIDEWALK + 0.45), (3.0, 0.4, 0.06), "wood_light")
    for x in (x0 + 0.6, x1 - 0.6):
        m.box((x, yb + 0.3, SIDEWALK + 0.21), (0.06, 0.3, 0.42), "metal_dark")
    m.quad([(x1 - 0.3, yb + 0.03 + OFF, SIDEWALK + 0.6), (x0 + 0.3, yb + 0.03 + OFF, SIDEWALK + 0.6),
            (x0 + 0.3, yb + 0.03 + OFF, SIDEWALK + 2.0), (x1 - 0.3, yb + 0.03 + OFF, SIDEWALK + 2.0)], "posters")
    # Chain-link fences in front of the lots under the railway.
    for y, (xa, xb) in ((5.05, (-15.0, -21.0)), (-5.05, (-21.0, -15.0))):
        m.quad([(xa, y, 0.0), (xb, y, 0.0), (xb, y, 2.0), (xa, y, 2.0)], "chain_link",
               uvs=[(0, 0), (12, 0), (12, 4), (0, 4)])
    m.cylinder((-22.5, -3.35, SIDEWALK), 0.05, 2.4, "metal_dark", segments=8)
    m.box((-22.5, -3.35, SIDEWALK + 1.55), (0.5, 0.06, 0.7), "metal_dark", faces=kit.SIDES[:2] + [(0, 0, 1), (0, 0, -1)])
    for y, normal_y in ((-3.39, -1), (-3.31, 1)):
        xs = (-22.25, -22.75) if normal_y > 0 else (-22.75, -22.25)
        m.quad([(xs[0], y, SIDEWALK + 1.22), (xs[1], y, SIDEWALK + 1.22),
                (xs[1], y, SIDEWALK + 1.88), (xs[0], y, SIDEWALK + 1.88)], "timetable")
    m.cylinder((-22.5, -3.35, SIDEWALK + 2.4), 0.25, 0.04, "metal_white", segments=12)  # the bus stop disc
    _vertical_sign(m, -25.0, 5.0, -1, "neon_sign")


def _main_street(m):
    _street_floor(m, -18.0, 22.0)
    # The vending corner by the alley, the phone box, bins and bicycles.
    x = -12.0
    for dx in (-0.45, 0.45):
        m.box((x + dx, 4.4, SIDEWALK + 1.1), (0.05, 0.05, 2.2), "metal_white")
    m.box((x, 4.4, SIDEWALK + 2.25), (1.0, 1.0, 0.1), "metal_white")
    m.box((x, 4.85, SIDEWALK + 1.1), (0.9, 0.05, 2.2), "metal_white", faces=[(0, -1, 0)])
    m.quad([(x - 0.4, 3.92, SIDEWALK + 2.0), (x + 0.4, 3.92, SIDEWALK + 2.0),
            (x + 0.4, 3.92, SIDEWALK + 2.18), (x - 0.4, 3.92, SIDEWALK + 2.18)], "lamp_glass")
    m.box((x, 4.7, SIDEWALK + 1.2), (0.3, 0.15, 0.4), "black")  # the phone
    for bx in (-4.2, -3.6):
        m.box((bx, 4.6, SIDEWALK + 0.45), (0.5, 0.5, 0.9), "metal_dark")
    for i, bx in enumerate((12.0, 12.7, 13.4)):
        _bicycle(m, bx, -4.4, SIDEWALK, lean=(i - 1) * 4)
    # Notice board on the convenience store's wall, torn posters.
    m.box((-2.0, -4.94, SIDEWALK + 1.5), (2.2, 0.06, 1.3), "wood_dark", faces=[(0, 1, 0)])
    m.quad([(-1.1, -4.91 + OFF, SIDEWALK + 1.0), (-2.9, -4.91 + OFF, SIDEWALK + 1.0),
            (-2.9, -4.91 + OFF, SIDEWALK + 2.0), (-1.1, -4.91 + OFF, SIDEWALK + 2.0)], "posters")
    # Signs: vertical neon on brackets; one amber sign fails (live).
    _vertical_sign(m, -12.5, -5.0, 1, "neon_sign")
    _vertical_sign(m, -3.0, 5.0, -1, "neon_sign")
    _vertical_sign(m, 8.0, -5.0, 1, "neon_amber")
    _vertical_sign(m, 13.0, 5.0, -1, "neon_sign")
    _vertical_sign(m, 19.5, -5.0, 1, "neon_sign")
    # Big signs on the building closing the street, facing down it.
    for y in (-3.4, 3.4):
        m.quad([(21.7, y + 0.4, 4.6), (21.7, y - 0.4, 4.6), (21.7, y - 0.4, 11.0), (21.7, y + 0.4, 11.0)], "neon_sign")
        m.box((21.85, y, 11.05), (0.3, 0.9, 0.08), "metal_dark")
    # Air conditioners on the upper floors.
    for ax, face in ((-9.0, 1), (5.0, 1), (-8.0, -1), (17.0, -1), (0.0, 1)):
        wall = [(0, -face, 0)]  # the side against the wall isn't built
        m.box((ax, -face * 4.75, 6.2), (0.8, 0.5, 0.6), "metal_white",
              faces=[f for f in kit.SIDES + [(0, 0, 1), (0, 0, -1)] if f not in wall])


def _bicycle(m, x, y, z, lean=0):
    """A parked bicycle along X: two thin wheels, a frame, handlebars."""
    for dx in (-0.5, 0.5):
        m.box((x + dx, y, z + 0.33), (0.62, 0.03, 0.62), "black", faces=kit.SIDES[2:])
    m.box((x, y, z + 0.55), (1.0, 0.04, 0.04), "metal_dark", rotation=kit.rot_x(lean))
    m.box((x - 0.15, y, z + 0.8), (0.25, 0.1, 0.05), "black")
    m.box((x + 0.45, y, z + 0.95), (0.05, 0.5, 0.03), "metal_dark")


def _alley(m):
    _flat(m, "concrete", 4.5, 5.0, 7.5, 21.0, SIDEWALK)
    # Pipes and cables on the walls, bins, a crate stack, a tiny shrine.
    for x, z in ((4.62, 3.2), (4.62, 5.5), (7.38, 4.4)):
        m.box((x, 13.0, z), (0.12, 16.0, 0.12), "metal_dark", faces=kit.SIDES[:2] + [(0, 0, 1), (0, 0, -1)])
    for y in (8.0, 9.2):
        m.box((5.0, y, SIDEWALK + 0.5), (0.7, 0.7, 1.0), "metal_dark")
    m.box((7.1, 15.5, SIDEWALK + 0.35), (0.6, 0.8, 0.7), "wood_light")
    m.box((7.1, 15.5, SIDEWALK + 0.95), (0.5, 0.6, 0.5), "wood_light")
    sx, sy = 4.85, 18.0
    m.box((sx, sy, SIDEWALK + 0.9), (0.4, 0.5, 0.5), "wood_dark")
    m.box((sx, sy, SIDEWALK + 1.2), (0.55, 0.65, 0.08), "roof_tile")
    m.quad([(sx + 0.21, sy - 0.12, SIDEWALK + 0.75), (sx + 0.21, sy + 0.12, SIDEWALK + 0.75),
            (sx + 0.21, sy + 0.12, SIDEWALK + 1.08), (sx + 0.21, sy - 0.12, SIDEWALK + 1.08)], "ofuda")
    # The wall lamp: a bare bulb.
    m.box((4.62, 12.0, 3.6), (0.12, 0.25, 0.25), "lamp_glass")
    # Posters on the west wall, bicycles against the east one.
    m.quad([(4.5 + OFF, 6.5, SIDEWALK + 0.6), (4.5 + OFF, 7.9, SIDEWALK + 0.6),
            (4.5 + OFF, 7.9, SIDEWALK + 1.8), (4.5 + OFF, 6.5, SIDEWALK + 1.8)], "posters")
    for y in (10.5, 11.3):
        _bicycle_y(m, 6.9, y, SIDEWALK)


def _bicycle_y(m, x, y, z):
    """A bicycle parked along Y, against the alley's east wall."""
    for dy in (-0.5, 0.5):
        m.box((x, y + dy, z + 0.33), (0.03, 0.62, 0.62), "black", faces=kit.SIDES[:2])
    m.box((x, y, z + 0.55), (0.04, 1.0, 0.04), "metal_dark")
    m.box((x, y - 0.15, z + 0.8), (0.1, 0.25, 0.05), "black")
    m.box((x, y + 0.45, z + 0.95), (0.5, 0.05, 0.03), "metal_dark")


def _pachinko_front(m):
    _flat(m, "concrete", 7.5, 17.0, 26.0, 30.0, SIDEWALK)
    # The hall: its whole front one lit facade, a sign tower on the roof.
    _wall(m, "pachinko_front", (7.5, 30.0), (26.0, 30.0), SIDEWALK, 9.0,
          uvs=[(0, 0), (1, 0), (1, 1), (0, 1)])
    m.box((16.75, 30.15, 9.2), (18.8, 0.3, 0.4), "concrete", faces=[(0, -1, 0), (0, 0, 1)])
    # The roof sign: a vertical sign's lettering laid on its side.
    m.quad([(11.0, 30.1, 9.5), (22.5, 30.1, 9.5), (22.5, 30.1, 12.3), (11.0, 30.1, 12.3)], "neon_sign",
           uvs=[(1, 0), (1, 1), (0, 1), (0, 0)])
    m.box((16.75, 30.3, 10.9), (11.5, 0.3, 2.8), "metal_dark", faces=[(0, 1, 0), (0, 0, 1), (1, 0, 0), (-1, 0, 0)])
    # Planters and a bench across the plaza, a vending machine by the door.
    for x in (11.0, 21.5):
        m.box((x, 24.0, SIDEWALK + 0.3), (2.4, 0.9, 0.6), "concrete")
        m.box((x, 24.0, SIDEWALK + 0.61), (2.2, 0.7, 0.02), "dirt", faces=[(0, 0, 1)])


def _plaza_decals(m):
    # The hall's light on the wet plaza.
    for x, column in ((10.0, 0), (13.5, 2), (17.0, 3), (20.5, 0), (24.0, 2)):
        _reflection(m, x, 29.9, 25.5, 2.4, column, z=SIDEWALK)
    _puddle(m, 14.0, 21.0, 1.6, 0.9, z=SIDEWALK)
    _puddle(m, 20.0, 27.5, 1.0, 0.6, z=SIDEWALK)


def _street_decals(m):
    """Wet-road decals over the street: sign reflections streaking toward
    the middle of the road, shop windows on the sidewalks, puddles, the
    centre line."""
    signs = [(-25.0, 3.0, 0), (-12.5, -3.0, 1), (-3.0, 3.0, 0), (8.0, -3.0, 2), (13.0, 3.0, 1), (19.5, -3.0, 0)]
    for x, y, column in signs:
        _reflection(m, x, y, y * 0.15, 1.4, column)
    for block in BLOCKS["main_street"][:8] + BLOCKS["bus_stop"]:
        facing = 1 if block.front == "+y" else -1
        wall_y = block.y1 if facing > 0 else block.y0
        units = len(block.shops)
        for i, style in enumerate(block.shops):
            if style == 3:
                continue
            # Units run along the front from its left seen from the street.
            t = (i + 0.5) / units
            x = block.x1 - (block.x1 - block.x0) * t if facing > 0 else block.x0 + (block.x1 - block.x0) * t
            _reflection(m, x, wall_y, wall_y + facing * 1.9, (block.x1 - block.x0) / units * 0.7,
                        3 if style != 2 else 0, z=SIDEWALK)
    for x, y, sx, sy in ((-26.0, 1.2, 1.4, 0.7), (-14.0, -1.5, 2.0, 1.0), (-4.0, 1.8, 1.2, 0.6),
                         (6.0, -0.8, 1.8, 0.9), (15.0, 1.0, 1.5, 0.8), (-19.0, -1.0, 1.1, 0.6)):
        _puddle(m, x, y, sx, sy)
    for x in range(-60, 20, 6):
        m.quad([(x, -0.06, OFF), (x + 3.0, -0.06, OFF), (x + 3.0, 0.06, OFF), (x, 0.06, OFF)], "road_paint",
               uvs=[(0, 0), (1, 0), (1, 1), (0, 1)])


def _base(m):
    """Always drawn: the road running on west into the dark, the elevated
    railway over the street, cables."""
    _street_floor(m, -120.0, -34.0)
    # The railway: a deck on pillars, north to south across the street.
    x0, x1, deck = -21.0, -15.0, 7.0
    m.box(((x0 + x1) / 2, 0.0, deck + 0.6), (x1 - x0, 240.0, 1.2), "concrete",
          faces=[(1, 0, 0), (-1, 0, 0), (0, 0, -1), (0, 0, 1)])
    for x in (x0 + 0.15, x1 - 0.15):
        m.box((x, 0.0, deck + 1.7), (0.3, 240.0, 1.0), "concrete", faces=kit.SIDES[:2] + [(0, 0, 1)])
    for y in range(-112, 120, 15):
        if abs(y) < 7:
            continue
        m.box(((x0 + x1) / 2, y + 0.5, deck / 2), (1.4, 1.4, deck), "concrete", faces=kit.SIDES)
    for x in (-18.9, -17.1):
        m.box((x, 0.0, deck + 1.28), (0.08, 240.0, 0.08), "metal_dark", faces=[(0, 0, 1), (1, 0, 0), (-1, 0, 0)])
    # Cables sagging across the street between the rooftops.
    for x, z in ((-27.0, 6.5), (-9.0, 7.0), (-8.6, 6.6), (4.0, 7.5), (16.0, 6.8), (16.4, 7.1)):
        points = []
        for i in range(11):
            t = i / 10
            points.append((x, -5.0 + 10.0 * t, z - 1.6 * t * (1 - t)))
        for p, q in zip(points, points[1:]):
            dy, dz = q[1] - p[1], q[2] - p[2]
            angle = math.degrees(math.atan2(dz, dy))
            m.box(((p[0] + q[0]) / 2, (p[1] + q[1]) / 2, (p[2] + q[2]) / 2), (0.03, math.hypot(dy, dz), 0.03),
                  "black", rotation=kit.rot_x(angle))


def _bake_lights():
    """Every bake-only light of the street; each cell is baked with all of
    them (a lamp near a border lights both sides the same)."""
    lights = []
    for x, y, arm in LAMPS:
        hx = x + LAMP_HEAD[0] * math.cos(math.radians(arm))
        hy = y + LAMP_HEAD[0] * math.sin(math.radians(arm))
        lights.append(atom_lightmap.point_light((hx, hy, LAMP_HEAD[1] - 0.1), 450.0, WARM))
    lights.append(atom_lightmap.point_light((4.85, 12.0, 3.5), 120.0, (1.0, 0.85, 0.6)))  # alley bulb
    # Shop windows: an area light just outside each lit unit, facing out.
    interior = {0: (1.0, 0.78, 0.5), 1: (0.9, 0.97, 1.0), 2: (1.0, 0.4, 0.32)}
    watts = {0: 90.0, 1: 160.0, 2: 60.0}
    for cell, blocks in BLOCKS.items():
        for block in blocks:
            units = len(block.shops)
            for i, style in enumerate(block.shops):
                if style == 3:
                    continue
                t = (i + 0.5) / units
                width = 0.8 * (abs(block.x1 - block.x0) if block.front in ("+y", "-y") else abs(block.y1 - block.y0)) / units
                if block.front == "+y":
                    location, rotation = (block.x1 - (block.x1 - block.x0) * t, block.y1 + 0.3, 2.0), (90, 0, 0)
                elif block.front == "-y":
                    location, rotation = (block.x0 + (block.x1 - block.x0) * t, block.y0 - 0.3, 2.0), (-90, 0, 0)
                elif block.front == "+x":
                    location, rotation = (block.x1 + 0.3, block.y0 + (block.y1 - block.y0) * t, 2.0), (0, -90, 0)
                else:
                    location, rotation = (block.x0 - 0.3, block.y1 - (block.y1 - block.y0) * t, 2.0), (0, 90, 0)
                lights.append((location, rotation, (width, 2.4), watts[style], interior[style]))
    # The pachinko front floods the plaza.
    for x, color in ((11.0, (1.0, 0.55, 0.35)), (16.75, (1.0, 0.45, 0.7)), (22.5, (1.0, 0.75, 0.4))):
        lights.append(((x, 29.6, 2.5), (-90, 0, 0), (5.5, 4.0), 700.0, color))
    # Vending machines (entities) glow onto the sidewalk.
    for x in (0.9, 2.1):
        lights.append(((x, 4.0, 1.1), (-90, 0, 0), (0.9, 1.6), 35.0, (0.8, 0.9, 1.0)))
    lights.append(((12.2, 29.5, 1.1), (-90, 0, 0), (0.9, 1.6), 35.0, (0.8, 0.9, 1.0)))
    # A thin moon through the haze.
    lights.append(atom_lightmap.sun_light((50, 0, 35), 0.04, (0.55, 0.62, 0.9), angle=2.0))
    return lights


NIGHT_SKY = ((0.05, 0.06, 0.11), 0.25)  # a faint sky fill for the bake


def build_night_street(pieces, collision, materials, collection):
    """Level E. Returns the Street holding one visual per cell (in CELLS
    order), then the always-drawn base and decals, and the collision."""
    level = Street(pieces, collision, materials, collection)

    builders = {"bus_stop": _bus_stop, "main_street": _main_street, "alley": _alley,
                "pachinko_front": _pachinko_front}
    level.cells = {}
    for cell, build in builders.items():
        m = kit.MeshBuilder(grid=4.0)  # lit by its lightmap: few vertices needed
        build(m)
        for block in BLOCKS[cell]:
            block.build(m)
        level.cells[cell] = level.add_visual("night_" + cell, m)

    base = kit.MeshBuilder(grid=8.0)
    _base(base)
    level.base = level.add_visual("night_base", base)
    decals = kit.MeshBuilder(grid=100.0)
    _street_decals(decals)
    _plaza_decals(decals)
    level.decals = level.add_visual("night_decals", decals)

    # Where things are (markers, M20); night_street.json says what they do.
    level.add_marker("spawn", "from_bus", -26.0, -3.6, -90, z=SIDEWALK)
    level.add_marker("spawn", "start", -30.0, 0.0, -90)
    for i, (x, y, arm) in enumerate(LAMPS):
        level.add_marker("entity", f"lamp_{i + 1}", x, y, arm, z=SIDEWALK)
    level.add_marker("entity", "vending_1", 0.9, 4.55, 0, z=SIDEWALK)
    level.add_marker("entity", "vending_2", 2.1, 4.55, 0, z=SIDEWALK)
    level.add_marker("entity", "vending_plaza", 12.2, 29.55, 0, z=SIDEWALK)
    level.add_marker("entity", "timetable", -22.5, -3.35, z=SIDEWALK)
    level.add_marker("entity", "phone_box", -12.0, 4.4, z=SIDEWALK)
    level.add_marker("entity", "notice_board", -2.0, -4.9, z=SIDEWALK)
    level.add_marker("entity", "ramen_shop", -10.5, -5.0, z=SIDEWALK)
    level.add_marker("entity", "shutter", 8.0, -5.0, z=SIDEWALK)
    level.add_marker("entity", "bicycles", 12.7, -4.4, z=SIDEWALK)
    level.add_marker("entity", "alley_shrine", 4.85, 18.0, z=SIDEWALK)
    level.add_marker("entity", "pachinko_doors", 16.75, 30.0, z=SIDEWALK)
    level.add_marker("entity", "train", -18.0, 150.0, 0, z=7.0 + 1.2)

    # Collision: the ground, kerbs, every building, the lots' fences, the
    # shelter, the planters, and a wall across the road's west end.
    boxes = [((-35.0, 10.0, -0.5), (140.0, 70.0, 1.0))]
    for side in (-1, 1):
        boxes.append(((-6.0, 4.0 * side, SIDEWALK / 2), (56.0, 2.0, SIDEWALK)))
    boxes.append(((6.0, 13.0, SIDEWALK / 2), (3.0, 16.0, SIDEWALK)))
    boxes.append(((16.75, 23.5, SIDEWALK / 2), (18.5, 13.0, SIDEWALK)))
    for blocks in BLOCKS.values():
        boxes.extend(block.collider() for block in blocks)
    boxes.append(((16.75, 35.0, 4.5), (18.5, 10.0, 9.0)))            # the pachinko hall
    for side in (-1, 1):
        boxes.append(((-18.0, 5.1 * side, 1.0), (6.0, 0.2, 2.0)))   # fences in front of the lots
    boxes.append(((-26.0, -4.75, 1.3), (4.2, 0.2, 2.6)))              # shelter back
    boxes.append(((-26.0, -4.45, 0.35), (3.0, 0.4, 0.7)))             # bench
    boxes.append(((-34.5, 0.0, 2.0), (1.0, 12.0, 4.0)))               # the road goes on without you
    boxes.append(((-12.0, 4.4, 1.2), (1.0, 1.0, 2.4)))                # phone box (entered from the front: blocked)
    for x in (11.0, 21.5):
        boxes.append(((x, 24.0, 0.35), (2.4, 0.9, 0.7)))
    for y in (8.0, 9.2):
        boxes.append(((5.0, y, 0.6), (0.7, 0.7, 1.2)))
    boxes.append(((7.1, 15.5, 0.6), (0.6, 0.8, 1.2)))
    level.add_collider("night_street_col", boxes)
    return level


def bake(scene, level, out_dir):
    """Bakes a lightmap per cell, with every cell and the base in the scene
    (shadows, bounce and sign light cross the borders)."""
    lights = _bake_lights()
    everything = list(level.cells.values()) + [level.base]
    for cell, obj in level.cells.items():
        others = [o for o in everything if o is not obj]
        atom_lightmap.bake(scene, obj, lights, f"{out_dir}/night_{cell}_lm.png",
                           size=LIGHTMAP_SIZES[cell], samples=SAMPLES, context=others, sky=NIGHT_SKY,
                           clamp=2.0)
