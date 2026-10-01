"""The character lab (M36): a studio for looking at a skinned character.

A model viewer as 2000s tools drew them: a pale floor with a measuring
grid, a seamless cyclorama curving up behind the subject (the level's fog
is the same colour, so the open sides dissolve into it), and a checkered
turntable the character stands on.

Blender Z-up; the subject stands at the origin facing -Y (glTF +Z), toward
the viewer's default camera.
"""

import math

import atom_kit as kit
from atom_street import Street

FLOOR = (-10.0, -10.0, 10.0, 2.0)  # x0, y0, x1, y1: up to the cove
COVE_RADIUS = 2.5
COVE_SEGMENTS = 10
WALL_TOP = 8.0
TURNTABLE_RADIUS = 1.4
TURNTABLE_HEIGHT = 0.05
TURNTABLE_SEGMENTS = 40


def _flat(m, material, x0, y0, x1, y1, z):
    tile = kit.tile_of(material)
    m.quad([(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], material,
           uvs=[(x0 / tile, y0 / tile), (x1 / tile, y0 / tile), (x1 / tile, y1 / tile), (x0 / tile, y1 / tile)])


def _cyclorama(m):
    """The cove: a quarter circle from the floor's back edge up to the back
    wall, smooth-shaded so floor and wall meet without a crease."""
    x0, _, x1, y_start = FLOOR
    profile = []  # (y, z) from the floor up
    for i in range(COVE_SEGMENTS + 1):
        t = 0.5 * math.pi * i / COVE_SEGMENTS
        profile.append((y_start + COVE_RADIUS * math.sin(t), COVE_RADIUS * (1.0 - math.cos(t))))
    profile.append((y_start + COVE_RADIUS, WALL_TOP))
    tile = kit.tile_of("lab_cyc")
    distance = 0.0
    for (ya, za), (yb, zb) in zip(profile, profile[1:]):
        step = math.hypot(yb - ya, zb - za)
        # Facing the studio (-Y and up): counter-clockwise seen from there.
        m.quad([(x0, ya, za), (x1, ya, za), (x1, yb, zb), (x0, yb, zb)], "lab_cyc",
               uvs=[(x0 / tile, distance / tile), (x1 / tile, distance / tile),
                    (x1 / tile, (distance + step) / tile), (x0 / tile, (distance + step) / tile)],
               smooth=True)
        distance += step


def _turntable(m):
    """A low disc: dark sides, a checker top mapped once across it."""
    m.cylinder((0.0, 0.0, 0.0), TURNTABLE_RADIUS, TURNTABLE_HEIGHT, "metal_dark",
               segments=TURNTABLE_SEGMENTS, caps=False)
    z = TURNTABLE_HEIGHT
    for i in range(TURNTABLE_SEGMENTS):
        a0 = 2.0 * math.pi * i / TURNTABLE_SEGMENTS
        a1 = 2.0 * math.pi * (i + 1) / TURNTABLE_SEGMENTS
        corners = [(0.0, 0.0, z),
                   (TURNTABLE_RADIUS * math.cos(a0), TURNTABLE_RADIUS * math.sin(a0), z),
                   (TURNTABLE_RADIUS * math.cos(a1), TURNTABLE_RADIUS * math.sin(a1), z)]
        uvs = [(0.5 + 0.5 * c[0] / TURNTABLE_RADIUS, 0.5 + 0.5 * c[1] / TURNTABLE_RADIUS) for c in corners]
        m.tri(corners, "lab_checker", uvs)


def build_character_lab(pieces, collision, materials, collection):
    level = Street(pieces, collision, materials, collection)
    m = kit.MeshBuilder(grid=1.0)
    x0, y0, x1, y1 = FLOOR
    _flat(m, "lab_floor", x0, y0, x1, y1, 0.0)
    _cyclorama(m)
    _turntable(m)
    level.add_visual("lab", m)

    level.add_marker("spawn", "start", 0.0, -6.0, 0.0)
    level.add_marker("entity", "rudy", 0.0, 0.0, 0.0, z=TURNTABLE_HEIGHT)

    level.add_collider("floor", [((0, (y0 + y1) / 2, -0.5), (x1 - x0, y1 - y0, 1.0)),
                                 ((0, 0, TURNTABLE_HEIGHT / 2), (2 * TURNTABLE_RADIUS * 0.9, 2 * TURNTABLE_RADIUS * 0.9,
                                                                  TURNTABLE_HEIGHT))])
    level.add_collider("bounds", [((x0 - 0.5, (y0 + y1) / 2, 2), (1, y1 - y0, 4)),
                                  ((x1 + 0.5, (y0 + y1) / 2, 2), (1, y1 - y0, 4)),
                                  ((0, y1 + 0.5, 2), (x1 - x0, 1, 4)),
                                  ((0, y0 - 0.5, 2), (x1 - x0, 1, 4))])
    return level
