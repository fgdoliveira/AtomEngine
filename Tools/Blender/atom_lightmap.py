"""Bakes a lightmap for one level mesh (M16).

A lightmap is a texture of baked light, mapped with a second UV set in
which no two faces overlap. Where vertex colours (atom_bake) only hold one
value per vertex, a lightmap resolves soft shadows and light pools across
a face - which a closed room lit through paper screens needs.

Steps:
1. A `lightmap` UV layer: Smart UV Project keeps flat, connected faces in
   one island (a wall's grid stays whole, so no seams inside it); islands
   are packed with a margin so bilinear filtering and the first mip
   levels don't bleed between them.
2. Cycles bakes diffuse direct + indirect light into a float image, lit by
   bake-only lights given by the level (daylight through the shoji and the
   door); the sky is black, so nothing leaks through the walls.
3. The result is scaled by EXPOSURE, clamped, encoded as sRGB (fine steps
   in the darks) and written by our own PNG writer, so the file is
   byte-identical across rebuilds.
4. Lint: lightmap UVs must lie in 0..1 and no two faces may overlap.
"""

import math
import struct
import zlib

import bpy
from mathutils import Euler

import atom_kit

UV_LAYER = "lightmap"
SAMPLES = 256
MARGIN_PX = 4
EXPOSURE = 1.0


def _unwrap(obj, size):
    mesh = obj.data
    render_uv = mesh.uv_layers.active_index
    layer = mesh.uv_layers.get(UV_LAYER) or mesh.uv_layers.new(name=UV_LAYER)
    mesh.uv_layers.active = layer  # unwrap and bake write the active layer
    for index, uv in enumerate(mesh.uv_layers):
        uv.active_render = index == render_uv  # textures keep their UVs

    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    margin = MARGIN_PX * 2.0 / size
    bpy.ops.uv.smart_project(angle_limit=math.radians(66.0), island_margin=margin,
                             area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    bpy.ops.uv.pack_islands(rotate=False, margin=margin)
    bpy.ops.object.mode_set(mode="OBJECT")


def _add_lights(lights, collection):
    """lights: (location, rotation in degrees, (width, height), watts, rgb)."""
    objects = []
    for index, (location, rotation, size, watts, color) in enumerate(lights):
        data = bpy.data.lights.new(f"atom_bake_light_{index}", type="AREA")
        data.shape = "RECTANGLE"
        data.size, data.size_y = size
        data.energy = watts
        data.color = color
        obj = bpy.data.objects.new(f"atom_bake_light_{index}", data)
        obj.location = location
        obj.rotation_euler = Euler([math.radians(a) for a in rotation])
        collection.objects.link(obj)
        objects.append(obj)
    return objects


def _write_png(path, size, rgb_bytes):
    """Minimal deterministic PNG (8-bit RGB, no metadata)."""
    rows = bytearray()
    stride = size * 3
    for y in range(size):
        rows.append(0)  # filter: none
        rows.extend(rgb_bytes[y * stride:(y + 1) * stride])

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    with open(path, "wb") as file:
        file.write(b"\x89PNG\r\n\x1a\n")
        file.write(chunk(b"IHDR", header))
        file.write(chunk(b"IDAT", zlib.compress(bytes(rows), 9)))
        file.write(chunk(b"IEND", b""))


def _to_srgb(value):
    value = min(max(value * EXPOSURE, 0.0), 1.0)
    if value <= 0.0031308:
        encoded = value * 12.92
    else:
        encoded = 1.055 * value ** (1.0 / 2.4) - 0.055
    return round(encoded * 255.0)


def _lint(obj, name):
    """UVs inside 0..1, and no pixel centre covered by two faces."""
    mesh = obj.data
    layer = mesh.uv_layers[UV_LAYER].data
    grid = 256
    owner = {}
    errors = []
    for polygon in mesh.polygons:
        uvs = [layer[i].uv.copy() for i in polygon.loop_indices]
        if any(not (0.0 <= c <= 1.0) for uv in uvs for c in uv):
            errors.append(f"lightmap: {name}: face {polygon.index} has UVs outside 0..1")
            continue
        # Triangle fan; a pixel counts only if strictly inside (1e-5), so
        # edges shared inside an island never count twice.
        for k in range(1, len(uvs) - 1):
            a, b, c = uvs[0], uvs[k], uvs[k + 1]
            area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)
            if abs(area) < 1e-12:
                continue
            x0 = max(0, int(min(a.x, b.x, c.x) * grid))
            x1 = min(grid - 1, int(max(a.x, b.x, c.x) * grid))
            y0 = max(0, int(min(a.y, b.y, c.y) * grid))
            y1 = min(grid - 1, int(max(a.y, b.y, c.y) * grid))
            for py in range(y0, y1 + 1):
                for px in range(x0, x1 + 1):
                    x, y = (px + 0.5) / grid, (py + 0.5) / grid
                    w0 = ((b.x - x) * (c.y - y) - (c.x - x) * (b.y - y)) / area
                    w1 = ((c.x - x) * (a.y - y) - (a.x - x) * (c.y - y)) / area
                    w2 = 1.0 - w0 - w1
                    if min(w0, w1, w2) <= 1e-5:
                        continue
                    other = owner.setdefault((px, py), polygon.index)
                    if other != polygon.index:
                        errors.append(f"lightmap: {name}: faces {other} and {polygon.index} overlap in the lightmap")
                        break
                else:
                    continue
                break
    for error in errors[:5]:
        print("lint ERROR: " + error)
    atom_kit.LINT_ERRORS.extend(errors)


def bake(scene, obj, lights, png_path, size=512):
    """Unwraps `obj`, bakes its light with `lights` and writes `png_path`."""
    name = obj.name
    _unwrap(obj, size)
    _lint(obj, name)

    light_objects = _add_lights(lights, obj.users_collection[0])
    world = scene.world
    saved_strength = None
    if world:
        background = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
        saved_strength = background.inputs["Strength"].default_value
        background.inputs["Strength"].default_value = 0.0

    # Only this mesh and its lights take part.
    taking_part = set([obj] + light_objects)
    hidden = [o for o in scene.objects if o not in taking_part and not o.hide_render]
    for o in hidden:
        o.hide_render = True

    image = bpy.data.images.new(atom_kit.PREFIX + name + "_lightmap", size, size, float_buffer=True)
    nodes_added = []
    for slot in obj.material_slots:
        tree = slot.material.node_tree
        node = tree.nodes.new("ShaderNodeTexImage")
        node.image = image
        for other in tree.nodes:
            other.select = False
        node.select = True
        tree.nodes.active = node
        nodes_added.append((tree, node))

    scene.cycles.samples = SAMPLES
    scene.render.bake.margin = MARGIN_PX
    scene.render.bake.use_clear = True
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.bake(type="DIFFUSE", pass_filter={"DIRECT", "INDIRECT"},
                        target="IMAGE_TEXTURES", margin=MARGIN_PX)

    pixels = [0.0] * (size * size * 4)
    image.pixels.foreach_get(pixels)
    # Blender's image rows start at the bottom; PNG rows at the top.
    rgb = bytearray(size * size * 3)
    for y in range(size):
        source = (size - 1 - y) * size * 4
        target = y * size * 3
        for x in range(size):
            for c in range(3):
                rgb[target + x * 3 + c] = _to_srgb(pixels[source + x * 4 + c])
    _write_png(png_path, size, rgb)

    luminance = sorted(max(pixels[i:i + 3]) for i in range(0, len(pixels), 4))
    print(f"lightmap: {name}: median {luminance[len(luminance) // 2]:.3f} "
          f"p90 {luminance[9 * len(luminance) // 10]:.3f} max {luminance[-1]:.3f}")

    for tree, node in nodes_added:
        tree.nodes.remove(node)
    bpy.data.images.remove(image)
    for o in light_objects:
        bpy.data.objects.remove(o)
    for o in hidden:
        o.hide_render = False
    if saved_strength is not None:
        background.inputs["Strength"].default_value = saved_strength
