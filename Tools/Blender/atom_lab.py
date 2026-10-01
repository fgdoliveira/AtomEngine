"""The character lab (M36): a studio for looking at a skinned character.

A model viewer as 2000s tools drew them: a pale floor with a measuring
grid, a seamless cyclorama curving up behind the subject (the level's fog
is the same colour, so the open sides dissolve into it), and a checkered
turntable the character stands on.

Drive mode (M38) adds things to walk on: two steps up to a platform, a
ramp to a higher one, crates, and a wall to back the camera into.

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


STEPS_Y = (-3.0, -1.0)            # the steps and their platform, along X
STEP = 0.15                       # rise of each step
RAMP_RISE, RAMP_RUN = 0.6, 3.0    # the ramp: about 11 degrees
RAMP_X = -3.5                     # where it starts, rising toward -X
CRATES = [((2.6, -6.2), 0.8, 0.0), ((-2.4, -7.4), 0.8, 0.0), ((-2.4, -7.4), 0.6, 0.8), ((5.5, -6.8), 1.0, 0.0)]
WALL = ((0.0, -9.2), (8.0, 0.3, 2.4))  # behind the spawn: the spring arm's test


def _obstacles(m):
    """Blocks to walk on; returns their collision boxes."""
    boxes = []
    y0, y1 = STEPS_Y
    yc, depth = (y0 + y1) / 2, y1 - y0
    # Two steps up to a platform, rising toward +X.
    for i, (xa, xb) in enumerate(((3.5, 4.1), (4.1, 4.7), (4.7, 7.0))):
        height = STEP * (i + 1)
        box = ((xa + xb) / 2, yc, height / 2), (xb - xa, depth, height)
        m.box(*box, "lab_block", faces=kit.NO_BOTTOM)
        boxes.append(box)
    # The ramp: a tilted slab, its top surface from (RAMP_X, 0) up to the
    # platform's edge; then the platform.
    angle = math.atan2(RAMP_RISE, RAMP_RUN)
    length, thickness = math.hypot(RAMP_RISE, RAMP_RUN) + 0.15, 0.2
    top = (RAMP_X - RAMP_RUN / 2, yc, RAMP_RISE / 2)
    normal = (math.sin(angle), 0.0, math.cos(angle))
    center = tuple(t - n * thickness / 2 for t, n in zip(top, normal))
    rotation = kit.rot_y(math.degrees(angle))
    # 2 cm narrower than the platform, so their sides don't share a plane
    # where the slab tucks under its edge (z-fighting; the lint catches it).
    m.box(center, (length, depth - 0.04, thickness), "lab_floor", rotation=rotation)
    boxes.append((center, (length, depth - 0.04, thickness), rotation))
    xa, xb = RAMP_X - RAMP_RUN, RAMP_X - RAMP_RUN - 2.0
    platform = (((xa + xb) / 2, yc, RAMP_RISE / 2), (xa - xb, depth, RAMP_RISE))
    m.box(*platform, "lab_block", faces=kit.NO_BOTTOM)
    boxes.append(platform)
    # Crates, one stacked.
    for (x, y), size, z in CRATES:
        box = ((x, y, z + size / 2), (size, size, size))
        m.box(*box, "lab_crate", faces="all" if z > 0 else kit.NO_BOTTOM)
        boxes.append(box)
    # The wall.
    (x, y), size = WALL
    box = ((x, y, size[2] / 2), size)
    m.box(*box, "lab_block", faces=kit.NO_BOTTOM)
    boxes.append(box)
    return boxes


def build_character_lab(pieces, collision, materials, collection):
    level = Street(pieces, collision, materials, collection)
    m = kit.MeshBuilder(grid=1.0)
    x0, y0, x1, y1 = FLOOR
    _flat(m, "lab_floor", x0, y0, x1, y1, 0.0)
    _cyclorama(m)
    _turntable(m)
    obstacles = _obstacles(m)
    level.add_visual("lab", m)

    level.add_marker("spawn", "start", 0.0, -6.0, 0.0)
    level.add_marker("entity", "rudy", 0.0, 0.0, 0.0, z=TURNTABLE_HEIGHT)

    level.add_collider("floor", [((0, (y0 + y1) / 2, -0.5), (x1 - x0, y1 - y0, 1.0)),
                                 ((0, 0, TURNTABLE_HEIGHT / 2), (2 * TURNTABLE_RADIUS * 0.9, 2 * TURNTABLE_RADIUS * 0.9,
                                                                  TURNTABLE_HEIGHT))])
    level.add_collider("obstacles", obstacles)
    level.add_collider("bounds", [((x0 - 0.5, (y0 + y1) / 2, 2), (1, y1 - y0, 4)),
                                  ((x1 + 0.5, (y0 + y1) / 2, 2), (1, y1 - y0, 4)),
                                  ((0, y1 + 0.5, 2), (x1 - x0, 1, 4)),
                                  ((0, y0 - 0.5, 2), (x1 - x0, 1, 4))])
    return level
