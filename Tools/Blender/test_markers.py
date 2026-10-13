"""The AtomEngine panel's markers are the export's markers (v0.0.17, M115).

    blender -b --factory-startup --python Tools/Blender/test_markers.py

Headless: the panel's operator makes markers, which must export exactly
like the ones the level builders make (Street.add_marker), including the
game yaw of a turned spawn and entity; its checks must find what the
export and the level parser would refuse. Exit code 1 on a failure
(CTest: Blender.markers).
"""

import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import atom_export  # noqa: E402
import atom_markers_panel as panel  # noqa: E402

failures = []


def expect(condition, what):
    print(("ok   " if condition else "FAIL ") + what)
    if not condition:
        failures.append(what)


def main():
    panel.register()
    scene = bpy.context.scene
    for obj in list(scene.objects):
        bpy.data.objects.remove(obj)
    collection = bpy.data.collections.new("Level")
    scene.collection.children.link(collection)
    bpy.context.view_layer.active_layer_collection = bpy.context.view_layer.layer_collection.children["Level"]

    # Made through the panel's operator, at the 3D cursor, then turned.
    scene.cursor.location = (2.0, -5.0, 0.5)
    expect(bpy.ops.atom.add_marker(kind="entity", name="lamp") == {"FINISHED"}, "operator adds an entity marker")
    lamp = bpy.context.view_layer.objects.active
    lamp.rotation_euler.z = math.radians(30.0)
    scene.cursor.location = (0.0, -9.0, 0.0)
    expect(bpy.ops.atom.add_marker(kind="spawn", name="start") == {"FINISHED"}, "operator adds a spawn marker")
    start = bpy.context.view_layer.objects.active
    start.rotation_euler.z = math.radians(90.0)
    try:  # headless, an operator's error report raises instead of returning
        refused = bpy.ops.atom.add_marker(kind="entity", name="bad.name") == {"CANCELLED"}
    except RuntimeError as error:
        refused = "without '.'" in str(error)
    expect(refused, "a name with '.' is refused")

    # The same placements made the builders' way, in another collection.
    builders = bpy.data.collections.new("Builders")
    scene.collection.children.link(builders)
    atom_export.make_marker("entity", "lamp", (2.0, -5.0, 0.5), 30.0, builders)
    atom_export.make_marker("spawn", "start", (0.0, -9.0, 0.0), 90.0, builders)
    bpy.context.view_layer.update()

    from_panel, errors_panel = atom_export.collect_markers(collection.all_objects)
    from_builders, _ = atom_export.collect_markers(builders.all_objects)
    expect(not errors_panel, "the panel's markers pass the export's rules")
    expect(from_panel == from_builders, "the panel's markers export exactly like the builders'")
    expect(from_panel["entities"]["lamp"] == {"position": [2.0, 0.5, 5.0], "yaw": 30.0},
           "entity: game position (x, z, -y) and yaw = rotation")
    expect(from_panel["spawns"]["start"] == {"position": [0.0, 0.0, 9.0], "yaw": -90.0},
           "spawn: yaw = -rotation (it looks along its arrow)")
    expect(lamp.name == "entity:lamp" and lamp["atom_marker_name"] == "lamp", "named by the convention, and by property")

    # The checks, against a level file.
    level = {"spawns": {"start": {}},
             "entities": [{"name": "lamp", "position": [1, 0, 1], "model": "Kit/lamp.glb"},
                          {"name": "bench"}]}
    problems = panel.check(collection.all_objects, level)
    text = "\n".join(f"{s}: {m}" for s, m in problems)
    print(text)
    expect(("WARNING", "entity:lamp is ignored: /entities/0/position wins") in problems,
           "a marker the file overrides is a warning")
    expect(("ERROR", "/entities/1 (bench) has no position and no entity:bench marker") in problems,
           "an entity with neither position nor marker is an error")
    expect(panel.describe(level, "entity", "lamp")[-1].startswith("the level file writes a position"),
           "the selected marker's entry says the file wins")
    expect(panel.describe(level, "spawn", "start")[-1] == "placed by this marker", "a spawn placed by its marker")

    ghost = atom_export.make_marker("entity", "ghost", (0, 0, 0), 0.0, collection)
    expect(("ERROR", "entity:ghost has no entity of that name in the level") in panel.check(collection.all_objects, level),
           "a marker naming no entity is an error")
    bpy.data.objects.remove(ghost)
    twin = bpy.data.objects.new("entity:lamp", None)  # Blender names it entity:lamp.001
    collection.objects.link(twin)
    errors = [m for s, m in panel.check(collection.all_objects, level) if s == "ERROR"]
    expect(any("unique in its level" in m for m in errors), "a second marker of one name is an error")

    panel.unregister()


try:
    main()
except Exception:
    import traceback
    traceback.print_exc()
    failures.append("exception")
print("FAILED: " + str(len(failures)) if failures else "PASS")
sys.exit(1 if failures else 0)
