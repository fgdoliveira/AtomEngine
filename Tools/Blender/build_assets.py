"""Builds the AtomEngine kit and exports one .glb per piece.

Headless:
    blender -b -P Tools/Blender/build_assets.py -- --out Assets/Kit

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

# Pick up edits when re-run inside a long-lived Blender session.
importlib.reload(atom_textures)
importlib.reload(atom_kit)


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=os.path.join(REPO_ROOT, "Assets", "Kit"))
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


def export_piece(obj, scene, out_dir):
    saved = obj.location.copy()
    obj.location = (0.0, 0.0, 0.0)

    for other in scene.objects:
        other.select_set(other == obj)

    path = os.path.join(out_dir, obj.name + ".glb")
    bpy.ops.export_scene.gltf(
        filepath=path,
        export_format="GLB",
        use_selection=True,
        export_yup=True,
        export_apply=True,
        export_texcoords=True,
        export_normals=True,
        export_materials="EXPORT",
        export_image_format="AUTO",
        export_cameras=False,
        export_lights=False,
        export_extras=False,
    )

    obj.location = saved
    return path


def main():
    args = parse_args()

    scene = fresh_scene()
    atom_kit.clear_generated()
    materials = atom_kit.build_materials()

    pieces = []
    x = 0.0
    for build in atom_kit.PIECES:
        obj = build(materials, scene.collection)
        width = obj.dimensions.x
        obj.location.x = x + width / 2.0
        x += width + 2.0
        pieces.append(obj)

    if args.no_export:
        return pieces

    if not bpy.app.background:
        # Exporting from a live session has proven crash-prone (the context
        # scene lags the window scene), so exports are headless only.
        print("Preview built in scene", SCENE_NAME, "- export with blender -b")
        return pieces

    os.makedirs(args.out, exist_ok=True)
    for obj in pieces:
        path = export_piece(obj, scene, args.out)
        print("Exported", os.path.relpath(path, REPO_ROOT))
    return pieces


main()
