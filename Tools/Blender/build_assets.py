"""Builds the AtomEngine kit and exports one .glb per piece.

Headless:
    blender -b --factory-startup -P Tools/Blender/build_assets.py

Writes Assets/Kit/<piece>.glb, Assets/Street/street.glb (visuals) and
Assets/Street/street_col.glb (collision proxies, no materials).

From a live Blender (e.g. Blender MCP), exec this file with __file__ set to
build a preview in a dedicated "AtomKit" scene; the open scene is untouched
and nothing is exported.
"""

import argparse
import importlib
import json
import math
import os
import sys
import time

import bpy
from mathutils import Vector

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
SCENE_NAME = "AtomKit"

if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

import atom_textures  # noqa: E402
import atom_kit  # noqa: E402
import atom_street  # noqa: E402
import atom_levels  # noqa: E402
import atom_bake  # noqa: E402
import atom_lightmap  # noqa: E402

# Pick up edits when re-run inside a long-lived Blender session.
importlib.reload(atom_textures)
importlib.reload(atom_kit)
importlib.reload(atom_street)
importlib.reload(atom_levels)
importlib.reload(atom_bake)
importlib.reload(atom_lightmap)


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=os.path.join(REPO_ROOT, "Assets"))
    parser.add_argument("--no-export", action="store_true")
    return parser.parse_args(argv)


def fresh_scene():
    if bpy.app.background:
        # Headless: the startup scene is ours; export needs it to be the
        # context scene, so build right there.
        scene = bpy.context.scene
        for obj in list(scene.objects):
            bpy.data.objects.remove(obj)
        return scene

    # Live session: build in a separate scene so the user's work is left
    # alone. Reuse it rather than deleting it; removing the scene a window
    # is displaying crashes Blender.
    scene = bpy.data.scenes.get(SCENE_NAME)
    if scene:
        for obj in list(scene.objects):
            bpy.data.objects.remove(obj)
    else:
        scene = bpy.data.scenes.new(SCENE_NAME)
    bpy.context.window.scene = scene
    return scene


def export_objects(objects, scene, path, materials=True):
    selected = set(objects)
    for obj in scene.objects:
        obj.select_set(obj in selected)

    # Windows sometimes holds a just-written file for a moment (antivirus,
    # indexer), making the next open fail with EINVAL; a short retry is
    # enough. Anything that persists is a real error.
    for attempt in range(5):
        try:
            bpy.ops.export_scene.gltf(
                filepath=path,
                export_format="GLB",
                use_selection=True,
                export_yup=True,
                export_apply=True,
                export_texcoords=materials,
                export_normals=True,
                export_materials="EXPORT" if materials else "NONE",
                export_image_format="AUTO",
                export_cameras=False,
                export_lights=False,
                export_extras=True,  # material custom props (atom_fog)
                export_vertex_color="NAME",
                export_vertex_color_name=atom_bake.ATTRIBUTE,
                export_all_vertex_colors=False,
            )
            break
        except RuntimeError as error:
            if attempt == 4:
                raise
            print(f"Export of {os.path.basename(path)} failed ({error}); retrying")
            time.sleep(0.5)
    print("Exported", os.path.relpath(path, REPO_ROOT))


def write_markers(collection, path):
    """Writes the level's spawn:/entity: empties as game-space placements
    (glTF axes: x, z, -y). Spawn yaw: the empty looks along its local +Y,
    game yaw 0 looks down -Z, so yaw = -rotation. Entity yaw: the model
    turns like a kit piece, yaw = rotation. Sorted and rounded, so the file
    is byte-identical across rebuilds. Returns the number of markers."""
    placements = {"spawns": {}, "entities": {}}
    for obj in collection.all_objects:
        if obj.type != "EMPTY" or ":" not in obj.name:
            continue
        kind, name = obj.name.split(":", 1)
        if kind not in ("spawn", "entity") or "." in name:
            atom_kit.LINT_ERRORS.append(f"marker {obj.name}: expected spawn:<name> or entity:<name>, unique")
            continue
        x, y, z = obj.matrix_world.translation
        rotation = math.degrees(obj.matrix_world.to_euler("XYZ").z)
        yaw = -rotation if kind == "spawn" else rotation
        placements["spawns" if kind == "spawn" else "entities"][name] = {
            "position": [round(x, 3), round(z, 3), round(-y, 3)],
            "yaw": round((yaw + 180.0) % 360.0 - 180.0, 2) + 0.0,
        }
    count = len(placements["spawns"]) + len(placements["entities"])
    if not count:
        if os.path.exists(path):
            os.remove(path)
        return 0
    document = {"_generated": "by Tools/Blender/build_assets.py from the level's markers; do not edit"}
    document.update(placements)
    with open(path, "w", encoding="utf-8", newline="\n") as file:
        json.dump(document, file, indent=2, sort_keys=True)
        file.write("\n")
    print("Exported", os.path.relpath(path, REPO_ROOT), f"({count} markers)")
    return count


# The street's chunks along X (Blender metres): name -> [x0, x1).
STREET_CHUNKS = {"west": (-1e9, -13.0), "centre": (-13.0, 13.0), "east": (13.0, 1e9)}


def split_into_chunks(objects, ranges, spanning=30.0):
    """Groups objects by the X range their origin falls in; objects wider
    than `spanning` metres go to "base". Deterministic: keeps input order."""
    groups = {"base": []}
    groups.update({name: [] for name in ranges})
    for obj in objects:
        corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
        width = max(c.x for c in corners) - min(c.x for c in corners)
        if width > spanning:
            groups["base"].append(obj)
            continue
        x = obj.matrix_world.translation.x
        name = next(n for n, (x0, x1) in ranges.items() if x0 <= x < x1)
        groups[name].append(obj)
    return groups


def export_piece(obj, scene, out_dir):
    """Exports a kit piece centred on the origin, with its moving parts
    (children) and their animation clips."""
    saved = obj.location.copy()
    obj.location = (0.0, 0.0, 0.0)
    export_objects([obj] + list(obj.children_recursive), scene, os.path.join(out_dir, obj.name + ".glb"))
    obj.location = saved


def main():
    args = parse_args()

    scene = fresh_scene()
    atom_kit.clear_generated()
    materials = atom_kit.build_materials()

    # Kit pieces, lined up away from the street for previewing.
    kit_collection = bpy.data.collections.new("Kit")
    scene.collection.children.link(kit_collection)
    pieces = {}
    collision = {}
    x = 0.0
    for build in atom_kit.PIECES:
        obj = build(materials, kit_collection)
        name = obj.name
        pieces[name] = obj
        if name in atom_kit.COLLISION:
            collision[name] = atom_kit.build_collision(
                name, atom_kit.COLLISION[name], kit_collection)

        width = obj.dimensions.x
        offset = (x + width / 2.0, 60.0, 0.0)
        obj.location = offset
        if name in collision:
            collision[name].location = offset
        x += width + 2.0

    street_collection = bpy.data.collections.new("Street")
    scene.collection.children.link(street_collection)
    street = atom_street.build_street(pieces, collision, materials, street_collection)

    # Levels B and C, each in its own collection around its own origin.
    levels = []
    for folder, build in atom_levels.LEVELS:
        level_collection = bpy.data.collections.new(folder.capitalize())
        scene.collection.children.link(level_collection)
        levels.append((folder, build(pieces, collision, materials, level_collection)))

    # Geometry that would z-fight is a build error, like a compile error:
    # nothing is exported until it is fixed.
    if atom_kit.LINT_ERRORS:
        print(f"lint: {len(atom_kit.LINT_ERRORS)} error(s); nothing exported")
        sys.exit(1)
    print("lint: no z-fighting between pieces")

    if args.no_export:
        return

    # Baked light (M15): each kit piece alone (entity models placed by
    # levels), then every level as a whole, so pieces shade each other.
    for obj in pieces.values():
        atom_bake.bake(scene, [obj] + list(obj.children_recursive))
    atom_bake.bake(scene, street.visual)
    for folder, level in levels:
        atom_bake.bake(scene, level.visual, atom_levels.BAKE_MODES.get(folder, "sky"))
    # Lightmaps (M16) where vertex light isn't enough; written next to the
    # level's glb, whose second UV set (TEXCOORD_1) maps them.
    for folder, level in levels:
        lights = atom_levels.LIGHTMAPS.get(folder)
        if lights:
            mesh = next(obj for obj in level.visual if obj.name == folder)
            level_dir = os.path.join(args.out, folder.capitalize())
            os.makedirs(level_dir, exist_ok=True)
            atom_lightmap.bake(scene, mesh, lights, os.path.join(level_dir, folder + "_lm.png"))

    if atom_kit.LINT_ERRORS:
        for error in atom_kit.LINT_ERRORS:
            print("lint ERROR: " + error)
        print(f"lint: {len(atom_kit.LINT_ERRORS)} bake error(s); nothing exported")
        sys.exit(1)
    print("bake: vertex light baked")

    if not bpy.app.background:
        # Exporting from a live session has proven crash-prone (the context
        # scene lags the window scene), so exports are headless only.
        print("Preview built in scene", SCENE_NAME, "- export with blender -b")
        return

    # Night sky panorama (M23), written like the lightmaps: our own PNG
    # writer, so it's byte-identical across rebuilds.
    sky_dir = os.path.join(args.out, "Sky")
    os.makedirs(sky_dir, exist_ok=True)
    sky = atom_textures.night_sky()
    height, width = sky.shape[:2]
    encoded = bytearray(width * height * 3)
    flat = sky.reshape(-1)
    for i in range(width * height * 3):
        encoded[i] = int(round(float(flat[i]) * 255.0))
    atom_lightmap._write_png(os.path.join(sky_dir, "night_sky.png"), width, encoded, height)
    print("Exported", os.path.relpath(os.path.join(sky_dir, "night_sky.png"), REPO_ROOT))

    kit_dir = os.path.join(args.out, "Kit")
    street_dir = os.path.join(args.out, "Street")
    os.makedirs(kit_dir, exist_ok=True)
    os.makedirs(street_dir, exist_ok=True)

    for obj in pieces.values():
        export_piece(obj, scene, kit_dir)

    # Chunks (M22): pieces go to the chunk of their stretch of street, so
    # each stretch is culled as a whole; what spans the whole street
    # (ground, wires, decals) stays in the base model.
    chunks = split_into_chunks(street.visual, STREET_CHUNKS)
    export_objects(chunks.pop("base"), scene, os.path.join(street_dir, "street.glb"))
    for name, objects in chunks.items():
        export_objects(objects, scene, os.path.join(street_dir, "street_" + name + ".glb"))
    export_objects(street.colliders, scene, os.path.join(street_dir, "street_col.glb"),
                   materials=False)

    for folder, level in levels:
        level_dir = os.path.join(args.out, folder.capitalize())
        os.makedirs(level_dir, exist_ok=True)
        export_objects(level.visual, scene, os.path.join(level_dir, folder + ".glb"))
        export_objects(level.colliders, scene, os.path.join(level_dir, folder + "_col.glb"),
                       materials=False)
        write_markers(level.collection, os.path.join(
            args.out, "Levels", atom_levels.LEVEL_FILES[folder] + ".markers.json"))


# Blender exits 0 even when the script raises; make a failed build fail.
try:
    main()
except Exception:
    import traceback
    traceback.print_exc()
    sys.exit(1)
