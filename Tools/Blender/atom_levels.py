"""Level B (shrine grounds) and level C (machiya interior).

Same conventions as atom_street: Blender Z-up, glTF converts (x, y, z) to
(x, z, -y), kit fronts face -Y. Each level is built around its own origin
and exported to its own pair of files.
"""

import math

import atom_kit as kit
from atom_street import Street


def _flat(m, material, x0, y0, x1, y1, z):
    tile = kit.tile_of(material)
    m.quad(
        [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)],
        material,
        uvs=[(x0 / tile, y0 / tile), (x1 / tile, y0 / tile),
             (x1 / tile, y1 / tile), (x0 / tile, y1 / tile)],
    )


# --------------------------------------------------------------------------
# Level B: shrine grounds
# --------------------------------------------------------------------------

def build_shrine_grounds(pieces, collision, materials, collection):
    """A walled precinct: gate at the south, a stone path through a second
    torii between lanterns and cedars, up to the main hall."""
    level = Street(pieces, collision, materials, collection)
    half_width, south, north = 12.0, -20.0, 24.0

    m = kit.MeshBuilder()
    _flat(m, "gravel", -40, -40, 40, 50, 0.0)
    _flat(m, "stone", -0.8, south + 0.4, 0.8, 14.3, 0.005)       # the approach
    level.add_visual("grounds", m)

    # Enclosure: stone walls on three sides, gate and walls to the south.
    for y in range(int(south) + 2, int(north), 4):
        level.place("stone_wall", -half_width, y, 90)
        level.place("stone_wall", half_width, y, 90)
    for x in range(int(-half_width) + 2, int(half_width), 4):
        level.place("stone_wall", x, north, 180)
    level.place("shrine_gate", 0.0, south, 0)
    for x in (-11.5, -7.5, -3.5, 3.5, 7.5, 11.5):
        level.place("stone_wall", x, south, 0)

    level.place("torii", 0.0, -12.0, 0)
    for y in (-6.0, 2.0, 8.0):
        for x in (-2.5, 2.5):
            level.place("toro", x, y, 0)

    cedars = [(-7, -14), (-9, -5), (-6.5, 3), (-9.5, 10), (-7, 19), (7.5, -15),
              (9, -7), (6.8, 1), (9.5, 9), (7.5, 20), (-10, 16), (10.5, 16)]
    for x, y in cedars:
        level.place("cedar", x, y, 0)

    level.place("haiden", 0.0, 18.0, 0)
    level.place("offering_box", 0.0, 14.9, 0, z=0.9)

    # Collision: a walkable floor and a ring around the precinct.
    level.add_collider("floor", [((0, 2, -0.5), (60, 70, 1.0))])
    level.add_collider("bounds", [
        ((-half_width - 1, 2, 2), (1, 50, 4)), ((half_width + 1, 2, 2), (1, 50, 4)),
        ((0, north + 1, 2), (2 * half_width + 2, 1, 4)), ((0, south - 1, 2), (2 * half_width + 2, 1, 4)),
    ])
    return level


# --------------------------------------------------------------------------
# Level C: machiya interior
# --------------------------------------------------------------------------

def build_machiya_interior(pieces, collision, materials, collection):
    """An earthen entry (doma), a step up onto a tatami room with a shoji
    wall glowing with daylight, fusuma, a tokonoma alcove with a scroll, a
    low table - and a dark corridor that goes nowhere you'd want to."""
    level = Street(pieces, collision, materials, collection)
    m = kit.MeshBuilder()
    ceiling = 2.9
    room_y0, room_y1 = 3.0, 10.5
    floor = 0.3

    # Floors: earth in the doma, a two-part step, tatami above.
    m.box((0, 1.5, -0.05), (3.2, 3.0, 0.1), "concrete", faces=[(0, 0, 1)])
    m.box((0, 2.85, 0.075), (3.2, 0.3, 0.15), "wood_light", faces=[(0, 0, 1), (0, -1, 0)])
    m.box((0, (room_y0 + room_y1) / 2, floor / 2), (8.0, room_y1 - room_y0, floor), "tatami",
          faces=[(0, 0, 1), (0, -1, 0)])
    m.box((0, 7.3, ceiling + 0.05), (8.2, 14.6, 0.1), "wood_dark", faces=[(0, 0, -1)])

    # Doma walls and the entrance door (closed; leaving is an interaction).
    m.box((0, -0.05, ceiling / 2), (3.4, 0.1, ceiling), "wood_dark")
    m.quad([(0.7, 0.01, 0.0), (-0.7, 0.01, 0.0), (-0.7, 0.01, 2.0), (0.7, 0.01, 2.0)], "door_lattice",
           uvs=[(0, 0), (2.3, 0), (2.3, 3.3), (0, 3.3)])
    for x in (-1.65, 1.65):
        m.box((x, 1.5, ceiling / 2), (0.1, 3.0, ceiling), "plaster")

    # Room walls: south returns beside the doma, fusuma west, shoji east.
    for x0, x1 in ((-4.0, -1.6), (1.6, 4.0)):
        m.box(((x0 + x1) / 2, room_y0 - 0.05, ceiling / 2), (x1 - x0, 0.1, ceiling), "plaster")
    m.box((-4.05, (room_y0 + room_y1) / 2, ceiling / 2), (0.1, room_y1 - room_y0, ceiling), "plaster")
    for i in range(4):
        y0 = room_y0 + 0.4 + i * 1.7
        m.quad([(-3.99, y0, floor), (-3.99, y0 + 1.6, floor), (-3.99, y0 + 1.6, floor + 1.9), (-3.99, y0, floor + 1.9)],
               "fusuma")
    m.box((4.05, (room_y0 + room_y1) / 2, ceiling / 2), (0.1, room_y1 - room_y0, ceiling), "wood_dark")
    for i in range(4):
        y0 = room_y0 + 0.4 + i * 1.7
        m.quad([(3.99, y0 + 1.6, floor + 0.3), (3.99, y0, floor + 0.3), (3.99, y0, floor + 2.1), (3.99, y0 + 1.6, floor + 2.1)],
               "shoji_glow", uvs=[(0, 0), (1.8, 0), (1.8, 2), (0, 2)])

    # North wall with the tokonoma alcove and the corridor opening.
    for x0, x1 in ((-4.0, -1.2), (1.2, 2.6), (3.6, 4.0)):
        m.box(((x0 + x1) / 2, room_y1 + 0.05, ceiling / 2), (x1 - x0, 0.1, ceiling), "plaster")
    m.box((0, 11.35, ceiling / 2), (2.4, 0.1, ceiling), "plaster")
    for x in (-1.25, 1.25):
        m.box((x, 10.9, ceiling / 2), (0.1, 0.8, ceiling), "wood_dark")
    m.box((0, 10.9, 0.225), (2.4, 0.8, 0.45), "wood_light", faces=[(0, 0, 1), (0, -1, 0)])
    m.quad([(-0.35, 11.29, 0.9), (0.35, 11.29, 0.9), (0.35, 11.29, 2.35), (-0.35, 11.29, 2.35)], "scroll")
    # Corridor: a black opening swallowing the light.
    m.box((2.55, 12.55, ceiling / 2), (0.1, 4.1, ceiling), "black")
    m.box((3.65, 12.55, ceiling / 2), (0.1, 4.1, ceiling), "black")
    m.box((3.1, 14.55, ceiling / 2), (1.2, 0.1, ceiling), "black")
    m.box((3.1, 12.55, floor - 0.01), (1.0, 4.1, 0.02), "black", faces=[(0, 0, 1)])

    # Low table with a folded letter.
    m.box((0, 6.5, floor + 0.3), (1.2, 0.9, 0.06), "wood_dark")
    for x, y in ((-0.5, 6.15), (0.5, 6.15), (-0.5, 6.85), (0.5, 6.85)):
        m.box((x, y, floor + 0.14), (0.06, 0.06, 0.28), "wood_dark")
    m.box((0.1, 6.45, floor + 0.34), (0.28, 0.2, 0.012), "cloth_white", rotation=kit.rot_z(12))
    level.add_visual("interior", m)

    level.add_collider("interior_col", [
        ((0, 1.5, -0.5), (3.4, 3.2, 1.0)),                 # doma floor
        ((0, 2.85, 0.075), (3.2, 0.3, 0.15)),              # first step
        ((0, 6.75, floor / 2), (8.0, 7.5, floor)),         # tatami (0.15 above the step)
        ((0, 10.9, 0.225), (2.4, 0.8, 0.45)),              # tokonoma dais
        ((0, 6.5, floor + 0.17), (1.3, 1.0, 0.34)),        # table
        ((0, -0.15, 1.5), (3.6, 0.3, 3.0)),                # entrance wall
        ((-1.75, 1.5, 1.5), (0.3, 3.0, 3.0)), ((1.75, 1.5, 1.5), (0.3, 3.0, 3.0)),
        ((-2.8, 2.9, 1.5), (2.4, 0.2, 3.0)), ((2.8, 2.9, 1.5), (2.4, 0.2, 3.0)),
        ((-4.15, 6.75, 1.5), (0.3, 8.0, 3.0)), ((4.15, 6.75, 1.5), (0.3, 8.0, 3.0)),
        ((-2.6, 10.6, 1.5), (2.8, 0.2, 3.0)), ((1.9, 10.6, 1.5), (1.4, 0.2, 3.0)),
        ((3.8, 10.6, 1.5), (0.4, 0.2, 3.0)),
        ((0, 11.45, 1.5), (2.6, 0.2, 3.0)),
        ((-1.3, 10.95, 1.5), (0.1, 0.9, 3.0)), ((1.3, 10.95, 1.5), (0.1, 0.9, 3.0)),
        ((3.1, 11.1, 1.5), (1.0, 0.3, 3.0)),               # the corridor: you don't go in
    ])
    return level


LEVELS = [
    ("shrine", build_shrine_grounds),
    ("interior", build_machiya_interior),
]
