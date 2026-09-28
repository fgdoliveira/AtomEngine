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
import os
import sys

import bpy

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
SCENE_NAME = "AtomKit"

if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

import atom_textures  # noqa: E402
import atom_kit  # noqa: E402
import atom_street  # noqa: E402
import atom_levels  # noqa: E402

# Pick up edits when re-run inside a long-lived Blender session.
importlib.reload(atom_textures)
importlib.reload(atom_kit)
importlib.reload(atom_street)
importlib.reload(atom_levels)


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
        export_extras=False,
    )
    print("Exported", os.path.relpath(path, REPO_ROOT))


def export_piece(obj, scene, out_dir):
    """Exports a kit piece centred on the origin."""
    saved = obj.location.copy()
    obj.location = (0.0, 0.0, 0.0)
    export_objects([obj], scene, os.path.join(out_dir, obj.name + ".glb"))
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

    if not bpy.app.background:
        # Exporting from a live session has proven crash-prone (the context
        # scene lags the window scene), so exports are headless only.
        print("Preview built in scene", SCENE_NAME, "- export with blender -b")
        return

    kit_dir = os.path.join(args.out, "Kit")
    street_dir = os.path.join(args.out, "Street")
    os.makedirs(kit_dir, exist_ok=True)
    os.makedirs(street_dir, exist_ok=True)

    for obj in pieces.values():
        export_piece(obj, scene, kit_dir)

    export_objects(street.visual, scene, os.path.join(street_dir, "street.glb"))
    export_objects(street.colliders, scene, os.path.join(street_dir, "street_col.glb"),
                   materials=False)

    for folder, level in levels:
        level_dir = os.path.join(args.out, folder.capitalize())
        os.makedirs(level_dir, exist_ok=True)
        export_objects(level.visual, scene, os.path.join(level_dir, folder + ".glb"))
        export_objects(level.colliders, scene, os.path.join(level_dir, folder + "_col.glb"),
                       materials=False)


main()
