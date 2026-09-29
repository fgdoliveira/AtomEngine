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

# Largest quad cell MeshBuilder emits by default (metres); see
# MeshBuilder._tessellated. About the vertex density of a PS2-era level.
GRID = 1.0

# Problems found by MeshBuilder.lint_coplanar while building; build_assets
# refuses to export while this is non-empty.
LINT_ERRORS = []


# --------------------------------------------------------------------------
# Materials
# --------------------------------------------------------------------------

# The flickering sign's colours (M25): amber tubes, a pale border.
_AMBER = {"tube_color": (1.0, 0.62, 0.15), "border_color": (1.0, 0.9, 0.6)}

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
    "tatami": (lambda: tex.tatami(), 0.9, 0.95, 0.0),
    "gravel": (lambda: tex.gravel(), 2.0, 1.0, 0.0),
    "bark": (lambda: tex.bark(), 1.2, 1.0, 0.0),
    "foliage": (lambda: tex.foliage(), 2.0, 1.0, 0.0),
    "wood_floor": (lambda: tex.wood_floor(), 1.5, 0.6, 0.0),
    "fusuma": (lambda: tex.fusuma(), 0.9, 0.95, 0.0),
    "scroll": (lambda: tex.scroll(), 1.0, 0.9, 0.0),
    "shoji_glow": (lambda: tex.shoji(seed=28), 0.9, 0.95, 0.35),
    "lantern_paper": (lambda: tex.lantern_paper(), 0.5, 0.9, 0.9),
    "straw": (lambda: tex.flat((0.62, 0.52, 0.30), size=16, seed=34, variation=0.15), 1.0, 1.0, 0.0),
    # Alpha-tested (see MASKED).
    "leaves": (lambda: tex.leaves(), 1.0, 1.0, 0.0),
    "grass": (lambda: tex.grass(), 1.0, 1.0, 0.0),
    "noren": (lambda: tex.noren(), 1.0, 0.95, 0.0),
    "chain_link": (lambda: tex.chain_link(), 0.5, 0.5, 0.0),
    # Decals (see DECALS).
    "water_stain": (lambda: tex.water_stain(), 1.0, 0.95, 0.0),
    "grime": (lambda: tex.grime(), 1.0, 0.95, 0.0),
    "shop_sign": (lambda: tex.shop_sign(), 1.0, 0.8, 0.0),
    "ofuda": (lambda: tex.ofuda(), 1.0, 0.9, 0.0),
    "road_diamond": (lambda: tex.road_diamond(), 1.0, 0.8, 0.0),
    # Night (M23): these take their emission from a mask (see EMISSIVE).
    "neon_sign": (lambda: tex.neon_sign()[0], 1.0, 0.5, 0.0),
    "lamp_glass": (lambda: tex.lamp_glass()[0], 1.0, 0.3, 0.0),
    # City layers (M24).
    "facade_atlas": (lambda: tex.facade_atlas()[0], 1.0, 0.9, 0.0),
    "skyline": (lambda: tex.skyline()[0], 1.0, 1.0, 0.0),
    # Night street (M25).
    "wet_asphalt": (lambda: tex.wet_asphalt(), 4.0, 0.25, 0.0),
    "shopfront_atlas": (lambda: tex.shopfront_atlas()[0], 1.0, 0.6, 0.0),
    "neon_amber": (lambda: tex.neon_sign(seed=62, **_AMBER)[0], 1.0, 0.5, 0.0),
    "pachinko_front": (lambda: tex.pachinko_front()[0], 1.0, 0.5, 0.0),
    "train_side": (lambda: tex.train_side()[0], 1.0, 0.4, 0.0),
    "timetable": (lambda: tex.timetable()[0], 1.0, 0.5, 0.0),
    "neon_reflection": (lambda: tex.neon_reflection()[0], 1.0, 0.2, 0.0),
    "puddle": (lambda: tex.puddle(), 1.0, 0.1, 0.0),
    "posters": (lambda: tex.posters(), 1.0, 0.9, 0.0),
    # Pachinko hall (M27).
    "carpet": (lambda: tex.carpet(), 2.0, 1.0, 0.0),
    "hall_wall": (lambda: tex.flat((0.22, 0.07, 0.09), size=32, seed=93, variation=0.1), 2.0, 0.9, 0.0),
    "pachinko_face": (lambda: tex.pachinko_face()[0], 1.0, 0.3, 0.0),
    "machine_top": (lambda: tex.machine_top()[0], 1.0, 0.4, 0.0),
    "ceiling_panel": (lambda: tex.ceiling_panel()[0], 1.0, 0.5, 0.0),
    "pachinko_screen": (lambda: tex.screen_placeholder()[0], 1.0, 0.2, 0.0),
    "pachinko_screen_b": (lambda: tex.screen_placeholder()[0], 1.0, 0.2, 0.0),
}

# Emissive masks (M23): material -> (mask generator, strength, fog amount).
# The mask says exactly which pixels glow; the base colour keeps the unlit
# look. Fog amount < 1 lets the light cut through fog (glTF extras
# "atom_fog", read by the engine).
EMISSIVE = {
    "neon_sign": (lambda: tex.neon_sign()[1], 5.0, 0.35),
    "lamp_glass": (lambda: tex.lamp_glass()[1], 4.0, 0.45),
    "facade_atlas": (lambda: tex.facade_atlas()[1], 2.0, 0.7),
    "skyline": (lambda: tex.skyline()[1], 1.5, 0.25),
    "shopfront_atlas": (lambda: tex.shopfront_atlas()[1], 1.2, 0.5),
    "neon_amber": (lambda: tex.neon_sign(seed=62, **_AMBER)[1], 5.0, 0.35),
    "pachinko_front": (lambda: tex.pachinko_front()[1], 2.2, 0.3),
    "train_side": (lambda: tex.train_side()[1], 2.5, 0.5),
    "timetable": (lambda: tex.timetable()[1], 1.2, 0.6),
    "neon_reflection": (lambda: tex.neon_reflection()[1], 1.5, 0.6),
    "pachinko_face": (lambda: tex.pachinko_face()[1], 1.5, 0.8),
    "machine_top": (lambda: tex.machine_top()[1], 2.5, 0.6),
    "ceiling_panel": (lambda: tex.ceiling_panel()[1], 1.6, 0.8),
    # Screens: the engine swaps the texture for a live one; the strength stays.
    "pachinko_screen": (lambda: tex.screen_placeholder()[1], 1.3, 0.8),
    "pachinko_screen_b": (lambda: tex.screen_placeholder()[1], 1.3, 0.8),
}

# Wet surfaces (M25): material -> wetness 0..1 (glTF extras "atom_wet").
# The engine ripples their emitted light (sign reflections) and lays a
# faint moving sheen over them.
WET = {
    "wet_asphalt": 0.5,
    "neon_reflection": 1.0,
    "puddle": 1.0,
}

# Alpha-tested materials (M17): name -> alpha cutoff. Pixels below it are
# not drawn; the cards are double-sided. Exported as glTF alphaMode MASK.
MASKED = {
    "skyline": 0.5,
    "leaves": 0.5,
    "grass": 0.5,
    "noren": 0.5,
    "chain_link": 0.5,
}

# Vertex sway (M19): material -> (direction, strength). "up": a card's
# bottom is rooted and its top moves; "down": it hangs from its top edge.
# Written into the baked colour's alpha as 1 - weight (atom_bake).
SWAY = {
    "grass": ("up", 1.0),
    "leaves": ("up", 0.45),
    "noren": ("down", 0.8),
}

# Decals (M18): alpha-blended layers lying just over another surface
# (DECAL_OFFSET in front of it), drawn after everything else with a depth
# bias. Exported as glTF alphaMode BLEND. The only faces the lint allows to
# lie (nearly) in the plane of another piece's face.
DECALS = {"water_stain", "grime", "shop_sign", "ofuda", "road_diamond", "road_paint",
          "neon_reflection", "puddle", "posters"}
DECAL_OFFSET = 0.002  # metres

# Faces of different pieces closer than this, parallel and overlapping,
# are treated as coplanar: they z-fight at a distance unless one is a decal.
NEAR_COPLANAR = 0.005  # metres


def _make_image(name, pixels):
    height, width = pixels.shape[:2]
    has_alpha = name in MASKED or name in DECALS
    image = bpy.data.images.new(PREFIX + name, width, height, alpha=has_alpha)
    image.colorspace_settings.name = "sRGB"
    if has_alpha:
        # Colour and alpha independent: premultiplying would blacken the
        # (dilated) colour of transparent texels, for filtering and bakes.
        image.alpha_mode = "CHANNEL_PACKED"
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

    if name in EMISSIVE:
        mask_generator, strength, fog = EMISSIVE[name]
        mask = nodes.new("ShaderNodeTexImage")
        mask.image = _make_image(name + "_emissive", mask_generator())
        mask.interpolation = "Linear"
        links.new(mask.outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = strength
        material["atom_fog"] = fog  # exported as material extras

    if name in WET:
        material["atom_wet"] = WET[name]

    if name in MASKED:
        _make_masked(material, texture, bsdf, name, image)
    elif name in DECALS:
        # Alpha straight into the BSDF: the exporter writes alphaMode BLEND.
        material.node_tree.links.new(texture.outputs["Alpha"], bsdf.inputs["Alpha"])

    return material


def _make_masked(material, texture, bsdf, name, image):
    """Alpha test: alpha = 1 - (texture alpha < cutoff). The glTF exporter
    reads this node chain as alphaMode MASK with that cutoff, and Cycles
    honours it while baking, so leaves shade by their shape."""
    cutoff = MASKED[name]
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    below = nodes.new("ShaderNodeMath")
    below.operation = "LESS_THAN"
    below.inputs[1].default_value = cutoff
    keep = nodes.new("ShaderNodeMath")
    keep.operation = "SUBTRACT"
    keep.inputs[0].default_value = 1.0
    links.new(texture.outputs["Alpha"], below.inputs[0])
    links.new(below.outputs[0], keep.inputs[1])
    links.new(keep.outputs[0], bsdf.inputs["Alpha"])
    material.use_backface_culling = False  # exported as doubleSided

    # Lint: a masked material whose texture is all opaque (or all empty)
    # would draw a solid card (or nothing).
    alpha = list(image.pixels)[3::4]
    if not alpha or min(alpha) >= cutoff or max(alpha) < cutoff:
        LINT_ERRORS.append(f"material {name}: alpha never crosses the cutoff {cutoff}")


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

# Face sets for boxes whose other faces can never be seen (resting on a
# floor, capped by a roof). Leaving them out saves triangles and keeps
# them from z-fighting with the faces they are pressed against.
SIDES = [(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0)]
NO_BOTTOM = SIDES + [(0, 0, 1)]


def rot_x(degrees):
    return Matrix.Rotation(math.radians(degrees), 3, "X")


def rot_y(degrees):
    return Matrix.Rotation(math.radians(degrees), 3, "Y")


def rot_z(degrees):
    return Matrix.Rotation(math.radians(degrees), 3, "Z")


class MeshBuilder:
    def __init__(self, grid=GRID):
        self.grid = grid
        self.verts = []
        self.faces = []  # (vertex indices, loop uvs, material name, smooth)
        # Per face: which call made it. Faces of one box or cylinder share
        # an id, so the lint never compares a piece with itself.
        self.pieces = []
        self._piece = 0
        self._grouped = False

    def _add_vert(self, position):
        self.verts.append(tuple(position))
        return len(self.verts) - 1

    def _begin_piece(self):
        if not self._grouped:
            self._piece += 1

    def _tag_faces(self):
        self.pieces.extend([self._piece] * (len(self.faces) - len(self.pieces)))

    def quad(self, corners, material, uvs=None, smooth=False):
        """corners counter-clockwise seen from the front."""
        self._begin_piece()
        indices = [self._add_vert(c) for c in corners]
        if uvs is None:
            uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
        self.faces.append((indices, list(uvs), material, smooth))
        self._tag_faces()

    def tri(self, corners, material, uvs):
        self._begin_piece()
        indices = [self._add_vert(c) for c in corners]
        self.faces.append((indices, list(uvs), material, False))
        self._tag_faces()

    def box(self, center, size, material, rotation=None, faces="all"):
        """Box with world-scale UVs; faces can exclude e.g. hidden bottoms."""
        rotation = rotation or Matrix.Identity(3)
        center = Vector(center)
        half = Vector(size) / 2.0
        tile = tile_of(material)

        self._begin_piece()
        self._grouped = True
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
        self._grouped = False

    def cylinder(self, base, radius, height, material, segments=12, caps=True):
        """Upright cylinder with shared ring verts so it shades smooth."""
        self._begin_piece()
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
        self._tag_faces()

    def lint_coplanar(self, name):
        """Finds z-fighting: faces of different pieces lying in the same
        axis-aligned plane, facing the same way, and overlapping. The depth
        test cannot order them, so they flicker as the view moves.

        Different materials are an error (visible flicker); the same material
        only a warning (identical texels, at most a lighting shimmer)."""
        min_area = 1e-4  # m^2 (1 cm^2)
        groups = {}  # (axis, facing) -> faces, searched by plane offset
        for face_index, (indices, _, material, _) in enumerate(self.faces):
            points = [self.verts[i] for i in indices]
            normal = Vector((0.0, 0.0, 0.0))  # Newell's method
            for a, b in zip(points, points[1:] + points[:1]):
                normal.x += (a[1] - b[1]) * (a[2] + b[2])
                normal.y += (a[2] - b[2]) * (a[0] + b[0])
                normal.z += (a[0] - b[0]) * (a[1] + b[1])
            if normal.length < 1e-9:
                continue
            normal.normalize()
            axis = max(range(3), key=lambda k: abs(normal[k]))
            if abs(abs(normal[axis]) - 1.0) > 1e-4:
                continue  # not axis-aligned (rotated pieces, roofs)
            others = [k for k in range(3) if k != axis]
            rect = (
                min(p[others[0]] for p in points), min(p[others[1]] for p in points),
                max(p[others[0]] for p in points), max(p[others[1]] for p in points),
            )
            groups.setdefault((axis, normal[axis] > 0), []).append(
                (points[0][axis], rect, material, self.pieces[face_index]))

        errors, warnings = [], 0
        for (axis, positive), faces in groups.items():
            faces.sort(key=lambda face: face[0])
            others = [k for k in range(3) if k != axis]
            for i in range(len(faces)):
                oa, ra, ma, pa = faces[i]
                for j in range(i + 1, len(faces)):
                    ob, rb, mb, pb = faces[j]
                    gap = ob - oa
                    if gap >= NEAR_COPLANAR - 1e-6:
                        break  # sorted: nothing further is near
                    if pa == pb:
                        continue
                    du = min(ra[2], rb[2]) - max(ra[0], rb[0])
                    dv = min(ra[3], rb[3]) - max(ra[1], rb[1])
                    if du <= 0 or dv <= 0 or du * dv < min_area:
                        continue
                    if ma in DECALS or mb in DECALS:
                        continue  # decals are meant to lie on surfaces
                    if ma == mb:
                        warnings += 1
                        continue
                    where = (f"{'XYZ'[axis]}={oa:+.3f} facing {'+' if positive else '-'}{'XYZ'[axis]}, "
                             f"{'XYZ'[others[0]]} {max(ra[0], rb[0]):.2f}..{min(ra[2], rb[2]):.2f}, "
                             f"{'XYZ'[others[1]]} {max(ra[1], rb[1]):.2f}..{min(ra[3], rb[3]):.2f}")
                    if gap < 1e-4:
                        errors.append(f"{name}: {ma} and {mb} overlap in the plane {where}")
                    else:
                        errors.append(
                            f"{name}: {ma} and {mb} overlap {gap * 1000:.1f} mm apart ({where}): "
                            f"make one a decal or separate them by {NEAR_COPLANAR * 1000:.0f} mm")
        if warnings:
            print(f"lint: {name}: {warnings} same-material coplanar overlaps (not fatal)")
        for error in errors:
            print("lint ERROR: " + error)
        LINT_ERRORS.extend(errors)

    def _tessellated(self):
        """Splits large flat quads into a grid of cells no bigger than
        `grid` metres. Baked light (M15) is stored per vertex, so a 10 m wall
        with four corners could only hold one smooth gradient; a grid lets
        it carry soft shadows in corners and under eaves. Smooth faces
        (cylinder sides, sharing vertices) are left alone."""
        verts, faces = [], []
        for indices, uvs, material, smooth in self.faces:
            corners = [Vector(self.verts[i]) for i in indices]
            if smooth or len(indices) != 4:
                faces.append(([len(verts) + k for k in range(len(indices))], uvs, material, smooth))
                verts.extend(tuple(c) for c in corners)
                continue
            p0, p1, p2, p3 = corners
            nu = max(1, math.ceil(max((p1 - p0).length, (p2 - p3).length) / self.grid - 1e-6))
            nv = max(1, math.ceil(max((p3 - p0).length, (p2 - p1).length) / self.grid - 1e-6))
            uv0, uv1, uv2, uv3 = (Vector(uv) for uv in uvs)

            def at(s, t, a0, a1, a2, a3):
                # Bilinear: corners are counter-clockwise from (0, 0).
                return (a0 * (1 - s) + a1 * s) * (1 - t) + (a3 * (1 - s) + a2 * s) * t

            # Cells share the grid's vertices, so the export stays compact.
            base = len(verts)
            for j in range(nv + 1):
                for i in range(nu + 1):
                    verts.append(tuple(at(i / nu, j / nv, p0, p1, p2, p3)))
            for j in range(nv):
                for i in range(nu):
                    cell = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                    faces.append(([base + b * (nu + 1) + a for a, b in cell],
                                  [tuple(at(a / nu, b / nv, uv0, uv1, uv2, uv3)) for a, b in cell],
                                  material, smooth))
        return verts, faces

    def build(self, name, materials, collection):
        # The lint sees the authored faces; tessellation comes after, so a
        # split face is never compared with its own cells.
        self.lint_coplanar(name)
        verts, built_faces = self._tessellated()
        mesh = bpy.data.meshes.new(PREFIX + name)
        mesh.from_pydata(verts, [], [f[0] for f in built_faces])

        used = []
        for _, _, material, _ in built_faces:
            if material not in used:
                used.append(material)
        for material in used:
            mesh.materials.append(materials[material])

        uv_layer = mesh.uv_layers.new(name="UVMap")
        loop = 0
        for polygon, (_, uvs, material, smooth) in zip(mesh.polygons, built_faces):
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
    # Decals: grime splashed up the ground-floor boards beside the door.
    gy = front - DECAL_OFFSET
    for x0, x1 in ((-3.0, -1.9), (1.6, 2.9)):
        m.quad([(x0, gy, 0.3), (x1, gy, 0.3), (x1, gy, 0.95), (x0, gy, 0.95)], "grime")
    # Noren: a torn shop curtain hanging from a rod in front of the door.
    y_noren = front - 0.3
    m.cylinder((-0.7, y_noren, 2.42), 0.02, 0.04, "wood_dark", segments=6)
    m.box((0.3, y_noren, 2.44), (2.1, 0.04, 0.04), "wood_dark")
    m.quad([(-0.55, y_noren, 1.45), (1.15, y_noren, 1.45), (1.15, y_noren, 2.42), (-0.55, y_noren, 2.42)],
           "noren")
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
    m.box((0, upper_center_y, z0 + upper_h / 2), (width, upper_depth, upper_h), "plaster", faces=NO_BOTTOM)
    m.box((0, upper_center_y, z0 + 0.12), (width + 0.04, upper_depth + 0.04, 0.24), "wood_dark")
    # A leak has stained the plaster beside the upper window.
    sy = upper_center_y - upper_depth / 2 - DECAL_OFFSET
    m.quad([(2.1, sy, z0 + 0.35), (3.0, sy, z0 + 0.35), (3.0, sy, z0 + 2.2), (2.1, sy, z0 + 2.2)], "water_stain")
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
    m.box((0, 0, 0.09), (0.58, 0.42, 0.18), "cloth_hakama", faces=NO_BOTTOM)
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


def build_shrine_gate(materials, collection):
    """A small roofed wooden gate with barred double doors."""
    m = MeshBuilder()
    for x in (-1.35, 1.35):
        m.box((x, 0, 1.3), (0.22, 0.22, 2.6), "wood_dark")
    m.box((0, 0, 2.5), (3.2, 0.26, 0.2), "wood_dark")               # lintel
    m.box((0, 0, 0.04), (2.7, 0.3, 0.08), "wood_dark")              # threshold
    for x in (-0.63, 0.63):
        m.box((x, 0, 1.2), (1.24, 0.08, 2.24), "wood_light")         # doors
        m.box((x, -0.05, 1.8), (1.1, 0.03, 0.08), "metal_dark")      # iron bands
        m.box((x, -0.05, 0.6), (1.1, 0.03, 0.08), "metal_dark")
    m.box((0, -0.07, 1.25), (1.6, 0.06, 0.14), "wood_dark")          # the bar
    # Paper talismans pasted on the doors, between the bar and the band.
    oy = -0.04 - DECAL_OFFSET
    for x in (-0.95, -0.35, 0.33, 0.92):
        m.quad([(x - 0.06, oy, 1.36), (x + 0.06, oy, 1.36), (x + 0.06, oy, 1.74), (x - 0.06, oy, 1.74)], "ofuda")
    for side in (-1, 1):
        m.box((0, side * 0.42, 2.83), (3.7, 0.95, 0.09), "roof_tile", rotation=rot_x(-side * 24))
    m.box((0, 0, 3.02), (3.8, 0.18, 0.14), "roof_tile")
    return m.build("shrine_gate", materials, collection)


def build_toro(materials, collection):
    """Stone lantern with a glowing paper window."""
    m = MeshBuilder()
    m.box((0, 0, 0.1), (0.62, 0.62, 0.2), "stone")
    m.cylinder((0, 0, 0.2), 0.13, 0.8, "stone", segments=8)
    m.box((0, 0, 1.05), (0.5, 0.5, 0.1), "stone")
    m.box((0, 0, 1.3), (0.42, 0.42, 0.4), "stone")
    for normal in ((0, -1), (0, 1), (-1, 0), (1, 0)):
        nx, ny = normal
        # 6 mm proud of the firebox: closer would z-fight at a distance.
        x, y = nx * 0.216, ny * 0.216
        tx, ty = -ny, nx  # tangent along the face
        corners = [
            (x - tx * 0.12, y - ty * 0.12, 1.18),
            (x + tx * 0.12, y + ty * 0.12, 1.18),
            (x + tx * 0.12, y + ty * 0.12, 1.42),
            (x - tx * 0.12, y - ty * 0.12, 1.42),
        ]
        m.quad(corners, "lantern_paper")
    m.box((0, 0, 1.56), (0.72, 0.72, 0.12), "stone")
    m.box((0, 0, 1.68), (0.4, 0.4, 0.12), "stone")
    m.box((0, 0, 1.8), (0.14, 0.14, 0.12), "stone")
    return m.build("toro", materials, collection)


def build_cedar(materials, collection):
    """A tall cedar: bark trunk and stacked, turned tiers of foliage."""
    m = MeshBuilder()
    m.cylinder((0, 0, 0), 0.34, 10.5, "bark", segments=8)
    tiers = [(3.0, 3.4), (4.8, 2.9), (6.4, 2.4), (7.8, 1.9), (9.0, 1.4), (10.0, 0.9)]
    for i, (z, width) in enumerate(tiers):
        m.box((0, 0, z), (width, width, 1.5), "foliage", rotation=rot_z(20 * i))
    return m.build("cedar", materials, collection)


def _cards(m, center, width, height, material, count=3, turn=0.0):
    """`count` vertical cards crossing at `center` (bottom centre), evenly
    turned: from any side, some card faces you."""
    cx, cy, cz = center
    for i in range(count):
        angle = math.radians(turn + 180.0 * i / count)
        dx, dy = math.cos(angle) * width / 2, math.sin(angle) * width / 2
        m.quad([(cx - dx, cy - dy, cz), (cx + dx, cy + dy, cz),
                (cx + dx, cy + dy, cz + height), (cx - dx, cy - dy, cz + height)], material)


def build_bush(materials, collection):
    """A shrub that shows over a 1.4 m wall: crossed leaf cards."""
    m = MeshBuilder()
    _cards(m, (0, 0, 0), 1.5, 1.7, "leaves", count=3)
    _cards(m, (0.3, 0.2, 0.5), 1.1, 1.3, "leaves", count=2, turn=30)
    return m.build("bush", materials, collection)


def build_grass_tuft(materials, collection):
    m = MeshBuilder()
    _cards(m, (0, 0, 0), 0.7, 0.5, "grass", count=3, turn=15)
    return m.build("grass_tuft", materials, collection)


def build_tree(materials, collection):
    """A broadleaf tree: bark trunk and a canopy of leaf-card clusters."""
    m = MeshBuilder()
    m.cylinder((0, 0, 0), 0.16, 3.4, "bark", segments=8)
    clusters = [((0, 0, 2.6), 2.4), ((0.9, 0.3, 2.9), 1.8), ((-0.8, 0.5, 3.0), 1.9),
                ((0.2, -0.9, 2.8), 1.8), ((-0.3, -0.2, 3.6), 2.0), ((0.6, 0.8, 3.4), 1.6)]
    for i, (center, size) in enumerate(clusters):
        cx, cy, cz = center
        _cards(m, (cx, cy, cz - size / 2), size, size, "leaves", count=3, turn=25 * i)
    return m.build("tree", materials, collection)


def build_chain_fence(materials, collection):
    """A 4 m section of rusty chain-link between two posts."""
    m = MeshBuilder()
    for x in (-2.0, 2.0):
        m.cylinder((x, 0, 0), 0.03, 1.9, "metal_dark", segments=6)
    m.box((0, 0, 1.85), (4.0, 0.04, 0.04), "metal_dark")
    m.quad([(-1.97, 0, 0.05), (1.97, 0, 0.05), (1.97, 0, 1.83), (-1.97, 0, 1.83)], "chain_link",
           uvs=[(0, 0), (7.9, 0), (7.9, 3.6), (0, 3.6)])
    return m.build("chain_fence", materials, collection)


def _animate(obj, action_name, data_path, keys, index=-1, interpolation="BEZIER"):
    """Keyframes `data_path` of `obj` at (frame, value) pairs and names the
    action, which the glTF exporter writes as an animation clip."""
    preferences = bpy.context.preferences.edit
    saved = preferences.keyframe_new_interpolation_type
    preferences.keyframe_new_interpolation_type = interpolation
    for frame, value in keys:
        if index >= 0:
            getattr(obj, data_path)[index] = value
        else:
            setattr(obj, data_path, value)
        obj.keyframe_insert(data_path=data_path, index=index, frame=frame)
    preferences.keyframe_new_interpolation_type = saved
    obj.animation_data.action.name = action_name


def _child(builder, name, materials, collection, parent, location):
    obj = builder.build(name, materials, collection)
    obj.parent = parent
    obj.location = location
    return obj


def build_windmill(materials, collection):
    """A wooden farm windmill: a tapering tower and a four-sailed rotor
    (child object) turning on its hub. Clip "spin": one turn in 8 s."""
    m = MeshBuilder()
    m.box((0, 0, 0.25), (2.2, 2.2, 0.5), "stone")
    for z0, z1, w in ((0.5, 2.9, 1.6), (2.9, 5.3, 1.3), (5.3, 7.4, 1.0)):
        m.box((0, 0, (z0 + z1) / 2), (w, w, z1 - z0), "wood_dark")
        m.box((0, 0, z1 - 0.04), (w + 0.12, w + 0.12, 0.1), "wood_light")  # trim band, 1 cm proud
    for side in (-1, 1):
        m.box((side * 0.32, 0, 7.75), (0.75, 1.5, 0.08), "roof_tile", rotation=rot_y(side * 32))
    m.box((0, -0.62, 6.6), (0.5, 0.25, 0.5), "black")          # hatch
    tower = m.build("windmill", materials, collection)

    rotor = MeshBuilder()
    rotor.box((0, 0, 0), (0.34, 0.5, 0.34), "wood_dark")        # hub
    for i in range(4):
        angle = 90.0 * i + 45.0
        rotation = rot_y(angle)
        # A spar from the hub, and a sail beside it (radial along local +Z).
        rotor.box(tuple(rotation @ Vector((0, 0, 1.45))), (0.09, 0.09, 2.9), "wood_light", rotation=rotation)
        rotor.box(tuple(rotation @ Vector((0.34, 0.03, 1.75))), (0.55, 0.02, 2.1), "cloth_white", rotation=rotation)
    blades = _child(rotor, "windmill_rotor", materials, collection, tower, (0, -0.86, 6.9))
    _animate(blades, "spin", "rotation_euler", [(0, 0.0), (192, -2.0 * math.pi)], index=1,
             interpolation="LINEAR")
    return tower


def build_shed(materials, collection):
    """A tool shed; its sliding door (child) opens with clip "open"."""
    m = MeshBuilder()
    width, depth, height = 3.2, 2.6, 2.4
    front = -depth / 2
    m.box((0, depth / 2 - 0.05, height / 2), (width, 0.1, height), "wood_dark")
    for x in (-width / 2 + 0.05, width / 2 - 0.05):
        m.box((x, 0, height / 2), (0.1, depth, height), "wood_dark")
    # Front wall with a doorway 1.2 m wide, 2.0 m high.
    m.box((-1.0, front + 0.05, height / 2), (1.2, 0.1, height), "wood_dark")
    m.box((1.2, front + 0.05, height / 2), (0.8, 0.1, height), "wood_dark")
    m.box((0.2, front + 0.05, 2.2), (1.2, 0.1, 0.4), "wood_dark")
    m.box((0, 0.2, height - 0.2), (width - 0.2, depth - 0.3, 0.05), "black", faces=[(0, 0, -1)])  # inside ceiling
    m.box((0, 0.1, 0.02), (width - 0.2, depth - 0.3, 0.04), "black", faces=[(0, 0, 1)])     # inside floor
    m.box((0, depth / 2 - 0.12, height / 2), (width - 0.2, 0.04, height), "black", faces=[(0, -1, 0)])
    for side in (-1, 1):
        m.box((0, side * 0.72, height + 0.28), (width + 0.4, 1.7, 0.08), "roof_tile", rotation=rot_x(-side * 20))
    shed = m.build("shed", materials, collection)

    door = MeshBuilder()
    door.box((0, 0, 1.0), (1.3, 0.05, 2.02), "wood_light")
    for z in (0.5, 1.5):
        door.box((0, -0.035, z), (1.2, 0.02, 0.08), "wood_dark")
    board = _child(door, "shed_door", materials, collection, shed, (0.2, front - 0.06, 0.0))
    _animate(board, "open", "location", [(0, 0.2), (36, -1.15)], index=0)
    return shed


def build_hanging_sign(materials, collection):
    """A signboard hanging from a post's arm; the board (child) swings on
    its pivot with clip "swing"."""
    m = MeshBuilder()
    m.box((0, 0, 1.3), (0.14, 0.14, 2.6), "wood_dark")
    m.box((0.45, 0, 2.5), (0.9, 0.1, 0.1), "wood_dark")
    post = m.build("hanging_sign", materials, collection)

    sign = MeshBuilder()
    sign.box((0, 0, -0.42), (0.8, 0.04, 0.5), "wood_light")
    for y in (-0.02 - DECAL_OFFSET, 0.02 + DECAL_OFFSET):
        facing = -1 if y < 0 else 1
        x0, x1 = (-0.36, 0.36) if facing < 0 else (0.36, -0.36)
        sign.quad([(x0, y, -0.62), (x1, y, -0.62), (x1, y, -0.22), (x0, y, -0.22)], "shop_sign")
    board = _child(sign, "hanging_sign_board", materials, collection, post, (0.7, 0, 2.45))
    swing = math.radians(9.0)
    _animate(board, "swing", "rotation_euler",
             [(0, 0.0), (18, swing), (36, 0.0), (54, -swing), (72, 0.0)], index=0)
    return post


def build_street_lamp(materials, collection):
    """A city street lamp: a pole, an arm, and a head whose glass faces down
    (the glow comes from its emissive mask, the light from the bake)."""
    m = MeshBuilder()
    m.cylinder((0, 0, 0), 0.07, 4.6, "metal_dark", segments=8)
    m.box((0.55, 0, 4.55), (1.1, 0.08, 0.08), "metal_dark")
    m.box((1.05, 0, 4.47), (0.42, 0.26, 0.14), "metal_dark", faces=NO_BOTTOM)
    m.quad([(0.86, 0.12, 4.40), (1.24, 0.12, 4.40), (1.24, -0.12, 4.40), (0.86, -0.12, 4.40)], "lamp_glass")
    return m.build("street_lamp", materials, collection)


def build_neon_sign(materials, collection):
    """A vertical shop sign on a bracket, lettered both sides."""
    m = MeshBuilder()
    m.box((0, 0, 3.2), (0.1, 0.5, 0.06), "metal_dark")               # bracket
    m.box((0, -0.35, 2.05), (0.06, 0.06, 2.4), "metal_dark")          # rail
    board_x0, board_x1, z0, z1 = -0.3, 0.3, 1.0, 3.1
    for y, flip in ((-0.03, False), (0.03, True)):
        xs = (board_x1, board_x0) if flip else (board_x0, board_x1)
        m.quad([(xs[0], y, z0), (xs[1], y, z0), (xs[1], y, z1), (xs[0], y, z1)], "neon_sign",
               uvs=[(0, 0), (1, 0), (1, 1), (0, 1)])
    m.box((0, 0, (z0 + z1) / 2), (0.62, 0.05, z1 - z0 + 0.02), "metal_dark", faces=SIDES[:2])
    m.box((0, 0, z1 + 0.02), (0.62, 0.06, 0.04), "metal_dark")
    m.box((0, 0, z0 - 0.02), (0.62, 0.06, 0.04), "metal_dark")
    return m.build("neon_sign", materials, collection)


def build_night_train(materials, collection):
    """Two commuter cars for the night city's elevated line (M25), 37 m
    long along Y, wheels on the ground plane (the entity sits on the rail
    deck). Lit windows glow from their mask; headlights at both ends."""
    m = MeshBuilder(grid=4.0)
    length, width, height, floor = 18.0, 2.8, 3.2, 0.5
    for centre in (-9.25, 9.25):
        y0, y1 = centre - length / 2, centre + length / 2
        x = width / 2
        side_uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
        m.quad([(x, y0, floor), (x, y1, floor), (x, y1, floor + height), (x, y0, floor + height)],
               "train_side", uvs=side_uvs)
        m.quad([(-x, y1, floor), (-x, y0, floor), (-x, y0, floor + height), (-x, y1, floor + height)],
               "train_side", uvs=side_uvs)
        m.box((0, centre, floor + height + 0.1), (width, length, 0.2), "metal_white", faces=[(0, 0, 1)])
        for y, normal in ((y0, (0, -1, 0)), (y1, (0, 1, 0))):
            m.box((0, y, floor + height / 2), (width, 0.02, height), "metal_white", faces=[normal])
        m.box((0, centre, floor / 2 + 0.05), (width - 0.4, length - 2.0, floor - 0.1), "black", faces=SIDES)
    for y, sign in ((-18.51, -1), (18.51, 1)):
        for x in (-0.9, 0.9):
            # Counter-clockwise seen from outside the end.
            m.quad([(x + 0.18 * sign, y, 1.3), (x - 0.18 * sign, y, 1.3), (x - 0.18 * sign, y, 1.55), (x + 0.18 * sign, y, 1.55)],
                   "lamp_glass")
    return m.build("night_train", materials, collection)


def build_bus_stop(materials, collection):
    """A rural bus stop (M26): a steel shelter with a bench and posters,
    the stop's round sign and a back-lit timetable on a post at its east
    end. Faces the road (-Y)."""
    m = MeshBuilder()
    width, back = 3.0, 0.5
    m.box((0, back, 1.2), (width, 0.06, 2.2), "metal_white", faces=SIDES)
    for x in (-width / 2, width / 2):
        m.box((x, 0.05, 1.25), (0.08, 0.95, 2.5), "metal_dark", faces=SIDES + [(0, 0, 1)])
    m.box((0, 0.0, 2.55), (width + 0.3, 1.3, 0.1), "metal_dark")
    m.box((0, 0.25, 0.45), (width - 0.4, 0.4, 0.06), "wood_light")
    for x in (-0.9, 0.9):
        m.box((x, 0.25, 0.21), (0.06, 0.3, 0.42), "metal_dark", faces=SIDES)
    y = back - 0.03 - DECAL_OFFSET
    m.quad([(-1.2, y, 0.7), (1.2, y, 0.7), (1.2, y, 2.0), (-1.2, y, 2.0)], "posters")
    px = width / 2 + 0.6
    m.cylinder((px, -0.2, 0), 0.05, 2.5, "metal_dark", segments=8)
    m.box((px, -0.2, 1.5), (0.5, 0.06, 0.7), "metal_dark", faces=SIDES[:2] + [(0, 0, 1), (0, 0, -1)])
    for yy, flip in ((-0.24, False), (-0.16, True)):
        xs = (px + 0.25, px - 0.25) if flip else (px - 0.25, px + 0.25)
        m.quad([(xs[0], yy, 1.17), (xs[1], yy, 1.17), (xs[1], yy, 1.83), (xs[0], yy, 1.83)], "timetable")
    m.cylinder((px, -0.2, 2.5), 0.25, 0.04, "metal_white", segments=12)
    return m.build("bus_stop", materials, collection)


def build_bus(materials, collection):
    """The night bus (M26): 10.5 m along X, driver's end at -X, doors on
    the +Y side near the front. Lit windows glow from the train's mask;
    headlights and a lit destination sign at the front. The door (child)
    folds open with clip "doors_open"."""
    m = MeshBuilder(grid=4.0)
    length, width, height, floor = 10.5, 2.5, 2.9, 0.35
    x0, x1, y = -length / 2, length / 2, width / 2
    uvs = [(0.05, 0), (0.62, 0), (0.62, 1), (0.05, 1)]
    m.quad([(x1, y, floor), (x0, y, floor), (x0, y, floor + height), (x1, y, floor + height)], "train_side", uvs=uvs)
    m.quad([(x0, -y, floor), (x1, -y, floor), (x1, -y, floor + height), (x0, -y, floor + height)], "train_side", uvs=uvs)
    m.box((0, 0, floor + height + 0.08), (length, width, 0.16), "metal_white", faces=[(0, 0, 1), (1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0)])
    m.box((x1, 0, floor + height / 2), (0.02, width, height), "metal_white", faces=[(1, 0, 0)])
    m.box((x0, 0, floor + 0.6), (0.02, width, 1.2), "metal_white", faces=[(-1, 0, 0)])
    m.quad([(x0 - 0.02, y - 0.1, floor + 1.25), (x0 - 0.02, -y + 0.1, floor + 1.25),
            (x0 - 0.02, -y + 0.1, floor + 2.45), (x0 - 0.02, y - 0.1, floor + 2.45)], "black")
    m.quad([(x0 - 0.02, 0.8, floor + 2.5), (x0 - 0.02, -0.8, floor + 2.5),
            (x0 - 0.02, -0.8, floor + 2.8), (x0 - 0.02, 0.8, floor + 2.8)], "timetable")
    for yy in (-0.85, 0.85):
        m.quad([(x0 - 0.03, yy + 0.18, floor + 0.35), (x0 - 0.03, yy - 0.18, floor + 0.35),
                (x0 - 0.03, yy - 0.18, floor + 0.55), (x0 - 0.03, yy + 0.18, floor + 0.55)], "lamp_glass")
    for wx in (x0 + 2.0, x1 - 2.5):
        for side in (-1, 1):
            m.box((wx, side * (y - 0.13), 0.45), (0.95, 0.3, 0.9), "black", faces=[(0, side, 0)])  # wheels, 2 cm proud
    m.box((0, 0, floor / 2 + 0.05), (length - 0.4, width - 0.3, floor - 0.1), "black", faces=SIDES)
    bus = m.build("bus", materials, collection)

    door = MeshBuilder()
    door.box((0, 0, 1.25), (1.0, 0.04, 2.3), "metal_white")
    door.quad([(0.4, 0.03, 0.9), (-0.4, 0.03, 0.9), (-0.4, 0.03, 2.2), (0.4, 0.03, 2.2)], "black")
    leaf = _child(door, "bus_door", materials, collection, bus, (x0 + 1.6, y + 0.03, floor))
    _animate(leaf, "doors_open", "location", [(0, x0 + 1.6), (24, x0 + 2.55)], index=0)
    return bus


def build_signpost(materials, collection):
    """A wooden field-path marker (static)."""
    m = MeshBuilder()
    m.box((0, 0, 0.8), (0.12, 0.12, 1.6), "wood_dark")
    m.box((0.3, 0, 1.35), (0.7, 0.05, 0.22), "wood_light")
    return m.build("signpost", materials, collection)


def build_haiden(materials, collection):
    """Main hall: raised platform with steps, lattice doors, deep gable roof.
    Front (-Y) steps rise 0.18 m each, low enough to walk up."""
    m = MeshBuilder()
    width, depth, height = 10.0, 7.0, 0.9
    m.box((0, 0, height / 2), (width, depth, height), "wood_dark")
    m.box((0, 0, height + 0.02), (width, depth, 0.04), "wood_floor", faces=[(0, 0, 1)])
    front = -depth / 2
    for k in range(4):
        top = height - 0.18 * (k + 1)
        m.box((0, front - 0.3 * (k + 0.5), top / 2), (3.2, 0.3, top), "wood_light")

    body_y = 0.6
    body_depth = 5.0
    z0 = height
    m.box((0, body_y, z0 + 1.6), (8.0, body_depth, 3.2), "plaster", faces=SIDES)
    m.box((0, body_y, z0 + 0.15), (8.04, body_depth + 0.04, 0.3), "wood_dark")
    y = body_y - body_depth / 2 - 0.02
    m.quad([(-3.0, y, z0 + 0.3), (3.0, y, z0 + 0.3), (3.0, y, z0 + 2.7), (-3.0, y, z0 + 2.7)], "door_lattice",
           uvs=[(0, 0), (10, 0), (10, 4), (0, 4)])
    for x in (-3.9, -3.05, 3.05, 3.9):
        m.box((x, body_y - body_depth / 2 - 0.1, z0 + 1.6), (0.26, 0.26, 3.2), "wood_dark", faces=NO_BOTTOM)
    # Shimenawa: a straw rope across the doors.
    m.box((0, y - 0.15, z0 + 2.9), (6.4, 0.22, 0.22), "straw")
    for x in (-2.0, 0.0, 2.0):
        m.box((x, y - 0.15, z0 + 2.55), (0.12, 0.12, 0.5), "straw")

    pitch = 30.0
    overhang = 1.4
    eave_z = z0 + 3.2
    half_span = body_depth / 2 + overhang
    rise = half_span * math.tan(math.radians(pitch))
    slab = half_span / math.cos(math.radians(pitch))
    for side in (-1, 1):
        center_y = body_y + side * half_span / 2
        center_z = eave_z + rise / 2 - overhang * math.tan(math.radians(pitch))
        m.box((0, center_y, center_z), (width + 0.8, slab, 0.18), "roof_tile", rotation=rot_x(-side * pitch))
    ridge_z = eave_z + rise - overhang * math.tan(math.radians(pitch))
    m.box((0, body_y, ridge_z + 0.1), (width + 1.0, 0.45, 0.35), "roof_tile")
    inner_rise = (body_depth / 2) * math.tan(math.radians(pitch))
    tile = tile_of("plaster")
    for side in (-1, 1):
        x = side * 4.0
        a = (x, body_y - body_depth / 2, eave_z)
        b = (x, body_y + body_depth / 2, eave_z)
        c = (x, body_y, eave_z + inner_rise)
        corners = [a, b, c] if side > 0 else [b, a, c]
        m.tri(corners, "plaster", [((p[1]) / tile, (p[2] - eave_z) / tile) for p in corners])
    return m.build("haiden", materials, collection)


def build_offering_box(materials, collection):
    """Saisen-bako: slatted offering box."""
    m = MeshBuilder()
    m.box((0, 0, 0.3), (1.2, 0.6, 0.6), "wood_dark")
    m.quad([(-0.55, -0.25, 0.605), (0.55, -0.25, 0.605), (0.55, 0.25, 0.605), (-0.55, 0.25, 0.605)], "lattice",
           uvs=[(0, 0), (2, 0), (2, 1), (0, 1)])
    return m.build("offering_box", materials, collection)


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
    build_shrine_gate,
    build_toro,
    build_cedar,
    build_haiden,
    build_offering_box,
    build_bush,
    build_grass_tuft,
    build_tree,
    build_chain_fence,
    build_windmill,
    build_shed,
    build_hanging_sign,
    build_signpost,
    build_street_lamp,
    build_neon_sign,
    build_night_train,
    build_bus_stop,
    build_bus,
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
    "shrine_gate": [((0, 0, 1.3), (3.0, 0.4, 2.6))],
    "toro": [((0, 0, 0.9), (0.66, 0.66, 1.8))],
    "cedar": [((0, 0, 1.5), (0.8, 0.8, 3.0))],
    # Platform top is walkable; steps rise 0.18 m; the hall body blocks.
    "haiden": [((0, 0, 0.45), (10.0, 7.0, 0.9))]
        + [((0, -3.5 - 0.3 * (k + 0.5), (0.9 - 0.18 * (k + 1)) / 2), (3.2, 0.3, 0.9 - 0.18 * (k + 1))) for k in range(4)]
        + [((0, 0.6, 0.9 + 1.6), (8.0, 5.0, 3.2))],
    "offering_box": [((0, 0, 0.3), (1.2, 0.6, 0.6))],
    "bush": [((0, 0, 0.6), (1.0, 1.0, 1.2))],
    "tree": [((0, 0, 1.5), (0.4, 0.4, 3.0))],
    "chain_fence": [((0, 0, 0.95), (4.0, 0.1, 1.9))],
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
