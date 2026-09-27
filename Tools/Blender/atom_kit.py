"""Modular rural-Japan kit for the AtomEngine tech demo.

Geometry is built directly (no modifiers) so the exported glTF matches what
the script describes. Blender is Z-up; the glTF exporter converts to Y-up.
Every piece has its origin on the ground at its footprint centre, and its
front faces -Y (glTF +Z).
"""

import math

import bpy
from mathutils import Matrix, Vector

import atom_textures as tex

PREFIX = "atom_"


# --------------------------------------------------------------------------
# Materials
# --------------------------------------------------------------------------

# name: (generator, metres per texture tile, roughness, emission strength)
MATERIALS = {
    "wood_dark": (lambda: tex.wood_boards(), 1.6, 0.85, 0.0),
    "wood_light": (lambda: tex.wood_boards(seed=21, color=(0.42, 0.32, 0.22), boards=6), 1.2, 0.8, 0.0),
    "plaster": (lambda: tex.plaster(), 2.0, 0.95, 0.0),
    "roof_tile": (lambda: tex.roof_tiles(), 1.0, 0.6, 0.0),
    "concrete": (lambda: tex.concrete(), 1.5, 0.9, 0.0),
    "asphalt": (lambda: tex.asphalt(), 4.0, 0.9, 0.0),
    "stone": (lambda: tex.stone_blocks(), 1.2, 0.95, 0.0),
    "torii_red": (lambda: tex.faded_paint(), 1.0, 0.7, 0.0),
    "metal_white": (lambda: tex.painted_metal(), 1.0, 0.5, 0.0),
    "metal_dark": (lambda: tex.dark_metal(), 1.0, 0.6, 0.0),
    "black": (lambda: tex.flat((0.06, 0.06, 0.06)), 1.0, 0.7, 0.0),
    "ceramic": (lambda: tex.flat((0.70, 0.70, 0.66)), 1.0, 0.3, 0.0),
    "lattice": (lambda: tex.lattice(), 0.6, 0.85, 0.0),
    "door_lattice": (lambda: tex.lattice(seed=16, bars=8, wood=(0.40, 0.29, 0.19)), 0.6, 0.85, 0.0),
    "shoji": (lambda: tex.shoji(), 0.9, 0.95, 0.0),
    "hazard": (lambda: tex.hazard_stripes(), 0.6, 0.6, 0.0),
    "road_paint": (lambda: tex.road_paint(), 1.0, 0.8, 0.0),
    "vending_front": (lambda: tex.vending_front(), 1.0, 0.3, 1.0),
    "dirt": (lambda: tex.dirt(), 3.0, 1.0, 0.0),
    "paddy": (lambda: tex.paddy(), 4.0, 0.2, 0.0),
    "cloth_white": (lambda: tex.flat((0.78, 0.77, 0.72), size=32, seed=30, variation=0.12), 1.0, 0.9, 0.0),
    "cloth_hakama": (lambda: tex.flat((0.30, 0.22, 0.34), size=32, seed=31, variation=0.15), 1.0, 0.9, 0.0),
    "skin": (lambda: tex.flat((0.62, 0.48, 0.38), size=16, seed=32, variation=0.06), 1.0, 0.7, 0.0),
    "hair_grey": (lambda: tex.flat((0.46, 0.46, 0.45), size=16, seed=33, variation=0.1), 1.0, 0.9, 0.0),
}


def _make_image(name, pixels):
    height, width = pixels.shape[:2]
    image = bpy.data.images.new(PREFIX + name, width, height, alpha=False)
    image.colorspace_settings.name = "sRGB"
    image.pixels.foreach_set(pixels.ravel())
    image.pack()
    return image


def _make_material(name):
    generator, _, roughness, emission = MATERIALS[name]
    image = _make_image(name, generator())

    material = bpy.data.materials.new(PREFIX + name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links

    bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = image
    texture.interpolation = "Linear"
    links.new(texture.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = 0.0

    if emission > 0.0:
        links.new(texture.outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = emission

    return material


def clear_generated():
    """Removes data from a previous run so rebuilds are clean."""
    for collection in (bpy.data.materials, bpy.data.images, bpy.data.meshes):
        for block in list(collection):
            if block.name.startswith(PREFIX):
                collection.remove(block)


def build_materials():
    return {name: _make_material(name) for name in MATERIALS}


def tile_of(material_name):
    return MATERIALS[material_name][1]


# --------------------------------------------------------------------------
# Mesh builder
# --------------------------------------------------------------------------

_BOX_FACES = (
    # normal, u axis, v axis   (u x v == normal, v is "up" on side faces)
    ((1, 0, 0), (0, 1, 0), (0, 0, 1)),
    ((-1, 0, 0), (0, -1, 0), (0, 0, 1)),
    ((0, 1, 0), (-1, 0, 0), (0, 0, 1)),
    ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
    ((0, 0, 1), (1, 0, 0), (0, 1, 0)),
    ((0, 0, -1), (-1, 0, 0), (0, 1, 0)),
)


def rot_x(degrees):
    return Matrix.Rotation(math.radians(degrees), 3, "X")


def rot_y(degrees):
    return Matrix.Rotation(math.radians(degrees), 3, "Y")


def rot_z(degrees):
    return Matrix.Rotation(math.radians(degrees), 3, "Z")


class MeshBuilder:
    def __init__(self):
        self.verts = []
        self.faces = []  # (vertex indices, loop uvs, material name, smooth)

    def _add_vert(self, position):
        self.verts.append(tuple(position))
        return len(self.verts) - 1

    def quad(self, corners, material, uvs=None, smooth=False):
        """corners counter-clockwise seen from the front."""
        indices = [self._add_vert(c) for c in corners]
        if uvs is None:
            uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
        self.faces.append((indices, list(uvs), material, smooth))

    def tri(self, corners, material, uvs):
        indices = [self._add_vert(c) for c in corners]
        self.faces.append((indices, list(uvs), material, False))

    def box(self, center, size, material, rotation=None, faces="all"):
        """Box with world-scale UVs; faces can exclude e.g. hidden bottoms."""
        rotation = rotation or Matrix.Identity(3)
        center = Vector(center)
        half = Vector(size) / 2.0
        tile = tile_of(material)

        for normal, u_axis, v_axis in _BOX_FACES:
            if faces != "all" and normal not in faces:
                continue
            n, u, v = Vector(normal), Vector(u_axis), Vector(v_axis)
            corners, uvs = [], []
            for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                local = n + u * su + v * sv
                local = Vector((local.x * half.x, local.y * half.y, local.z * half.z))
                corners.append(center + rotation @ local)
                uvs.append((local.dot(u) / tile, local.dot(v) / tile))
            self.quad(corners, material, uvs)

    def cylinder(self, base, radius, height, material, segments=12, caps=True):
        """Upright cylinder with shared ring verts so it shades smooth."""
        base = Vector(base)
        tile = tile_of(material)
        circumference = 2.0 * math.pi * radius
        ring_bottom, ring_top = [], []
        for i in range(segments):
            angle = 2.0 * math.pi * i / segments
            offset = Vector((math.cos(angle) * radius, math.sin(angle) * radius, 0))
            ring_bottom.append(self._add_vert(base + offset))
            ring_top.append(self._add_vert(base + offset + Vector((0, 0, height))))

        for i in range(segments):
            j = (i + 1) % segments
            u0 = circumference * i / segments / tile
            u1 = circumference * (i + 1) / segments / tile
            v1 = height / tile
            self.faces.append((
                [ring_bottom[i], ring_bottom[j], ring_top[j], ring_top[i]],
                [(u0, 0), (u1, 0), (u1, v1), (u0, v1)],
                material,
                True,
            ))

        if caps:
            cap_uv = [
                (0.5 + 0.5 * math.cos(2 * math.pi * i / segments),
                 0.5 + 0.5 * math.sin(2 * math.pi * i / segments))
                for i in range(segments)
            ]
            self.faces.append((list(ring_top), cap_uv, material, False))
            self.faces.append((list(reversed(ring_bottom)), list(reversed(cap_uv)), material, False))

    def build(self, name, materials, collection):
        mesh = bpy.data.meshes.new(PREFIX + name)
        mesh.from_pydata(self.verts, [], [f[0] for f in self.faces])

        used = []
        for _, _, material, _ in self.faces:
            if material not in used:
                used.append(material)
        for material in used:
            mesh.materials.append(materials[material])

        uv_layer = mesh.uv_layers.new(name="UVMap")
        loop = 0
        for polygon, (_, uvs, material, smooth) in zip(mesh.polygons, self.faces):
            polygon.material_index = used.index(material)
            polygon.use_smooth = smooth
            for uv in uvs:
                uv_layer.data[loop].uv = uv
                loop += 1

        mesh.validate()
        mesh.update()

        obj = bpy.data.objects.new(name, mesh)
        collection.objects.link(obj)
        return obj


# --------------------------------------------------------------------------
# Kit pieces
# --------------------------------------------------------------------------

def build_machiya(materials, collection):
    """Two-storey town house, 6.4 m wide, 8 m deep, gable along the street."""
    m = MeshBuilder()
    width, depth = 6.4, 8.0
    ground_h, upper_h = 2.8, 2.3
    front = -depth / 2.0

    m.box((0, 0, 0.15), (width + 0.1, depth + 0.1, 0.3), "concrete", faces=[(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1)])
    m.box((0, 0, 0.3 + ground_h / 2), (width, depth, ground_h), "wood_dark")

    # Ground floor front: sliding lattice door and two koshi windows.
    y = front - 0.02
    m.quad([(-0.6, y, 0.3), (1.2, y, 0.3), (1.2, y, 2.4), (-0.6, y, 2.4)], "door_lattice",
           uvs=[(0, 0), (3, 0), (3, 3.5), (0, 3.5)])
    m.box((0.3, front - 0.06, 2.45), (2.0, 0.12, 0.1), "wood_light")
    for x in (-0.65, 1.25):
        m.box((x, front - 0.06, 1.35), (0.1, 0.12, 2.1), "wood_light")
    m.box((0.3, front - 0.06, 0.33), (2.0, 0.12, 0.06), "wood_light")
    for x0 in (-2.8, 1.7):
        m.quad([(x0, y, 1.0), (x0 + 1.1, y, 1.0), (x0 + 1.1, y, 2.1), (x0, y, 2.1)], "lattice",
               uvs=[(0, 0), (1.8, 0), (1.8, 1.8), (0, 1.8)])
        m.box((x0 + 0.55, front - 0.08, 0.95), (1.25, 0.16, 0.08), "wood_light")

    # Hisashi: small tiled eave between the floors.
    eave_depth, pitch = 0.9, 22.0
    run = eave_depth / math.cos(math.radians(pitch))
    m.box((0, front - eave_depth / 2, 3.05 - eave_depth / 2 * math.tan(math.radians(pitch))),
          (width + 0.3, run, 0.08), "roof_tile", rotation=rot_x(pitch))

    # Upper floor: plaster with a wooden band, set back slightly.
    upper_depth = depth - 1.0
    upper_center_y = 0.5
    z0 = 0.3 + ground_h
    m.box((0, upper_center_y, z0 + upper_h / 2), (width, upper_depth, upper_h), "plaster")
    m.box((0, upper_center_y, z0 + 0.12), (width + 0.04, upper_depth + 0.04, 0.24), "wood_dark")
    uy = upper_center_y - upper_depth / 2 - 0.02
    m.quad([(-1.8, uy, z0 + 0.7), (1.8, uy, z0 + 0.7), (1.8, uy, z0 + 1.8), (-1.8, uy, z0 + 1.8)], "shoji",
           uvs=[(0, 0), (4, 0), (4, 1.2), (0, 1.2)])
    m.quad([(-1.8, uy - 0.1, z0 + 0.7), (1.8, uy - 0.1, z0 + 0.7), (1.8, uy - 0.1, z0 + 1.3), (-1.8, uy - 0.1, z0 + 1.3)], "lattice",
           uvs=[(0, 0), (6, 0), (6, 1), (0, 1)])

    # Main gable roof, ridge along X.
    roof_pitch = 26.0
    overhang = 0.7
    eave_z = z0 + upper_h
    half_span = upper_depth / 2 + overhang
    rise = half_span * math.tan(math.radians(roof_pitch))
    slab = half_span / math.cos(math.radians(roof_pitch))
    for side in (-1, 1):
        center_y = upper_center_y + side * half_span / 2
        center_z = eave_z + rise / 2 - overhang * math.tan(math.radians(roof_pitch))
        m.box((0, center_y, center_z), (width + 1.0, slab, 0.14), "roof_tile",
              rotation=rot_x(-side * roof_pitch))
    ridge_z = eave_z + rise - overhang * math.tan(math.radians(roof_pitch))
    m.box((0, upper_center_y, ridge_z + 0.08), (width + 1.1, 0.35, 0.25), "roof_tile")

    # Gable ends.
    inner_rise = (upper_depth / 2) * math.tan(math.radians(roof_pitch))
    tile = tile_of("plaster")
    for side in (-1, 1):
        x = side * width / 2
        a = (x, upper_center_y - upper_depth / 2, eave_z)
        b = (x, upper_center_y + upper_depth / 2, eave_z)
        c = (x, upper_center_y, eave_z + inner_rise)
        corners = [a, b, c] if side > 0 else [b, a, c]
        uvs = [((p[1]) / tile, (p[2] - eave_z) / tile) for p in corners]
        m.tri(corners, "plaster", uvs)

    return m.build("machiya", materials, collection)


def build_utility_pole(materials, collection):
    """Concrete pole with cross-arms, insulators, transformer and guard."""
    m = MeshBuilder()
    height = 9.0
    m.cylinder((0, 0, 0), 0.16, height, "concrete", segments=10)
    m.cylinder((0, 0, 0), 0.18, 1.8, "hazard", segments=10, caps=False)

    for z, length in ((8.4, 1.8), (7.7, 1.4)):
        m.box((0, 0.2, z), (length, 0.1, 0.1), "metal_dark")
        for x in (-length / 2 + 0.1, 0.0, length / 2 - 0.1):
            if x == 0.0 and length < 1.5:
                continue
            m.cylinder((x, 0.2, z + 0.05), 0.05, 0.16, "ceramic", segments=6)

    m.box((0, 0.28, 6.4), (0.1, 0.4, 0.1), "metal_dark")
    m.cylinder((0, 0.55, 5.9), 0.26, 0.85, "metal_dark", segments=10)
    m.box((0, -0.19, 2.2), (0.3, 0.02, 0.45), "metal_white")  # address plate
    return m.build("utility_pole", materials, collection)


def build_vending_machine(materials, collection):
    m = MeshBuilder()
    w, d, h = 1.0, 0.75, 1.83
    m.box((0, 0, h / 2), (w, d, h), "metal_white", faces=[(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1)])
    y = -d / 2 - 0.005
    m.quad([(-0.44, y, 0.12), (0.44, y, 0.12), (0.44, y, 1.72), (-0.44, y, 1.72)], "vending_front")
    m.box((0, -d / 2 - 0.05, h + 0.03), (w + 0.04, 0.12, 0.06), "metal_white")
    m.box((0, 0, 0.04), (w + 0.02, d + 0.02, 0.08), "black", faces=[(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0)])
    return m.build("vending_machine", materials, collection)


def build_torii(materials, collection):
    m = MeshBuilder()
    span = 1.6
    for x in (-span, span):
        m.cylinder((x, 0, 0), 0.18, 3.7, "torii_red", segments=12, caps=False)
        m.cylinder((x, 0, 0), 0.22, 0.35, "black", segments=12)
    m.box((0, 0, 3.15), (4.0, 0.18, 0.24), "torii_red")          # nuki
    m.box((0, 0, 3.5), (0.22, 0.16, 0.5), "torii_red")           # gakuzuka
    m.box((0, 0, 3.85), (4.6, 0.34, 0.3), "torii_red")           # shimaki
    m.box((0, 0, 4.06), (4.9, 0.42, 0.14), "black")              # kasagi
    return m.build("torii", materials, collection)


def build_stone_wall(materials, collection):
    """4 m segment; tiles seamlessly along X."""
    m = MeshBuilder()
    m.box((0, 0, 0.65), (4.0, 0.5, 1.3), "stone", faces=[(0, 1, 0), (0, -1, 0), (1, 0, 0), (-1, 0, 0)])
    m.box((0, 0, 1.35), (4.0, 0.6, 0.1), "concrete")
    return m.build("stone_wall", materials, collection)


def build_wood_fence(materials, collection):
    """4 m segment of vertical board fence."""
    m = MeshBuilder()
    for i in range(5):
        m.box((-2.0 + i, 0.06, 0.65), (0.08, 0.08, 1.3), "wood_light")
    for z in (0.3, 1.1):
        m.box((0, 0.06, z), (4.0, 0.03, 0.08), "wood_light")
    count = 32
    pitch = 4.0 / count
    for i in range(count):
        x = -2.0 + pitch * (i + 0.5)
        top = 1.2 + 0.03 * math.sin(i * 1.7)
        m.box((x, 0.0, top / 2 + 0.05), (pitch * 0.8, 0.02, top), "wood_dark")
    return m.build("wood_fence", materials, collection)


def build_road(materials, collection):
    """8 m (X) by 7 m (Y) road segment with gutters; tiles along X."""
    m = MeshBuilder()
    length, width = 8.0, 6.0
    m.box((0, 0, -0.05), (length, width, 0.1), "asphalt", faces=[(0, 0, 1)])
    for side in (-1, 1):
        y = side * (width / 2 - 0.3)
        m.quad([(-length / 2, y - 0.06, 0.004), (length / 2, y - 0.06, 0.004),
                (length / 2, y + 0.06, 0.004), (-length / 2, y + 0.06, 0.004)], "road_paint",
               uvs=[(0, 0), (8, 0), (8, 0.12), (0, 0.12)])
        gy = side * (width / 2 + 0.25)
        m.box((0, gy, -0.05), (length, 0.5, 0.1), "concrete", faces=[(0, 0, 1)])
        m.box((0, gy + side * 0.3, 0.05), (length, 0.1, 0.3), "concrete",
              faces=[(0, 0, 1), (0, -side, 0)])
    return m.build("road", materials, collection)


def build_hokora(materials, collection):
    """Small roadside shrine on a stone plinth."""
    m = MeshBuilder()
    m.box((0, 0, 0.2), (1.4, 1.2, 0.4), "stone")
    m.box((0, 0, 0.75), (0.9, 0.8, 0.7), "wood_dark")
    m.quad([(-0.3, -0.405, 0.45), (0.3, -0.405, 0.45), (0.3, -0.405, 1.0), (-0.3, -0.405, 1.0)], "door_lattice",
           uvs=[(0, 0), (1, 0), (1, 0.9), (0, 0.9)])
    for side in (-1, 1):
        m.box((0, side * 0.3, 1.25), (1.2, 0.75, 0.06), "roof_tile", rotation=rot_x(-side * 30))
    m.box((0, 0, 1.46), (1.25, 0.12, 0.1), "black")
    m.box((0, -0.7, 0.08), (0.5, 0.3, 0.16), "stone")  # offering step
    return m.build("hokora", materials, collection)


def build_keeper(materials, collection):
    """The shrine keeper: an old priest in white kimono, faded purple
    hakama and a black eboshi, hands folded. Low-poly on purpose; the fog
    and distance do the rest. Faces -Y like the rest of the kit."""
    m = MeshBuilder()
    # Hakama: wide pleated trousers, flaring at the hem, straw sandals.
    m.box((0, 0, 0.47), (0.50, 0.34, 0.86), "cloth_hakama")
    m.box((0, 0, 0.09), (0.58, 0.42, 0.18), "cloth_hakama")
    for x in (-0.11, 0.11):
        m.box((x, -0.06, 0.015), (0.11, 0.26, 0.03), "black")
    # Kimono body and wide hanging sleeves, a slight stoop forward.
    m.box((0, -0.02, 1.13), (0.46, 0.28, 0.54), "cloth_white", rotation=rot_x(-4))
    for side in (-1, 1):
        m.box((side * 0.28, -0.03, 1.07), (0.13, 0.32, 0.44), "cloth_white", rotation=rot_x(-4))
    m.box((0, -0.19, 0.93), (0.16, 0.08, 0.08), "skin")          # folded hands
    m.box((0, -0.03, 1.40), (0.10, 0.10, 0.08), "skin")          # neck
    m.box((0, -0.05, 1.52), (0.18, 0.20, 0.22), "skin")          # head
    m.box((0, 0.01, 1.60), (0.20, 0.20, 0.07), "hair_grey")      # hair
    m.box((0, 0.06, 1.52), (0.19, 0.07, 0.17), "hair_grey")
    m.box((0, -0.01, 1.70), (0.13, 0.19, 0.15), "black")         # eboshi
    return m.build("keeper", materials, collection)


PIECES = [
    build_machiya,
    build_utility_pole,
    build_vending_machine,
    build_torii,
    build_stone_wall,
    build_wood_fence,
    build_road,
    build_hokora,
    build_keeper,
]


# --------------------------------------------------------------------------
# Collision proxies
# --------------------------------------------------------------------------

# Boxes (centre, size) in piece-local space. Walkable floors come from the
# street's ground plane; these only need to stop the player or be stepped on.
COLLISION = {
    "machiya": [((0, 0, 2.6), (6.5, 8.1, 5.2))],
    "utility_pole": [((0, 0, 4.5), (0.4, 0.4, 9.0))],
    "vending_machine": [((0, 0, 0.92), (1.04, 0.8, 1.84))],
    "torii": [((x, 0, 1.85), (0.44, 0.44, 3.7)) for x in (-1.6, 1.6)],
    "stone_wall": [((0, 0, 0.7), (4.0, 0.6, 1.4))],
    "wood_fence": [((0, 0.03, 0.65), (4.0, 0.14, 1.3))],
    # Curbs are low enough to step onto.
    "road": [((0, side * 3.55, 0.05), (8.0, 0.1, 0.3)) for side in (-1, 1)],
    "hokora": [((0, 0, 0.8), (1.4, 1.2, 1.6)), ((0, -0.7, 0.08), (0.5, 0.3, 0.16))],
    "keeper": [((0, 0, 0.85), (0.6, 0.5, 1.7))],
}


def box_triangles(center, size, rotation=None):
    """Vertices and quads (CCW outward) of a box, for collision meshes."""
    rotation = rotation or Matrix.Identity(3)
    center = Vector(center)
    half = Vector(size) / 2.0
    verts, faces = [], []
    for normal, u_axis, v_axis in _BOX_FACES:
        n, u, v = Vector(normal), Vector(u_axis), Vector(v_axis)
        base = len(verts)
        for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            local = n + u * su + v * sv
            local = Vector((local.x * half.x, local.y * half.y, local.z * half.z))
            verts.append(tuple(center + rotation @ local))
        faces.append((base, base + 1, base + 2, base + 3))
    return verts, faces


def build_collision(name, boxes, collection):
    verts, faces = [], []
    for center, size in boxes:
        box_verts, box_faces = box_triangles(center, size)
        offset = len(verts)
        verts.extend(box_verts)
        faces.extend(tuple(i + offset for i in face) for face in box_faces)

    mesh = bpy.data.meshes.new(PREFIX + name + "_col")
    mesh.from_pydata(verts, [], faces)
    mesh.validate()
    obj = bpy.data.objects.new(name + "_col", mesh)
    obj.display_type = "WIRE"
    collection.objects.link(obj)
    return obj
