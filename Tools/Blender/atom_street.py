"""Assembles the demo street from kit pieces.

Blender space: the road runs along X, "north" is +Y (glTF -Z). Kit pieces
face -Y by default, so north-side pieces face the road unrotated and
south-side pieces are turned 180 degrees.

Every visual instance shares its kit mesh (linked duplicate), so the glTF
stores each mesh once. Collision proxies are placed alongside and exported
to a separate file.
"""

import math

import bpy
from mathutils import Vector

import atom_kit as kit

STREET_HALF_LENGTH = 40.0
HOUSE_OFFSET = 8.4       # house centre distance from the road centre line
EDGE_OFFSET = 4.6        # walls and fences along the road edge
POLE_OFFSET = 4.0


class Street:
    def __init__(self, pieces, collision, materials, collection):
        self.pieces = pieces          # name -> kit object
        self.collision = collision    # name -> kit collision object
        self.materials = materials
        self.collection = collection
        self.visual = []
        self.colliders = []

    def place(self, name, x, y, yaw_degrees=0.0, z=0.0):
        rotation = (0.0, 0.0, math.radians(yaw_degrees))

        obj = bpy.data.objects.new(f"{name}.{len(self.visual):03d}", self.pieces[name].data)
        obj.location = (x, y, z)
        obj.rotation_euler = rotation
        self.collection.objects.link(obj)
        self.visual.append(obj)

        source = self.collision.get(name)
        if source:
            col = bpy.data.objects.new(f"{name}_col.{len(self.colliders):03d}", source.data)
            col.location = (x, y, z)
            col.rotation_euler = rotation
            col.display_type = "WIRE"
            self.collection.objects.link(col)
            self.colliders.append(col)
        return obj

    def add_visual(self, name, builder):
        obj = builder.build(name, self.materials, self.collection)
        self.visual.append(obj)
        return obj

    def add_collider(self, name, boxes):
        obj = kit.build_collision(name, boxes, self.collection)
        self.colliders.append(obj)
        return obj


def _ground(street):
    m = kit.MeshBuilder()

    def flat(material, x0, y0, x1, y1, z):
        tile = kit.tile_of(material)
        m.quad(
            [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)],
            material,
            uvs=[(x0 / tile, y0 / tile), (x1 / tile, y0 / tile),
                 (x1 / tile, y1 / tile), (x0 / tile, y1 / tile)],
        )

    flat("dirt", -70, -45, 70, 45, -0.02)
    flat("paddy", 14, -45, 70, -4.95, -0.01)
    flat("stone", -14.7, -10.2, -13.3, -4.9, 0.005)  # shrine approach
    street.add_visual("ground", m)


def _wires(street, pole_xs):
    """Sagging power lines between consecutive poles."""
    m = kit.MeshBuilder()
    # Pole is turned 90 degrees: cross-arm offsets land on world Y and the
    # arm's forward offset lands on world -X.
    attach = [(8.61, dy) for dy in (-0.8, 0.0, 0.8)] + [(7.91, dy) for dy in (-0.6, 0.6)]
    segments = 10
    sag = 0.45

    for x0, x1 in zip(pole_xs, pole_xs[1:]):
        for z, dy in attach:
            a = Vector((x0 - 0.2, POLE_OFFSET + dy, z))
            b = Vector((x1 - 0.2, POLE_OFFSET + dy, z))
            points = []
            for i in range(segments + 1):
                t = i / segments
                p = a.lerp(b, t)
                p.z -= 4.0 * sag * t * (1.0 - t)
                points.append(p)
            for p, q in zip(points, points[1:]):
                direction = q - p
                rotation = direction.to_track_quat("X", "Z").to_matrix()
                m.box((p + q) / 2, (direction.length, 0.025, 0.025), "black", rotation=rotation)

    street.add_visual("power_lines", m)


def _bounds(street):
    """Invisible walls keeping the player on the playable street."""
    x = STREET_HALF_LENGTH + 1.0
    street.add_collider("bounds", [
        ((-x, 0, 2), (1, 40, 4)),
        ((x, 0, 2), (1, 40, 4)),
        ((0, 15, 2), (2 * x, 1, 4)),
        ((0, -17, 2), (2 * x, 1, 4)),
    ])
    # Walkable floor for the whole area.
    street.add_collider("floor", [((0, 0, -0.5), (2 * x + 2, 40, 1.0))])


def build_street(pieces, collision, materials, collection):
    street = Street(pieces, collision, materials, collection)

    # Road, running on into the fog past both ends.
    for i in range(-6, 6):
        street.place("road", i * 8.0 + 4.0, 0.0)

    # North side: houses facing the road.
    for x in (-34, -26, -14, -6, 10, 18, 34):
        street.place("machiya", x, HOUSE_OFFSET)
    street.place("vending_machine", -20.0, 4.1)
    street.place("vending_machine", 29.8, 4.1)
    for x in (0, 4):
        street.place("stone_wall", x, EDGE_OFFSET)
    for x in (24, 28):
        street.place("wood_fence", x, EDGE_OFFSET)

    # South side: shrine wall with the torii, two houses, then the fence
    # that holds back the rice paddies.
    for x in (-38, -34, -30, -26, -22, -18, -10, -6):
        street.place("stone_wall", x, -EDGE_OFFSET)
    street.place("torii", -14.0, -5.5, 180)
    street.place("hokora", -14.0, -11.0, 180)
    # Behind the hokora: the barred gate to the shrine grounds, fenced off.
    street.place("shrine_gate", -14.0, -13.8, 180)
    for x in (-17.3, -10.7):
        street.place("wood_fence", x, -13.8, 180)
    street.place("vending_machine", -2.6, -4.1, 180)
    for x in (2, 10):
        street.place("machiya", x, -HOUSE_OFFSET, 180)
    for x in (16, 20, 24, 28, 32, 36, 40):
        street.place("wood_fence", x, -EDGE_OFFSET, 180)

    pole_xs = [-38.0, -22.0, -6.0, 10.0, 26.0, 42.0]
    for x in pole_xs:
        street.place("utility_pole", x, POLE_OFFSET, 90)
    _wires(street, pole_xs)

    _ground(street)
    _bounds(street)
    return street
