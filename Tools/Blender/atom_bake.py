"""Bakes light into vertex colours (M15).

Two modes, chosen per level:
- "sky" (outdoors): Cycles path-traces how much light reaches every vertex
  from a uniform white sky (its occlusion) plus what bounces off nearby
  surfaces.
- "ao" (indoors): ambient occlusion within AO_DISTANCE - how enclosed each
  point is. A closed room gets no sky, so a sky bake would be almost black;
  its real lighting is left to lightmaps (M16).

The result lands in a colour attribute, exported as glTF COLOR_0; the
engine scales its ambient term by it, while the sun stays dynamic with its
shadow map.

Determinism: CPU, fixed seed, fixed samples, no denoiser, and results
quantised to 8 bits, so rebuilds stay byte-identical.
"""

import bpy

ATTRIBUTE = "baked_light"
SAMPLES = 64
AO_DISTANCE = 1.5  # metres


def _prepare_scene(scene):
    scene.render.engine = "CYCLES"
    cycles = scene.cycles
    cycles.device = "CPU"
    cycles.samples = SAMPLES
    cycles.seed = 0
    cycles.use_animated_seed = False
    cycles.use_denoising = False
    cycles.max_bounces = 3
    cycles.diffuse_bounces = 3

    # A uniform white sky of strength 1: an unoccluded, upward-facing
    # surface receives exactly 1.0, so values read as "share of the sky".
    world = bpy.data.worlds.get("atom_bake_world") or bpy.data.worlds.new("atom_bake_world")
    world.use_nodes = True
    background = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    background.inputs["Color"].default_value = (1.0, 1.0, 1.0, 1.0)
    background.inputs["Strength"].default_value = 1.0
    world.light_settings.distance = AO_DISTANCE
    scene.world = world


def _single_user(objects):
    # Linked duplicates share a mesh, but each placement has its own light.
    for obj in objects:
        if obj.data.users > 1:
            obj.data = obj.data.copy()


def _quantise(mesh):
    attribute = mesh.color_attributes[ATTRIBUTE]
    values = [0.0] * (len(attribute.data) * 4)
    attribute.data.foreach_get("color", values)
    values = [round(min(max(v, 0.0), 1.0) * 255.0) / 255.0 for v in values]
    attribute.data.foreach_set("color", values)


def bake(scene, objects, mode="sky"):
    """Bakes `objects` (one level: everything else is hidden from rays)."""
    _prepare_scene(scene)
    _single_user(objects)

    baking = set(objects)
    hidden = []
    for obj in scene.objects:
        if obj not in baking and not obj.hide_render:
            obj.hide_render = True
            hidden.append(obj)

    for obj in scene.objects:
        obj.select_set(obj in baking and obj.type == "MESH")
    meshes = [obj for obj in objects if obj.type == "MESH"]
    for obj in meshes:
        mesh = obj.data
        if ATTRIBUTE not in mesh.color_attributes:
            mesh.color_attributes.new(ATTRIBUTE, "BYTE_COLOR", "POINT")
        attribute = mesh.color_attributes[ATTRIBUTE]
        mesh.color_attributes.active_color = attribute
        mesh.color_attributes.render_color_index = mesh.color_attributes.find(ATTRIBUTE)
    bpy.context.view_layer.objects.active = meshes[0]

    if mode == "ao":
        bpy.ops.object.bake(type="AO", target="VERTEX_COLORS")
    else:
        bpy.ops.object.bake(
            type="DIFFUSE",
            pass_filter={"DIRECT", "INDIRECT"},
            target="VERTEX_COLORS",
        )

    for obj in meshes:
        _quantise(obj.data)
        _check(obj)
    for obj in hidden:
        obj.hide_render = False


def _check(obj):
    """A bake that is all black (no light reached it: a sealed box, the
    world missing) or all white (nothing occludes anything: the bake
    didn't run) is a build error, reported with the kit's lint errors."""
    import atom_kit

    attribute = obj.data.color_attributes[ATTRIBUTE]
    values = [0.0] * (len(attribute.data) * 4)
    attribute.data.foreach_get("color", values)
    luminance = [max(values[i:i + 3]) for i in range(0, len(values), 4)]
    if not luminance:
        return
    if max(luminance) <= 0.0:
        atom_kit.LINT_ERRORS.append(f"bake: {obj.name}: no light reached any vertex")
    elif min(luminance) >= 1.0:
        atom_kit.LINT_ERRORS.append(f"bake: {obj.name}: fully lit everywhere (bake not applied?)")
