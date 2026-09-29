"""The city's middle and far layers (M24).

A small playable district should imply a larger city. Beyond the walkable
street the player sees three representations, each cheaper than the last:

- Middle (40-150 m): simple box shells with a facade atlas and lit windows
  from an emissive mask. No interior, collision, shadows or bake.
- Impostors: one detailed building pre-rendered from 8 directions into an
  atlas; the engine shows the view closest to the camera on a card.
- Far: rings of skyline cards (alpha-tested silhouettes with lit windows)
  at increasing distances, for a little parallax in front of the panorama.
"""

import json
import math
import os
import tempfile

import bpy
import numpy as np
from mathutils import Vector

import atom_kit as kit
import atom_lightmap

# Quadrant (u0, v0) of each facade style in the atlas (see
# atom_textures.FACADE_STYLES): index % 2 across, index // 2 up.
def _style_uv(style):
    u0, v0 = (style % 2) * 0.5, (style // 2) * 0.5
    return u0, v0, u0 + 0.5, v0 + 0.5


def facade_building(m, cx, cy, width, depth, height, style, tank=False, antenna=False):
    """A box shell: four facades mapped once each onto one atlas quadrant
    (a floor is ~3 m, the quadrant holds 7-8 floors), a flat roof and a
    few roof silhouettes."""
    u0, v0, u1, v1 = _style_uv(style)
    x0, x1 = cx - width / 2, cx + width / 2
    y0, y1 = cy - depth / 2, cy + depth / 2
    uvs = [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]
    # Counter-clockwise seen from outside.
    m.quad([(x0, y0, 0), (x1, y0, 0), (x1, y0, height), (x0, y0, height)], "facade_atlas", uvs=uvs)  # -Y
    m.quad([(x1, y1, 0), (x0, y1, 0), (x0, y1, height), (x1, y1, height)], "facade_atlas", uvs=uvs)  # +Y
    m.quad([(x1, y0, 0), (x1, y1, 0), (x1, y1, height), (x1, y0, height)], "facade_atlas", uvs=uvs)  # +X
    m.quad([(x0, y1, 0), (x0, y0, 0), (x0, y0, height), (x0, y1, height)], "facade_atlas", uvs=uvs)  # -X
    m.box((cx, cy, height + 0.3), (width + 0.2, depth + 0.2, 0.6), "concrete", faces=kit.NO_BOTTOM)
    if tank:
        m.box((cx + width * 0.2, cy, height + 1.8), (2.4, 2.4, 2.4), "metal_dark")
        m.box((cx + width * 0.2, cy, height + 0.8), (0.3, 0.3, 1.0), "metal_dark")
    if antenna:
        m.cylinder((cx - width * 0.25, cy + depth * 0.2, height + 0.6), 0.08, 6.0, "metal_dark", segments=6)


def build_mid_blocks(materials, collection):
    """A ring of building shells around the night test field, 45-90 m out,
    on a dark ground that continues past the field's edge."""
    m = kit.MeshBuilder(grid=1000.0)  # low poly: no bake to carry
    m.quad([(-400, -400, -0.03), (400, -400, -0.03), (400, 400, -0.03), (-400, 400, -0.03)], "asphalt",
           uvs=[(-100, -100), (100, -100), (100, 100), (-100, 100)])
    blocks = [
        (48, 20, 14, 12, 22), (52, -18, 16, 14, 30), (40, -52, 12, 16, 18), (-4, -62, 18, 12, 26),
        (-44, -46, 14, 14, 34), (-62, -24, 12, 18, 20), (-50, 34, 16, 12, 28), (-2, 68, 14, 14, 24),
        (30, 60, 18, 12, 32), (70, 44, 12, 12, 20), (78, -30, 14, 16, 36), (-78, -40, 16, 14, 22),
        (-82, 20, 12, 12, 30), (8, 88, 16, 16, 38),
    ]
    for i, (x, y, w, d, h) in enumerate(blocks):
        facade_building(m, x, y, w, d, h, style=i % 4, tank=i % 3 == 0, antenna=i % 4 == 1)
    return m.build("mid_blocks", materials, collection)


def build_skyline(materials, collection):
    """Three rings of inward-facing skyline cards. Further rings are taller
    so they show above nearer ones; their texture repeats at different
    rates so the silhouettes don't line up."""
    m = kit.MeshBuilder(grid=10000.0)
    for radius, height, repeats, segments in ((180.0, 45.0, 3, 24), (260.0, 75.0, 4, 28), (380.0, 120.0, 5, 32)):
        for i in range(segments):
            a0 = 2 * math.pi * i / segments
            a1 = 2 * math.pi * (i + 1) / segments
            p0 = (radius * math.cos(a0), radius * math.sin(a0))
            p1 = (radius * math.cos(a1), radius * math.sin(a1))
            u0 = repeats * i / segments
            u1 = repeats * (i + 1) / segments
            # Facing the centre: counter-clockwise seen from inside.
            m.quad([(p1[0], p1[1], -2.0), (p0[0], p0[1], -2.0), (p0[0], p0[1], height), (p1[0], p1[1], height)],
                   "skyline", uvs=[(u1, 0), (u0, 0), (u0, 1), (u1, 1)])
    return m.build("skyline", materials, collection)


def build_impostor_tower(materials, collection):
    """The detailed building the impostor is rendered from."""
    m = kit.MeshBuilder(grid=1000.0)
    facade_building(m, 0, 0, 14, 14, 40, style=3, tank=True, antenna=True)
    m.box((0, -7.4, 3.2), (12, 0.8, 0.3), "metal_dark")  # an awning over the shops
    return m.build("impostor_tower", materials, collection)


# --------------------------------------------------------------------------
# Impostor rendering
# --------------------------------------------------------------------------

VIEWS = 8
TILE = (128, 256)  # width, height in pixels per view


def render_impostor(scene, obj, png_path, json_path, fog_amount=0.6):
    """Renders `obj` from VIEWS directions around it (orthographic, like a
    card seen from far away) into one RGBA atlas, VIEWS tiles across.

    View k looks at the building from angle k * 360/VIEWS, measured from its
    front (-Y in Blender, +Z in the game) toward +X - the engine picks the
    tile the same way. Transparent texels take the colour of their opaque
    neighbours (dilation), so filtering never pulls in black edges. Cycles
    on the CPU with a fixed seed keeps it byte-identical across rebuilds."""
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    low = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
    high = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
    centre = (low + high) / 2
    footprint = math.hypot(high.x - low.x, high.y - low.y)  # widest from any side
    height = high.z - min(low.z, 0.0)
    # The tile is twice as tall as wide: the camera must fit both.
    card_height = max(height, footprint * TILE[1] / TILE[0]) * 1.04
    card_width = card_height * TILE[0] / TILE[1]

    saved = {
        "engine": scene.render.engine, "x": scene.render.resolution_x, "y": scene.render.resolution_y,
        "percent": scene.render.resolution_percentage, "transparent": scene.render.film_transparent,
        "camera": scene.camera, "transform": scene.view_settings.view_transform, "world": scene.world,
    }
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 24
    scene.cycles.seed = 0
    scene.cycles.use_denoising = False
    scene.render.resolution_x, scene.render.resolution_y = TILE
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.view_settings.view_transform = "Standard"  # plain sRGB, as the engine expects
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"

    # A dim night sky for fill light; the windows' emission does the rest.
    world = bpy.data.worlds.get("atom_impostor_world") or bpy.data.worlds.new("atom_impostor_world")
    world.use_nodes = True
    background = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    background.inputs["Color"].default_value = (0.05, 0.06, 0.10, 1.0)
    background.inputs["Strength"].default_value = 0.6
    scene.world = world

    hidden = [o for o in scene.objects if o is not obj and not o.hide_render]
    for o in hidden:
        o.hide_render = True

    camera_data = bpy.data.cameras.new("atom_impostor_camera")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = card_height  # the larger (vertical) side
    camera = bpy.data.objects.new("atom_impostor_camera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera

    atlas = np.zeros((TILE[1], TILE[0] * VIEWS, 4), dtype=np.float32)
    distance = footprint * 2.0 + 10.0
    temp = os.path.join(tempfile.gettempdir(), "atom_impostor_view.png")
    for k in range(VIEWS):
        angle = 2 * math.pi * k / VIEWS
        camera.location = (centre.x + distance * math.sin(angle),
                           centre.y - distance * math.cos(angle),
                           card_height / 2)
        look = Vector((centre.x, centre.y, card_height / 2)) - camera.location
        camera.rotation_euler = look.to_track_quat("-Z", "Y").to_euler()
        camera_data.clip_end = distance * 3
        scene.render.filepath = temp
        bpy.ops.render.render(write_still=True)
        image = bpy.data.images.load(temp, check_existing=False)
        pixels = np.array(image.pixels[:], dtype=np.float32).reshape(TILE[1], TILE[0], 4)
        bpy.data.images.remove(image)
        atlas[:, k * TILE[0]:(k + 1) * TILE[0]] = pixels[::-1]  # rows top first

    # Dilation: spread opaque colours outward into the transparent texels.
    rgb, alpha = atlas[:, :, :3].copy(), atlas[:, :, 3].copy()
    filled = alpha > 0.5
    for _ in range(12):
        grown = np.zeros_like(rgb)
        count = np.zeros(filled.shape)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            shifted = np.roll(np.roll(filled, dy, 0), dx, 1)
            grown += np.roll(np.roll(rgb, dy, 0), dx, 1) * shifted[:, :, None]
            count += shifted
        new = (~filled) & (count > 0)
        rgb[new] = grown[new] / count[new][:, None]
        filled |= new
    atlas = np.concatenate([rgb, alpha[:, :, None]], axis=2)

    encoded = (np.clip(atlas, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    atom_lightmap._write_png(png_path, TILE[0] * VIEWS, bytearray(encoded.tobytes()), TILE[1], channels=4)
    with open(json_path, "w", encoding="utf-8", newline="\n") as file:
        json.dump({
            "_generated": "by Tools/Blender/build_assets.py; do not edit",
            "atlas": os.path.basename(png_path),
            "views": VIEWS,
            "width": round(card_width, 3),
            "height": round(card_height, 3),
            "fog": fog_amount,
        }, file, indent=2, sort_keys=True)
        file.write("\n")

    for o in hidden:
        o.hide_render = False
    bpy.data.objects.remove(camera)
    bpy.data.cameras.remove(camera_data)
    scene.render.engine = saved["engine"]
    scene.render.resolution_x, scene.render.resolution_y = saved["x"], saved["y"]
    scene.render.resolution_percentage = saved["percent"]
    scene.render.film_transparent = saved["transparent"]
    scene.camera = saved["camera"]
    scene.view_settings.view_transform = saved["transform"]
    scene.world = saved["world"]
