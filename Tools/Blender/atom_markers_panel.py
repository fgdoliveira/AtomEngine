"""The AtomEngine panel in Blender's sidebar (v0.0.17, M115): add spawn and
entity markers without typing naming conventions, see what the level file
says about the selected one, and check every marker against it.

Open it from the repository (it uses the export's own marker code):
    pwsh Tools/Blender/open.ps1 [scene.blend]
or, in a running Blender: Text Editor > Open > this file > Run Script.
The tab "AtomEngine" appears in the 3D view's sidebar (N).

Markers are ordinary empties - move and turn them with Blender's own
tools. A spawn's arrow and an entity's axes turn about Z; the export turns
that into the game's yaw (atom_export.placement_of). The level file decides
what an entity *is*; where a position is written there, it wins over the
marker, and the panel says so.
"""

import json
import os
import sys

import bpy

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

import atom_export  # noqa: E402

REPO_ROOT = atom_export.REPO_ROOT


# --- Logic (no UI: the headless test calls these) ---------------------------

def resolve_level_path(path):
    """The level file named in the scene's "Level file" field: absolute, or
    relative to the repository."""
    if not path:
        return ""
    path = bpy.path.abspath(path)
    return path if os.path.isabs(path) else os.path.join(REPO_ROOT, path)


def load_level(path):
    """The level file as a dict, or (None, why)."""
    if not path or not os.path.exists(path):
        return None, "no level file" if not path else f"{path} not found"
    try:
        with open(path, encoding="utf-8") as file:
            return json.load(file), ""
    except (OSError, ValueError) as error:
        return None, f"{os.path.basename(path)}: {error}"


def level_entry(level, kind, name):
    """What the level file says about a marker: (entry or None, index)."""
    if not level:
        return None, -1
    if kind == "spawn":
        return level.get("spawns", {}).get(name), -1
    for index, entity in enumerate(level.get("entities", [])):
        if entity.get("name") == name:
            return entity, index
    return None, -1


def describe(level, kind, name):
    """Lines for the selected marker: its JSON entry and what wins."""
    entry, index = level_entry(level, kind, name)
    if level is None:
        return ["no level file set: only the marker itself is checked"]
    if entry is None:
        return [f"the level has no {'spawn' if kind == 'spawn' else 'entity'} \"{name}\""]
    lines = [f"/{'spawns/' + name if kind == 'spawn' else 'entities/' + str(index)}"]
    if kind == "entity":
        lines.append("model: " + entry.get("model", "none"))
        capabilities = [key for key in ("collider", "interactable", "animation", "animator", "mover", "style")
                        if key in entry]
        lines.append("has: " + (", ".join(capabilities) if capabilities else "nothing else"))
    if "position" in entry:
        lines.append("the level file writes a position: it wins, this marker is ignored")
    elif "yaw" in entry:
        lines.append("placed by this marker; the level file's yaw wins over its turn")
    else:
        lines.append("placed by this marker")
    return lines


def check(objects, level):
    """Every marker among `objects` against the export's rules and the level
    file: [(severity, message)], severity "ERROR" or "WARNING" - the same
    words AtomLevelCheck uses."""
    problems = []
    placements, errors = atom_export.collect_markers(objects)
    problems += [("ERROR", error) for error in errors]
    if level is None:
        return problems
    for name in placements["entities"]:
        entry, index = level_entry(level, "entity", name)
        if entry is None:
            problems.append(("ERROR", f"entity:{name} has no entity of that name in the level"))
        elif "position" in entry:
            problems.append(("WARNING", f"entity:{name} is ignored: /entities/{index}/position wins"))
    for name in placements["spawns"]:
        entry, _ = level_entry(level, "spawn", name)
        if entry is not None and "position" in entry:
            problems.append(("WARNING", f"spawn:{name} is ignored: /spawns/{name}/position wins"))
    # With markers, the level loads them, and anything left unplaced is an
    # error (the parser's rule); with none, there is no markers file and an
    # unplaced entity stands at the origin.
    has_markers = bool(placements["entities"] or placements["spawns"])
    for index, entity in enumerate(level.get("entities", [])):
        name = entity.get("name", "")
        if "position" not in entity and name not in placements["entities"]:
            if has_markers:
                problems.append(("ERROR", f"/entities/{index} ({name}) has no position and no entity:{name} marker"))
            elif "model" in entity or "collider" in entity:
                problems.append(("WARNING", f"/entities/{index} ({name}) has no position: it stands at the origin"))
    for name, spawn in level.get("spawns", {}).items():
        if "position" not in spawn and name not in placements["spawns"]:
            problems.append(("ERROR", f"/spawns/{name} has no position and no spawn:{name} marker"))
    return problems


# --- Blender UI ---------------------------------------------------------------

class ATOM_OT_add_marker(bpy.types.Operator):
    """Add a spawn or entity marker at the 3D cursor"""
    bl_idname = "atom.add_marker"
    bl_label = "Add AtomEngine marker"
    bl_options = {"REGISTER", "UNDO"}

    kind: bpy.props.EnumProperty(items=[("spawn", "Spawn", "Where the player can start"),
                                        ("entity", "Entity", "Where an entity of the level file stands")])
    name: bpy.props.StringProperty(name="Name")

    def execute(self, context):
        name = self.name.strip()
        if not name or "." in name or ":" in name:
            self.report({"ERROR"}, "a marker needs a name without '.' or ':'")
            return {"CANCELLED"}
        collection = context.view_layer.active_layer_collection.collection
        cursor = context.scene.cursor
        obj = atom_export.make_marker(self.kind, name, tuple(cursor.location), 0.0, collection)
        for other in context.selected_objects:
            other.select_set(False)
        obj.select_set(True)
        context.view_layer.objects.active = obj
        return {"FINISHED"}


class ATOM_OT_check_markers(bpy.types.Operator):
    """Check every marker in the scene against the export's rules and the level file"""
    bl_idname = "atom.check_markers"
    bl_label = "Check markers"

    def execute(self, context):
        level, why = load_level(resolve_level_path(context.scene.atom_level_file))
        problems = check(context.scene.objects, level)
        context.window_manager.atom_report = "\n".join(f"{s}: {m}" for s, m in problems) or "no problems"
        if level is None:
            self.report({"WARNING"}, why + ": markers checked on their own")
        errors = sum(1 for severity, _ in problems if severity == "ERROR")
        self.report({"ERROR"} if errors else {"INFO"},
                    f"{errors} error(s), {len(problems) - errors} warning(s)")
        return {"FINISHED"}


class ATOM_PT_markers(bpy.types.Panel):
    bl_label = "Markers"
    bl_idname = "ATOM_PT_markers"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "AtomEngine"

    def draw(self, context):
        layout = self.layout
        layout.prop(context.scene, "atom_level_file", text="Level file")
        row = layout.row(align=True)
        row.prop(context.window_manager, "atom_marker_name", text="Name")
        row = layout.row(align=True)
        spawn = row.operator("atom.add_marker", text="Add spawn")
        spawn.kind, spawn.name = "spawn", context.window_manager.atom_marker_name
        entity = row.operator("atom.add_marker", text="Add entity")
        entity.kind, entity.name = "entity", context.window_manager.atom_marker_name

        obj = context.active_object
        marker = atom_export.marker_of(obj) if obj else None
        box = layout.box()
        if marker:
            kind, name = marker
            placement = atom_export.placement_of(kind, obj)
            box.label(text=f"{kind}:{name}")
            box.label(text="game position: " + " ".join(f"{v:.2f}" for v in placement["position"]))
            box.label(text=f"game yaw: {placement['yaw']:.1f}")
            level, why = load_level(resolve_level_path(context.scene.atom_level_file))
            for line in describe(level, kind, name) if level is not None else [why]:
                box.label(text=line)
        else:
            box.label(text="select a marker to see its level entry")

        layout.operator("atom.check_markers", icon="CHECKMARK")
        for line in context.window_manager.atom_report.splitlines()[:12]:
            layout.label(text=line, icon="ERROR" if line.startswith("ERROR") else "INFO")


CLASSES = (ATOM_OT_add_marker, ATOM_OT_check_markers, ATOM_PT_markers)


def register():
    bpy.types.Scene.atom_level_file = bpy.props.StringProperty(
        name="Level file", subtype="FILE_PATH",
        description="The level JSON these markers belong to (relative to the repository, or absolute)")
    bpy.types.WindowManager.atom_marker_name = bpy.props.StringProperty(name="Marker name")
    bpy.types.WindowManager.atom_report = bpy.props.StringProperty(name="Report")
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
    del bpy.types.Scene.atom_level_file
    del bpy.types.WindowManager.atom_marker_name
    del bpy.types.WindowManager.atom_report


if __name__ == "__main__":
    # Run as a script: register for this session (again, if it already was).
    try:
        unregister()
    except (RuntimeError, AttributeError):
        pass
    register()
    print("AtomEngine panel: the 3D view's sidebar (N), tab AtomEngine")
