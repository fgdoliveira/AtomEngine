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

import hashlib
import json
import math
import os
import struct
import zlib

import bpy
import numpy as np
from mathutils import Euler

import atom_kit

UV_LAYER = "lightmap"

# Bake cache: where fingerprints of finished bakes are kept (not committed;
# a fresh clone simply bakes everything once). None disables the cache.
CACHE_DIR = None
# GPU baking (build_assets --gpu): fast for iterating on light, but not
# byte-identical to the CPU (nor exactly repeatable). Such lightmaps carry
# GPU_MARKER in a PNG text chunk, and a test refuses them in Assets/.
USE_GPU = False
GPU_MARKER = b"atom-gpu-bake"
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


def point_light(location, watts, color, radius=0.15):
    """A bake-only point light (M25): a lamp head, a bare bulb."""
    return {"type": "POINT", "location": location, "watts": watts, "color": color, "radius": radius}


def sun_light(rotation, strength, color, angle=1.0):
    """A bake-only sun (M25): moonlight. `rotation` in degrees; strength in
    W/m^2; `angle` (degrees) softens its shadows."""
    return {"type": "SUN", "rotation": rotation, "watts": strength, "color": color, "angle": angle}


def _add_lights(lights, collection):
    """lights: area lights as (location, rotation in degrees, (width,
    height), watts, rgb), or the dicts made by point_light and sun_light."""
    objects = []
    for index, light in enumerate(lights):
        name = f"atom_bake_light_{index}"
        if isinstance(light, dict):
            data = bpy.data.lights.new(name, type=light["type"])
            if light["type"] == "POINT":
                data.shadow_soft_size = light["radius"]
            else:
                data.angle = math.radians(light["angle"])
            data.energy = light["watts"]
            data.color = light["color"]
            location = light.get("location", (0.0, 0.0, 0.0))
            rotation = light.get("rotation", (0.0, 0.0, 0.0))
        else:
            location, rotation, size, watts, color = light
            data = bpy.data.lights.new(name, type="AREA")
            data.shape = "RECTANGLE"
            data.size, data.size_y = size
            data.energy = watts
            data.color = color
        obj = bpy.data.objects.new(name, data)
        obj.location = location
        obj.rotation_euler = Euler([math.radians(a) for a in rotation])
        collection.objects.link(obj)
        objects.append(obj)
    return objects


def _write_png(path, width, rgb_bytes, height=None, channels=3, text=None):
    """Minimal deterministic PNG (8-bit RGB or RGBA, no metadata unless
    `text` is given: a tEXt chunk), rows top first."""
    height = width if height is None else height
    rows = bytearray()
    stride = width * channels
    for y in range(height):
        rows.append(0)  # filter: none
        rows.extend(rgb_bytes[y * stride:(y + 1) * stride])

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    colour_type = 6 if channels == 4 else 2  # RGBA or RGB
    header = struct.pack(">IIBBBBB", width, height, 8, colour_type, 0, 0, 0)
    with open(path, "wb") as file:
        file.write(b"\x89PNG\r\n\x1a\n")
        file.write(chunk(b"IHDR", header))
        if text:
            file.write(chunk(b"tEXt", b"Comment\x00" + text))
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


def _hash_mesh(h, obj, uv_layers):
    """Geometry that affects a bake: world transform, positions, faces,
    their materials and the named UV layers."""
    mesh = obj.data
    h.update(np.array(obj.matrix_world, dtype=np.float64).tobytes())
    co = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", co)
    h.update(co.tobytes())
    loops = np.empty(len(mesh.loops), dtype=np.int32)
    mesh.loops.foreach_get("vertex_index", loops)
    h.update(loops.tobytes())
    starts = np.empty(len(mesh.polygons), dtype=np.int32)
    mesh.polygons.foreach_get("loop_start", starts)
    material = np.empty(len(mesh.polygons), dtype=np.int32)
    mesh.polygons.foreach_get("material_index", material)
    h.update(starts.tobytes() + material.tobytes())
    for name in uv_layers:
        layer = mesh.uv_layers.get(name)
        if layer:
            uv = np.empty(len(mesh.loops) * 2, dtype=np.float32)
            layer.data.foreach_get("uv", uv)
            h.update(name.encode() + uv.tobytes())


def _hash_materials(h, objects, seen_images):
    """What a surface looks like to the bake: its images, its emission and
    alpha (every node input value, and every image's pixels)."""
    names = sorted({slot.material.name for o in objects for slot in o.material_slots if slot.material})
    for name in names:
        material = bpy.data.materials[name]
        h.update(name.encode())
        for node in sorted(material.node_tree.nodes, key=lambda n: n.name):
            h.update(node.bl_idname.encode())
            for socket in node.inputs:
                if hasattr(socket, "default_value"):
                    try:
                        h.update(repr(tuple(socket.default_value)).encode())
                    except TypeError:
                        h.update(repr(socket.default_value).encode())
            image = getattr(node, "image", None)
            if image is not None:
                if image.name not in seen_images:
                    pixels = np.empty(len(image.pixels), dtype=np.float32)
                    image.pixels.foreach_get(pixels)
                    seen_images[image.name] = hashlib.sha256(pixels.tobytes()).hexdigest()
                h.update(seen_images[image.name].encode())


_IMAGE_HASHES = {}


def fingerprint(obj, context, lights, size, samples, sky, clamp):
    """Everything a bake's result depends on, as one hash: the mesh (with
    its lightmap UVs), what surrounds it, the lights, the settings, the
    device, Blender's version and this file's code."""
    h = hashlib.sha256()
    h.update(open(__file__, "rb").read())
    h.update(bpy.app.version_string.encode())
    h.update(b"gpu" if USE_GPU else b"cpu")
    h.update(repr((size, samples, sky, clamp, MARGIN_PX, EXPOSURE, list(lights))).encode())
    render_uv = obj.data.uv_layers[0].name
    _hash_mesh(h, obj, [render_uv, UV_LAYER])
    for other in sorted(context, key=lambda o: o.name):
        h.update(other.name.encode())
        _hash_mesh(h, other, [other.data.uv_layers[0].name] if other.data.uv_layers else [])
    _hash_materials(h, [obj] + list(context), _IMAGE_HASHES)
    return h.hexdigest()


def _file_md5(path):
    return hashlib.md5(open(path, "rb").read()).hexdigest() if os.path.exists(path) else None


def _use_gpu(scene):
    """Cycles on the NVIDIA GPU: OptiX if available, else CUDA."""
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for backend in ("OPTIX", "CUDA"):
        try:
            prefs.compute_device_type = backend
        except TypeError:
            continue
        prefs.refresh_devices()
        devices = [d for d in prefs.devices if d.type == backend]
        if devices:
            for d in prefs.devices:
                d.use = d.type == backend
            scene.cycles.device = "GPU"
            return backend
    raise RuntimeError("--gpu: no OptiX or CUDA device found")


def bake(scene, obj, lights, png_path, size=512, samples=SAMPLES, context=(), sky=None, clamp=0.0):
    """Unwraps `obj`, bakes its light with `lights` and writes `png_path`.
    `context` objects stay in the scene while baking (M25): they cast
    shadows onto `obj`, bounce light onto it and, if emissive (signs, lit
    windows), light it. `sky` = (rgb, strength) lets a faint sky in; by
    default the sky is black. `clamp` > 0 caps indirect samples: small,
    bright emitters (neon) otherwise leave speckles (fireflies)."""
    name = obj.name
    _unwrap(obj, size)
    _lint(obj, name)

    # The cache: an unchanged bake keeps its PNG (the UVs above are still
    # made, the exported mesh needs them). A hit needs the fingerprint and
    # the file on disk to be exactly what the last bake left.
    # One record per device, so a --gpu session doesn't evict the CPU's.
    device = "gpu" if USE_GPU else "cpu"
    record_path = os.path.join(CACHE_DIR, f"{os.path.basename(png_path)}.{device}.json") if CACHE_DIR else None
    key = fingerprint(obj, context, lights, size, samples, sky, clamp) if record_path else None
    if record_path and os.path.exists(record_path):
        with open(record_path, encoding="utf-8") as file:
            record = json.load(file)
        if record.get("fingerprint") == key and record.get("png_md5") == _file_md5(png_path):
            print(f"lightmap: {name}: unchanged, bake skipped (cache)")
            return

    if USE_GPU:
        print(f"lightmap: {name}: baking on the GPU ({_use_gpu(scene)}) - not for committing")
    else:
        scene.cycles.device = "CPU"

    light_objects = _add_lights(lights, obj.users_collection[0])
    world = scene.world
    saved_strength = saved_color = None
    if world:
        background = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
        saved_strength = background.inputs["Strength"].default_value
        saved_color = tuple(background.inputs["Color"].default_value)
        background.inputs["Strength"].default_value = 0.0
        if sky:
            background.inputs["Color"].default_value = tuple(sky[0]) + (1.0,)
            background.inputs["Strength"].default_value = sky[1]

    # Only this mesh, its context and its lights take part.
    taking_part = set([obj] + list(context) + light_objects)
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

    scene.cycles.samples = samples
    saved_clamp = scene.cycles.sample_clamp_indirect
    scene.cycles.sample_clamp_indirect = clamp
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
    _write_png(png_path, size, rgb, text=GPU_MARKER if USE_GPU else None)
    if record_path:
        os.makedirs(CACHE_DIR, exist_ok=True)
        with open(record_path, "w", encoding="utf-8", newline="\n") as file:
            json.dump({"fingerprint": key, "png_md5": _file_md5(png_path)}, file, indent=2)

    luminance = sorted(max(pixels[i:i + 3]) for i in range(0, len(pixels), 4))
    print(f"lightmap: {name}: median {luminance[len(luminance) // 2]:.3f} "
          f"p90 {luminance[9 * len(luminance) // 10]:.3f} max {luminance[-1]:.3f}")

    scene.cycles.sample_clamp_indirect = saved_clamp
    for tree, node in nodes_added:
        tree.nodes.remove(node)
    bpy.data.images.remove(image)
    for o in light_objects:
        bpy.data.objects.remove(o)
    for o in hidden:
        o.hide_render = False
    if saved_strength is not None:
        background.inputs["Strength"].default_value = saved_strength
        background.inputs["Color"].default_value = saved_color
