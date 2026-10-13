"""Exports a hand-made Blender scene as an AtomEngine level (v0.0.17, M116).

    blender -b --factory-startup <scene.blend> --python Tools/Blender/export_scene.py -- \
        --level <name> --out <app>/Assets [--bake sky|ao|none]

(normally through: pwsh Tools/Blender/export.ps1 -Scene <scene.blend> -Level <name>)

The .blend is the authority for the geometry and the markers; no Python
builds it. The level file, <out>/Levels/<level>.json, is written by hand:
it says what the entities are and do, and where the models go - the
export writes exactly the paths its "model" and "collision" name, so the
two can't disagree. The scene follows three conventions:

- a collection "Visual": what is drawn        -> the level's "model"
- a collection "Collision": what the player walks on and bumps into
  (any meshes; materials are dropped)           -> the level's "collision"
- spawn:<name> / entity:<name> empties anywhere in the scene (made by
  hand or with the AtomEngine panel)            -> Levels/<level>.markers.json

Everything else in the scene (cameras, lights, reference objects) is left
out.

The same exporter settings, marker conversion and write-if-changed as the
procedural build (atom_export). --bake sky|ao bakes vertex light into the
visual meshes first (atom_bake, CPU, deterministic); the .blend itself is
never saved, so the bake doesn't change the source.
"""

import argparse
import json
import os
import sys

import bpy

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

import atom_bake  # noqa: E402
import atom_export  # noqa: E402

REPO_ROOT = atom_export.REPO_ROOT


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(prog="export_scene.py")
    parser.add_argument("--level", required=True, help="the level's name: Levels/<level>.json")
    parser.add_argument("--out", required=True, help="the app's Assets folder")
    parser.add_argument("--bake", choices=("none", "sky", "ao"), default="none")
    args = parser.parse_args(argv)
    args.out = os.path.abspath(args.out if os.path.isabs(args.out) else os.path.join(REPO_ROOT, args.out))
    return args


def collection(name):
    """The scene's collection of that name, case-insensitive, or None."""
    for found in bpy.data.collections:
        if found.name.lower() == name.lower():
            return found
    return None


def refuse_foreign(path, source):
    """A generated file names its source; one made from a different source
    (another scene, or the procedural build) is never overwritten."""
    if not os.path.exists(path):
        return
    with open(path, encoding="utf-8") as file:
        try:
            note = json.load(file).get("_generated", "")
        except ValueError:
            note = ""
    if note and source not in note:
        raise RuntimeError(f"{os.path.relpath(path, REPO_ROOT)} was generated {note.split(';')[0]}, "
                           f"not from {source}: refusing to overwrite it")


def main():
    args = parse_args()
    scene = bpy.context.scene
    blend = bpy.data.filepath
    if not blend:
        raise RuntimeError("open a .blend: blender -b <scene.blend> --python export_scene.py -- ...")
    source = os.path.relpath(blend, REPO_ROOT).replace("\\", "/")
    print(f"export_scene: {source} -> level {args.level}")
    atom_export.reset_stats()

    visual_collection = collection("Visual")
    collision_collection = collection("Collision")
    problems = []
    if not visual_collection:
        problems.append("no collection \"Visual\" (what is drawn)")
    if not collision_collection:
        problems.append("no collection \"Collision\" (what the player walks on)")
    visual = [o for o in (visual_collection.all_objects if visual_collection else []) if o.type == "MESH"]
    colliders = [o for o in (collision_collection.all_objects if collision_collection else []) if o.type == "MESH"]
    if visual_collection and not visual:
        problems.append("the \"Visual\" collection has no mesh")
    if collision_collection and not colliders:
        problems.append("the \"Collision\" collection has no mesh")
    placements, marker_errors = atom_export.collect_markers(scene.objects)
    problems += marker_errors
    level_file = os.path.join(args.out, "Levels", args.level + ".json")
    model = collision_model = ""
    if not os.path.exists(level_file):
        problems.append(f"no level file {os.path.relpath(level_file, REPO_ROOT)}: write it first (it names these outputs)")
    else:
        with open(level_file, encoding="utf-8") as file:
            level = json.load(file)
        model, collision_model = level.get("model", ""), level.get("collision", "")
        if not model.endswith(".glb") or not collision_model.endswith(".glb"):
            problems.append(f"{args.level}.json must name its \"model\" and \"collision\" (.glb, relative to Assets/)")
    if problems:
        for problem in problems:
            print("export ERROR: " + problem)
        raise RuntimeError(f"{len(problems)} problem(s); nothing exported")

    markers_path = os.path.join(args.out, "Levels", args.level + ".markers.json")
    refuse_foreign(markers_path, source)

    if args.bake != "none":
        atom_bake.bake(scene, visual, args.bake)
    atom_export.export_objects(visual, scene, os.path.join(args.out, model))
    atom_export.export_objects(colliders, scene, os.path.join(args.out, collision_model), materials=False)

    # The markers of the whole scene (a hand-made scene is one level).
    markers = bpy.data.collections.get("_atom_markers") or bpy.data.collections.new("_atom_markers")
    for obj in scene.objects:
        if atom_export.marker_of(obj) and obj.name not in markers.objects:
            markers.objects.link(obj)
    atom_export.write_markers(markers, markers_path, source=f"Tools/Blender/export_scene.py from {source}")
    stats = atom_export.STATS
    print(f"export: {stats['written']} written, {stats['unchanged']} unchanged")


try:
    main()
except Exception:
    import traceback
    traceback.print_exc()
    sys.exit(1)
