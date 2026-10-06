"""DRIFT's low-poly models (M77), ported from the three.js original
(3d-game-00/tools/build_models.py) with the geometry unchanged: the same
products are what makes the port comparable side by side.

Headless:
    blender -b --factory-startup -P Tools/Blender/drift_models.py

Writes Games/Drift/Assets/{ship,ring,orb,rock}.glb. Like the original, the
props (ring, orb, rock) are authored with Blender's Z as the game's depth
and turned +90 degrees about X before export; the ship is authored in
Blender's own axes (Z up, nose +Y). glTF is Y-up for three.js and for
AtomEngine alike, so the files load the same way in both.
"""
import bpy, bmesh, math, os, random

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Games", "Drift", "Assets")
os.makedirs(OUT, exist_ok=True)


def reset():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    for m in list(bpy.data.meshes):
        bpy.data.meshes.remove(m)
    for m in list(bpy.data.materials):
        bpy.data.materials.remove(m)


def mat(name, color, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = 0.8
    if emit:
        b.inputs["Emission Color"].default_value = (*color, 1)
        b.inputs["Emission Strength"].default_value = emit
    return m


def finish(obj, material, bevel=0.0, segs=1):
    obj.data.materials.append(material)
    if bevel:
        mod = obj.modifiers.new("bevel", "BEVEL")
        mod.width = bevel
        mod.segments = segs
        mod.limit_method = "ANGLE"
    for p in obj.data.polygons:
        p.use_smooth = False
    return obj


def export(name, objs, native_axes=False):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.convert(target="MESH")
        bpy.ops.object.join()
    # models are authored with Blender Z as three.js depth; rotate so export axes match
    obj = bpy.context.view_layer.objects.active
    obj.select_set(True)
    if not native_axes:
        obj.rotation_euler.x += math.pi / 2
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(OUT, name + ".glb"),
        export_format="GLB",
        use_selection=True,
        export_apply=True,
    )
    print("exported", name)


# Palette: muted, adult
HULL = (0.86, 0.80, 0.70)     # warm off-white
ACCENT = (0.78, 0.30, 0.10)   # burnt orange
DARK = (0.10, 0.10, 0.16)     # gunmetal
CORAL = (1.0, 0.42, 0.30)
AMBER = (1.0, 0.70, 0.25)
ROCK = (0.28, 0.25, 0.33)

# ---------- ship (designed live via Blender MCP; Blender-native axes: Z up, nose +Y) ----------
reset()
hull, accent, dark, glow = mat("hull", HULL), mat("accent", ACCENT), mat("dark", DARK), mat("glow", AMBER, 6)
parts = []


def part(obj, material, bevel=0.04, segs=2, scale=None):
    if scale:
        obj.scale = scale
    return parts.append(finish(obj, material, bevel, segs))


bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=0.6, radius2=0.12, depth=2.8, location=(0, 0.2, 0), rotation=(-math.pi / 2, 0, 0))
part(bpy.context.object, hull, 0.06, scale=(1, 1, 0.65))
bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.55, depth=1.0, location=(0, -1.55, 0), rotation=(math.pi / 2, 0, 0))
part(bpy.context.object, dark, 0.08, scale=(1.25, 1, 1.1))
bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.4, depth=0.1, location=(0, -2.08, 0), rotation=(math.pi / 2, 0, 0))
part(bpy.context.object, glow, 0, scale=(1.25, 1, 1.1))
bpy.ops.mesh.primitive_uv_sphere_add(segments=8, ring_count=5, radius=0.35, location=(0, 0.4, 0.3))
part(bpy.context.object, dark, 0, scale=(0.8, 1.7, 0.6))
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -0.55, 0.02))
part(bpy.context.object, accent, 0.03, scale=(1.08, 0.16, 0.72))  # hull stripe

for side in (-1, 1):
    # swept delta wing: root chord at the fuselage, tip swept back
    bm = bmesh.new()
    vs = [bm.verts.new(v) for v in [(0, 0.6, 0), (0, -1.2, 0), (side * 2.2, -1.7, -0.1), (side * 2.2, -1.1, -0.1)]]
    top = bm.faces.new(vs if side > 0 else vs[::-1])
    ext = bmesh.ops.extrude_face_region(bm, geom=[top])
    for v in ext["geom"]:
        if isinstance(v, bmesh.types.BMVert):
            v.co.z += 0.12
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("wing")
    bm.to_mesh(me)
    bm.free()
    w = bpy.data.objects.new("wing", me)
    bpy.context.scene.collection.objects.link(w)
    w.location = (side * 0.35, 0, -0.12)
    part(w, hull, 0.03)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(side * 2.55, -1.4, 0.05))
    part(bpy.context.object, accent, 0.04, scale=(0.14, 0.9, 0.5))

bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -1.3, 0.75))
fin = bpy.context.object
fin.scale = (0.1, 1.1, 0.9)
bm = bmesh.new()
bm.from_mesh(fin.data)
for v in bm.verts:
    if v.co.z > 0:  # sweep the fin back
        v.co.y -= 0.45
bm.to_mesh(fin.data)
bm.free()
part(fin, accent, 0.04)

export("ship", parts, native_axes=True)

# ---------- ring gate ----------
reset()
bpy.ops.mesh.primitive_torus_add(major_segments=16, minor_segments=4, major_radius=3.2, minor_radius=0.22)
ring = bpy.context.object
parts = [finish(ring, mat("ringglow", CORAL, 4))]
for i in range(4):  # chunky clamps around the ring
    a = i * math.pi / 2 + math.pi / 4
    bpy.ops.mesh.primitive_cube_add(size=1, location=(math.cos(a) * 3.2, math.sin(a) * 3.2, 0))
    c = bpy.context.object
    c.scale = (0.7, 0.7, 0.6)
    c.rotation_euler = (0, 0, a)
    parts.append(finish(c, mat("clamp%d" % i, DARK), 0.08, 2))
export("ring", parts)

# ---------- orb ----------
reset()
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=0.45)
export("orb", [finish(bpy.context.object, mat("orbglow", AMBER, 5))])

# ---------- rock ----------
reset()
random.seed(7)
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1.0)
rock = bpy.context.object
for v in rock.data.vertices:
    v.co *= 0.75 + random.random() * 0.45
rock.scale = (1.3, 0.9, 1.1)
export("rock", [finish(rock, mat("rock", ROCK))])
