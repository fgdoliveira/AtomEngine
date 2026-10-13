"""What every AtomEngine export writes, and how (v0.0.17, M113).

Shared by the procedural build (build_assets.py) and the hand-made scene
export (export_scene.py), so both produce the same runtime outputs:

- export_objects: a .glb through Blender's glTF exporter, with the
  engine's settings (Y up, applied modifiers, the baked-light colour
  attribute, material extras);
- write_markers: a level's spawn:/entity: empties as <level>.markers.json;
- write_if_changed: every output goes through it. A file whose bytes are
  unchanged is not rewritten - its timestamp stays, and git, the build's
  asset copy and hot reload see nothing.

Counts what was written and what was already up to date (STATS).
"""

import json
import math
import os
import time

import bpy

import atom_bake

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

# The vertex colour attribute holding baked light.
BAKED_LIGHT_ATTRIBUTE = atom_bake.ATTRIBUTE

STATS = {"written": 0, "unchanged": 0}


def reset_stats():
    STATS["written"] = 0
    STATS["unchanged"] = 0


def _relative(path):
    return os.path.relpath(path, REPO_ROOT).replace("\\", "/")


def write_if_changed(path, data, text=False):
    """Writes `data` (bytes) to `path` unless the file already holds exactly
    that. Text (`text=True`, written with LF) compares with line endings
    normalised: a git checkout with core.autocrlf holds the same JSON with
    CRLF, and rewriting it would change nothing but its bytes. Returns
    True when it wrote."""
    if os.path.exists(path):
        with open(path, "rb") as file:
            existing = file.read()
        if text:
            existing = existing.replace(b"\r\n", b"\n")
        if existing == data:
            STATS["unchanged"] += 1
            return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    temporary = path + ".tmp"
    with open(temporary, "wb") as file:
        file.write(data)
    os.replace(temporary, path)
    STATS["written"] += 1
    return True


def report(path, written, note=""):
    print(("Exported " if written else "Unchanged ") + _relative(path) + (f" ({note})" if note else ""))


def export_objects(objects, scene, path, materials=True):
    """Exports `objects` (selected, nothing else) as one .glb at `path`,
    through a temporary file: the result replaces `path` only if it
    differs."""
    selected = set(objects)
    for obj in scene.objects:
        obj.select_set(obj in selected)

    temporary = path + ".export.glb"
    # Windows sometimes holds a just-written file for a moment (antivirus,
    # indexer), making the next open fail with EINVAL; a short retry is
    # enough. Anything that persists is a real error.
    for attempt in range(5):
        try:
            bpy.ops.export_scene.gltf(
                filepath=temporary,
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
                export_vertex_color_name=BAKED_LIGHT_ATTRIBUTE,
                export_all_vertex_colors=False,
            )
            break
        except RuntimeError as error:
            if attempt == 4:
                raise
            print(f"Export of {os.path.basename(path)} failed ({error}); retrying")
            time.sleep(0.5)
    with open(temporary, "rb") as file:
        data = file.read()
    os.remove(temporary)
    report(path, write_if_changed(path, data))


def marker_of(obj):
    """(kind, name) of a marker empty, or None. Markers made by the level
    builders (or the AtomEngine panel) carry their real name in
    properties: object names are global in Blender, so two levels'
    "spawn:start" can't both keep it. Empties placed by hand go by their
    object name, "spawn:<name>" / "entity:<name>"."""
    if obj.type != "EMPTY":
        return None
    if "atom_marker_kind" in obj:
        return obj["atom_marker_kind"], obj["atom_marker_name"]
    if ":" in obj.name:
        return tuple(obj.name.split(":", 1))
    return None


def placement_of(kind, obj):
    """A marker's game-space placement (glTF axes: x, z, -y). Spawn yaw:
    the empty looks along its local +Y, game yaw 0 looks down -Z, so
    yaw = -rotation. Entity yaw: the model turns like a kit piece,
    yaw = rotation. Rounded, so the file is byte-identical across
    rebuilds."""
    x, y, z = obj.matrix_world.translation
    rotation = math.degrees(obj.matrix_world.to_euler("XYZ").z)
    yaw = -rotation if kind == "spawn" else rotation
    return {
        "position": [round(x, 3), round(z, 3), round(-y, 3)],
        "yaw": round((yaw + 180.0) % 360.0 - 180.0, 2) + 0.0,
    }


def collect_markers(objects):
    """Placements and errors for the marker empties among `objects`:
    ({"spawns": {...}, "entities": {...}}, [error, ...])."""
    placements = {"spawns": {}, "entities": {}}
    errors = []
    for obj in objects:
        marker = marker_of(obj)
        if not marker:
            continue
        kind, name = marker
        group = "spawns" if kind == "spawn" else "entities"
        if kind not in ("spawn", "entity") or not name or "." in name or name in placements[group]:
            errors.append(f"marker {obj.name}: expected spawn:<name> or entity:<name>, unique in its level")
            continue
        placements[group][name] = placement_of(kind, obj)
    return placements, errors


def write_markers(collection, path, source="Tools/Blender/build_assets.py"):
    """Writes the collection's spawn:/entity: empties to `path`. Sorted and
    rounded, so the file is byte-identical across rebuilds. A bad marker
    stops the build instead of silently losing a spawn. Returns the
    number of markers."""
    # Blender updates matrix_world lazily: an empty placed (or parented) by
    # a builder reads as the origin until the scene is evaluated. A full
    # build evaluated it while baking; a markers-only export (M113) bakes
    # nothing, so update here - without it every marker came out at 0, 0, 0.
    bpy.context.view_layer.update()
    placements, errors = collect_markers(collection.all_objects)
    if errors:
        for error in errors:
            print("lint ERROR: " + error)
        raise RuntimeError(f"{len(errors)} bad marker(s) in {os.path.basename(path)}")
    count = len(placements["spawns"]) + len(placements["entities"])
    if not count:
        if os.path.exists(path):
            os.remove(path)
            print("Removed", _relative(path), "(no markers left)")
        return 0
    document = {"_generated": f"by {source} from the level's markers; do not edit"}
    document.update(placements)
    text = json.dumps(document, indent=2, sort_keys=True) + "\n"
    report(path, write_if_changed(path, text.encode("utf-8"), text=True), f"{count} markers")
    return count


def write_json(path, document):
    """A generated JSON file (sorted, two-space indent, LF), if changed."""
    text = json.dumps(document, indent=2, sort_keys=True) + "\n"
    report(path, write_if_changed(path, text.encode("utf-8"), text=True))
