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

    _run_bake(mode)
    _rebake_masked(meshes, mode)

    for obj in meshes:
        _quantise(obj.data)
        _check(obj)
    for obj in hidden:
        obj.hide_render = False


def _run_bake(mode):
    if mode == "ao":
        bpy.ops.object.bake(type="AO", target="VERTEX_COLORS")
    else:
        bpy.ops.object.bake(
            type="DIFFUSE",
            pass_filter={"DIRECT", "INDIRECT"},
            target="VERTEX_COLORS",
        )


def _is_masked(material):
    import atom_kit
    return material is not None and material.name[len(atom_kit.PREFIX):] in atom_kit.MASKED


def _rebake_masked(meshes, mode):
    """Alpha-tested cards (M17) need a second pass. The first bake keeps
    alpha on, so leaves cast leaf-shaped occlusion on everything else and
    on each other - but a vertex that falls on a transparent texel of its
    own card reads no light at all. So the cards are baked again with their
    masked materials opaque (so they have a surface) while their objects
    are hidden from shadow and bounce rays (so a canopy of now-solid cards
    doesn't bury itself); card vertices that read nothing in the first pass
    take that value."""
    targets = [obj for obj in meshes
               if any(_is_masked(slot.material) for slot in obj.material_slots)]
    if not targets:
        return

    # Opaque for this pass: unhook the alpha chain from the BSDF.
    unhooked = []
    for material in {slot.material for obj in targets for slot in obj.material_slots}:
        if _is_masked(material):
            bsdf = next(n for n in material.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
            link = bsdf.inputs["Alpha"].links[0]
            unhooked.append((material, link.from_socket, bsdf.inputs["Alpha"]))
            material.node_tree.links.remove(link)

    temporary = "baked_opaque"
    for obj in meshes:
        obj.select_set(obj in targets)
    for obj in targets:
        obj.visible_shadow = False
        obj.visible_diffuse = False
    for obj in targets:
        mesh = obj.data
        mesh.color_attributes.new(temporary, "BYTE_COLOR", "POINT")
        # The bake writes the render colour attribute, so point both at it.
        mesh.color_attributes.active_color = mesh.color_attributes[temporary]
        mesh.color_attributes.render_color_index = mesh.color_attributes.find(temporary)
    bpy.context.view_layer.objects.active = targets[0]
    _run_bake(mode)

    for obj in targets:
        mesh = obj.data
        masked_slots = {i for i, slot in enumerate(obj.material_slots) if _is_masked(slot.material)}
        card_vertices = {v for poly in mesh.polygons if poly.material_index in masked_slots
                         for v in poly.vertices}
        final = mesh.color_attributes[ATTRIBUTE]
        opaque = mesh.color_attributes[temporary]
        for v in card_vertices:
            if max(final.data[v].color[:3]) < 0.02:
                final.data[v].color = opaque.data[v].color
        mesh.color_attributes.remove(opaque)
        mesh.color_attributes.active_color = mesh.color_attributes[ATTRIBUTE]
        mesh.color_attributes.render_color_index = mesh.color_attributes.find(ATTRIBUTE)

    for obj in targets:
        obj.visible_shadow = True
        obj.visible_diffuse = True
    for material, socket, alpha in unhooked:
        material.node_tree.links.new(socket, alpha)
    for obj in meshes:
        obj.select_set(True)


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
    # Loose cards alone under the sky (a grass tuft baked by itself) have
    # nothing to occlude them: fully lit is right there.
    only_cards = all(_is_masked(slot.material) for slot in obj.material_slots)
    if max(luminance) <= 0.0:
        atom_kit.LINT_ERRORS.append(f"bake: {obj.name}: no light reached any vertex")
    elif min(luminance) >= 1.0 and not only_cards:
        atom_kit.LINT_ERRORS.append(f"bake: {obj.name}: fully lit everywhere (bake not applied?)")
