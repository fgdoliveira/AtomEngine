"""Builds the AtomEngine kit and levels, and exports them.

Headless (normally through Tools/Blender/export.ps1):
    blender -b --factory-startup -P Tools/Blender/build_assets.py [-- options]

Options (after "--"):
    --only <unit>[,<unit>...]  build and export only these units (v0.0.17,
                               M113); --list prints them. Default: all.
    --what all|geometry|markers
                               geometry: models, collision, bakes;
                               markers: only <level>.markers.json (no bake,
                               no model export); all (default): both.
    --no-cache                 re-bake every lightmap (fingerprints in
                               build/bake_cache/ skip unchanged ones).
    --gpu                      bake lightmaps on the NVIDIA GPU, for fast
                               iteration (never commit those).
    --no-export                stop after the lint.

A unit is what one builder makes: the kit (Content/Kit), the sky
(Content/Sky), the street, the city layers, the night street, and each
level folder of atom_levels.LEVELS. Every unit places kit pieces, so the
kit is always *built*; it is baked and exported only when selected.

Every output goes through atom_export.write_if_changed: an unchanged file
is not rewritten.

From a live Blender (e.g. Blender MCP), exec this file with __file__ set to
build a preview in a dedicated "AtomKit" scene; the open scene is untouched
and nothing is exported.
"""

import argparse
import importlib
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
import atom_export  # noqa: E402
import atom_lightmap  # noqa: E402
import atom_city  # noqa: E402
import atom_night  # noqa: E402
import atom_pachinko  # noqa: E402

# Pick up edits when re-run inside a long-lived Blender session.
importlib.reload(atom_textures)
importlib.reload(atom_kit)
importlib.reload(atom_street)
importlib.reload(atom_pachinko)  # before atom_levels, which uses it
importlib.reload(atom_levels)
importlib.reload(atom_bake)
importlib.reload(atom_export)
importlib.reload(atom_lightmap)
importlib.reload(atom_city)
importlib.reload(atom_night)

export_objects = atom_export.export_objects
write_markers = atom_export.write_markers


# v0.0.14: the levels that moved to the Showcase write there.
SHOWCASE_FOLDERS = {"first", "lab", "lakeshore"}


def out_for(args, folder):
    return args.showcase if folder in SHOWCASE_FOLDERS else args.out


# M91: the first-render stage's geometry is shared (Content/First): the
# Showcase's documentation stage and Apps/Samples/HelloAtom both load it. Its
# level file and markers stay the Showcase's.
CONTENT_LEVEL_FOLDERS = {"first"}


def geometry_out_for(args, folder):
    return args.content if folder in CONTENT_LEVEL_FOLDERS else out_for(args, folder)


# The export units (M113), in build order, with what each writes.
FIXED_UNITS = {
    "kit": "Content/Kit/<piece>.glb (every kit piece)",
    "sky": "Content/Sky/night_sky.png",
    "street": "Demo Street/street*.glb, street_col.glb",
    "city": "Demo City/mid_blocks.glb, skyline.glb, tower_impostor.*",
    "night": "Demo Night/night_*.glb, lightmaps, night_street.markers.json",
}


def all_units():
    units = dict(FIXED_UNITS)
    for folder, _ in atom_levels.LEVELS:
        markers = atom_levels.LEVEL_FILES.get(folder)
        units[folder] = f"{folder.capitalize()}/{folder}.glb, {folder}_col.glb" + (
            f", {markers}.markers.json" if markers else "") + (
            ", a lightmap" if folder in atom_levels.LIGHTMAPS else "")
    return units


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(prog="build_assets.py")
    # v0.0.14: the demo's own products in Apps/Games/Demo/Assets; the generic kit
    # pieces and sky, which other apps use too, in the shared Content/.
    parser.add_argument("--out", default=os.path.join(REPO_ROOT, "Apps", "Games", "Demo", "Assets"))
    parser.add_argument("--content", default=os.path.join(REPO_ROOT, "Content"))
    # The Showcase's stages and terrain (first render, the lab, the lakeshore).
    parser.add_argument("--showcase", default=os.path.join(REPO_ROOT, "Apps", "Showcase", "Assets"))
    parser.add_argument("--no-export", action="store_true")
    # Lightmaps whose inputs are unchanged since the last bake are kept
    # (fingerprints in build/bake_cache/); --no-cache bakes everything.
    parser.add_argument("--no-cache", action="store_true")
    # Bake lightmaps on the NVIDIA GPU: seconds instead of minutes, for
    # tuning light. Not byte-identical to the CPU: rebuild without it
    # before committing (a test refuses GPU-baked lightmaps).
    parser.add_argument("--gpu", action="store_true")
    parser.add_argument("--only", default="")
    parser.add_argument("--what", choices=("all", "geometry", "markers"), default="all")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args(argv)

    units = all_units()
    if args.only:
        args.units = [u.strip() for u in args.only.split(",") if u.strip()]
        unknown = [u for u in args.units if u not in units]
        if unknown:
            raise SystemExit("build_assets: no unit " + ", ".join(unknown) + "; units: " + ", ".join(units))
    else:
        args.units = list(units)
    return args


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


def markers_path(args, folder):
    """Where a level folder's markers go - next to its level file, which
    must exist: markers for a level nobody loads are a mistake (until
    M113 the lakeshore's were written after its level had moved into the
    Showcase's)."""
    name = atom_levels.LEVEL_FILES.get(folder)
    if not name:
        return None
    levels = os.path.join(out_for(args, folder), "Levels")
    if not os.path.exists(os.path.join(levels, name + ".json")):
        raise RuntimeError(f"atom_levels.LEVEL_FILES: {folder} -> {name}, but there is no {name}.json in "
                           f"{os.path.relpath(levels, REPO_ROOT)}")
    return os.path.join(levels, name + ".markers.json")


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
    started = time.monotonic()
    args = parse_args()
    if args.list:
        for unit, writes in all_units().items():
            print(f"{unit:10} {writes}")
        return
    selected = set(args.units)
    geometry = args.what in ("all", "geometry")
    markers = args.what in ("all", "markers")
    print("build: units " + ", ".join(args.units) + f"; {args.what}")
    atom_lightmap.CACHE_DIR = None if args.no_cache else os.path.join(REPO_ROOT, "build", "bake_cache")
    atom_lightmap.USE_GPU = args.gpu
    atom_export.reset_stats()

    scene = fresh_scene()
    atom_kit.clear_generated()
    materials = atom_kit.build_materials()

    # Kit pieces, lined up away from the street for previewing. Always
    # built: every other unit places them.
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

    # Every unit is built, selected or not (M113): Blender's object names
    # are global, so a copy gets ".001" or ".061" depending on what was
    # built before it, and the names go into the .glb. Building all of them
    # (about a second) gives a one-unit export the same bytes as the full
    # build; only baking and exporting are per unit.
    street_collection = bpy.data.collections.new("Street")
    scene.collection.children.link(street_collection)
    street = atom_street.build_street(pieces, collision, materials, street_collection)

    # The other levels, each in its own collection around its own origin.
    levels = []
    for folder, build in atom_levels.LEVELS:
        level_collection = bpy.data.collections.new(folder.capitalize())
        scene.collection.children.link(level_collection)
        levels.append((folder, build(pieces, collision, materials, level_collection)))

    # City layers (M24): middle shells, far skyline, and the impostor source.
    city_collection = bpy.data.collections.new("City")
    scene.collection.children.link(city_collection)
    mid_blocks = atom_city.build_mid_blocks(materials, city_collection)
    skyline = atom_city.build_skyline(materials, city_collection)
    tower = atom_city.build_impostor_tower(materials, city_collection)
    tower.location = (0.0, -600.0, 0.0)  # out of the way of the other scenes

    # Level E, the night street (M25): its own collection, around its own
    # origin like the other levels.
    night_collection = bpy.data.collections.new("Night")
    scene.collection.children.link(night_collection)
    night = atom_night.build_night_street(pieces, collision, materials, night_collection)

    # From here on, only the selected units (the bake still walks all).
    street_all, levels_all = street, levels
    street = street if "street" in selected else None
    night = night if "night" in selected else None
    levels = [(folder, level) for folder, level in levels if folder in selected]

    # Geometry that would z-fight is a build error, like a compile error:
    # nothing is exported until it is fixed.
    if atom_kit.LINT_ERRORS:
        print(f"lint: {len(atom_kit.LINT_ERRORS)} error(s); nothing exported")
        sys.exit(1)
    print("lint: no z-fighting between pieces")

    if args.no_export:
        return

    if geometry:
        # Baked light (M15): each kit piece alone (entity models placed by
        # levels), then every level as a whole, so pieces shade each other.
        # A unit not selected isn't baked, but its meshes are still made
        # single-user in the full build's order (M113): a shared mesh gets
        # a copy named ".001", ".006"... by how many were made before it,
        # and the name goes into the .glb.
        def bake_or_copy(selected_unit, objects, mode="sky"):
            if selected_unit:
                atom_bake.bake(scene, objects, mode)
            else:
                atom_bake._single_user(objects)

        for obj in pieces.values():
            bake_or_copy("kit" in selected, [obj] + list(obj.children_recursive))
        bake_or_copy(street is not None, street_all.visual)
        for folder, level in levels_all:
            mode = atom_levels.BAKE_MODES.get(folder, "sky")
            if mode != "none":
                bake_or_copy(folder in selected, level.visual, mode)
        # Lightmaps (M16) where vertex light isn't enough; written next to the
        # level's glb, whose second UV set (TEXCOORD_1) maps them.
        for folder, level in levels:
            lights = atom_levels.LIGHTMAPS.get(folder)
            if lights:
                mesh = next(obj for obj in level.visual if obj.name == folder)
                level_dir = os.path.join(geometry_out_for(args, folder), folder.capitalize())
                os.makedirs(level_dir, exist_ok=True)
                atom_lightmap.bake(scene, mesh, lights, os.path.join(level_dir, folder + "_lm.png"),
                                   **atom_levels.LIGHTMAP_OPTIONS.get(folder, {}))

        # The night street: a lightmap per cell, lit by the street's own
        # lamps, windows and signs.
        night_dir = os.path.join(args.out, "Night")
        if night:
            os.makedirs(night_dir, exist_ok=True)
            atom_night.bake(scene, night, night_dir)

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

    if geometry and "sky" in selected:
        # Night sky panorama (M23), written like the lightmaps: our own PNG
        # writer, so it's byte-identical across rebuilds.
        sky_dir = os.path.join(args.content, "Sky")
        os.makedirs(sky_dir, exist_ok=True)
        sky = atom_textures.night_sky()
        height, width = sky.shape[:2]
        encoded = bytearray(width * height * 3)
        flat = sky.reshape(-1)
        for i in range(width * height * 3):
            encoded[i] = int(round(float(flat[i]) * 255.0))
        sky_path = os.path.join(sky_dir, "night_sky.png")
        atom_export.report(sky_path, atom_lightmap._write_png(sky_path, width, encoded, height))

    if geometry and "kit" in selected:
        kit_dir = os.path.join(args.content, "Kit")
        os.makedirs(kit_dir, exist_ok=True)
        for obj in pieces.values():
            export_piece(obj, scene, kit_dir)

    if geometry and street:
        # Chunks (M22): pieces go to the chunk of their stretch of street, so
        # each stretch is culled as a whole; what spans the whole street
        # (ground, wires, decals) stays in the base model.
        street_dir = os.path.join(args.out, "Street")
        os.makedirs(street_dir, exist_ok=True)
        chunks = split_into_chunks(street.visual, STREET_CHUNKS)
        export_objects(chunks.pop("base"), scene, os.path.join(street_dir, "street.glb"))
        for name, objects in chunks.items():
            export_objects(objects, scene, os.path.join(street_dir, "street_" + name + ".glb"))
        export_objects(street.colliders, scene, os.path.join(street_dir, "street_col.glb"),
                       materials=False)

    if geometry and "city" in selected:
        # Mid and far layers aren't baked: lit windows come from their emissive
        # masks, the rest from the level's ambient.
        city_dir = os.path.join(args.out, "City")
        os.makedirs(city_dir, exist_ok=True)
        export_objects([mid_blocks], scene, os.path.join(city_dir, "mid_blocks.glb"))
        export_objects([skyline], scene, os.path.join(city_dir, "skyline.glb"))
        atom_city.render_impostor(scene, tower, os.path.join(city_dir, "tower_impostor.png"),
                                  os.path.join(city_dir, "tower_impostor.json"))

    if night:
        # Night street: the always-drawn base (the road west, the railway,
        # cables, wet-road decals), a chunk per cell, one collision file.
        if geometry:
            export_objects([night.base, night.decals], scene, os.path.join(night_dir, "night_street.glb"))
            for cell, obj in night.cells.items():
                export_objects([obj], scene, os.path.join(night_dir, "night_" + cell + ".glb"))
            export_objects(night.colliders, scene, os.path.join(night_dir, "night_street_col.glb"),
                           materials=False)
        if markers:
            write_markers(night.collection, os.path.join(args.out, "Levels", "night_street.markers.json"))

    for folder, level in levels:
        if geometry:
            level_dir = os.path.join(geometry_out_for(args, folder), folder.capitalize())
            os.makedirs(level_dir, exist_ok=True)
            export_objects(level.visual, scene, os.path.join(level_dir, folder + ".glb"))
            export_objects(level.colliders, scene, os.path.join(level_dir, folder + "_col.glb"),
                           materials=False)
        path = markers_path(args, folder)
        if markers and path:
            write_markers(level.collection, path, extra=getattr(level, "markers_extra", None))

    stats = atom_export.STATS
    print(f"export: {stats['written']} written, {stats['unchanged']} unchanged, "
          f"{time.monotonic() - started:.0f} s")


# Blender exits 0 even when the script raises; make a failed build fail.
try:
    main()
except SystemExit:
    raise
except Exception:
    import traceback
    traceback.print_exc()
    sys.exit(1)
